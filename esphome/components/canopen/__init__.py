from pathlib import Path

from esphome import automation
import esphome.codegen as cg
from esphome.components.canbus import CanbusComponent
from esphome.components.const import CONF_SERIAL_NUMBER
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_IDENTITY

CODEOWNERS = ["@mrk-its"]
DEPENDENCIES = ["canbus"]
DOMAIN = "canopen"
MULTI_CONF = True

CONF_CANBUS_ID = "canbus_id"
CONF_NODE_ID = "node_id"
CONF_HEARTBEAT_INTERVAL = "heartbeat_interval"
CONF_ON_NMT_STATE_CHANGE = "on_nmt_state_change"
CONF_NMT_STARTUP = "nmt_startup"
CONF_CANOPENNODE_PATH = "canopennode_path"
CONF_VENDOR_ID = "vendor_id"
CONF_PRODUCT_CODE = "product_code"
CONF_REVISION_NUMBER = "revision_number"
CONF_DEVICE_NAME = "device_name"
CONF_HARDWARE_VERSION = "hardware_version"
CONF_SOFTWARE_VERSION = "software_version"

# CANopenNode upstream + library.json + SDO server block size option + the ESPHome target header CO_driver_target.h
# (branch esphome), pinned to a commit
CANOPENNODE_SOURCE = "https://github.com/mrk-its/CANopenNode.git#30a669c94678595c4cac7fe9d9c5e3fb75166e18"

ns = cg.esphome_ns.namespace("canopen")
CANopenComponent = ns.class_("CANopenComponent", cg.Component)
NMTState = ns.enum("NMTState", is_class=True)


CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_NMT_STATE_CHANGE,
        "add_on_nmt_state_callback",
        [(NMTState, "state")],
    ),
)


def absolute_path(value):
    # the zephyr library converter rejects relative file:// sources
    return str(Path(cv.string(value)).expanduser().resolve())


# 0x1017 (producer heartbeat time) holds uint16 ms
UINT16_MS = cv.All(
    cv.positive_time_period_milliseconds,
    cv.Range(min=cv.TimePeriod(milliseconds=1), max=cv.TimePeriod(milliseconds=65535)),
)

# 0x1018; vendor ids are assigned by CiA
IDENTITY_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_VENDOR_ID, default=0): cv.hex_uint32_t,
        cv.Optional(CONF_PRODUCT_CODE, default=0): cv.hex_uint32_t,
        cv.Optional(CONF_REVISION_NUMBER, default=0): cv.hex_uint32_t,
        cv.Optional(CONF_SERIAL_NUMBER, default=0): cv.hex_uint32_t,
    }
)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CANopenComponent),
        cv.GenerateID(CONF_CANBUS_ID): cv.use_id(CanbusComponent),
        cv.Required(CONF_NODE_ID): cv.All(cv.int_, cv.Range(min=1, max=127)),
        cv.Optional(CONF_HEARTBEAT_INTERVAL, "5000ms"): UINT16_MS,
        # pre_operational: until an NMT master (or set_nmt_state()) starts the node
        cv.Optional(CONF_NMT_STARTUP, default="operational"): cv.one_of(
            "operational", "pre_operational", lower=True
        ),
        # our own NMT state
        cv.Optional(CONF_ON_NMT_STATE_CHANGE): automation.validate_automation({}),
        cv.Optional(CONF_IDENTITY, default={}): IDENTITY_SCHEMA,
        # 0x1008 (default: the node's name), 0x1009, 0x100A (default: the ESPHome version)
        cv.Optional(CONF_DEVICE_NAME): cv.string,
        cv.Optional(CONF_HARDWARE_VERSION): cv.string,
        cv.Optional(CONF_SOFTWARE_VERSION): cv.string,
        # local CANopenNode checkout instead of CANOPENNODE_SOURCE (development)
        cv.Optional(CONF_CANOPENNODE_PATH): absolute_path,
    }
).extend(cv.COMPONENT_SCHEMA)


def add_canopennode_library(path: str | None = None) -> None:
    """Add the CANopenNode library (also used by the C++ unit tests).

    Its target header (CO_driver_target.h) is in the library itself, so no include path is needed.
    """
    if path is not None:
        cg.add_library("CANopenNode", None, f"file://{path}")
    else:
        cg.add_library("CANopenNode", None, CANOPENNODE_SOURCE)


async def to_code(config):
    add_canopennode_library(config.get(CONF_CANOPENNODE_PATH))

    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NODE_ID])
    await cg.register_component(var, config)

    canbus = await cg.get_variable(config[CONF_CANBUS_ID])
    cg.add(var.set_canbus(canbus))
    cg.add(var.set_heartbeat_interval(config[CONF_HEARTBEAT_INTERVAL]))
    if config[CONF_NMT_STARTUP] != "operational":
        cg.add(var.set_start_operational(False))
    identity = config[CONF_IDENTITY]
    cg.add(
        var.set_identity(
            identity[CONF_VENDOR_ID],
            identity[CONF_PRODUCT_CODE],
            identity[CONF_REVISION_NUMBER],
            identity[CONF_SERIAL_NUMBER],
        )
    )
    for key, setter in (
        (CONF_DEVICE_NAME, var.set_device_name),
        (CONF_HARDWARE_VERSION, var.set_hardware_version),
        (CONF_SOFTWARE_VERSION, var.set_software_version),
    ):
        if key in config:
            cg.add(setter(config[key]))

    await automation.build_callback_automations(var, config, CALLBACK_AUTOMATIONS)
