# required for cpp unit testing.
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # the build's real backend (host) is compiled too, and ota auto-loads md5 only through a function
    manifest.dependencies = ["canopen", "md5"]
