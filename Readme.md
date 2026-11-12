Student Name(s) and Registration Number(s): CB.SC.U4CSE24520, 
                                            CB.SC.U4CSE2436, 
                                            CB.SC.U4CSE24509, 
                                            CB.SC.U4CSE24533 


Mentor: Dr. A. Baskar

Course: 23CSE304 – Embedded Systems

Department: Department of Computer Science and Engineering

Institution: School of Computing, Amrita Vishwa Vidyapeetham, Coimbatore, India

# ChargeGuard – STM32F401 Smart Charging Monitor

ChargeGuard is an embedded-based charging monitoring and power analysis system developed using the STM32F401 microcontroller and INA219 sensor.

The system measures USB charging voltage and current, transmits the readings to a computer through UART, retrieves the phone's battery percentage using Android Debug Bridge (ADB), and displays the charging data through a live Python graph.

## Project Structure

```text
ChargeGuard/
│
├── README.md
├── chargeguard_live.py
├── Phone_current/
├── Data/
├── Demo/
├── Report/
└── Output_Screenshots/
```

- **Phone_current:** STM32F401 firmware and Keil µVision 5 project files.
- **Data:** Collected charging data in CSV format.
- **Demo:** Working demonstration video or video link.
- **Report:** Complete project report in PDF format.
- **Output_Screenshots:** Hardware connections, debugging results, terminal output and graphs.
- **chargeguard_live.py:** Python application for serial data reception and live visualisation.

## 1. Hardware Requirements

- STM32F401CCU6 Black Pill development board.
- INA219 voltage and current sensor.
- USB male and female breakout boards.
- USB-to-TTL converter.
- USB charging adapter and cable.
- Android phone.
- Jumper wires.
- ST-LINK debugger.

## 2. Hardware Connections

### INA219 to STM32F401

Connect the INA219 sensor using I2C communication.

- INA219 VCC → STM32 3.3V
- INA219 GND → STM32 GND
- INA219 SCL → PB6
- INA219 SDA → PB7

### USB Charging Connections

Connect the INA219 in series with the positive USB charging supply.

- Charger-side positive → INA219 VIN+
- INA219 VIN− → Phone-side positive
- Connect the USB ground lines together.
- Pass the USB data lines (D+ and D−) through the breakout connections.

The sensor measures voltage and current in the USB charging path, not the phone's internal battery current.

### UART Connections

Connect the USB-to-TTL converter to the STM32F401.

- STM32 PA2 (USART2 TX) → Converter RX
- STM32 PA3 (USART2 RX) → Converter TX
- STM32 GND → Converter GND

Use a 3.3V TTL-compatible converter.

## 3. Firmware Setup Using Keil µVision 5

1. Connect the STM32F401 board to the computer using ST-LINK.
2. Open the `Phone_current` folder.
3. Open the Keil project file (`.uvprojx`).
4. Check that all required source files and header files are included in the project.
5. Build the project using the Build option.
6. Resolve any compilation errors.
7. Open the Debug settings and select the ST-LINK debugger.
8. Download the firmware to the STM32F401.
9. Enter Debug mode.
10. Open the Watch window and add the following variables:

```text
sensor_ok
i2c_error
voltage_V
current_mA
power_mW
sample_number
battery_percent_manual
```

11. Run the program and observe the sensor readings.

The variable `sensor_ok` should become `1` when the INA219 sensor is successfully initialised.

Check that the voltage and current readings are changing as expected.

After verifying the firmware, allow it to run continuously so that UART readings can be received by the Python application. Do not leave the debugger paused while collecting data.

## 4. Python Application Setup

### Install Python

Install Python 3 if it is not already installed.

Open PowerShell or the VS Code terminal and install the required libraries:

```bash
pip install pyserial matplotlib
```

### Connect the USB-to-TTL Converter

1. Connect the USB-to-TTL converter to the computer.
2. Open Windows Device Manager.
3. Expand Ports (COM & LPT).
4. Identify the COM port assigned to the converter.
5. Open `chargeguard_live.py` in Visual Studio Code.
6. Set the correct COM port and baud rate in the Python application.

For example:

```python
COM_PORT = "COM9"
BAUD_RATE = 9600
```

Use the actual COM port shown on your computer. The baud rate must match the STM32 firmware configuration.

### Run the Python Application

Open the terminal in the project folder and execute:

```bash
python chargeguard_live.py
```

The application will receive the sensor readings through UART and process the incoming data.

## 5. Android Battery Percentage Setup Using ADB

ADB is used to retrieve the battery percentage from the Android phone.

### Enable Wireless Debugging

1. Enable Developer Options on the Android phone.
2. Open Developer Options.
3. Enable Wireless Debugging.
4. Connect the phone and computer to the same Wi-Fi network.
5. Select Pair device with pairing code.

### Pair the Phone

Open PowerShell and execute:

```powershell
& "C:\Android\Sdk\platform-tools\adb.exe" pair IP_ADDRESS:PAIRING_PORT
```

Enter the pairing code displayed on the phone.

### Connect the Phone

Use the connection IP address and connection port shown in Wireless Debugging:

```powershell
& "C:\Android\Sdk\platform-tools\adb.exe" connect IP_ADDRESS:CONNECTION_PORT
```

Check whether the phone is connected:

```powershell
& "C:\Android\Sdk\platform-tools\adb.exe" devices
```

The connected device should appear in the device list.

To check the battery information manually:

```powershell
& "C:\Android\Sdk\platform-tools\adb.exe" shell dumpsys battery
```

The output contains the phone's battery level.

The Python application uses ADB to retrieve this percentage during monitoring.

**Note:** Replace the IP address and ports with the values displayed on your phone. The pairing port and connection port are different.

## 6. Running the Complete System

Follow these steps to run ChargeGuard:

1. Assemble the hardware connections.
2. Connect the INA219 sensor to the STM32F401.
3. Connect the USB charging path through the sensor.
4. Connect the ST-LINK debugger and program the microcontroller using Keil.
5. Verify successful sensor initialisation and readings.
6. Connect the USB-to-TTL converter to the computer.
7. Identify the correct COM port.
8. Enable Wireless Debugging on the Android phone.
9. Pair and connect the phone using ADB.
10. Open the Python application in VS Code.
11. Verify the COM port and baud rate.
12. Run `chargeguard_live.py`.
13. Observe the incoming readings in the terminal.
14. Monitor the live graph showing charging current against battery percentage.
15. Check the collected CSV data in the `Data` folder.

## 7. Output

The system provides the following outputs:

- USB charging voltage readings.
- USB charging current readings.
- Calculated power readings.
- Mobile phone battery percentage.
- Live graph of charging current against battery percentage.
- CSV file containing collected readings.

The graph uses:

- **X-axis:** Battery percentage (%).
- **Y-axis:** Charging current (mA).

Individual current readings are plotted and connected using a line to observe variations during charging.

## 8. Troubleshooting

### INA219 Sensor Not Detected

- Check the VCC and GND connections.
- Verify the I2C connections on PB6 and PB7.
- Check whether the sensor address is `0x40`.
- Verify the `sensor_ok` and `i2c_error` variables in Keil.

### No UART Readings

- Verify PA2 is connected to converter RX.
- Check the common ground connection.
- Confirm the correct COM port.
- Ensure the baud rate matches the firmware.
- Close other applications using the same COM port.

### ADB Device Not Detected

- Check that Wireless Debugging is enabled.
- Ensure both devices are on the same Wi-Fi network.
- Verify the pairing and connection ports.
- Refresh the pairing code if necessary.
- Check the output of `adb devices`.

### Python Application Not Running

Install the required libraries:

```bash
pip install pyserial matplotlib
```

Verify the COM port and check that the STM32 is transmitting valid serial data.

## 9. Project Resources

- **Project Report:** Available in the `Report` folder.
- **Working Demonstration:** Available in the `Demo` folder or through the video link provided there.
- **Output Screenshots:** Available in the `Output_Screenshots` folder.
- **Collected Dataset:** Available in the `Data` folder.

## 10. Future Enhancements

- Automatic charging cut-off based on configurable conditions.
- Charging threshold alerts.
- Additional charging analysis and visualisation.
- Improved monitoring interface.

---

**Developed as an Embedded Systems Project using STM32F401, INA219, UART, I2C, Python and Android Debug Bridge (ADB).**
