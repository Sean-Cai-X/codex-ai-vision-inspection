

#ifndef MU_PARSER_CLASSFUNCTION_READER_H
#define MU_PARSER_CLASSFUNCTION_READER_H

#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <stack>
#include <string>

#include "muParserDef.h"
#include "muParserToken.h"
#include "muParserClass.h"

namespace mu
{

  class ParserBase;

  
  class   ParserClassFunctionReader
  {
  private:
      typedef ParserToken<value_type, string_type> token_type;

  private:
      ParserBase *m_pParser;
      string_type m_strFormula;
      int  m_iPos;
      int  m_iSynFlags;
      bool m_bIgnoreUndefVar;

      const funmap_type *m_pFunDef;
      const funmap_type *m_pPostOprtDef;
      const funmap_type *m_pInfixOprtDef;
      const funmap_type *m_pOprtDef;
      const valmap_type *m_pConstDef;
      const strmap_type *m_pStrVarDef;

      varmap_type *m_pVarDef;
      facfun_type m_pFactory;

      std::vector<identfun_type> m_vIdentFun;
      varmap_type m_UsedVar;
      value_type m_fZero;
      int m_iBrackets;
	  int m_iBigBrackets;

	  void *m_pClass;
	  classbasemap_type *m_pClassDef;
      bool m_bClassDef;
	  classbase *m_pClassBase;
	  void *m_pClassObj;
	  int  m_iPreFlags;

	  stringmap_type *m_pStringVarDef;
      string_type m_strcreateclass;
      string_type m_strcreateobj;
  private:

	  enum EPreCodes
	  {
		  isBO      = 1 << 0,
		  isBC      = 1 << 1,
		  isVAL     = 1 << 2,
		  isVAR     = 1 << 3,
		  isCOMMA   = 1 << 4,
		  isFUN     = 1 << 5,
		  isOPT     = 1 << 6,
		  isPOSTOP  = 1 << 7,
		  isINFIXOP = 1 << 8,
		  isEND     = 1 << 9,
		  isSTR     = 1 << 10,
		  isASSIGN  = 1 << 11,
		  isLB      = 1 << 12,
		  isRB	    = 1 << 13,
		  isSEMIC   = 1 << 14,
		  isClassDef= 1 << 15,
		  isClassObjDef= 1 << 16,
		  isClassPointObjDef=1<<17,
		  isClassMemOp=1 << 18,
		  isClassPointOp=1<<19,
		  isClassMemVar=1<<20,
		  isClassMemFuc=1<<21,
		  isClassObj=1<<22,
		  isIf		=1<<23,
		  isElse	=1<<24,
		  isWhile	=1<<25,

		  isANY     = ~0
	  };

  private:

      enum ESynCodes
      {
        noBO      = 1 << 0,
        noBC      = 1 << 1,
        noVAL     = 1 << 2,
        noVAR     = 1 << 3,
        noCOMMA   = 1 << 4,
        noFUN     = 1 << 5,
        noOPT     = 1 << 6,
        noPOSTOP  = 1 << 7,
	    noINFIXOP = 1 << 8,
        noEND     = 1 << 9,
        noSTR     = 1 << 10,
        noASSIGN  = 1 << 11,
		noLB      = 1 << 12,
		noRB	  = 1 << 13,
		noSEMIC   = 1 << 14,
		noClassDef= 1 << 15,
		noClassObjDef= 1 << 16,
		noClassMemOp=1 << 17,
		noClassPointOp=1<<18,
		noClassMemVar=1<<19,
		noClassMemFuc=1<<20,
		noClassObj=1<<21,
        noANY     = ~0
      };

      ParserClassFunctionReader(const ParserClassFunctionReader &a_Reader);
      ParserClassFunctionReader& operator=(const ParserClassFunctionReader &a_Reader);
      void Assign(const ParserClassFunctionReader &a_Reader);
#if defined(CXPARSER_TOKENREADER_LIFECYCLE_V1)
        void CopyLifecycleStateFrom(const ParserClassFunctionReader &a_Reader);
        void FinalizeCloneParent(ParserBase *a_pParent);
        void ResetLifecycleForNewFormula();
        void ResetLifecycleForExpressionEnd();
#endif

  public:
      ParserClassFunctionReader(ParserBase *a_pParent);
     ~ParserClassFunctionReader();
      
      ParserClassFunctionReader* Clone(ParserBase *a_pParent) const;
      void AddValIdent(identfun_type a_pCallback);
      void SetVarCreator(facfun_type a_pFactory);
	  void SetClassUing(void * a_pVoid);
      int GetPos() const;
      const string_type& GetFormula() const;
      const varmap_type& GetUsedVar() const;
      void SetFormula(const string_type &a_strFormula);
      void SetDefs( const funmap_type *a_pFunDef,
                    const funmap_type *a_pOprtDef,
                    const funmap_type *a_pInfixOprtDef,
                    const funmap_type *a_pPostOprtDef,
                    varmap_type *a_pVarDef,
                    const strmap_type *a_pStrVarDef,
                    const valmap_type *a_pConstDef );
      void IgnoreUndefVar(bool bIgnore);
	  void UsingClassDef(bool busing);

      void ReInit();
	  void EndExpress();

      
      token_type ReadNextToken();

	 void SetCreateClassObj(const string_type &a_strclass,const string_type &a_strobj);

  private:

      void SetParent(ParserBase *a_pParent);

      int ExtractToken( const char_type *a_szCharSet,
                        string_type &a_strTok, int a_iPos ) const;
      bool IsBuiltIn(token_type &a_Tok);
      bool IsEOF(token_type &a_Tok);

      bool IsInfixOpTok(token_type &a_Tok);
      bool IsFunTok(token_type &a_Tok);
      bool IsPostOpTok(token_type &a_Tok);
      bool IsOprt(token_type &a_Tok);
      bool IsValTok(token_type &a_Tok);
      bool IsVarTok(token_type &a_Tok);
      bool IsStrVarTok(token_type &a_Tok);
      bool IsUndefVarTok(token_type &a_Tok);
      bool IsString(token_type &a_Tok);

	  bool IsClassDefTok(token_type &a_Tok);
	  bool IsClassObjDefTok(token_type &a_Tok);
	  bool IsClassObjTok(token_type &a_Tok);
	  bool IsClassFucTok(token_type &a_Tok);

	  bool IsReturnTok(token_type &a_Tok);

	  void Error( EErrorCodes a_iErrc, int a_iPos = -1,
                  const string_type &a_sTok = string_type() ) const;
  };
}

#endif
