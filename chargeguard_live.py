
import re
import time
import csv
import os
import subprocess
from collections import defaultdict
from datetime import datetime

import serial
import matplotlib.pyplot as plt


# ==================================================
# CONFIGURATION
# ==================================================

SERIAL_PORT = "COM9"
BAUD_RATE = 9600

ADB = r"C:\Android\Sdk\platform-tools\adb.exe"

ADB_DEVICE = (
    "adb-10BE131CJ30009N-9SAYnX._adb-tls-connect._tcp"
)

REFRESH_INTERVAL = 5

CSV_FILE = "chargeguard_data.csv"


# ==================================================
# STORE INDIVIDUAL READINGS
# ==================================================

battery_current_samples = defaultdict(list)


# ==================================================
# GET PHONE BATTERY PERCENTAGE USING ADB
# ==================================================

def get_battery_percentage():

    try:

        result = subprocess.run(
            [
                ADB,
                "-s",
                ADB_DEVICE,
                "shell",
                "dumpsys",
                "battery"
            ],
            capture_output=True,
            text=True,
            timeout=5
        )

        match = re.search(
            r"^\s*level:\s*(\d+)",
            result.stdout,
            re.MULTILINE
        )

        if match:

            return int(match.group(1))

        else:

            print("Battery percentage not found.")

    except Exception as e:

        print("ADB Error:", e)

    return None


# ==================================================
# LOAD PREVIOUS READINGS FROM CSV
# ==================================================

def load_previous_data():

    if not os.path.exists(CSV_FILE):

        return

    with open(CSV_FILE, "r", newline="") as file:

        reader = csv.DictReader(file)

        for row in reader:

            try:

                battery = int(row["battery_percentage"])

                current = float(row["current_mA"])

                battery_current_samples[battery].append(
                    current
                )

            except (ValueError, KeyError):

                continue

    print("Previous readings loaded.")


# ==================================================
# CREATE OR OPEN CSV FILE
# ==================================================

load_previous_data()

file_exists = os.path.exists(CSV_FILE)

csv_file = open(
    CSV_FILE,
    "a",
    newline=""
)

csv_writer = csv.writer(csv_file)

if not file_exists or os.path.getsize(CSV_FILE) == 0:

    csv_writer.writerow([
        "timestamp",
        "sample",
        "battery_percentage",
        "voltage_V",
        "current_mA"
    ])

    csv_file.flush()


# ==================================================
# SET UP LIVE LINE GRAPH
# ==================================================

plt.ion()

fig, ax = plt.subplots(figsize=(10, 6))

line, = ax.plot(
    [],
    [],
    marker="o",
    linestyle="-",
    linewidth=1.5,
    markersize=4,
    label="Individual Current Readings"
)

ax.set_title(
    "ChargeGuard - Battery Percentage vs Charging Current"
)

ax.set_xlabel(
    "Phone Battery Percentage (%)"
)

ax.set_ylabel(
    "Charging Current (mA)"
)

ax.set_xlim(0, 100)

ax.grid(
    True,
    linestyle="--",
    alpha=0.6
)

ax.legend()

plt.tight_layout()


# ==================================================
# UPDATE GRAPH WITH EVERY INDIVIDUAL READING
# ==================================================

def update_graph():

    all_points = []

    # Collect all individual readings
    for percentage, currents in battery_current_samples.items():

        for current in currents:

            all_points.append(
                (percentage, current)
            )

    # Sort by battery percentage
    all_points.sort(
        key=lambda point: point[0]
    )

    battery_levels = [
        point[0] for point in all_points
    ]

    current_readings = [
        point[1] for point in all_points
    ]

    # Connect all individual points
    line.set_data(
        battery_levels,
        current_readings
    )

    ax.set_xlim(0, 100)

    ax.relim()

    ax.autoscale_view(
        scalex=False,
        scaley=True
    )

    fig.canvas.draw_idle()

    fig.canvas.flush_events()


# Display previously saved readings
update_graph()


# ==================================================
# MAIN PROGRAM
# ==================================================

ser = None

battery_percent = None

last_adb_update = 0

try:

    # Connect STM32 serial port
    ser = serial.Serial(
        SERIAL_PORT,
        BAUD_RATE,
        timeout=1
    )

    time.sleep(2)

    print("\n================================")
    print("       CHARGEGUARD STARTED")
    print("================================")

    print("Serial Port:", SERIAL_PORT)

    print("Baud Rate:", BAUD_RATE)

    print("CSV File:", CSV_FILE)

    print("Waiting for STM32 readings...\n")


    # ==================================================
    # CONTINUOUS READING LOOP
    # ==================================================

    while True:

        current_time = time.time()


        # ----------------------------------------------
        # UPDATE BATTERY PERCENTAGE EVERY 5 SECONDS
        # ----------------------------------------------

        if current_time - last_adb_update >= REFRESH_INTERVAL:

            new_percentage = get_battery_percentage()

            if new_percentage is not None:

                battery_percent = new_percentage

                print(
                    "Phone Battery:",
                    battery_percent,
                    "%"
                )

            last_adb_update = current_time


        # ----------------------------------------------
        # READ STM32 SERIAL DATA
        # ----------------------------------------------

        if ser.in_waiting:

            raw_data = ser.readline().decode(
                "utf-8",
                errors="ignore"
            ).strip()

            try:

                parts = raw_data.split(",")

                # Expected format: sample,voltage_mV,current_deci_mA
                if len(parts) != 3:

                    continue

                sample = int(parts[0])

                voltage_mV = int(parts[1])

                current_deci_mA = int(parts[2])


                # Convert sensor values
                voltage_V = voltage_mV / 1000.0

                current_mA = current_deci_mA / 10.0


                # --------------------------------------
                # SAVE AND PLOT READING
                # --------------------------------------

                if battery_percent is not None:

                    timestamp = datetime.now().strftime(
                        "%Y-%m-%d %H:%M:%S"
                    )


                    # Save individual reading to CSV
                    csv_writer.writerow([
                        timestamp,
                        sample,
                        battery_percent,
                        voltage_V,
                        current_mA
                    ])

                    csv_file.flush()


                    # Store every individual reading
                    battery_current_samples[
                        battery_percent
                    ].append(current_mA)


                    # Print reading
                    print(
                        f"Sample: {sample} | "
                        f"Battery: {battery_percent}% | "
                        f"Voltage: {voltage_V:.3f} V | "
                        f"Current: {current_mA:.1f} mA | "
                        f"Saved"
                    )


                    # Update live graph
                    update_graph()


            except ValueError:

                # Ignore headers and invalid serial data
                continue


        # Keep graph responsive
        plt.pause(0.01)


# ==================================================
# HANDLE STOPPING
# ==================================================

except KeyboardInterrupt:

    print("\nChargeGuard stopped by user.")


except serial.SerialException as e:

    print("\nSerial connection error:", e)


# ==================================================
# CLOSE CONNECTIONS AND SAVE DATA
# ==================================================

finally:

    if ser is not None and ser.is_open:

        ser.close()

    csv_file.close()

    plt.ioff()

    plt.show()

    print("\nAll readings saved successfully.")

    print("CSV file:", CSV_FILE)
