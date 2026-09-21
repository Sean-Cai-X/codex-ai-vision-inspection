

#ifndef MU_PARSER_DLL_H
#define MU_PARSER_DLL_H

#if defined(_WIN32x)
    #ifdef MUPARSERLIB_EXPORTS
    #define MU_PARSER_API __declspec(dllexport)
    #else
    #define MU_PARSER_API __declspec(dllimport)
    #endif

    #define MU_LIB_CALL __cdecl

    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else

#endif

typedef void* parser_handle;
typedef double (*fun_type1)(double);
typedef double (*fun_type2)(double, double);
typedef double (*fun_type3)(double, double, double);
typedef double (*fun_type4)(double, double, double, double);
typedef double (*fun_type5)(double, double, double, double, double);
typedef double (*multfun_type)(const double*, int);
typedef double (*strfun_type1)(const char*);
typedef void (*errhandler_type)();
typedef double* (*facfun_type)(const char*,void *);
typedef bool (*identfun_type)(const char*, int&, double&);

#if defined(_WIN32x)
extern "C"

{
#endif

MU_PARSER_API parser_handle MU_LIB_CALL  mupInit();
MU_PARSER_API void MU_LIB_CALL  mupRelease(parser_handle a_hParser);
MU_PARSER_API const char* MU_LIB_CALL  mupGetExpr(parser_handle a_hParser);
MU_PARSER_API void MU_LIB_CALL  mupSetExpr(parser_handle a_hParser, const char *a_szExpr);
MU_PARSER_API void MU_LIB_CALL  mupSetErrorHandler(errhandler_type a_pErrHandler);
MU_PARSER_API void MU_LIB_CALL  mupSetVarFactory(parser_handle a_hParser, facfun_type a_pFactory);

MU_PARSER_API double mupEval(parser_handle a_hParser);

MU_PARSER_API void MU_LIB_CALL  mupDefineFun1(parser_handle a_hParser, const char *a_szName, fun_type1 a_pFun, bool a_bAllowOpt = true);
MU_PARSER_API void MU_LIB_CALL  mupDefineFun2(parser_handle a_hParser, const char *a_szName, fun_type2 a_pFun, bool a_bAllowOpt = true);
MU_PARSER_API void MU_LIB_CALL  mupDefineFun3(parser_handle a_hParser, const char *a_szName, fun_type3 a_pFun, bool a_bAllowOpt = true);
MU_PARSER_API void MU_LIB_CALL  mupDefineFun4(parser_handle a_hParser, const char *a_szName, fun_type4 a_pFun, bool a_bAllowOpt = true);
MU_PARSER_API void MU_LIB_CALL  mupDefineFun5(parser_handle a_hParser, const char *a_szName, fun_type5 a_pFun, bool a_bAllowOpt = true);
MU_PARSER_API void MU_LIB_CALL  mupDefineStrFun(parser_handle a_hParser, const char *a_szName, strfun_type1 a_pFun, bool a_bAllowOpt = true);
MU_PARSER_API void MU_LIB_CALL  mupDefineMultFun(parser_handle a_hParser, const char *a_szName, multfun_type a_pFun, bool a_bAllowOpt = true);
MU_PARSER_API void MU_LIB_CALL  mupDefineOprt(parser_handle a_hParser, const char *a_szName, fun_type2 a_pFun, int a_iPri = 0, bool a_bAllowOpt = true);
MU_PARSER_API void MU_LIB_CALL  mupDefineConst(parser_handle a_hParser, const char *a_szName, double a_fVal);
MU_PARSER_API void MU_LIB_CALL  mupDefineStrConst(parser_handle a_hParser, const char *a_szName, const char *a_sVal);
MU_PARSER_API void MU_LIB_CALL  mupDefineVar(parser_handle a_hParser, const char *a_szName, double *a_fVar);
MU_PARSER_API void MU_LIB_CALL  mupDefinePostfixOprt(parser_handle a_hParser, const char *a_szName, fun_type1 a_pOprt, bool a_bAllowOpt = true);
MU_PARSER_API void MU_LIB_CALL  mupDefineInfixOprt(parser_handle a_hParser, const char *a_szName, fun_type1 a_pOprt, bool a_bAllowOpt=true);

MU_PARSER_API void MU_LIB_CALL  mupDefineNameChars(parser_handle a_hParser, const char *a_szCharset);
MU_PARSER_API void MU_LIB_CALL  mupDefineOprtChars(parser_handle a_hParser, const char *a_szCharset);
MU_PARSER_API void MU_LIB_CALL  mupDefineInfixOprtChars(parser_handle a_hParser, const char *a_szCharset);

MU_PARSER_API void MU_LIB_CALL  mupRemoveVar(parser_handle a_hParser, const char *a_szName);
MU_PARSER_API void MU_LIB_CALL  mupClearVar(parser_handle a_hParser);
MU_PARSER_API void MU_LIB_CALL  mupClearConst(parser_handle a_hParser);
MU_PARSER_API void MU_LIB_CALL  mupClearOprt(parser_handle a_hParser);

MU_PARSER_API int MU_LIB_CALL  mupGetExprVarNum(parser_handle a_hParser);
MU_PARSER_API int MU_LIB_CALL  mupGetVarNum(parser_handle a_hParser);
MU_PARSER_API int MU_LIB_CALL  mupGetConstNum(parser_handle a_hParser);
MU_PARSER_API void MU_LIB_CALL  mupGetExprVar(parser_handle a_hParser, unsigned a_iVar, const char **a_pszName, double **a_pVar);
MU_PARSER_API void MU_LIB_CALL  mupGetVar(parser_handle a_hParser, unsigned a_iVar, const char **a_pszName, double **a_pVar);
MU_PARSER_API void MU_LIB_CALL  mupGetConst(parser_handle a_hParser, unsigned a_iVar, const char **a_pszName, double &a_pVar);

MU_PARSER_API void MU_LIB_CALL  mupAddValIdent(parser_handle a_hParser, identfun_type);

MU_PARSER_API bool MU_LIB_CALL  mupError();
MU_PARSER_API void MU_LIB_CALL  mupErrorReset();
MU_PARSER_API const char* MU_LIB_CALL  mupGetErrorMsg();
MU_PARSER_API int MU_LIB_CALL  mupGetErrorCode();
MU_PARSER_API int MU_LIB_CALL  mupGetErrorPos();
MU_PARSER_API const char* MU_LIB_CALL  mupGetErrorToken();

#if defined(_WIN32x)
}
#endif

#endif
