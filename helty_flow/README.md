# Helty Flow VMC Home Assistant Integration

> [!CAUTION]
> **Disclaimer: This is not an official integration from Helty or Alpac.** This is a community-developed custom component and is not supported or endorsed by the hardware manufacturers.

This integration allows you to control and monitor Helty Flow VMC units that are running the [helty-nodemcu-firmware](https://github.com/helty-flow-iot) via MQTT.

## Features

- **Automatic Discovery:** Scans your MQTT broker for VMC units automatically.
- **Fan Control:** Set fan speeds (1-4) and preset modes (Off, Hyper Speed, Night Mode, Free Cooling, Normal).
- **Sensors:** Monitor internal/external temperatures and alarm status.
- **Multiple Devices:** Supports multiple VMC units in a single Home Assistant instance.

## Installation

1. Copy the `ha-helty-flow` folder into your Home Assistant's `custom_components/` directory.
2. Rename the folder to `helty_flow`.
3. Restart Home Assistant.
4. Go to **Settings > Devices & Services > Add Integration** and search for "Helty Flow VMC".

## Requirements

- A Helty Flow VMC unit with a Modbus-to-MQTT bridge running the compatible firmware.
- A functional MQTT broker already configured in Home Assistant.
