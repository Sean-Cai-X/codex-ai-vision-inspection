#pragma once

#include "CxTorchRuntimeService.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace cxvision_yolov8n_training {
namespace fs = std::filesystem;

inline std::string ArgValue(int argc, char **argv, const std::string &name) {
  const std::string prefix = name + "=";
  for (int i = 1; i < argc; ++i) {
    if (argv[i] == nullptr)
      continue;
    const std::string arg = argv[i];
    if (arg == name && i + 1 < argc && argv[i + 1] != nullptr)
      return argv[i + 1];
    if (arg.rfind(prefix, 0) == 0)
      return arg.substr(prefix.size());
  }
  return {};
}

inline std::string Escape(const std::string &value) {
  std::string out;
  for (const char ch : value) {
    if (ch == '\\' || ch == '"')
      out += '\\';
    if (ch == '\n') {
      out += "\\n";
      continue;
    }
    if (ch == '\r')
      continue;
    out += ch;
  }
  return out;
}

inline bool ReadText(const fs::path &path, std::string &text) {
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return false;
  text.assign(std::istreambuf_iterator<char>(input),
              std::istreambuf_iterator<char>());
  return !text.empty();
}

inline int RunYoloV8nTrainingLifecycleCli(int argc, char **argv) {
  const fs::path dataset_root = ArgValue(argc, argv, "--dataset-root");
  const fs::path training_plan = ArgValue(argc, argv, "--training-plan");
  const fs::path output_dir = ArgValue(argc, argv, "--out");
  const std::string requested_device =
      ArgValue(argc, argv, "--torch-device").empty()
          ? "cpu"
          : ArgValue(argc, argv, "--torch-device");
  fs::path runtime_dll = ArgValue(argc, argv, "--torch-runtime-dll");
  if (runtime_dll.empty())
    runtime_dll = fs::absolute(argv[0]).parent_path() /
                  "libtorch_module_runtime.dll";

  std::error_code ec;
  if (!fs::is_directory(dataset_root, ec) || ec ||
      !fs::is_regular_file(training_plan, ec) || ec ||
      !fs::is_regular_file(runtime_dll, ec) || ec) {
    std::cout << "conclusion=ASSET_PREFLIGHT_FAIL\n"
              << "reason=dataset root, training plan, or runtime DLL is missing\n";
    return 2;
  }
  if (fs::exists(output_dir, ec)) {
    std::cout << "conclusion=ASSET_PREFLIGHT_FAIL\n"
              << "reason=output directory already exists; use a new RUN_ID\n";
    return 2;
  }
  fs::create_directories(output_dir, ec);
  if (ec) {
    std::cout << "conclusion=ASSET_PREFLIGHT_FAIL\n"
              << "reason=output directory cannot be created\n";
    return 2;
  }

  std::string plan_json;
  if (!ReadText(training_plan, plan_json)) {
    std::cout << "conclusion=ASSET_PREFLIGHT_FAIL\n"
              << "reason=training plan cannot be read\n";
    return 2;
  }

#ifdef _WIN32
  const fs::path runtime_dir = runtime_dll.parent_path();
  const char *old_path = std::getenv("PATH");
  const std::string process_path =
      runtime_dir.string() + (old_path == nullptr ? "" : ";" + std::string(old_path));
  SetEnvironmentVariableA("PATH", process_path.c_str());
#endif

  CxTorchRuntimeConfig config;
  config.runtime_dll_path = runtime_dll.string();
  config.output_root = output_dir.string();
  config.device = requested_device;
  config.log_level = "info";
  CxTorchRuntimeService service;
  std::string reason;
  if (!service.Initialize(config, reason)) {
    std::cout << "conclusion=CXX_YOLOV8N_TRAINING_FAIL\nreason=" << reason
              << "\n";
    return 3;
  }

  CxTorchTaskRequest request;
  request.task = "torch.train.detection.yolov8.lifecycle.v1";
  request.device = requested_device;
  request.dataset_root = dataset_root.string();
  request.manifest_path =
      (dataset_root / "package_manifest.json").string();
  request.case_name = "asset_driven_yolov8n_training_lifecycle";
  request.extra_json = plan_json;
  request.output_dir = output_dir.string();
  CxTorchTaskResponse response;
  const bool ok = service.Execute(request, response, reason);
  service.Shutdown();

  std::ofstream response_file(output_dir / "torch_runtime_task_response.json");
  response_file << "{\n"
                << "  \"schema\": \"cxvision.torch_runtime_task_response.v1\",\n"
                << "  \"task\": \"" << request.task << "\",\n"
                << "  \"ok\": " << (response.ok ? "true" : "false")
                << ",\n"
                << "  \"status\": \"" << Escape(response.status)
                << "\",\n"
                << "  \"error_code\": " << response.error_code << ",\n"
                << "  \"error_message\": \""
                << Escape(response.error_message) << "\",\n"
                << "  \"requested_device\": \"" << requested_device
                << "\",\n"
                << "  \"actual_device\": \""
                << Escape(response.actual_device) << "\",\n"
                << "  \"train_runtime_ms\": " << response.train_runtime_ms
                << ",\n"
                << "  \"result_ref\": \"" << Escape(response.result_ref)
                << "\",\n"
                << "  \"evidence_ref\": \""
                << Escape(response.evidence_ref) << "\",\n"
                << "  \"reason\": \"" << Escape(reason) << "\"\n"
                << "}\n";

  std::cout << "conclusion="
            << (ok ? response.status : "CXX_YOLOV8N_TRAINING_FAIL") << "\n"
            << "runtime_ok=" << (response.ok ? "true" : "false") << "\n"
            << "requested_device=" << requested_device << "\n"
            << "actual_device=" << response.actual_device << "\n"
            << "train_runtime_ms=" << response.train_runtime_ms << "\n"
            << "result_ref=" << response.result_ref << "\n"
            << "reason=" << reason << "\n";
  return ok ? 0 : 3;
}

} // namespace cxvision_yolov8n_training
