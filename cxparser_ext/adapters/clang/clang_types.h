#ifndef CXPARSER_EXT_CLANG_TYPES_H
#define CXPARSER_EXT_CLANG_TYPES_H

#include <string>
#include <vector>

#include "../../meta/parser_meta_types.h"

namespace cxparser_ext
{
struct ClangTypeInfo
{
  std::string spelling;
  std::string qualified_name;
  bool is_const = false;
  bool is_ref = false;
  bool is_ptr = false;
  bool is_builtin = false;
};

struct ClangParamInfo
{
  std::string name;
  ClangTypeInfo type;
  bool has_default = false;
  std::string default_expr;
};

struct ClangMethodInfo
{
  std::string name;
  std::string qualified_name;
  ClangTypeInfo return_type;
  std::vector<ClangParamInfo> params;
  bool is_const = false;
  bool is_static = false;
  bool is_public = false;
  ParserSourceLocation source;
};

struct ClangClassInfo
{
  std::string name;
  std::string qualified_name;
  std::string namespace_name;
  std::vector<ClangMethodInfo> methods;
  ParserSourceLocation source;
};

struct ApiSchema
{
  std::string module_name;
  std::vector<ClangClassInfo> classes;
};

struct ClangCompileContext
{
  std::string project_root;
  std::string compile_db_path;
  std::vector<std::string> include_dirs;
  std::vector<std::string> defines;
};
}

#endif
