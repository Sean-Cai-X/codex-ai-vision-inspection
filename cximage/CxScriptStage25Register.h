#ifndef CXIMAGE_CXSCRIPTSTAGE25REGISTER_H
#define CXIMAGE_CXSCRIPTSTAGE25REGISTER_H

#include "muParser.h"
#include "CxScriptStage25Manifest.h"

void RegisterStage25CxScriptBindings(mu::Parser& parser);

extern Stage25Manifest g_stage25_manifest;
extern Stage25ImageCase* g_current_image;
extern Stage25FindlineProfile* g_current_findline_profile;
extern Stage25FindcircleProfile* g_current_findcircle_profile;
extern Stage25EvidenceProfile* g_current_evidence_profile;

#endif