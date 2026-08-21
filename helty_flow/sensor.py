"""Sensor platform for Helty Flow VMC."""
import json
import logging

from homeassistant.components.sensor import (
    SensorDeviceClass,
    SensorEntity,
    SensorStateClass,
)
from homeassistant.components.mqtt import async_subscribe
from homeassistant.config_entries import ConfigEntry
from homeassistant.const import UnitOfTemperature
from homeassistant.core import HomeAssistant, callback
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.entity import DeviceInfo

from .const import (
    DOMAIN,
    CONF_DEVICE_ID,
    CONF_NAME,
    TOPIC_STATE,
    TOPIC_INFO,
    TOPIC_VERSION,
)

_LOGGER = logging.getLogger(__name__)

async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up the Helty Flow VMC sensors from a config entry."""
    data = hass.data[DOMAIN][config_entry.entry_id]
    devices = data.get("devices", [])
    
    entities = []
    for device_data in devices:
        device_id = device_data[CONF_DEVICE_ID]
        name = device_data[CONF_NAME]
        
        entities.extend([
            HeltyFlowSpeedSensor(hass, device_id, name),
            HeltyFlowVersionSensor(hass, device_id, name),
            HeltyFlowTempSensor(hass, device_id, name, "Internal Temperature", "IntTemperature"),
            HeltyFlowTempSensor(hass, device_id, name, "External Temperature", "ExtTemperature"),
            HeltyFlowAlarmSensor(hass, device_id, name),
        ])

    async_add_entities(entities)


class HeltyFlowBaseSensor(SensorEntity):
    """Base class for Helty Flow sensors."""

    _attr_should_poll = False
    _attr_has_entity_name = True

    def __init__(self, hass: HomeAssistant, device_id: str, device_name: str, sensor_name: str) -> None:
        """Initialize the sensor."""
        self.hass = hass
        self._device_id = device_id
        self._device_name = device_name
        self._sensor_name = sensor_name
        self._attr_unique_id = f"helty_flow_{device_id}_{sensor_name.lower().replace(' ', '_')}"
        self._state = None

    @property
    def name(self) -> str:
        """Return the name of the sensor."""
        return self._sensor_name

    @property
    def device_info(self) -> DeviceInfo:
        """Return device information."""
        return DeviceInfo(
            identifiers={(DOMAIN, self._device_id)},
            name=self._device_name,
            manufacturer="Alpac Helty",
            model="Flow Compact VMC",
        )


class HeltyFlowSpeedSensor(HeltyFlowBaseSensor):
    """Representation of the VMC speed state sensor."""

    def __init__(self, hass: HomeAssistant, device_id: str, name: str) -> None:
        """Initialize."""
        super().__init__(hass, device_id, name, "Speed State")
        self._topic = TOPIC_STATE.format(device_id=device_id)

    @property
    def native_value(self):
        """Return the state of the sensor."""
        return self._state

    async def async_added_to_hass(self) -> None:
        """Subscribe to MQTT topic."""
        @callback
        def message_received(msg):
            """Handle new MQTT messages."""
            self._state = msg.payload
            self.async_write_ha_state()

        await async_subscribe(self.hass, self._topic, message_received)


class HeltyFlowTempSensor(HeltyFlowBaseSensor):
    """Representation of VMC temperature sensors."""

    _attr_device_class = SensorDeviceClass.TEMPERATURE
    _attr_state_class = SensorStateClass.MEASUREMENT
    _attr_native_unit_of_measurement = UnitOfTemperature.CELSIUS

    def __init__(self, hass: HomeAssistant, device_id: str, name: str, sensor_name: str, json_key: str) -> None:
        """Initialize."""
        super().__init__(hass, device_id, name, sensor_name)
        self._json_key = json_key
        self._topic = TOPIC_INFO.format(device_id=device_id)

    @property
    def native_value(self):
        """Return the state of the sensor."""
        return self._state

    async def async_added_to_hass(self) -> None:
        """Subscribe to MQTT topic."""
        @callback
        def message_received(msg):
            """Handle new MQTT messages."""
            try:
                data = json.loads(msg.payload)
                if self._json_key in data:
                    self._state = data[self._json_key]
                    self.async_write_ha_state()
            except (json.JSONDecodeError, TypeError):
                _LOGGER.error("Invalid JSON received on %s", self._topic)

        await async_subscribe(self.hass, self._topic, message_received)


class HeltyFlowAlarmSensor(HeltyFlowBaseSensor):
    """Representation of the VMC alarm status sensor."""

    def __init__(self, hass: HomeAssistant, device_id: str, name: str) -> None:
        """Initialize."""
        super().__init__(hass, device_id, name, "Alarm Status")
        self._topic = TOPIC_INFO.format(device_id=device_id)

    @property
    def native_value(self):
        """Return the state of the sensor."""
        return self._state

    async def async_added_to_hass(self) -> None:
        """Subscribe to MQTT topic."""
        @callback
        def message_received(msg):
            """Handle new MQTT messages."""
            try:
                data = json.loads(msg.payload)
                if "Alarm" in data:
                    self._state = data["Alarm"]
                    self.async_write_ha_state()
            except (json.JSONDecodeError, TypeError):
                _LOGGER.error("Invalid JSON received on %s", self._topic)

        await async_subscribe(self.hass, self._topic, message_received)


class HeltyFlowVersionSensor(HeltyFlowBaseSensor):
    """Representation of the VMC firmware version sensor."""

    def __init__(self, hass: HomeAssistant, device_id: str, name: str) -> None:
        """Initialize."""
        super().__init__(hass, device_id, name, "Firmware Version")
        self._topic = TOPIC_VERSION.format(device_id=device_id)

    @property
    def native_value(self):
        """Return the state of the sensor."""
        return self._state

    async def async_added_to_hass(self) -> None:
        """Subscribe to MQTT topic."""
        @callback
        def message_received(msg):
            """Handle new MQTT messages."""
            self._state = str(msg.payload)
            self.async_write_ha_state()

        await async_subscribe(self.hass, self._topic, message_received)
