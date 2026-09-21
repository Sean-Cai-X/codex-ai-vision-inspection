import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path


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


def emit_success(fields: dict[str, str]) -> int:
    print("ok=true")
    print("exit_code=0")
    for key, value in fields.items():
        print(f"field.{key}={encode_value(value)}")
    return 0


def emit_failure(message: str, extra_fields: dict[str, str] | None = None) -> int:
    print("ok=false")
    print("exit_code=1")
    print(f"field.error={encode_value(message)}")
    print(f"field.error_message={encode_value(message)}")
    for key, value in (extra_fields or {}).items():
        print(f"field.{key}={encode_value(value)}")
    return 1


def resolve_blender_executable(force_missing_blender: bool) -> str:
    if force_missing_blender:
        return ""

    direct = os.environ.get("THREED_BLENDER_EXE", "").strip()
    if direct and Path(direct).exists():
        return direct

    roots = [
        Path("C:/Program Files/Blender Foundation"),
        Path("C:/Program Files"),
    ]
    candidates: list[Path] = []
    for root in roots:
        if not root.exists():
            continue
        candidates.extend(root.glob("Blender*/blender.exe"))
    candidates = sorted(candidates, reverse=True)
    if candidates:
        return str(candidates[0])
    return ""


def resolve_workspace_root() -> Path:
    return Path(__file__).resolve().parents[3]


def resolve_worker_script() -> Path:
    return Path(__file__).resolve().with_name("BlenderSceneWorker.py")


def run_blender_tool(blender_path: str, request_path: Path, session_id: str) -> tuple[bool, dict[str, str], str]:
    worker_script = resolve_worker_script()
    if not worker_script.exists():
        return False, {}, "Blender scene worker is missing"

    session_root = resolve_workspace_root() / "codex-lan-agent" / "3D" / "runtime" / "blender_sessions"
    session_root.mkdir(parents=True, exist_ok=True)
    response_path = Path(tempfile.gettempdir()) / f"codex_3d_blender_response_{session_id}.txt"
    if response_path.exists():
        response_path.unlink()

    blend_path = session_root / f"{session_id}.blend"
    command = [blender_path]
    if blend_path.exists():
        command.append(str(blend_path))
        command.append("--background")
    else:
        command.extend(["--background", "--factory-startup"])
    command.extend(
        [
            "--python",
            str(worker_script),
            "--",
            "--request-file",
            str(request_path),
            "--response-file",
            str(response_path),
            "--session-root",
            str(session_root),
        ]
    )

    completed = subprocess.run(
        command,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    if not response_path.exists():
        message = (completed.stderr or completed.stdout or "Blender did not produce a response file").strip()
        return False, {"blender_stdout": completed.stdout.strip(), "blender_stderr": completed.stderr.strip()}, message

    fields: dict[str, str] = {}
    ok = False
    exit_code = completed.returncode
    for raw_line in response_path.read_text(encoding="utf-8").splitlines():
        if "=" not in raw_line:
            continue
        key, value = raw_line.split("=", 1)
        value = decode_value(value)
        if key == "ok":
            ok = value in {"true", "1", "yes", "ok"}
            continue
        if key == "exit_code":
            try:
                exit_code = int(value)
            except ValueError:
                exit_code = completed.returncode
            continue
        if key.startswith("field."):
            fields[key[6:]] = value

    fields["blender_stdout"] = completed.stdout.strip()
    fields["blender_stderr"] = completed.stderr.strip()
    if not ok and "error" not in fields:
        fields["error"] = (completed.stderr or completed.stdout or "Blender tool failed").strip()
    return ok, fields, fields.get("error", "")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--request-file", required=True)
    parser.add_argument("--force-missing-blender", action="store_true")
    args = parser.parse_args()

    request_path = Path(args.request_file)
    if not request_path.exists():
        return emit_failure("request file not found", {"request_file": str(request_path)})

    values = read_request_file(request_path)
    tool_name = values.get("tool", "")
    arguments = {key[4:]: value for key, value in values.items() if key.startswith("arg.")}
    blender_path = resolve_blender_executable(args.force_missing_blender)

    if tool_name == "bridge.healthcheck":
        return emit_success(
            {
                "bridge_kind": "blender",
                "python_path": sys.executable,
                "blender_available": "true" if blender_path else "false",
                "blender_path": blender_path,
            }
        )

    if not blender_path:
        return emit_failure("Blender executable not found", {"tool_name": tool_name})
    if tool_name not in {
        "blender.import_asset",
        "blender.transform_object",
        "blender.get_scene_snapshot",
        "blender.capture_viewport",
    }:
        return emit_failure(
            "Unsupported Blender tool",
            {
                "tool_name": tool_name,
                "blender_path": blender_path,
            },
        )

    ok, fields, error = run_blender_tool(
        blender_path,
        request_path,
        arguments.get("session_id", "session_live"),
    )
    fields["tool_name"] = tool_name
    fields["blender_path"] = blender_path
    if not ok:
        return emit_failure(error or "Blender live scene operation failed", fields)
    return emit_success(fields)


if __name__ == "__main__":
    raise SystemExit(main())
