"""Tests for the canopen component (CANopenNode based)."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.canopen import CONFIG_SCHEMA, SDO_WRITE_SCHEMA
from esphome.config import read_config
import esphome.config_validation as cv
from esphome.core import CORE


def _config(**kwargs):
    return {"canbus_id": "can_bus", "node_id": 0x10, **kwargs}


def test_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Heartbeat clients become monitor_node() calls, triggers become callbacks."""
    main_cpp = generate_main(component_config_path("canopen.yaml"))

    assert "canopen::CANopenComponent(16)" in main_cpp
    assert "canopen_node->set_heartbeat_interval(2000);" in main_cpp
    assert "canopen_node->monitor_node(1, 3000);" in main_cpp
    assert "canopen_node->monitor_node(2, 500);" in main_cpp
    assert "canopen_node->set_start_operational(false);" in main_cpp
    for callback in (
        "add_on_nmt_state_callback",
        "add_on_node_event_callback",
        "add_on_heartbeat_callback",
        "add_on_emcy_callback",
    ):
        assert f"canopen_node->{callback}(" in main_cpp
    assert "canopen_node->set_response_timeout(500);" in main_cpp
    # identity: unset fields are 0; device name / software version keep their C++ defaults
    assert "canopen_node->set_identity(0x12345678, 0x2A, 0x00, 0x00);" in main_cpp
    assert 'canopen_node->set_hardware_version("rev B");' in main_cpp
    assert "set_device_name(" not in main_cpp
    assert "set_software_version(" not in main_cpp
    # sdo_write: negative values are sent as two's complement in `size` bytes
    assert "canopen::SDOWriteAction<>" in main_cpp
    assert "set_value([]() -> uint32_t {\n    return 65535;\n});" in main_cpp
    assert "set_size(2);" in main_cpp
    assert "set_command(canopen::NMTCommand::NMT_COMMAND_PRE_OPERATIONAL);" in main_cpp


def test_defaults() -> None:
    config = CONFIG_SCHEMA(_config())
    assert config["heartbeat_interval"].total_milliseconds == 5000
    assert config["heartbeat_clients"] == []
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


def test_heartbeat_client_own_node_id() -> None:
    with pytest.raises(cv.Invalid, match="own id"):
        CONFIG_SCHEMA(_config(heartbeat_clients=[{"node_id": 0x10, "timeout": "1s"}]))


@pytest.mark.parametrize(("timeout", "valid"), [("65535ms", True), ("65536ms", False)])
def test_heartbeat_client_timeout_fits_uint16(timeout: str, valid: bool) -> None:
    """0x1016 stores the consumer timeout as uint16 milliseconds."""
    config = _config(heartbeat_clients=[{"node_id": 0x01, "timeout": timeout}])
    if valid:
        CONFIG_SCHEMA(config)
    else:
        with pytest.raises(cv.Invalid, match="at most"):
            CONFIG_SCHEMA(config)


@pytest.mark.parametrize(("interval", "valid"), [("65535ms", True), ("65536ms", False)])
def test_heartbeat_interval_fits_uint16(interval: str, valid: bool) -> None:
    """0x1017 stores the producer heartbeat time as uint16 milliseconds."""
    config = _config(heartbeat_interval=interval)
    if valid:
        CONFIG_SCHEMA(config)
    else:
        with pytest.raises(cv.Invalid, match="at most"):
            CONFIG_SCHEMA(config)


SDO_WRITE = {"id": "canopen_node", "node_id": 1, "index": 0x1017}


@pytest.mark.parametrize(
    ("value", "size", "valid"),
    [
        (255, 1, True),
        (256, 1, False),
        (-128, 1, True),
        (-129, 1, False),
        (65535, 2, True),
        (0xFFFFFFFF, 4, True),
    ],
)
def test_sdo_write_value_fits_size(value: int, size: int, valid: bool) -> None:
    config = {**SDO_WRITE, "value": value, "size": size}
    if valid:
        SDO_WRITE_SCHEMA(config)
    else:
        with pytest.raises(cv.Invalid, match="doesn't fit"):
            SDO_WRITE_SCHEMA(config)


def test_sdo_write_size() -> None:
    with pytest.raises(cv.Invalid):
        SDO_WRITE_SCHEMA({**SDO_WRITE, "value": 1, "size": 3})


def test_register_node(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A profile gets the SDO client / NMT master it asks for and is monitored with its heartbeat_timeout."""
    main_cpp = generate_main(component_config_path("profile.yaml"))

    assert "fake_canopen_profile::FakeProfile(canopen_node, 33)" in main_cpp
    assert "fake_canopen_profile::FakeProfile(canopen_node, 34)" in main_cpp
    # merged with heartbeat_clients in monitor_node(); no heartbeat_timeout: not monitored
    assert "canopen_node->monitor_node(33, 3000);" in main_cpp
    assert "canopen_node->monitor_node(33, 1500);" in main_cpp
    assert "monitor_node(34" not in main_cpp
    defines = {define.name for define in CORE.defines}
    assert {
        "USE_CANOPEN_SDO_CLIENT",
        "USE_CANOPEN_NMT_MASTER",
        "USE_CANOPEN_PDO",
    } <= defines
    assert "-DCO_CONFIG_SDO_CLI=0x0F" in CORE.build_flags
    assert "-DCO_CONFIG_NMT=0x03" in CORE.build_flags
    assert "-DCO_CONFIG_PDO=0x402F" in CORE.build_flags


def test_register_node_own_id(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
) -> None:
    """A profile can't drive the hub's own node id."""
    CORE.config_path = component_config_path("profile_own_id.yaml")
    assert read_config({}) is None
    assert "node_id 16 is the own id of canopen canopen_node" in capsys.readouterr().out
