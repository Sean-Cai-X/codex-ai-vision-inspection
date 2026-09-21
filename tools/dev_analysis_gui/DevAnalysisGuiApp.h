#pragma once

#include "DevAnalysisGuiState.h"

#include <string>

struct GLFWwindow;

struct DevAnalysisGuiLaunchOptions
{
  std::string chain_id;
  std::string case_name;
  std::string load_result_path;
  std::string capture_dir;
};

class DevAnalysisGuiApp
{
public:
  explicit DevAnalysisGuiApp(const DevAnalysisGuiLaunchOptions& options = {});
  ~DevAnalysisGuiApp();

  int Run();

private:
  void InitializeWindow();
  void InitializeImGui();
  void ApplyLaunchOptions();
  void MainLoop();
  void RenderFrame();
  int RunAutomationCapture();
  bool CaptureFramebufferToPpm(const std::string& output_path) const;
  void Shutdown();

  GLFWwindow* window_ = nullptr;
  DevAnalysisState state_;
  DevAnalysisGuiLaunchOptions launch_options_;
  bool initialized_ = false;
};