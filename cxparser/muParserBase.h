

#ifndef MU_PARSER_BASE_H
#define MU_PARSER_BASE_H

#include <cmath>
#include <string>
#include <iostream>
#include <map>
#include <memory>

#include "muParserDef.h"
#include "muParserStack.h"
#include "muParserTokenReader.h"
#include "muParserBytecode.h"
#include "muParserError.h"
#include "muParserClass.h"
#include "muParserTreeNode.h"
#include "muparserClassReader.h"
#include "muParserClassFunctionReader.h"

namespace mu
{


	static const char_type *c_DefaultOprt[29]=
	{
		"<=",
		">=",
		"!=",
		"==",
		"<",
		">",
		"+",
		"-",
		"*",
		"/",
		"^",
		"and",
		"or",
		"xor",
		"=",
		"(",
		")",
		",",
		";",
		"{",
		"}",
		".",
		"->",
		"if",
		"else",
		"while",
		"return",
		"for",
		0
	};
class   ParserBase
{
friend class ParserTokenReader;
friend class ParserClassReader;
friend class ParserClassFunctionReader;
private:
    typedef value_type (ParserBase::*ParseFunction)() const;
	typedef ParserToken<value_type, string_type> token_type;
    typedef std::vector<string_type> stringbuf_type;
    typedef ParserTokenReader token_reader_type;
    typedef ParserToken<vision_type, string_type> token_viosiontype;
 public:

    
    typedef ParserError exception_type;

    ParserBase();
    ParserBase( const ParserBase &a_Parser );
    ParserBase& operator=(const ParserBase &a_Parser);

    
    virtual ~ParserBase()
    {
			classbasemap_type:: iterator pIter,pIter2;
			for ( pIter = m_ClassDefMap.begin( ) ; pIter != m_ClassDefMap.end( ) ;)
			{
				pIter2=pIter;
				pIter++ ;
				classbase *pbase=pIter2->second;
				switch(pbase->m_iclasstype)
				{
					case CLASS_ORG:
					break;
					case CLASS_PARSER:
					break;
					case CLASS_CREATE:
					break;
					default:
					break;
				}
				delete pbase;
			}
	}

    
    inline value_type Eval() const
    {
      if (m_bHasControlFlow)
      {
        m_pParseFormula = &ParserBase::ParseString;
      }
      return (this->*m_pParseFormula)();
    }

    
    void SetExpr(const string_type &a_sExpr);
	void SetVarFactory(facfun_type a_pFactory,void *a_pvoid=0);
    void EnableOptimizer(bool a_bIsOn=true);
    void EnableByteCode(bool a_bIsOn=true);
    void EnableBuiltInOprt(bool a_bIsOn=true);
    bool HasBuiltInOprt() const;
    void AddValIdent(identfun_type a_pCallback);

#define MUP_DEFINE_FUNC(TYPE)                                                           \
    inline void DefineFun(const string_type &a_strName, TYPE a_pFun, bool a_bAllowOpt = true)  \
	{                                                                                   \
		AddCallback( a_strName, ParserCallback(a_pFun, a_bAllowOpt),                      \
		m_FunDef, ValidNameChars() );                                        \
	}

    MUP_DEFINE_FUNC(fun_type1)
    MUP_DEFINE_FUNC(fun_type2)
    MUP_DEFINE_FUNC(fun_type3)
    MUP_DEFINE_FUNC(fun_type4)
    MUP_DEFINE_FUNC(fun_type5)
    MUP_DEFINE_FUNC(multfun_type)
    MUP_DEFINE_FUNC(strfun_type1)
#undef MUP_DEFINE_FUNC

    void DefineOprt(const string_type &a_strName, fun_type2 a_pFun, unsigned a_iPri=0, bool a_bAllowOpt = false);
    void DefineConst(const string_type &a_sName, value_type a_fVal);
    void DefineStrConst(const string_type &a_sName, const string_type &a_strVal);
    void DefineVar(const string_type &a_sName, value_type *a_fVar);
    void DefinePostfixOprt(const string_type &a_strFun, fun_type1 a_pOprt, bool a_bAllowOpt=true);
    void DefineInfixOprt(const string_type &a_strName, fun_type1 a_pOprt, int a_iPrec=prINFIX, bool a_bAllowOpt=true);
	void DefineGetAdress(const string_type &a_strName,fun_lptype a_pFun, int a_iPrec=prINFIX, bool a_bAllowOpt=true);

#define MUP_DEFINE_VOID_FUNC(TYPE)                                                               \
	inline void DefineVoidFun(const string_type &a_strName, TYPE a_pFun, bool a_bAllowOpt = true)  \
	{                                                                                              \
		AddCallback( a_strName, ParserCallback(a_pFun, a_bAllowOpt),						       \
		m_FunDef, ValidNameChars() );														       \
	}

	MUP_DEFINE_VOID_FUNC(fun_void_type1)
	MUP_DEFINE_VOID_FUNC(fun_void_type2)
	MUP_DEFINE_VOID_FUNC(fun_void_type3)
	MUP_DEFINE_VOID_FUNC(fun_void_type4)
	MUP_DEFINE_VOID_FUNC(fun_void_type5)
	MUP_DEFINE_VOID_FUNC(multfun_void_type)
	MUP_DEFINE_VOID_FUNC(strfun_void_type1)
#undef MUP_DEFINE_VOID_FUNC

	void UsingClass(bool usingclass)
	{
		(void)usingclass;
		m_pTokenReader->UsingClassDef(true);
    }

	template<class ACLASS>
	void DefineOrgClass(const char *a_szName,
						ACLASS *apclass)
	{
		(void)apclass;
		string_type a_sName(a_szName);

		classbasemap_type::iterator itor=m_ClassDefMap.find(a_sName);
		if (itor!=m_ClassDefMap.end())

			Error(ecNAME_CONFLICT);

		CheckName(a_sName, ValidNameChars());

		OrgClass<ACLASS> *paclass=new OrgClass<ACLASS>;

		m_ClassDefMap[a_sName] = (classbase*) paclass;

		ReInit();
	}

	template<class ACLASS>
	void DefineClass(const char *a_szName,
						ACLASS *apclass)
	{
		(void)apclass;

		string_type a_sName(a_szName);
		if (m_ClassDefMap.find(a_sName)!=m_ClassDefMap.end())
			Error(ecNAME_CONFLICT);

		CheckName(a_sName, ValidNameChars());

		ParserClass<ACLASS> *paclass=new ParserClass<ACLASS>;

		m_ClassDefMap[a_sName] = (classbase*) paclass;

		ReInit();
	}

  	
  	void DefineCreateClass(const char *a_szName,
  							const char *a_szstr)
	{

		string_type a_sName(a_szName);

		if (m_ClassDefMap.find(a_sName)!=m_ClassDefMap.end())
			Error(ecNAME_CONFLICT);

		CheckName(a_sName, ValidNameChars());

		CreateClass *paclass=new CreateClass(a_sName,this);

		m_ClassDefMap[a_sName] = (classbase*) paclass;

		CompileClassDeclara(a_szstr,paclass);
		ReInit();
    }

  	
  	void DefineCreateClasFun(const char *a_szClassName,
 		const char *a_szClassmemberFuncName,
 		const char *a_szFuncStr)
	{
		string_type a_sClassName(a_szClassName);
		string_type a_sClassmemberFuncName(a_szClassmemberFuncName);
		string_type a_sClassFunStr(a_szFuncStr);

		CreateClass *pCreateclass = ResolveCreateClass(a_sClassName);
		if (pCreateclass == NULL)
			return;

		pCreateclass->AddClassFun(a_sClassmemberFuncName,a_sClassFunStr);
	}

	
	template<class ACLASS, class TMETHOD>
	void DefineClassFunImpl(const char *a_szClassName,
							const char *a_szClassmemberFuncName,
							TMETHOD aparclass)
	{
		string_type a_sClassName(a_szClassName);
		string_type a_sClassmemberFuncName(a_szClassmemberFuncName);

		classbase *pclass = ResolveRegisteredClass(a_sClassName);
		if (pclass == NULL)
			return;

		ParserClass<ACLASS> *ptureclass=(ParserClass<ACLASS> *)pclass;
		ptureclass->AddClassFun(a_sClassmemberFuncName, aparclass);
		(void)ptureclass->GetFuncSignatureShape(a_sClassmemberFuncName);

		ReInit();
	}

#define CXPARSER_DEFINE_CLASSFUN_FORWARD(METHOD_SIG) \
	template<class ACLASS> \
	void DefineClassFun(const char *a_szClassName, \
		ACLASS *apclass, \
		const char *a_szClassmemberFuncName, \
		METHOD_SIG ) \
	{ \
		(void)apclass; \
		DefineClassFunImpl<ACLASS>(a_szClassName, a_szClassmemberFuncName, aparclass); \
	}

	CXPARSER_DEFINE_CLASSFUN_FORWARD(void (ACLASS::*aparclass)(void))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(void (ACLASS::*aparclass)(double))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(void (ACLASS::*aparclass)(double,double))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(void (ACLASS::*aparclass)(double,double,double))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(void (ACLASS::*aparclass)(double,double,double,double))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(void (ACLASS::*aparclass)(double,const char_type *))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(void (ACLASS::*aparclass)(int))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(void (ACLASS::*aparclass)(int,int))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(void (ACLASS::*aparclass)(int,double))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(void (ACLASS::*aparclass)(double,int))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(void (ACLASS::*aparclass)(int,int,int))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(void (ACLASS::*aparclass)(int,int,int,int))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(void (ACLASS::*aparclass)(int,int,int,int,int))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(void (ACLASS::*aparclass)(int,int,int,int,int,int))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(void (ACLASS::*aparclass)(int,int,int,int,int,int,int))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(void (ACLASS::*aparclass)(const char *))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(void (ACLASS::*aparclass)(void *))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(void (ACLASS::*aparclass)(charpvect &))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(void (ACLASS::*aparclass)(paramvect &))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(int (ACLASS::*aparclass)(charpvect &))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(int (ACLASS::*aparclass)(paramvect &))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(int (ACLASS::*aparclass)(int))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(int (ACLASS::*aparclass)(int,int))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(int (ACLASS::*aparclass)(int,double))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(int (ACLASS::*aparclass)(double,int))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(int (ACLASS::*aparclass)())
	CXPARSER_DEFINE_CLASSFUN_FORWARD(double (ACLASS::*aparclass)(void *))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(double (ACLASS::*aparclass)(double,const char_type *))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(double (ACLASS::*aparclass)(charpvect &))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(double (ACLASS::*aparclass)(paramvect &))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(double (ACLASS::*aparclass)(int))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(double (ACLASS::*aparclass)(int,int))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(double (ACLASS::*aparclass)(int,double))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(double (ACLASS::*aparclass)(double,int))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(double (ACLASS::*aparclass)(int,int,int))
	CXPARSER_DEFINE_CLASSFUN_FORWARD(double (ACLASS::*aparclass)())
	CXPARSER_DEFINE_CLASSFUN_FORWARD(char* (ACLASS::*aparclass)())

#undef CXPARSER_DEFINE_CLASSFUN_FORWARD

	void DefineClassFun(const char *a_szClassName,
		const char *a_szClassmemberFuncName,
		const char *a_szClassFuncStr )
	{
		
		string_type a_sClassName( a_szClassName);
		string_type a_sClassmemberFuncName(a_szClassmemberFuncName);
		string_type a_sFuncstr(a_szClassFuncStr);

		classbase *pclass = nullptr;
		classbasemap_type::iterator itor=m_ClassDefMap.find(a_sClassName);
		if (itor!=m_ClassDefMap.end())
			pclass=itor->second;
		else
			{
				Error(ecNAME_CONFLICT);
				return;
			}

		CreateClass *ptureclass=(CreateClass *)pclass;
		ptureclass->AddClassFun(a_sClassmemberFuncName, a_sFuncstr);

		ReInit();
    }
	void SetClassObjectPoint()
	{

    }
	void SetClassMemberVarPoint()
	{

	}

    
    void ClearVar();
    void ClearFun();
    void ClearConst();
    void ClearInfixOprt();
    void ClearPostfixOprt();
    void ClearOprt();
    void ClearClassObj();
    void RemoveVar(const string_type &a_strVarName);
	void RemoveClassObject(const string_type &a_strClassnName);
	void RemoveClassObject(const string_type &a_strClassnName,const string_type &a_strOjectName);
    const varmap_type& GetUsedVar() const;
    const varmap_type& GetVar() const;
    const valmap_type& GetConst() const;
    const string_type& GetExpr() const;
    const funmap_type& GetFunDef() const;

	const classbasemap_type& GetClassMap() const;
	const string_type& GetFormula() const;

    const char_type ** GetOprtDef() const
    {
      return (const char **)(&c_DefaultOprt[0]);
    }

    void DefineNameChars(const char_type *a_szCharset)
    {
      m_sNameChars = a_szCharset;
    }

    void DefineOprtChars(const char_type *a_szCharset)
    {
		m_sOprtChars = a_szCharset;
    }

    void DefineInfixOprtChars(const char_type *a_szCharset)
    {
		m_sInfixOprtChars = a_szCharset;
    }

    const char_type* ValidNameChars() const
    {
		assert(m_sNameChars.size());
		return m_sNameChars.c_str();
    }

    const char_type* ValidOprtChars() const
    {
		assert(m_sOprtChars.size());
		return m_sOprtChars.c_str();
    }

    const char_type* ValidInfixOprtChars() const
    {
		assert(m_sInfixOprtChars.size());
		return m_sInfixOprtChars.c_str();
    }

    void  Error( EErrorCodes a_iErrc,
                 int a_iPos = (int)mu::string_type::npos,
                 const string_type &a_strTok = string_type() ) const;
	void RunCode()
	{
		RunCollectionCmdCode();
	}

 protected:

    
    void Init()
    {
      InitCharSets();
      InitFun();
      InitConst();
      InitOprt();
    }

    virtual void InitCharSets() = 0;
    virtual void InitFun() = 0;
    virtual void InitConst() = 0;
    virtual void InitOprt() = 0;

 private:
    void Assign(const ParserBase &a_Parser);
    void InitTokenReader();
	void InitClassReader();
	void InitClassFunReader();
    void ReInit() const;
	void EndExpress() const;
    void AddCallback( const string_type &a_strName,
                      const ParserCallback &a_Callback,
                      funmap_type &a_Storage,
                      const char_type *a_szCharSet );

    void ApplyBinOprt(ParserStack<token_type> &a_stOpt,
                      ParserStack<token_type> &a_stVal,
					  ParserStack<token_type> &a_classobj) const;

    void ApplyFunc(ParserStack<token_type> &a_stOpt,
                   ParserStack<token_type> &a_stVal,
				   ParserStack<token_type> &a_classobj,
                   int iArgCount) const;

    token_type ApplyNumFunc(const token_type &a_FunTok,
                            const std::vector<token_type> &a_vArg) const;

    token_type ApplyStrFunc(const token_type &a_FunTok,
                            token_type &a_Arg) const;

    int GetOprtPri(const token_type &a_Tok) const;

    value_type ParseString() const;
    value_type ParseCmdCode() const;
    value_type ParseValue() const;

    void  ClearFormula();
    void  CheckName(const string_type &a_strName, const string_type &a_CharSet) const;

#if defined(MU_PARSER_DUMP_STACK) | defined(MU_PARSER_DUMP_CMDCODE)
    void StackDump(const ParserStack<token_type > &a_stVal,
                   const ParserStack<token_type > &a_stOprt) const;
#endif

    
	mutable ParseFunction  m_pParseFormula;
    mutable const ParserByteCode::map_type *m_pCmdCode;
    mutable ParserByteCode m_vByteCode;
    mutable stringbuf_type m_vStringBuf;

    stringbuf_type m_vStringVarBuf;

    std::unique_ptr <token_reader_type> m_pTokenReader;
    std::unique_ptr <ParserClassReader> m_pClassReader;
	std::unique_ptr <ParserClassFunctionReader> m_pClassFunReader;

    funmap_type  m_FunDef;
    funmap_type  m_PostOprtDef;
    funmap_type  m_InfixOprtDef;
    funmap_type  m_OprtDef;

	valmap_type  m_ConstDef;
    strmap_type  m_StrVarDef;
    varmap_type  m_VarDef;
	stringmap_type m_StringVarDef;

    bool m_bOptimize;
    bool m_bUseByteCode;
    bool m_bHasControlFlow;
    bool m_bBuiltInOp;
	bool m_bcolllection;

    string_type m_sNameChars;
    string_type m_sOprtChars;
    string_type m_sInfixOprtChars;
  public:
	mutable string_type m_StrCollection;
	mutable ParserByteCode m_vByteCodeCollection;
	mutable const ParserByteCode::map_type *m_pCmdCodeCollection;
	mutable classbasemap_type m_ClassDefMap;
	mutable string_type m_StringFormula;

    
    virtualclass *m_PVirClassGroup;

	ParserByteCode::storage_type GetCollectionStorage()
	{
		return m_vByteCodeCollection.GetStorageBaseData();
	}

	value_type RunCollectionCmdCode()const;

	void SetColllection(bool Collect)
	{
		m_bcolllection=Collect;
	}

	void ClearCollection()
	{
		m_vByteCodeCollection.clear();
		m_StrCollection=string_type("");
	}

	 ParserByteCode::storage_type GetStorageBase();

	 bool m_bstopcompile;
	 bool m_bstoprun;
	 void RunStop()
	 {
		m_bstoprun=true;
	 }
	 void CompileStop()
	 {
		 m_bstopcompile=true;
	 }
	 void RunOk()
	 {
		m_bstoprun=false;
	 }
	 void CompileOk()
	 {
		 m_bstopcompile=false;
	 }

	typedef ParserStack<token_type> TokeStack;

	mutable TokeStack m_OptStack;

	mutable map<string_type,TokeStack> m_mapoptstack;

	void RunOptString(const char *optstackname);
	void SetOptStack(const string_type & optstackname);
	void ClearOptStack();

	bool m_boptcollect;
	void SetOptCollect(bool bcollec)
	{
		m_boptcollect=bcollec;
	}

	value_type RunCollectionOpt() const;
	value_type RunOptStack(TokeStack & optstack) const;

	value_type RunOptVect_SetTime(TokeStack & optstack) const;
	value_type RunClassFuncCode(int * vFuncByteCode,int iFuncBytesize,stringbuf_type& strtab) const;

	void CopyRUNOpt(int ioptnum);
	void RunOpt(int ioptnum);

	void CompileClassDeclara(const string_type &a_strdeclarastr,CreateClass *paclass);
	void CompileFuncAndRunString(const string_type &a_strfucstr,const string_type &a_strclass,const string_type &a_strobj);
	void CompileFuncAndRunString(const string_type &a_strfucstr,const string_type &a_strclass,const string_type &a_strobj,const paramvect &aparms);
	classbase* ResolveRegisteredClass(const string_type &a_sClassName);
	CreateClass* ResolveCreateClass(const string_type &a_sClassName);

	CreateClass *m_pcreateclass;

    bool m_bprecompile;

	void* GetClassObj(const string_type & strclass,const string_type & strobj);
    void* GetClassObj(const string_type &  strclass,int iobjnum);
	int GetClassObjSum(const string_type &  strclass);
    string_type GetClassObjName(const string_type & strclass, int object_index) const;

    bool m_bcmd;

};

}

#endif

