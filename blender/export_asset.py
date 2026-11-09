"""Create one portable Blender asset in Quby's managed library."""

import argparse
import csv
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import struct
import sys
import tempfile
import uuid
import zipfile
from urllib.parse import unquote, urlsplit

import bpy
from mathutils import Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
from preview import apply_asset_preview

MESHES = {".fbx", ".glb", ".gltf"}
IMAGES = {".png", ".jpg", ".jpeg", ".webp", ".exr", ".tif", ".tiff"}


def unpack(source, folder):
    with zipfile.ZipFile(source) as package:
        entries = package.infolist()
        if len(entries) > 10000 or sum(entry.file_size for entry in entries) > 24 * 1024**3:
            raise ValueError("This archive exceeds Quby's extraction limits.")
        for entry in entries:
            parts = entry.filename.replace("\\", "/").rstrip("/").split("/")
            if any(p in {"", ".", ".."} or p.endswith((".", " ")) or
                   re.search(r'[<>:"|?*\x00-\x1f]', p) or
                   re.fullmatch(r"(?i)(CON|PRN|AUX|NUL|COM[1-9¹²³]|LPT[1-9¹²³])", p.split(".")[0].rstrip())
                   for p in parts):
                raise ValueError("The archive contains an unsafe path.")
            target = folder.joinpath(*parts)
            if entry.is_dir() or target.suffix.lower() not in MESHES | IMAGES | {".bin"}:
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            with package.open(entry) as contents, target.open("xb") as output:
                shutil.copyfileobj(contents, output)


def texture_maps(files):
    patterns = {
        "Base Color": r"(albedo|base.?color|diffuse)",
        "Roughness": r"roughness",
        "Metallic": r"(metalness|metallic)",
        "Normal": r"(normal|nor_gl)(?!_dx)",
        "Alpha": r"(opacity|alpha)",
    }
    maps = {}
    for channel, pattern in patterns.items():
        candidates = [p for p in files if p.suffix.lower() in IMAGES and
                      re.search(r"(?i)(?:^|[_ .-])" + pattern + r"(?:$|[_ .-])", p.stem)
                      and not (channel == "Normal" and re.search(r"(?i)(directx|normal[_-]?dx|nor_dx)", p.stem))]
        if candidates:
            maps[channel] = candidates[0]
    return maps


def build_material(name, maps):
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    shader = nodes.get("Principled BSDF")
    for index, (channel, path) in enumerate(maps.items()):
        image = bpy.data.images.load(str(path), check_existing=True)
        if channel != "Base Color":
            image.colorspace_settings.name = "Non-Color"
        texture = nodes.new("ShaderNodeTexImage")
        texture.image = image
        texture.location = (-520, -260 * index)
        output = texture.outputs["Color"]
        if channel == "Normal":
            normal = nodes.new("ShaderNodeNormalMap")
            normal.location = (-230, -260 * index)
            links.new(output, normal.inputs["Color"])
            output = normal.outputs["Normal"]
        links.new(output, shader.inputs[channel])
    return material


def validate_gltf_references(mesh, root):
    if mesh.suffix.lower() == ".glb":
        with mesh.open("rb") as stream:
            header = stream.read(20)
            if len(header) != 20 or header[:4] != b"glTF" or header[16:20] != b"JSON":
                raise ValueError("Invalid GLB header.")
            size = struct.unpack_from("<I", header, 12)[0]
            if size > 128 * 1024**2:
                raise ValueError("The GLB metadata is too large.")
            data = json.loads(stream.read(size))
    else:
        data = json.loads(mesh.read_text(encoding="utf-8"))
    for item in data.get("images", []) + data.get("buffers", []):
        uri = item.get("uri", "")
        if not uri or uri.startswith("data:"):
            continue
        parsed = urlsplit(uri)
        if parsed.scheme or parsed.netloc or not (mesh.parent / unquote(parsed.path)).resolve().is_relative_to(root):
            raise ValueError("The model references files outside its asset folder.")


def make_asset(source, folder, scene):
    if source.suffix.lower() == ".zip":
        unpack(source, folder)
        files = sorted(p for p in folder.rglob("*") if p.is_file())
    else:
        files = [source]
    meshes = [p for p in files if p.suffix.lower() in MESHES]
    maps = texture_maps(files)
    name = source.stem
    packaged_preview = next((p for p in files if p.suffix.lower() in {".png", ".jpg", ".jpeg", ".webp"}
                             and re.search(r"(?i)(^|[_ .-])(preview|thumbnail)([_ .-]|$)", p.stem)), None)
    if not meshes:
        if source.suffix.lower() in IMAGES:
            maps = {"Base Color": source}
        if "Base Color" not in maps:
            raise ValueError("No FBX, glTF, GLB, or recognized surface textures were found.")
        material = build_material(name, maps)
        bpy.ops.mesh.primitive_uv_sphere_add(segments=48, ring_count=24)
        sample = bpy.context.object
        sample.data.materials.append(material)
        for face in sample.data.polygons:
            face.use_smooth = True
        return material, [sample], packaged_preview, True

    def mesh_rank(path):
        lod = re.search(r"(?i)lod[_-]?(\d+)", path.stem)
        return (int(lod[1]) if lod else 0, {".glb": 0, ".gltf": 1, ".fbx": 2}[path.suffix.lower()], str(path))

    mesh = min(meshes, key=mesh_rank)
    asset_root = folder.resolve() if source.suffix.lower() == ".zip" else source.parent
    if mesh.suffix.lower() == ".fbx":
        bpy.ops.import_scene.fbx(filepath=str(mesh), use_image_search=False)
    else:
        validate_gltf_references(mesh, asset_root)
        bpy.ops.import_scene.gltf(filepath=str(mesh))
    for image in bpy.data.images:
        if image.source == "FILE" and not image.packed_file and image.filepath:
            path = Path(bpy.path.abspath(image.filepath)).resolve()
            if not path.is_relative_to(asset_root) or not path.is_file():
                # Broken external references are replaced by matching local maps below.
                bpy.data.images.remove(image, do_unlink=True)
    objects = list(scene.objects)
    geometry = [obj for obj in objects if obj.type == "MESH" and len(obj.data.vertices)]
    if not geometry:
        raise ValueError("The selected model has no mesh geometry.")
    if "Base Color" in maps and (mesh.suffix.lower() == ".fbx" or not any(obj.data.materials for obj in geometry)):
        material = build_material(name, maps)
        for obj in geometry:
            for index, slot in enumerate(obj.data.materials):
                if slot is None or not slot.node_tree or not any(n.type == "TEX_IMAGE" and n.image for n in slot.node_tree.nodes):
                    obj.data.materials[index] = material
            if not obj.data.materials:
                obj.data.materials.append(material)
    if len(objects) == 1:
        asset = objects[0]
        asset.name = name
    else:
        asset = bpy.data.collections.new(name)
        scene.collection.children.link(asset)
        for obj in objects:
            for collection in list(obj.users_collection):
                collection.objects.unlink(obj)
            asset.objects.link(obj)
    return asset, geometry, packaged_preview, False


def render_thumbnail(scene, geometry, destination):
    bpy.context.view_layer.update()
    points = [obj.matrix_world @ Vector(corner) for obj in geometry for corner in obj.bound_box]
    center = Vector([(min(p[i] for p in points) + max(p[i] for p in points)) / 2 for i in range(3)])
    radius = max(max((p - center).length for p in points), 0.001)
    temporary = []

    def place(data, offset):
        obj = bpy.data.objects.new(data.name, data)
        scene.collection.objects.link(obj)
        obj.location = center + radius * Vector(offset)
        obj.rotation_euler = (center - obj.location).to_track_quat("-Z", "Y").to_euler()
        temporary.append(obj)
        return obj

    camera = bpy.data.cameras.new("Foundry thumbnail camera")
    camera.type = "ORTHO"
    camera.ortho_scale = radius * 2.35
    camera.clip_start = radius * 0.01
    camera.clip_end = radius * 20
    scene.camera = place(camera, (3, -5, 3))
    lights = []
    for offset, power in [((1, -3, 4), 1100), ((-3, -2, 2), 550), ((1, 3, 3), 850)]:
        light = bpy.data.lights.new("Foundry thumbnail light", "AREA")
        light.energy = power * radius ** 2
        light.size = radius * 2
        lights.append(light)
        place(light, offset)
    try:
        scene.render.engine = "CYCLES"
        scene.cycles.device = "CPU"
        scene.cycles.samples = 12
        scene.cycles.use_denoising = True
        scene.render.resolution_x = scene.render.resolution_y = 256
        scene.render.resolution_percentage = 100
        scene.render.image_settings.file_format = "PNG"
        scene.render.filepath = str(destination)
        bpy.ops.render.render(write_still=True)
    finally:
        for obj in temporary:
            bpy.data.objects.remove(obj, do_unlink=True)
        bpy.data.cameras.remove(camera)
        for light in lights:
            bpy.data.lights.remove(light)


def register_library(root):
    libraries = bpy.context.preferences.filepaths.asset_libraries
    for library in libraries:
        if library.path and Path(bpy.path.abspath(library.path)).resolve() == root:
            return library.name
    if os.name == "nt":
        output = subprocess.check_output(["tasklist.exe", "/FO", "CSV", "/NH", "/FI", "IMAGENAME eq blender.exe"],
                                         text=True, errors="replace", timeout=15)
        own_pids = {str(os.getpid()), str(os.getppid())}
        if any(len(row) > 1 and row[0].lower() == "blender.exe" and row[1] not in own_pids
               for row in csv.reader(output.splitlines())):
            raise RuntimeError("Close Blender once so Quby can register the asset library, then retry.")
    name = "Megascans"
    suffix = 2
    while libraries.get(name):
        name = f"Megascans (Foundry {suffix})"
        suffix += 1
    if "FINISHED" not in bpy.ops.preferences.asset_library_add(directory=str(root), name=name):
        raise RuntimeError("Blender could not register the asset library.")
    if "FINISHED" not in bpy.ops.wm.save_userpref():
        raise RuntimeError("Blender could not save the asset library preference.")
    return name


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("library", type=Path)
    parser.add_argument("--preview", type=Path)
    options = parser.parse_args(sys.argv[sys.argv.index("--") + 1:])
    source, root = options.source.resolve(strict=True), options.library.resolve()
    if source.suffix.lower() not in MESHES | IMAGES | {".zip"}:
        raise ValueError("Unsupported asset file.")
    root.mkdir(parents=True, exist_ok=True)
    library_name = register_library(root)
    bpy.ops.wm.read_homefile(use_factory_startup=True, use_empty=True, load_ui=False)
    scene = bpy.context.scene
    with tempfile.TemporaryDirectory(prefix=".foundry-", dir=root.parent) as temp:
        folder = Path(temp)
        asset, geometry, packaged_preview, sample = make_asset(source, folder, scene)
        asset.asset_mark()
        asset.asset_data.description = "Imported with Quby - MegascanImporter"
        thumbnail = options.preview or packaged_preview
        preview_kind = "source" if thumbnail and apply_asset_preview(asset, thumbnail) == "source" else "rendered"
        if preview_kind == "rendered":
            generated = folder / "foundry-preview.png"
            render_thumbnail(scene, geometry, generated)
            if apply_asset_preview(asset, generated) != "source":
                raise RuntimeError("Blender could not attach the rendered asset preview.")
        if sample:
            bpy.data.objects.remove(geometry[0], do_unlink=True)
        bpy.ops.file.pack_all()
        stage = folder / "asset.blend"
        bpy.ops.wm.save_as_mainfile(filepath=str(stage), compress=True)
        bpy.ops.wm.open_mainfile(filepath=str(stage), load_ui=False)
        marked = [item for items in (bpy.data.objects, bpy.data.collections, bpy.data.materials)
                  for item in items if item.asset_data]
        if len(marked) != 1 or not marked[0].preview or not all(marked[0].preview.image_size):
            raise RuntimeError("The saved Blender file did not retain its asset and thumbnail.")
        name = re.sub(r"[^\w .-]", "_", source.stem).strip(". ")[:72] or "Asset"
        target = root / f"{name}--foundry-{uuid.uuid4().hex}.blend"
        os.rename(stage, target)
        print(f"FOUNDRY_OK {library_name} ({preview_kind} preview)", flush=True)


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"FOUNDRY_ERROR {error}", flush=True)
        raise
