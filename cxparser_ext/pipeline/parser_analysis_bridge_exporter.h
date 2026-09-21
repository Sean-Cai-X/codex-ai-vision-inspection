#ifndef CXPARSER_EXT_PARSER_ANALYSIS_BRIDGE_EXPORTER_H
#define CXPARSER_EXT_PARSER_ANALYSIS_BRIDGE_EXPORTER_H

#include "../meta/parser_evidence.h"
#include "parser_analysis_bridge_types.h"

namespace cxparser_ext
{
class ParserAnalysisBridgeExporter
{
public:
  bool BuildEvidence(const ParserAnalysisBridgeResult &bridge_result,
                     ParserEvidenceBundle &bundle) const;
};
}

#endif
