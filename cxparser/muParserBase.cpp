

#include "muParser.h"

#include <cassert>
#include <cmath>
#include <cstring>
#include <memory>
#include <algorithm>
#include <vector>
#include <stack>
#include <deque>
#include <list>
#include <sstream>

using namespace std;
namespace
{
  bool IsControlFlowIdentChar(mu::char_type ch)
  {
    return (ch >= _T('0') && ch <= _T('9')) ||
           (ch >= _T('a') && ch <= _T('z')) ||
           (ch >= _T('A') && ch <= _T('Z')) ||
           ch == _T('_');
  }
  bool ContainsControlFlowKeyword(const mu::string_type &expr, const mu::char_type *keyword)
  {
    const mu::string_type needle(keyword);
    mu::string_type::size_type pos = expr.find(needle);
    while (pos != mu::string_type::npos)
    {
      const bool hasPrev = pos > 0;
      const mu::string_type::size_type nextPos = pos + needle.length();
      const bool hasNext = nextPos < expr.length();
      const bool prevIsIdent = hasPrev && IsControlFlowIdentChar(expr[pos - 1]);
      const bool nextIsIdent = hasNext && IsControlFlowIdentChar(expr[nextPos]);
      if (!prevIsIdent && !nextIsIdent)
        return true;
      pos = expr.find(needle, pos + needle.length());
    }
    return false;
  }

  template<typename T>
  T ReadPackedBytecodeItem(const mu::ParserByteCode::map_type *src)
  {
    T value;
    std::memcpy(&value, src, sizeof(T));
    return value;
  }

  mu::string_type TrimCreateClassText(const mu::string_type &text)
  {
    mu::string_type::size_type begin = 0;
    mu::string_type::size_type end = text.length();
    while (begin < end && (text[begin] == _T(' ') || text[begin] == _T('\t') ||
                           text[begin] == _T('\r') || text[begin] == _T('\n')))
      ++begin;
    while (end > begin && (text[end - 1] == _T(' ') || text[end - 1] == _T('\t') ||
                           text[end - 1] == _T('\r') || text[end - 1] == _T('\n')))
      --end;
    return text.substr(begin, end - begin);
  }

  void SplitCreateClassMemberNames(const mu::string_type &member_list,
                                   std::vector<mu::string_type> &member_names)
  {
    member_names.clear();
    mu::string_type::size_type start = 0;
    while (start < member_list.length())
    {
      mu::string_type::size_type end = member_list.find(_T(','), start);
      mu::string_type member_name = (end == mu::string_type::npos)
        ? member_list.substr(start)
        : member_list.substr(start, end - start);
      member_name = TrimCreateClassText(member_name);
      if (!member_name.empty())
        member_names.push_back(member_name);
      start = (end == mu::string_type::npos) ? member_list.length() : end + 1;
    }
  }

  bool SplitCreateClassDeclaration(const mu::string_type &decl,
                                   mu::string_type &type_name,
                                   std::vector<mu::string_type> &member_names)
  {
    type_name.clear();
    member_names.clear();
    mu::string_type trimmed = TrimCreateClassText(decl);
    if (trimmed.empty())
      return false;

    mu::string_type::size_type split = trimmed.find_first_of(_T(" \t"));
    if (split == mu::string_type::npos)
      return false;

    type_name = TrimCreateClassText(trimmed.substr(0, split));
    SplitCreateClassMemberNames(trimmed.substr(split + 1), member_names);
    return !type_name.empty() && !member_names.empty();
  }

  mu::string_type ReplaceCreateClassParamRefs(const mu::string_type &script_text,
                                              const mu::paramvect &params)
  {
    if (params.empty())
      return script_text;

    mu::string_type rewritten;
    mu::string_type token;
    bool in_string = false;
    bool escaped = false;

    auto flush_token = [&]()
    {
      if (token.empty())
        return;

      bool replaced = false;
      if (token == "arg" || token == "p")
      {
        std::ostringstream value_builder;
        value_builder << params[0];
        rewritten += value_builder.str();
        replaced = true;
      }
      else if ((token.size() > 3 && token.substr(0, 3) == "arg") ||
               (token.size() > 1 && token[0] == 'p'))
      {
        std::string index_text = (token.substr(0, 3) == "arg")
          ? token.substr(3)
          : token.substr(1);
        if (!index_text.empty())
        {
          const int index = std::atoi(index_text.c_str());
          if (index >= 0 && index < static_cast<int>(params.size()))
          {
            std::ostringstream value_builder;
            value_builder << params[static_cast<size_t>(index)];
            rewritten += value_builder.str();
            replaced = true;
          }
        }
      }

      if (!replaced)
        rewritten += token;
      token.clear();
    };

    for (mu::string_type::size_type i = 0; i < script_text.length(); ++i)
    {
      const mu::char_type ch = script_text[i];
      if (in_string)
      {
        rewritten += ch;
        if (escaped)
        {
          escaped = false;
          continue;
        }
        if (ch == _T('\\'))
        {
          escaped = true;
          continue;
        }
        if (ch == _T('"'))
          in_string = false;
        continue;
      }

      if (ch == _T('"'))
      {
        flush_token();
        rewritten += ch;
        in_string = true;
        escaped = false;
        continue;
      }

      if (IsControlFlowIdentChar(ch))
      {
        token += ch;
        continue;
      }

      flush_token();
      rewritten += ch;
    }

    flush_token();
    return rewritten;
  }
}

namespace mu
{

ParserBase::ParserBase()
  :m_pParseFormula(&ParserBase::ParseString)
  ,m_pCmdCode(0)
  ,m_vByteCode()
  ,m_vStringBuf()
  ,m_pTokenReader()
  ,m_FunDef()
  ,m_PostOprtDef()
  ,m_InfixOprtDef()
  ,m_OprtDef()
  ,m_ConstDef()
  ,m_StrVarDef()
  ,m_VarDef()
  ,m_bOptimize(true)
  ,m_bUseByteCode(true)
  ,m_bHasControlFlow(false)
  ,m_bBuiltInOp(true)
  ,m_sNameChars()
  ,m_sOprtChars()
  ,m_sInfixOprtChars()
  ,m_bcolllection(false)
  ,m_pClassReader()
  ,m_pClassFunReader()
{

  InitTokenReader();
  InitClassReader();
  InitClassFunReader();

  m_bstopcompile=false;
  m_bstoprun=false;
  m_boptcollect=false;
  m_bprecompile = false;
  m_pcreateclass = NULL;
  m_bcmd = true;

}

ParserBase::ParserBase(const ParserBase &a_Parser)
  :m_pParseFormula(&ParserBase::ParseString)
  ,m_pCmdCode(0)
  ,m_vByteCode()
  ,m_vStringBuf()
  ,m_pTokenReader()
  ,m_FunDef()
  ,m_PostOprtDef()
  ,m_InfixOprtDef()
  ,m_OprtDef()
  ,m_ConstDef()
  ,m_StrVarDef()
  ,m_VarDef()
  ,m_bOptimize(true)
  ,m_bUseByteCode(true)
  ,m_bHasControlFlow(false)
  ,m_bBuiltInOp(true)
  ,m_bcolllection(false)
  ,m_pClassReader()
  ,m_pClassFunReader()
{
  m_pTokenReader.reset(new token_reader_type(this));
  m_pClassReader.reset(new ParserClassReader(this));
  m_pClassFunReader.reset(new ParserClassFunctionReader(this));
  Assign(a_Parser);
}

ParserBase& ParserBase::operator=(const ParserBase &a_Parser)
{
  Assign(a_Parser);
  return *this;
}

void ParserBase::Assign(const ParserBase &a_Parser)
{
  if (&a_Parser==this)
    return;

  ReInit();

  m_ConstDef = a_Parser.m_ConstDef;
  m_VarDef = a_Parser.m_VarDef;
  m_bOptimize  = a_Parser.m_bOptimize;
  m_bHasControlFlow = a_Parser.m_bHasControlFlow;
  m_bBuiltInOp = a_Parser.m_bBuiltInOp;
  m_vStringBuf = a_Parser.m_vStringBuf;
  m_pTokenReader.reset(a_Parser.m_pTokenReader->Clone(this));
  m_pClassReader.reset(a_Parser.m_pClassReader->Clone(this));
  m_pClassFunReader.reset(a_Parser.m_pClassFunReader->Clone(this));
  m_StrVarDef = a_Parser.m_StrVarDef;
  m_vStringVarBuf = a_Parser.m_vStringVarBuf;

  m_FunDef = a_Parser.m_FunDef;
  m_PostOprtDef = a_Parser.m_PostOprtDef;
  m_InfixOprtDef = a_Parser.m_InfixOprtDef;

  m_sNameChars = a_Parser.m_sNameChars;
  m_sOprtChars = a_Parser.m_sOprtChars;
  m_sInfixOprtChars = a_Parser.m_sInfixOprtChars;

}

void ParserBase::InitTokenReader()
{
   m_pTokenReader.reset(new token_reader_type(this));
}
void ParserBase::InitClassReader()
{
	m_pClassReader.reset(new ParserClassReader(this));
}
void ParserBase::InitClassFunReader()
{
    m_pClassFunReader.reset(new ParserClassFunctionReader(this));
}

void ParserBase::ReInit() const
{

  m_pParseFormula = &ParserBase::ParseString;
  m_vStringBuf.clear();
  m_vByteCode.clear();
  m_pTokenReader->ReInit();
  const_cast<ParserBase*>(this)->m_bstoprun = false;
}

void ParserBase::EndExpress()const
{
  m_vStringBuf.clear();
  m_vByteCode.clear();
  m_pTokenReader->EndExpress();
}

void ParserBase::AddValIdent(identfun_type a_pCallback)
{
  m_pTokenReader->AddValIdent(a_pCallback);

  m_pClassFunReader->AddValIdent(a_pCallback);
}

void ParserBase::SetVarFactory(facfun_type a_pFactory,void * a_pvoid)
{
	m_pTokenReader->SetVarCreator(a_pFactory);
	m_pTokenReader->SetClassUing(a_pvoid);
}

void ParserBase::AddCallback( const string_type &a_strName,
                              const ParserCallback &a_Callback,
                              funmap_type &a_Storage,
                              const char_type *a_szCharSet )
{
  if (a_Callback.GetAddr()==0)
      Error(ecINVALID_FUN_PTR);

  const funmap_type *pFunMap = &a_Storage;

  if ( pFunMap!=&m_FunDef && m_FunDef.find(a_strName)!=m_FunDef.end() )
    Error(ecNAME_CONFLICT);

  if ( pFunMap!=&m_PostOprtDef && m_PostOprtDef.find(a_strName)!=m_PostOprtDef.end() )
    Error(ecNAME_CONFLICT);

  if ( pFunMap!=&m_InfixOprtDef && pFunMap!=&m_OprtDef && m_InfixOprtDef.find(a_strName)!=m_InfixOprtDef.end() )
    Error(ecNAME_CONFLICT);

  if ( pFunMap!=&m_InfixOprtDef && pFunMap!=&m_OprtDef && m_OprtDef.find(a_strName)!=m_OprtDef.end() )
    Error(ecNAME_CONFLICT);

  CheckName(a_strName, a_szCharSet);
  a_Storage[a_strName] = a_Callback;
  ReInit();
}

void ParserBase::CheckName(const string_type &a_sName,
                           const string_type &a_szCharSet) const
{
  if ( !a_sName.length() ||
       (a_sName.find_first_not_of(a_szCharSet)!=string_type::npos) ||
       (a_sName[0]>='0' && a_sName[0]<='9'))
  {
    Error(ecINVALID_NAME);
  }
}

void ParserBase::SetExpr(const string_type &a_sExpr)
{

  string_type sBuf(a_sExpr + _T(" ") );
  m_bHasControlFlow =
      ContainsControlFlowKeyword(a_sExpr, _T("if")) ||
      ContainsControlFlowKeyword(a_sExpr, _T("else")) ||
      ContainsControlFlowKeyword(a_sExpr, _T("while")) ||
      ContainsControlFlowKeyword(a_sExpr, _T("return")) ||
      (a_sExpr.find(_T("{")) != string_type::npos) ||
      (a_sExpr.find(_T("}")) != string_type::npos);
  m_pTokenReader->SetFormula(sBuf);

  ReInit();
}

void ParserBase::DefineInfixOprt(const string_type &a_sName,
                                 fun_type1 a_pFun,
                                 int a_iPrec,
                                 bool a_bAllowOpt)
{
  AddCallback( a_sName,
               ParserCallback(a_pFun, a_bAllowOpt, a_iPrec, cmOPRT_INFIX),
               m_InfixOprtDef,
               ValidOprtChars() );
}

void ParserBase::DefineGetAdress(const string_type &a_sName,
								 fun_lptype a_pFun,
								 int a_iPrec,
								 bool a_bAllowOpt)
{
	AddCallback( a_sName,
		ParserCallback(a_pFun, a_bAllowOpt, a_iPrec, cmOPRT_INFIX),
		m_InfixOprtDef,
		ValidOprtChars() );
}

void ParserBase::DefinePostfixOprt(const string_type &a_sName,
                                   fun_type1 a_pFun,
                                   bool a_bAllowOpt)
{
  AddCallback( a_sName,
               ParserCallback(a_pFun, a_bAllowOpt, prPOSTFIX, cmOPRT_POSTFIX),
               m_PostOprtDef,
               ValidOprtChars() );
}

void ParserBase::DefineOprt( const string_type &a_sName,
                             fun_type2 a_pFun,
                             unsigned a_iPrec,
                             bool a_bAllowOpt )
{

  for (int i=0; m_bBuiltInOp && i<cmCOMMA; ++i)
    if (a_sName == string_type(c_DefaultOprt[i]))
      Error(ecBUILTIN_OVERLOAD);

  AddCallback( a_sName,
               ParserCallback(a_pFun, a_bAllowOpt, a_iPrec, cmOPRT_BIN),
               m_OprtDef,
               ValidOprtChars() );
}

void ParserBase::DefineStrConst(const string_type &a_strName, const string_type &a_strVal)
{

  if (m_StrVarDef.find(a_strName)!=m_StrVarDef.end())
    Error(ecNAME_CONFLICT);

  CheckName(a_strName, ValidNameChars());

  m_vStringVarBuf.push_back(a_strVal);

  m_StrVarDef[a_strName] = m_vStringBuf.size();

  ReInit();
}

void ParserBase::DefineVar(const string_type &a_sName, value_type *a_pVar)
{
  if (a_pVar==0)
    Error(ecINVALID_VAR_PTR);

  if (m_ConstDef.find(a_sName)!=m_ConstDef.end())
    Error(ecNAME_CONFLICT);

  if (m_FunDef.find(a_sName)!=m_FunDef.end())
    Error(ecNAME_CONFLICT);

  CheckName(a_sName, ValidNameChars());
  m_VarDef[a_sName] = a_pVar;
  ReInit();
}

void ParserBase::DefineConst(const string_type &a_sName, value_type a_fVal)
{
  CheckName(a_sName, ValidNameChars());
  m_ConstDef[a_sName] = a_fVal;
  ReInit();
}

int ParserBase::GetOprtPri(const token_type &a_Tok) const
{
  switch (a_Tok.GetCode())
  {

    case cmEND:        return -5;
	case cmCOMMA:      return -4;
    case cmBO :
    case cmBC :        return -2;
    case cmASSIGN:     return -1;
    case cmAND:
    case cmXOR:
    case cmOR:         return  prLOGIC;
    case cmLT :
    case cmGT :
    case cmLE :
    case cmGE :
    case cmNEQ:
    case cmEQ :        return  prCMP;
    case cmADD:
    case cmSUB:        return  prADD_SUB;
    case cmMUL:
    case cmDIV:        return  prMUL_DIV;
    case cmPOW:        return  prPOW;

    case cmOPRT_INFIX:
    case cmOPRT_BIN:   return a_Tok.GetPri();
    default:  Error(ecINTERNAL_ERROR, 5);
              return 999;
  }
}

const varmap_type& ParserBase::GetUsedVar() const
{
  try
  {
    m_pTokenReader->IgnoreUndefVar(true);

    ParseString();

    m_pTokenReader->IgnoreUndefVar(false);
  }
  catch(exception_type &e)
  {
    m_pTokenReader->IgnoreUndefVar(false);
    throw e;
  }

  m_pParseFormula = &ParserBase::ParseString;

  return m_pTokenReader->GetUsedVar();
}

const varmap_type& ParserBase::GetVar() const
{
  return m_VarDef;
}

const classbasemap_type& ParserBase::GetClassMap() const
{
	return m_ClassDefMap;
}
const string_type& ParserBase::GetFormula() const
{
	return m_StringFormula;
}

const valmap_type& ParserBase::GetConst() const
{
   return m_ConstDef;
}

const funmap_type& ParserBase::GetFunDef() const
{
  return m_FunDef;
}

const string_type& ParserBase::GetExpr() const
{
  return m_pTokenReader->GetFormula();
}

ParserBase::token_type ParserBase::ApplyNumFunc( const token_type &a_FunTok,
                                                 const std::vector<token_type> &a_vArg) const
{
  token_type  valTok;
  int  iArgCount = (unsigned)a_vArg.size();
  void  *pFunc = a_FunTok.GetFuncAddr();
  assert(pFunc);

  switch(a_FunTok.GetArgCount())
  {
	case -1:

			{
		    if (iArgCount==0)
				Error(ecTOO_FEW_PARAMS, m_pTokenReader->GetPos(), a_FunTok.GetAsString());

				std::vector<value_type> vArg;
				for (int i=0; i<iArgCount; ++i)
				  vArg.push_back(a_vArg[i].GetVal());
				valTok.SetVal( ((multfun_type)a_FunTok.GetFuncAddr())(&vArg[0], (int)vArg.size()) );
			}
			break;
	case 1:
			valTok.SetVal( ((fun_type1)a_FunTok.GetFuncAddr())(a_vArg[0].GetVal()));
			break;
	case 2:
			valTok.SetVal( ((fun_type2)a_FunTok.GetFuncAddr())(a_vArg[1].GetVal(),
																 a_vArg[0].GetVal()) );
			break;
	case 3:
			valTok.SetVal( ((fun_type3)a_FunTok.GetFuncAddr())(a_vArg[2].GetVal(),
																   a_vArg[1].GetVal(),
																	a_vArg[0].GetVal()) );
			break;
	case 4:
			valTok.SetVal( ((fun_type4)a_FunTok.GetFuncAddr())(a_vArg[3].GetVal(),
															   a_vArg[2].GetVal(),
                                                               a_vArg[1].GetVal(),
															   a_vArg[0].GetVal()));
			break;
	case 5:
			valTok.SetVal( ((fun_type5)a_FunTok.GetFuncAddr())(a_vArg[4].GetVal(),
															   a_vArg[3].GetVal(),
															   a_vArg[2].GetVal(),
                                                               a_vArg[1].GetVal(),
															   a_vArg[0].GetVal()));
			break;
	default:
			Error(ecINTERNAL_ERROR, 6);
  }

  bool bVolatile = a_FunTok.IsFlagSet(token_type::flVOLATILE);
  for (int i=0; (bVolatile==false) && (i<iArgCount); ++i)
	  bVolatile |= a_vArg[i].IsFlagSet(token_type::flVOLATILE);

  if (bVolatile)
    valTok.AddFlags(token_type::flVOLATILE);

#if defined(_MSC_VER)
  #pragma warning( disable : 4311 )
#endif

#ifndef nodefCmdCode
if(true == m_bcmd)
{

  if ( m_bOptimize &&
	  !valTok.IsFlagSet(token_type::flVOLATILE) &&
	  !a_FunTok.IsFlagSet(token_type::flVOLATILE) )
  {

      if(a_FunTok.GetCode()!=cmOPRT_INFIX)
      {
          m_vByteCode.RemoveValEntries(iArgCount);
          m_vByteCode.AddVal( valTok.GetVal() );
      }
  }
  else
  {

	  m_vByteCode.AddFun(pFunc, (a_FunTok.GetArgCount()==-1) ? -iArgCount : iArgCount);
  }
}
#endif
  return valTok;

#if defined(_MSC_VER)
  #pragma warning( default : 4311 )
#endif
}

ParserBase::token_type ParserBase::ApplyStrFunc(const token_type &a_FunTok,
                                                token_type &a_Arg) const
{
  if (a_Arg.GetCode()!=cmSTRING)
    Error(ecSTRING_EXPECTED, m_pTokenReader->GetPos(), a_FunTok.GetAsString());

  strfun_type1 pFunc = (strfun_type1)a_FunTok.GetFuncAddr();
  assert(pFunc);

  value_type fResult = pFunc( a_Arg.GetAsString().c_str() );
#ifndef nodefCmdCode
if(true == m_bcmd)
{

  if ( m_bOptimize &&
       !a_FunTok.IsFlagSet(token_type::flVOLATILE) )
	{
	    m_vByteCode.AddVal( fResult );
	}
	else
	{

		m_vByteCode.AddStrFun((void*)pFunc, a_FunTok.GetArgCount(), a_Arg.GetIdx());
	}
}
#endif
	a_Arg.SetVal(fResult);
	return a_Arg;
}

void ParserBase::ApplyFunc( ParserStack<token_type> &a_stOpt,
                            ParserStack<token_type> &a_stVal,
                            ParserStack<token_type> &a_classobj,
                            int a_iArgCount) const
{
  assert(m_pTokenReader.get());

 bool bVolatile;
  if (a_stOpt.empty() )
	  return;
  if(a_stOpt.top().GetFuncAddr()==0
	  &&a_stOpt.top().GetCode()!=cmClassFuc)
	  return;
  token_type funTok = a_stOpt.pop();

  if(cmClassFuc!=funTok.GetCode())
	{

		int iArgCount = ( funTok.GetCode()==cmOPRT_BIN ) ? funTok.GetArgCount() : a_iArgCount;

		if (funTok.GetArgCount()>0 && iArgCount>funTok.GetArgCount())
			Error(ecTOO_MANY_PARAMS, m_pTokenReader->GetPos()-1, funTok.GetAsString());

		if ( funTok.GetCode()!=cmOPRT_BIN && iArgCount<funTok.GetArgCount() )
			Error(ecTOO_FEW_PARAMS, m_pTokenReader->GetPos()-1, funTok.GetAsString());

		std::vector<token_type> stArg;
		for (int i=0; i<iArgCount; ++i)
		{
			stArg.push_back( a_stVal.pop() );
			if ( stArg.back().GetType()==tpSTR && funTok.GetType()!=tpSTR )
				Error(ecVAL_EXPECTED, m_pTokenReader->GetPos(), funTok.GetAsString());
		}

		switch(funTok.GetType())
		{
		case tpSTR:
			a_stVal.push(ApplyStrFunc(funTok,stArg.back()));
			break;
		case tpDBL:
			a_stVal.push(ApplyNumFunc(funTok, stArg));
			break;
		case tpVOID:
			break;
		}
	}
	else
	{
		std::vector<token_type> stArg;

		 token_type  valTok,classobjTok;
		 value_type avalue;
		 int  iArgCount =  funTok.GetClassFucArgCount();
		 iArgCount = (iArgCount == -1) ? a_iArgCount : iArgCount;
		 int iClassObjCount = (unsigned)a_classobj.size();
		 classbase  *pclass =funTok.m_pClass;
		 void *pobj = funTok.m_pClassObj;
		 assert(pclass);

		 paramvect vArg;
		 voidparamvect voidpArg;
		 charpvect charpArg;

		 switch(funTok.GetClassFucArgType())
		{
		 case Param_none:
		 case Param_double_1:
		 case Param_double_2:
		 case Param_double_3:
		 case Param_double_4:
		 case Param_int_1:
		 case Param_int_2:
		 case Param_int_3:
		 case Param_int_4:
		 case Param_int_5:
		 case Param_int_6:
		 case Param_int_7:
		 case Param_any:
			 for (int i=0; i<iArgCount; ++i)
			 {
				 valTok=a_stVal.pop();

				 avalue=valTok.GetVal();
				 vArg.push_back( avalue);
			 }
			 std::reverse(vArg.begin(), vArg.end());
			 if(false==m_bprecompile)
			 pclass->ApplyClassFunc(pobj,funTok.GetAsString(),vArg);

			 {

#ifndef nodefCmdCode
			if(true == m_bcmd)
				 m_vByteCode.AddClassMemberFunNum(pclass,
												pobj,
												pclass->GetClassFuncLP(funTok.GetAsString()),
											iArgCount);
#endif
			 }

			 break;
		 case Param_charp_1:
		 case Param_charp_any:
		 case Param_charp_any_Return_int:
		 case Param_charp_any_Return_double:
			for(int i=0;i<iArgCount;++i)
			{
				valTok=a_stVal.pop();
				stArg.push_back( valTok );
				if ( stArg.back().GetType()!=tpSTR )
					Error(ecVAL_EXPECTED, m_pTokenReader->GetPos(), funTok.GetAsString());
				charpArg.push_back(valTok.GetAsString());
			}
			 if(funTok.GetClassFucArgType()!=Param_charp_1)
				 std::reverse(charpArg.begin(), charpArg.end());
			 if(false==m_bprecompile)
			 {
				 if(funTok.GetClassFucArgType()==Param_charp_any_Return_int ||
					funTok.GetClassFucArgType()==Param_charp_any_Return_double)
				 {
					 valTok.SetVal(pclass->ApplyClassFunc(pobj,funTok.GetAsString(),charpArg));
					 a_stVal.push(valTok);
				 }
				 else
				 {
					 pclass->ApplyClassFunc(pobj,funTok.GetAsString(),charpArg);
				 }
			 }

			 {

#ifndef nodefCmdCode
if(true == m_bcmd)
				 m_vByteCode.AddClassMemberFunStr(pclass,
					 pobj,
					 pclass->GetClassFuncLP(funTok.GetAsString()),
					 iArgCount,stArg.back().GetIdx());
#endif
			 }
			 break;
		 case Param_double_charp_2:
		 case Param_double_charp_2_Return_double:
			 {
				 token_type arg_string = a_stVal.pop();
				 token_type arg_number = a_stVal.pop();
				 if (arg_string.GetType()!=tpSTR)
					 Error(ecVAL_EXPECTED, m_pTokenReader->GetPos(), funTok.GetAsString());
				 vArg.push_back(arg_number.GetVal());
				 charpArg.push_back(arg_string.GetAsString());
				 if(false==m_bprecompile)
				 {
					 if(funTok.GetClassFucArgType()==Param_double_charp_2_Return_double)
					 {
						 valTok.SetVal(pclass->ApplyClassFunc(pobj,funTok.GetAsString(),vArg, charpArg));
						 a_stVal.push(valTok);
					 }
					 else
					 {
						 pclass->ApplyClassFunc(pobj,funTok.GetAsString(),vArg, charpArg);
					 }
				 }
			 }
			 break;

		 case Param_voidp_1:
				{
					classobjTok = a_classobj.pop();
					void *ptheobj = classobjTok.GetClassObj();
						voidpArg.push_back(ptheobj);
					if(false==m_bprecompile)
						pclass->ApplyClassFunc(pobj,funTok.GetAsString(),voidpArg);

#ifndef nodefCmdCode
if(true == m_bcmd)
{					classbase *pvoidpclass = classobjTok.GetClass();
					int istacknum = pvoidpclass->findstacknum(ptheobj);
					m_vByteCode.AddClassMemberFunVoidp(pclass,
						pobj,
						pclass->GetClassFuncLP(funTok.GetAsString()),
						istacknum,
						ptheobj);
}
#endif
				}
				break;
		 case Param_voidp_2:
		 case Param_voidp_3:
		 case Param_voidp_4:
		 case Param_voidp_5:

			 for (int i=0; i<iArgCount; ++i)
			 {
				 classobjTok = a_classobj.pop();
				 void *ptheobj = classobjTok.GetClassObj();
				 voidpArg.push_back(ptheobj);
			 }
			 std::reverse(voidpArg.begin(), voidpArg.end());
			 if(false==m_bprecompile)
				 pclass->ApplyClassFunc(pobj,funTok.GetAsString(),voidpArg);

			 break;
		 case Param_voidp_1_Return_double:
				 {
					 voidpArg.push_back((a_classobj.pop()).GetClassObj());
				 }
				 if(false==m_bprecompile)
					 valTok.SetVal(pclass->ApplyClassFunc(pobj,funTok.GetAsString(),voidpArg));
				 a_stVal.push(valTok);
			 break;
		 case Param_0_Return_int:
		 case Param_0_Return_double:
		 case Param_int_1_Return_int:
		 case Param_int_2_Return_int:
		 case Param_int_1_Return_double:
		 case Param_int_2_Return_double:
		 case Param_int_3_Return_double:
		 case Param_any_Return_int:
		 case Param_any_Return_double:

			 for (int i=0; i<iArgCount; ++i)
				{
					valTok=a_stVal.pop();
					avalue=valTok.GetVal();
					vArg.push_back( avalue);
				}
			 std::reverse(vArg.begin(), vArg.end());
			 if(false==m_bprecompile)
			 valTok.SetVal(pclass->ApplyClassFunc(pobj,funTok.GetAsString(),vArg));
			 a_stVal.push(valTok);

			 break;
		 case Param_0_Return_charp:
			 {

			 string_type astring(pclass->ApplyClassFuncString(pobj,funTok.GetAsString()));
			valTok.SetString(astring,astring.size());
			a_stVal.push(valTok);
			 }

			break;
		 default:
			 Error(ecINTERNAL_ERROR, 6);
			}

		 bool bVolatile = funTok.IsFlagSet(token_type::flVOLATILE);

		 if (bVolatile)
			 valTok.AddFlags(token_type::flVOLATILE);
	}

}

void ParserBase::ApplyBinOprt( ParserStack<token_type> &a_stOpt,
                               ParserStack<token_type> &a_stVal,
							   ParserStack<token_type> &a_classobj) const
{
  assert(a_stOpt.size());

  if (a_stOpt.top().GetCode()==cmOPRT_BIN)
  {
     ApplyFunc(a_stOpt, a_stVal, a_classobj,2);
  }
  else
  {

    MUP_ASSERT(a_stVal.size()>=2||a_classobj.size()>=2);

	if(a_stVal.size()>=2)
	{
		token_type valTok1 = a_stVal.pop(),
				   valTok2 = a_stVal.pop(),
				   optTok = a_stOpt.pop(),
				   resTok;

		if ( valTok1.GetType()!=valTok2.GetType() ||
			 (valTok1.GetType()==tpSTR && valTok2.GetType()==tpSTR) )
		  Error(ecOPRT_TYPE_CONFLICT, m_pTokenReader->GetPos(), optTok.GetAsString());

		value_type x = valTok2.GetVal(),
					 y = valTok1.GetVal();

		switch (optTok.GetCode())
		{

		  case cmAND: resTok.SetVal( (int)x & (int)y ); break;
		  case cmOR:  resTok.SetVal( (int)x | (int)y ); break;
		  case cmXOR: resTok.SetVal( (int)x ^ (int)y ); break;
		  case cmLT:  resTok.SetVal( x < y ); break;
		  case cmGT:  resTok.SetVal( x > y ); break;
		  case cmLE:  resTok.SetVal( x <= y ); break;
		  case cmGE:  resTok.SetVal( x >= y ); break;
		  case cmNEQ: resTok.SetVal( x != y ); break;
		  case cmEQ:  resTok.SetVal( x == y ); break;
		  case cmADD: resTok.SetVal( x + y ); break;
		  case cmSUB: resTok.SetVal( x - y ); break;
		  case cmMUL: resTok.SetVal( x * y ); break;
		  case cmDIV: resTok.SetVal( x / y ); break;
          case cmPOW: resTok.SetVal(pow(x, y)); break;

		  case cmASSIGN:

					{
						if (valTok2.GetCode()!=cmVAR
							||valTok2.GetCode()!=cmVAR)
						Error(ecINTERNAL_ERROR, 7);

					  value_type *pVar = valTok2.GetVar();
					  resTok.SetVal( *pVar = y );
					  a_stVal.push( resTok );
#ifndef nodefCmdCode
				if(true == m_bcmd)
					  m_vByteCode.AddAssignOp(pVar);
#endif
					  return;

					}

		  default:  Error(ecINTERNAL_ERROR, 8);
		}
#ifndef nodefCmdCode
if(true == m_bcmd)
{

		if (!m_bOptimize)
		{

		  m_vByteCode.AddOp(optTok.GetCode());

		}
		else if ( valTok1.IsFlagSet(token_type::flVOLATILE) ||
				  valTok2.IsFlagSet(token_type::flVOLATILE) )
		{

		  m_vByteCode.AddOp(optTok.GetCode());
		  resTok.AddFlags(token_type::flVOLATILE);
		}
		else
		{

		  m_vByteCode.RemoveValEntries(2);
		  m_vByteCode.AddVal(resTok.GetVal());
		}
}
#endif
		a_stVal.push( resTok );
	}
	else if(a_classobj.size()>=2)
	{
		token_type objTok1 = a_classobj.pop(),
			objTok2 = a_classobj.pop(),
			optTok = a_stOpt.pop(),
			resTok;

		if ( objTok1.GetType()!=objTok2.GetType() ||
			(objTok1.GetType()==tpSTR && objTok2.GetType()==tpSTR) )
			Error(ecOPRT_TYPE_CONFLICT, m_pTokenReader->GetPos(), optTok.GetAsString());

		void* x = objTok2.GetClassObj();
		void* y = objTok1.GetClassObj();

		switch (optTok.GetCode())
		{

		case cmAND:
			break;
		case cmOR:
			break;
		case cmXOR:
			break;
		case cmLT:
			break;
		case cmGT:
			break;
		case cmLE:
			break;
		case cmGE:
			break;
		case cmNEQ:
			break;
		case cmEQ:
			break;
		case cmADD:
			break;
		case cmSUB:
			break;
		case cmMUL:
			break;
		case cmDIV:
			break;
		case cmPOW:
			break;
		case cmASSIGN:

			{
				if (objTok2.GetCode()!=cmClassObj
					||objTok2.GetCode()!=cmClassObj)
					Error(ecINTERNAL_ERROR, 7);
				if(objTok2.m_pClass != objTok2.m_pClass)
					Error(ecINTERNAL_ERROR, 7);
				void * pObj = objTok2.GetClassObj();
				pObj = y;
				resTok.SetClassVar(objTok2.m_pClass, pObj );
				a_classobj.push( resTok );

				return;

			}

			default:  Error(ecINTERNAL_ERROR, 8);
		}

		a_classobj.push( resTok );
	}

  }
}

value_type ParserBase::ParseCmdCode() const
{
#if defined(_MSC_VER)
  #pragma warning( disable : 4312 )
#endif

	paramvect vArg;
	voidparamvect voidpArg;
	charpvect charpArg;

  value_type Stack[99];
  ECmdCode iCode;
  bytecode_type idx(0);
  int i(0);

  __start:

  idx = m_pCmdCode[i];
  iCode = (ECmdCode)m_pCmdCode[i+1];
  i += 2;

#ifdef _DEBUG
  if (idx>=99)
    throw exception_type(ecGENERIC, "", m_pTokenReader->GetFormula(), -1);
#endif

  switch (iCode)
  {

    case cmAND: Stack[idx]  = (int)Stack[idx] & (int)Stack[idx+1]; goto __start;
    case cmOR:  Stack[idx]  = (int)Stack[idx] | (int)Stack[idx+1]; goto __start;
    case cmXOR: Stack[idx]  = (int)Stack[idx] ^ (int)Stack[idx+1]; goto __start;
    case cmLE:  Stack[idx]  = Stack[idx] <= Stack[idx+1]; goto __start;
    case cmGE:  Stack[idx]  = Stack[idx] >= Stack[idx+1]; goto __start;
    case cmNEQ: Stack[idx]  = Stack[idx] != Stack[idx+1]; goto __start;
    case cmEQ:  Stack[idx]  = Stack[idx] == Stack[idx+1]; goto __start;
	case cmLT:  Stack[idx]  = Stack[idx] < Stack[idx+1];  goto __start;
	case cmGT:  Stack[idx]  = Stack[idx] > Stack[idx+1];  goto __start;
    case cmADD: Stack[idx] += Stack[1+idx]; goto __start;
	case cmSUB: Stack[idx] -= Stack[1+idx]; goto __start;
	case cmMUL: Stack[idx] *= Stack[1+idx]; goto __start;
	case cmDIV: Stack[idx] /= Stack[1+idx]; goto __start;
    case cmPOW: Stack[idx]  = pow(Stack[idx], Stack[1+idx]); goto __start;

    case cmASSIGN:
	{

		value_type **pDest = (value_type**)(&m_pCmdCode[i]);

		i += m_vByteCode.GetPtrSize();

		Stack[idx] = **pDest = Stack[idx+1];
	}
	goto __start;

    case cmOPRT_BIN:
		Stack[idx] = (**(fun_type2**)(&m_pCmdCode[i]))(Stack[idx], Stack[idx+1]);
		++i;
	goto __start;

	case cmVAR:
		Stack[idx] = **(value_type**)(&m_pCmdCode[i]);
		i += m_vByteCode.GetValSize();
	goto __start;

	case cmVAL:
	Stack[idx] = *(value_type*)(&m_pCmdCode[i]);
	i += m_vByteCode.GetValSize();
	goto __start;

    case cmFUNC_STR:
	{
		i++;
		strfun_type1 pFun = *(strfun_type1*)(&m_pCmdCode[i]);
		i += m_vByteCode.GetPtrSize();

		int iIdxStack = (int)m_pCmdCode[i++];

		MUP_ASSERT( iIdxStack>=0 && iIdxStack<(int)m_vStringBuf.size() );
		Stack[idx] = pFun(m_vStringBuf[iIdxStack].c_str());
	}
	goto __start;

    case cmFUNC:
    {
		int iArgCount = (int)m_pCmdCode[i++];
		switch(iArgCount)
		{
			case 1: Stack[idx] = (*(fun_type1*)(&m_pCmdCode[i]))(Stack[idx]); break;
			case 2: Stack[idx] = (*(fun_type2*)(&m_pCmdCode[i]))(Stack[idx], Stack[idx+1]); break;
			case 3: Stack[idx] = (*(fun_type3*)(&m_pCmdCode[i]))(Stack[idx], Stack[idx+1], Stack[idx+2]); break;
			case 4: Stack[idx] = (*(fun_type4*)(&m_pCmdCode[i]))(Stack[idx], Stack[idx+1], Stack[idx+2], Stack[idx+3]); break;
			case 5: Stack[idx] = (*(fun_type5*)(&m_pCmdCode[i]))(Stack[idx], Stack[idx+1], Stack[idx+2], Stack[idx+3], Stack[idx+4]); break;
			default:
			if (iArgCount>0)
			Error(ecINTERNAL_ERROR, 1);

			Stack[idx] =(*(multfun_type*)(&m_pCmdCode[i]))(&Stack[idx], -iArgCount);
			break;
		}
		i += m_vByteCode.GetPtrSize();
	}
    goto __start;
	case cmClassFucNum:
		{
			int iArgCount = (int)m_pCmdCode[i++];
			vArg.clear();
			for (int inum=0; inum<iArgCount; ++inum)
			{
				vArg.push_back( Stack[idx+iArgCount-inum-1]);
			}
			const int iPtrSize = m_vByteCode.GetPtrSize();
			classbase *pclass = ReadPackedBytecodeItem<classbase *>(&m_pCmdCode[i]);
			i += iPtrSize;
			void *pobj = ReadPackedBytecodeItem<void *>(&m_pCmdCode[i]);
			i += iPtrSize;
			void *pfunc = ReadPackedBytecodeItem<void *>(&m_pCmdCode[i]);
			i += iPtrSize;
			Stack[idx]= pclass->ApplyClassFunc(pobj,pfunc,vArg);
			++i;

		}
		goto __start;
	case cmClassFucStr:
		{

			int iArgCount = (int)m_pCmdCode[i++];
			int iIdxStack;
			charpArg.clear();
			for (int inum=0; inum<iArgCount; ++inum)
			{
				iIdxStack = (int)m_pCmdCode[i + 3 * m_vByteCode.GetPtrSize()];
				charpArg.push_back(m_vStringBuf[iIdxStack].c_str());
			}
			const int iPtrSize = m_vByteCode.GetPtrSize();
			classbase *pclass = ReadPackedBytecodeItem<classbase *>(&m_pCmdCode[i]);
			i += iPtrSize;
			void *pobj = ReadPackedBytecodeItem<void *>(&m_pCmdCode[i]);
			i += iPtrSize;
			void *pfunc = ReadPackedBytecodeItem<void *>(&m_pCmdCode[i]);
			i += iPtrSize;
			Stack[idx]= pclass->ApplyClassFunc(pobj,pfunc,charpArg);
			i += 2;

		}
		goto __start;
	case cmClassFucVp:
		{

			int iArgCount = (int)m_pCmdCode[i++];
			void * ptheobj;
			voidpArg.clear();
			for (int inum=0; inum<iArgCount; ++inum)
			{
				ptheobj = ReadPackedBytecodeItem<void *>(&m_pCmdCode[i + 3 * m_vByteCode.GetPtrSize()]);
				voidpArg.push_back(ptheobj);
			}
			const int iPtrSize = m_vByteCode.GetPtrSize();
			classbase *pclass = ReadPackedBytecodeItem<classbase *>(&m_pCmdCode[i]);
			i += iPtrSize;
			void *pobj = ReadPackedBytecodeItem<void *>(&m_pCmdCode[i]);
			i += iPtrSize;
			void *pfunc = ReadPackedBytecodeItem<void *>(&m_pCmdCode[i]);
			i += iPtrSize;
			Stack[idx]= pclass->ApplyClassFunc(pobj,pfunc,voidpArg);
			i += iPtrSize + 1;

		}
		goto __start;
	case cmEND:
		return Stack[1];
	default:
		Error(ecINTERNAL_ERROR, 2);
    return 0;
  }

#if defined(_MSC_VER)
  #pragma warning( default : 4312 )
#endif
}

value_type ParserBase::ParseValue() const
{
  return *(value_type*)(&m_pCmdCode[2]);
}

value_type ParserBase::ParseString() const
{
	#if defined(_MSC_VER)
	#pragma warning( disable : 4311 )
	#endif
	if(!m_pTokenReader->GetFormula().length())
		Error(ecUNEXPECTED_EOF, 0);

	ParserStack<token_type> stOpt, stVal,stClassObj;

	ParserStack<int> stArgCount;
	token_type opta, opt ,opt1,opt2;
	token_type * popt;
	token_type val, tval;
	string_type strBuf;

	ParserStack<token_type> stOptStack;

	token_type ObjectTok;

	classbase  *pclass;
	void *pobj ;
	value_type avalue;

    std::deque<int> stOptStack_Conditionstorage;
    std::deque<int> stOptStack_prebracketstorage;

	ReInit();

	int icuroptid = 0;

	int iprelbid ;
	int iconditionid ;

	for(;;)
	{
        opt = m_pTokenReader->ReadNextToken();

		opt.SetRunIndex(icuroptid);

		if(opt.GetCode() == cmIf
		|| opt.GetCode() == cmElse
		|| opt.GetCode() == cmWhile )
		{
             stOptStack_Conditionstorage.push_front(icuroptid);

		}
		if(opt.GetCode() == cmLB  )
		{
             stOptStack_prebracketstorage.push_front(icuroptid);
		}
		if(opt.GetCode() == cmRB  )
		{

			if(stOptStack_prebracketstorage.size()==stOptStack_Conditionstorage.size())
			{
                iconditionid =  stOptStack_Conditionstorage[0];
                stOptStack_Conditionstorage.pop_front();
				if(cmWhile==stOptStack[iconditionid].GetCode())
				{
					opt.SetGotoIndex(iconditionid);
				}
			}
            iprelbid =  stOptStack_prebracketstorage[0];
            stOptStack_prebracketstorage.pop_front();

			popt = stOptStack.GetAt(iprelbid);
			popt->SetGotoIndex(icuroptid+1);

		}

		stOptStack.push(opt);

		icuroptid ++;

		if( opt.GetCode() == cmEND )
		{
		  break;
		}
	}

	if(true==m_boptcollect)
	{
		m_OptStack.clear();
		m_OptStack = stOptStack;
	}

	std::stack<int>  stopt_storage;
	std::stack<int>  stval_storage;
	std::stack<int>  stClassObj_storage;
	int ioptid =0;

	ECmdCode iGetCode;
	int igotoindex =0;
	int icurstep = 0;
	for(;;)
	{
		if(true==m_bstoprun)
		{
			#ifndef nodefCmdCode
			if(true == m_bcmd)
				m_vByteCode.Finalize();
			#endif
				break;
		}

		opt = stOptStack[icurstep];
		iGetCode=opt.GetCode();
		icurstep++;

		switch (iGetCode)
		{

		  case cmSTRING:
				  opt.SetIdx((int)m_vStringBuf.size());
				  stVal.push(opt);
				  m_vStringBuf.push_back(opt.GetAsString());
				  break;

		  case cmVAR:
				  stVal.push(opt);
				#ifndef nodefCmdCode
				if(true == m_bcmd)
				{

							   m_vByteCode.AddVar( static_cast<value_type*>(opt.GetVar()) );
				}
				#endif
				  break;
		  case cmVARLP:
				  stVal.push(opt);
				#ifndef nodefCmdCode
				if(true == m_bcmd)
				{

							  m_vByteCode.AddVar( static_cast<value_type*>(opt.GetVar()) );
				}
				#endif
				  break;
		  case cmVAL:
				  stVal.push(opt);
				#ifndef nodefCmdCode
				if(true == m_bcmd)
				{

					m_vByteCode.AddVal( opt.GetVal());
				}
				#endif
				  break;
		  case cmCOMMA:
				  if (stArgCount.empty())
					Error(ecUNEXPECTED_COMMA, m_pTokenReader->GetPos());
				  ++stArgCount.top();

		  case cmSEMICOLON:
		  case cmEND:
		  case cmBC:
			  {

				  if (opta.GetCode()==cmBO)
					  --stArgCount.top();
				  {
					while ( stOpt.size() && stOpt.top().GetCode() != cmBO )
					{
						  if (stOpt.top().GetCode()==cmOPRT_INFIX)
						  {
							  ApplyFunc(stOpt, stVal,stClassObj, 1);
						  }
						  else
						  {
							  ApplyBinOprt(stOpt, stVal,stClassObj);
						  }
					}

					if (stOpt.size() && stOpt.top().GetCode()==cmOPRT_INFIX)
					  ApplyFunc(stOpt, stVal,stClassObj, 1);
					if ( opt.GetCode()!=cmBC
						|| stOpt.size()==0
						|| stOpt.top().GetCode()!=cmBO )
						break;

					assert(stArgCount.size());
					int iArgCount = stArgCount.pop();

					stOpt.pop();

					if(stOpt.size()
						&&(stOpt.top().GetCode()==cmWhile))
					{
						avalue = stVal.top().GetVal();

						if(avalue!=0)
						{
							igotoindex = 0;
							stVal.pop();
							stOpt.pop();
						}
						else
						{
							igotoindex = 1;
							stVal.pop();
							stOpt.pop();

						}
						break;
					}
					if(stOpt.size()
						&&(stOpt.top().GetCode()==cmIf))
					{
						avalue = stVal.top().GetVal();

						if(avalue>0)
						{

							igotoindex = 0;
							stVal.pop();
							stOpt.pop();
						}
						else
						{

							igotoindex = 1;
							stVal.pop();
							stOpt.pop();
						}
						break;
					}

					if (iArgCount>1
						&&(stOpt.size()==0 || stOpt.top().GetCode()!=cmFUNC))
						if(stOpt.size()!=0 && stOpt.top().GetCode()!=cmClassFuc)
					  Error(ecUNEXPECTED_ARG, m_pTokenReader->GetPos());

					if (stOpt.size() && stOpt.top().GetCode()!=cmOPRT_INFIX)
					  ApplyFunc(stOpt, stVal,stClassObj, iArgCount);
				  }
				  break;
			  }

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
		  case cmDIV:
		  case cmPOW:
		  case cmASSIGN:
		  case cmOPRT_BIN:

				  while ( stOpt.size() && stOpt.top().GetCode() != cmBO)
				  {
					  if (GetOprtPri(stOpt.top()) < GetOprtPri(opt))
						break;
					if (stOpt.top().GetCode()==cmOPRT_INFIX)
					  ApplyFunc(stOpt, stVal, stClassObj,1);
					else
					  ApplyBinOprt(stOpt, stVal,stClassObj);
				  }

				  stOpt.push(opt);
				  break;

		  case cmBO:
				  stArgCount.push(1);
				  stOpt.push(opt);
    					break;
		  case cmFUNC:
		  case cmFUNC_STR:
		  case cmClassFuc:
		  case cmOPRT_INFIX:
				  stOpt.push(opt);
    					break;
		  case cmOPRT_POSTFIX:
				  stOpt.push(opt);
				  ApplyFunc(stOpt, stVal, stClassObj,1);
				  break;

		  case cmLB:
				if(1==igotoindex)
					icurstep=opt.GetGotoIndex();

				ioptid = stOpt.size();
				ioptid = ioptid >0? ioptid-1:0;
				stopt_storage.push(ioptid);
				ioptid = stVal.size();
				ioptid = ioptid >0? ioptid-1:0;
				stval_storage.push(ioptid);
				ioptid = stClassObj.size();
				ioptid = ioptid >0? ioptid-1:0;
				stClassObj_storage.push(ioptid);

			 break;
		  case cmRB:
			  icurstep=opt.GetGotoIndex()>-1?opt.GetGotoIndex():icurstep;

			  ioptid = stopt_storage.top();
			  for(int i=stOpt.size();i>ioptid;i--)
				  stOpt.pop();

			  ioptid = stopt_storage.top();
			  for(int i=stVal.size();i>ioptid;i--)
				  stVal.pop();

			  ioptid = stClassObj_storage.top();
			  for(int i=stClassObj.size();i>ioptid;i--)
				  stClassObj.pop();

			  stopt_storage.pop();
			  stval_storage.pop();
			  stClassObj_storage.pop();

			break;
		  case cmIf:
		  case cmWhile:
			   stOpt.push(opt);
			  break;
		  case cmElse:

			if(1==igotoindex)
			{
				igotoindex = 0;
			}
			else
			{
				igotoindex = 1;
			}
			break;
		  case cmCallin:
			  break;
		  case cmReturn:
			  #ifndef nodefCmdCode
			  if (true == m_bcmd)
			  {
				  m_vByteCode.Finalize();
			  }
			  #endif
			  const_cast<ParserBase*>(this)->m_bstoprun = true;
			  return 0;
		  case cmClassObjDef:
			{
				pclass= opt.m_pClass;
				pobj=pclass->addvar(opt.GetAsString());
				if(0!=pobj)
					opt.SetClassVarDef(pclass,pobj, opt.GetAsString() );
			}
			  break;
		  case cmClass:

			  break;

		 case cmClassObj:

			  stClassObj.push(opt);

			  break;

		  case cmMember:
			  stClassObj.pop();
			  break;
		  case cmPointer:

			  break;

		  default:
			Error(ecINTERNAL_ERROR, 3);
		}

		if( opt.GetCode() == cmEND )
		{
	#ifndef nodefCmdCode
	if(true == m_bcmd)
	{

			m_vByteCode.Finalize();
	}
	#endif
			break;
		}
	  }

if (m_bstoprun)
		{
			return 0;
		}

#ifndef nodefCmdCode
if(true == m_bcmd)
{

		  m_pCmdCode = m_vByteCode.GetRawData();
}
#endif

		  m_StringFormula=m_pTokenReader->GetFormula();
#ifndef nodefCmdCode
if(true == m_bcmd)
{

		  if(true==m_bcolllection)
		  {
			  m_vByteCodeCollection.SetStorageBase(
				  m_vByteCodeCollection.CollectStorageData(m_vByteCodeCollection.GetStorageBase(),
															m_vByteCode.GetStorageBase())
													);

			  string_type sBul=m_pTokenReader->GetFormula()+"\r\n";
			  m_StrCollection=m_StrCollection+sBul;
		  }
}
#endif

		  if(0!=stVal.size())
		  {

			  if (stVal.size()!=1)
			  {

			  }
			  if (stVal.top().GetType()!=tpDBL)
				  Error(ecSTR_RESULT);

			  value_type fVal = stVal.top().GetVal();

#ifdef nodefCmdCode
              if (m_bUseByteCode && !m_bHasControlFlow)
			  {
				  m_pParseFormula =
					  (m_pCmdCode[1]==cmVAL && m_pCmdCode[6]==cmEND) ?
					  &ParserBase::ParseValue :
				  &ParserBase::ParseCmdCode;
			  }
#endif

			  return fVal;
		  }
		  else
		  {
			  return 0;\
		  }
}

ParserByteCode::storage_type ParserBase::GetStorageBase()
{
	return m_vByteCode.GetStorageBaseData();
}

void  ParserBase::Error(EErrorCodes a_iErrc, int a_iPos, const string_type &a_sTok) const
{
  throw exception_type(a_iErrc, a_sTok, m_pTokenReader->GetFormula(), a_iPos);
}

void ParserBase::ClearVar()
{
  m_VarDef.clear();
  ReInit();
}

void ParserBase::RemoveVar(const string_type &a_strVarName)
{
	varmap_type::iterator item = m_VarDef.find(a_strVarName);
	if (item!=m_VarDef.end())
	{
		m_VarDef.erase(item);
		ReInit();
	}
}

void ParserBase::ClearFormula()
{
  m_vByteCode.clear();
  m_pCmdCode = 0;
  m_pTokenReader->SetFormula("");
  ReInit();
}

void ParserBase::ClearFun()
{
  m_FunDef.clear();
  ReInit();
}

void ParserBase::ClearConst()
{
  m_ConstDef.clear();
  m_StrVarDef.clear();
  ReInit();
}

void ParserBase::ClearPostfixOprt()
{
  m_PostOprtDef.clear();
  ReInit();
}

void ParserBase::ClearOprt()
{
  m_OprtDef.clear();
  ReInit();
}

void ParserBase::ClearClassObj()
{
	classbasemap_type:: iterator pIter;

	for ( pIter = m_ClassDefMap.begin( ) ; pIter != m_ClassDefMap.end(); pIter++ )
	{
		classbase *pbase=pIter->second;
		pbase->clearvar();
	}

}

void ParserBase::RemoveClassObject(const string_type &a_strClassnName)
{
	classbasemap_type:: iterator pIter= m_ClassDefMap.find(a_strClassnName);

	classbase *pbase=pIter->second;

	pbase->clearvar();
}

void ParserBase::RemoveClassObject(const string_type &a_strClassnName,const string_type &a_strOjectName)
{
	classbasemap_type:: iterator pIter= m_ClassDefMap.find(a_strClassnName);

	classbase *pbase=pIter->second;

	pbase->delvar(a_strOjectName);
}

void ParserBase::ClearInfixOprt()
{
  m_InfixOprtDef.clear();
  ReInit();
}

void ParserBase::EnableOptimizer(bool a_bIsOn)
{
  m_bOptimize = a_bIsOn;
  ReInit();
}

void ParserBase::EnableByteCode(bool a_bIsOn)
{
  m_bUseByteCode = a_bIsOn;
  if (!a_bIsOn)
    ReInit();
}

void ParserBase::EnableBuiltInOprt(bool a_bIsOn)
{
  m_bBuiltInOp = a_bIsOn;
  ReInit();
}

bool ParserBase::HasBuiltInOprt() const
{
  return m_bBuiltInOp;
}

value_type  ParserBase::RunCollectionCmdCode() const
{
   int isize= m_vByteCodeCollection.GetRawDataSize();
   if(isize<3)
	   return 0;

   paramvect vArg;
   voidparamvect voidpArg;
   charpvect charpArg;

  m_pCmdCodeCollection=m_vByteCodeCollection.GetRawData();

  value_type Stack[99];
  ECmdCode iCode;
  bytecode_type idx(0);
  int i(0);

  __start:

  idx = m_pCmdCodeCollection[i];
  iCode = (ECmdCode)m_pCmdCodeCollection[i+1];
  i += 2;

#ifdef _DEBUG
  if (idx>=99)
    throw exception_type(ecGENERIC, "", m_pTokenReader->GetFormula(), -1);
#endif

  switch (iCode)
  {

    case cmAND: Stack[idx]  = (int)Stack[idx] & (int)Stack[idx+1]; goto __start;
    case cmOR:  Stack[idx]  = (int)Stack[idx] | (int)Stack[idx+1]; goto __start;
    case cmXOR: Stack[idx]  = (int)Stack[idx] ^ (int)Stack[idx+1]; goto __start;
    case cmLE:  Stack[idx]  = Stack[idx] <= Stack[idx+1]; goto __start;
    case cmGE:  Stack[idx]  = Stack[idx] >= Stack[idx+1]; goto __start;
    case cmNEQ: Stack[idx]  = Stack[idx] != Stack[idx+1]; goto __start;
    case cmEQ:  Stack[idx]  = Stack[idx] == Stack[idx+1]; goto __start;
	case cmLT:  Stack[idx]  = Stack[idx] < Stack[idx+1];  goto __start;
	case cmGT:  Stack[idx]  = Stack[idx] > Stack[idx+1];  goto __start;
    case cmADD: Stack[idx] += Stack[1+idx]; goto __start;
    case cmSUB: Stack[idx] -= Stack[1+idx]; goto __start;
	case cmMUL: Stack[idx] *= Stack[1+idx]; goto __start;
	case cmDIV: Stack[idx] /= Stack[1+idx]; goto __start;
    case cmPOW: Stack[idx]  = pow(Stack[idx], Stack[1+idx]); goto __start;

    case cmASSIGN:
           {

             value_type **pDest = (value_type**)(&m_pCmdCodeCollection[i]);

             i += m_vByteCodeCollection.GetPtrSize();

             Stack[idx] = **pDest = Stack[idx+1];
           }
           goto __start;

    case cmOPRT_BIN:
			Stack[idx] = (**(fun_type2**)(&m_pCmdCodeCollection[i]))(Stack[idx], Stack[idx+1]);
			++i;
			goto __start;

	  case cmVAR:
	        Stack[idx] = **(value_type**)(&m_pCmdCodeCollection[i]);
	        i += m_vByteCodeCollection.GetValSize();
	        goto __start;

	  case cmVAL:
            Stack[idx] = *(value_type*)(&m_pCmdCodeCollection[i]);
              i += m_vByteCodeCollection.GetValSize();
            goto __start;

    case cmFUNC_STR:
            {
              i++;
              strfun_type1 pFun = *(strfun_type1*)(&m_pCmdCodeCollection[i]);
              i += m_vByteCodeCollection.GetPtrSize();

              int iIdxStack = (int)m_pCmdCodeCollection[i++];

              MUP_ASSERT( iIdxStack>=0 && iIdxStack<(int)m_vStringBuf.size() );
              Stack[idx] = pFun(m_vStringBuf[iIdxStack].c_str());
            }
            goto __start;

    case cmFUNC:
		{
		      int iArgCount = (int)m_pCmdCodeCollection[i++];
              switch(iArgCount)
		      {
                case 1: Stack[idx] = (*(fun_type1*)(&m_pCmdCodeCollection[i]))(Stack[idx]); break;
			    case 2: Stack[idx] = (*(fun_type2*)(&m_pCmdCodeCollection[i]))(Stack[idx], Stack[idx+1]); break;
			    case 3: Stack[idx] = (*(fun_type3*)(&m_pCmdCodeCollection[i]))(Stack[idx], Stack[idx+1], Stack[idx+2]); break;
			    case 4: Stack[idx] = (*(fun_type4*)(&m_pCmdCodeCollection[i]))(Stack[idx], Stack[idx+1], Stack[idx+2], Stack[idx+3]); break;
                case 5: Stack[idx] = (*(fun_type5*)(&m_pCmdCodeCollection[i]))(Stack[idx], Stack[idx+1], Stack[idx+2], Stack[idx+3], Stack[idx+4]); break;
                default:
				 if (iArgCount>0)
                  Error(ecINTERNAL_ERROR, 1);
                 Stack[idx] =(*(multfun_type*)(&m_pCmdCodeCollection[i]))(&Stack[idx], -iArgCount);
                  break;
		       }
		          i += m_vByteCodeCollection.GetPtrSize();
		  }
		  goto __start;
	case cmClassFucNum:
		{
			int iArgCount = (int)m_pCmdCodeCollection[i++];
			vArg.clear();
			for (int inum=0; inum<iArgCount; ++inum)
			{
				vArg.push_back( Stack[idx+iArgCount-inum-1]);
			}
			const int iPtrSize = m_vByteCodeCollection.GetPtrSize();
			classbase *pclass = ReadPackedBytecodeItem<classbase *>(&m_pCmdCodeCollection[i]);
			i += iPtrSize;
			void *pobj = ReadPackedBytecodeItem<void *>(&m_pCmdCodeCollection[i]);
			i += iPtrSize;
			void *pfunc = ReadPackedBytecodeItem<void *>(&m_pCmdCodeCollection[i]);
			i += iPtrSize;
			Stack[idx]= pclass->ApplyClassFunc(pobj,pfunc,vArg);
			++i;

		}
		goto __start;
	case cmClassFucStr:
		{

			int iArgCount = (int)m_pCmdCodeCollection[i++];
			int iIdxStack;
			charpArg.clear();
			for (int inum=0; inum<iArgCount; ++inum)
			{
				iIdxStack = (int)m_pCmdCodeCollection[i + 3 * m_vByteCodeCollection.GetPtrSize()];
				charpArg.push_back(m_vStringBuf[iIdxStack].c_str());
			}
			const int iPtrSize = m_vByteCodeCollection.GetPtrSize();
			classbase *pclass = ReadPackedBytecodeItem<classbase *>(&m_pCmdCodeCollection[i]);
			i += iPtrSize;
			void *pobj = ReadPackedBytecodeItem<void *>(&m_pCmdCodeCollection[i]);
			i += iPtrSize;
			void *pfunc = ReadPackedBytecodeItem<void *>(&m_pCmdCodeCollection[i]);
			i += iPtrSize;
			Stack[idx]= pclass->ApplyClassFunc(pobj,pfunc,charpArg);
			i += 2;

		}
		goto __start;
	case cmClassFucVp:
		{

			int iArgCount = (int)m_pCmdCodeCollection[i++];
			void * ptheobj;
			voidpArg.clear();
			for (int inum=0; inum<iArgCount; ++inum)
			{
				ptheobj = ReadPackedBytecodeItem<void *>(&m_pCmdCodeCollection[i + 3 * m_vByteCodeCollection.GetPtrSize()]);
				voidpArg.push_back(ptheobj);
			}
			const int iPtrSize = m_vByteCodeCollection.GetPtrSize();
			classbase *pclass = ReadPackedBytecodeItem<classbase *>(&m_pCmdCodeCollection[i]);
			i += iPtrSize;
			void *pobj = ReadPackedBytecodeItem<void *>(&m_pCmdCodeCollection[i]);
			i += iPtrSize;
			void *pfunc = ReadPackedBytecodeItem<void *>(&m_pCmdCodeCollection[i]);
			i += iPtrSize;
			Stack[idx]= pclass->ApplyClassFunc(pobj,pfunc,voidpArg);
			i += iPtrSize + 1;

		}
		goto __start;
	case cmEND:
		        return Stack[1];

	  default:
            Error(ecINTERNAL_ERROR, 2);
            return 0;
  }
}

value_type ParserBase::RunCollectionOpt() const
{
#if defined(_MSC_VER)
#pragma warning( disable : 4311 )
#endif

	ParserStack<token_type> stOpt, stVal,stClassObj;

	ParserStack<int> stArgCount;
	token_type opta, opt ,opt1,opt2;
	token_type * popt;
	token_type val, tval;
	string_type strBuf;

	token_type ObjectTok;
	token_type *ptoke,*ptoketemp;
	classbase  *pclass;
	void *pobj ;
	value_type avalue;

	ReInit();

	int igetstep = 0;
	int icuroptid = 0;

	int iprelbid ;
	int iconditionid ;

	std::stack<int>  stopt_storage;
	std::stack<int>  stval_storage;
	std::stack<int>  stClassObj_storage;
	int ioptid =0;

	ECmdCode iGetCode;
	int igotoindex =0;
	int icurstep = 0;
	for(;;)
	{
		if(true==m_bstoprun)
			break;

		opt = m_OptStack[icurstep];
		iGetCode=opt.GetCode();
		icurstep++;

		switch (iGetCode)
		{

		case cmSTRING:
			opt.SetIdx((int)m_vStringBuf.size());
			stVal.push(opt);
			m_vStringBuf.push_back(opt.GetAsString());
			break;

		case cmVAR:
			stVal.push(opt);
			break;
		case cmVARLP:
			stVal.push(opt);
			break;
		case cmVAL:
			stVal.push(opt);
			break;
		case cmCOMMA:
			if (stArgCount.empty())
				Error(ecUNEXPECTED_COMMA, m_pTokenReader->GetPos());
			++stArgCount.top();

		case cmSEMICOLON:
		case cmEND:
		case cmBC:
			{

				if (opta.GetCode()==cmBO)
					--stArgCount.top();
				{
					while ( stOpt.size() && stOpt.top().GetCode() != cmBO )
					{
						if (stOpt.top().GetCode()==cmOPRT_INFIX)
						{
							ApplyFunc(stOpt, stVal,stClassObj, 1);
						}
						else
						{
							ApplyBinOprt(stOpt, stVal,stClassObj);
						}
					}

					if (stOpt.size() && stOpt.top().GetCode()==cmOPRT_INFIX)
						ApplyFunc(stOpt, stVal,stClassObj, 1);
					if ( opt.GetCode()!=cmBC
						|| stOpt.size()==0
						|| stOpt.top().GetCode()!=cmBO )
						break;

					assert(stArgCount.size());
					int iArgCount = stArgCount.pop();

					stOpt.pop();

					if(stOpt.size()
						&&(stOpt.top().GetCode()==cmWhile))
					{
						avalue= stVal.top().GetVal();

						if(avalue!=0)
						{
							igotoindex = 0;
							stVal.pop();
							stOpt.pop();
						}
						else
						{
							igotoindex = 1;
							stVal.pop();
							stOpt.pop();

						}
						break;
					}
					if(stOpt.size()
						&&(stOpt.top().GetCode()==cmIf))
					{
						avalue= stVal.top().GetVal();

						if(avalue>0)
						{

							igotoindex = 0;
							stVal.pop();
							stOpt.pop();
						}
						else
						{

							igotoindex = 1;
							stVal.pop();
							stOpt.pop();
						}
						break;
					}

					if (iArgCount>1
						&&(stOpt.size()==0 || stOpt.top().GetCode()!=cmFUNC))
						if(stOpt.size()!=0 && stOpt.top().GetCode()!=cmClassFuc)
							Error(ecUNEXPECTED_ARG, m_pTokenReader->GetPos());

					if (stOpt.size() && stOpt.top().GetCode()!=cmOPRT_INFIX)
						ApplyFunc(stOpt, stVal,stClassObj, iArgCount);
				}
				break;
			}

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
		case cmDIV:
		case cmPOW:
		case cmASSIGN:
		case cmOPRT_BIN:

			while ( stOpt.size() && stOpt.top().GetCode() != cmBO)
			{
				if (GetOprtPri(stOpt.top()) < GetOprtPri(opt))
					break;
				if (stOpt.top().GetCode()==cmOPRT_INFIX)
					ApplyFunc(stOpt, stVal, stClassObj,1);
				else
					ApplyBinOprt(stOpt, stVal,stClassObj);
			}

			stOpt.push(opt);
			break;

		case cmBO:
			stArgCount.push(1);
			stOpt.push(opt);
			break;
		case cmFUNC:
		case cmFUNC_STR:
		case cmClassFuc:
		case cmOPRT_INFIX:
			stOpt.push(opt);
			break;
		case cmOPRT_POSTFIX:
			stOpt.push(opt);
			ApplyFunc(stOpt, stVal, stClassObj,1);
			break;

		case cmLB:
			if(1==igotoindex)
				icurstep=opt.GetGotoIndex();

			ioptid = stOpt.size();
			ioptid = ioptid >0? ioptid-1:0;
			stopt_storage.push(ioptid);
			ioptid = stVal.size();
			ioptid = ioptid >0? ioptid-1:0;
			stval_storage.push(ioptid);
			ioptid = stClassObj.size();
			ioptid = ioptid >0? ioptid-1:0;
			stClassObj_storage.push(ioptid);

			break;
		case cmRB:
			icurstep=opt.GetGotoIndex()>-1?opt.GetGotoIndex():icurstep;

			ioptid = stopt_storage.top();
			for(int i=stOpt.size();i>ioptid;i--)
				stOpt.pop();

			ioptid = stopt_storage.top();
			for(int i=stVal.size();i>ioptid;i--)
				stVal.pop();

			ioptid = stClassObj_storage.top();
			for(int i=stClassObj.size();i>ioptid;i--)
				stClassObj.pop();

			stopt_storage.pop();
			stval_storage.pop();
			stClassObj_storage.pop();

			break;
		case cmIf:
		case cmWhile:
			stOpt.push(opt);
			break;
		case cmElse:

			if(1==igotoindex)
			{
				igotoindex = 0;
			}
			else
			{
				igotoindex = 1;
			}
			break;
		case cmReturn:
			const_cast<ParserBase*>(this)->m_bstoprun = true;
			return 0;
		case cmCallin:
			break;
		case cmClassObjDef:
			{
				pclass= opt.m_pClass;
				pobj=pclass->addvar(opt.GetAsString());
				if(0!=pobj)
					opt.SetClassVarDef(pclass,pobj, opt.GetAsString() );
			}
			break;
		case cmClass:

			break;

		case cmClassObj:

			stClassObj.push(opt);

			break;

		case cmMember:
			stClassObj.pop();
			break;
		case cmPointer:

			break;

		default:
			Error(ecINTERNAL_ERROR, 3);
		}

		if( opt.GetCode() == cmEND )
			break;
	}

	m_StringFormula=m_pTokenReader->GetFormula();

	if(0!=stVal.size())
	{

		if (stVal.top().GetType()!=tpDBL)
			Error(ecSTR_RESULT);

		value_type fVal = stVal.top().GetVal();

		return fVal;
	}
	else
	{
		return 0;
	}
}

void ParserBase::CopyRUNOpt(int inum)
{
	char chnum[25];
	sprintf(chnum, "%d" ,inum);

	m_mapoptstack[string(chnum)] = m_OptStack;

}

void ParserBase::RunOpt(int ioptnum)
{
	char chnum[25];
	sprintf(chnum, "%d" ,ioptnum);
	if (m_mapoptstack.find(string(chnum))!=m_mapoptstack.end())
	RunOptStack(m_mapoptstack[string(chnum)]);

}

void ParserBase::RunOptString(const char *poptstackname)
{

	std::string optstackname(poptstackname);
	if(optstackname.size()<=0)
		return;
	if (m_mapoptstack.find(optstackname)!=m_mapoptstack.end())
	RunOptStack(m_mapoptstack[optstackname]);
}

void ParserBase::SetOptStack(const string_type & optstackname)
{
	if(optstackname.size()<=0)
	return;

	m_mapoptstack[optstackname] = m_OptStack;
}
void ParserBase::ClearOptStack()
{
	m_mapoptstack.clear();
}

inline value_type ParserBase::RunOptStack(TokeStack & optstack) const
{
#if defined(_MSC_VER)
#pragma warning( disable : 4311 )
#endif

	if(optstack.size()==0)
		return 0;

	ParserStack<token_type> stOpt, stVal,stClassObj;

	ParserStack<int> stArgCount;
	token_type opta, opt ,opt1,opt2;
	token_type * popt;
	token_type val, tval;
	string_type strBuf;

	token_type ObjectTok;
	token_type *ptoke,*ptoketemp;
	classbase  *pclass;
	void *pobj ;
	value_type avalue;

	ReInit();

	std::stack<int>  stopt_storage;
	std::stack<int>  stval_storage;
	std::stack<int>  stClassObj_storage;
	int ioptid =0;

	ECmdCode iGetCode;
	int igotoindex =0;
	int icurstep = 0;
	for(;;)
	{
		if(true==m_bstoprun)
			break;

		opt = optstack[icurstep];
		iGetCode=opt.GetCode();
		icurstep++;

		switch (iGetCode)
		{

		case cmSTRING:
			opt.SetIdx((int)m_vStringBuf.size());
			stVal.push(opt);
			m_vStringBuf.push_back(opt.GetAsString());
			break;

		case cmVAR:
			stVal.push(opt);

			break;
		case cmVARLP:
			stVal.push(opt);

			break;
		case cmVAL:
			stVal.push(opt);

			break;
		case cmCOMMA:
			if (stArgCount.empty())
				Error(ecUNEXPECTED_COMMA, icurstep);
			++stArgCount.top();

		case cmSEMICOLON:
		case cmEND:
		case cmBC:
			{

				if (opta.GetCode()==cmBO)
					--stArgCount.top();
				{
					while ( stOpt.size() && stOpt.top().GetCode() != cmBO )
					{
						if (stOpt.top().GetCode()==cmOPRT_INFIX)
						{
							ApplyFunc(stOpt, stVal,stClassObj, 1);
						}
						else
						{
							ApplyBinOprt(stOpt, stVal,stClassObj);
						}
					}

					if (stOpt.size() && stOpt.top().GetCode()==cmOPRT_INFIX)
						ApplyFunc(stOpt, stVal,stClassObj, 1);
					if ( opt.GetCode()!=cmBC
						|| stOpt.size()==0
						|| stOpt.top().GetCode()!=cmBO )
						break;

					assert(stArgCount.size());
					int iArgCount = stArgCount.pop();

					stOpt.pop();

					if(stOpt.size()
						&&(stOpt.top().GetCode()==cmWhile))
					{
						avalue= stVal.top().GetVal();

						if(avalue!=0)
						{
							igotoindex = 0;
							stVal.pop();
							stOpt.pop();
						}
						else
						{
							igotoindex = 1;
							stVal.pop();
							stOpt.pop();

						}
						break;
					}
					if(stOpt.size()
						&&(stOpt.top().GetCode()==cmIf))
					{
						avalue= stVal.top().GetVal();

						if(avalue>0)
						{

							igotoindex = 0;
							stVal.pop();
							stOpt.pop();
						}
						else
						{

							igotoindex = 1;
							stVal.pop();
							stOpt.pop();
						}
						break;
					}

					if (iArgCount>1
						&&(stOpt.size()==0 || stOpt.top().GetCode()!=cmFUNC))
						if(stOpt.size()!=0 && stOpt.top().GetCode()!=cmClassFuc)
							Error(ecUNEXPECTED_ARG, m_pTokenReader->GetPos());

					if (stOpt.size() && stOpt.top().GetCode()!=cmOPRT_INFIX)
						ApplyFunc(stOpt, stVal,stClassObj, iArgCount);
				}
				break;
			}

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
		case cmDIV:
		case cmPOW:
		case cmASSIGN:
		case cmOPRT_BIN:

			while ( stOpt.size() && stOpt.top().GetCode() != cmBO)
			{
				if (GetOprtPri(stOpt.top()) < GetOprtPri(opt))
					break;
				if (stOpt.top().GetCode()==cmOPRT_INFIX)
					ApplyFunc(stOpt, stVal, stClassObj,1);
				else
					ApplyBinOprt(stOpt, stVal,stClassObj);
			}

			stOpt.push(opt);
			break;

		case cmBO:
			stArgCount.push(1);
			stOpt.push(opt);
			break;
		case cmFUNC:
		case cmFUNC_STR:
		case cmClassFuc:
		case cmOPRT_INFIX:
			stOpt.push(opt);
			break;
		case cmOPRT_POSTFIX:
			stOpt.push(opt);
			ApplyFunc(stOpt, stVal, stClassObj,1);
			break;

		case cmLB:
			if(1==igotoindex)
				icurstep=opt.GetGotoIndex();

			ioptid = stOpt.size();
			ioptid = ioptid >0? ioptid-1:0;
			stopt_storage.push(ioptid);
			ioptid = stVal.size();
			ioptid = ioptid >0? ioptid-1:0;
			stval_storage.push(ioptid);
			ioptid = stClassObj.size();
			ioptid = ioptid >0? ioptid-1:0;
			stClassObj_storage.push(ioptid);

			break;
		case cmRB:
			icurstep=opt.GetGotoIndex()>-1?opt.GetGotoIndex():icurstep;

			ioptid = stopt_storage.top();
			for(int i=stOpt.size();i>ioptid;i--)
				stOpt.pop();

			ioptid = stopt_storage.top();
			for(int i=stVal.size();i>ioptid;i--)
				stVal.pop();

			ioptid = stClassObj_storage.top();
			for(int i=stClassObj.size();i>ioptid;i--)
				stClassObj.pop();

			stopt_storage.pop();
			stval_storage.pop();
			stClassObj_storage.pop();

			break;
		case cmIf:
		case cmWhile:
			stOpt.push(opt);
			break;
		case cmElse:

			if(1==igotoindex)
			{
				igotoindex = 0;
			}
			else
			{
				igotoindex = 1;
			}
			break;
		case cmReturn:
			const_cast<ParserBase*>(this)->m_bstoprun = true;
			return 0;
		case cmCallin:
			break;
		case cmClassObjDef:
			{
				pclass= opt.m_pClass;
				pobj=pclass->addvar(opt.GetAsString());
				if(0!=pobj)
					opt.SetClassVarDef(pclass,pobj, opt.GetAsString() );
			}
			break;
		case cmClass:

			break;

		case cmClassObj:

			stClassObj.push(opt);

			break;

		case cmMember:
			stClassObj.pop();
			break;
		case cmPointer:

			break;

		default:
			Error(ecINTERNAL_ERROR, 3);
		}

		if( opt.GetCode() == cmEND )
			break;

	}

	if(0!=stVal.size())
	{
		if (stVal.top().GetType()!=tpDBL)
			Error(ecSTR_RESULT);

		value_type fVal = stVal.top().GetVal();
		return fVal;
	}
	else
	{
		return 0;
	}
}

void ParserBase::CompileClassDeclara(const string_type &a_strdeclarastr,CreateClass *paclass)
{
	if (paclass == NULL)
		return;

	string_type::size_type start = 0;
	while (start < a_strdeclarastr.length())
	{
		string_type::size_type end = a_strdeclarastr.find(_T(';'), start);
		string_type declaration = (end == string_type::npos)
			? a_strdeclarastr.substr(start)
			: a_strdeclarastr.substr(start, end - start);
		start = (end == string_type::npos) ? a_strdeclarastr.length() : end + 1;

		string_type type_name;
		std::vector<string_type> member_names;
		if (!SplitCreateClassDeclaration(declaration, type_name, member_names))
			continue;

		classbase *member_class = ResolveRegisteredClass(type_name);
		if (member_class == NULL)
		{
			return;
		}

		for (std::vector<string_type>::const_iterator member_it = member_names.begin();
			 member_it != member_names.end();
			 ++member_it)
		{
			if (!paclass->addclassdef(member_class, *member_it))
			{
				Error(ecCLASSMEMOPT_CONFICT, -1, *member_it);
				return;
			}
		}
	}
}

void ParserBase::CompileFuncAndRunString(const string_type &a_strfucstr,const string_type &a_strclass,const string_type &a_strobj)
{
	paramvect empty_params;
	CompileFuncAndRunString(a_strfucstr, a_strclass, a_strobj, empty_params);
}

void ParserBase::CompileFuncAndRunString(const string_type &a_strfucstr,const string_type &a_strclass,const string_type &a_strobj,const paramvect &aparms)
{
	CreateClass *scripted_class = ResolveCreateClass(a_strclass);
	if (scripted_class == NULL)
		return;

	void *pobj = GetClassObj(a_strclass, a_strobj);
	if (pobj == NULL)
	{
		Error(ecCLASSOBJ_CONFICT, -1, a_strobj);
		return;
	}

	CreateClass *previous_createclass = m_pcreateclass;
	m_pcreateclass = scripted_class;
	scripted_class->GetRunParser(this);
	struct AliasRestore
	{
		string_type alias_name;
		value_type *previous_ptr;
		bool had_previous;
	};
	std::vector<AliasRestore> alias_restores;
	std::vector<value_type> arg_alias_values(aparms.begin(), aparms.end());
	const int member_count = scripted_class->GetClassMemberNum();
	for (int i = 0; i < member_count; ++i)
	{
		const string_type member_name = scripted_class->GetClassMemberName(i);
		classbase *member_class = scripted_class->GetMemberClass(member_name);
		if (member_class == NULL)
			continue;
		if (member_class->getclass() != string_type(typeid(value_type).name()))
			continue;

		const string_type storage_name = scripted_class->BuildMemberStorageName(a_strobj, member_name);
		value_type *member_ptr = static_cast<value_type *>(member_class->getvar(storage_name));
		if (member_ptr == NULL)
			continue;

		AliasRestore restore;
		restore.alias_name = member_name;
		varmap_type::iterator existing = m_VarDef.find(member_name);
		restore.had_previous = (existing != m_VarDef.end());
		restore.previous_ptr = restore.had_previous ? existing->second : NULL;
		alias_restores.push_back(restore);
		m_VarDef[member_name] = member_ptr;
	}
	for (int i = 0; i < static_cast<int>(arg_alias_values.size()); ++i)
	{
		std::vector<string_type> alias_names;
		if (i == 0)
		{
			alias_names.push_back("arg");
			alias_names.push_back("p");
		}
		std::ostringstream arg_name_builder;
		arg_name_builder << "arg" << i;
		alias_names.push_back(arg_name_builder.str());
		std::ostringstream p_name_builder;
		p_name_builder << "p" << i;
		alias_names.push_back(p_name_builder.str());

		for (std::vector<string_type>::const_iterator alias_it = alias_names.begin();
			 alias_it != alias_names.end();
			 ++alias_it)
		{
			AliasRestore arg_restore;
			arg_restore.alias_name = *alias_it;
			varmap_type::iterator existing = m_VarDef.find(arg_restore.alias_name);
			arg_restore.had_previous = (existing != m_VarDef.end());
			arg_restore.previous_ptr = arg_restore.had_previous ? existing->second : NULL;
			alias_restores.push_back(arg_restore);
			m_VarDef[arg_restore.alias_name] = &arg_alias_values[static_cast<size_t>(i)];
		}
	}

	const string_type bound_param_script = ReplaceCreateClassParamRefs(a_strfucstr, aparms);
	SetExpr(bound_param_script);
	m_vByteCode.SetCreateClassLP(scripted_class);

	try
	{
		(void)pobj;
		Eval();
	}
	catch (...)
	{
		for (std::vector<AliasRestore>::const_reverse_iterator it = alias_restores.rbegin();
			 it != alias_restores.rend();
			 ++it)
		{
			if (it->had_previous)
				m_VarDef[it->alias_name] = it->previous_ptr;
			else
				m_VarDef.erase(it->alias_name);
		}
		m_vByteCode.SetCreateClassLP(previous_createclass);
		m_pcreateclass = previous_createclass;
		throw;
	}

	for (std::vector<AliasRestore>::const_reverse_iterator it = alias_restores.rbegin();
		 it != alias_restores.rend();
		 ++it)
	{
		if (it->had_previous)
			m_VarDef[it->alias_name] = it->previous_ptr;
		else
			m_VarDef.erase(it->alias_name);
	}
	m_vByteCode.SetCreateClassLP(previous_createclass);
	m_pcreateclass = previous_createclass;
}

classbase* ParserBase::ResolveRegisteredClass(const string_type &a_sClassName)
{
	classbasemap_type::iterator itor = m_ClassDefMap.find(a_sClassName);
	if (itor == m_ClassDefMap.end() || itor->second == NULL)
	{
		Error(ecCLASS_CONFICT, -1, a_sClassName);
		return NULL;
	}

	return itor->second;
}

CreateClass* ParserBase::ResolveCreateClass(const string_type &a_sClassName)
{
	classbase *registered_class = ResolveRegisteredClass(a_sClassName);
	if (registered_class == NULL)
		return NULL;

	CreateClass *scripted_class = dynamic_cast<CreateClass *>(registered_class);
	if (scripted_class == NULL)
	{
		Error(ecCLASSFUC_CONFICT, -1, a_sClassName);
		return NULL;
	}

	return scripted_class;
}

#if defined(MUP_DUMP_STACK) | defined(MUP_DUMP_CMDCODE)

void ParserBase::StackDump( const ParserStack<token_type > &a_stVal,
                                          const ParserStack<token_type > &a_stOprt ) const
{
  using std::cout;
  ParserStack<token_type> stOprt(a_stOprt),
                          stVal(a_stVal);

  cout << "\nValue stack:\r\n";
  while ( !stVal.empty() )
  {
    token_type val = stVal.pop();
    cout << " " << val.GetVal() << " ";
  }
  cout << "\nOperator stack:\r\n";

  while ( !stOprt.empty() )
  {
     if (stOprt.top().GetCode()<=cmASSIGN)
     {
       cout << "OPRT_INTRNL \""
            << ParserBase::c_DefaultOprt[stOprt.top().GetCode()]
            << "\" \r\n";
	   }
     else
     {
		    switch(stOprt.top().GetCode())
		    {
		      case cmVAR:        cout << "VAR\r\n";  break;
		      case cmVAL:        cout << "VAL\r\n";  break;
		      case cmFUNC:       cout << "FUNC_NUM \""
                                  << stOprt.top().GetAsString()
                                  << "\"\r\n";   break;
		      case cmOPRT_INFIX: cout << "OPRT_INFIX \""
                                  << stOprt.top().GetAsString()
                                  << "\"\r\n";   break;
			  case cmOPRT_BIN:   cout << "OPRT_BIN \""
                                  << stOprt.top().GetAsString()
                                  << "\"\r\n";        break;
              case cmFUNC_STR:   cout << "FUNC_STR\r\n";  break;
		      case cmEND:        cout << "END\r\n";       break;
		      case cmUNKNOWN:    cout << "UNKNOWN\r\n";   break;
		      case cmBO:         cout << "BRACKET \"(\"\r\n";  break;
		      case cmBC:         cout << "BRACKET \")\"\r\n";  break;
		      default:           cout << stOprt.top().GetType() << " ";  break;
		    }
     }
     stOprt.pop();
  }

  cout << dec << "\r\n";
}

#endif
void* ParserBase::GetClassObj(const string_type & strclass,const string_type & strobj)
{
	mu::classbasemap_type classmap = GetClassMap();
	classbase *pclass;
	if(!classmap.size())
		return NULL;

	classbasemap_type::const_iterator item = classmap.begin();
	for (; item!=classmap.end(); ++item)
	{
		if(strclass==item->first)
		{
			pclass=(item->second);

			for(int i=0;i<pclass->size();i++)
			{
				if(strobj == pclass->getvar(i))
					return pclass->getvarpoint(i);
			}
		}
	}
	return NULL;
}
void* ParserBase::GetClassObj(const string_type &  strclass,int iobjnum)
{
	mu::classbasemap_type classmap = GetClassMap();
	classbase *pclass;
	if(!classmap.size())
		return 0;

	classbasemap_type::const_iterator item = classmap.begin();
	for (; item!=classmap.end(); ++item)
	{
		if(strclass==item->first)
		{
			pclass=(item->second);

			if(iobjnum>=pclass->size())
				return 0;
			for(int i=0;i<pclass->size();i++)
			{
				return pclass->getvarpoint(iobjnum);
			}
		}
	}
	return NULL;
}
mu::string_type ParserBase::GetClassObjName(const mu::string_type & strclass, int object_index) const
{
    mu::classbasemap_type classmap = GetClassMap();
    classbase *pclass = nullptr;
    if (!classmap.size())
        return string_type();

    mu::classbasemap_type::const_iterator item = classmap.begin();
    for (; item != classmap.end(); ++item)
    {
        if (strclass == item->first)
        {
            pclass = const_cast<classbase*>(item->second);
            break;
        }
    }

    if (!pclass)
        return string_type();

    if (object_index < 0 || object_index >= pclass->size())
        return string_type();

    return pclass->getvar(object_index);
}

int ParserBase::GetClassObjSum(const string_type &  strclass)
{
	mu::classbasemap_type classmap = GetClassMap();
	classbase *pclass;
	if(!classmap.size())
		return 0;

	mu::classbasemap_type::const_iterator item = classmap.begin();
	for (; item!=classmap.end(); ++item)
	{
		if(strclass==item->first)
		{
			pclass=(item->second);

			return pclass->size();
		}
	}
	return 0;
}
}
