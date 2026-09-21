

#include "muParserCallback.h"

namespace mu
{

	
	ParserCallback::ParserCallback(fun_type1 a_pFun, bool a_bAllowOpti, int a_iPrec, ECmdCode a_iCode)
		:m_pFun((void*)a_pFun)
		,m_iArgc(1)
		,m_iPri(a_iPrec)
		,m_iCode(a_iCode)
		,m_iType(tpDBL)
		,m_bAllowOpti(a_bAllowOpti)
		,m_iReturnType(tpDBL)
	{

	}

	ParserCallback::ParserCallback( fun_type2 a_pFun, bool a_bAllowOpti, int a_iPrec, ECmdCode a_iCode)
		:m_pFun((void*)a_pFun)
		,m_iArgc(2)
		,m_iPri(a_iPrec)
		,m_iCode(a_iCode)
		,m_iType(tpDBL)
		,m_bAllowOpti(a_bAllowOpti)
		,m_iReturnType(tpDBL)
	{
	}

	ParserCallback::ParserCallback(fun_type3 a_pFun, bool a_bAllowOpti)
		:m_pFun((void*)a_pFun)
		,m_iArgc(3)
		,m_iPri(-1)
		,m_iCode(cmFUNC)
		,m_iType(tpDBL)
		,m_bAllowOpti(a_bAllowOpti)
		,m_iReturnType(tpDBL)
	{}

	ParserCallback::ParserCallback(fun_type4 a_pFun, bool a_bAllowOpti)
		:m_pFun((void*)a_pFun)
		,m_iArgc(4)
		,m_iPri(-1)
		,m_iCode(cmFUNC)
		,m_iType(tpDBL)
		,m_bAllowOpti(a_bAllowOpti)
		,m_iReturnType(tpDBL)
	{}

	ParserCallback::ParserCallback(fun_type5 a_pFun, bool a_bAllowOpti)
		:m_pFun((void*)a_pFun)
		,m_iArgc(5)
		,m_iPri(-1)
		,m_iCode(cmFUNC)
		,m_iType(tpDBL)
		,m_bAllowOpti(a_bAllowOpti)
		,m_iReturnType(tpDBL)
	{}

	ParserCallback::ParserCallback(multfun_type a_pFun, bool a_bAllowOpti)
		:m_pFun((void*)a_pFun)
		,m_iArgc(-1)
		,m_iPri(-1)
		,m_iCode(cmFUNC)
		,m_iType(tpDBL)
		,m_bAllowOpti(a_bAllowOpti)
		,m_iReturnType(tpDBL)
	{}

	ParserCallback::ParserCallback(strfun_type1 a_pFun, bool a_bAllowOpti)
		:m_pFun((void*)a_pFun)
		,m_iArgc(1)
		,m_iPri(-1)
		,m_iCode(cmFUNC_STR)
		,m_iType(tpSTR)
		,m_bAllowOpti(a_bAllowOpti)
		,m_iReturnType(tpDBL)
	{}

  ParserCallback::ParserCallback(fun_lptype a_pFun, bool a_bAllowOpti, int a_iPrec, ECmdCode a_iCode)
    :m_pFun((void*)a_pFun)
    ,m_iArgc(1)
    ,m_iPri(a_iPrec)
    ,m_iCode(a_iCode)
    ,m_iType(tpDBL)
    ,m_bAllowOpti(a_bAllowOpti)
	,m_iReturnType(tpDBLLP)
  {
  }
  ParserCallback::ParserCallback(visionfun_type a_pFun,bool a_bAllowOpti)
	  :m_pFun((void *)a_pFun)
	  ,m_iArgc(-1)
	  ,m_iPri(-1)
	  ,m_iCode(cmFUNC)
	  ,m_iType(tpDBL)
	  ,m_bAllowOpti(a_bAllowOpti)
	  ,m_iReturnType(tpDBL)
  {}
  ParserCallback::ParserCallback(visionfun_lptype a_pFun,bool a_bAllowOpti)
	  :m_pFun((void *)a_pFun)
	  ,m_iArgc(-1)
	  ,m_iPri(-1)
	  ,m_iCode(cmFUNC)
	  ,m_iType(tpDBL)
	  ,m_bAllowOpti(a_bAllowOpti)
	,m_iReturnType(tpDBL)
  {}

  ParserCallback::ParserCallback(fun_void_type1 a_pFun, bool a_bAllowOpti, int a_iPrec, ECmdCode a_iCode)
	  :m_pFun((void*)a_pFun)
	  ,m_iArgc(1)
	  ,m_iPri(a_iPrec)
	  ,m_iCode(a_iCode)
	  ,m_iType(tpVOID)
	  ,m_bAllowOpti(a_bAllowOpti)
	  ,m_iReturnType(tpVOID)
  {

  }
  ParserCallback::ParserCallback( fun_void_type2 a_pFun, bool a_bAllowOpti, int a_iPrec, ECmdCode a_iCode)
	  :m_pFun((void*)a_pFun)
	  ,m_iArgc(2)
	  ,m_iPri(a_iPrec)
	  ,m_iCode(a_iCode)
	  ,m_iType(tpVOID)
	  ,m_bAllowOpti(a_bAllowOpti)
	  ,m_iReturnType(tpVOID)
  {
  }
  ParserCallback::ParserCallback(fun_void_type3 a_pFun, bool a_bAllowOpti)
	  :m_pFun((void*)a_pFun)
	  ,m_iArgc(3)
	  ,m_iPri(-1)
	  ,m_iCode(cmFUNC)
	  ,m_iType(tpVOID)
	  ,m_bAllowOpti(a_bAllowOpti)
	  ,m_iReturnType(tpVOID)
  {

  }
  ParserCallback::ParserCallback(fun_void_type4 a_pFun, bool a_bAllowOpti)
	  :m_pFun((void*)a_pFun)
	  ,m_iArgc(4)
	  ,m_iPri(-1)
	  ,m_iCode(cmFUNC)
	  ,m_iType(tpVOID)
	  ,m_bAllowOpti(a_bAllowOpti)
	  ,m_iReturnType(tpVOID)
  {

  }
  ParserCallback::ParserCallback(fun_void_type5 a_pFun, bool a_bAllowOpti)
	  :m_pFun((void*)a_pFun)
	  ,m_iArgc(5)
	  ,m_iPri(-1)
	  ,m_iCode(cmFUNC)
	  ,m_iType(tpVOID)
	  ,m_bAllowOpti(a_bAllowOpti)
	  ,m_iReturnType(tpVOID)
  {

  }
  ParserCallback::ParserCallback(multfun_void_type a_pFun, bool a_bAllowOpti)
	  :m_pFun((void*)a_pFun)
	  ,m_iArgc(-1)
	  ,m_iPri(-1)
	  ,m_iCode(cmFUNC)
	  ,m_iType(tpVOID)
	  ,m_bAllowOpti(a_bAllowOpti)
	  ,m_iReturnType(tpVOID)
  {

  }
  ParserCallback::ParserCallback(strfun_void_type1 a_pFun, bool a_bAllowOpti)
	  :m_pFun((void*)a_pFun)
	  ,m_iArgc(1)
	  ,m_iPri(-1)
	  ,m_iCode(cmFUNC_STR)
	  ,m_iType(tpVOID)
	  ,m_bAllowOpti(a_bAllowOpti)
	  ,m_iReturnType(tpVOID)
  {}

  ParserCallback::ParserCallback()
    :m_pFun(0)
    ,m_iArgc(0)
    ,m_iCode(cmUNKNOWN)
    ,m_iType(tpVOID)
    ,m_bAllowOpti(0)
	,m_iReturnType(tpDBL)
  {
  }

  
  ParserCallback::ParserCallback(const ParserCallback &a_Fun)
  {
    m_pFun = a_Fun.m_pFun;
    m_iArgc = a_Fun.m_iArgc;
    m_bAllowOpti = a_Fun.m_bAllowOpti;
    m_iCode = a_Fun.m_iCode;
    m_iType = a_Fun.m_iType;
    m_iPri = a_Fun.m_iPri;
	m_iReturnType=a_Fun.m_iReturnType;
  }

  
  ParserCallback* ParserCallback::Clone() const
  {
    return new ParserCallback(*this);
  }

  bool ParserCallback::IsOptimizable() const
  {
    return m_bAllowOpti;
  }

  void* ParserCallback::GetAddr() const
  {
    return m_pFun;
  }

  ECmdCode  ParserCallback::GetCode() const
  {
    return m_iCode;
  }

  ETypeCode ParserCallback::GetType() const
  {
    return m_iType;
  }

  int ParserCallback::GetPri()  const
  {
    return m_iPri;
  }

  int ParserCallback::GetArgc() const
  {
    return m_iArgc;
  }

  ETypeCode ParserCallback::GetReturn() const
  {
	  return m_iReturnType;
  }
}
