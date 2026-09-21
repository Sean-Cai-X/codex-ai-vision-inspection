#ifndef CXIMAGE_CXSCRIPT_GLOBAL_VALUE_SET_H
#define CXIMAGE_CXSCRIPT_GLOBAL_VALUE_SET_H

#include <string>
#include <map>
#include "ParserClass.h"

struct CxScriptGlobalValueSet
{
    std::map<std::string, double> numbers;
    std::map<std::string, std::string> strings;
};

bool LoadHeadlessGlobalDeclarations(
    const std::string& init_script_path,
    CxScriptGlobalValueSet& values,
    std::string& reason);

bool ApplyGlobalOverrides(
    CxScriptGlobalValueSet& values,
    const std::map<std::string, double>& overrides,
    std::string& reason);

std::map<std::string, double> BuildHeadlessGlobalOverrides(
    const struct CxScriptHeadlessOptions& options);

bool BindGlobalValueSetToParser(
    mu::CxParserRuntime& runtime,
    CxScriptGlobalValueSet& values,
    std::string& reason);

bool LoadHeadlessGlobalValuesFile(
    const std::string& values_path,
    std::map<std::string, double>& overrides,
    std::string& reason);

#endif