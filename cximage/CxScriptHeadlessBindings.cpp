#include "pch.h"
#include "CxScriptHeadlessBindings.h"

#include "CxAnnotationToolRegister.h"
#include "CxShapeTestRegister.h"

void RegisterCxScriptAnnotationManifestBindings(mu::Parser& parser)
{
    RegisterCxAnnotationToolBindings(parser);
}

void RegisterCxScriptShapeSuiteBindings(mu::Parser& parser)
{
    RegisterCxShapeTestBindings(parser);
}