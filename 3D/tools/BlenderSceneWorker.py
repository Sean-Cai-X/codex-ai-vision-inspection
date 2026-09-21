import sys
import argparse
import json
import shutil
import urllib.request
from pathlib import Path

import bpy
import math


def decode_value(value: str) -> str:
    result = []
    escaping = False
    for ch in value:
        if not escaping:
            if ch == "\\":
                escaping = True
            else:
                result.append(ch)
            continue
        if ch == "n":
            result.append("\n")
        elif ch == "r":
            result.append("\r")
        elif ch == "t":
            result.append("\t")
        else:
            result.append(ch)
        escaping = False
    if escaping:
        result.append("\\")
    return "".join(result)


def encode_value(value: str) -> str:
    return (
        value.replace("\\", "\\\\")
        .replace("\n", "\\n")
        .replace("\r", "\\r")
        .replace("\t", "\\t")
        .replace('"', '\\"')
    )


def read_request_file(request_file: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for raw_line in request_file.read_text(encoding="utf-8").splitlines():
        if "=" not in raw_line:
            continue
        key, value = raw_line.split("=", 1)
        values[key] = decode_value(value)
    return values


def write_response_file(response_file: Path, ok: bool, exit_code: int, fields: dict[str, str]) -> None:
    with response_file.open("w", encoding="utf-8") as handle:
        handle.write(f"ok={'true' if ok else 'false'}\n")
        handle.write(f"exit_code={exit_code}\n")
        for key, value in fields.items():
            handle.write(f"field.{key}={encode_value(value)}\n")


def ensure_directory(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def to_key(value: str) -> str:
    normalized = []
    for ch in value.lower():
        if ch.isalnum():
            normalized.append(ch)
        else:
            normalized.append("_")
    key = "".join(normalized).strip("_")
    return key or "item"


def find_scene_objects() -> list[bpy.types.Object]:
    objects = []
    for obj in bpy.data.objects:
        if obj.get("codex_asset_id"):
            objects.append(obj)
    return sorted(objects, key=lambda item: item.name)


def object_row(obj: bpy.types.Object) -> str:
    asset_id = str(obj.get("codex_asset_id", ""))
    part_id = str(obj.get("codex_part_id", ""))
    display_name = str(obj.get("codex_display_name", obj.name))
    location = f"{obj.location.x},{obj.location.y},{obj.location.z}"
    return f"{obj.name}:{asset_id}:{part_id}:{display_name}:{location}"


def build_scene_fields(extra_fields: dict[str, str] | None = None) -> dict[str, str]:
    objects = find_scene_objects()
    fields = {
        "scene_objects": "|".join(object_row(obj) for obj in objects),
        "scene_object_count": str(len(objects)),
    }
    if extra_fields:
        fields.update(extra_fields)
    return fields


def reset_scene() -> None:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete()
    for collection in [bpy.data.meshes, bpy.data.cameras, bpy.data.lights, bpy.data.materials]:
        for item in list(collection):
            if item.users == 0:
                collection.remove(item)


def ensure_camera_and_light() -> None:
    scene = bpy.context.scene

    camera = bpy.data.objects.get("CodexCamera")
    if camera is None:
        camera_data = bpy.data.cameras.new("CodexCamera")
        camera = bpy.data.objects.new("CodexCamera", camera_data)
        scene.collection.objects.link(camera)
    camera.location = (7.0, -7.0, 5.5)
    camera.rotation_euler = (math.radians(63.0), 0.0, math.radians(45.0))
    scene.camera = camera

    light = bpy.data.objects.get("CodexSun")
    if light is None:
        light_data = bpy.data.lights.new("CodexSun", type="SUN")
        light = bpy.data.objects.new("CodexSun", light_data)
        scene.collection.objects.link(light)
    light.location = (5.0, -5.0, 8.0)
    light.rotation_euler = (math.radians(45.0), 0.0, math.radians(45.0))


def mark_object(obj: bpy.types.Object, asset_id: str, part_id: str, display_name: str) -> None:
    obj["codex_asset_id"] = asset_id
    obj["codex_part_id"] = part_id
    obj["codex_display_name"] = display_name


def create_placeholder_asset(asset_id: str, display_name: str) -> None:
    bpy.ops.mesh.primitive_cube_add(location=(0.0, 0.0, 1.0))
    body = bpy.context.active_object
    body.name = f"obj_{asset_id}_part_body"
    body.scale = (1.4, 0.9, 2.0)
    mark_object(body, asset_id, "part_body", "body")

    bpy.ops.mesh.primitive_cube_add(location=(1.55, 0.0, 1.0))
    door = bpy.context.active_object
    door.name = f"obj_{asset_id}_part_door"
    door.scale = (0.1, 0.82, 1.82)
    mark_object(door, asset_id, "part_door", "door")

    bpy.ops.mesh.primitive_cylinder_add(radius=0.08, depth=0.32, location=(1.72, 0.0, 1.0), rotation=(0.0, math.radians(90.0), 0.0))
    handle = bpy.context.active_object
    handle.name = f"obj_{asset_id}_part_handle"
    mark_object(handle, asset_id, "part_handle", "handle")

    bpy.context.scene["codex_asset_display_name"] = display_name


def import_local_glb(glb_path: Path, asset_id: str) -> None:
    before = {obj.name for obj in bpy.data.objects}
    bpy.ops.import_scene.gltf(filepath=str(glb_path))
    for obj in bpy.data.objects:
        if obj.name in before or obj.type != "MESH":
            continue
        part_key = f"part_{to_key(obj.name)}"
        mark_object(obj, asset_id, part_key, obj.name)


def materialize_glb(glb_url: str, downloads_root: Path, asset_id: str) -> Path | None:
    if not glb_url:
        return None
    if glb_url.startswith("file://"):
        local_path = Path(glb_url[7:])
        return local_path if local_path.exists() else None

    direct_path = Path(glb_url)
    if direct_path.exists():
        return direct_path

    if glb_url.startswith("http://") or glb_url.startswith("https://"):
        ensure_directory(downloads_root)
        target_path = downloads_root / f"{asset_id}.glb"
        request = urllib.request.Request(
            glb_url,
            headers={"User-Agent": "codex-lan-agent-3d/1.0"},
        )
        with urllib.request.urlopen(request, timeout=120) as response:
            with target_path.open("wb") as handle:
                shutil.copyfileobj(response, handle)
        return target_path if target_path.exists() else None

    return None


def parse_vec3(text: str) -> tuple[float, float, float]:
    parts = text.split(",")
    if len(parts) != 3:
        raise ValueError(f"Invalid vec3: {text}")
    return float(parts[0]), float(parts[1]), float(parts[2])


def find_object_or_raise(object_id: str) -> bpy.types.Object:
    obj = bpy.data.objects.get(object_id)
    if obj is None:
        raise RuntimeError(f"Object not found: {object_id}")
    return obj


def import_asset(arguments: dict[str, str], blend_path: Path) -> dict[str, str]:
    asset_id = arguments.get("asset_id", "").strip() or "asset_live"
    display_name = arguments.get("display_name", "").strip() or asset_id
    glb_url = arguments.get("glb_url", "").strip()

    reset_scene()
    ensure_camera_and_light()

    downloads_root = blend_path.parent / "downloads"
    local_glb = materialize_glb(glb_url, downloads_root, asset_id)
    import_source = "placeholder"

    if local_glb and local_glb.exists():
        import_local_glb(local_glb, asset_id)
        import_source = "glb"
    else:
        create_placeholder_asset(asset_id, display_name)

    bpy.ops.wm.save_as_mainfile(filepath=str(blend_path))
    fields = build_scene_fields({"import_status": "ok", "import_source": import_source})
    return fields


def transform_object(arguments: dict[str, str], blend_path: Path) -> dict[str, str]:
    obj = find_object_or_raise(arguments.get("object_id", ""))
    obj.location = parse_vec3(arguments.get("translation", "0,0,0"))
    rotation = parse_vec3(arguments.get("rotation", "0,0,0"))
    obj.rotation_euler = tuple(math.radians(value) for value in rotation)
    obj.scale = parse_vec3(arguments.get("scale", "1,1,1"))
    bpy.context.view_layer.update()
    bpy.ops.wm.save_as_mainfile(filepath=str(blend_path))
    return build_scene_fields({"transform_status": "ok", "translation": arguments.get("translation", "0,0,0")})


def get_scene_snapshot() -> dict[str, str]:
    return build_scene_fields()


def capture_viewport(arguments: dict[str, str], output_root: Path) -> dict[str, str]:
    session_id = arguments.get("session_id", "session_live")
    image_path = output_root / f"{session_id}_viewport.png"
    ensure_directory(output_root)

    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(image_path)
    bpy.ops.render.render(write_still=True)
    return {"image_path": str(image_path)}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--request-file", required=True)
    parser.add_argument("--response-file", required=True)
    parser.add_argument("--session-root", required=True)
    raw_args = sys.argv[1:]
    if "--" in raw_args:
        raw_args = raw_args[raw_args.index("--") + 1 :]
    args = parser.parse_args(raw_args)

    request_path = Path(args.request_file)
    response_path = Path(args.response_file)
    session_root = Path(args.session_root)
    ensure_directory(session_root)
    ensure_directory(response_path.parent)

    values = read_request_file(request_path)
    tool_name = values.get("tool", "")
    arguments = {key[4:]: value for key, value in values.items() if key.startswith("arg.")}
    session_id = arguments.get("session_id", "session_live")
    blend_path = session_root / f"{session_id}.blend"
    output_root = session_root / "captures"

    try:
        if tool_name == "blender.import_asset":
            fields = import_asset(arguments, blend_path)
        elif tool_name == "blender.transform_object":
            fields = transform_object(arguments, blend_path)
        elif tool_name == "blender.get_scene_snapshot":
            fields = get_scene_snapshot()
        elif tool_name == "blender.capture_viewport":
            fields = capture_viewport(arguments, output_root)
        else:
            raise RuntimeError(f"Unsupported Blender tool: {tool_name}")
        write_response_file(response_path, True, 0, fields)
        return 0
    except Exception as ex:
        write_response_file(response_path, False, 1, {"error": str(ex)})
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
