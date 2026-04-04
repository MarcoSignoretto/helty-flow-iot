# Alpac Helty Flow VMC - Home Assistant Integration (Modbus)

This project provides a complete solution for integrating Alpac Helty Flow VMC (Mechanical Extract Ventilation) units with Home Assistant. It consists of two main parts:
1.  **NodeMCU Firmware:** A Modbus-to-MQTT bridge running on an ESP8266.
2.  **Home Assistant Custom Integration:** A user-friendly component that automatically discovers and controls your VMC units.

> [!TIP]
> **New:** Use the [helty_flow](https://github.com/MarcoSignoretto/helty-flow-iot/blob/master/helty_flow/README.md) custom integration for a seamless, UI-based setup. No manual YAML configuration is required!

The foundation of this project is inspired by the excellent work shared in the Home Assistant community forum: [Alpac Helty Flow VMC - The modbus way](https://community.home-assistant.io/t/alpac-helty-flow-vmc-the-modbus-way/578774).

## Features

*   **Custom Integration:** Easy setup via the Home Assistant UI with automatic device discovery.
*   **VMC Control:** Adjust fan speed and operational modes of the Helty Flow VMC unit.
*   **Sensor Monitoring:** Read internal and external temperatures, and other operational parameters from the VMC.
*   **Alarm Status:** Monitor alarm flags, such as filter change indications.
*   **OTA Updates:** Convenient Over-The-Air (OTA) firmware updates for the NodeMCU.
*   **Home Assistant Integration:** Seamless integration with Home Assistant for automation, dashboards, and logging.

## Hardware Requirements

To set up this integration, you will need the following hardware components:

*   **Alpac Helty Flow Compact VMC:** Ensure your unit has a Modbus RS485 RTU interface. The 7-pin onboard connector provides Modbus pins at position 4 (Modbus RX +) and 5 (Modbus RX -).
*   **NodeMCU ESP8266 ESP-12F:** A popular Wi-Fi enabled microcontroller for IoT projects. [Example: AZ-Delivery NodeMCU Lua Lolin V3 Modul mit ESP8266-12E](https://www.az-delivery.de/it/products/nodemcu-lua-lolin-v3-modul-mit-esp8266-12e-unverlotet)
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
*   **Home Assistant:** Your Home Assistant instance with the MQTT integration configured.

## Wiring and Connections

**(Note: Please refer to the original Home Assistant community thread for detailed diagrams and images.)**

The NodeMCU will connect to the RS485 converter, which then connects to the VMC unit.

*   **NodeMCU Pinout (Example):**
    *   `RE_DE` (RS485 Transmit/Receive Enable): Connect to NodeMCU GPIO4 (D2)
    *   `RX` (RS485 Receive): Connect to NodeMCU GPIO12 (D6)
    *   `TX` (RS485 Transmit): Connect to NodeMCU GPIO5 (D1)
*   **RS485 Converter to VMC:**
    *   Connect the Data+ (A) and Data- (B) terminals of the RS485 converter to the VMC's Modbus pins as follows:
        *   RS485 Data+ (A) to VMC Pin 4 (Modbus RX +)
        *   RS485 Data- (B) to VMC Pin 5 (Modbus RX -)

## NodeMCU Firmware Setup

1.  **Install Arduino IDE and Board Support:** Set up your Arduino IDE. Ensure you have the ESP8266 board support installed via the Boards Manager. For detailed instructions, refer to the documentation: [AZ197_A_8_7_EN_B07K24YQZQ_5e65f752_957e_4af5_9fd5_a7a1678ad12c.pdf](docs/AZ197_A_8_7_EN_B07K24YQZQ_5e65f752_957e_4af5_9fd5_a7a1678ad12c.pdf)
2.  **Install Arduino Libraries:** Install all the required libraries listed in the "Software Requirements" section using the Arduino Library Manager (`Sketch > Include Library > Manage Libraries...`).
    *   **Special Configuration for `ElegantOTA`:** After installing `ElegantOTA`, you must enable its asynchronous mode:
        1.  Navigate to your Arduino libraries directory (usually `Documents/Arduino/libraries`).
        2.  Find the `ElegantOTA` folder, then open its `src` subfolder.
        3.  Open the file **`ElegantOTA.h`** in a text editor.
        4.  Change the line `#define ELEGANTOTA_USE_ASYNC_WEBSERVER 0` to `#define ELEGANTOTA_USE_ASYNC_WEBSERVER 1`.
        5.  Save the file and restart your Arduino IDE.
3.  **Open the Firmware:** Open the provided `.ino` firmware file (`helty-nodemcu-firmware/helty-nodemcu-firmware.ino`) in the Arduino IDE.
4.  **Configure `secrets.h`:**
    *   Locate the `helty-nodemcu-firmware/secrets.h.template` file.
    *   **Copy** this file and rename the copy to `helty-nodemcu-firmware/secrets.h`.
    *   **Edit** `helty-nodemcu-firmware/secrets.h` and update your actual Wi-Fi credentials (`ssid`, `password`) and MQTT server details (`mqttServer`, `mqttPort`, `mqttUser`, `mqttPassword`). **Do not commit `secrets.h` to version control.**
5.  **Review Configuration Parameters:**
    *   Define a unique `ESP_DEVICE_NAME` in `secrets.h` (e.g., `vmc_kitchen`, `vmc_bedroom`). This name will be used to generate device-specific MQTT topics (e.g., `vmcs/your_device_name/state`).
    *   Adjust the `MODBUS_SLAVE_ID` if your VMC unit uses a different ID than the default (often `2` in the example).
    *   Verify or adjust the `RE_DE`, `RX`, `TX` pin definitions if your wiring differs.
    *   Review the Modbus register definitions (`SPEED_HREG`, `INTTEMP_IREG`, `EXTTEMP_IREG`, `ALARM_IREG`) to ensure they match your VMC's Modbus map.
6.  **Upload Firmware:** Connect your NodeMCU to your computer via USB and upload the firmware. The first upload must be wired.
7.  **OTA Updates:** After the initial upload, you can perform subsequent updates wirelessly:
    *   In the Arduino IDE, go to **Sketch > Export Compiled Binary** to generate a `.bin` file.
    *   Open a web browser and navigate to `http://<ESP_IP>/update` (replace `<ESP_IP>` with your NodeMCU's actual IP address).
    *   Upload the generated `.bin` file through the web interface.

## Home Assistant Configuration

Once the NodeMCU is running and connected to your MQTT broker, you can add it to Home Assistant using the provided custom integration.

### Installation

1.  Copy the `helty_flow` folder into your Home Assistant's `custom_components/` directory.
2.  Restart Home Assistant.
3.  In Home Assistant, go to **Settings > Devices & Services**.
4.  Click **Add Integration** and search for **Helty Flow VMC**.
5.  Follow the on-screen instructions. You can either:
    *   **Scan automatically:** The integration will listen for active VMC units on your MQTT broker.
    *   **Manual configuration:** Enter the `ESP_DEVICE_NAME` you defined in your `secrets.h`.

### Features of the Integration

*   **Automatic Discovery:** No need to manually configure YAML sensors or fans.
*   **Unified Device:** Each VMC appears as a single device in Home Assistant with all its sensors and fan controls grouped together.
*   **Rich Control:** Full support for fan speeds and preset modes (Hyper Speed, Night Mode, etc.).

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
*   **Temperature Values:** Temperatures are usually reported in 0.1 °C units. The integration handles this scaling automatically.

## Troubleshooting and Notes

*   **Multiple VMC Units:** Each unit requires a unique `ESP_DEVICE_NAME` in its firmware configuration.
*   **MQTT Connection:** Ensure Home Assistant's MQTT integration is correctly configured and connected to the same broker as the NodeMCU.

## Acknowledgements

Special thanks to the Home Assistant community, especially the author of the original post on the Home Assistant forum, for providing the detailed guide that made this project possible.

---
*This README was updated to reflect the new custom integration.*
