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
            {nova_script.string()}
        });
        auto blender_invoker = std::make_shared<ExternalToolCommandInvoker>(ExternalToolCommandConfig{
            python_path,
            {blender_script.string()}
        });

        const CommandResult nova_health = nova_invoker->Invoke("bridge.healthcheck", {});
        Require(nova_health.ok, "Nova3D bridge healthcheck should pass");
        Require(nova_health.fields.at("bridge_kind") == "nova3d", "Nova3D bridge kind should be nova3d");
        Require(!nova_health.fields.at("python_path").empty(), "Nova3D bridge should expose python path");
        Require(!nova_health.fields.at("repo_path").empty(), "Nova3D bridge should expose repo path");

        const CommandResult blender_health = blender_invoker->Invoke("bridge.healthcheck", {});
        Require(blender_health.ok, "Blender bridge healthcheck should pass");
        Require(blender_health.fields.at("bridge_kind") == "blender", "Blender bridge kind should be blender");
        Require(
            blender_health.fields.find("blender_available") != blender_health.fields.end(),
            "Blender bridge should expose availability");

        std::cout << "3D live bridge smoke test passed\n";
        return 0;
    }
    catch (const std::exception & ex) {
        std::cerr << ex.what() << "\n";
        return 1;
    }
}
