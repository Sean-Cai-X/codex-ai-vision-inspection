#ifndef CXIMAGE_MEASUREMENT_SEMANTICS_CXMEASUREMENTSEMANTICEVIDENCEWRITER_H
#define CXIMAGE_MEASUREMENT_SEMANTICS_CXMEASUREMENTSEMANTICEVIDENCEWRITER_H

#include "measurement_semantics/CxMeasurementSemanticTypes.h"

#include <filesystem>
#include <string>

struct CxScriptExecutionCapture;
struct CxScriptHeadlessOptions;

bool WriteMeasurementSemanticSidecars(
    const CxScriptExecutionCapture& capture,
    const CxScriptHeadlessOptions& options,
    const std::filesystem::path& output_dir,
    std::string& out_reason);

#endif
