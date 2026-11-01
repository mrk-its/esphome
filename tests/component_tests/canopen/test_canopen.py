"""Tests for the canopen component (CANopenNode based)."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.canopen import CONFIG_SCHEMA
import esphome.config_validation as cv


def _config(**kwargs):
    return {"canbus_id": "can_bus", "node_id": 0x10, **kwargs}


def test_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The node's settings become setter calls, the trigger a callback."""
    main_cpp = generate_main(component_config_path("canopen.yaml"))

    assert "canopen::CANopenComponent(16)" in main_cpp
    assert "canopen_node->set_heartbeat_interval(2000);" in main_cpp
    assert "canopen_node->set_start_operational(false);" in main_cpp
    assert "canopen_node->add_on_nmt_state_callback(" in main_cpp
    # identity: unset fields are 0; device name / software version keep their C++ defaults
    assert "canopen_node->set_identity(0x12345678, 0x2A, 0x00, 0x00);" in main_cpp
    assert 'canopen_node->set_hardware_version("rev B");' in main_cpp
    assert "set_device_name(" not in main_cpp
    assert "set_software_version(" not in main_cpp


def test_defaults() -> None:
    config = CONFIG_SCHEMA(_config())
    assert config["heartbeat_interval"].total_milliseconds == 5000
    assert config["nmt_startup"] == "operational"
    assert config["identity"] == {
        "vendor_id": 0,
        "product_code": 0,
        "revision_number": 0,
        "serial_number": 0,
    }


@pytest.mark.parametrize("node_id", [0, 128])
def test_node_id_range(node_id: int) -> None:
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA(_config(node_id=node_id))


@pytest.mark.parametrize(("interval", "valid"), [("65535ms", True), ("65536ms", False)])
def test_heartbeat_interval_fits_uint16(interval: str, valid: bool) -> None:
    """0x1017 stores the producer heartbeat time as uint16 milliseconds."""
    config = _config(heartbeat_interval=interval)
    if valid:
        CONFIG_SCHEMA(config)
    else:
        with pytest.raises(cv.Invalid, match="at most"):
            CONFIG_SCHEMA(config)
