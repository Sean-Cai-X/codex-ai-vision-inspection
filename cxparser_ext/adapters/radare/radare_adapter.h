#ifndef CXPARSER_EXT_RADARE_ADAPTER_H
#define CXPARSER_EXT_RADARE_ADAPTER_H

#include <string>
#include <vector>

#include "../../meta/parser_evidence.h"
#include "radare_types.h"

namespace cxparser_ext
{
class IRadareAdapter
{
public:
  virtual ~IRadareAdapter() {}

  virtual bool OpenTarget(const RadareTarget &target) = 0;
  virtual bool AnalyzeAll() = 0;
  virtual bool GetFunctions(std::vector<RadareFunctionInfo> &functions) = 0;
  virtual bool GetCallGraph(const std::string &address, RadareCallGraph &graph) = 0;
  virtual bool GetDebugSnapshot(RadareDebugSnapshot &snapshot) = 0;
  virtual bool BuildEvidence(ParserEvidenceBundle &bundle) = 0;
};
}

#endif
