import argparse
import asyncio
import json
import os
import sys
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


def resolve_workspace_root() -> Path:
    return Path(__file__).resolve().parents[3]


def resolve_nova_repo() -> Path:
    return resolve_workspace_root() / "analysis_workspace" / "nova3d-mcp-main"


def resolve_runtime_env_candidates() -> list[Path]:
    workspace_root = resolve_workspace_root()
    return [
        workspace_root / "codex-lan-agent" / "3D" / "runtime" / "nova3d.env",
        workspace_root / "analysis_workspace" / "nova3d-mcp-main" / ".env",
    ]


def load_env_value_from_file(target_key: str, env_path: Path) -> str:
    if not env_path.exists():
        return ""
    for raw_line in env_path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        if key.strip() != target_key:
            continue
        cleaned = value.strip().strip('"').strip("'")
        return cleaned
    return ""


def ensure_runtime_env_loaded() -> str:
    token_from_process = os.environ.get("NOVA3D_TOKEN", "").strip()
    api_url_from_process = os.environ.get("NOVA3D_API_URL", "").strip()
    if token_from_process and api_url_from_process:
        return "process"

    if token_from_process and not api_url_from_process:
        for env_path in resolve_runtime_env_candidates():
            loaded_api_url = load_env_value_from_file("NOVA3D_API_URL", env_path).strip()
            if loaded_api_url:
                os.environ["NOVA3D_API_URL"] = loaded_api_url
                return "process+" + str(env_path)
        return "process"

    for env_path in resolve_runtime_env_candidates():
        loaded_token = load_env_value_from_file("NOVA3D_TOKEN", env_path).strip()
        loaded_api_url = load_env_value_from_file("NOVA3D_API_URL", env_path).strip()
        if loaded_token and not token_from_process:
            os.environ["NOVA3D_TOKEN"] = loaded_token
        if loaded_api_url and not api_url_from_process:
            os.environ["NOVA3D_API_URL"] = loaded_api_url
        if loaded_token or loaded_api_url:
            return str(env_path)
    return ""


def add_repo_to_sys_path(repo_path: Path) -> None:
    repo_str = str(repo_path)
    if repo_str not in sys.path:
        sys.path.insert(0, repo_str)


def to_backend_value(value):
    if value is None:
        return ""
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, (int, float)):
        return str(value)
    if isinstance(value, str):
        return value
    if isinstance(value, list):
        if all(isinstance(item, str) for item in value):
            return "|".join(value)
        return json.dumps(value, separators=(",", ":"))
    if isinstance(value, dict):
        return json.dumps(value, separators=(",", ":"))
    return str(value)


def normalize_result(result: dict) -> dict[str, str]:
    fields: dict[str, str] = {}
    for key, value in result.items():
        if key == "failed":
            continue
        fields[key] = to_backend_value(value)
    return fields


async def dispatch_tool(tool_name: str, arguments: dict[str, str], force_missing_token: bool) -> tuple[bool, dict[str, str], str]:
    repo_path = resolve_nova_repo()
    if not repo_path.exists():
        return False, {"repo_path": str(repo_path)}, "nova3d-mcp repo not found"

    if force_missing_token:
        os.environ.pop("NOVA3D_TOKEN", None)

    token_source = "" if force_missing_token else ensure_runtime_env_loaded()
    add_repo_to_sys_path(repo_path)
    import nova3d_mcp.server as server_module

    if tool_name == "bridge.healthcheck":
        return True, {
            "bridge_kind": "nova3d",
            "python_path": sys.executable,
            "repo_path": str(repo_path),
            "token_present": "true" if bool(os.environ.get("NOVA3D_TOKEN", "").strip()) else "false",
            "token_source": token_source,
            "api_url": os.environ.get("NOVA3D_API_URL", "https://nova3d.xyz/api"),
        }, ""

    server_module._startup_error = None
    await server_module._validate_startup()

    if tool_name == "generate_3d":
        result = await server_module.generate_3d(
            prompt=arguments.get("prompt", ""),
            model=arguments.get("model"),
        )
    elif tool_name == "regenerate_part":
        result = await server_module.regenerate_part(
            code_artifact=json.loads(arguments.get("code_artifact", "{}")),
            part_type=arguments.get("part_type", ""),
            description=arguments.get("description", ""),
            model=arguments.get("model"),
        )
    elif tool_name == "add_part":
        result = await server_module.add_part(
            code_artifact=json.loads(arguments.get("code_artifact", "{}")),
            description=arguments.get("description", ""),
            model=arguments.get("model"),
        )
    elif tool_name == "articulate_model":
        model_artifact = arguments.get("model_artifact", "")
        result = await server_module.articulate_model(
            code_artifact=json.loads(arguments.get("code_artifact", "{}")),
            articulation_request=arguments.get("articulation_request", ""),
            model_url=arguments.get("model_url") or None,
            model_artifact=json.loads(model_artifact) if model_artifact else None,
            model=arguments.get("model"),
        )
    elif tool_name == "get_generation_status":
        result = await server_module.get_generation_status(
            workflow_id=arguments.get("workflow_id", ""),
        )
    else:
        return False, {"tool_name": tool_name}, "unsupported Nova3D tool"

    if result.get("failed"):
        fields = normalize_result(result)
        if token_source:
            fields["token_source"] = token_source
        return False, fields, result.get("error_message", "Nova3D tool failed")
    fields = normalize_result(result)
    if token_source:
        fields["token_source"] = token_source
    return True, fields, ""


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--request-file", required=True)
    parser.add_argument("--force-missing-token", action="store_true")
    args = parser.parse_args()

    request_path = Path(args.request_file)
    if not request_path.exists():
        return emit_failure("request file not found", {"request_file": str(request_path)})

    values = read_request_file(request_path)
    tool_name = values.get("tool", "")
    arguments = {key[4:]: value for key, value in values.items() if key.startswith("arg.")}
    try:
        ok, fields, error = asyncio.run(dispatch_tool(tool_name, arguments, args.force_missing_token))
    except Exception as ex:
        return emit_failure(str(ex))

    if not ok:
        return emit_failure(error, fields)
    return emit_success(fields)


if __name__ == "__main__":
    raise SystemExit(main())
