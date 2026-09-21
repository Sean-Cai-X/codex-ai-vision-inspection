#ifndef CXIMAGE_MANUAL_CONSOLE_SCRIPT_DEBUG_PANEL_H
#define CXIMAGE_MANUAL_CONSOLE_SCRIPT_DEBUG_PANEL_H

#include "ViewController.h"

const char* UiTextOrDash(const std::string& text);

std::string InferCurrentTemplateTool(const ManualTestContext& context);

std::string InferCurrentTemplatePath(const ManualTestContext& context);

int CountSelectedParamCandidates(const ManualTestContext& context);

bool PrepareCurrentFastMatchScriptRun(ManualTestContext& context);

#endif
