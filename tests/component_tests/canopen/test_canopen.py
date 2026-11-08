"""Tests for the canopen component (CANopenNode based)."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.canopen import CONFIG_SCHEMA, SDO_WRITE_SCHEMA, enable_sdo_block
from esphome.config import read_config
import esphome.config_validation as cv
from esphome.core import CORE, EsphomeError


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
    # SDO block transfer: the CANopenNode configuration and the define for dump_config()
    assert "-DCO_CONFIG_SDO_SRV=0x06" in CORE.build_flags
    assert "-DCO_CONFIG_SDO_SRV_BUFFER_SIZE=900" in CORE.build_flags
    assert "-DCO_CONFIG_SDO_SRV_BLKSIZE=16" in CORE.build_flags
    assert "-DCO_CONFIG_CRC16=0x01" in CORE.build_flags
    defines = {d.name: str(d.value) for d in CORE.defines}
    assert defines["CANOPEN_SDO_BLOCK_SIZE"] == "16"
    # canopen.nmt: no action class, a static function calling the hub
    assert (
        "canopen_node->send_nmt(1, canopen::NMTCommand::NMT_COMMAND_PRE_OPERATIONAL);"
        in main_cpp
    )


def test_defaults() -> None:
    config = CONFIG_SCHEMA(_config())
    assert config["heartbeat_interval"].total_milliseconds == 5000
    assert config["heartbeat_clients"] == []
    assert config["nmt_startup"] == "operational"
    assert "sdo_block_size" not in config  # segmented only
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
    # segmented only (block transfer is optional in servers, CANopenNode's client doesn't fall back)
    assert "-DCO_CONFIG_SDO_CLI=0x0B" in CORE.build_flags
    assert "-DCO_CONFIG_FIFO=0x01" in CORE.build_flags
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


def test_local_nodes(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Nodes on the same canbus are linked into a ring (each receives the others' frames), per canbus."""
    main_cpp = generate_main(component_config_path("local.yaml"))

    links = [
        line.strip() for line in main_cpp.splitlines() if "set_next_local(" in line
    ]
    # in order; the last call per node wins: a -> b -> c -> a
    assert links == [
        "node_a->set_next_local(node_b);",
        "node_b->set_next_local(node_a);",
        "node_b->set_next_local(node_c);",
        "node_c->set_next_local(node_a);",
    ]
    assert "node_d->set_next_local(" not in main_cpp


def test_local_nodes_same_id(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
) -> None:
    """Two nodes on one canbus can't share a node id."""
    CORE.config_path = component_config_path("local_same_id.yaml")
    assert read_config({}) is None
    assert "node_id 16 is also used by canopen" in capsys.readouterr().out


def test_sdo_block_size_differs(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
) -> None:
    """sdo_block_size is a CANopenNode build flag: one value for every node of the firmware."""
    CORE.config_path = component_config_path("block_size_differs.yaml")
    assert read_config({}) is None
    assert "sdo_block_size applies to every canopen node" in capsys.readouterr().out


def test_enable_sdo_block_prefers_configured_size() -> None:
    """A component's default block size doesn't override a node's sdo_block_size, whatever the order."""
    CORE.config = {"canopen": [{"node_id": 1}, {"node_id": 2, "sdo_block_size": 32}]}
    enable_sdo_block(16)
    assert "-DCO_CONFIG_SDO_SRV_BLKSIZE=32" in CORE.build_flags
    assert "-DCO_CONFIG_SDO_SRV_BLKSIZE=16" not in CORE.build_flags


def test_ota_only_when_requested(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The program download objects and SDO block transfer come with `ota: - platform: canopen` only."""
    main_cpp = generate_main(component_config_path("no_ota.yaml"))
    assert "CANopenOTAComponent" not in main_cpp
    assert "-DCO_CONFIG_CRC16=0x01" not in CORE.build_flags
    assert "CANOPEN_SDO_BLOCK_SIZE" not in {define.name for define in CORE.defines}


def test_ota_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("ota.yaml"))
    assert "canopen::CANopenOTAComponent()" in main_cpp
    assert "->set_canopen(canopen_node);" in main_cpp
    assert "->set_reboot(false);" in main_cpp
    # block transfer at the default size: the hub's sdo_block_size isn't set
    assert "-DCO_CONFIG_SDO_SRV_BLKSIZE=16" in CORE.build_flags


def _defines() -> set[str]:
    return {define.name for define in CORE.defines}


def test_client_only_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Without node_id the hub is an SDO client only: no node setters, the client compiled in."""
    main_cpp = generate_main(component_config_path("client_only.yaml"))
    assert "canopen::CANopenComponent(0)" in main_cpp
    assert "canopen_client->set_response_timeout(200);" in main_cpp
    assert "canopen_client->add_on_heartbeat_callback(" in main_cpp
    for setter in (
        "set_heartbeat_interval(",
        "set_identity(",
        "set_start_operational(",
    ):
        assert setter not in main_cpp
    assert "USE_CANOPEN_SDO_CLIENT" in _defines()
    assert "-DCO_CONFIG_SDO_CLI=0x0B" in CORE.build_flags
    # no hub with a node id: the node part isn't compiled (nor linked from CANopenNode)
    assert "USE_CANOPEN_NODE" not in _defines()
    # canopen.sdo_read
    assert "canopen::SDOReadAction<>" in main_cpp
    assert "set_size(4);" in main_cpp
    assert "get_value_trigger()" in main_cpp
    assert "get_error_trigger()" in main_cpp


@pytest.mark.parametrize(
    "option",
    [
        {"heartbeat_interval": "1s"},
        {"heartbeat_clients": [{"node_id": 1, "timeout": "1s"}]},
        {"nmt_startup": "pre_operational"},
        {"identity": {"vendor_id": 1}},
        {"device_name": "x"},
        {"sdo_block_size": 16},
    ],
)
def test_client_only_rejects_node_options(option: dict) -> None:
    with pytest.raises(cv.Invalid, match="needs a CANopen node"):
        CONFIG_SCHEMA({"canbus_id": "can_bus", **option})


def test_client_only_sdo_client_false() -> None:
    with pytest.raises(cv.Invalid, match="sdo_client: false without node_id"):
        CONFIG_SCHEMA({"canbus_id": "can_bus", "sdo_client": False})


def test_client_only_defaults() -> None:
    """The node options get their defaults only with node_id."""
    config = CONFIG_SCHEMA({"canbus_id": "can_bus"})
    assert "node_id" not in config
    config = CONFIG_SCHEMA(_config())
    assert config["heartbeat_interval"].total_milliseconds == 5000


@pytest.mark.parametrize(
    ("yaml", "message"),
    [
        ("client_only_ota.yaml", "ota canopen needs a CANopen node"),
        (
            "sdo_client_false_client_only.yaml",
            "sdo_client applies to every canopen hub of the firmware",
        ),
    ],
)
def test_client_only_invalid(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
    yaml: str,
    message: str,
) -> None:
    CORE.config_path = component_config_path(yaml)
    assert read_config({}) is None
    assert message in capsys.readouterr().out


@pytest.mark.parametrize(
    ("yaml", "message"),
    [
        ("client_only_nmt.yaml", "canopen.nmt needs a CANopen node"),
        (
            "sdo_client_false_used.yaml",
            "has sdo_client: false, but the SDO client is used",
        ),
    ],
)
def test_codegen_errors(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    yaml: str,
    message: str,
) -> None:
    with pytest.raises(EsphomeError, match=message):
        generate_main(component_config_path(yaml))


def test_sdo_client_auto(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A node compiles the client only if something uses it."""
    generate_main(component_config_path("no_ota.yaml"))
    assert "USE_CANOPEN_SDO_CLIENT" not in _defines()


def test_sdo_client_true(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("sdo_client_true.yaml"))
    assert "USE_CANOPEN_SDO_CLIENT" in _defines()
    assert "USE_CANOPEN_NODE" in _defines()
