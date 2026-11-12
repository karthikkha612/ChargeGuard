
#include "stm32f4xx.h"
#include <stdint.h>
#include <stdio.h>

/*
 * ChargeGuard
 * MCU: STM32F401CCU6 Black Pill
 * Sensor: INA219
 *
 * I2C1: PB6 = SCL, PB7 = SDA
 * USART2: PA2 = TX, PA3 = RX
 *
 * UART: 9600 baud, 8-N-1
 * Assumed APB1 clock: 16 MHz
 * INA219 address: 0x40
 * Shunt resistor: 0.1 ohm
 */

/* INA219 registers */
#define INA_ADDR       0x40U
#define REG_CONFIG     0x00U
#define REG_BUS        0x02U
#define REG_CURRENT    0x04U
#define REG_CALIB      0x05U

/* I2C status bits */
#define SR1_SB         (1U << 0)
#define SR1_ADDR       (1U << 1)
#define SR1_BTF        (1U << 2)
#define SR1_RXNE       (1U << 6)
#define SR1_TXE        (1U << 7)
#define SR1_BERR       (1U << 8)
#define SR1_ARLO       (1U << 9)
#define SR1_AF         (1U << 10)
#define SR1_OVR        (1U << 11)

#define SR1_ERRORS \
    (SR1_BERR | SR1_ARLO | SR1_AF | SR1_OVR)

/* Variables visible in Keil Watch */
volatile uint32_t sensor_ok = 0U;
volatile uint32_t i2c_error = 0U;

volatile float voltage_V = 0.0f;
volatile float current_mA = 0.0f;
volatile float power_mW = 0.0f;

volatile uint32_t battery_percent_manual = 0U;
volatile uint32_t sample_number = 0U;
volatile uint16_t bus_raw_watch = 0U;

/* Function declarations */
static void I2C1_Init(void);
static int I2C_Wait(uint32_t mask);
static void I2C_Stop(void);
static int INA_Write(uint8_t reg, uint16_t value);
static int INA_Read16(uint8_t reg, uint16_t *value);
static int INA_Init(void);
static void INA_ReadMeasurements(void);
static void USART2_Init(void);
static void USART2_SendChar(char c);
static void USART2_SendString(const char *s);
static void USART2_SendMeasurements(void);
static void Delay(void);

/* ---------------- UART ---------------- */

static void USART2_Init(void)
{
    /* Enable GPIOA and USART2 clocks */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    (void)RCC->AHB1ENR;
    (void)RCC->APB1ENR;

    /* PA2 and PA3 alternate-function mode */
    GPIOA->MODER &= ~((3U << 4U) | (3U << 6U));
    GPIOA->MODER |=  ((2U << 4U) | (2U << 6U));

    /* AF7 = USART2 */
    GPIOA->AFR[0] &= ~((15U << 8U) | (15U << 12U));
    GPIOA->AFR[0] |=  ((7U << 8U) | (7U << 12U));

    /* Push-pull, no pull-up/down */
    GPIOA->OTYPER &= ~((1U << 2U) | (1U << 3U));
    GPIOA->PUPDR &= ~((3U << 4U) | (3U << 6U));

    /* Reset and configure USART2 */
    RCC->APB1RSTR |= RCC_APB1RSTR_USART2RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_USART2RST;

    USART2->CR1 = 0U;
    USART2->CR2 = 0U;
    USART2->CR3 = 0U;

    /*
     * For USART2 peripheral clock = 16 MHz:
     * BRR = 16,000,000 / 9,600 ˜ 1667
     */
    USART2->BRR = 1667U;

    /* Enable transmitter, receiver and USART */
    USART2->CR1 = USART_CR1_TE |
                  USART_CR1_RE |
                  USART_CR1_UE;
}

static void USART2_SendChar(char c)
{
    while ((USART2->SR & USART_SR_TXE) == 0U)
    {
    }

    USART2->DR = (uint8_t)c;
}

static void USART2_SendString(const char *s)
{
    while (*s != '\0')
    {
        USART2_SendChar(*s++);
    }
}

static void USART2_SendMeasurements(void)
{
    char line[80];

    /*
     * Python format:
     * sample,voltage_mV,current_deci_mA
     *
     * voltage_mV: voltage in millivolts
     * current_deci_mA: current in tenths of a mA
     */
    int voltage_mV = (int)(voltage_V * 1000.0f);
    int current_deci_mA = (int)(current_mA * 10.0f);

    int length = snprintf(
        line,
        sizeof(line),
        "%lu,%d,%d\r\n",
        (unsigned long)sample_number,
        voltage_mV,
        current_deci_mA
    );

    if (length > 0 && length < (int)sizeof(line))
    {
        USART2_SendString(line);
    }
}

/* ---------------- I2C ---------------- */

static int I2C_Wait(uint32_t mask)
{
    uint32_t timeout = 500000U;

    while (timeout-- != 0U)
    {
        uint32_t status = I2C1->SR1;

        if ((status & SR1_ERRORS) != 0U)
        {
            return 0;
        }

        if ((status & mask) == mask)
        {
            return 1;
        }
    }

    return 0;
}

static void I2C_Stop(void)
{
    I2C1->CR1 |= I2C_CR1_STOP;
}

static void I2C1_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

    (void)RCC->AHB1ENR;
    (void)RCC->APB1ENR;

    /* PB6/PB7 alternate-function mode */
    GPIOB->MODER &= ~((3U << 12U) | (3U << 14U));
    GPIOB->MODER |=  ((2U << 12U) | (2U << 14U));

    /* Open-drain */
    GPIOB->OTYPER |= (1U << 6U) | (1U << 7U);

    /* Pull-ups */
    GPIOB->PUPDR &= ~((3U << 12U) | (3U << 14U));
    GPIOB->PUPDR |=  ((1U << 12U) | (1U << 14U));

    /* High speed */
    GPIOB->OSPEEDR |= (3U << 12U) | (3U << 14U);

    /* AF4 = I2C1 */
    GPIOB->AFR[0] &= ~((15U << 24U) | (15U << 28U));
    GPIOB->AFR[0] |=  ((4U << 24U) | (4U << 28U));

    /* Reset peripheral */
    RCC->APB1RSTR |= RCC_APB1RSTR_I2C1RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C1RST;

    I2C1->CR1 = 0U;

    /* Assumes APB1 clock is 16 MHz */
    I2C1->CR2 = 16U;
    I2C1->CCR = 80U;
    I2C1->TRISE = 17U;

    I2C1->CR1 = I2C_CR1_PE;
}

/* Write one 16-bit INA219 register */
static int INA_Write(uint8_t reg, uint16_t value)
{
    I2C1->CR1 |= I2C_CR1_START;

    if (!I2C_Wait(SR1_SB))
        goto fail;

    I2C1->DR = (uint8_t)(INA_ADDR << 1);

    if (!I2C_Wait(SR1_ADDR))
        goto fail;

    (void)I2C1->SR1;
    (void)I2C1->SR2;

    I2C1->DR = reg;

    if (!I2C_Wait(SR1_TXE))
        goto fail;

    I2C1->DR = (uint8_t)(value >> 8);

    if (!I2C_Wait(SR1_TXE))
        goto fail;

    I2C1->DR = (uint8_t)value;

    if (!I2C_Wait(SR1_BTF))
        goto fail;

    I2C_Stop();
    return 1;

fail:
    I2C_Stop();
    return 0;
}

/* Read one 16-bit INA219 register */
static int INA_Read16(uint8_t reg, uint16_t *value)
{
    uint8_t high_byte;
    uint8_t low_byte;

    /* Select register */
    I2C1->CR1 |= I2C_CR1_START;

    if (!I2C_Wait(SR1_SB))
        goto fail;

    I2C1->DR = (uint8_t)(INA_ADDR << 1);

    if (!I2C_Wait(SR1_ADDR))
        goto fail;

    (void)I2C1->SR1;
    (void)I2C1->SR2;

    I2C1->DR = reg;

    if (!I2C_Wait(SR1_BTF))
        goto fail;

    /* Repeated START for reading */
    I2C1->CR1 |= I2C_CR1_START;

    if (!I2C_Wait(SR1_SB))
        goto fail;

    I2C1->DR = (uint8_t)((INA_ADDR << 1) | 1U);

    if (!I2C_Wait(SR1_ADDR))
        goto fail;

    /* Prepare a two-byte receive */
    I2C1->CR1 |= I2C_CR1_POS;
    I2C1->CR1 &= ~I2C_CR1_ACK;

    (void)I2C1->SR1;
    (void)I2C1->SR2;

    if (!I2C_Wait(SR1_BTF))
        goto fail;

    I2C_Stop();

    high_byte = (uint8_t)I2C1->DR;
    low_byte = (uint8_t)I2C1->DR;

    I2C1->CR1 &= ~I2C_CR1_POS;
    I2C1->CR1 |= I2C_CR1_ACK;

    *value = ((uint16_t)high_byte << 8U) | low_byte;

    return 1;

fail:
    I2C_Stop();
    I2C1->CR1 &= ~I2C_CR1_POS;
    I2C1->CR1 |= I2C_CR1_ACK;
    return 0;
}

/* Configure INA219 */
static int INA_Init(void)
{
    uint16_t config;

    if (!INA_Read16(REG_CONFIG, &config))
    {
        i2c_error = 1U;
        return 0;
    }

    /* Continuous shunt and bus measurement */
    if (!INA_Write(REG_CONFIG, 0x399FU))
    {
        i2c_error = 2U;
        return 0;
    }

    /* Calibration for assumed shunt and current LSB */
    if (!INA_Write(REG_CALIB, 4096U))
    {
        i2c_error = 3U;
        return 0;
    }

    i2c_error = 0U;
    return 1;
}

/* Read voltage and current */
static void INA_ReadMeasurements(void)
{
    uint16_t current_raw;

    if (!INA_Read16(REG_BUS, (uint16_t *)&bus_raw_watch))
    {
        sensor_ok = 0U;
        i2c_error = 4U;
        return;
    }

    if (!INA_Read16(REG_CURRENT, &current_raw))
    {
        sensor_ok = 0U;
        i2c_error = 5U;
        return;
    }

    voltage_V = (float)(bus_raw_watch >> 3U) * 0.004f;

    current_mA = (float)((int16_t)current_raw) * 0.1f;

    power_mW = voltage_V * current_mA;

    sensor_ok = 1U;
    i2c_error = 0U;
    sample_number++;
}

/* Approximate delay */
static void Delay(void)
{
    for (volatile uint32_t i = 0U; i < 4000000U; i++)
    {
        __NOP();
    }
}

/* ---------------- Main ---------------- */

int main(void)
{
    I2C1_Init();
    USART2_Init();

    /* Send CSV header once */
    USART2_SendString(
        "sample,voltage_mV,current_deci_mA\r\n"
    );

    while (1)
    {
        if (sensor_ok == 0U)
        {
            sensor_ok = (uint32_t)INA_Init();
        }

        if (sensor_ok != 0U)
        {
            INA_ReadMeasurements();

            if (sensor_ok != 0U)
            {
                USART2_SendMeasurements();
            }
        }

        Delay();
    }
}
