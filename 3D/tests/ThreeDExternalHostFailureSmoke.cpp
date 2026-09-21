#include "BlenderExternalToolInvoker.h"
#include "BlenderMcpSceneBackend.h"
#include "Nova3DAssetAdapter.h"
#include "Nova3DExternalToolInvoker.h"
#include "Nova3DMcpHostedBackend.h"
#include "ThreeDHostValidation.h"

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

std::string ResolveStubPath(const char * argv0) {
    return (std::filesystem::path(argv0).parent_path() / "codex_lan_agent_3d_tool_stub.exe").string();
}

}  // namespace

int main(int argc, char ** argv) {
    try {
        Require(argc > 0, "argv[0] unavailable");
        const std::string stub_path = ResolveStubPath(argv[0]);
        Require(std::filesystem::exists(stub_path), "tool stub executable missing");

        auto state_store = std::make_shared<ThreeDSessionStateStore>();
        auto nova_command = std::make_shared<ExternalToolCommandInvoker>(ExternalToolCommandConfig{
            stub_path,
            {"--backend", "nova3d"}
        });
        auto blender_command = std::make_shared<ExternalToolCommandInvoker>(ExternalToolCommandConfig{
            stub_path,
            {"--backend", "blender", "--mode", "viewport_failure"}
        });
        auto nova_backend = std::make_shared<Nova3DMcpHostedBackend>(
            std::make_shared<Nova3DExternalToolInvoker>(nova_command));
        auto blender_backend = std::make_shared<BlenderMcpSceneBackend>(
            std::make_shared<BlenderExternalToolInvoker>(blender_command));
        auto generator = std::make_shared<Nova3DAssetAdapter>(nova_backend, state_store, "session_workflow");
        auto scene_adapter = std::make_shared<BlenderSceneAdapter>(blender_backend, state_store, "session_workflow");
        auto orchestrator = std::make_shared<ThreeDOrchestrator>(generator, scene_adapter);
        auto coordinator = std::make_shared<ThreeDHostedWorkflowCoordinator>(
            state_store,
            nova_backend,
            blender_backend,
            orchestrator,
            "session_workflow");

        ThreeDHostValidationSuite validation(coordinator, state_store);
        const CommandResult end_to_end = validation.RunEndToEndValidationMatrix(
            "validate external host failure bridge",
            "trace_external_host_failure",
            "external failure cabinet",
            "gemini");
        Require(!end_to_end.ok, "external end-to-end validation should fail");

        const CommandResult blender = validation.RunBlenderValidationMatrix();
        Require(!blender.ok, "external blender failure should surface");
        Require(
            blender.fields.at("validation_rows").find("blender.viewport_capture:fail") != std::string::npos,
            "viewport capture failure should remain visible across external bridge");

        std::cout << "3D external host failure smoke test passed\n";
        return 0;
    }
    catch (const std::exception & ex) {
        std::cerr << ex.what() << "\n";
        return 1;
    }
}
