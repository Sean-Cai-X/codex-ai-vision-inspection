#ifndef CXIMAGE_CXSCRIPT_CATALOG_REGISTER_H
#define CXIMAGE_CXSCRIPT_CATALOG_REGISTER_H

#include "muParser.h"

void RegisterCxScriptCatalogBindings(mu::Parser& parser);

extern CxScriptCatalogRuntime g_cxscript_catalog;
extern CxScriptCatalogEntry* g_current_catalog_entry;

#endif