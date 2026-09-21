#ifndef CXPARSER_EXT_PARSER_META_TYPES_H
#define CXPARSER_EXT_PARSER_META_TYPES_H

#include <string>
#include <vector>

namespace cxparser_ext
{
struct ParserSourceLocation
{
  std::string file;
  int line = 0;
  int column = 0;
};

enum ParserValueKind
{
  pvk_unknown,
  pvk_void,
  pvk_bool,
  pvk_int,
  pvk_double,
  pvk_string,
  pvk_object,
  pvk_pointer,
  pvk_array
};

struct ParserTypeMeta
{
  std::string name;
  std::string qualified_name;
  ParserValueKind value_kind = pvk_unknown;
  bool is_const = false;
  bool is_ref = false;
  bool is_ptr = false;
  bool is_variadic = false;
};

struct ParserParamMeta
{
  std::string name;
  ParserTypeMeta type;
  bool has_default = false;
  std::string default_value_text;
};

struct ParserMethodMeta
{
  std::string name;
  std::string qualified_name;
  std::string owner_class;
  ParserTypeMeta return_type;
  std::vector<ParserParamMeta> params;
  bool is_const = false;
  bool is_static = false;
  bool is_variadic = false;
  std::string binding_id;
  ParserSourceLocation source;
};

struct ParserClassMeta
{
  std::string name;
  std::string qualified_name;
  std::string parser_alias;
  bool is_external = false;
  bool is_script_defined = false;
  std::vector<ParserMethodMeta> methods;
  ParserSourceLocation source;
};
}

#endif
