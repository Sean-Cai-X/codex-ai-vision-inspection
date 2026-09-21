#ifndef CXIMAGE_CXPARAMETER_PROFILE_REGISTER_H
#define CXIMAGE_CXPARAMETER_PROFILE_REGISTER_H

#include "muParser.h"

void RegisterCxParameterProfileBindings(mu::Parser& parser);

extern CxParameterProfileRuntime g_cxscript_parameter_profile;
extern CxParameterProfile* g_current_parameter_profile;

#endif
