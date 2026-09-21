#ifndef CXPARSER_EXT_CLANG_ADAPTER_H
#define CXPARSER_EXT_CLANG_ADAPTER_H

#include <string>
#include <vector>

#include "../../meta/parser_binding_spec.h"
#include "clang_types.h"

namespace cxparser_ext
{
struct MethodDiff
{
  std::string qualified_name;
  std::string change_kind;
  std::string details;
};

struct RefactorDiff
{
  std::vector<MethodDiff> method_changes;
  std::vector<std::string> removed_classes;
  std::vector<std::string> added_classes;
};

class IClangAdapter
{
public:
  virtual ~IClangAdapter() {}

  virtual bool LoadCompileContext(const ClangCompileContext &ctx) = 0;
  virtual bool ExtractApiSchema(const std::vector<std::string> &headers, ApiSchema &schema) = 0;
  virtual bool BuildBindingSpec(const ApiSchema &schema, ParserBindingSpec &spec) = 0;
  virtual bool BuildRefactorDiff(const ApiSchema &old_schema,
                                 const ApiSchema &new_schema,
                                 RefactorDiff &diff) = 0;
};
}

#endif
