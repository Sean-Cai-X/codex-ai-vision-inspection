cmake_minimum_required(VERSION 3.21)

project(codex_lan_agent_3d LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(codex_lan_agent_3d
  src/BlenderExternalToolInvoker.cpp
  src/BlenderSceneAdapter.cpp
  src/BlenderMcpSceneBackend.cpp
  src/CxCloudSceneAdapter.cpp
  src/CxGeomSceneAdapter.cpp
  src/ExternalToolCommandInvoker.cpp
  src/ThreeDHostValidation.cpp
  src/Nova3DAssetAdapter.cpp
  src/Nova3DExternalToolInvoker.cpp
  src/Nova3DMcpHostedBackend.cpp
  src/ThreeDControlSurface.cpp
  src/ThreeDHostedWorkflowCoordinator.cpp
  src/ThreeDMcpAdapter.cpp
  src/ThreeDMcpProtocolBridge.cpp
  src/ThreeDMcpResourceAdapter.cpp
  src/ThreeDParserDispatchBridge.cpp
  src/ThreeDOrchestrator.cpp
  src/ThreeDSceneBridge.cpp
  src/ThreeDSessionStateStore.cpp
)

target_include_directories(codex_lan_agent_3d PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

add_executable(codex_lan_agent_3d_smoke
  tests/ThreeDSystemSmoke.cpp
)

target_link_libraries(codex_lan_agent_3d_smoke PRIVATE
  codex_lan_agent_3d
)

add_executable(codex_lan_agent_3d_bridge_smoke
  tests/ThreeDSceneBridgeSmoke.cpp
)

target_link_libraries(codex_lan_agent_3d_bridge_smoke PRIVATE
  codex_lan_agent_3d
)

add_executable(codex_lan_agent_3d_adapter_smoke
  tests/ThreeDAdapterSmoke.cpp
)

target_link_libraries(codex_lan_agent_3d_adapter_smoke PRIVATE
  codex_lan_agent_3d
)

add_executable(codex_lan_agent_3d_control_smoke
  tests/ThreeDControlSurfaceSmoke.cpp
)

target_link_libraries(codex_lan_agent_3d_control_smoke PRIVATE
  codex_lan_agent_3d
)

add_executable(codex_lan_agent_3d_mcp_smoke
  tests/ThreeDMcpAdapterSmoke.cpp
)

target_link_libraries(codex_lan_agent_3d_mcp_smoke PRIVATE
  codex_lan_agent_3d
)

add_executable(codex_lan_agent_3d_mcp_protocol_smoke
  tests/ThreeDMcpProtocolSmoke.cpp
)

target_link_libraries(codex_lan_agent_3d_mcp_protocol_smoke PRIVATE
  codex_lan_agent_3d
)

add_executable(codex_lan_agent_3d_host_session_smoke
  tests/ThreeDHostSessionSmoke.cpp
)

target_link_libraries(codex_lan_agent_3d_host_session_smoke PRIVATE
  codex_lan_agent_3d
)

add_executable(codex_lan_agent_3d_hosted_bridge_smoke
  tests/ThreeDHostedBridgeSmoke.cpp
)

target_link_libraries(codex_lan_agent_3d_hosted_bridge_smoke PRIVATE
  codex_lan_agent_3d
)

add_executable(codex_lan_agent_3d_hosted_workflow_smoke
  tests/ThreeDHostedWorkflowCoordinatorSmoke.cpp
)

target_link_libraries(codex_lan_agent_3d_hosted_workflow_smoke PRIVATE
  codex_lan_agent_3d
)

add_executable(codex_lan_agent_3d_host_validation_smoke
  tests/ThreeDHostValidationSmoke.cpp
)

target_link_libraries(codex_lan_agent_3d_host_validation_smoke PRIVATE
  codex_lan_agent_3d
)

add_executable(codex_lan_agent_3d_host_validation_failure_smoke
  tests/ThreeDHostValidationFailureSmoke.cpp
)

target_link_libraries(codex_lan_agent_3d_host_validation_failure_smoke PRIVATE
  codex_lan_agent_3d
)

add_executable(codex_lan_agent_3d_tool_stub
  tests/ThreeDToolStub.cpp
)

target_link_libraries(codex_lan_agent_3d_tool_stub PRIVATE
  codex_lan_agent_3d
)

add_executable(codex_lan_agent_3d_external_host_smoke
  tests/ThreeDExternalHostSmoke.cpp
)

target_link_libraries(codex_lan_agent_3d_external_host_smoke PRIVATE
  codex_lan_agent_3d
)

add_executable(codex_lan_agent_3d_external_host_failure_smoke
  tests/ThreeDExternalHostFailureSmoke.cpp
)

target_link_libraries(codex_lan_agent_3d_external_host_failure_smoke PRIVATE
  codex_lan_agent_3d
)

add_executable(codex_lan_agent_3d_live_bridge_smoke
  tests/ThreeDLiveBridgeSmoke.cpp
)

target_link_libraries(codex_lan_agent_3d_live_bridge_smoke PRIVATE
  codex_lan_agent_3d
)

add_executable(codex_lan_agent_3d_live_bridge_failure_smoke
  tests/ThreeDLiveBridgeFailureSmoke.cpp
)

target_link_libraries(codex_lan_agent_3d_live_bridge_failure_smoke PRIVATE
  codex_lan_agent_3d
)

add_executable(codex_lan_agent_3d_blender_live_ops_smoke
  tests/ThreeDBlenderLiveOpsSmoke.cpp
)

target_link_libraries(codex_lan_agent_3d_blender_live_ops_smoke PRIVATE
  codex_lan_agent_3d
)

add_executable(codex_lan_agent_3d_blender_remote_glb_smoke
  tests/ThreeDBlenderRemoteGlbSmoke.cpp
)

target_link_libraries(codex_lan_agent_3d_blender_remote_glb_smoke PRIVATE
  codex_lan_agent_3d
)

add_executable(codex_lan_agent_3d_live_end_to_end_smoke
  tests/ThreeDLiveEndToEndSmoke.cpp
)

target_link_libraries(codex_lan_agent_3d_live_end_to_end_smoke PRIVATE
  codex_lan_agent_3d
)

add_executable(codex_lan_agent_3d_live_validation_runner
  tests/ThreeDLiveValidationRunner.cpp
)

target_link_libraries(codex_lan_agent_3d_live_validation_runner PRIVATE
  codex_lan_agent_3d
)

add_executable(codex_lan_agent_3d_nova_env_file_smoke
  tests/ThreeDNova3DEnvFileSmoke.cpp
)

target_link_libraries(codex_lan_agent_3d_nova_env_file_smoke PRIVATE
  codex_lan_agent_3d
)
