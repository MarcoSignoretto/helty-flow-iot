"""Constants for the Helty Flow VMC integration."""

DOMAIN = "helty_flow"

CONF_DEVICE_ID = "device_id"
CONF_NAME = "name"

DEFAULT_NAME = "Helty Flow VMC"

# Topics
TOPIC_BASE = "vmcs/{device_id}"
TOPIC_STATE = f"{TOPIC_BASE}/state"
TOPIC_FAN_SPEED = f"{TOPIC_BASE}/fan_speed"
TOPIC_INFO = f"{TOPIC_BASE}/info"
TOPIC_CMD_SPEED = f"{TOPIC_BASE}/cmnd/speed"
TOPIC_LWT = f"{TOPIC_BASE}/LWT"
TOPIC_VERSION = f"{TOPIC_BASE}/version"

# Speeds & Modes
PRESET_MODES = ["Off", "Hyper Speed", "Night Mode", "Free Cooling", "Normal"]

MODE_TO_VAL = {
    "Off": 0,
    "Hyper Speed": 5,
    "Night Mode": 6,
    "Free Cooling": 7,
    "Normal": 1,
}

VAL_TO_MODE = {v: k for k, v in MODE_TO_VAL.items()}
# Any value 1-4 that is not explicitly in VAL_TO_MODE should map to Normal?
# Actually, 1 is Normal. 2, 3, 4 are just "Normal" with higher speed.
# If we are in speed 2, VAL_TO_MODE.get(2) would be None.
# We'll handle this in the fan entity.
