#ifndef CXIMAGE_CXSCRIPT_EVIDENCE_CHAIN_REGISTER_H
#define CXIMAGE_CXSCRIPT_EVIDENCE_CHAIN_REGISTER_H

#include "muParser.h"

void RegisterCxScriptEvidenceChainBindings(mu::Parser& parser);

extern CxScriptEvidenceChainRuntime g_cxscript_evidence_chain;
extern CxScriptEvidenceCase* g_current_evidence_case;

#endif
