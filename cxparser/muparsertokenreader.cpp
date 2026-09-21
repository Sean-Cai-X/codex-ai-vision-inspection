

#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <stack>
#include <string>

#include "muParserTokenReader.h"

#include "muParserBase.h"

namespace mu
{

  class ParserBase;

  
  ParserTokenReader::ParserTokenReader(const ParserTokenReader &a_Reader)
  {
    Assign(a_Reader);
  }

  ParserTokenReader& ParserTokenReader::operator=(const ParserTokenReader &a_Reader)
  {
    if (&a_Reader!=this)
      Assign(a_Reader);

    return *this;
  }

  void ParserTokenReader::Assign(const ParserTokenReader &a_Reader)
  {
#if defined(CXPARSER_TOKENREADER_LIFECYCLE_V1)
    CopyLifecycleStateFrom(a_Reader);
#else
    m_pParser = a_Reader.m_pParser;
    m_strFormula = a_Reader.m_strFormula;
    m_iPos = a_Reader.m_iPos;
    m_iSynFlags = a_Reader.m_iSynFlags;

    m_UsedVar = a_Reader.m_UsedVar;
    m_pFunDef = a_Reader.m_pFunDef;
    m_pConstDef = a_Reader.m_pConstDef;
    m_pVarDef = a_Reader.m_pVarDef;
    m_pStrVarDef = a_Reader.m_pStrVarDef;
	m_pStringVarDef = a_Reader.m_pStringVarDef;
    m_pPostOprtDef = a_Reader.m_pPostOprtDef;
    m_pInfixOprtDef = a_Reader.m_pInfixOprtDef;
    m_pOprtDef = a_Reader.m_pOprtDef;
    m_bIgnoreUndefVar = a_Reader.m_bIgnoreUndefVar;
    m_vIdentFun = a_Reader.m_vIdentFun;
    m_pFactory = a_Reader.m_pFactory;
    m_iBrackets = a_Reader.m_iBrackets;

	m_iBigBrackets =a_Reader.m_iBigBrackets;
	m_pClassDef=a_Reader.m_pClassDef;
	m_bClassDef=a_Reader.m_bClassDef;
	m_iPreFlags=a_Reader.m_iPreFlags;

	m_icurID = a_Reader.m_icurID;
#endif
  }

#if defined(CXPARSER_TOKENREADER_LIFECYCLE_V1)
  void ParserTokenReader::CopyLifecycleStateFrom(const ParserTokenReader &a_Reader)
  {
    m_pParser = a_Reader.m_pParser;
    m_strFormula = a_Reader.m_strFormula;
    m_iPos = a_Reader.m_iPos;
    m_iSynFlags = a_Reader.m_iSynFlags;

    m_UsedVar = a_Reader.m_UsedVar;
    m_pFunDef = a_Reader.m_pFunDef;
    m_pConstDef = a_Reader.m_pConstDef;
    m_pVarDef = a_Reader.m_pVarDef;
    m_pStrVarDef = a_Reader.m_pStrVarDef;
    m_pStringVarDef = a_Reader.m_pStringVarDef;
    m_pPostOprtDef = a_Reader.m_pPostOprtDef;
    m_pInfixOprtDef = a_Reader.m_pInfixOprtDef;
    m_pOprtDef = a_Reader.m_pOprtDef;
    m_bIgnoreUndefVar = a_Reader.m_bIgnoreUndefVar;
    m_vIdentFun = a_Reader.m_vIdentFun;
    m_pFactory = a_Reader.m_pFactory;
    m_iBrackets = a_Reader.m_iBrackets;
    m_iBigBrackets = a_Reader.m_iBigBrackets;
    m_pClassDef = a_Reader.m_pClassDef;
    m_bClassDef = a_Reader.m_bClassDef;
    m_iPreFlags = a_Reader.m_iPreFlags;
    m_icurID = a_Reader.m_icurID;
  }

  void ParserTokenReader::FinalizeCloneParent(ParserBase *a_pParent)
  {
    SetParent(a_pParent);
    m_UsedVar.clear();
  }

  void ParserTokenReader::ResetLifecycleForNewFormula()
  {
    m_iPos = 0;
    m_iSynFlags = noOPT | noBC | noPOSTOP | noASSIGN | noRB | noClassObjDef;
    m_iBrackets = 0;
    m_iBigBrackets = 0;
    m_iPreFlags = 0;
    m_UsedVar.clear();
  }

  void ParserTokenReader::ResetLifecycleForExpressionEnd()
  {
    m_iSynFlags = noOPT | noBC | noPOSTOP | noASSIGN | noRB | noClassObjDef;
    m_iBrackets = 0;
    m_iPreFlags = isANY;
    m_UsedVar.clear();
  }
#endif

  ParserTokenReader::ParserTokenReader(ParserBase *a_pParent)
    :m_pParser(a_pParent)
    ,m_strFormula()
    ,m_iPos(0)
    ,m_iSynFlags(0)
    ,m_bIgnoreUndefVar(false)
    ,m_pFunDef(0)
    ,m_pPostOprtDef(0)
    ,m_pInfixOprtDef(0)
    ,m_pOprtDef(0)
    ,m_pConstDef(0)
    ,m_pStrVarDef(0)
    ,m_pVarDef(0)
    ,m_pFactory(0)
    ,m_vIdentFun()
    ,m_UsedVar()
    ,m_fZero(0)
    ,m_iBrackets(0)
	,m_iBigBrackets(0)
	,m_pClass(0)
	,m_pClassDef(0)
	,m_pClassBase(0)
	,m_pClassObj(0)
	,m_bClassDef(false)
	,m_iPreFlags(0)
	,m_pStringVarDef(0)

  {
    assert(m_pParser);
    SetParent(m_pParser);
  }

  ParserTokenReader::~ParserTokenReader()
  {}

  ParserTokenReader* ParserTokenReader::Clone(ParserBase *a_pParent) const
  {
    std::unique_ptr <ParserTokenReader> ptr(new ParserTokenReader(*this));
#if defined(CXPARSER_TOKENREADER_LIFECYCLE_V1)
    ptr->FinalizeCloneParent(a_pParent);
#else
    ptr->SetParent(a_pParent);
#endif
    return ptr.release();
  }

  void ParserTokenReader::AddValIdent(identfun_type a_pCallback)
  {
    m_vIdentFun.push_back(a_pCallback);
  }

  void ParserTokenReader::SetVarCreator(facfun_type a_pFactory)
  {
    m_pFactory = a_pFactory;
  }

  void ParserTokenReader::SetClassUing(void * a_pVoid)
  {
	  m_pClass=a_pVoid;
  }

  int ParserTokenReader::GetPos() const
  {
    return m_iPos;
  }

  const string_type& ParserTokenReader::GetFormula() const
  {
    return m_strFormula;
  }

  const varmap_type& ParserTokenReader::GetUsedVar() const
  {
    return m_UsedVar;
  }

  
  void ParserTokenReader::SetFormula(const string_type &a_strFormula)
  {
    m_strFormula = a_strFormula;
#if defined(CXPARSER_TOKENREADER_LIFECYCLE_V1)
    ResetLifecycleForNewFormula();
#else
    ReInit();
#endif
  }

  void ParserTokenReader::SetDefs( const funmap_type *a_pFunDef,
	  const funmap_type *a_pOprtDef,
                                   const funmap_type *a_pInfixOprtDef,
                                   const funmap_type *a_pPostOprtDef,
                                   varmap_type *a_pVarDef,
                                   const strmap_type *a_pStrVarDef,
                                   const valmap_type *a_pConstDef)
  {
    m_pFunDef = a_pFunDef;
    m_pOprtDef = a_pOprtDef;
    m_pInfixOprtDef = a_pInfixOprtDef;
    m_pPostOprtDef = a_pPostOprtDef;
    m_pVarDef = a_pVarDef;
    m_pStrVarDef = a_pStrVarDef;
    m_pConstDef = a_pConstDef;
  }

  void ParserTokenReader::IgnoreUndefVar(bool bIgnore)
  {
    m_bIgnoreUndefVar = bIgnore;
  }

  void ParserTokenReader::UsingClassDef(bool busing)
  {
	  m_bClassDef=busing;
  }

  void ParserTokenReader::ReInit()
  {
#if defined(CXPARSER_TOKENREADER_LIFECYCLE_V1)
    ResetLifecycleForNewFormula();
#else
    m_iPos = 0;
    m_iSynFlags = noOPT | noBC | noPOSTOP | noASSIGN | noRB  | noClassObjDef;
    m_iBrackets = 0;
	m_iBigBrackets =0;
	m_iPreFlags=0;
    m_UsedVar.clear();
#endif
  }

  void ParserTokenReader::EndExpress()
  {
#if defined(CXPARSER_TOKENREADER_LIFECYCLE_V1)
    ResetLifecycleForExpressionEnd();
#else
    m_iSynFlags = noOPT | noBC | noPOSTOP | noASSIGN | noRB | noClassObjDef;
    m_iBrackets = 0;
	m_iPreFlags = isANY;
    m_UsedVar.clear();
#endif
  }

ParserTokenReader::token_type ParserTokenReader::ReadNextToken()
{

m_icurID = 0;
    assert(m_pParser);
    std::stack<int> FunArgs;
    const char_type *szFormula = m_strFormula.c_str();
	int icharsize = m_strFormula.size();
    token_type tok;

	if( IsComment(tok)) ;

	if ( IsEOF(tok) )
		return tok;
	if ( IsOprt(tok) )
		return tok;
	if( IsBuiltInCoditionTok(tok))
		return tok;
	if ( IsBuiltIn(tok) )
		return tok;
    if ( IsFunTok(tok) )
		return tok;
    if ( IsValTok(tok) )
		return tok;
    if ( IsVarTok(tok) )
		return tok;
	if ( IsStrVarTok(tok) )
		return tok;
    if ( IsString(tok) )
		return tok;
    if ( IsInfixOpTok(tok) )
		return tok;
    if ( IsPostOpTok(tok) )
		return tok;

	if ( m_bClassDef&&IsClassDefTok(tok))
		return tok;
	if ( m_bClassDef&&IsClassObjDefTok(tok))
		return tok;
	if ( m_bClassDef&&IsClassObjTok(tok))
		return tok;
	if ( m_bClassDef&&IsClassFucTok(tok))
		return tok;

    if ( (m_bIgnoreUndefVar || m_pFactory) && IsUndefVarTok(tok) )
		return tok;

    string_type strTok;
    int iEnd = ExtractToken(m_pParser->ValidNameChars(), strTok, m_iPos);
    if (iEnd!=m_iPos)
      Error(ecUNASSIGNABLE_TOKEN, m_iPos, strTok);
    Error(ecUNASSIGNABLE_TOKEN, m_iPos, m_strFormula.substr(m_iPos));
    return token_type();
}

  void ParserTokenReader::SetParent(ParserBase *a_pParent)
  {
    m_pParser  = a_pParent;
    m_pFunDef  = &a_pParent->m_FunDef;
    m_pOprtDef = &a_pParent->m_OprtDef;
    m_pInfixOprtDef = &a_pParent->m_InfixOprtDef;
    m_pPostOprtDef  = &a_pParent->m_PostOprtDef;
    m_pVarDef       = &a_pParent->m_VarDef;
    m_pStrVarDef    = &a_pParent->m_StrVarDef;
    m_pConstDef     = &a_pParent->m_ConstDef;

	m_pClassDef     = &a_pParent->m_ClassDefMap;
	m_pStringVarDef = &a_pParent->m_StringVarDef;

  }

  int ParserTokenReader::ExtractToken( const char_type *a_szCharSet,
                                       string_type &a_sTok, int a_iPos ) const
  {

	int iEnd = (int)m_strFormula.find_first_not_of(a_szCharSet, a_iPos);

    if (iEnd==(int)string_type::npos)
        iEnd = (int)m_strFormula.length();

    a_sTok = string_type( m_strFormula.begin()+a_iPos, m_strFormula.begin()+iEnd);
    a_iPos = iEnd;
    return iEnd;
  }

  bool ParserTokenReader::IsComment(token_type &a_Tok)
  {

	const char_type* szFormula = m_strFormula.c_str();
	int icharsize = m_strFormula.size();
	if(szFormula[m_iPos]=='/')
	{
	  if(szFormula[m_iPos+1]=='*')
	  {
		  ++m_iPos;
		  ++m_iPos;
		  while(szFormula[m_iPos]!='*')
		  {
			  ++m_iPos;
			  if(m_iPos>=icharsize-1)
				  break;
		  }
		  if(szFormula[m_iPos]=='*')
			  if(szFormula[m_iPos+1]=='/')
			  {
				  ++m_iPos;
				  ++m_iPos;
			  }
	  }
	  if(szFormula[m_iPos+1]=='/')
	  {
		  ++m_iPos;
		  ++m_iPos;
		  while(szFormula[m_iPos]!='\r'||szFormula[m_iPos]!='\n')
		  {
			  ++m_iPos;
			  if(m_iPos>=icharsize-1)
				  break;
		  }
	  }

	}

	  while (szFormula[m_iPos]==' '
		  ||szFormula[m_iPos]==0x09
		  ||szFormula[m_iPos]=='\r'
		  ||szFormula[m_iPos]=='\n')
		  ++m_iPos;

	  return false;
  }

  bool ParserTokenReader::IsBuiltInCoditionTok(token_type &a_Tok)
  {
	  string_type strTok;
	  int iEnd = ExtractToken(m_pParser->ValidNameChars(), strTok, m_iPos);
	  if (iEnd==m_iPos)
		  return false;

	  const char_type **const pOprt = m_pParser->GetOprtDef();
	  assert(pOprt);

	  const char_type *szExpr = m_strFormula.c_str();

	  ECmdCode eFunTok = cmUNKNOWN;
	  if(strTok==string_type(pOprt[cmIf]))
	  {
		  eFunTok = (ECmdCode)cmIf;
		  if (eFunTok==cmUNKNOWN)
			  return false;
		  int pos = iEnd;
		  while (szExpr[pos] == ' ' || szExpr[pos] == '\t' ||
				 szExpr[pos] == '\r' || szExpr[pos] == '\n')
		  {
			  ++pos;
		  }
		  if (szExpr[pos]!='(')
			  return false;

		  a_Tok.Set(eFunTok, strTok);

		  m_iPos = pos;
		  if (m_iSynFlags & noFUN)
			  Error(ecUNEXPECTED_FUN, m_iPos-(int)a_Tok.GetAsString().length(), a_Tok.GetAsString());

		  m_iSynFlags = noANY ^ noBO;

		  return true;
	  }
	  if(strTok==string_type(pOprt[cmWhile]))
	  {
		  eFunTok = (ECmdCode)cmWhile;
		  if (eFunTok==cmUNKNOWN)
			  return false;
		  int pos = iEnd;
		  while (szExpr[pos] == ' ' || szExpr[pos] == '\t' ||
				 szExpr[pos] == '\r' || szExpr[pos] == '\n')
		  {
			  ++pos;
		  }
		  if (szExpr[pos]!='(')
			  return false;

		  a_Tok.Set(eFunTok, strTok);

		  m_iPos = pos;
		  if (m_iSynFlags & noFUN)
			  Error(ecUNEXPECTED_FUN, m_iPos-(int)a_Tok.GetAsString().length(), a_Tok.GetAsString());

		  m_iSynFlags = noANY ^ noBO;

		  return true;
	  }

	  if(strTok==string_type(pOprt[cmElse]))
		{
		  eFunTok = (ECmdCode)cmElse;
		  if (eFunTok==cmUNKNOWN)
			  return false;
		  int pos = iEnd;
		  while (szExpr[pos] == ' ' || szExpr[pos] == '\t' ||
				 szExpr[pos] == '\r' || szExpr[pos] == '\n')
		  {
			  ++pos;
		  }
		  if (szExpr[pos]!='{')
			  return false;

		  a_Tok.Set(eFunTok, strTok);

		  m_iPos = pos;
		  if (m_iSynFlags & noFUN)
			  Error(ecUNEXPECTED_FUN, m_iPos-(int)a_Tok.GetAsString().length(), a_Tok.GetAsString());

		  m_iSynFlags = noANY ^ noLB;

		  return true;
	  }

	  if(strTok==string_type(pOprt[cmReturn]))
	  {
		  eFunTok = (ECmdCode)cmReturn;
		  if (eFunTok==cmUNKNOWN)
			  return false;

		  int pos = iEnd;
		  while (szExpr[pos] == ' ' || szExpr[pos] == '\t' ||
				 szExpr[pos] == '\r' || szExpr[pos] == '\n')
		  {
			  ++pos;
		  }

		  if (szExpr[pos] != ';')
			  Error(ecUNEXPECTED_OPERATOR, pos, strTok);

		  a_Tok.Set(eFunTok, strTok);
		  m_iPos = pos;
		  m_iSynFlags = noANY ^ noSEMIC;
		  return true;
	  }
	  return false;
  }

  bool ParserTokenReader::IsBuiltIn(token_type &a_Tok)
  {

	  const char_type **pOprtDef = m_pParser->GetOprtDef();

	  const char_type* szFormula = m_strFormula.c_str();

    for (int i=0; pOprtDef[i]; i++)
    {
      std::size_t len = std::strlen( pOprtDef[i] );

      if (!std::strncmp(&szFormula[m_iPos], pOprtDef[i], len))
	    {
			switch(i)
			{

				case cmAND:
				case cmOR:
				case cmXOR:
				case cmLT:
				case cmGT:
				case cmLE:
				case cmGE:
				case cmNEQ:
				case cmEQ:
				case cmADD:
				case cmSUB:
				case cmMUL:

					if(isClassDef==m_iPreFlags)
					{
						m_iPreFlags=isClassObjDef;
						m_iSynFlags=noANY ^ ( noClassObjDef );
						break;
					}

				case cmDIV:
				case cmPOW:
				case cmASSIGN:

				  if (i==cmASSIGN && m_iSynFlags & noASSIGN)
					Error(ecUNEXPECTED_OPERATOR, m_iPos, pOprtDef[i]);
				  if (!m_pParser->HasBuiltInOprt()) continue;
				  if (m_iSynFlags & noOPT)
				  {

						if ( IsInfixOpTok(a_Tok) )
						  return true;

						Error(ecUNEXPECTED_OPERATOR, m_iPos, pOprtDef[i]);
				  }
				  m_iPreFlags  = isASSIGN;
				  m_iSynFlags  = noBC | noOPT | noCOMMA | noPOSTOP | noASSIGN | noLB | noRB;
				  m_iSynFlags |= ( (i != cmEND) && ( i != cmBC) ) ? noEND : 0;
				  break;

				case cmCOMMA:
						  if (m_iSynFlags & noCOMMA)
							  Error(ecUNEXPECTED_COMMA, m_iPos, pOprtDef[i]);

						  m_iPreFlags  = isCOMMA;
						  m_iSynFlags  = noBC | noOPT | noEND | noCOMMA | noPOSTOP | noASSIGN | noLB | noRB;
						break;

				case cmBO:
						  if (m_iSynFlags & noBO)
							  Error(ecUNEXPECTED_PARENS, m_iPos, pOprtDef[i]);

						  m_iPreFlags =  isBO;
						  m_iSynFlags =  noOPT | noEND | noCOMMA | noPOSTOP | noASSIGN | noLB | noRB;
						++m_iBrackets;
						  break;

				case cmBC:
					if (m_iSynFlags & noBC)
						Error(ecUNEXPECTED_PARENS, m_iPos, pOprtDef[i]);

					m_iPreFlags  = isBC;
					m_iSynFlags  = noBO | noVAR | noVAL | noFUN | noINFIXOP | noSTR | noASSIGN | noRB;

					if (--m_iBrackets<0)
						Error(ecUNEXPECTED_PARENS, m_iPos, pOprtDef[i]);
					break;

				case cmLB:
					if (m_iSynFlags & noLB)
						Error(ecMISSING_BIGBRACKET, m_iPos, pOprtDef[i]);

					m_iPreFlags =  isLB;
					m_iSynFlags =  noBC | noOPT | noEND | noCOMMA | noPOSTOP | noASSIGN | noRB | noSEMIC;

					++m_iBigBrackets;

					break;

				case cmRB:
					if (m_iSynFlags & noRB)
						Error(ecMISSING_BIGBRACKET, m_iPos, pOprtDef[i]);

					m_iPreFlags  = isRB;
					m_iSynFlags  = noINFIXOP | noASSIGN ;

					if (--m_iBigBrackets<0)
						Error(ecMISSING_BIGBRACKET, m_iPos, pOprtDef[i]);

					break;
				case cmSEMICOLON:
					if (m_iSynFlags & noSEMIC)
						Error(ecMISSING_BIGBRACKET, m_iPos, pOprtDef[i]);

					m_iPreFlags = isSEMIC;
					m_iSynFlags  = noBO | noINFIXOP | noSTR | noASSIGN ;

					break;
				case cmMember:
					if (m_iSynFlags & noClassMemOp)
						Error(ecCLASSMEMOPT_CONFICT, m_iPos, pOprtDef[i]);

					m_iPreFlags = isClassMemOp;
					m_iSynFlags  =  noANY ^ ( noClassMemVar | noClassMemFuc );

					break;
				case cmPointer:
					if (m_iSynFlags & noClassPointOp)
						Error(ecCLASSPOINT_CONFICT, m_iPos, pOprtDef[i]);

					m_iPreFlags = isClassPointOp;
					m_iSynFlags  =  noANY ^ ( noClassMemVar | noClassMemFuc );

					break;
				case cmReturn:
				{
					const int iReturnEnd = m_iPos + (int)len;
					string_type strTok;
					const int iNameEnd = ExtractToken(m_pParser->ValidNameChars(), strTok, m_iPos);
					if (iNameEnd != iReturnEnd)
						continue;

					int iSemiPos = iReturnEnd;
					while (szFormula[iSemiPos] == ' ' || szFormula[iSemiPos] == '\t' ||
						   szFormula[iSemiPos] == '\r' || szFormula[iSemiPos] == '\n')
					{
						++iSemiPos;
					}

					if (szFormula[iSemiPos] != ';')
						Error(ecUNEXPECTED_OPERATOR, iSemiPos, pOprtDef[i]);

					m_iPos = iSemiPos;
					m_iPreFlags = isANY;
					m_iSynFlags = noANY ^ noSEMIC;
					a_Tok.Set((ECmdCode)i, pOprtDef[i]);
					return true;
				}
				default:
				  Error(ecINTERNAL_ERROR);
			}

			m_iPos += (int)len;
			a_Tok.Set( (ECmdCode)i, pOprtDef[i] );
			return true;
	    }
    }

    return false;
  }

  bool ParserTokenReader::IsEOF(token_type &a_Tok)
  {
    const char_type* szFormula = m_strFormula.c_str();

    if ( !szFormula[m_iPos] || szFormula[m_iPos] == '\n')
    {
      if ( m_iSynFlags & noEND )
        Error(ecUNEXPECTED_EOF, m_iPos);

      if (m_iBrackets>0)
        Error(ecMISSING_PARENS, m_iPos, _T(")"));

	  if(m_iBigBrackets>0)
		  Error(ecMISSING_BIGBRACKET,m_iPos,_T("}"));

      m_iSynFlags = 0;
      a_Tok.Set(cmEND);

      return true;
    }

    return false;
  }

  
  bool ParserTokenReader::IsInfixOpTok(token_type &a_Tok)

  {
    string_type sTok;
    int iEnd = ExtractToken(m_pParser->ValidInfixOprtChars(), sTok, m_iPos);
    if (iEnd==m_iPos)
      return false;

    funmap_type::const_iterator item = m_pInfixOprtDef->find(sTok);
    if (item==m_pInfixOprtDef->end())
      return false;

    a_Tok.Set(item->second, sTok);
    m_iPos = (int)iEnd;

    if (m_iSynFlags & noINFIXOP)
      Error(ecUNEXPECTED_OPERATOR, m_iPos, a_Tok.GetAsString());

    m_iSynFlags = noPOSTOP | noINFIXOP | noBC | noSTR | noASSIGN | noClassDef;
    return true;
  }

  bool ParserTokenReader::IsFunTok(token_type &a_Tok)
  {
    string_type strTok;

    int iEnd = ExtractToken(m_pParser->ValidNameChars(), strTok, m_iPos);
    if (iEnd==m_iPos)
      return false;

    funmap_type::const_iterator item = m_pFunDef->find(strTok);
    if (item==m_pFunDef->end())
        return false;

    a_Tok.Set(item->second, strTok);

    m_iPos = (int)iEnd;
    if (m_iSynFlags & noFUN)
      Error(ecUNEXPECTED_FUN, m_iPos-(int)a_Tok.GetAsString().length(), a_Tok.GetAsString());

    m_iSynFlags = noANY ^ noBO;
	m_iPreFlags = isFUN;
    return true;
  }

  bool ParserTokenReader::IsPostOpTok(token_type &a_Tok)
  {

    string_type sTok;
    int iEnd = ExtractToken(m_pParser->ValidOprtChars(), sTok, m_iPos);
    if (iEnd==m_iPos)
      return false;

    funmap_type::const_iterator item = m_pPostOprtDef->begin();
    for (item=m_pPostOprtDef->begin(); item!=m_pPostOprtDef->end(); ++item)
    {
      if (sTok.find(item->first)!=0)
        continue;

      a_Tok.Set(item->second, sTok);
      m_iPos += (int)item->first.length();

      if (m_iSynFlags & noPOSTOP)
        Error(ecUNEXPECTED_OPERATOR, m_iPos-(int)a_Tok.GetAsString().length(), a_Tok.GetAsString());

      m_iSynFlags = noVAL | noVAR | noFUN | noBO | noPOSTOP | noSTR | noASSIGN | noClassDef;
      return true;
    }

    return false;
  }

  bool ParserTokenReader::IsOprt(token_type &a_Tok)
  {

	const char_type *szFormula = m_strFormula.c_str();
    int iVarEnd = (int)strspn(&szFormula[m_iPos], m_pParser->ValidOprtChars());
    if (!iVarEnd) return false;

    string_type strOprt(&szFormula[m_iPos], &szFormula[m_iPos+iVarEnd]);

    funmap_type::const_iterator item = m_pOprtDef->find(strOprt);
    if (item==m_pOprtDef->end())
      return false;

    a_Tok.Set(item->second, strOprt);

    if (m_iSynFlags & noOPT)
    {

      if ( IsInfixOpTok(a_Tok) ) return true;

      Error(ecUNEXPECTED_OPERATOR, m_iPos, a_Tok.GetAsString());
    }

	m_iPos += iVarEnd;
    m_iSynFlags  = noBC | noOPT | noCOMMA | noPOSTOP | noEND | noBC | noASSIGN | noClassDef;
    return true;
  }

  
  bool ParserTokenReader::IsValTok(token_type &a_Tok)
  {

    assert(m_pConstDef);
    assert(m_pParser);

    #if defined(_MSC_VER)
      #pragma warning( disable : 4244 )
    #endif

    string_type strTok;
    value_type fVal(0);
    int iEnd(0);

    iEnd = ExtractToken(m_pParser->ValidNameChars(), strTok, m_iPos);
    if (iEnd!=m_iPos)
    {
		  valmap_type::const_iterator item = m_pConstDef->find(strTok);
		  if (item!=m_pConstDef->end())
		  {
			m_iPos = iEnd;
			a_Tok.SetVal(item->second, strTok);

			if (m_iSynFlags & noVAL)
			  Error(ecUNEXPECTED_VAL, m_iPos - (int)strTok.length(), strTok);

			m_iSynFlags = noVAL | noVAR | noFUN | noBO | noINFIXOP | noSTR | noASSIGN | noClassDef |noClassObjDef |noClassObj;
            m_iSynFlags &= ~noOPT;
			return true;
		  }
    }

    std::vector<identfun_type>::const_iterator item = m_vIdentFun.begin();
    for (item = m_vIdentFun.begin(); item!=m_vIdentFun.end(); ++item)
    {
      int iStart = m_iPos;
      if ( (*item)(m_strFormula.c_str() + m_iPos, m_iPos, fVal) )
      {
        strTok.assign(m_strFormula.c_str(), iStart, m_iPos);

        if (m_iSynFlags & noVAL)
          Error(ecUNEXPECTED_VAL, m_iPos - (int)strTok.length(), strTok);

        a_Tok.SetVal(fVal, strTok);
        m_iSynFlags = noVAL | noVAR | noFUN | noBO | noINFIXOP | noSTR | noASSIGN;
        m_iSynFlags &= ~noOPT;
        return true;
      }
    }

    return false;

    #if defined(_MSC_VER)
      #pragma warning( default : 4244 )
    #endif
  }

  
  bool ParserTokenReader::IsVarTok(token_type &a_Tok)
  {

    if (!m_pVarDef->size())
      return false;

    string_type strTok;
    int iEnd = ExtractToken(m_pParser->ValidNameChars(), strTok, m_iPos);
    if (iEnd==m_iPos)
      return false;

    varmap_type::const_iterator item =  m_pVarDef->find(strTok);
    if (item==m_pVarDef->end())
      return false;

    if (m_iSynFlags & noVAR)
      Error(ecUNEXPECTED_VAR, m_iPos, strTok);

    m_iPos = iEnd;
    a_Tok.SetVar(item->second, strTok);
    m_UsedVar[item->first] = item->second;

    m_iSynFlags = noVAL | noVAR | noFUN | noBO | noPOSTOP | noINFIXOP | noSTR
        | noClassDef | noClassObjDef | noClassObj;
    m_iSynFlags &= ~noOPT;
    return true;
  }

  bool ParserTokenReader::IsClassObjDefTok(token_type &a_Tok)
  {
	  if(isClassDef!=m_iPreFlags
		  &&isClassPointObjDef!=m_iPreFlags)
		  return false;

	  string_type strTok;

	  int iEnd = ExtractToken(m_pParser->ValidNameChars(), strTok, m_iPos);

	  if (iEnd==m_iPos)
		  return false;

	  if (m_iSynFlags & noClassObjDef)
			Error(ecUNEXPECTED_VAR, m_iPos - (int)a_Tok.GetAsString().length(), strTok);

	  string_type str=m_pClassBase->getclass();

	  if(isClassDef==m_iPreFlags)
	  {
		  if(str==typeid(value_type).name())
		  {
				  if ( m_bIgnoreUndefVar || m_pFactory )
				  {

					  return false;
				  }
				  else
				  {

						  value_type *fVar =(value_type *) ( m_pClassBase->addvar(strTok));
						  if(0==fVar)
							  Error(ecUNEXPECTED_VAR, m_iPos - (int)a_Tok.GetAsString().length(), a_Tok.GetAsString());
						  a_Tok.SetVar(fVar, strTok );
						  (*m_pVarDef)[strTok] = fVar;
						  m_UsedVar[strTok] = fVar;
				  }
		  }
		  else
		  {

				{

					  void *pclassvar=m_pClassBase->addvar(strTok);
					  if(0==pclassvar)
						  Error(ecUNEXPECTED_VAR, m_iPos - (int)a_Tok.GetAsString().length(), strTok);

					  m_pClassObj=pclassvar;
					  a_Tok.SetClassVarDef(m_pClassBase,pclassvar, strTok );
				}
		  }

	  }
	  else
	  if(isClassPointObjDef==m_iPreFlags)
	  {
		  if(str==typeid(value_type).name())
		  {

			  assert(0);

		  }
		  else
		  {

			  void *pclassvar=m_pClassBase->addpointvar(strTok,0);

			  if(0==pclassvar)
				  Error(ecUNEXPECTED_VAR, m_iPos - (int)a_Tok.GetAsString().length(), strTok);

			  m_pClassObj=pclassvar;
			  a_Tok.SetClassVar(m_pClassBase,pclassvar, strTok );
		  }

	  }
	  m_pClassBase=NULL;
	  m_iPos = iEnd;

	  m_iSynFlags =noVAL | noVAR | noFUN | noBO | noPOSTOP | noINFIXOP | noSTR
		  | noClassDef | noClassObjDef | noClassObj;
	  m_iPreFlags = isClassObjDef;
	  return true;
  }

  bool ParserTokenReader::IsClassObjTok(token_type &a_Tok)
  {
	  if( 0!=m_iPreFlags
		  &&isSEMIC!=m_iPreFlags
		  &&isBO!=m_iPreFlags
		  &&isCOMMA!=m_iPreFlags
		  &&isLB!=m_iPreFlags
		  &&isRB!=m_iPreFlags
		  &&isASSIGN!=m_iPreFlags)
		  return false;

	  string_type strTok;
	  int iEnd = ExtractToken(m_pParser->ValidNameChars(), strTok, m_iPos);
	  if (iEnd==m_iPos)
		  return false;

	  if (m_iSynFlags & noClassObj)
			Error(ecUNEXPECTED_VAR, m_iPos - (int)a_Tok.GetAsString().length(),strTok);

	  {
		  mu::classbasemap_type classmap = m_pParser->GetClassMap();
		  if(!classmap.size())
			  return false;

		  classbasemap_type::const_iterator item = classmap.begin();

		  for (; item!=classmap.end(); ++item)
		  {
			  classbase *pclass=(item->second);
			  if(pclass->findvar( strTok ))
			  {
				   void *pclassvar=pclass->getvar(strTok);
				  a_Tok.SetClassVar(pclass,pclassvar, strTok );
				  m_pClassBase=pclass;
				  m_pClassObj=pclassvar;
				  break;
			  }
		  }

		  if(item==classmap.end())
		  {

			  Error(ecUNEXPECTED_VAR, m_iPos - (int)a_Tok.GetAsString().length(), strTok);

		  }

	  }

	  m_iPos = iEnd;

	  m_iSynFlags =noVAL | noVAR | noFUN | noBO | noPOSTOP | noINFIXOP | noSTR
		  | noClassDef | noClassObjDef | noClassObj;
	  m_iPreFlags = isClassObj;
	  return true;
  }

  bool ParserTokenReader::IsClassDefTok(token_type &a_Tok)
  {
	  if(0!=m_iPreFlags
		  &&isSEMIC!=m_iPreFlags
		  &&isLB!=m_iPreFlags
		  &&isRB!=m_iPreFlags)
		  return false;

	  if (!m_pClassDef->size())
		  return false;

	  string_type strTok;
	  int iEnd = ExtractToken(m_pParser->ValidNameChars(), strTok, m_iPos);
	  if (iEnd==m_iPos)
		  return false;

	  classbasemap_type::const_iterator item =  m_pClassDef->find(strTok);
	  if (item==m_pClassDef->end())
		  return false;

	  if (m_iSynFlags & noClassDef)
		  Error(ecCLASS_CONFICT, m_iPos, strTok);

	  m_iPos = iEnd;

	  m_pClassBase=item->second;

	  a_Tok.SetClass(m_pClassBase,strTok);

	  m_iSynFlags =  noANY ^ ( noClassObjDef );
	  m_iPreFlags = isClassDef;
	  return true;
  }

  bool ParserTokenReader::IsClassFucTok(token_type &a_Tok)
  {

	  if(isClassMemOp!=m_iPreFlags
		  &&isClassPointOp!=m_iPreFlags)
		  return false;
	  string_type strTok;

	  int iEnd = ExtractToken(m_pParser->ValidNameChars(), strTok, m_iPos);
	  if (iEnd==m_iPos)
		  return false;

		if(!m_pClassBase->findclassfun(strTok))
			Error(ecCLASSFUC_CONFICT, m_iPos - (int)a_Tok.GetAsString().length(), strTok);

		a_Tok.SetClassFuc(m_pClassBase,m_pClassObj,strTok);

	  m_iPos = (int)iEnd;

	  m_iSynFlags = noANY ^ noBO;
	  m_iPreFlags = isClassMemFuc;
	  return true;
  }

  bool ParserTokenReader::IsStrVarTok(token_type &a_Tok)
  {
    if (!m_pStrVarDef || !m_pStrVarDef->size())
      return false;

    string_type strTok;
    int iEnd = ExtractToken(m_pParser->ValidNameChars(), strTok, m_iPos);
    if (iEnd==m_iPos)
      return false;

    strmap_type::const_iterator item =  m_pStrVarDef->find(strTok);
    if (item==m_pStrVarDef->end())
      return false;

    if (m_iSynFlags & noSTR)
      Error(ecUNEXPECTED_VAR, m_iPos, strTok);

    m_iPos = iEnd;
    if (!m_pParser->m_vStringVarBuf.size())
      Error(ecINTERNAL_ERROR);

    a_Tok.SetString(m_pParser->m_vStringVarBuf[item->second], m_pParser->m_vStringVarBuf.size() );

    m_iSynFlags = m_iSynFlags = noANY ^ ( noBC | noOPT | noEND | noCOMMA);
    return true;
  }

  bool ParserTokenReader::IsUndefVarTok(token_type &a_Tok)
  {
    string_type strTok;
    int iEnd = ExtractToken(m_pParser->ValidNameChars(), strTok, m_iPos);
    if (iEnd==m_iPos)
      return false;

    if (m_iSynFlags & noVAR)
      Error(ecUNEXPECTED_VAR, m_iPos - (int)a_Tok.GetAsString().length(), strTok);

    if (m_pFactory)
    {
		value_type *fVar = m_pFactory(strTok.c_str(),m_pClass);
		a_Tok.SetVar(fVar, strTok );

		(*m_pVarDef)[strTok] = fVar;
		m_UsedVar[strTok] = fVar;
    }
    else
    {
		a_Tok.SetVar((value_type*)&m_fZero, strTok);
		m_UsedVar[strTok] = 0;
    }

    m_iPos = iEnd;

    m_iSynFlags = noVAL | noVAR | noFUN | noBO | noPOSTOP | noINFIXOP | noSTR;
    return true;
  }

  
  bool ParserTokenReader::IsString(token_type &a_Tok)
  {
    if (m_strFormula[m_iPos]!='"')
      return false;

    string_type strBuf(&m_strFormula[m_iPos+1]);
    std::size_t iEnd(0), iSkip(0);

    for(iEnd=(int)strBuf.find("\""); iEnd!=string_type::npos; iEnd=(int)strBuf.find("\"", iEnd))
    {
      if (strBuf[iEnd-1]!='\\') break;
      strBuf.replace(iEnd-1, 2, "\"");
      iSkip++;
    }

    if (iEnd==string_type::npos)
      Error(ecUNTERMINATED_STRING, m_iPos, "\"");

    string_type strTok(strBuf.begin(), strBuf.begin()+iEnd);

    if (m_iSynFlags & noSTR)
      Error(ecUNEXPECTED_STR, m_iPos, strTok);

		m_pParser->m_vStringBuf.push_back(strTok);
    a_Tok.SetString(strTok, m_pParser->m_vStringBuf.size());

    m_iPos += (int)strTok.length() + 2 + (int)iSkip;
    m_iSynFlags = m_iSynFlags = noANY ^ ( noBC | noOPT | noEND | noCOMMA );

    return true;
  }

  void  ParserTokenReader::Error( EErrorCodes a_iErrc,
                                  int a_iPos,
                                  const string_type &a_sTok) const
  {
    m_pParser->Error(a_iErrc, a_iPos, a_sTok);
  }
}
