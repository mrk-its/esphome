from enum import StrEnum, auto  # noqa

from esphome import config_validation as cv


class HALEnum(StrEnum):
    @staticmethod
    def _generate_next_value_(name, start, *args, **kw):
        return name.upper()

    @staticmethod
    def from_values(name, values):
        return HALEnum(name, [(v, v) for v in values])

    @classmethod
    def cv_enum(cls):
        return cv.enum({v: v for v in cls})


def optional_dict(schema, default_value=None):
    return lambda value: schema(value or default_value or {})
