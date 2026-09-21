#ifndef CXIMAGE_CXPARSER_SNAPSHOT_TYPES_H
#define CXIMAGE_CXPARSER_SNAPSHOT_TYPES_H

#include "CxAnnotationToolRuntime.h"
#include "CxShapeTestRuntime.h"

#include <string>
#include <vector>

struct CxAnnotationToolManifestSnapshot
{
    std::string source_path;
    std::vector<CxAnnotationToolSpec> tools;
};

struct CxShapeTestSuiteSnapshot
{
    std::string source_path;
    std::vector<CxShapeTestCase> cases;
};

#endif