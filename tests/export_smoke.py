"""Run inside a disposable Blender profile, with a GLB fixture and library path."""

import os
from pathlib import Path
import sys
from unittest.mock import patch

import bpy

project = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(project / "blender"))
import export_asset

expected_config = Path(os.environ["FOUNDRY_TEST_CONFIG"]).resolve()
assert expected_config == Path(bpy.utils.user_resource("CONFIG")).resolve()
assert expected_config.name == "config" and expected_config.parent.name == "foundry-export-test"
root = Path(sys.argv[sys.argv.index("--") + 2]).resolve()
assert root.parent == expected_config.parent

# The live Blender process uses a different profile. Exercise actual registration
# and export while bypassing only the process guard inside this disposable test.
with patch.object(export_asset.subprocess, "check_output", return_value=""):
    export_asset.main()
asset = next(obj for obj in bpy.data.objects if obj.asset_data)
assert tuple(asset.preview.image_size) == (256, 256)
pixels = list(asset.preview.image_pixels)
assert len(set(pixels)) > 32, "Thumbnail must show the rendered model."
assert len(list(root.glob("*--foundry-*.blend"))) >= 1
print("Verified marked asset and nonblank thumbnail after reopening")
