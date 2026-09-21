#include "ExternalToolCommandInvoker.h"

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
        const std::filesystem::path runtime_root = ResolveRuntimeRoot(argv[0]);
        std::filesystem::create_directories(runtime_root);

        const std::filesystem::path nova_script = tools_root / "Nova3DLiveBridge.py";
        Require(std::filesystem::exists(nova_script), "Nova3D live bridge script is missing");

        const std::filesystem::path env_path = runtime_root / "nova3d.env";
        const std::filesystem::path backup_path = runtime_root / "nova3d.env.bak_codex_test";
        if (std::filesystem::exists(backup_path)) {
            std::filesystem::remove(backup_path);
        }
        if (std::filesystem::exists(env_path)) {
            std::filesystem::rename(env_path, backup_path);
        }

        {
            std::ofstream env_file(env_path, std::ios::out | std::ios::trunc);
            env_file << "NOVA3D_TOKEN=n3d_fake_token_for_env_smoke\n";
            env_file << "NOVA3D_API_URL=https://nova3d.xyz/api\n";
        }

        auto restore = [&]() {
            if (std::filesystem::exists(env_path)) {
                std::filesystem::remove(env_path);
            }
            if (std::filesystem::exists(backup_path)) {
                std::filesystem::rename(backup_path, env_path);
            }
        };

        auto nova_invoker = std::make_shared<ExternalToolCommandInvoker>(ExternalToolCommandConfig{
            python_path,
            {nova_script.string()}
        });

        const CommandResult health = nova_invoker->Invoke("bridge.healthcheck", {});
        restore();

        Require(health.ok, "Nova3D env healthcheck should pass: " + DescribeResult(health));
        Require(health.fields.at("token_present") == "true", "Nova3D env file should expose token presence");
        Require(
            health.fields.at("token_source").find("nova3d.env") != std::string::npos,
            "Nova3D env file should be reported as token source: " + DescribeResult(health));
        Require(
            health.fields.at("api_url") == "https://nova3d.xyz/api",
            "Nova3D env file should populate API URL: " + DescribeResult(health));

        std::cout << "3D nova3d env file smoke test passed\n";
        return 0;
    }
    catch (const std::exception & ex) {
        std::cerr << ex.what() << "\n";
        return 1;
    }
}
