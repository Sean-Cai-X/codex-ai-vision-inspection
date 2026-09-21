#ifndef CXPARSER_EXT_CXSCRIPT_RUNTIME_H
#define CXPARSER_EXT_CXSCRIPT_RUNTIME_H

#include "cxscript_c_frontend.h"
#include "cxscript_runtime_types.h"

namespace cxparser_ext
{
bool BuildCxscriptIdentity(const CxscriptExecutionArgs &args,
                           CxscriptIdentity &identity);
void BuildCxscriptContext(const CxscriptExecutionArgs &args,
                          CxscriptExecutionContext &context);
bool LoadCxscriptText(const CxscriptIdentity &identity,
                      const std::string &fallback_text,
                      std::string &script_text,
                      std::string &script_origin);
void NormalizeCxscriptText(const std::string &source_text,
                           std::string &normalized_text);
bool ExtractCxscriptHeaderMetadata(const std::string &script_text,
                                   CxscriptHeaderMetadata &metadata);
void AnalyzeCxscriptFlow(const std::string &script_text,
                         CxscriptFlowProfile &flow_profile);
void AnalyzeCxscriptSemantics(const std::string &script_text,
                              const CxscriptIrProgram &program,
                              CxscriptBasicSemanticProfile &basic_profile,
                              CxscriptBindingSemanticProfile &binding_profile);
void BuildCxscriptLayerProfile(const std::string &script_text,
                               CxscriptLayerProfile &layer_profile,
                               std::string *normalized_text = 0);
void EvaluateCxscriptCompileBridgeSafety(const std::string &script_text,
                                         const CxscriptIrProgram &program,
                                         CxscriptLayerProfile &layer_profile);
void BuildCxscriptStructureSummary(const std::string &script_text,
                                   CxscriptExecutionResult &result);
void BuildCxscriptRuntimeReport(const CxscriptExecutionResult &result,
                                CxscriptRuntimeReport &report);
void RefreshCxscriptRuntimeSlices(CxscriptRuntimeReport &report);
void FormatCxscriptResult(const CxscriptExecutionResult &result,
                          std::vector<std::string> &lines);
bool IsCxscriptCompileBridgeEligible(const CxscriptExecutionResult &summary,
                                     const std::string &script_origin,
                                     bool has_catalog_fallback);
bool ShouldUseCatalogFallback(const CxscriptExecutionResult &summary,
                              const std::string &script_origin,
                              bool has_catalog_fallback);
std::string DescribeCxscriptFallbackReason(const CxscriptExecutionResult &summary,
                                           const std::string &script_origin,
                                           bool has_catalog_fallback);
}

#endif
