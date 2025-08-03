from esphome import pins
import esphome.codegen as cg
from esphome.components.canbus import CANBUS_SCHEMA, CanbusComponent, register_canbus
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INSTANCE, CONF_RX_PIN, CONF_TX_PIN

ns = cg.esphome_ns.namespace("stm32_can")
STM32Can = ns.class_(
    "STM32Can",
    CanbusComponent,
)

DEPENDENCIES = []


def canbus_instance(value):
    CAN_INSTANCES = ("CAN1", "CAN2")
    if value not in CAN_INSTANCES:
        raise cv.Invalid(f"invalid FDCAN instance, must be one of {CAN_INSTANCES}")
    return value


CONFIG_SCHEMA = CANBUS_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(STM32Can),
        cv.Optional(CONF_INSTANCE, default="CAN1"): canbus_instance,
        cv.Required("tx_pin"): pins.internal_gpio_output_pin_schema,
        cv.Required("rx_pin"): pins.internal_gpio_output_pin_schema,
    }
)


async def to_code(config):
    stm32_can = cg.new_Pvariable(config[CONF_ID])
    await register_canbus(stm32_can, config)

    rx_pin = await cg.gpio_pin_expression(config[CONF_RX_PIN])
    cg.add(stm32_can.set_rx_pin(rx_pin))

    tx_pin = await cg.gpio_pin_expression(config[CONF_TX_PIN])
    cg.add(stm32_can.set_tx_pin(tx_pin))

    if CONF_INSTANCE in config:
        cg.add(stm32_can.set_instance(cg.RawExpression(config[CONF_INSTANCE])))
