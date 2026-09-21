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

        auto nova_invoker = std::make_shared<ExternalToolCommandInvoker>(ExternalToolCommandConfig{
            python_path,
            {nova_script.string(), "--force-missing-token"}
        });
        auto blender_invoker = std::make_shared<ExternalToolCommandInvoker>(ExternalToolCommandConfig{
            python_path,
            {blender_script.string(), "--force-missing-blender"}
        });

        const CommandResult nova_failure = nova_invoker->Invoke(
            "generate_3d",
            {
                {"prompt", "failure smoke chair"},
                {"model", "gemini"}
            });
        Require(!nova_failure.ok, "Nova3D missing-token smoke should fail");
        Require(
            nova_failure.fields.at("error").find("NOVA3D_TOKEN") != std::string::npos,
            "Nova3D missing-token smoke should expose token error");

        const CommandResult blender_failure = blender_invoker->Invoke(
            "blender.capture_viewport",
            {
                {"session_id", "session_live_failure"}
            });
        Require(!blender_failure.ok, "Blender missing-executable smoke should fail");
        Require(
            blender_failure.fields.at("error").find("Blender executable not found") != std::string::npos,
            "Blender missing-executable smoke should expose blender error");

        std::cout << "3D live bridge failure smoke test passed\n";
        return 0;
    }
    catch (const std::exception & ex) {
        std::cerr << ex.what() << "\n";
        return 1;
    }
}
