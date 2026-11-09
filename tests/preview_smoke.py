"""Run in Blender background mode to verify a source image survives saving."""

from pathlib import Path
import sys

import bpy

project = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(project / "blender"))
from preview import apply_asset_preview

target = Path(sys.argv[sys.argv.index("--") + 1])
asset = bpy.data.objects["Cube"]
asset.asset_mark()
assert apply_asset_preview(asset, project / "assets" / "icon.png") == "source"
preview_size = tuple(asset.preview.image_size)
assert all(preview_size), preview_size
bpy.ops.wm.save_as_mainfile(filepath=str(target))
bpy.ops.wm.open_mainfile(filepath=str(target))
asset = bpy.data.objects["Cube"]
assert asset.asset_data is not None
assert tuple(asset.preview.image_size) == preview_size
print("Source thumbnail preserved after reopening:", preview_size)
