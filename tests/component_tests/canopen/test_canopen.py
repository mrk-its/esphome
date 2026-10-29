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
    """Heartbeat clients become monitor_node() calls, on_node_event a callback."""
    main_cpp = generate_main(component_config_path("canopen.yaml"))

    assert "canopen::CANopenComponent(16)" in main_cpp
    assert "canopen_node->set_heartbeat_interval(2000);" in main_cpp
    assert "canopen_node->monitor_node(1, 3000);" in main_cpp
    assert "canopen_node->monitor_node(2, 500);" in main_cpp
    assert "canopen_node->add_on_node_event_callback(" in main_cpp


def test_defaults() -> None:
    config = CONFIG_SCHEMA(_config())
    assert config["heartbeat_interval"].total_milliseconds == 5000
    assert config["heartbeat_clients"] == []


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
