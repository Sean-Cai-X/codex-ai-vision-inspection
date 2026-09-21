#ifndef CXIMAGE_CXPARAM_REGRESSION_REGISTER_H
#define CXIMAGE_CXPARAM_REGRESSION_REGISTER_H

#include "muParser.h"
#include "CxParamRegressionRuntime.h"

void RegisterCxParamRegressionBindings(mu::Parser& parser);

extern CxParamRegressionRuntime g_cxscript_param_regression;
extern CxParamRange* g_current_param_range;
extern CxParamCandidate* g_current_param_candidate;

#endif
