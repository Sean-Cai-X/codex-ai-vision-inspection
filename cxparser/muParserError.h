

#ifndef MU_PARSER_ERROR_H
#define MU_PARSER_ERROR_H

#include <cassert>
#include <stdexcept>
#include <string>
#include <sstream>
#include <vector>
#include <memory>

#include "muParserDef.h"

#if defined(_WIN32x)
#ifdef _EX
#define   __declspec(dllexport)
#else
#define   __declspec(dllimport)
#endif
#endif

namespace mu
{

    
    enum  EErrorCodes
	{

	  ecUNEXPECTED_OPERATOR = 0,
	  ecUNASSIGNABLE_TOKEN  = 1,
	  ecUNEXPECTED_EOF      = 2,
	  ecUNEXPECTED_COMMA    = 3,
	  ecUNEXPECTED_ARG      = 4,
	  ecUNEXPECTED_VAL      = 5,
	  ecUNEXPECTED_VAR      = 6,
	  ecUNEXPECTED_PARENS   = 7,
	  ecUNEXPECTED_STR      = 8,
	  ecSTRING_EXPECTED     = 9,
	  ecVAL_EXPECTED        = 10,
	  ecMISSING_PARENS      = 11,
	  ecUNEXPECTED_FUN      = 12,
	  ecUNTERMINATED_STRING = 13,
	  ecTOO_MANY_PARAMS     = 14,
	  ecTOO_FEW_PARAMS      = 15,
	  ecOPRT_TYPE_CONFLICT  = 16,
	  ecSTR_RESULT          = 17,

	  ecINVALID_NAME        = 18,
	  ecBUILTIN_OVERLOAD    = 19,
	  ecINVALID_FUN_PTR     = 20,
	  ecINVALID_VAR_PTR     = 21,

	  ecNAME_CONFLICT       = 22,
	  ecOPT_PRI             = 23,

	  ecDOMAIN_ERROR        = 24,
	  ecDIV_BY_ZERO         = 25,
	  ecGENERIC             = 26,
	  ecMISSING_BIGBRACKET	= 27,
	  ecCLASS_CONFICT       = 28,
	  ecCLASSOBJ_CONFICT    = 29,
	  ecCLASSFUC_CONFICT    = 30,
	  ecCLASSMEMOPT_CONFICT = 31,
	  ecCLASSPOINT_CONFICT = 32,

	  ecINTERNAL_ERROR      = 33,

	  ecCOUNT,
	  ecUNDEFINED           = -1
	};

    
    class  ParserErrorMsg
	{
	public:
		typedef ParserErrorMsg self_type;

		ParserErrorMsg& operator=(const ParserErrorMsg &);
		ParserErrorMsg(const ParserErrorMsg&);
		ParserErrorMsg();

	   ~ParserErrorMsg();

		static const ParserErrorMsg& Instance();
		string_type operator[](unsigned a_iIdx) const;

	private:
		std::vector<string_type>  m_vErrMsg;
		static const self_type m_Instance;
	};

    
    class  ParserError
	{
	private:

		void ReplaceSubString( string_type &strSource,
							   const string_type &strFind,
							   const string_type &strReplaceWith);
		void Reset();

	public:
		ParserError();
		explicit ParserError(EErrorCodes a_iErrc);
		explicit ParserError(const string_type &sMsg);
		ParserError( EErrorCodes a_iErrc,
					 const string_type &sTok,
					 const string_type &sFormula = string_type("(formula is not available)"),
					 int a_iPos = -1);
		ParserError( EErrorCodes a_iErrc,
					 int a_iPos,
					 const string_type &sTok);
		ParserError( const char_type *a_szMsg,
					 int a_iPos = -1,
					 const string_type &sTok = string_type());
		ParserError(const ParserError &a_Obj);
		ParserError& operator=(const ParserError &a_Obj);
	   ~ParserError();

		void SetFormula(const string_type &a_strFormula);
		const string_type& GetExpr() const;
		const string_type& GetMsg() const;
		std::size_t GetPos() const;
		const string_type& GetToken() const;
		EErrorCodes GetCode() const;

	private:
		string_type m_strMsg;
		string_type m_strFormula;
		string_type m_strTok;
		int m_iPos;
		EErrorCodes m_iErrc;
		const ParserErrorMsg &m_ErrMsg;
	};
}

#endif
