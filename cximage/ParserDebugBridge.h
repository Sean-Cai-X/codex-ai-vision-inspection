#ifndef CXIMAGE_PARSER_DEBUG_BRIDGE_H
#define CXIMAGE_PARSER_DEBUG_BRIDGE_H

#include "CxParserRuntimeOwner.h"

#include <opencv2/core/mat.hpp>

#include <string>
#include <vector>
#include <map>
#include <cstdint>

class Image;

struct ParserDebugObjectSnapshot
{
  std::string name;
  std::string type;
  int declared_line = 0;
  bool exists_in_parser = false;
  std::string runtime_state = "PENDING";
  std::string last_method;
  int last_update_line = 0;
  std::string value_summary;
  bool visualizable = false;
  std::string visual_source = "stale_runtime";
  bool stale = true;
  bool has_circle = false;
  float circle_cx = 0.0f;
  float circle_cy = 0.0f;
  float circle_inner = 0.0f;
  float circle_radius = 0.0f;
  double ring_outer_radius = 0.0;
  double ring_inner_radius = 0.0;
  double ring_thickness = 0.0;
  double ring_center_distance = 0.0;
  bool ring_concentric_ok = false;
  bool ring_inside_ok = false;
  bool ring_thickness_ok = false;
  double ring_score = 0.0;
  std::string ring_status;
  std::string ring_reason;
  std::string ring_result_ref;

  bool has_fastmatch_diagnostic = false;
  bool fastmatch_allowed = false;
  std::string fastmatch_status;
  std::string fastmatch_reason;
  std::string fastmatch_result_ref;

  bool has_fastmatch = false;
  bool fastmatch_model_available = false;
  int fastmatch_model_point_count = 0;
  int fastmatch_candidate_count = 0;
  int fastmatch_best_index = -1;
  double fastmatch_best_score = 0.0;
  double fastmatch_best_x = 0.0;
  double fastmatch_best_y = 0.0;
  bool fastmatch_has_result_box = false;
  std::string fastmatch_result_status;
};

struct ParserDebugVariableSnapshot
{
  std::string name;
  bool exists_in_parser = false;
  double value = 0.0;
};
struct CxScriptLineView
{
  int line_no = 0;
  std::string source_line;
  std::string statement_type;
  std::string status;
  std::string reason;
};

struct CxScriptStatementView
{
  int statement_id = 0;
  int line_no = 0;
  std::string statement_type;
  std::string lhs_variable;
  std::string lhs_type;
  std::string source_object;
  std::string method_name;
  std::string returned_object_ref;
  std::string status;
  std::string reason;
};

struct CxScriptObjectAssignmentView
{
  std::string lhs_variable;
  std::string lhs_type;
  std::string source_object;
  std::string method_name;
  std::string returned_object_ref;
  std::string source_line;
  int line_no = 0;
  std::string status;
  std::string reason;
};

struct CxScriptSemanticBridgeResult
{
  bool ok = false;
  std::string status;
  std::string reason;
  std::string raw_log;
  std::vector<CxScriptLineView> line_views;
  std::vector<CxScriptStatementView> statement_views;
  std::vector<CxScriptObjectAssignmentView> object_assignments;
};

class ParserDebugBridge
{
public:
  void Bind(CxParserRuntimeOwner* owner) { myOwner = owner; }
  bool CompileScript(const std::string& scriptText);
  bool CompileScriptOnly(const std::string& scriptText);
  bool RunScript(const std::string& scriptText);
  bool RunPrefixToLine(const std::string& scriptText, int lineNo);
  bool QueryObjectExists(const std::string& type, const std::string& name) const;
  void* QueryClassObject(const std::string& type, const std::string& name) const;
  std::vector<std::string> ListClassObjectNames(const std::string& type) const;
  Image* QueryImage(const std::string& name) const;
  bool QueryDouble(const std::string& name, double& value) const;
  bool SetDouble(const std::string& name, double value);
  bool SetGlobalInt(const std::string& name, int value);
  bool SetGlobalDouble(const std::string& name, double value);
  bool SetGlobalString(const std::string& name, const std::string& value);
  void RemoveGlobalString(const std::string& name);
  bool ApplyStatement(const std::string& statement);
  bool StageGlobalMatInput(const cv::Mat& image);
  bool SetGlobalMatInput(const cv::Mat& image);
  void ClearGlobalInputs();
  bool HasGlobalMatInput() const { return !myGlobalMatInput.empty(); }
  int GlobalMatInputWidth() const { return myGlobalMatInput.cols; }
  int GlobalMatInputHeight() const { return myGlobalMatInput.rows; }
  bool EnsureAlgorithmRuntimeResources();
  std::vector<ParserDebugObjectSnapshot> SnapshotRuntimeObjects(
    const std::string& lastMethod, int lastUpdateLine,
    const std::string& runtimeStatus) const;
  std::vector<ParserDebugVariableSnapshot> SnapshotRuntimeVariables() const;
  bool RunCxParserExtDebugInProcess(
    const std::string& scriptPath,
    CxScriptSemanticBridgeResult& outResult);
  const std::string& LastError() const { return myLastError; }
  void Stop();
  void ResetRuntime();

private:
  std::string PrepareScript(const std::string& scriptText) const;
  bool RebindGlobalInputs();
  bool BindStagedGlobalMatInput();

  CxParserRuntimeOwner* myOwner = nullptr;
  cv::Mat myGlobalMatInput;
  std::map<std::string, double> myGlobalNumericInputs;
  std::map<std::string, std::string> myGlobalStringInputs;
  std::string myLastError;
  std::uint64_t myRuntimeResetSerial = 0;
  std::uint64_t myGlobalMatInputBoundSerial = 0;
};

#endif
