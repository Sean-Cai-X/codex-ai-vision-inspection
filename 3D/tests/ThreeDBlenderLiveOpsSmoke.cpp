#include "ExternalToolCommandInvoker.h"

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
        const std::filesystem::path blender_script = tools_root / "BlenderLiveBridge.py";
        Require(std::filesystem::exists(blender_script), "Blender live bridge script is missing");

        auto blender_invoker = std::make_shared<ExternalToolCommandInvoker>(ExternalToolCommandConfig{
            python_path,
            {blender_script.string()}
        });

        const std::string session_id = "session_blender_live_ops";
        const CommandResult imported = blender_invoker->Invoke(
            "blender.import_asset",
            {
                {"session_id", session_id},
                {"asset_id", "asset_blender_live"},
                {"glb_url", ""},
                {"display_name", "live cabinet"}
            });
        Require(imported.ok, "blender import should pass: " + DescribeResult(imported));
        Require(imported.fields.at("scene_object_count") == "3", "blender import should create 3 objects");

        const std::string object_id = "obj_asset_blender_live_part_door";
        const CommandResult transformed = blender_invoker->Invoke(
            "blender.transform_object",
            {
                {"session_id", session_id},
                {"object_id", object_id},
                {"translation", "2,0,1"},
                {"rotation", "0,30,0"},
                {"scale", "1,1,1"}
            });
        Require(transformed.ok, "blender transform should pass: " + DescribeResult(transformed));
        Require(transformed.fields.at("translation") == "2,0,1", "transform translation should be echoed");

        const CommandResult snapshot = blender_invoker->Invoke(
            "blender.get_scene_snapshot",
            {
                {"session_id", session_id}
            });
        Require(snapshot.ok, "blender snapshot should pass: " + DescribeResult(snapshot));
        Require(
            snapshot.fields.at("scene_objects").find("obj_asset_blender_live_part_door:asset_blender_live:part_door:door:2.0,0.0,1.0") != std::string::npos,
            "snapshot should contain transformed door location: " + DescribeResult(snapshot));

        const CommandResult captured = blender_invoker->Invoke(
            "blender.capture_viewport",
            {
                {"session_id", session_id}
            });
        Require(captured.ok, "blender viewport capture should pass: " + DescribeResult(captured));
        const std::filesystem::path image_path = captured.fields.at("image_path");
        Require(std::filesystem::exists(image_path), "viewport image should exist");

        std::cout << "3D blender live ops smoke test passed\n";
        return 0;
    }
    catch (const std::exception & ex) {
        std::cerr << ex.what() << "\n";
        return 1;
    }
}
