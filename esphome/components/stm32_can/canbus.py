import esphome.codegen as cg
from esphome.components.canbus import CANBUS_SCHEMA, CanbusComponent, register_canbus
import esphome.config_validation as cv
from esphome.const import CONF_ID

ns = cg.esphome_ns.namespace("stm32_can")
STM32Can = ns.class_(
    "STM32Can",
    CanbusComponent,
)

DEPENDENCIES = ["logger"]

CONFIG_SCHEMA = CANBUS_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(STM32Can),
    }
)


async def to_code(config):
    stm32_can = cg.new_Pvariable(config[CONF_ID])
    await register_canbus(stm32_can, config)
