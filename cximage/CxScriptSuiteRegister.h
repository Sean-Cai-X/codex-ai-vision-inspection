#ifndef CXIMAGE_CXSCRIPT_SUITE_REGISTER_H
#define CXIMAGE_CXSCRIPT_SUITE_REGISTER_H

#include "muParser.h"

void RegisterCxScriptSuiteBindings(mu::Parser& parser);

extern CxScriptSuiteRuntime g_cxscript_suite;
extern CxScriptSuiteCase* g_current_suite_case;

#endif