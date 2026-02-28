# Alpac Helty Flow VMC - Home Assistant Integration (Modbus)

This project aims to replicate the integration of Alpac Helty Flow VMC (Mechanical Extract Ventilation) units with Home Assistant using Modbus RTU over RS485. This allows for comprehensive control and monitoring of the VMC unit via Home Assistant.

The foundation of this project is inspired by the excellent work shared in the Home Assistant community forum: [Alpac Helty Flow VMC - The modbus way](https://community.home-assistant.io/t/alpac-helty-flow-vmc-the-modbus-way/578774).

## Features

*   **VMC Control:** Adjust fan speed and operational modes of the Helty Flow VMC unit.
*   **Sensor Monitoring:** Read internal and external temperatures, and other operational parameters from the VMC.
*   **Alarm Status:** Monitor alarm flags, such as filter change indications.
*   **OTA Updates:** Convenient Over-The-Air (OTA) firmware updates for the NodeMCU.
*   **Home Assistant Integration:** Seamless integration with Home Assistant for automation, dashboards, and logging.

## Hardware Requirements

To set up this integration, you will need the following hardware components:

*   **Alpac Helty Flow Compact VMC:** Ensure your unit has a Modbus RS485 RTU interface. The 7-pin onboard connector typically provides Modbus pins at position 5 (data+) and 4 (data-).
*   **NodeMCU ESP8266 ESP-12F:** A popular Wi-Fi enabled microcontroller for IoT projects.
*   **RS-485 TTL to RS485 Converter:** A module like the ANGEEK MAX485 module is required to convert TTL signals from the NodeMCU to RS485 for the VMC.
*   **Wiring Components:**
    *   Single-sided breadboard
    *   15-pin headers for NodeMCU
    *   4-pin headers for the Modbus converter
    *   Two-pole terminal block
    *   Shielded 2-wire cable (e.g., 4x0.22 mmq used for alarm systems) for connecting to the VMC's Modbus interface.
*   **Power Supply:** A 5V power adapter for the NodeMCU. Optionally, a mini DC-DC converter can be used to power the NodeMCU directly from the VMC's 24V power supply.

## Software Requirements

*   **Arduino IDE:** Version 2.1.0 or compatible, used for compiling and uploading the NodeMCU firmware (`.ino` file).
*   **Arduino Libraries:** The NodeMCU firmware relies on the following libraries. Install them via the Arduino Library Manager:
    *   `ESP8266WiFi` (usually comes with ESP8266 board support)
    *   `PubSubClient` (by Nick O'Leary)
    *   `ArduinoJson` (by Benoit Blanchon)
    *   `ESPAsyncTCP` (by dvarrel)
    *   `ESPAsyncWebServer` (by Ayush Sharma)
    *   `ElegantOTA` (by Ayush Sharma)
    *   `ModbusRTU` (by Alexander Emelianov)
    *   `SoftwareSerial` (usually built-in)

*   **MQTT Server:** A running MQTT broker (e.g., Mosquitto) is essential for communication between the NodeMCU and Home Assistant.
*   **Home Assistant:** Your Home Assistant instance to integrate and manage the VMC.

## Wiring and Connections

**(Note: Please refer to the original Home Assistant community thread for detailed diagrams and images.)**

The NodeMCU will connect to the RS485 converter, which then connects to the VMC unit.

*   **NodeMCU Pinout (Example):**
    *   `RE_DE` (RS485 Transmit/Receive Enable): Connect to NodeMCU GPIO4 (D2)
    *   `RX` (RS485 Receive): Connect to NodeMCU GPIO12 (D6)
    *   `TX` (RS485 Transmit): Connect to NodeMCU GPIO13 (D7)
*   **RS485 Converter to VMC:**
    *   Connect the Data+ (A) and Data- (B) terminals of the RS485 converter to the corresponding Modbus pins on your Alpac Helty Flow VMC unit (typically pin 5 for Data+ and pin 4 for Data- on the 7-pin onboard connector).

## NodeMCU Firmware Setup

1.  **Install Arduino IDE and Board Support:** Set up your Arduino IDE. Ensure you have the ESP8266 board support installed via the Boards Manager. For detailed instructions, refer to the documentation: [AZ197_A_8_7_EN_B07K24YQZQ_5e65f752_957e_4af5_9fd5_a7a1678ad12c.pdf](docs/AZ197_A_8_7_EN_B07K24YQZQ_5e65f752_957e_4af5_9fd5_a7a1678ad12c.pdf)
2.  **Install Arduino Libraries:** Install all the required libraries listed in the "Software Requirements" section using the Arduino Library Manager (`Sketch > Include Library > Manage Libraries...`).
    *   **Special Configuration for `ElegantOTA`:** After installing `ElegantOTA`, you must enable its asynchronous mode:
        1.  Navigate to your Arduino libraries directory (usually `Documents/Arduino/libraries`).
        2.  Find the `ElegantOTA` folder, then open its `src` subfolder.
        3.  Open the file **`ElegantOTA.h`** in a text editor.
        4.  Change the line `#define ELEGANTOTA_USE_ASYNC_WEBSERVER 0` to `#define ELEGANTOTA_USE_ASYNC_WEBSERVER 1`.
        5.  Save the file and restart your Arduino IDE.
3.  **Open the Firmware:** Open the provided `.ino` firmware file (`helty-nodemcu-firmware.ino`) in the Arduino IDE.
4.  **Configure `secrets.h`:**
    *   Locate the `helty-nodemcu-firmware/secrets.h.template` file.
    *   **Copy** this file and rename the copy to `helty-nodemcu-firmware/secrets.h`.
    *   **Edit** `helty-nodemcu-firmware/secrets.h` and update your actual Wi-Fi credentials (`ssid`, `password`) and MQTT server details (`mqttServer`, `mqttPort`, `mqttUser`, `mqttPassword`). **Do not commit `secrets.h` to version control.**
5.  **Review Configuration Parameters:**
    *   Define a unique `ESP_DEVICE_NAME` for your NodeMCU (e.g., `VMC_Letto`).
    *   Adjust the `MODBUS_SLAVE_ID` if your VMC unit uses a different ID than the default (often `2` in the example).
    *   Verify or adjust the `RE_DE`, `RX`, `TX` pin definitions if your wiring differs.
    *   Review the Modbus register definitions (`SPEED_HREG`, `INTTEMP_IREG`, `EXTTEMP_IREG`, `ALARM_IREG`) to ensure they match your VMC's Modbus map.
6.  **Upload Firmware:** Connect your NodeMCU to your computer via USB and upload the firmware. The first upload must be wired.
7.  **OTA Updates:** After the initial upload, you can perform subsequent updates wirelessly:
    *   In the Arduino IDE, go to **Sketch > Export Compiled Binary** to generate a `.bin` file.
    *   Open a web browser and navigate to `http://<ESP_IP>/update` (replace `<ESP_IP>` with your NodeMCU's actual IP address).
    *   Upload the generated `.bin` file through the web interface.

### Modbus Communication Parameters

The VMC communicates using Modbus RTU over RS485 with the following parameters:

*   **Baud Rate:** 19200
*   **Data Bits:** 8
*   **Parity:** None (N)
*   **Stop Bits:** 1
*   **Format:** 8N1

## Home Assistant Configuration

Once the NodeMCU is running and connected to your MQTT broker, Home Assistant can be configured to interact with it.

*   **MQTT Discovery:** If your NodeMCU firmware supports MQTT discovery, Home Assistant should automatically detect and configure the VMC entities.
*   **Manual Configuration:** Otherwise, you will need to manually configure MQTT sensors, switches, and other entities in your `configuration.yaml` based on the MQTT topics published by the NodeMCU.
*   **Example Entities:** You can create entities like `fan.vmc_sala` for control and sensors for temperature readings.
*   **Dashboards:** Examples of dashboards, such as those using `button-card` templates, can be found in the original thread to create a user-friendly interface for controlling the VMC and displaying its status.

## Modbus Specifics

*   **Slave Address:** The Modbus slave ID typically ranges from 1 to 247. The default for the VMC in the example is often 2.
*   **Function Codes Implemented:**
    *   Read Input Registers (0x04)
    *   Read Holding Registers (0x03)
    *   Write Single Register (0x06)
    *   Write Multiple Registers (0x10)
*   **Data Model:**
    *   **Input Registers:** Read-only 16-bit words (e.g., temperatures, alarms).
    *   **Holding Registers:** Read/write 16-bit words (e.g., fan speed).
*   **Temperature Values:** Temperatures are usually reported in 0.1 °C units, requiring a `multiply: 0.1` filter in Home Assistant/ESPHome configurations.

## Troubleshooting and Notes

*   **Multiple VMC Units:** If you have multiple VMC units, each requires a unique Modbus ID. Changing these IDs often requires intervention from an Alpac technician.
*   **Termination Resistors:** 120-ohm termination resistors are not typically needed for a simple 1-to-1 connection between the NodeMCU and VMC but are crucial for daisy-chained Modbus networks.

## Acknowledgements

Special thanks to the Home Assistant community, especially the author of the original post on the Home Assistant forum, for providing the detailed guide that made this project possible.

---
*This README was generated with assistance from an AI.*