from esphome import pins
import esphome.codegen as cg
from esphome.components.canbus import CANBUS_SCHEMA, CanbusComponent, register_canbus
from esphome.components.zephyr import zephyr_add_overlay, zephyr_add_prj_conf
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["logger"]

CONF_INSTANCE = "instance"
CONF_RX_BUFFER_SIZE = "rx_buffer_size"

ns = cg.esphome_ns.namespace("zephyr_can")
ZephyrCan = ns.class_(
    "ZephyrCan",
    CanbusComponent,
)


def canbus_instance(value):
    FDCAN_INSTANCES = ("FDCAN1", "FDCAN2", "FDCAN3")
    if value not in FDCAN_INSTANCES:
        raise cv.Invalid(f"invalid FDCAN instance, must be one of {FDCAN_INSTANCES}")
    return value


CONFIG_SCHEMA = CANBUS_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(ZephyrCan),
        cv.Required("tx_pin"): pins.internal_gpio_output_pin_schema,
        cv.Required("rx_pin"): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_INSTANCE, default="FDCAN1"): canbus_instance,
        cv.Optional(CONF_RX_BUFFER_SIZE, default=8): int,
    }
)


def can_pin_name(pin):
    return "p" + chr(ord("a") + pin["number"] // 16) + str(pin["number"] % 16)


async def to_code(config):
    zephyr_add_prj_conf("CAN", True)
    zephyr_add_prj_conf("CAN_SHELL", True)

    zephyr_add_prj_conf("CAN_FD_MODE", False)
    zephyr_add_prj_conf("CAN_MANUAL_RECOVERY_MODE", False)
    zephyr_add_prj_conf("CAN_STATS", True)
    zephyr_add_prj_conf("CAN_ACCEPT_RTR", False)
    zephyr_add_prj_conf("CONFIG_CAN_INIT_PRIORITY", 255)
    instance = config[CONF_INSTANCE].lower()
    rx_queue = f"{config[CONF_ID]}_rx_queue"
    rx_buffer_size = config[CONF_RX_BUFFER_SIZE]

    cg.add_global(
        cg.RawExpression(f"""CAN_MSGQ_DEFINE({rx_queue}, {rx_buffer_size})""")
    )

    zephyr_can = cg.new_Pvariable(
        config[CONF_ID],
        cg.RawExpression(f"DEVICE_DT_GET(DT_NODELABEL({instance}))"),
        cg.RawExpression(f"&{rx_queue}"),
    )
    await register_canbus(zephyr_can, config)

    rx_pin = can_pin_name(config["rx_pin"])
    tx_pin = can_pin_name(config["tx_pin"])

    zephyr_add_overlay(f"""
      &fdcan1 {{
        pinctrl-0 = <&fdcan1_rx_{rx_pin} &fdcan1_tx_{tx_pin}>;
      }};
    """)

    # rx_pin = await cg.gpio_pin_expression(config[CONF_RX_PIN])
    # cg.add(zephyr_can.set_rx_pin(rx_pin))
    #
    # tx_pin = await cg.gpio_pin_expression(config[CONF_TX_PIN])
    # cg.add(zephyr_can.set_tx_pin(tx_pin))

    # if CONF_INSTANCE in config:
    #     cg.add(zephyr_can.set_instance(cg.RawExpression(config[CONF_INSTANCE])))
