import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.one_wire import OneWireBus
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .. import ds2484_ns

CODEOWNERS = ["@mrk-its"]

DS2484OneWireBus = ds2484_ns.class_(
    "DS2484OneWireBus", OneWireBus, i2c.I2CDevice, cg.Component
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DS2484OneWireBus),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x30))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await i2c.register_i2c_device(var, config)

    # var = cg.new_Pvariable(config[CONF_ID])
    # await cg.register_component(var, config)

    # pin = await cg.gpio_pin_expression(config[CONF_PIN])
    # cg.add(var.set_pin(pin))
