import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@mrk-its"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True


ds2484_ns = cg.esphome_ns.namespace("ds2484")

DS2484 = ds2484_ns.class_("DS2484", cg.Component, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(DS2484),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x30))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await i2c.register_i2c_device(var, config)
