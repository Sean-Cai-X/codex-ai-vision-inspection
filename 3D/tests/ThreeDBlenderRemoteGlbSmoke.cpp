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
        const std::filesystem::path blender_script = tools_root / "BlenderLiveBridge.py";
        Require(std::filesystem::exists(blender_script), "Blender live bridge script is missing");

        auto blender_invoker = std::make_shared<ExternalToolCommandInvoker>(ExternalToolCommandConfig{
            python_path,
            {blender_script.string()}
        });

        const std::string session_id = "session_blender_remote_glb";
        const std::string sample_glb_url =
            "https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/main/Models/Box/glTF-Binary/Box.glb";

        const CommandResult imported = blender_invoker->Invoke(
            "blender.import_asset",
            {
                {"session_id", session_id},
                {"asset_id", "asset_remote_glb"},
                {"glb_url", sample_glb_url},
                {"display_name", "remote box"}
            });
        Require(imported.ok, "remote blender import should pass: " + DescribeResult(imported));
        Require(imported.fields.at("import_source") == "glb", "remote import should use glb source");
        Require(imported.fields.at("scene_object_count") != "0", "remote import should produce scene objects");

        const std::string object_id = FirstObjectId(imported.fields.at("scene_objects"));
        Require(!object_id.empty(), "remote import should expose at least one object id");

        const CommandResult transformed = blender_invoker->Invoke(
            "blender.transform_object",
            {
                {"session_id", session_id},
                {"object_id", object_id},
                {"translation", "3,0,0"},
                {"rotation", "0,15,0"},
                {"scale", "1,1,1"}
            });
        Require(transformed.ok, "remote blender transform should pass: " + DescribeResult(transformed));

        const CommandResult snapshot = blender_invoker->Invoke(
            "blender.get_scene_snapshot",
            {
                {"session_id", session_id}
            });
        Require(snapshot.ok, "remote blender snapshot should pass: " + DescribeResult(snapshot));
        Require(
            snapshot.fields.at("scene_objects").find("asset_remote_glb") != std::string::npos,
            "remote snapshot should retain asset id: " + DescribeResult(snapshot));

        const CommandResult captured = blender_invoker->Invoke(
            "blender.capture_viewport",
            {
                {"session_id", session_id}
            });
        Require(captured.ok, "remote blender viewport capture should pass: " + DescribeResult(captured));
        Require(std::filesystem::exists(captured.fields.at("image_path")), "remote viewport image should exist");

        std::cout << "3D blender remote glb smoke test passed\n";
        return 0;
    }
    catch (const std::exception & ex) {
        std::cerr << ex.what() << "\n";
        return 1;
    }
}
