

#ifndef MU_PARSER_TEST_H
#define MU_PARSER_TEST_H

#include <string>
#include <numeric>
#include "muParser.h"
#include "muParserInt.h"

#if defined(_WIN32x)
#ifdef _EX
#define   __declspec(dllexport)
#else
#define   __declspec(dllimport)
#endif
#endif

namespace mu
{

namespace Test
{

class  ParserTester
{
public:
    enum TestProfile
    {
      BasicProfile,
      ControlFlowProfile,
      FullProfile
    };

private:

    static value_type f1of1(value_type v) { return v;};

    static value_type f1of2(value_type v, value_type  ) {return v;};
    static value_type f2of2(value_type  , value_type v) {return v;};

    static value_type f1of3(value_type v, value_type  , value_type  ) {return v;};
    static value_type f2of3(value_type  , value_type v, value_type  ) {return v;};
    static value_type f3of3(value_type  , value_type  , value_type v) {return v;};

    static value_type f1of4(value_type v, value_type,   value_type  , value_type  ) {return v;}
    static value_type f2of4(value_type  , value_type v, value_type  , value_type  ) {return v;}
    static value_type f3of4(value_type  , value_type,   value_type v, value_type  ) {return v;}
    static value_type f4of4(value_type  , value_type,   value_type  , value_type v) {return v;}

	static value_type f1of5(value_type v, value_type,   value_type  , value_type  , value_type  ) { return v; }
	static value_type f2of5(value_type  , value_type v, value_type  , value_type  , value_type  ) { return v; }
	static value_type f3of5(value_type  , value_type,   value_type v, value_type  , value_type  ) { return v; }
	static value_type f4of5(value_type  , value_type,   value_type  , value_type v, value_type  ) { return v; }
	static value_type f5of5(value_type  , value_type,   value_type  , value_type  , value_type v) { return v; }

    static value_type Min(value_type a_fVal1, value_type a_fVal2) { return (a_fVal1<a_fVal2) ? a_fVal1 : a_fVal2; }

	static value_type Max(const value_type *a_afArg, int a_iArgc)
	{
		value_type fRes = 0;
		if (!a_iArgc)
			return fRes;

		fRes=a_afArg[0];
		for (int i=0; i<a_iArgc; ++i) fRes = max(fRes, a_afArg[i]);

		return fRes;
	}

    static value_type plus2(value_type v1) { return v1+2; }
    static value_type times3(value_type v1) { return v1*3; }
    static value_type sqr(value_type v1) { return v1*v1; }

    static value_type sign(value_type v) { return -v; }

    static value_type Sum(const value_type *a_afArg, int a_iArgc)
    {
      if (!a_iArgc)
        throw mu::Parser::exception_type("too few arguments for function sum.");

      value_type fRes=0;
      for (int i=0; i<a_iArgc; ++i) fRes += a_afArg[i];
      return fRes;
    }

    static value_type Rnd(value_type v)
    {
      return (value_type)(1+(v*std::rand()/(RAND_MAX+1.0)));
    }

    static value_type RndWithString(const char *)
    {
      return (value_type)(1+(1000.0f*std::rand()/(RAND_MAX+1.0)));
    }

    static value_type ValueOf(const char *)
    {
      return 123;
    }

    static value_type StrToFloat(const char *a_szMsg)
    {
      using namespace std;
      return atof(a_szMsg);
    }

	static void_type printword(value_type v)
	{

	}

	static value_type Milli(value_type v) { return v/(value_type)1e3; }

    static int c_iCount;

	int TestNames();
	int TestSyntax();
	int TestMultiArg();
	int TestVolatile();
	int TestPostFix();
	int TestFormula();
	int TestInfixOprt();
	int TestBinOprt();
	int TestVarConst();
	int TestInterface();
	int TestComment();
	int TestException();
    int TestStrArg();

	int TestClassVar();
	int TestFunction();
	int TestCollection();
	int TestblockStack();
	int TestWhileIfBlock();
	int TestCompileClassDef();

	int TestCompileProcess();

	int TestIfCondition();
	int TestElseCondition();
	int TestWhileCondition();
	int TestAllCondition();
	int TestClassVariadicBinding();
	int TestReturnCondition();

    void Abort() const;

	void ListFunction(const mu::ParserBase *pParser);

	void ListVar(mu::ParserBase  &Pparser);
	void ListClass(mu::ParserBase &Pparser);
	void ListFormula(mu::ParserBase &Pparser);
public:
    typedef int (ParserTester::*testfun_type)();

	
	ParserTester(TestProfile profile = FullProfile);

   ~ParserTester()
   {
   };

    ParserTester(const ParserTester &a_Obj)
    :m_vTestFun()
    ,m_stream(a_Obj.m_stream)
    {};

	void Run();
    void SetStream(std::ostream *a_stream);

private:
    std::vector<testfun_type> m_vTestFun;
    std::ostream *m_stream;

	void AddTest(testfun_type a_pFun);

    int EqnTest(const std::string &a_str, double a_fRes, bool a_fPass);
    int ThrowTest(const std::string &a_str, int a_iErrc, bool a_bFail = true);

    int EqnTestInt(const std::string &a_str, double a_fRes, bool a_fPass);

	int EqnTestClass(const std::string &a_str,
		const std::string &a_strclass,
		const std::string &a_strobj,
		double a_fRes, bool a_fPass);

};

}

}

#endif
