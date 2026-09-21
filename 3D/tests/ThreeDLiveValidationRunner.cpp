#include "BlenderExternalToolInvoker.h"
#include "BlenderMcpSceneBackend.h"
#include "Nova3DAssetAdapter.h"
#include "Nova3DExternalToolInvoker.h"
#include "Nova3DMcpHostedBackend.h"
#include "ThreeDHostedWorkflowCoordinator.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace codex_lan_agent_3d;

namespace {

void Require(bool condition, const std::string & message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::string ResolvePythonPath() {
    const char * local_app_data = std::getenv("LOCALAPPDATA");
    Require(local_app_data != nullptr, "LOCALAPPDATA is not available");
    const std::filesystem::path path =
        std::filesystem::path(local_app_data) / "Programs" / "Python" / "Python311" / "python.exe";
    Require(std::filesystem::exists(path), "Python 3.11 executable is missing");
    return path.string();
}

std::filesystem::path ResolveToolsRoot(const char * argv0) {
    const std::filesystem::path exe_path = std::filesystem::absolute(argv0);
    return exe_path.parent_path().parent_path().parent_path() / "tools";
}

std::filesystem::path ResolveRuntimeRoot(const char * argv0) {
    const std::filesystem::path exe_path = std::filesystem::absolute(argv0);
    return exe_path.parent_path().parent_path().parent_path() / "runtime";
}

std::string ReadEnvValueFromFile(const std::filesystem::path & env_path, const std::string & key) {
    if (!std::filesystem::exists(env_path)) {
        return "";
    }

    std::ifstream input(env_path);
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#' || line.find('=') == std::string::npos) {
            continue;
        }
        const std::size_t separator = line.find('=');
        const std::string current_key = line.substr(0, separator);
        if (current_key != key) {
            continue;
        }
        std::string value = line.substr(separator + 1);
        if (!value.empty() && (value.front() == '"' || value.front() == '\'')) {
            value.erase(value.begin());
        }
        if (!value.empty() && (value.back() == '"' || value.back() == '\'')) {
            value.pop_back();
        }
        return value;
    }
    return "";
}

std::string ResolveRuntimeSetting(
    const std::filesystem::path & env_path,
    const char * process_key,
    const std::string & fallback) {
    const char * process_value = std::getenv(process_key);
    if (process_value != nullptr && process_value[0] != '\0') {
        return process_value;
    }
    const std::string file_value = ReadEnvValueFromFile(env_path, process_key);
    if (!file_value.empty()) {
        return file_value;
    }
    return fallback;
}

std::string EscapeJson(const std::string & value) {
    return JsonEscape(value);
}

void WriteJsonObject(const std::filesystem::path & output_path, const std::vector<std::pair<std::string, std::string>> & entries) {
    std::ofstream output(output_path, std::ios::out | std::ios::trunc);
    output << "{\n";
    for (std::size_t index = 0; index < entries.size(); ++index) {
        output << "  \"" << entries[index].first << "\": \"" << EscapeJson(entries[index].second) << "\"";
        if (index + 1 < entries.size()) {
            output << ",";
        }
        output << "\n";
    }
    output << "}\n";
}

std::string GetFieldOrDefault(const CommandResult & result, const std::string & key, const std::string & fallback = std::string()) {
    const auto it = result.fields.find(key);
    if (it == result.fields.end()) {
        return fallback;
    }
    return it->second;
}

std::string FirstObjectId(const std::string & scene_objects) {
    const std::size_t row_end = scene_objects.find('|');
    const std::string row = row_end == std::string::npos ? scene_objects : scene_objects.substr(0, row_end);
    const std::size_t separator = row.find(':');
    if (separator == std::string::npos) {
        return "";
    }
    return row.substr(0, separator);
}

}  // namespace

int main(int argc, char ** argv) {
    try {
        Require(argc > 0, "argv[0] unavailable");
        const std::string python_path = ResolvePythonPath();
        const std::filesystem::path tools_root = ResolveToolsRoot(argv[0]);
        const std::filesystem::path runtime_root = ResolveRuntimeRoot(argv[0]);
        std::filesystem::create_directories(runtime_root);
        const std::filesystem::path env_path = runtime_root / "nova3d.env";
        const std::string prompt = ResolveRuntimeSetting(
            env_path,
            "THREED_NOVA3D_PROMPT",
            "a simple cabinet with a body and a door");
        const std::string model = ResolveRuntimeSetting(
            env_path,
            "THREED_NOVA3D_MODEL",
            "gemini");
        const std::string session_id = ResolveRuntimeSetting(
            env_path,
            "THREED_SESSION_ID",
            "session_live_runner");

        const std::filesystem::path nova_script = tools_root / "Nova3DLiveBridge.py";
        const std::filesystem::path blender_script = tools_root / "BlenderLiveBridge.py";
        Require(std::filesystem::exists(nova_script), "Nova3D live bridge script is missing");
        Require(std::filesystem::exists(blender_script), "Blender live bridge script is missing");

        auto nova_command = std::make_shared<ExternalToolCommandInvoker>(ExternalToolCommandConfig{
            python_path,
            {nova_script.string()}
        });
        auto blender_command = std::make_shared<ExternalToolCommandInvoker>(ExternalToolCommandConfig{
            python_path,
            {blender_script.string()}
        });

        const CommandResult nova_health = nova_command->Invoke("bridge.healthcheck", {});
        const CommandResult blender_health = blender_command->Invoke("bridge.healthcheck", {});

        std::string status = "ready_for_live_e2e";
        std::string detail = "Nova3D and Blender are ready";
        std::string viewport_path;
        std::string conversation_url;
        std::string validation_mode = "nova3d_blender_live";
        std::string viewport_exists = "false";
        std::string blender_import_source;
        const std::string sample_glb_url =
            "https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/main/Models/Box/glTF-Binary/Box.glb";

        if (!nova_health.ok) {
            status = "blocked_nova_bridge";
            detail = GetFieldOrDefault(nova_health, "error", "Nova3D bridge healthcheck failed");
        }
        else if (!blender_health.ok) {
            status = "blocked_blender_bridge";
            detail = GetFieldOrDefault(blender_health, "error", "Blender bridge healthcheck failed");
        }
        else if (GetFieldOrDefault(blender_health, "blender_available") != "true") {
            status = "blocked_blender_missing";
            detail = "Blender executable is not available";
        }
        else if (GetFieldOrDefault(nova_health, "token_present") != "true") {
            validation_mode = "blender_remote_glb_fallback";
            const CommandResult imported = blender_command->Invoke(
                "blender.import_asset",
                {
                    {"session_id", session_id},
                    {"asset_id", "asset_runtime_remote_glb"},
                    {"glb_url", sample_glb_url},
                    {"display_name", "runtime remote box"}
                });
            Require(imported.ok, "fallback blender import failed");
            blender_import_source = GetFieldOrDefault(imported, "import_source");

            const std::string object_id = FirstObjectId(GetFieldOrDefault(imported, "scene_objects"));
            Require(!object_id.empty(), "fallback blender import did not expose an object id");

            const CommandResult transformed = blender_command->Invoke(
                "blender.transform_object",
                {
                    {"session_id", session_id},
                    {"object_id", object_id},
                    {"translation", "3,0,0"},
                    {"rotation", "0,15,0"},
                    {"scale", "1,1,1"}
                });
            Require(transformed.ok, "fallback blender transform failed");

            const CommandResult captured = blender_command->Invoke(
                "blender.capture_viewport",
                {
                    {"session_id", session_id}
                });
            Require(captured.ok, "fallback blender viewport capture failed");
            viewport_path = GetFieldOrDefault(captured, "image_path");
            viewport_exists = std::filesystem::exists(viewport_path) ? "true" : "false";
            status = "blender_ready_waiting_nova_token";
            detail = "Blender live flow verified with a remote GLB sample; NOVA3D_TOKEN is still required for Nova3D generation";
        }
        else {
            auto state_store = std::make_shared<ThreeDSessionStateStore>();
            auto nova_backend = std::make_shared<Nova3DMcpHostedBackend>(
                std::make_shared<Nova3DExternalToolInvoker>(nova_command));
            auto blender_backend = std::make_shared<BlenderMcpSceneBackend>(
                std::make_shared<BlenderExternalToolInvoker>(blender_command));
            auto generator = std::make_shared<Nova3DAssetAdapter>(nova_backend, state_store, session_id);
            auto scene_adapter = std::make_shared<BlenderSceneAdapter>(blender_backend, state_store, session_id);
            auto orchestrator = std::make_shared<ThreeDOrchestrator>(generator, scene_adapter);
            auto coordinator = std::make_shared<ThreeDHostedWorkflowCoordinator>(
                state_store,
                nova_backend,
                blender_backend,
                orchestrator,
                session_id);

            const CommandResult started = coordinator->StartSession(
                "produce live validation report",
                "trace_live_runner");
            Require(started.ok, "live runner session start failed");

            const CommandResult generated = coordinator->GenerateStructuredAsset(
                prompt,
                model);
            Require(generated.ok, "live runner generation failed");
            conversation_url = GetFieldOrDefault(generated, "conversation_url");

            const CommandResult imported = coordinator->ImportActiveAsset();
            Require(imported.ok, "live runner import failed");
            blender_import_source = GetFieldOrDefault(imported, "import_source");

            const CommandResult captured = coordinator->CaptureViewport();
            Require(captured.ok, "live runner viewport capture failed");
            viewport_path = GetFieldOrDefault(captured, "image_path");
            viewport_exists = std::filesystem::exists(viewport_path) ? "true" : "false";
            detail = "Live end-to-end validation completed";
        }

        const std::filesystem::path report_path = runtime_root / "live_validation_report.json";
        WriteJsonObject(
            report_path,
            {
                {"status", status},
                {"detail", detail},
                {"validation_mode", validation_mode},
                {"prompt", prompt},
                {"model", model},
                {"session_id", session_id},
                {"nova_token_present", GetFieldOrDefault(nova_health, "token_present", "false")},
                {"nova_token_source", GetFieldOrDefault(nova_health, "token_source")},
                {"nova_api_url", GetFieldOrDefault(nova_health, "api_url")},
                {"nova_repo_path", GetFieldOrDefault(nova_health, "repo_path")},
                {"blender_available", GetFieldOrDefault(blender_health, "blender_available", "false")},
                {"blender_path", GetFieldOrDefault(blender_health, "blender_path")},
                {"blender_import_source", blender_import_source},
                {"fallback_glb_url", sample_glb_url},
                {"conversation_url", conversation_url},
                {"viewport_path", viewport_path},
                {"viewport_exists", viewport_exists},
                {"report_path", report_path.string()}
            });

        std::cout << "3D live validation runner completed: " << status << "\n";
        std::cout << report_path.string() << "\n";
        return 0;
    }
    catch (const std::exception & ex) {
        std::cerr << ex.what() << "\n";
        return 1;
    }
}
