

#ifndef MU_PARSER_CALLBACK_H
#define MU_PARSER_CALLBACK_H

#include "muParserDef.h"

namespace mu
{

class  ParserCallback
{
public:

    ParserCallback(fun_type1 a_pFun, bool a_bAllowOpti, int a_iPrec = -1, ECmdCode a_iCode=cmFUNC);
    ParserCallback(fun_type2 a_pFun, bool a_bAllowOpti, int a_iPrec = -1, ECmdCode a_iCode=cmFUNC);
    ParserCallback(fun_type3 a_pFun, bool a_bAllowOpti);
    ParserCallback(fun_type4 a_pFun, bool a_bAllowOpti);
    ParserCallback(fun_type5 a_pFun, bool a_bAllowOpti);
    ParserCallback(multfun_type a_pFun, bool a_bAllowOpti);
    ParserCallback(strfun_type1 a_pFun, bool a_bAllowOpti);

	ParserCallback(fun_lptype a_pFun, bool a_bAllowOpti, int a_iPrec = -1, ECmdCode a_iCode=cmFUNC);
	ParserCallback(visionfun_type a_pFun,bool a_bAllowOpti);
	ParserCallback(visionfun_lptype a_pFun,bool a_bAllowOpti);

	ParserCallback(fun_void_type1 a_pFun, bool a_bAllowOpti, int a_iPrec = -1, ECmdCode a_iCode=cmFUNC);
	ParserCallback(fun_void_type2 a_pFun, bool a_bAllowOpti, int a_iPrec = -1, ECmdCode a_iCode=cmFUNC);
	ParserCallback(fun_void_type3 a_pFun, bool a_bAllowOpti);
	ParserCallback(fun_void_type4 a_pFun, bool a_bAllowOpti);
	ParserCallback(fun_void_type5 a_pFun, bool a_bAllowOpti);
	ParserCallback(multfun_void_type a_pFun, bool a_bAllowOpti);
	ParserCallback(strfun_void_type1 a_pFun, bool a_bAllowOpti);

    ParserCallback();
    ParserCallback(const ParserCallback &a_Fun);

    
    ParserCallback* Clone() const;

    bool IsOptimizable() const;

    void* GetAddr() const;
    ECmdCode  GetCode() const;
    ETypeCode GetType() const;
    int GetPri()  const;
    int GetArgc() const;
	ETypeCode GetReturn() const;
private:
    void *m_pFun;

    int   m_iArgc;
    int   m_iPri;
    ECmdCode  m_iCode;
    ETypeCode m_iType;
    bool  m_bAllowOpti;
	ETypeCode m_iReturnType;

};

typedef std::map<string_type, ParserCallback> funmap_type;

}

#endif
