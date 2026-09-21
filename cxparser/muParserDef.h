

#ifndef MUP_DEF_H
#define MUP_DEF_H

#include <string>
#include <map>
#include <vector>

#include "muParserFixes.h"

#define MEC_VERSION _T("1.0.0")

#define MEC_OPRT_CHARS _T("+-*^/?<>=#!$%&|~'_")

#define MEC_CHARS _T("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ")

#define MUP_BASETYPE double

#define MUP_STRING_TYPE std::string

#define MUP_BYTECODE_TYPE int

#define MUP_VISION_TYPE double
#define MUP_VISIONLP_TYPE int*
#define MUP_VOID_TYPE void

#ifndef _T
  #define _T(x) x
#endif

#if defined(_DEBUG)

  #define MUP_FAIL(MSG)    \
          bool MSG=false;  \
          assert(MSG);

  #define MUP_ASSERT(COND)                              \
          if (!(COND))                                  \
          {                                             \
            stringstream_type ss;                       \
            ss << _T("Assertion \""#COND"\" failed: "); \
            ss << __FILE__ << _T(" line ") << __LINE__; \
            ss << ".";                                  \
            throw ParserError( ss.str() );              \
          }
#else
  #define MUP_FAIL(MSG)
  #define MUP_ASSERT(COND)
#endif

namespace mu
{

enum ECmdCode
{

  cmLE            = 0,
  cmGE            = 1,
  cmNEQ           = 2,
  cmEQ            = 3,
  cmLT            = 4,
  cmGT            = 5,
  cmADD           = 6,
  cmSUB           = 7,
  cmMUL           = 8,
  cmDIV           = 9,
  cmPOW           = 10,
  cmAND           = 11,
  cmOR            = 12,
  cmXOR           = 13,
  cmASSIGN        = 14,
  cmBO            = 15,
  cmBC            = 16,
  cmCOMMA         = 17,
  cmSEMICOLON     = 18,
  cmLB			  = 19,
  cmRB			  = 20,
  cmMember		  = 21,

  cmPointer		  = 22,
  cmIf			  = 23,
  cmElse		  = 24,
  cmWhile         = 25,
  cmReturn        = 26,

  cmVAR           = 27,
  cmSTRVAR        = 28,
  cmVAL           = 29,

  cmFUNC          = 30,
  cmFUNC_STR      = 31,

  cmSTRING        = 32,
  cmOPRT_BIN      = 33,
  cmOPRT_POSTFIX  = 34,
  cmOPRT_INFIX    = 35,
  cmEND           = 36,

  cmClassObjDef   = 37,
  cmVARLP		  = 38,
  cmONEAREAEND    = 39,
  cmClass		  = 40,
  cmClassObj	  = 41,
  cmClassFuc	  = 42,
  cmClassFucNum	  = 43,
  cmClassFucStr	  = 44,

  cmClassFucVp	  = 45,

  cmStorageEnd	  = 46,
  cmCallin		  = 47,
  cmStringVar	  = 48,
  cmStorageBegin  = 49,
  cmUNKNOWN       = 50

};

enum ETypeCode
{
  tpSTR  = 0,

  tpDBL   = 1,
  tpVOID  = 2,
  tpSTRLP = 3,
  tpDBLLP = 4,
  tpVOIDLP= 5,
  tpCLASS = 6

};

enum EParamCodes
{
	Param_none=1,
	Param_voidp_1,
	Param_voidp_2,
	Param_voidp_3,
	Param_voidp_4,
	Param_voidp_5,
	Param_voidp_1_Return_double,
	Param_double_1,
	Param_double_2,
	Param_double_3,
	Param_double_4,
	Param_int_1,
	Param_int_2,
	Param_int_3,
	Param_int_4,
	Param_int_5,
	Param_int_6,
	Param_int_7,
	Param_charp_1,
	Param_0_Return_charp,
	Param_0_Return_int,
	Param_0_Return_double,
	Param_int_1_Return_int,
	Param_int_2_Return_int,
	Param_int_1_Return_double,
	Param_int_2_Return_double,
	Param_int_3_Return_double,
	Param_any,
	Param_any_Return_int,
	Param_any_Return_double,
	Param_double_charp_2,
	Param_double_charp_2_Return_double,
	Param_int_double_2,
	Param_double_int_2,
	Param_int_double_2_Return_int,
	Param_double_int_2_Return_int,
	Param_int_double_2_Return_double,
	Param_double_int_2_Return_double,
	Param_charp_any,
	Param_charp_any_Return_int,
	Param_charp_any_Return_double
};

enum EClassTypeCodes
{
	CLASS_ORG=1,
	CLASS_PARSER,
	CLASS_CREATE,
	CLASS_ANY
};

enum EPrec
{

  prLOGIC   = 1,
  prCMP     = 2,
  prADD_SUB = 3,
  prMUL_DIV = 4,
  prPOW     = 5,

  prINFIX    = 4,
  prPOSTFIX  = 4
};

typedef MUP_BASETYPE value_type;
typedef MUP_STRING_TYPE string_type;
typedef MUP_BYTECODE_TYPE bytecode_type;

typedef MUP_VISION_TYPE vision_type;
typedef MUP_VISIONLP_TYPE vision_lptype;
typedef MUP_VOID_TYPE void_type;

typedef string_type::value_type char_type;
typedef std::basic_stringstream<char_type,
                                std::char_traits<char_type>,
                                std::allocator<char_type> > stringstream_type;

typedef std::map<string_type, value_type*> varmap_type;
typedef std::map<string_type, value_type> valmap_type;
typedef std::map<string_type, std::size_t> strmap_type;

typedef std::map<string_type,string_type*> stringmap_type;

typedef value_type (*fun_type1)(value_type);
typedef value_type (*fun_type2)(value_type, value_type);
typedef value_type (*fun_type3)(value_type, value_type, value_type);
typedef value_type (*fun_type4)(value_type, value_type, value_type, value_type);
typedef value_type (*fun_type5)(value_type, value_type, value_type, value_type, value_type);
typedef value_type (*multfun_type)(const value_type*, int);
typedef value_type (*strfun_type1)(const char *);

typedef value_type* (*fun_lptype)(value_type);

typedef void_type  (*fun_void_type1)(value_type);
typedef void_type  (*fun_void_type2)(value_type, value_type);
typedef void_type  (*fun_void_type3)(value_type, value_type, value_type);
typedef void_type  (*fun_void_type4)(value_type, value_type, value_type, value_type);
typedef void_type  (*fun_void_type5)(value_type, value_type, value_type, value_type, value_type);
typedef void_type  (*multfun_void_type)(const value_type*, int);
typedef void_type  (*strfun_void_type1)(const char *);

typedef vision_type (*visionfun_type)(const vision_type*,vision_type);
typedef vision_type (*visionfun_lptype)(const vision_type*,vision_lptype);

typedef bool (*identfun_type)(const char_type*, int&, value_type&);

typedef value_type* (*facfun_type)(const char_type*,void *);

typedef struct virtualclass
{
	MUP_BYTECODE_TYPE * pclass_inicall;
	int ilen_icall;
	MUP_BYTECODE_TYPE * pclass_funcall;
	int ilen_fcall;
}VIRCLASS;
}

#endif
