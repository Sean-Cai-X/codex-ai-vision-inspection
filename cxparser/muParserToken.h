

#ifndef MU_PARSER_TOKEN_H
#define MU_PARSER_TOKEN_H

#include <cassert>
#include <string>
#include <stack>
#include <vector>
#include <memory>

#include "muParserError.h"
#include "muParserCallback.h"
#include "muParserClass.h"
namespace mu
{

template<typename TBase, typename TString>

class ParserToken
				{
public:

    enum ETokFlags
	  {
	    flVOLATILE = 1
	  };

public:

	classbase*m_pClass;
	void *m_pClassObj;
	int m_iobjectnum;
	int m_iRunIndex;
	int m_iGotoIndex;

private:
    ECmdCode  m_iCode;
    ETypeCode m_iType;
    void  *m_pTok;
	int  m_iFlags;
	int  m_iIdx;
    TString m_strTok;
    TString m_strVal;
    value_type m_fVal;
    std::unique_ptr <ParserCallback> m_pCallback;

public:

    
    ParserToken()
      :m_iCode(cmUNKNOWN)
      ,m_iType(tpVOID)
      ,m_pTok(0)
      ,m_iFlags(0)
      ,m_iIdx(-1)
      ,m_strTok()
      ,m_pCallback()
	  ,m_pClass(0)
	  ,m_pClassObj(0)
	  ,m_iobjectnum(-1)
	  ,m_iGotoIndex(-1)
	  ,m_iRunIndex(-1)
    {

	}

    ParserToken(const ParserToken &a_Tok)
    {
      Assign(a_Tok);
    }

    ParserToken& operator=(const ParserToken &a_Tok)
    {
      Assign(a_Tok);
      return *this;
    }

    
    void Assign(const ParserToken &a_Tok)
    {
      m_iCode = a_Tok.m_iCode;
      m_pTok = a_Tok.m_pTok;
      m_iFlags = a_Tok.m_iFlags;
      m_strTok = a_Tok.m_strTok;
      m_iIdx = a_Tok.m_iIdx;
      m_strVal = a_Tok.m_strVal;
      m_iType = a_Tok.m_iType;
      m_fVal = a_Tok.m_fVal;

      m_pCallback.reset(a_Tok.m_pCallback.get() ? a_Tok.m_pCallback->Clone() : 0);

	  m_pClass=a_Tok.m_pClass;
	  m_pClassObj=a_Tok.m_pClassObj;
	  m_iobjectnum=a_Tok.m_iobjectnum;
	  m_iGotoIndex = a_Tok.m_iGotoIndex;
	  m_iRunIndex = a_Tok.m_iRunIndex;
    }

    void AddFlags(int a_iFlags)
    {
      m_iFlags |= a_iFlags;
    }

    bool IsFlagSet(int a_iFlags) const
    {

        return (bool)(m_iFlags & a_iFlags);
    }

    
    ParserToken& Set(ECmdCode a_iType, const TString &a_strTok=TString())
    {

      assert(a_iType!=cmVAR);
      assert(a_iType!=cmVAL);
      assert(a_iType!=cmFUNC);

      m_iCode = a_iType;
      m_iType = tpVOID;
      m_pTok = 0;
      m_iFlags = 0;
      m_strTok = a_strTok;
      m_iIdx = -1;

      return *this;
    }

	ParserToken& Set(ECmdCode a_iType, void *pvoid )
	{

		assert(a_iType!=cmVAR);
		assert(a_iType!=cmVAL);
		assert(a_iType!=cmFUNC);

		m_iCode = a_iType;
		m_iType = tpVOID;
		m_pTok = pvoid;
		m_iFlags = 0;
		m_strTok = TString();
		m_iIdx = -1;

		return *this;
	}

    ParserToken& Set(const ParserCallback &a_pCallback, const TString &a_sTok)
    {
      assert(a_pCallback.GetAddr());

      m_iCode = a_pCallback.GetCode();
      m_iType = tpVOID;
      m_strTok = a_sTok;
      m_pCallback.reset(new ParserCallback(a_pCallback));

      m_pTok = 0;
      m_iFlags = 0;
      m_iIdx = -1;

      if (!m_pCallback->IsOptimizable())
        AddFlags(flVOLATILE);

      return *this;
    }

	ParserToken& SetRunIndex(int index)
	{
		m_iRunIndex = index;

		return *this;
	}

	ParserToken& SetGotoIndex(int index)
	{
		m_iGotoIndex = index;

		return *this;
	}

	int GetGotoIndex()
	{
		return m_iGotoIndex;
	}

    
    ParserToken& SetVal(TBase a_fVal, const TString &a_strTok=TString())
    {
      m_iCode = cmVAL;
      m_iType = tpDBL;
      m_fVal = a_fVal;
      m_iFlags = 0;
      m_strTok = a_strTok;
      m_iIdx = -1;
      m_pTok = 0;
      m_pCallback.reset(0);
      return *this;
    }

    
    ParserToken& SetVar(TBase *a_pVar, const TString &a_strTok)
    {
      m_iCode = cmVAR;
      m_iType = tpDBL;
      m_iFlags = 0;
      m_strTok = a_strTok;
      m_iIdx = -1;
      m_pTok = (void*)a_pVar;
      m_pCallback.reset(0);

      AddFlags(ParserToken::flVOLATILE);
      return *this;
    }

    
	ParserToken& SetClass(classbase *a_pBase, const TString &a_strTok)
	{
		m_iCode = cmClass;
		m_iType = tpCLASS;
		m_iFlags = 0;
		m_strTok = a_strTok;
		m_iIdx = -1;
		m_pClass=a_pBase;
		m_pCallback.reset(0);

		return *this;
	}

    
	ParserToken& SetClassVar(classbase *a_pBase,void *a_pclassobj, const TString &a_strTok=TString())
	{
		m_iCode = cmClassObj;
		m_iType = tpCLASS;
		m_iFlags = 0;
		m_strTok = a_strTok;
		m_iIdx = -1;
		m_pClass=a_pBase;
		m_pClassObj=a_pclassobj;
		m_pCallback.reset(0);

		return *this;
	}

	ParserToken& SetClassVarDef(classbase *a_pBase,void *a_pclassobj, const TString &a_strTok)
	{
		m_iCode = cmClassObjDef;
		m_iType = tpCLASS;
		m_iFlags = 0;
		m_strTok = a_strTok;
		m_iIdx = -1;
		m_pClass=a_pBase;
		m_pClassObj=a_pclassobj;
		m_pCallback.reset(0);

		return *this;
	}

	ParserToken& SetClassFuc(classbase *a_pBase,void *a_pclassobj, const TString &a_strTok)
	{
		m_iCode = cmClassFuc;
		m_iType = tpCLASS;
		m_iFlags = 0;
		m_strTok = a_strTok;
		m_iIdx = -1;
		m_pClass=a_pBase;
		m_pClassObj=a_pclassobj;
		m_pCallback.reset(0);

		return *this;
	}

    ParserToken& SetString(const TString &a_strTok, std::size_t a_iSize)
    {
      m_iCode = cmSTRING;
      m_iType = tpSTR;
      m_iFlags = 0;
      m_strTok = a_strTok;
      m_iIdx = static_cast<int>(a_iSize);

      m_pTok = 0;
      m_pCallback.reset(0);

      AddFlags(ParserToken::flVOLATILE);
      return *this;
    }

	ParserToken& SetStringVar(const TString *a_pstr , const TString &a_strTok)
	{
		m_iCode = cmStringVar;
		m_iType = tpSTRLP;
		m_iFlags = 0;
		m_strTok = a_strTok;
		m_iIdx = -1;
		m_pTok = (void*)a_pstr;
		m_pCallback.reset(0);

		AddFlags(ParserToken::flVOLATILE);
		return *this;
	}

    ParserToken& SetString(const std::string &a_strTok)
    {
      m_iCode = cmSTRING;
      m_iType = tpSTR;
      m_iFlags = 0;
      m_iIdx = -1;

      m_pTok = 0;
      m_pCallback.reset(0);

      m_strTok = a_strTok;

      return *this;
    }

    void SetIdx(int a_iIdx)
    {
      if (m_iCode!=cmSTRING || a_iIdx<0)
	      throw ParserError(ecINTERNAL_ERROR);

      m_iIdx = a_iIdx;
    }

    int GetIdx() const
    {
      if (m_iIdx<0 || m_iCode!=cmSTRING )
        throw ParserError(ecINTERNAL_ERROR);

      return m_iIdx;
    }

    ECmdCode GetCode() const
    {
      if (m_pCallback.get())
      {
        return m_pCallback->GetCode();
      }
      else
      {
        return m_iCode;
      }
	}

    ETypeCode GetType() const
    {
      if (m_pCallback.get())
      {
        return m_pCallback->GetType();
      }
      else
      {
        return m_iType;
      }
    }

	ETypeCode GetReturnType() const
	{
		if (m_pCallback.get())
		{
			return m_pCallback->GetReturn();
		}
		else
		{
			return m_iType;
		}
	}

    int GetPri() const
    {
      if ( !m_pCallback.get())
	      throw ParserError(ecINTERNAL_ERROR);

      if ( m_pCallback->GetCode()!=cmOPRT_BIN && m_pCallback->GetCode()!=cmOPRT_INFIX)
	      throw ParserError(ecINTERNAL_ERROR);

      return m_pCallback->GetPri();
    }

    void* GetFuncAddr() const
    {
      return (m_pCallback.get()) ? m_pCallback->GetAddr() : 0;
    }

    TBase GetVal() const
    {
      switch (m_iCode)
      {
        case cmVAL:  return m_fVal;
        case cmVAR:  return *((TBase*)m_pTok);

		default:     throw ParserError(ecINTERNAL_ERROR);
      }
    }

    TBase* GetVar() const
    {
      if (m_iCode!=cmVAR)
	      throw ParserError(ecINTERNAL_ERROR);

      return (TBase*)m_pTok;
    }

	classbase*GetClass() const
	{
		if(m_iCode!=cmClassObj)
			throw ParserError(ecINTERNAL_ERROR);
		return m_pClass;

	}
	void *GetClassObj() const
	{
		if(m_iCode!=cmClassObj)
			throw ParserError(ecINTERNAL_ERROR);
		return m_pClassObj;
	}

    int GetArgCount() const
    {
      assert(m_pCallback.get());

      if (!m_pCallback->GetAddr())
	      throw ParserError(ecINTERNAL_ERROR);

      return m_pCallback->GetArgc();
    }

	int GetClassFucArgCount() const
	{
		assert(m_pClass);
		assert(m_pClassObj);
		return m_pClass->GetFuncArgCount(m_strTok);
	}

	int GetClassFucArgType() const
	{
		assert(m_pClass);
		assert(m_pClassObj);
		return m_pClass->GetFuncArgType(m_strTok);
	}

    const TString& GetAsString() const
    {
      return m_strTok;
    }

    ETypeCode GetReturn() const
    {

	 if (m_pCallback.get())
      {
        return m_pCallback->GetReturn();
      }
      else
      {
          throw ParserError(ecINTERNAL_ERROR);
	 }
    }

};

}

#endif
