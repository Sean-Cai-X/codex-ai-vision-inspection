#ifndef CXIMAGE_MANUAL_CONSOLE_FINDLINE_DEBUG_H
#define CXIMAGE_MANUAL_CONSOLE_FINDLINE_DEBUG_H

#include <string>
#include "ViewController.h"

const char* FindLineModeName(int mode);

void AppendPointsShapeToXY(PointsShape& points, std::vector<float>& outXY);

void RefreshFindLineDisplaySnapshot(ManualTestContext& context,
                                   RuntimeObjectView& object,
                                   FindLine& lineTool);

std::string BuildFindLineMeasureHint(const RuntimeObjectView& object);

void RefreshFindLineMeasureSnapshot(RuntimeObjectView& object,
    FindLine& lineTool);

bool ApplyRuntimeFindLineWHgap(
    ManualTestContext& context,
    const std::string& objectName,
    int wgap,
    int hgap,
    int updateLineNo,
    const char* updateSource,
    std::string& outReason);

bool ResolveDebugIntValue(ManualTestContext& context, const std::string& token, int& value);

bool TryExecuteFindLineSetline(ManualTestContext& context,
    int lineIndex,
    const std::string& statement);

bool TryExecuteFindLineParamMethod(ManualTestContext& context,
    int lineIndex,
    const std::string& statement);

bool TryExecuteFindLineRuntimeMethod(ManualTestContext& context, int lineIndex, const std::string& statement);

bool TryExecutePendingRuntimeMethod(ManualTestContext& context,
    int lineIndex,
    const std::string& statement);

bool TryExecuteGetResultBinding(ManualTestContext& context,
    int lineIndex,
    const std::string& statement);

bool TryExecuteImageCopyFromMat(ManualTestContext& context,
    int lineIndex,
    const std::string& statement);

#endif
