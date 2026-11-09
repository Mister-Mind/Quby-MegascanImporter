"""Assign an available source preview to a marked Blender asset."""

from pathlib import Path

import bpy


def apply_asset_preview(asset, preview_path=None):
    """Return 'source' when a supplied image was applied, otherwise 'generated'."""
    if asset.asset_data is None:
        raise ValueError("Mark the Blender data block as an asset before assigning a preview.")

    if preview_path:
        path = Path(preview_path)
        if path.is_file() and path.suffix.lower() in {".png", ".jpg", ".jpeg", ".webp"}:
            try:
                with bpy.context.temp_override(id=asset):
                    result = bpy.ops.ed.lib_id_load_custom_preview(filepath=str(path.resolve()))
                if "FINISHED" in result and asset.preview and all(asset.preview.image_size):
                    return "source"
            except RuntimeError:
                pass

    asset.asset_generate_preview()
    return "generated"
