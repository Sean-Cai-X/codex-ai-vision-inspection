#ifndef CXIMAGE_MANUAL_CONSOLE_FINDCIRCLE_DEBUG_H
#define CXIMAGE_MANUAL_CONSOLE_FINDCIRCLE_DEBUG_H

#include <string>
#include "ViewController.h"
#include "FindCircle.h"

void RefreshFindCircleDisplaySnapshot(ManualTestContext& context,
    RuntimeObjectView& object);

void RefreshFindCircleMeasureGeometrySnapshot(
    RuntimeObjectView& object,
    FindCircle& circle);

bool ResolveDebugIntToken(ManualTestContext& context, const std::string& token, int& value);

bool TryExecuteFindCircleSetcircle(ManualTestContext& context,
    int lineIndex,
    const std::string& statement);

bool TryExecuteFindCircleParamMethod(ManualTestContext& context,
    int lineIndex,
    const std::string& statement);

void FillFindCircleResultView(RuntimeObjectView& object,
    FindCircle& circle,
    const std::string& methodName);

bool SaveFindCircleDebugSnapshotJson(const ManualTestContext& context,
    std::string& outPath,
    std::string& outReason);

bool TryExecuteFindCircleRuntimeMethod(ManualTestContext& context,
    int lineIndex,
    const std::string& statement);

bool TryHandleFindCircleGetResult(ManualTestContext& context,
    int lineIndex,
    const std::string& statement);

#endif
