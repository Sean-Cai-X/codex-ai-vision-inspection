#ifndef CXPARSER_EXT_PARSER_TASK_COORDINATOR_H
#define CXPARSER_EXT_PARSER_TASK_COORDINATOR_H

#include <string>
#include <vector>

#include "../meta/parser_binding_spec.h"
#include "parser_task_unit.h"

namespace cxparser_ext
{
class ParserTaskCoordinator
{
public:
  void SetBindingSpec(const ParserBindingSpec &spec);
  void Reset();

  bool AddTaskEnvelope(const CxTaskEnvelope &envelope);
  bool AddTask(const ExecutionTarget &target);
  bool UpdateTaskRoute(const std::string &task_id, const ParserRoutePolicy &route);

  bool PrepareTask(const std::string &task_id);
  bool RunTask(const std::string &task_id);
  bool ValidateTask(const std::string &task_id);

  bool ExecuteTask(const std::string &task_id);
  bool PrepareAll();
  bool RunAll();
  bool ValidateAll();
  bool ExecuteAll();
  void RecordReplayChain(const std::string &task_id,
                         const std::string &execution_mode,
                         const std::string &replay_source_task_id);

  ParserTaskUnit *FindTask(const std::string &task_id);
  const ParserTaskUnit *FindTask(const std::string &task_id) const;
  const ParserTaskOutcome *FindTaskOutcome(const std::string &task_id) const;
  const std::vector<TaskChainRecord> &GetChainRecords() const;

private:
  ParserBindingSpec binding_spec_;
  std::vector<ParserTaskUnit> tasks_;
  std::vector<TaskChainRecord> chain_records_;
};
}

#endif
