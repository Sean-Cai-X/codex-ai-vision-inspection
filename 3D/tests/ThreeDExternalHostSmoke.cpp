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

std::string DescribeResult(const CommandResult & result) {
    std::string description =
        "ok=" + std::string(result.ok ? "true" : "false") +
        ", exit_code=" + std::to_string(result.exit_code);
    for (const auto & entry : result.fields) {
        description += ", " + entry.first + "=" + entry.second;
    }
    return description;
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
            {"--backend", "blender"}
        });
        const CommandResult direct_nova = nova_command->Invoke(
            "generate_3d",
            {
                {"prompt", "external cabinet"},
                {"model", "gemini"}
            });
        Require(
            direct_nova.fields.find("conversation_url") != direct_nova.fields.end(),
            "direct nova call should expose conversation_url: " + DescribeResult(direct_nova));

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
        const CommandResult started = coordinator->StartSession(
            "validate external host bridge",
            "trace_external_host");
        Require(started.ok, "session start should pass: " + DescribeResult(started));

        const CommandResult nova = validation.RunNova3DValidationMatrix("external cabinet", "gemini");
        Require(nova.ok, "nova matrix should pass: " + DescribeResult(nova));

        const CommandResult blender = validation.RunBlenderValidationMatrix();
        Require(blender.ok, "blender matrix should pass: " + DescribeResult(blender));

        const CommandResult end_to_end = validation.RunEndToEndValidationMatrix(
            "validate external host bridge",
            "trace_external_host",
            "external cabinet",
            "gemini");
        Require(end_to_end.ok, "external host validation should pass: " + DescribeResult(end_to_end));
        Require(
            end_to_end.fields.at("all_passed") == "true",
            "all external host checks should pass: " + DescribeResult(end_to_end));
        Require(
            end_to_end.fields.at("validation_rows").find("flow.blender_matrix:pass") != std::string::npos,
            "blender validation should pass through external bridge: " + DescribeResult(end_to_end));

        std::cout << "3D external host smoke test passed\n";
        return 0;
    }
    catch (const std::exception & ex) {
        std::cerr << ex.what() << "\n";
        return 1;
    }
}
