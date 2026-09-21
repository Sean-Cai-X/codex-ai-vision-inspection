#include "BlenderExternalToolInvoker.h"
#include "BlenderMcpSceneBackend.h"
#include "Nova3DAssetAdapter.h"
#include "Nova3DExternalToolInvoker.h"
#include "Nova3DMcpHostedBackend.h"
#include "ThreeDHostedWorkflowCoordinator.h"

#include <cstdlib>
#include <filesystem>
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

std::string DescribeResult(const CommandResult & result) {
    std::string description =
        "ok=" + std::string(result.ok ? "true" : "false") +
        ", exit_code=" + std::to_string(result.exit_code);
    for (const auto & entry : result.fields) {
        description += ", " + entry.first + "=" + entry.second;
    }
    return description;
}

}  // namespace

int main(int argc, char ** argv) {
    try {
        Require(argc > 0, "argv[0] unavailable");
        const std::string python_path = ResolvePythonPath();
        const std::filesystem::path tools_root = ResolveToolsRoot(argv[0]);
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
        Require(nova_health.ok, "Nova3D bridge healthcheck should pass: " + DescribeResult(nova_health));
        const CommandResult blender_health = blender_command->Invoke("bridge.healthcheck", {});
        Require(blender_health.ok, "Blender bridge healthcheck should pass: " + DescribeResult(blender_health));
        Require(
            blender_health.fields.at("blender_available") == "true",
            "Blender must be available for live end-to-end smoke: " + DescribeResult(blender_health));

        if (nova_health.fields.at("token_present") != "true") {
            std::cout << "3D live end-to-end smoke skipped: NOVA3D_TOKEN not configured\n";
            return 0;
        }

        auto state_store = std::make_shared<ThreeDSessionStateStore>();
        auto nova_backend = std::make_shared<Nova3DMcpHostedBackend>(
            std::make_shared<Nova3DExternalToolInvoker>(nova_command));
        auto blender_backend = std::make_shared<BlenderMcpSceneBackend>(
            std::make_shared<BlenderExternalToolInvoker>(blender_command));
        auto generator = std::make_shared<Nova3DAssetAdapter>(nova_backend, state_store, "session_live_e2e");
        auto scene_adapter = std::make_shared<BlenderSceneAdapter>(blender_backend, state_store, "session_live_e2e");
        auto orchestrator = std::make_shared<ThreeDOrchestrator>(generator, scene_adapter);
        auto coordinator = std::make_shared<ThreeDHostedWorkflowCoordinator>(
            state_store,
            nova_backend,
            blender_backend,
            orchestrator,
            "session_live_e2e");

        const CommandResult started = coordinator->StartSession(
            "validate live nova3d to blender flow",
            "trace_live_e2e");
        Require(started.ok, "live session start should pass: " + DescribeResult(started));

        const CommandResult generated = coordinator->GenerateStructuredAsset(
            "a simple cabinet with a body and a door",
            "gemini");
        Require(generated.ok, "live generation should pass: " + DescribeResult(generated));
        Require(!generated.fields.at("glb_url").empty(), "live generation should return glb_url");
        Require(!generated.fields.at("workflow_id").empty(), "live generation should return workflow_id");

        const CommandResult imported = coordinator->ImportActiveAsset();
        Require(imported.ok, "live import should pass: " + DescribeResult(imported));
        Require(
            imported.fields.at("scene_object_count") != "0",
            "live import should produce scene objects: " + DescribeResult(imported));

        const CommandResult summary = coordinator->BuildSessionSummary();
        Require(summary.ok, "live session summary should pass: " + DescribeResult(summary));
        Require(!summary.fields.at("active_object_id").empty(), "live summary should expose active object");

        const CommandResult captured = coordinator->CaptureViewport();
        Require(captured.ok, "live viewport capture should pass: " + DescribeResult(captured));
        Require(!captured.fields.at("image_path").empty(), "live viewport capture should return image_path");
        Require(std::filesystem::exists(captured.fields.at("image_path")), "live viewport image should exist");

        std::cout << "3D live end-to-end smoke test passed\n";
        return 0;
    }
    catch (const std::exception & ex) {
        std::cerr << ex.what() << "\n";
        return 1;
    }
}
