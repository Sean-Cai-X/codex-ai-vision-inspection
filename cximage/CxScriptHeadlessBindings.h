#ifndef CX_SCRIPT_HEADLESS_BINDINGS_H
#define CX_SCRIPT_HEADLESS_BINDINGS_H

#include "muParser.h"

void RegisterCxScriptAnnotationManifestBindings(mu::Parser& parser);
void RegisterCxScriptShapeSuiteBindings(mu::Parser& parser);

#endif