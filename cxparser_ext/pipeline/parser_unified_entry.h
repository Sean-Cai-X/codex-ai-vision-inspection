#ifndef CXPARSER_EXT_PARSER_UNIFIED_ENTRY_H
#define CXPARSER_EXT_PARSER_UNIFIED_ENTRY_H

#include <string>
#include <vector>

#include "../meta/parser_binding_spec.h"
#include "../meta/parser_evidence.h"
#include "../meta/parser_image_analysis_protocol.h"
#include "parser_image_analysis_node.h"
#include "parser_task_coordinator.h"

namespace cxparser_ext
{
struct ParserMainThreadTick
{
  std::string thread_name;
  std::string thread_role;
  int cycle_index = 0;
  int accepted_task_count = 0;
  int executed_task_count = 0;
  int replay_count = 0;
  std::string last_replay_task_id;
  std::string last_replay_source_task_id;
  std::vector<std::string> last_replay_modules;
  std::vector<std::string> last_replay_stages;
  bool success = false;
};

struct ParserReplaySummary
{
  int replay_count = 0;
  std::string replay_task_id;
  std::string replay_source_task_id;
  std::vector<std::string> replay_modules;
  std::vector<std::string> replay_stages;
};

class ParserUnifiedEntry
{
public:
  void SetBindingSpec(const ParserBindingSpec &spec);
  void Reset();
  bool SubmitEnvelope(const CxTaskEnvelope &envelope);
  bool SubmitTask(const ExecutionTarget &target);
  bool SubmitImageAnalysisTask(const ImageAnalysisRequest &request);
  bool ExecuteMainThreadCycle();

  const ParserMainThreadTick &GetLastTick() const;
  ParserReplaySummary GetLastReplaySummary() const;
  const ParserEvidenceBundle *FindTaskEvidence(const std::string &task_id) const;
  const ParserTaskUnit *FindTask(const std::string &task_id) const;
  const ParserTaskOutcome *FindTaskOutcome(const std::string &task_id) const;
  const ImageAnalysisResult *FindImageAnalysisResult(const std::string &task_id) const;
  const std::vector<TaskChainRecord> &GetChainRecords() const;
  bool ReplayTask(const std::string &task_id);

private:
  struct ImageAnalysisTaskRecord
  {
    ImageAnalysisRequest request;
    ImageAnalysisResult result;
    bool executed = false;
    bool success = false;
  };

  void RecordMainThreadTrace(const std::string &task_id);
  bool ExecuteImageAnalysisTask(const std::string &task_id);

  ParserTaskCoordinator coordinator_;
  ParserImageAnalysisNode image_analysis_node_;
  ParserMainThreadTick last_tick_;
  std::vector<std::string> submission_order_;
  std::vector<ImageAnalysisTaskRecord> image_analysis_tasks_;
};
}

#endif
