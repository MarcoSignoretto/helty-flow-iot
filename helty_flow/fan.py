"""Fan platform for Helty Flow VMC."""
import logging
from typing import Any

from homeassistant.components.fan import (
    FanEntity,
    FanEntityFeature,
)
from homeassistant.components.mqtt import async_publish, async_subscribe
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, callback
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.entity import DeviceInfo
from homeassistant.util.percentage import (
    int_states_in_range,
    percentage_to_ranged_value,
    ranged_value_to_percentage,
)

from .const import (
    DOMAIN,
    CONF_DEVICE_ID,
    CONF_NAME,
    TOPIC_STATE,
    TOPIC_FAN_SPEED,
    TOPIC_CMD_SPEED,
    PRESET_MODES,
    MODE_TO_VAL,
    VAL_TO_MODE,
)

_LOGGER = logging.getLogger(__name__)

SPEED_RANGE = (1, 4)

async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up the Helty Flow VMC fans from a config entry."""
    data = hass.data[DOMAIN][config_entry.entry_id]
    devices = data.get("devices", [])
    
    entities = []
    for device_data in devices:
        device_id = device_data[CONF_DEVICE_ID]
        name = device_data[CONF_NAME]
        entities.append(HeltyFlowFan(hass, device_id, name, config_entry))

    async_add_entities(entities)


class HeltyFlowFan(FanEntity):
    """Representation of a Helty Flow VMC fan."""

    _attr_should_poll = False
    _attr_has_entity_name = True

    def __init__(self, hass: HomeAssistant, device_id: str, name: str, entry: ConfigEntry) -> None:
        """Initialize the fan."""
        self.hass = hass
        self._device_id = device_id
        self._device_name = name
        self._entry = entry
        self._state = None
        self._percentage = 0
        self._preset_mode = None
        self._attr_unique_id = f"helty_flow_{device_id}_fan"

        self._state_topic = TOPIC_STATE.format(device_id=device_id)
        self._fan_speed_topic = TOPIC_FAN_SPEED.format(device_id=device_id)
        self._cmd_topic = TOPIC_CMD_SPEED.format(device_id=device_id)

    @property
    def name(self) -> str:
        """Return the name of the fan."""
        return self._device_name

    @property
    def is_on(self) -> bool:
        """Return true if fan is on."""
        return self._percentage > 0 or (self._state is not None and self._state > 0)

    @property
    def percentage(self) -> int:
        """Return the current percentage."""
        return self._percentage

    @property
    def preset_mode(self) -> str:
        """Return the current preset mode."""
        return self._preset_mode

    @property
    def preset_modes(self) -> list[str]:
        """Return a list of available preset modes."""
        return PRESET_MODES

    @property
    def supported_features(self) -> FanEntityFeature:
        """Flag supported features."""
        return FanEntityFeature.SET_SPEED | FanEntityFeature.PRESET_MODE | FanEntityFeature.TURN_ON | FanEntityFeature.TURN_OFF

    @property
    def speed_count(self) -> int:
        """Return the number of speeds the fan supports."""
        return int_states_in_range(SPEED_RANGE)

    @property
    def device_info(self) -> DeviceInfo:
        """Return device information."""
        return DeviceInfo(
            identifiers={(DOMAIN, self._device_id)},
            name=self._device_name,
            manufacturer="Alpac Helty",
            model="Flow Compact VMC",
        )

    async def async_added_to_hass(self) -> None:
        """Subscribe to MQTT topics."""
        @callback
        def state_received(msg):
            """Handle new MQTT state messages."""
            try:
                val = int(msg.payload)
                self._state = val
                self._preset_mode = VAL_TO_MODE.get(val, "Normal")
                self.async_write_ha_state()
            except ValueError:
                _LOGGER.error("Invalid state received: %s", msg.payload)

        @callback
        def fan_speed_received(msg):
            """Handle new MQTT fan speed messages."""
            try:
                val = int(msg.payload)
                if val == 0:
                    self._percentage = 0
                else:
                    self._percentage = ranged_value_to_percentage(SPEED_RANGE, val)
                self.async_write_ha_state()
            except ValueError:
                _LOGGER.error("Invalid fan speed received: %s", msg.payload)

        await async_subscribe(self.hass, self._state_topic, state_received)
        await async_subscribe(self.hass, self._fan_speed_topic, fan_speed_received)

    async def async_set_percentage(self, percentage: int) -> None:
        """Set the speed percentage of the fan."""
        if percentage == 0:
            await self.async_turn_off()
        else:
            val = percentage_to_ranged_value(SPEED_RANGE, percentage)
            await async_publish(self.hass, self._cmd_topic, str(val))

    async def async_set_preset_mode(self, preset_mode: str) -> None:
        """Set the preset mode of the fan."""
        if preset_mode in MODE_TO_VAL:
            val = MODE_TO_VAL[preset_mode]
            await async_publish(self.hass, self._cmd_topic, str(val))

    async def async_turn_on(
        self,
        percentage: int | None = None,
        preset_mode: str | None = None,
        **kwargs: Any,
    ) -> None:
        """Turn on the fan."""
        if preset_mode:
            await self.async_set_preset_mode(preset_mode)
        elif percentage:
            await self.async_set_percentage(percentage)
        else:
            # Default to speed 1 if nothing else specified
            await async_publish(self.hass, self._cmd_topic, "1")

    async def async_turn_off(self, **kwargs: Any) -> None:
        """Turn off the fan."""
        await async_publish(self.hass, self._cmd_topic, "0")
