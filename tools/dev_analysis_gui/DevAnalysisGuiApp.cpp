#include "DevAnalysisGuiApp.h"

#include "panels/DevAnalysisGuiPanels.h"

#include <glad/glad.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
std::string TrimText(std::string text)
{
  while (!text.empty() && (text.back() == '\r' || text.back() == '\n' || text.back() == ' ' || text.back() == '\t'))
  {
    text.pop_back();
  }
  std::size_t start = 0;
  while (start < text.size() && (text[start] == ' ' || text[start] == '\t' || text[start] == '\r' || text[start] == '\n'))
  {
    ++start;
  }
  return text.substr(start);
}

void AssignBuffer(std::array<char, 768>& buffer, const std::string& value)
{
  std::snprintf(buffer.data(), buffer.size(), "%s", value.c_str());
  buffer.back() = '\0';
}

void AssignBuffer(std::array<char, 1024>& buffer, const std::string& value)
{
  std::snprintf(buffer.data(), buffer.size(), "%s", value.c_str());
  buffer.back() = '\0';
}

std::string ReadTextFile(const std::filesystem::path& path)
{
  std::ifstream input(path, std::ios::binary);
  if (!input)
  {
    return {};
  }
  return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

std::string ExtractJsonValue(const std::string& text, const char* key)
{
  const std::string token = std::string("\"") + key + "\"";
  const std::size_t key_pos = text.find(token);
  if (key_pos == std::string::npos)
  {
    return {};
  }

  std::size_t pos = text.find(':', key_pos + token.size());
  if (pos == std::string::npos)
  {
    return {};
  }
  ++pos;
  while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\r' || text[pos] == '\n'))
  {
    ++pos;
  }
  if (pos >= text.size())
  {
    return {};
  }

  if (text[pos] == '"')
  {
    ++pos;
    std::string value;
    while (pos < text.size())
    {
      const char ch = text[pos++];
      if (ch == '\\' && pos < text.size())
      {
        value += text[pos++];
        continue;
      }
      if (ch == '"')
      {
        break;
      }
      value += ch;
    }
    return value;
  }

  if (text[pos] == '[' || text[pos] == '{')
  {
    const char open_char = text[pos];
    const char close_char = open_char == '[' ? ']' : '}';
    std::size_t start = pos;
    int depth = 0;
    bool in_string = false;
    while (pos < text.size())
    {
      const char ch = text[pos++];
      if (ch == '\\' && in_string)
      {
        if (pos < text.size())
        {
          ++pos;
        }
        continue;
      }
      if (ch == '"')
      {
        in_string = !in_string;
        continue;
      }
      if (in_string)
      {
        continue;
      }
      if (ch == open_char)
      {
        ++depth;
      }
      else if (ch == close_char)
      {
        --depth;
        if (depth == 0)
        {
          return TrimText(text.substr(start, pos - start));
        }
      }
    }
    return TrimText(text.substr(start));
  }

  const std::size_t start = pos;
  while (pos < text.size() && text[pos] != ',' && text[pos] != '\r' && text[pos] != '\n' && text[pos] != '}')
  {
    ++pos;
  }
  return TrimText(text.substr(start, pos - start));
}

bool TryParseRectFromText(const std::string& text,
                          float& x,
                          float& y,
                          float& width,
                          float& height)
{
  static const std::regex rect_pattern(R"(top1_rect\s*[=:]\s*([-+]?\d*\.?\d+),([-+]?\d*\.?\d+),([-+]?\d*\.?\d+),([-+]?\d*\.?\d+))");
  std::smatch match;
  if (!std::regex_search(text, match, rect_pattern) || match.size() != 5)
  {
    return false;
  }

  x = std::stof(match[1].str());
  y = std::stof(match[2].str());
  width = std::stof(match[3].str());
  height = std::stof(match[4].str());
  return width > 0.0f && height > 0.0f;
}

std::string PickActiveVisualPath(const DevAnalysisState& state)
{
  if (state.show_primary_visual &&
      !state.operation_observe.primary_visual_ref.empty() &&
      std::filesystem::exists(std::filesystem::path(state.operation_observe.primary_visual_ref)))
  {
    return state.operation_observe.primary_visual_ref;
  }
  if (!state.operation_observe.input_image_ref.empty() &&
      std::filesystem::exists(std::filesystem::path(state.operation_observe.input_image_ref)))
  {
    return state.operation_observe.input_image_ref;
  }
  return {};
}

void DestroyActiveTexture(DevAnalysisState& state)
{
  if (state.active_visual_texture_id != 0)
  {
    const GLuint texture_id = static_cast<GLuint>(state.active_visual_texture_id);
    glDeleteTextures(1, &texture_id);
    state.active_visual_texture_id = 0;
  }
  state.active_visual_width = 0;
  state.active_visual_height = 0;
  state.active_visual_loaded = false;
  state.active_visual_path.clear();
}

bool LoadActiveTextureFromPath(const std::string& image_path, DevAnalysisState& state)
{
  DestroyActiveTexture(state);
  if (image_path.empty())
  {
    return false;
  }

  const cv::Mat image = cv::imread(image_path, cv::IMREAD_COLOR);
  if (image.empty())
  {
    return false;
  }

  cv::Mat rgba_image;
  cv::cvtColor(image, rgba_image, cv::COLOR_BGR2RGBA);

  GLuint texture_id = 0;
  glGenTextures(1, &texture_id);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, rgba_image.cols, rgba_image.rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba_image.data);
  glBindTexture(GL_TEXTURE_2D, 0);

  state.active_visual_texture_id = static_cast<unsigned int>(texture_id);
  state.active_visual_width = rgba_image.cols;
  state.active_visual_height = rgba_image.rows;
  state.active_visual_loaded = true;
  state.active_visual_path = image_path;
  return true;
}

void RefreshLoadedVisualArtifacts(DevAnalysisState& state,
                                  const std::string& raw_result_text,
                                  const std::string& evidence_ref)
{
  state.detected_rect_valid = false;
  state.detected_rect_x = 0.0f;
  state.detected_rect_y = 0.0f;
  state.detected_rect_width = 0.0f;
  state.detected_rect_height = 0.0f;
  state.detected_rect_label.clear();

  const std::string visual_path = PickActiveVisualPath(state);
  LoadActiveTextureFromPath(visual_path, state);

  std::string rect_source = state.operation_observe.elements_ref + "\n" +
                            state.operation_observe.element_summary_ref + "\n" +
                            state.metrics.key_fields + "\n" +
                            raw_result_text;
  if (!evidence_ref.empty() && std::filesystem::exists(std::filesystem::path(evidence_ref)))
  {
    rect_source += "\n" + ReadTextFile(std::filesystem::path(evidence_ref));
  }

  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
  if (TryParseRectFromText(rect_source, x, y, width, height))
  {
    state.detected_rect_valid = true;
    state.detected_rect_x = x;
    state.detected_rect_y = y;
    state.detected_rect_width = width;
    state.detected_rect_height = height;
    state.detected_rect_label = "top1_rect";
  }
}

std::string EscapeJson(const std::string& text)
{
  std::string escaped;
  escaped.reserve(text.size() + 16);
  for (const char ch : text)
  {
    switch (ch)
    {
    case '\\': escaped += "\\\\"; break;
    case '"': escaped += "\\\""; break;
    case '\n': escaped += "\\n"; break;
    case '\r': escaped += "\\r"; break;
    case '\t': escaped += "\\t"; break;
    default: escaped += ch; break;
    }
  }
  return escaped;
}

bool ApplyChainId(const std::string& chain_id, DevAnalysisState& state)
{
  if (chain_id.empty())
  {
    return false;
  }
  if (chain_id == "cxcore_to_ensmallen")
  {
    state.selected_chain = DevAnalysisChainId::CxcoreToEnsmallen;
    return true;
  }
  if (chain_id == "cxcore_to_mlpack")
  {
    state.selected_chain = DevAnalysisChainId::CxcoreToMlpack;
    return true;
  }
  if (chain_id == "cxcore_to_torch")
  {
    state.selected_chain = DevAnalysisChainId::CxcoreToTorch;
    return true;
  }
  return false;
}

void ApplyCaseName(const std::string& case_name, DevAnalysisState& state)
{
  if (case_name.empty())
  {
    return;
  }
  for (int index = 0; index < static_cast<int>(state.cases.size()); ++index)
  {
    if (state.cases[static_cast<std::size_t>(index)].name == case_name)
    {
      state.selected_case_index = index;
      RefreshDevAnalysisChainData(state);
      return;
    }
  }
  state.operation_select.task_case_ref = case_name;
}

void LoadResultPackageIntoState(const std::filesystem::path& result_path, DevAnalysisState& state)
{
  const std::string text = ReadTextFile(result_path);
  if (text.empty())
  {
    return;
  }

  const std::string evidence_ref = ExtractJsonValue(text, "evidence_ref");
  const std::string result_ref = ExtractJsonValue(text, "result_ref");
  const std::string input_image_ref = ExtractJsonValue(text, "input_image_ref");
  const std::string primary_visual_ref = ExtractJsonValue(text, "primary_visual_ref");
  const std::string visualization_refs = ExtractJsonValue(text, "visualization_refs");
  const std::string gui_visual_refs = ExtractJsonValue(text, "gui_visual_refs");
  const std::string supporting_image_refs = ExtractJsonValue(text, "supporting_image_refs");
  const std::string stage_semantic = ExtractJsonValue(text, "stage_semantic_ref");
  const std::string problem_entry = ExtractJsonValue(text, "problem_entry_ref");
  const std::string elements_ref = ExtractJsonValue(text, "elements");
  const std::string element_summary = ExtractJsonValue(text, "element_summary");
  const std::string element_chains = ExtractJsonValue(text, "element_chains");
  const std::string element_chain_summary = ExtractJsonValue(text, "element_chain_summary");
  const std::string element_status_summary = ExtractJsonValue(text, "element_status_summary");
  const std::string main_chain_ref = ExtractJsonValue(text, "main_chain_ref");
  const std::string stage_execution_entry = ExtractJsonValue(text, "stage_execution_entry");
  const std::string conclusion = ExtractJsonValue(text, "conclusion");
  const std::string issue_focus = ExtractJsonValue(text, "issue_focus");
  const std::string next_action = ExtractJsonValue(text, "next_action");
  const std::string notes = ExtractJsonValue(text, "notes");
  const std::string thread_handoff = ExtractJsonValue(text, "thread_handoff");
  const std::string metrics = ExtractJsonValue(text, "metrics");
  const std::string sample_switch = ExtractJsonValue(text, "sample_switch");
  const std::string statistics_evidence = ExtractJsonValue(text, "statistics_evidence_ref");
  const std::string baseline_result = ExtractJsonValue(text, "baseline_result_ref");

  if (!sample_switch.empty())
  {
    state.operation_select.sample_switch_ref = sample_switch;
  }
  if (!input_image_ref.empty())
  {
    state.operation_select.test_image_ref = input_image_ref;
    state.operation_observe.input_image_ref = input_image_ref;
  }
  if (!primary_visual_ref.empty())
  {
    state.operation_observe.primary_visual_ref = primary_visual_ref;
  }
  if (!supporting_image_refs.empty())
  {
    state.operation_observe.supporting_image_refs_ref = supporting_image_refs;
  }
  if (!visualization_refs.empty())
  {
    state.operation_observe.visualization_refs = visualization_refs;
  }
  if (!gui_visual_refs.empty())
  {
    state.operation_observe.gui_visual_refs = gui_visual_refs;
  }
  if (!statistics_evidence.empty())
  {
    state.operation_observe.statistics_evidence_ref = statistics_evidence;
  }
  if (!baseline_result.empty())
  {
    state.operation_observe.baseline_result_ref = baseline_result;
  }
  if (!stage_semantic.empty())
  {
    state.operation_observe.stage_semantic_ref = stage_semantic;
  }
  if (!problem_entry.empty())
  {
    state.operation_observe.problem_entry_ref = problem_entry;
    state.operation_record.issue_entry_ref = problem_entry;
    AssignBuffer(state.issue_entry_buffer, problem_entry);
  }
  if (!evidence_ref.empty())
  {
    state.operation_observe.evidence_ref = evidence_ref;
  }
  if (!result_ref.empty())
  {
    state.operation_observe.result_ref = result_ref;
  }
  if (!elements_ref.empty())
  {
    state.operation_observe.elements_ref = elements_ref;
  }
  if (!element_summary.empty())
  {
    state.operation_observe.element_summary_ref = element_summary;
  }
  if (!element_chains.empty())
  {
    state.operation_observe.element_chains_ref = element_chains;
  }
  if (!element_chain_summary.empty())
  {
    state.operation_observe.element_chain_summary_ref = element_chain_summary;
  }
  if (!element_status_summary.empty())
  {
    state.operation_observe.element_status_summary_ref = element_status_summary;
  }
  if (!main_chain_ref.empty())
  {
    state.operation_observe.main_chain_ref = main_chain_ref;
  }
  if (!stage_execution_entry.empty())
  {
    state.operation_observe.stage_execution_entry_ref = stage_execution_entry;
    state.operation_run.segment_run_ref = stage_execution_entry;
  }
  if (!conclusion.empty())
  {
    state.operation_observe.conclusion_ref = conclusion;
  }
  if (!issue_focus.empty())
  {
    state.operation_observe.anomaly_ref = issue_focus;
  }
  if (!next_action.empty())
  {
    state.operation_record.next_step_ref = next_action;
    AssignBuffer(state.next_step_buffer, next_action);
  }
  if (!notes.empty())
  {
    state.operation_observe.notes_ref = notes;
  }
  if (!thread_handoff.empty())
  {
    state.operation_observe.thread_handoff_ref = thread_handoff;
  }
  if (!metrics.empty())
  {
    state.metrics.key_fields = metrics;
  }

  state.operation_record.runtime_fillback_status_ref = "loaded_result_package";
  RefreshLoadedVisualArtifacts(state, text, evidence_ref);
}

void WriteStateResultJson(const std::filesystem::path& output_path, const DevAnalysisState& state)
{
  std::ofstream output(output_path, std::ios::binary);
  output << "{\n";
  output << "  \"result_ref\": \"" << EscapeJson(state.operation_observe.result_ref) << "\",\n";
  output << "  \"evidence_ref\": \"" << EscapeJson(state.operation_observe.evidence_ref) << "\",\n";
  output << "  \"input_image_ref\": \"" << EscapeJson(state.operation_observe.input_image_ref) << "\",\n";
  output << "  \"primary_visual_ref\": \"" << EscapeJson(state.operation_observe.primary_visual_ref) << "\",\n";
  output << "  \"visualization_refs\": \"" << EscapeJson(state.operation_observe.visualization_refs) << "\",\n";
  output << "  \"gui_visual_refs\": \"" << EscapeJson(state.operation_observe.gui_visual_refs) << "\",\n";
  output << "  \"elements\": \"" << EscapeJson(state.operation_observe.elements_ref) << "\",\n";
  output << "  \"element_summary\": \"" << EscapeJson(state.operation_observe.element_summary_ref) << "\",\n";
  output << "  \"element_chains\": \"" << EscapeJson(state.operation_observe.element_chains_ref) << "\",\n";
  output << "  \"element_chain_summary\": \"" << EscapeJson(state.operation_observe.element_chain_summary_ref) << "\",\n";
  output << "  \"element_status_summary\": \"" << EscapeJson(state.operation_observe.element_status_summary_ref) << "\",\n";
  output << "  \"main_chain_ref\": \"" << EscapeJson(state.operation_observe.main_chain_ref) << "\",\n";
  output << "  \"stage_execution_entry\": \"" << EscapeJson(state.operation_observe.stage_execution_entry_ref) << "\",\n";
  output << "  \"conclusion\": \"" << EscapeJson(state.operation_observe.conclusion_ref) << "\",\n";
  output << "  \"issue_focus\": \"" << EscapeJson(state.operation_observe.anomaly_ref) << "\",\n";
  output << "  \"next_action\": \"" << EscapeJson(state.operation_record.next_step_ref) << "\",\n";
  output << "  \"notes\": \"" << EscapeJson(state.operation_observe.notes_ref) << "\",\n";
  output << "  \"thread_handoff\": \"" << EscapeJson(state.operation_observe.thread_handoff_ref) << "\"\n";
  output << "}\n";
}
}
DevAnalysisGuiApp::DevAnalysisGuiApp(const DevAnalysisGuiLaunchOptions& options)
  : launch_options_(options)
{
  InitializeDevAnalysisState(state_);
}

DevAnalysisGuiApp::~DevAnalysisGuiApp()
{
  Shutdown();
}

int DevAnalysisGuiApp::Run()
{
  InitializeWindow();
  InitializeImGui();
  initialized_ = true;
  ApplyLaunchOptions();

  if (!launch_options_.capture_dir.empty())
  {
    return RunAutomationCapture();
  }

  MainLoop();
  return 0;
}

void DevAnalysisGuiApp::InitializeWindow()
{
  if (glfwInit() == GLFW_FALSE)
  {
    throw std::runtime_error("glfwInit() failed for dev_analysis_gui");
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  window_ = glfwCreateWindow(1600, 960, "dev_analysis_gui", nullptr, nullptr);
  if (window_ == nullptr)
  {
    glfwTerminate();
    throw std::runtime_error("Failed to create dev_analysis_gui window");
  }

  glfwMakeContextCurrent(window_);
  glfwSwapInterval(1);

  if (gladLoadGL() == 0)
  {
    throw std::runtime_error("gladLoadGL() failed for dev_analysis_gui");
  }
}

void DevAnalysisGuiApp::InitializeImGui()
{
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding = 8.0f;
  style.FrameRounding = 6.0f;
  style.GrabRounding = 6.0f;
  style.Colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.09f, 0.10f, 1.0f);
  style.Colors[ImGuiCol_TitleBg] = ImVec4(0.12f, 0.13f, 0.14f, 1.0f);
  style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.17f, 0.18f, 0.19f, 1.0f);
  style.Colors[ImGuiCol_Button] = ImVec4(0.23f, 0.31f, 0.39f, 1.0f);
  style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.29f, 0.38f, 0.48f, 1.0f);
  style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.34f, 0.43f, 0.54f, 1.0f);

  ImGui_ImplGlfw_InitForOpenGL(window_, true);
  ImGui_ImplOpenGL3_Init("#version 330");
}

void DevAnalysisGuiApp::ApplyLaunchOptions()
{
  if (ApplyChainId(launch_options_.chain_id, state_))
  {
    RefreshDevAnalysisChainData(state_);
  }

  if (!launch_options_.case_name.empty())
  {
    ApplyCaseName(launch_options_.case_name, state_);
  }

  if (!launch_options_.load_result_path.empty())
  {
    LoadResultPackageIntoState(std::filesystem::path(launch_options_.load_result_path), state_);
  }
}

void DevAnalysisGuiApp::MainLoop()
{
  while (!glfwWindowShouldClose(window_))
  {
    glfwPollEvents();
    RenderFrame();
  }
}

void DevAnalysisGuiApp::RenderFrame()
{
  int display_width = 0;
  int display_height = 0;
  glfwGetFramebufferSize(window_, &display_width, &display_height);
  glViewport(0, 0, display_width, display_height);
  glClearColor(0.05f, 0.06f, 0.07f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(1580.0f, 120.0f), ImGuiCond_Always);
  DrawDevAnalysisTopFlow(state_);

  ImGui::SetNextWindowPos(ImVec2(10.0f, 140.0f), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(320.0f, 620.0f), ImGuiCond_Always);
  DrawDevAnalysisControlPanel(state_);

  ImGui::SetNextWindowPos(ImVec2(340.0f, 140.0f), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(1250.0f, 620.0f), ImGuiCond_Always);
  DrawDevAnalysisWorkspaceTabs(state_);

  ImGui::SetNextWindowPos(ImVec2(10.0f, 770.0f), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(500.0f, 180.0f), ImGuiCond_Always);
  DrawDevAnalysisActionPanel(state_);

  ImGui::SetNextWindowPos(ImVec2(520.0f, 770.0f), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(520.0f, 180.0f), ImGuiCond_Always);
  DrawDevAnalysisResultPanel(state_);

  ImGui::SetNextWindowPos(ImVec2(1050.0f, 770.0f), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(540.0f, 180.0f), ImGuiCond_Always);
  DrawDevAnalysisAttachmentPanel(state_);

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  glfwSwapBuffers(window_);
}

int DevAnalysisGuiApp::RunAutomationCapture()
{
  const std::filesystem::path capture_dir = launch_options_.capture_dir;
  std::filesystem::create_directories(capture_dir);
  const std::filesystem::path capture_result_path = capture_dir / "result.json";

  struct CaptureStep
  {
    const char* tab_name;
    const char* file_name;
  };

  const std::array<CaptureStep, 4> steps = {{
    {"Select What", "gui_first_operation.ppm"},
    {"Observe What", "gui_display.ppm"},
    {"Adjust What", "gui_image_operation.ppm"},
    {"Run What", "gui_operation_flow.ppm"}
  }};

  for (const CaptureStep& step : steps)
  {
    glfwPollEvents();
    state_.forced_workspace_tab = step.tab_name;
    RenderFrame();
    if (!CaptureFramebufferToPpm((capture_dir / step.file_name).string()))
    {
      return 1;
    }
  }

  state_.forced_workspace_tab.clear();

  if (!launch_options_.load_result_path.empty() &&
      std::filesystem::exists(std::filesystem::path(launch_options_.load_result_path)))
  {
    const std::filesystem::path source_result_path = std::filesystem::path(launch_options_.load_result_path);
    std::error_code source_ec;
    std::error_code target_ec;
    const std::filesystem::path normalized_source = std::filesystem::weakly_canonical(source_result_path, source_ec);
    const std::filesystem::path normalized_target = std::filesystem::weakly_canonical(capture_result_path, target_ec);

    const bool same_existing_file =
      !source_ec && !target_ec &&
      std::filesystem::exists(source_result_path) &&
      std::filesystem::exists(capture_result_path) &&
      normalized_source == normalized_target;

    if (!same_existing_file)
    {
      std::filesystem::copy_file(source_result_path,
                                 capture_result_path,
                                 std::filesystem::copy_options::overwrite_existing);
    }
  }
  else
  {
    WriteStateResultJson(capture_result_path, state_);
  }

  return 0;
}

bool DevAnalysisGuiApp::CaptureFramebufferToPpm(const std::string& output_path) const
{
  int width = 0;
  int height = 0;
  glfwGetFramebufferSize(window_, &width, &height);
  if (width <= 0 || height <= 0)
  {
    return false;
  }

  std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3u);
  glReadBuffer(GL_FRONT);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

  std::ofstream output(output_path, std::ios::binary);
  if (!output)
  {
    return false;
  }

  output << "P6\n" << width << ' ' << height << "\n255\n";
  const std::size_t row_bytes = static_cast<std::size_t>(width) * 3u;
  for (int row = height - 1; row >= 0; --row)
  {
    const std::size_t offset = static_cast<std::size_t>(row) * row_bytes;
    output.write(reinterpret_cast<const char*>(pixels.data() + offset), static_cast<std::streamsize>(row_bytes));
  }
  return true;
}

void DevAnalysisGuiApp::Shutdown()
{
  if (!initialized_)
  {
    return;
  }

  DestroyActiveTexture(state_);
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  if (window_ != nullptr)
  {
    glfwDestroyWindow(window_);
    window_ = nullptr;
  }

  glfwTerminate();
  initialized_ = false;
}
