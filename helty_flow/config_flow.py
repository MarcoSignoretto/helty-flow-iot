"""Config flow for Helty Flow VMC integration."""
import asyncio
import logging
import voluptuous as vol

from homeassistant import config_entries
from homeassistant.components import mqtt
from homeassistant.core import callback
import homeassistant.helpers.config_validation as cv

from .const import DOMAIN, CONF_DEVICE_ID, CONF_NAME, DEFAULT_NAME

_LOGGER = logging.getLogger(__name__)

class HeltyFlowConfigFlow(config_entries.ConfigFlow, domain=DOMAIN):
    """Handle a config flow for Helty Flow VMC."""

    VERSION = 1

    def __init__(self):
        """Initialize the flow."""
        self._discovered_devices = {}
        self._num_devices = 0
        self._devices = []

    async def async_step_user(self, user_input=None):
        """Handle the initial step - Choice between Scan and Manual."""
        return self.async_show_menu(
            step_id="user",
            menu_options=["scan", "manual_count"]
        )

    async def async_step_scan(self, user_input=None):
        """Scan for devices on MQTT."""
        if user_input is None:
            # Start scanning
            self._discovered_devices = {}
            
            @callback
            def discovery_callback(msg):
                """Handle discovery messages."""
                # Topic structure is vmcs/<device_id>/...
                parts = msg.topic.split('/')
                if len(parts) >= 2:
                    device_id = parts[1]
                    if device_id not in self._discovered_devices:
                        self._discovered_devices[device_id] = device_id

            # Subscribe to all helty topics for discovery
            unsubscribe = await mqtt.async_subscribe(
                self.hass, "vmcs/+/state", discovery_callback
            )
            
            # Wait for devices to check in (5 seconds)
            await asyncio.sleep(5)
            unsubscribe()

            if not self._discovered_devices:
                return self.async_show_form(
                    step_id="scan_no_devices",
                    last_step=False
                )

            # Show found devices
            return await self.async_step_select_devices()

    async def async_step_scan_no_devices(self, user_input=None):
        """Handle the case where no devices were found."""
        return await self.async_step_user()

    async def async_step_select_devices(self, user_input=None):
        """Let user select which discovered devices to add."""
        if user_input is not None:
            selected_ids = user_input["selected_devices"]
            self._devices = [
                {CONF_DEVICE_ID: d_id, CONF_NAME: f"{DEFAULT_NAME} {d_id}"}
                for d_id in selected_ids
            ]
            # Since we have the IDs, we can just create the entry
            # or allow the user to rename them? Let's create entry for simplicity
            # as renaming multiple devices in a flow is tedious.
            return self.async_create_entry(
                title=f"Helty Flow VMCs ({len(self._devices)} devices)",
                data={"devices": self._devices}
            )

        return self.async_show_form(
            step_id="select_devices",
            data_schema=vol.Schema({
                vol.Required("selected_devices"): cv.multi_select(self._discovered_devices),
            })
        )

    async def async_step_manual_count(self, user_input=None):
        """Manual entry: ask how many devices."""
        if user_input is not None:
            self._num_devices = user_input["num_devices"]
            return await self.async_step_device_config()

        return self.async_show_form(
            step_id="manual_count",
            data_schema=vol.Schema({
                vol.Required("num_devices", default=1): vol.All(vol.Coerce(int), vol.Range(min=1, max=20)),
            })
        )

    async def async_step_device_config(self, user_input=None):
        """Manual entry: configuration of each device."""
        errors = {}
        if user_input is not None:
            self._devices.append(user_input)
            if len(self._devices) >= self._num_devices:
                return self.async_create_entry(
                    title=f"Helty Flow VMCs ({len(self._devices)} devices)",
                    data={"devices": self._devices}
                )

        current_idx = len(self._devices) + 1
        data_schema = vol.Schema({
            vol.Required(CONF_DEVICE_ID): cv.string,
            vol.Optional(CONF_NAME, default=f"{DEFAULT_NAME} {current_idx}"): cv.string,
        })

        return self.async_show_form(
            step_id="device_config",
            data_schema=data_schema,
            description_placeholders={"index": str(current_idx)},
            errors=errors
        )
