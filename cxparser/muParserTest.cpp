

#include "muParserTest.h"

#include <cstdio>
#include <cmath>
#include <iostream>

#define PARSER_CONST_PI  3.141592653589793238462643
#define PARSER_CONST_E   2.718281828459045235360287

using namespace std;
static int iusingtestnum=0;
static double g_last_fucn4_args[4] = {0.0, 0.0, 0.0, 0.0};
static int g_last_fucn_i4_args[4] = {0, 0, 0, 0};

class usingclass
{
public :
	int m_inumid;
	int m_variadic_num_count;
	double m_variadic_num_sum;
	int m_variadic_str_count;
	int m_variadic_str_chars;
	std::string m_variadic_str_joined;
	usingclass()
	{
		iusingtestnum++;
		m_inumid = iusingtestnum;
		m_variadic_num_count = 0;
		m_variadic_num_sum = 0.0;
		m_variadic_str_count = 0;
		m_variadic_str_chars = 0;
	}

	void ISCALL()
	{
		cout<<"iusing obj num ="<<m_inumid<<"\r\n";
	}

	void AFUCN()
	{
		cout<<"usingclass AFUCN Runing"<<"\r\n";
	};
	void BFUCN()
	{
		cout<<"usingclass BFUCN Runing"<<"\r\n";
	};
	void CFUCN()
	{

	};
	void FUCN1(double adouble)
	{
		cout<<"usingclass FUCN1(PARM="<<adouble<<") Runing"<<"\r\n";
	}
	void FUCN1_2(double adouble)
	{
		cout<<"usingclass FUCN1_2(PARM="<<adouble<<")"<<"\r\n";
	}
	void FUCN2(double adouble ,double bdouble)
	{
		cout<<"usingclass FUCN2(PARM1="<<adouble<<",PARM2="<<bdouble<<")"<<"\r\n";
	}
	void FUCN2_2(double adouble ,double bdouble)
	{
		cout<<"usingclass FUCN2_2(PARM1="<<adouble<<",PARM2="<<bdouble<<")"<<"\r\n";
	}
	void FUCN3(double adouble ,double bdouble,double cdouble)
	{
		cout<<"usingclass FUCN3(PARM1="<<adouble<<",PARM2="<<bdouble<< ",PARM3="<<cdouble<<")"<<"\r\n";
	}
	void FUCN3_2(double adouble ,double bdouble,double cdouble)
	{
		cout<<"usingclass FUCN3(PARM1="<<adouble<<",PARM2="<<bdouble<< ",PARM3="<<cdouble<<")"<<"\r\n";
	}
	void FUCN4(double adouble ,double bdouble,double cdouble,double ddouble )
	{
		g_last_fucn4_args[0] = adouble;
		g_last_fucn4_args[1] = bdouble;
		g_last_fucn4_args[2] = cdouble;
		g_last_fucn4_args[3] = ddouble;
		cout<<"usingclass FUCN4(PARM1="<<adouble<<",PARM2="<<bdouble<< ",PARM3="<<cdouble<<",PARM4="<<ddouble<<")"<<"\r\n";
	}
	void FUCN_I1(int aint)
	{
		cout<<"usingclass FUCN_I1(PARM1="<<aint<<")"<<"\r\n";
	}
	void FUCN_I2(int aint,int bint)
	{
		cout<<"usingclass FUCN_I2(PARM1="<<aint<<",PARM2="<<bint<<")"<<"\r\n";
	}
	void FUCN_I3(int aint,int bint,int cint)
	{
		cout<<"usingclass FUCN_I3(PARM1="<<aint<<",PARM2="<<bint<<",PARM3="<<cint<<")"<<"\r\n";
	}
	void FUCN_I4(int aint,int bint,int cint,int dint )
	{
		g_last_fucn_i4_args[0] = aint;
		g_last_fucn_i4_args[1] = bint;
		g_last_fucn_i4_args[2] = cint;
		g_last_fucn_i4_args[3] = dint;
		cout<<"usingclass FUCN_I4(PARM1="<<aint<<",PARM2="<<bint<<",PARM3="<<cint<<",PARM4="<<dint<<")"<<"\r\n";
	}
	void FUCN_I5(int aint,int bint,int cint,int dint,int eint )
	{
		cout<<"usingclass FUCN_I5(PARM1="<<aint<<",PARM2="<<bint<<",PARM3="<<cint<<",PARM4="<<dint<<",PARM5="<<eint<<")"<<"\r\n";
	}
	void FUCN_I6(int aint,int bint,int cint,int dint,int eint ,int fint)
	{
		cout<<"usingclass FUCN_I6(PARM1="<<aint<<",PARM2="<<bint<<",PARM3="<<cint<<",PARM4="<<dint<<",PARM5="<<eint<<",PARM6="<<fint<<")"<<"\r\n";
	}
	void FUCN_I7(int aint,int bint,int cint,int dint,int eint ,int fint,int gint)
	{
		cout<<"usingclass FUCN_I7(PARM1="<<aint<<",PARM2="<<bint<<",PARM3="<<cint<<",PARM4="<<dint<<",PARM5="<<eint<<",PARM6="<<fint<<",PARM7="<<gint<<")"<<"\r\n";
	}
	void FUCN_VOIDP(void*pusingclass)
	{
		cout<<"usingclass FUCN_VOIDP(PARM1=";
		((usingclass*)pusingclass)->ISCALL();
		cout<<")"<<"\r\n";
	}
	void FUCN_VOIDP2(void*pusingclass,void*pusingclass2)
	{
		cout<<"usingclass FUCN_VOIDP2(PARM1=";
		((usingclass*)pusingclass)->ISCALL();
		cout<<",PARM2=";
		((usingclass*)pusingclass2)->ISCALL();
		cout<<")"<<"\r\n";
	}
	void FUCN_VOIDP3(void*pusingclass,void*pusingclass2,void*pusingclass3)
	{
		cout<<"usingclass FUCN_VOIDP3(PARM1=";
		((usingclass*)pusingclass)->ISCALL();
		cout<<",PARM2=";
		((usingclass*)pusingclass2)->ISCALL();
		cout<<",PARM3=";
		((usingclass*)pusingclass3)->ISCALL();
		cout<<")"<<"\r\n";
	}
	void FUCN_VOIDP4(void*pusingclass,void*pusingclass2,void*pusingclass3,void*pusingclass4)
	{
		cout<<"usingclass FUCN_VOIDP4(PARM1=";
		((usingclass*)pusingclass)->ISCALL();
		cout<<",PARM2=";
		((usingclass*)pusingclass2)->ISCALL();
		cout<<",PARM3=";
		((usingclass*)pusingclass3)->ISCALL();
		cout<<",PARM4=";
		((usingclass*)pusingclass4)->ISCALL();
		cout<<")"<<"\r\n";
	}
	void FUCN_VOIDP5(void*pusingclass,void*pusingclass2,void*pusingclass3,void*pusingclass4,void*pusingclass5)
	{
		cout<<"usingclass FUCN_VOIDP5(PARM1=";
		((usingclass*)pusingclass)->ISCALL();
		cout<<",PARM2=";
		((usingclass*)pusingclass2)->ISCALL();
		cout<<",PARM3=";
		((usingclass*)pusingclass3)->ISCALL();
		cout<<",PARM4=";
		((usingclass*)pusingclass4)->ISCALL();
		cout<<",PARM5=";
		((usingclass*)pusingclass5)->ISCALL();
		cout<<")"<<"\r\n";
	}

	void FUCN_CHARP(const char *ptextchar)
	{
		cout<<"usingclass FUCN_CHARP(PARM1=";
		cout<< ptextchar;
		cout<<")"<<"\r\n";
	}
	double FUCN_I1_RE_DB(int aint)
	{
		cout<<"usingclass FUCN_I1_RE_DB(PARM1="<<aint<<")"<<"\r\n";
		return 1.1;
	}
	double FUCN_I2_RE_DB(int aint,int bint)
	{
		cout<<"usingclass FUCN_I2_RE_DB(PARM1="<<aint<<",PARM2="<<bint<<")"<<"\r\n";
		return 2.2;
	}
	int FUN_I2_RE_INT(int aint,int bint)
	{
		cout<<"usingclass FUN_I2_RE_INT(PARM1="<<aint<<",PARM2="<<bint<<")"<<"\r\n";
		return 2;
	}
	int FUN_I1_RE_INT(int aint )
	{
		cout<<"usingclass FUCN_I1_RE_DB(PARM1="<<aint<<")"<<"\r\n";
		return 1.1;
	}
	int FUN_0_RE_INT()
	{
		cout<<"usingclass FUN_0_RE_INT( )"<<"\r\n";
		return 111;
	}
	double FUN_0_RE_DOUBLE()
	{
		cout<<"usingclass FUN_0_RE_DOUBLE( )"<<"\r\n";
		return 1.11;
	}
	double FUN_I1_RE_DOUBLE(int it)
	{
		return it+1.222;
	}
	double FUN_VP_RE_DOUBLE(void *pusingclass)
	{
		cout<<"usingclass FUCN_VOIDP(PARM1=";
		((usingclass*)pusingclass)->ISCALL();
		cout<<")"<<"\r\n";
		return 3.333;
	}
	void FUCNX_2(double aint,double bint)
	{

	}
	void FUCN_ANY(mu::paramvect &values)
	{
		m_variadic_num_count++;
		m_variadic_num_sum = 0.0;
		for (size_t i = 0; i < values.size(); ++i)
			m_variadic_num_sum += values[i];
	}
	double FUN_ANY_RE_DOUBLE(mu::paramvect &values)
	{
		FUCN_ANY(values);
		return m_variadic_num_sum;
	}
	void FUCN_CHARP_ANY(mu::charpvect &values)
	{
		m_variadic_str_count++;
		m_variadic_str_chars = 0;
		m_variadic_str_joined.clear();
		for (size_t i = 0; i < values.size(); ++i)
		{
			m_variadic_str_chars += (int)values[i].size();
			m_variadic_str_joined += values[i];
			if (i + 1 < values.size())
				m_variadic_str_joined += "|";
		}
	}
	int FUN_CHARP_ANY_RE_INT(mu::charpvect &values)
	{
		FUCN_CHARP_ANY(values);
		return m_variadic_str_chars;
	}

};

namespace mu
{
  namespace Test
  {

    int ParserTester::c_iCount = 0;

    
    ParserTester::ParserTester(TestProfile profile)
      :m_vTestFun()
      ,m_stream(&std::cout)
    {
      switch (profile)
      {
      case BasicProfile:
        AddTest(&ParserTester::TestNames);
        AddTest(&ParserTester::TestSyntax);
        AddTest(&ParserTester::TestPostFix);
        AddTest(&ParserTester::TestInfixOprt);
        AddTest(&ParserTester::TestVarConst);
        AddTest(&ParserTester::TestMultiArg);
        AddTest(&ParserTester::TestFormula);
        AddTest(&ParserTester::TestInterface);
        AddTest(&ParserTester::TestBinOprt);
        AddTest(&ParserTester::TestException);
        AddTest(&ParserTester::TestStrArg);
        AddTest(&ParserTester::TestComment);
        break;

      case ControlFlowProfile:
        AddTest(&ParserTester::TestInfixOprt);
        AddTest(&ParserTester::TestIfCondition);
        AddTest(&ParserTester::TestElseCondition);
        AddTest(&ParserTester::TestWhileCondition);
        AddTest(&ParserTester::TestAllCondition);
        AddTest(&ParserTester::TestReturnCondition);
        break;

      case FullProfile:
      default:
        AddTest(&ParserTester::TestNames);
        AddTest(&ParserTester::TestSyntax);
        AddTest(&ParserTester::TestPostFix);
        AddTest(&ParserTester::TestInfixOprt);
        AddTest(&ParserTester::TestVarConst);
        AddTest(&ParserTester::TestMultiArg);
        AddTest(&ParserTester::TestFormula);
        AddTest(&ParserTester::TestInterface);
        AddTest(&ParserTester::TestBinOprt);
        AddTest(&ParserTester::TestException);
        AddTest(&ParserTester::TestStrArg);
        AddTest(&ParserTester::TestComment);
        AddTest(&ParserTester::TestIfCondition);
        AddTest(&ParserTester::TestElseCondition);
        AddTest(&ParserTester::TestWhileCondition);
        AddTest(&ParserTester::TestAllCondition);
        AddTest(&ParserTester::TestClassVariadicBinding);
        AddTest(&ParserTester::TestReturnCondition);
        break;
      }
      ParserTester::c_iCount = 0;
    }

    int ParserTester::TestInterface()
    {
      int iStat = 0;
      *m_stream << "testing member functions...";

      value_type afVal[3] = {1,2,3};
      Parser p;

	  try
	  {
		  p.DefineVar("a", &afVal[0]);
		  p.DefineVar("b", &afVal[1]);
		  p.DefineVar("c", &afVal[2]);
          p.SetExpr("a+b+c");
		  p.Eval();
	  }
      catch(...)
      {
        iStat += 1;
      }

      try
      {
        p.RemoveVar("c");
        p.Eval();
        iStat += 1;
      }
      catch(...)
      {

      }

      if (iStat==0)
        *m_stream << "passed" << "\r\n";
      else
        *m_stream << "\n  failed with " << iStat << " errors" << "\r\n";

      return iStat;
    }

    int ParserTester::TestStrArg()
    {
      int iStat = 0;
      *m_stream << "testing string arguments...";

      iStat += EqnTest("valueof(\"aaa\")+valueof(\"bbb\")  ", 246, true);
      iStat += EqnTest("2*(valueof(\"aaa\")-23)+valueof(\"bbb\")", 323, true);

      iStat += EqnTest("a*(atof(\"10\")-b)", 8, true);
      iStat += EqnTest("a-(atof(\"10\")*b)", -19, true);

      if (iStat==0)
        *m_stream << "passed" << "\r\n";
      else
        *m_stream << "\n  failed with " << iStat << " errors" << "\r\n";

      return iStat;
    }

    int ParserTester::TestBinOprt()
    {
      int iStat = 0;
      *m_stream << "testing binary operators...";

      iStat += EqnTest("1 xor 2", 3, true);
      iStat += EqnTest("a xor b", 3, true);
      iStat += EqnTest("1 xor 2 xor 3", 0, true);
      iStat += EqnTest("a xor b xor 3", 0, true);
      iStat += EqnTest("a xor b xor c", 0, true);
      iStat += EqnTest("(1 xor 2) xor 3", 0, true);
      iStat += EqnTest("(a xor b) xor c", 0, true);
      iStat += EqnTest("(a) xor (b) xor c", 0, true);
      iStat += EqnTest("1 or 2", 3, true);
      iStat += EqnTest("a or b", 3, true);

      iStat += EqnTest("a = b", 2, true);
      iStat += EqnTest("a = sin(b)", 0.909297, true);
      iStat += EqnTest("a = 1+sin(b)", 1.909297, true);
	  iStat += EqnTest("a = 1+max(b,0.2,0.3,0.4)", 3, true);
	  iStat += EqnTest("a = 1+max(2*b,1,2.3,0.4)", 5, true);
	  iStat += EqnTest("a = 1+max(2*b,b,a-4,0.4)", 5, true);

      iStat += EqnTestInt("1 | 2", 3, true);
      iStat += EqnTestInt("1 || 2", 1, true);
      iStat += EqnTestInt("123 & 456", 72, true);
      iStat += EqnTestInt("(123 & 456) % 10", 2, true);
      iStat += EqnTestInt("1 && 0", 0, true);
      iStat += EqnTestInt("123 && 456", 1, true);
      iStat += EqnTestInt("1 << 3", 8, true);
      iStat += EqnTestInt("8 >> 3", 1, true);
      iStat += EqnTestInt("10 ^ 10", 0, true);
      iStat += EqnTestInt("10 * 10 ^ 99", 7, true);
      iStat += EqnTestInt("9 / 4", 2, true);
      iStat += EqnTestInt("9 % 4", 1, true);
      iStat += EqnTestInt("IF(5%2,1,0)", 1, true);
      iStat += EqnTestInt("IF(4%2,1,0)", 0, true);
      iStat += EqnTestInt("-10+1", -9, true);
      iStat += EqnTestInt("1+2*3", 7, true);
      iStat += EqnTestInt("const1 != const2", 1, true);
      iStat += EqnTestInt("const1 != const2", 0, false);
      iStat += EqnTestInt("const1 == const2", 0, true);
      iStat += EqnTestInt("const1 == 1", 1, true);
      iStat += EqnTestInt("10*(const1 == 1)", 10, true);
      iStat += EqnTestInt("2*(const1 | const2)", 6, true);
      iStat += EqnTestInt("2*(const1 | const2)", 7, false);
      iStat += EqnTestInt("const1 < const2", 1, true);
      iStat += EqnTestInt("const2 > const1", 1, true);
      iStat += EqnTestInt("const1 <= 1", 1, true);
      iStat += EqnTestInt("const2 >= 2", 1, true);
      iStat += EqnTestInt("2*(const1 + const2)", 6, true);
      iStat += EqnTestInt("2*(const1 - const2)", -2, true);

      iStat += EqnTestInt("a != b", 1, true);
      iStat += EqnTestInt("a != b", 0, false);
      iStat += EqnTestInt("a == b", 0, true);
      iStat += EqnTestInt("a == 1", 1, true);
      iStat += EqnTestInt("10*(a == 1)", 10, true);
      iStat += EqnTestInt("2*(a | b)", 6, true);
      iStat += EqnTestInt("2*(a | b)", 7, false);
      iStat += EqnTestInt("a < b", 1, true);
      iStat += EqnTestInt("b > a", 1, true);
      iStat += EqnTestInt("a <= 1", 1, true);
      iStat += EqnTestInt("b >= 2", 1, true);
      iStat += EqnTestInt("2*(a + b)", 6, true);
      iStat += EqnTestInt("2*(a - b)", -2, true);
      iStat += EqnTestInt("a + (a << b)", 5, true);
      iStat += EqnTestInt("-2^2", -4, true);

      iStat += EqnTestInt("a + b * c", 7, true);
      iStat += EqnTestInt("a * b + c", 5, true);
      iStat += EqnTestInt("a<b && b>10", 0, true);
      iStat += EqnTestInt("a<b && b<10", 1, true);

      iStat += EqnTestInt("a + b << c", 17, true);
      iStat += EqnTestInt("a << b + c", 7, true);
      iStat += EqnTestInt("c * b < a", 0, true);
      iStat += EqnTestInt("c * b == 6 * a", 1, true);

      if (iStat==0)
        *m_stream << "passed" << "\r\n";
      else
        *m_stream << "\n  failed with " << iStat << " errors" << "\r\n";

      return iStat;
    }

    int ParserTester::TestNames()
    {
      int  iStat= 0,
           iErr = 0;
	  string astr;
     *m_stream << "testing name restriction enforcement...";

      Parser p;

  #define PARSER_THROWCHECK(DOMAIN, FAIL, EXPR, ARG) \
      iErr = 0;                                      \
      ParserTester::c_iCount++;                      \
      try                                            \
      {                                              \
        p.Define##DOMAIN(EXPR, ARG);                 \
		astr = EXPR ;                                \
        *m_stream << "\n  "<<astr<<" passed";        \
      }                                              \
      catch(Parser::exception_type&)                 \
      {                                              \
	  iErr = (FAIL==false) ? 0 : 1;                  \
	  astr = EXPR ;                                  \
        *m_stream << "\n  "<<astr<<" failed";        \
      }                                              \
      iStat += iErr;

      PARSER_THROWCHECK(Const, false, "0a", 1)
      PARSER_THROWCHECK(Const, false, "9a", 1)
      PARSER_THROWCHECK(Const, false, "+a", 1)
      PARSER_THROWCHECK(Const, false, "-a", 1)
      PARSER_THROWCHECK(Const, false, "a-", 1)
      PARSER_THROWCHECK(Const, false, "a*", 1)
      PARSER_THROWCHECK(Const, false, "a?", 1)
      PARSER_THROWCHECK(Const, false, "a?", 1)
      PARSER_THROWCHECK(Const, false, "a?", 1)
      PARSER_THROWCHECK(Const, true, "a", 1)
      PARSER_THROWCHECK(Const, true, "a_min", 1)
      PARSER_THROWCHECK(Const, true, "a_min0", 1)
      PARSER_THROWCHECK(Const, true, "a_min9", 1)

      value_type a;
      p.ClearConst();
      PARSER_THROWCHECK(Var, false, "9a", &a)
      PARSER_THROWCHECK(Var, false, "0a", &a)
      PARSER_THROWCHECK(Var, false, "+a", &a)
      PARSER_THROWCHECK(Var, false, "-a", &a)
      PARSER_THROWCHECK(Var, false, "?a", &a)
      PARSER_THROWCHECK(Var, false, "!a", &a)
      PARSER_THROWCHECK(Var, false, "a+", &a)
      PARSER_THROWCHECK(Var, false, "a-", &a)
      PARSER_THROWCHECK(Var, false, "a*", &a)
      PARSER_THROWCHECK(Var, false, "a?", &a)
      PARSER_THROWCHECK(Var, true, "a", &a)
      PARSER_THROWCHECK(Var, true, "a_min", &a)
      PARSER_THROWCHECK(Var, true, "a_min0", &a)
      PARSER_THROWCHECK(Var, true, "a_min9", &a)
      PARSER_THROWCHECK(Var, false, "a_min9", 0)

      PARSER_THROWCHECK(PostfixOprt, false, "(k", f1of1)
      PARSER_THROWCHECK(PostfixOprt, false, "9+", f1of1)
      PARSER_THROWCHECK(PostfixOprt, false, "+", 0)

      PARSER_THROWCHECK(PostfixOprt, true, "-a",  f1of1)
      PARSER_THROWCHECK(PostfixOprt, true, "?a",  f1of1)
      PARSER_THROWCHECK(PostfixOprt, true, "_",   f1of1)
      PARSER_THROWCHECK(PostfixOprt, true, "#",   f1of1)
      PARSER_THROWCHECK(PostfixOprt, true, "&&",  f1of1)
      PARSER_THROWCHECK(PostfixOprt, true, "||",  f1of1)

      PARSER_THROWCHECK(PostfixOprt, true, "|",   f1of1)
      PARSER_THROWCHECK(PostfixOprt, true, "++",  f1of1)
      PARSER_THROWCHECK(PostfixOprt, true, "--",  f1of1)
      PARSER_THROWCHECK(PostfixOprt, true, "?>",  f1of1)
      PARSER_THROWCHECK(PostfixOprt, true, "?<",  f1of1)
      PARSER_THROWCHECK(PostfixOprt, true, "**",  f1of1)
      PARSER_THROWCHECK(PostfixOprt, true, "xor", f1of1)
      PARSER_THROWCHECK(PostfixOprt, true, "and", f1of1)
      PARSER_THROWCHECK(PostfixOprt, true, "or",  f1of1)
      PARSER_THROWCHECK(PostfixOprt, true, "not", f1of1)
      PARSER_THROWCHECK(PostfixOprt, true, "!",   f1of1)

      PARSER_THROWCHECK(Oprt, false, "+",  f1of2)
      PARSER_THROWCHECK(Oprt, false, "-",  f1of2)
      PARSER_THROWCHECK(Oprt, false, "*",  f1of2)
      PARSER_THROWCHECK(Oprt, false, "/",  f1of2)

      p.EnableBuiltInOprt(false);
      PARSER_THROWCHECK(Oprt, true, "+",  f1of2)
      PARSER_THROWCHECK(Oprt, true, "-",  f1of2)
      PARSER_THROWCHECK(Oprt, true, "*",  f1of2)
      PARSER_THROWCHECK(Oprt, true, "/",  f1of2)
  #undef PARSER_THROWCHECK

      if (iStat==0)
        *m_stream << "passed" << "\r\n";
      else
		  *m_stream << "\n  failed with " << iStat << " errors:" << astr<<"\r\n";

      return iStat;
    }

    int ParserTester::TestSyntax()
    {
      int iStat = 0;
      *m_stream << "testing syntax engine...";

      iStat += EqnTest("(1+ 2*a)", 3, true);
      iStat += EqnTest("(2+", 0, false);
      iStat += EqnTest("2++4", 0, false);
      iStat += EqnTest("2+-4", 0, false);
      iStat += EqnTest("(2+)", 0, false);
      iStat += EqnTest("--2", 0, false);
      iStat += EqnTest("ksdfj", 0, false);

      iStat += EqnTest("sin(cos)", 0, false);
      iStat += EqnTest("5t6", 0, false);
      iStat += EqnTest("5 t 6", 0, false);
      iStat += EqnTest("8*", 0, false);
      iStat += EqnTest(",3", 0, false);
      iStat += EqnTest("3,5", 0, false);
      iStat += EqnTest("sin(8,8)", 0, false);
      iStat += EqnTest("(7,8)", 0, false);
      iStat += EqnTest("sin)", 0, false);
      iStat += EqnTest("a)", 0, false);
      iStat += EqnTest("pi)", 0, false);
      iStat += EqnTest("sin(())", 0, false);
      iStat += EqnTest("sin()", 0, false);

      if (iStat==0)
        *m_stream << "passed" << "\r\n";
      else
        *m_stream << "\n  failed with " << iStat << " errors" << "\r\n";

      return iStat;
    }

    int ParserTester::TestVarConst()
    {
      int iStat = 0;
      *m_stream << "testing variable/constant name recognition...";

      iStat += EqnTest("const", 1, true);
      iStat += EqnTest("const1", 2, true);
      iStat += EqnTest("const2", 3, true);
      iStat += EqnTest("2*const", 2, true);
      iStat += EqnTest("2*const1", 4, true);
      iStat += EqnTest("2*const2", 6, true);
      iStat += EqnTest("2*const+1", 3, true);
      iStat += EqnTest("2*const1+1", 5, true);
      iStat += EqnTest("2*const2+1", 7, true);
      iStat += EqnTest("const", 0, false);
      iStat += EqnTest("const1", 0, false);
      iStat += EqnTest("const2", 0, false);

      iStat += EqnTest("a", 1, true);
      iStat += EqnTest("aa", 2, true);
      iStat += EqnTest("2*a", 2, true);
      iStat += EqnTest("2*aa", 4, true);
      iStat += EqnTest("2*a-1", 1, true);
      iStat += EqnTest("2*aa-1", 3, true);

      try
      {
        int idx;
        mu::Parser p;
        mu::value_type vVarVal[] = { 1, 2, 3, 4, 5};
        p.DefineVar("a", &vVarVal[0]);
        p.DefineVar("b", &vVarVal[1]);
        p.DefineVar("c", &vVarVal[2]);
        p.DefineVar("d", &vVarVal[3]);
        p.DefineVar("e", &vVarVal[4]);

        p.SetExpr("a+b+c+d");
        mu::varmap_type UsedVar = p.GetUsedVar();
        int iCount = (int)UsedVar.size();
        if (iCount!=4) throw false;

        mu::varmap_type::const_iterator item = UsedVar.begin();
        for (idx=0; item!=UsedVar.end(); ++item)
        {
          if (&vVarVal[idx++]!=item->second)
            throw false;
        }

        p.SetExpr("undef1+undef2+undef3");
        UsedVar = p.GetUsedVar();
        iCount = (int)UsedVar.size();
        if (iCount!=3) throw false;

        for (item = UsedVar.begin(); item!=UsedVar.end(); ++item)
        {
          if (item->second!=0)
            throw false;
        }

        p.SetExpr("a+b");
        UsedVar = p.GetUsedVar();
        iCount = (int)UsedVar.size();
        if (iCount!=2) throw false;
        item = UsedVar.begin();
        for (idx=0; item!=UsedVar.end(); ++item)
          if (&vVarVal[idx++]!=item->second) throw false;

      }
      catch(...)
      {
        iStat += 1;
      }

      if (iStat==0)
        *m_stream << "passed" << "\r\n";
      else
        *m_stream << "\n  failed with " << iStat << " errors" << "\r\n";

      return iStat;
    }

	int ParserTester::TestFunction()
	{
		int iStat = 0;
		*m_stream << "testing multiarg functions...";

			Parser p;
			try
			{
				p.DefineVoidFun("printword",printword);
				iStat += EqnTest("sin(2)",  1, false);
				iStat += EqnTest("printword()",  1, true);
			}
			catch(...)
			{
				iStat += 1;
			}

		if (iStat==0)
			*m_stream << "passed" << "\r\n";
		else
			*m_stream << "\n  failed with " << iStat << " errors" << "\r\n";

		return iStat;
	}

    int ParserTester::TestMultiArg()
    {
      int iStat = 0;
      *m_stream << "testing multiarg functions...";

      iStat += EqnTest("f1of1(1)", 1, true);
      iStat += EqnTest("f1of2(1, 2)", 1, true);
      iStat += EqnTest("f2of2(1, 2)", 2, true);
      iStat += EqnTest("f1of3(1, 2, 3)", 1, true);
      iStat += EqnTest("f2of3(1, 2, 3)", 2, true);
      iStat += EqnTest("f3of3(1, 2, 3)", 3, true);
      iStat += EqnTest("f1of4(1, 2, 3, 4)", 1, true);
      iStat += EqnTest("f2of4(1, 2, 3, 4)", 2, true);
      iStat += EqnTest("f3of4(1, 2, 3, 4)", 3, true);
      iStat += EqnTest("f4of4(1, 2, 3, 4)", 4, true);
      iStat += EqnTest("f1of5(1, 2, 3, 4, 5)", 1, true);
      iStat += EqnTest("f2of5(1, 2, 3, 4, 5)", 2, true);
      iStat += EqnTest("f3of5(1, 2, 3, 4, 5)", 3, true);
      iStat += EqnTest("f4of5(1, 2, 3, 4, 5)", 4, true);
      iStat += EqnTest("f5of5(1, 2, 3, 4, 5)", 5, true);

      iStat += EqnTest("f1of1(1,2)", 0, false);
      iStat += EqnTest("f1of1()", 0, false);
      iStat += EqnTest("f1of2(1, 2, 3)", 0, false);
      iStat += EqnTest("f1of2(1)", 0, false);
      iStat += EqnTest("f1of3(1, 2, 3, 4)", 0, false);
      iStat += EqnTest("f1of3(1)", 0, false);
      iStat += EqnTest("f1of4(1, 2, 3, 4, 5)", 0, false);
      iStat += EqnTest("f1of4(1)", 0, false);
      iStat += EqnTest("(1,2,3)", 0, false);
      iStat += EqnTest("1,2,3", 0, false);
      iStat += EqnTest("(1*a,2,3)", 0, false);
      iStat += EqnTest("1,2*a,3", 0, false);

      iStat += EqnTest("min(a, 1)",  1, true);
      iStat += EqnTest("min(3*2, 1)",  1, true);
      iStat += EqnTest("min(3*2, 1)",  6, false);

      iStat += EqnTest("min(3*a+1, 1)",  1, true);
      iStat += EqnTest("max(3*a+1, 1)",  4, true);
      iStat += EqnTest("max(3*a+1, 1)*2",  8, true);
      iStat += EqnTest("2*max(3*a+1, 1)+2",  10, true);

      iStat += EqnTest("sum(1,2,3)",  6, true);
      iStat += EqnTest("2*sum(1,2,3)",  12, true);
      iStat += EqnTest("2*sum(1,2,3)+2",  14, true);
      iStat += EqnTest("2*sum(-1,2,3)+2",  10, true);
      iStat += EqnTest("2*sum(-1,2,-(-a))+2",  6, true);
      iStat += EqnTest("2*sum(-1,10,-a)+2",  18, true);
      iStat += EqnTest("2*sum(1,2,3)*2",  24, true);
      iStat += EqnTest("sum(1,-max(1,2),3)*2",  4, true);
      iStat += EqnTest("sum(1*3, 4, a+2)",  10, true);
      iStat += EqnTest("sum(1*3, 2*sum(1,2,2), a+2)",  16, true);
      iStat += EqnTest("sum(1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2)", 24, true);

      iStat += EqnTest("sum()",  0, false);
      iStat += EqnTest("sum(,)",  0, false);
      iStat += EqnTest("sum(1,2,)",  0, false);
      iStat += EqnTest("sum(,1,2)",  0, false);

      if (iStat==0)
        *m_stream << "passed" << "\r\n";
      else
        *m_stream << "\n  failed with " << iStat << " errors" << "\r\n";

      return iStat;
    }

    int ParserTester::TestInfixOprt()
    {
      int iStat = 0;
      *m_stream << "testing infix operators...";

      iStat += EqnTest("-1",    -1, true);
      iStat += EqnTest("-(-1)",  1, true);
      iStat += EqnTest("-(-1)*2",  2, true);
      iStat += EqnTest("-(-2)*sqrt(4)",  4, true);
      iStat += EqnTest("-a",  -1, true);
      iStat += EqnTest("-(a)",  -1, true);
      iStat += EqnTest("-(-a)",  1, true);
      iStat += EqnTest("-(-a)*2",  2, true);
      iStat += EqnTest("-(8)", -8, true);
      iStat += EqnTest("-8", -8, true);
      iStat += EqnTest("-(2+1)", -3, true);
      iStat += EqnTest("-(f1of1(1+2*3)+1*2)", -9, true);
      iStat += EqnTest("-(-f1of1(1+2*3)+1*2)", 5, true);
      iStat += EqnTest("-sin(8)", -0.989358, true);
      iStat += EqnTest("3-(-a)", 4, true);
      iStat += EqnTest("3--a", 4, true);
      iStat += EqnTest("a=-b+1", -1, true);
      iStat += EqnTest("a=3--b", 5, true);

      iStat += EqnTest("~2#", 8, true);
      iStat += EqnTest("~f1of1(2)#", 8, true);
      iStat += EqnTest("~(b)#", 8, true);
      iStat += EqnTest("(~b)#", 12, true);
      iStat += EqnTest("~(2#)", 8, true);
      iStat += EqnTest("~(f1of1(2)#)", 8, true);

      iStat += EqnTest("-2^2",-4, true);
      iStat += EqnTest("-(a+b)^2",-9, true);
      iStat += EqnTest("(-3)^2",9, true);
      iStat += EqnTest("-(-2^2)",4, true);
      iStat += EqnTest("3+-3^2",-6, true);

      iStat += EqnTest("-2?", -4, true);
      iStat += EqnTest("-(1+1)?",-4, true);
      iStat += EqnTest("2+-(1+1)?",-2, true);
      iStat += EqnTest("2+-2?", -2, true);

      iStat += EqnTest("$2^2",4, true);
      iStat += EqnTest("$(a+b)^2",9, true);
      iStat += EqnTest("($3)^2",9, true);
      iStat += EqnTest("$($2^2)",-4, true);
      iStat += EqnTest("3+$3^2",12, true);

      if (iStat==0)
        *m_stream << "passed" << "\r\n";
      else
        *m_stream << "\n  failed with " << iStat << " errors" << "\r\n";

      return iStat;
    }

    int ParserTester::TestPostFix()
    {
      int iStat = 0;
      *m_stream << "testing postfix operators...";

      iStat += EqnTest("3m+5", 5.003, true);
      iStat += EqnTest("1000m", 1, true);
      iStat += EqnTest("1000 m", 1, true);
      iStat += EqnTest("(a)m", 1e-3, true);
      iStat += EqnTest("-(a)m", -1e-3, true);
      iStat += EqnTest("-2m", -2e-3, true);
      iStat += EqnTest("f1of1(1000)m", 1, true);
      iStat += EqnTest("-f1of1(1000)m", -1, true);
      iStat += EqnTest("-f1of1(-1000)m", 1, true);
      iStat += EqnTest("f4of4(0,0,0,1000)m", 1, true);
      iStat += EqnTest("2+(a*1000)m", 3, true);

      iStat += EqnTest("1000m", 0.1, false);
      iStat += EqnTest("(a)m", 2, false);

      iStat += EqnTest("a m", 0, false);
      iStat += EqnTest("4 + m", 0, false);
      iStat += EqnTest("m4", 0, false);
      iStat += EqnTest("sin(m)", 0, false);
      iStat += EqnTest("m m", 0, false);
      iStat += EqnTest("m(8)", 0, false);
      iStat += EqnTest("4,m", 0, false);
      iStat += EqnTest("-m", 0, false);
      iStat += EqnTest("2(-m)", 0, false);
      iStat += EqnTest("2(m)", 0, false);

      if (iStat==0)
        *m_stream << "passed" << "\r\n";
      else
        *m_stream << "\n  failed with " << iStat << " errors" << "\r\n";

      return iStat;
    }

    int ParserTester::TestVolatile()
    {
      int iStat = 0;
      *m_stream << "testing volatile/nonvolatile functions...";

      try
      {

        mu::Parser p;
        p.DefineFun("rnd", Rnd, false);
        p.DefineFun("valueof", RndWithString, false);

        p.SetExpr("3+rnd(8)");
        if (p.Eval()==p.Eval()) iStat += 1;

        p.SetExpr("3+rnd(8)");
        p.Eval();
        if (p.Eval()==p.Eval()) iStat += 1;

        p.SetExpr("3*rnd(8)+3");
        p.Eval();
        if (p.Eval()==p.Eval()) iStat += 1;

        p.SetExpr("10+3*sin(rnd(8))-1");
        p.Eval();
        if (p.Eval()==p.Eval()) iStat += 1;

        p.SetExpr("3+rnd(rnd(8))*2");
        p.Eval();
        if (p.Eval()==p.Eval()) iStat += 1;

        p.SetExpr("valueof(\"Das ist ein Test\")");
        p.Eval();
        if (p.Eval()==p.Eval()) iStat += 1;
      }
      catch(Parser::exception_type &e)
      {
        *m_stream << "\n  " << e.GetExpr() << " : " << e.GetMsg();
        iStat += 1;
      }

      try
      {
        mu::Parser p;
        p.DefineFun("rnd", Rnd);
        p.DefineFun("valueof", RndWithString);

        p.SetExpr("3+rnd(8)");
        if (p.Eval()!=p.Eval()) iStat += 1;

        p.SetExpr("3+rnd(8)");
        p.Eval();
        if (p.Eval()!=p.Eval()) iStat += 1;

        p.SetExpr("3*rnd(8)+3");
        p.Eval();
        if (p.Eval()!=p.Eval()) iStat += 1;

        p.SetExpr("10+3*sin(rnd(8))-1");
        p.Eval();
        if (p.Eval()!=p.Eval()) iStat += 1;

        p.SetExpr("3+rnd(rnd(8))*2");
        p.Eval();
        if (p.Eval()!=p.Eval()) iStat += 1;

        p.SetExpr("valueof(\"Das ist ein Test\")");
        p.Eval();
        if (p.Eval()!=p.Eval()) iStat += 1;
      }
      catch(Parser::exception_type &e)
      {
        *m_stream << "\n  " << e.GetExpr() << " : " << e.GetMsg();
        iStat += 1;
      }

      if (iStat==0)
        *m_stream << "passed" << "\r\n";
      else
        *m_stream << "\n  failed with " << iStat << " errors" << "\r\n";

      return iStat;
    }

    int ParserTester::TestFormula()
    {
      int iStat = 0;
      *m_stream << "testing sample formulas...";

      iStat += EqnTest("1+2-3*4/5^6", 2.99923, true);
      iStat += EqnTest("1^2/3*4-5+6", 2.3333, true);
      iStat += EqnTest("1+2*3", 7, true);
      iStat += EqnTest("1+2*3", 7, true);
      iStat += EqnTest("(1+2)*3", 9, true);
      iStat += EqnTest("(1+2)*(-3)", -9, true);
      iStat += EqnTest("2/4", 0.5, true);

      iStat += EqnTest("exp(ln(7))", 7, true);
      iStat += EqnTest("e^ln(7)", 7, true);
      iStat += EqnTest("e^(ln(7))", 7, true);
      iStat += EqnTest("(e^(ln(7)))", 7, true);
      iStat += EqnTest("1-(e^(ln(7)))", -6, true);
      iStat += EqnTest("2*(e^(ln(7)))", 14, true);
      iStat += EqnTest("10^log(5)", 5, true);
      iStat += EqnTest("10^log10(5)", 5, true);
      iStat += EqnTest("2^log2(4)", 4, true);
      iStat += EqnTest("-(sin(0)+1)", -1, true);
      iStat += EqnTest("-(2^1.1)", -2.14354692, true);

      iStat += EqnTest("(cos(2.41)/b)", -0.372056, true);

      iStat += EqnTest(
        "(((-9))-e/(((((((pi-(((-7)+(-3)/4/e))))/(((-5))-2)-((pi+(-0))*(sqrt((e+e))*(-8))*(((-pi)+(-pi)-(-9)*(6*5))"
        "/(-e)-e))/2)/((((sqrt(2/(-e)+6)-(4-2))+((5/(-2))/(1*(-pi)+3))/8)*pi*((pi/((-2)/(-6)*1*(-1))*(-6)+(-e)))))/"
        "((e+(-2)+(-e)*((((-3)*9+(-e)))+(-9)))))))-((((e-7+(((5/pi-(3/1+pi)))))/e)/(-5))/(sqrt((((((1+(-7))))+((((-"
        "e)*(-e)))-8))*(-5)/((-e)))*(-6)-((((((-2)-(-9)-(-e)-1)/3))))/(sqrt((8+(e-((-6))+(9*(-9))))*(((3+2-8))*(7+6"
        "+(-5))+((0/(-e)*(-pi))+7)))+(((((-e)/e/e)+((-6)*5)*e+(3+(-5)/pi))))+pi))/sqrt((((9))+((((pi))-8+2))+pi))/e"
        "*4)*((-5)/(((-pi))*(sqrt(e)))))-(((((((-e)*(e)-pi))/4+(pi)*(-9)))))))+(-pi)", -12.23016549, true);

      iStat += EqnTest("1+2-3*4/5^6*(2*(1-5+(3*7^9)*(4+6*7-3)))+12", -7995810.09926, true);

      iStat += EqnTest(
          "(atan(sin((((((((((((((((pi/cos((a/((((0.53-b)-pi)*e)/b))))+2.51)+a)-0.54)/0.98)+b)*b)+e)/a)+b)+a)+b)+pi)/e"
          ")+a)))*2.77)", -2.16995656, true);

      if (iStat==0)
        *m_stream << "passed" << "\r\n";
      else
        *m_stream << "\n  failed with " << iStat << " errors" << "\r\n";

      return iStat;
    }

    int ParserTester::TestException()
    {
      int  iStat = 0;
      *m_stream << "testing error codes...";

      iStat += ThrowTest("3+",        ecUNEXPECTED_EOF);
      iStat += ThrowTest("3+)",       ecUNEXPECTED_PARENS);
      iStat += ThrowTest("sin(3,4)",  ecTOO_MANY_PARAMS);
      iStat += ThrowTest("3,4",       ecUNEXPECTED_COMMA);
      iStat += ThrowTest("IF(3)",     ecTOO_FEW_PARAMS);
      iStat += ThrowTest("(1+2",      ecMISSING_PARENS);
      iStat += ThrowTest("sin(3)3",   ecUNEXPECTED_VAL);
      iStat += ThrowTest("sin(3)xyz", ecUNASSIGNABLE_TOKEN);
      iStat += ThrowTest("sin(3)cos(3)", ecUNEXPECTED_FUN);

      iStat += ThrowTest("valueof(\"xxx\")",  999, false);
      iStat += ThrowTest("valueof()",          ecGENERIC);
      iStat += ThrowTest("1+valueof(\"abc\"",  ecMISSING_PARENS);
      iStat += ThrowTest("valueof(\"abc\"",    ecMISSING_PARENS);
      iStat += ThrowTest("valueof(\"abc",      ecUNTERMINATED_STRING);
      iStat += ThrowTest("valueof(\"abc\",3)", ecUNEXPECTED_ARG);
      iStat += ThrowTest("valueof(3)",         ecSTRING_EXPECTED);
      iStat += ThrowTest("sin(\"abc\")",       ecVAL_EXPECTED);
      iStat += ThrowTest("valueof(\"\\\"abc\\\"\")",  999, false);
      iStat += ThrowTest("\"hello world\"",    ecSTR_RESULT);
      iStat += ThrowTest("(\"hello world\")",  ecSTR_RESULT);
      iStat += ThrowTest("\"abcd\"+100",       ecOPRT_TYPE_CONFLICT);
      iStat += ThrowTest("\"a\"+\"b\"",        ecOPRT_TYPE_CONFLICT);

      iStat += ThrowTest("3=4", ecUNEXPECTED_OPERATOR);
      iStat += ThrowTest("sin(8)=4", ecUNEXPECTED_OPERATOR);
      iStat += ThrowTest("\"test\"=a", ecUNEXPECTED_OPERATOR);
      iStat += ThrowTest("sin=9", ecUNEXPECTED_OPERATOR);
      iStat += ThrowTest("(8)=5", ecUNEXPECTED_OPERATOR);
      iStat += ThrowTest("(a)=5", ecUNEXPECTED_OPERATOR);
      iStat += ThrowTest("a=\"tttt\"", ecOPRT_TYPE_CONFLICT);

      if (iStat==0)
        *m_stream << "passed" << "\r\n";
      else
         *m_stream << "\n  failed with " << iStat << " errors" << "\r\n";

      return iStat;
    }

	int ParserTester::TestComment()
	{
		ParserTester::c_iCount++;
		int iStat = 0;
		*m_stream << "testing comment...";

		value_type afVal[3] = {1,2,3};
		Parser p;

		int aint=0;
		double adouble=0;
		try
		{
			double *apdouble=0;
			p.DefineOrgClass("double",apdouble);
			p.UsingClass(true);
			*m_stream <<"\r\n";
			*m_stream <<"***********************"<<"\r\n";
			*m_stream <<"test    comment   Begin"<<"\r\n";
			*m_stream <<"***********************"<<"\r\n";

			p.SetExpr("double a=10;");
			p.Eval();
			*m_stream<< "Input: " <<"\r\n";
			ListFormula(p);
			*m_stream<< "Output: " <<"\r\n";
			ListVar(p);
			*m_stream<< "End " <<"\r\n\r\n";

			double* pda =(double*) p.GetClassObj("double","a");
			if(*pda !=10) iStat++;

			p.SetExpr("a=20;");
			p.Eval();
			*m_stream<< "Input: " <<"\r\n";
			ListFormula(p);
			*m_stream<< "Output: " <<"\r\n";
			ListVar(p);
			*m_stream<< "End " <<"\r\n\r\n";

			pda =(double*) p.GetClassObj("double","a");
			if(*pda !=20) iStat++;

			p.SetExpr("a=a+20;a=a+10;");
			p.Eval();
			*m_stream<< "Input: " <<"\r\n";
			ListFormula(p);
			*m_stream<< "Output: " <<"\r\n";
			ListVar(p);
			*m_stream<< "End " <<"\r\n\r\n";

			pda =(double*) p.GetClassObj("double","a");
			if(*pda !=50) iStat++;

		}
		catch(Parser::exception_type &e)
		{
			iStat += 1;
			*m_stream << " Parser exception: " << e.GetMsg() << "\r\n";
			*m_stream << " Token: " << e.GetToken() << "\r\n";
		}
		catch(std::exception &e)
		{
			iStat += 1;
			*m_stream << " std::exception: " << e.what() << "\r\n";
		}
		catch(...)
		{
			iStat += 1;
			*m_stream <<" TestComment() catch error "<<"\r\n";
		}

		if (iStat==0)
			*m_stream << "passed" << "\r\n";
		else
			*m_stream << "\n comment failed with " << iStat << " errors" << "\r\n";

		return iStat;
	}

	int ParserTester::TestClassVariadicBinding()
	{
		int iStat = 0;
		*m_stream << "testing class variadic binding...";

		Parser p;

		try
		{
			double *apdouble = 0;
			p.DefineOrgClass("double", apdouble);
			p.UsingClass(true);

			usingclass *atestclass = 0;
			p.DefineClass("usingclass", atestclass);
			p.DefineClassFun("usingclass", atestclass, "FUCN_ANY", &usingclass::FUCN_ANY);
			p.DefineClassFun("usingclass", atestclass, "FUN_ANY_RE_DOUBLE", &usingclass::FUN_ANY_RE_DOUBLE);
			p.DefineClassFun("usingclass", atestclass, "FUCN_CHARP_ANY", &usingclass::FUCN_CHARP_ANY);
			p.DefineClassFun("usingclass", atestclass, "FUN_CHARP_ANY_RE_INT", &usingclass::FUN_CHARP_ANY_RE_INT);

			double sumret = 0.0;
			double charret = 0.0;
			p.DefineVar("sumret", &sumret);
			p.DefineVar("charret", &charret);

			p.SetExpr("usingclass atestclass;sumret=atestclass.FUN_ANY_RE_DOUBLE(10,20,30,40);charret=atestclass.FUN_CHARP_ANY_RE_INT(\"one\",\"three\");atestclass.FUCN_ANY(1,2,3);atestclass.FUCN_CHARP_ANY(\"alpha\",\"beta\",\"g\");");
			p.Eval();

			usingclass *pclassobj = (usingclass*)p.GetClassObj("usingclass", "atestclass");
			if (pclassobj == 0)
				iStat++;
			else
			{
				if (sumret != 100.0) iStat++;
				if (charret != 8.0) iStat++;
				if (pclassobj->m_variadic_num_count != 2) iStat++;
				if (pclassobj->m_variadic_num_sum != 6.0) iStat++;
				if (pclassobj->m_variadic_str_count != 2) iStat++;
				if (pclassobj->m_variadic_str_chars != 10) iStat++;
				if (pclassobj->m_variadic_str_joined != "alpha|beta|g") iStat++;
			}
		}
		catch(Parser::exception_type &e)
		{
			iStat += 1;
			*m_stream << " Parser exception: " << e.GetMsg() << "\r\n";
			*m_stream << " Token: " << e.GetToken() << "\r\n";
		}
		catch(std::exception &e)
		{
			iStat += 1;
			*m_stream << " std::exception: " << e.what() << "\r\n";
		}
		catch(...)
		{
			iStat += 1;
			*m_stream << " TestClassVariadicBinding() catch error " << "\r\n";
		}

		if (iStat==0)
			*m_stream << "passed" << "\r\n";
		else
			*m_stream << "\n class variadic binding failed with " << iStat << " errors" << "\r\n";

		return iStat;
	}

	int ParserTester::TestClassVar()
	{
		int iStat = 0;
		*m_stream << "testing member functions...";

		value_type afVal[3] = {1,2,3};
		Parser p;

		int aint=0;
		double adouble=0;

		try
		{
			double *apdouble=0;
			p.DefineOrgClass("double",apdouble);
			p.UsingClass(true);
			*m_stream <<"\r\n";
			*m_stream <<"***********************"<<"\r\n";
			*m_stream <<"test class define Begin"<<"\r\n";
			*m_stream <<"***********************"<<"\r\n";

			p.SetExpr("double a;");
			p.Eval();
			ListFormula(p);
			ListVar(p);

			p.SetExpr("double b;");
			p.Eval();
			ListFormula(p);
			ListVar(p);
			ListClass(p);

			p.SetExpr("b=20;");
			p.Eval();
			ListFormula(p);
			ListVar(p);
			ListClass(p);

			p.SetExpr("double c;c=20;");
			p.Eval();
			ListFormula(p);
			ListVar(p);
			ListClass(p);

			p.SetExpr("a=sin(b);c=(a+c)/2;");
			p.Eval();
			ListFormula(p);
			ListVar(p);
			ListClass(p);

			int *aint=0;
			p.DefineOrgClass("int",aint);
			p.SetExpr("int itempc;");
			p.Eval();
			ListFormula(p);
			ListVar(p);
			ListClass(p);

			p.SetExpr("int ac;double ca;double bc;double ec=0;ca=sin(ec);bc=cos(ec);");
			p.Eval();
			ListFormula(p);
			ListVar(p);
			ListClass(p);

			p.SetExpr("ca=10;bc=2;ca=sin(ec+ca);bc=cos(ec+ca);");
			p.Eval();
			ListFormula(p);
			ListVar(p);
			ListClass(p);
			int inum=0;

			usingclass* atestclass=0;

			p.DefineClass("usingclass",atestclass);

			*m_stream <<"\r\n";
			*m_stream <<"*************************"<<"\r\n";
			*m_stream <<"Test DefineClassFun Begin"<<"\r\n";
			*m_stream <<"*************************"<<"\r\n";

			p.DefineClassFun("usingclass",atestclass,"AFUCN",&usingclass::AFUCN);
			p.DefineClassFun("usingclass",atestclass,"BFUCN",&usingclass::BFUCN);
			p.DefineClassFun("usingclass",atestclass,"FUCN1",&usingclass::FUCN1);
			p.DefineClassFun("usingclass",atestclass,"FUCN2",&usingclass::FUCN2);
			p.DefineClassFun("usingclass",atestclass,"FUCN2_2",&usingclass::FUCN2_2);
			p.DefineClassFun("usingclass",atestclass,"FUCNX_2",&usingclass::FUCNX_2);

			p.DefineClassFun("usingclass",atestclass,"FUCN3",&usingclass::FUCN3);
			p.DefineClassFun("usingclass",atestclass,"FUCN4",&usingclass::FUCN4);
			p.DefineClassFun("usingclass",atestclass,"FUCN_I1",&usingclass::FUCN_I1);
			p.DefineClassFun("usingclass",atestclass,"FUCN_I2",&usingclass::FUCN_I2);
			p.DefineClassFun("usingclass",atestclass,"FUCN_I3",&usingclass::FUCN_I3);
			p.DefineClassFun("usingclass",atestclass,"FUCN_I4",&usingclass::FUCN_I4);
			p.DefineClassFun("usingclass",atestclass,"FUCN_I5",&usingclass::FUCN_I5);
			p.DefineClassFun("usingclass",atestclass,"FUCN_I6",&usingclass::FUCN_I6);
			p.DefineClassFun("usingclass",atestclass,"FUCN_I7",&usingclass::FUCN_I7);
			p.DefineClassFun("usingclass",atestclass,"FUCN_VOIDP",&usingclass::FUCN_VOIDP);

			p.DefineClassFun("usingclass",atestclass,"FUCN_CHARP",&usingclass::FUCN_CHARP);
			p.DefineClassFun("usingclass",atestclass,"FUCN_I1_RE_DB",&usingclass::FUCN_I1_RE_DB);
			p.DefineClassFun("usingclass",atestclass,"FUCN_I2_RE_DB",&usingclass::FUCN_I2_RE_DB);

			p.DefineClassFun("usingclass",atestclass,"FUN_I2_RE_INT",&usingclass::FUN_I2_RE_INT);
			p.DefineClassFun("usingclass",atestclass,"FUN_I1_RE_INT",&usingclass::FUN_I1_RE_INT);
			p.DefineClassFun("usingclass",atestclass,"FUN_0_RE_INT",&usingclass::FUN_0_RE_INT);
			p.DefineClassFun("usingclass",atestclass,"FUN_0_RE_DOUBLE",&usingclass::FUN_0_RE_DOUBLE);
			p.DefineClassFun("usingclass",atestclass,"FUN_I1_RE_DOUBLE",&usingclass::FUN_I1_RE_DOUBLE);
			p.DefineClassFun("usingclass",atestclass,"FUN_VP_RE_DOUBLE",&usingclass::FUN_VP_RE_DOUBLE);

			p.DefineClassFun("usingclass",atestclass,"ISCALL",&usingclass::ISCALL);

			p.SetExpr("usingclass atestclass;");

			p.Eval();

			ListFormula(p);

			p.SetExpr("atestclass.AFUCN();");

			p.Eval();

			ListFormula(p);

			p.SetExpr("atestclass.FUCN1(1);");

			p.Eval();

			ListFormula(p);

			p.SetExpr("atestclass.FUCN2(1,2);");

			p.Eval();

			ListFormula(p);

			p.SetExpr("atestclass.FUCN3(1,2,3);");

			p.Eval();

			ListFormula(p);

			p.SetExpr("atestclass.FUCN4(1,2,3,4);");

			p.Eval();

			ListFormula(p);
			if (g_last_fucn4_args[0] != 1.0 || g_last_fucn4_args[1] != 2.0 ||
				g_last_fucn4_args[2] != 3.0 || g_last_fucn4_args[3] != 4.0)
			{
				*m_stream << " Class double 4-arg order failed" << "\r\n";
				iStat++;
			}

			p.SetExpr("atestclass.FUCN_I1(1);");

			p.Eval();

			ListFormula(p);

			p.SetExpr("atestclass.FUCN_I2(1,2);");

			p.Eval();

			ListFormula(p);

			p.SetExpr("atestclass.FUCN_I3(1,2,3);");

			p.Eval();

			ListFormula(p);

			p.SetExpr("atestclass.FUCN_I4(1,2,3,4);");

			p.Eval();

			ListFormula(p);
			if (g_last_fucn_i4_args[0] != 1 || g_last_fucn_i4_args[1] != 2 ||
				g_last_fucn_i4_args[2] != 3 || g_last_fucn_i4_args[3] != 4)
			{
				*m_stream << " Class int 4-arg order failed" << "\r\n";
				iStat++;
			}

			p.SetExpr("atestclass.FUCN_I5(1,2,3,4,5);");

			p.Eval();

			ListFormula(p);

			p.SetExpr("atestclass.FUCN_I6(1,2,3,4,5,6);");

			p.Eval();

			ListFormula(p);

			p.SetExpr("atestclass.FUCN_I7(1,2,3,4,5,6,7);");

			p.Eval();

			ListFormula(p);

			p.SetExpr("atestclass.FUCN2(1,2);");

			p.Eval();

			ListFormula(p);

			p.SetExpr("atestclass.FUCN2_2(3,4);");

			p.Eval();

			ListFormula(p);

			p.SetExpr("bc=atestclass.FUCN_I1_RE_DB(1);");

			p.Eval();

			ListVar(p);

			ListFormula(p);

			p.SetExpr("bc=atestclass.FUCN_I2_RE_DB(1,2);");

			p.Eval();

			ListVar(p);

			ListFormula(p);

			p.SetExpr("bc=atestclass.FUN_I1_RE_DOUBLE(1);");

			p.Eval();

			ListVar(p);

			ListFormula(p);

			p.SetExpr("usingclass atestclassn;bc=atestclass.FUN_VP_RE_DOUBLE(atestclassn);");

			p.Eval();

			ListVar(p);

			ListFormula(p);

			p.SetExpr("bc=atestclass.FUN_I2_RE_INT(1,2);");

			p.Eval();

			ListVar(p);

			ListFormula(p);

			p.SetExpr("bc=atestclass.FUN_I1_RE_INT(1);");

			p.Eval();

			ListVar(p);

			ListFormula(p);

			p.SetExpr("bc=atestclass.FUN_0_RE_INT();");

			p.Eval();

			ListVar(p);

			ListFormula(p);

			p.SetExpr("bc=atestclass.FUN_0_RE_DOUBLE();");

			p.Eval();

			ListVar(p);

			ListFormula(p);

			p.SetExpr("bc=sin(ca);atestclass.FUCN2(2,bc);");

			p.Eval();

			ListFormula(p);

			p.SetExpr("usingclass atestclass2;atestclass.FUCN_VOIDP(atestclass2);");

			p.Eval();

			ListFormula(p);

			ListVar(p);

			ListClass(p);

			*m_stream <<"\r\n";
			*m_stream <<"*************************"<<"\r\n";
			*m_stream <<"Test { class }           "<<"\r\n";
			*m_stream <<"*************************"<<"\r\n";

			p.SetExpr("{usingclass atestclass3;}");

			p.Eval();

			ListFormula(p);

			ListClass(p);

			p.SetExpr("{usingclass atestclass4;usingclass atestclass5;usingclass atestclass6;usingclass atestclass7;}");

			p.Eval();

			ListFormula(p);

			ListClass(p);

			p.SetExpr("{usingclass atestclass8;{usingclass atestclass9;}usingclass atestclass10;}usingclass atestclass11;");

			p.Eval();

			ListFormula(p);

			ListClass(p);

			p.SetExpr("{usingclass atemp1;atemp1.FUCN2(1,2);{usingclass atemp2;atemp2.FUCN1(1);}double adx1=10;}");

			p.Eval();

			ListFormula(p);

			ListClass(p);

			*m_stream <<"\r\n";
			*m_stream <<"*************************"<<"\r\n";
			*m_stream <<"Test class assgin          "<<"\r\n";
			*m_stream <<"*************************"<<"\r\n";

			p.SetExpr("usingclass atestassgin1;usingclass atestassgin2;atestassgin1.ISCALL();atestassgin2.ISCALL();");

			p.Eval();

			ListFormula(p);

			ListClass(p);

			p.SetExpr("atestassgin1 = atestassgin2;atestassgin1.ISCALL();atestassgin2.ISCALL();");

			p.Eval();

			ListFormula(p);

			ListClass(p);

			p.UsingClass(false);

			*m_stream <<"\r\n";
			*m_stream <<"***************************"<<"\r\n";
			*m_stream <<"Test char class member func"<<"\r\n";
			*m_stream <<"***************************"<<"\r\n";

			p.SetExpr("atestclass.FUCN_CHARP(\"hello world!\");");

			p.Eval();

			ListFormula(p);

			ListVar(p);

			ListClass(p);

			*m_stream <<"\r\n";
			*m_stream <<"***************************"<<"\r\n";
			*m_stream <<"Test clear class member func"<<"\r\n";
			*m_stream <<"***************************"<<"\r\n";

			p.ClearClassObj();

			ListClass(p);

			p.SetExpr("usingclass atestassgin1;usingclass atestassgin2;atestassgin1.ISCALL();atestassgin2.ISCALL();");

			p.Eval();

			ListFormula(p);

			ListClass(p);

			*m_stream <<"*************************"<<"\r\n";
			*m_stream <<"     Test Class end"<<"\r\n";
			*m_stream <<"*************************"<<"\r\n";

		}
		catch(...)
		{
			iStat += 1;
		}

		if (iStat==0)
			*m_stream << "passed" << "\r\n";
		else
			*m_stream << "\n  failed with " << iStat << " errors" << "\r\n";

		return iStat;
	}

	int ParserTester::TestIfCondition()
	{
		ParserTester::c_iCount++;
		int iStat = 0;
		*m_stream << "testing if condition...";

		Parser p;
		double a = 0;
		double b = 0;
		double c = 0;
		double d = 0;
		try
		{
			p.DefineVar("a", &a);
			p.DefineVar("b", &b);
			p.DefineVar("c", &c);
			p.DefineVar("d", &d);
			*m_stream <<"\r\n";
			*m_stream <<"***********************"<<"\r\n";
			*m_stream <<"test if condition Begin"<<"\r\n";
			*m_stream <<"***********************"<<"\r\n";

			p.SetExpr("a=0;b=0;c=0;d=0;if(a>0){d=d+1;a=10;}if(a<1){a=100;d=10;}");
			p.Eval();
			p.Eval();
			*m_stream<< "Input: " <<"\r\n";
			ListFormula(p);
			*m_stream<< "Output: " <<"\r\n";
			ListVar(p);
			*m_stream<< "End " <<"\r\n\r\n";

			if(a !=100) iStat++;
			if(d !=10) iStat++;

			p.SetExpr(
			"\
a=0;d=0;c=0;b=0;\
if(a>0) \
{ \
a=100; \
if(b>0)\
{\
b=100;\
}\
a=a/3;  \
} \
if(c<1)\
{ \
c=100;\
if(d<1)\
{\
d=100;\
}\
c=c/3;\
d=(d+100)/3;\
}"
			);
			p.Eval();
			*m_stream<< "Input: " <<"\r\n";
			ListFormula(p);
			*m_stream<< "Output: " <<"\r\n";
			ListVar(p);
			*m_stream<< "End " <<"\r\n\r\n";

			if(a !=0) iStat++;
			if(b !=0) iStat++;
			if(c !=100.0/3.0) iStat++;
			if(d !=200.0/3.0) iStat++;
		}
		catch(Parser::exception_type &e)
		{
			iStat += 1;
			*m_stream << " Parser exception: " << e.GetMsg() << "\r\n";
			*m_stream << " Token: " << e.GetToken() << "\r\n";
		}
		catch(std::exception &e)
		{
			iStat += 1;
			*m_stream << " std::exception: " << e.what() << "\r\n";
		}
		catch(...)
		{
			iStat += 1;
			*m_stream <<" TestComment() catch error "<<"\r\n";
		}

		if (iStat==0)
			*m_stream << "passed" << "\r\n";
		else
			*m_stream << "\n  failed with " << iStat << " errors" << "\r\n";

		return iStat;
	}

	int ParserTester::TestElseCondition()
	{
		ParserTester::c_iCount++;
		int iStat = 0;
		*m_stream << "testing if condition...";

		Parser p;
		double a = 0;
		double b = 0;
		double c = 0;
		double d = 0;
		try
		{
			p.DefineVar("a", &a);
			p.DefineVar("b", &b);
			p.DefineVar("c", &c);
			p.DefineVar("d", &d);
			*m_stream <<"\r\n";
			*m_stream <<"***********************"<<"\r\n";
			*m_stream <<"test if condition Begin"<<"\r\n";
			*m_stream <<"***********************"<<"\r\n";

			p.SetExpr("a=0;b=0;c=0;d=0;if(a>0){d=d+1;a=10;}else{a=100;d=10;}");
			p.Eval();
			p.Eval();
			*m_stream<< "Input: " <<"\r\n";
			ListFormula(p);
			*m_stream<< "Output: " <<"\r\n";
			ListVar(p);
			*m_stream<< "End " <<"\r\n\r\n";

			if(a !=100) iStat++;
			if(d !=10) iStat++;

			p.SetExpr(
				"\
a=0;d=0;c=0;b=0;\
if(a>0) \
{ \
a=100; \
if(b>0)\
{\
b=100;\
}\
} \
else\
{\
a=a+10/3;\
}\
if(c<1)\
{ \
c=100;\
if(d<1)\
{\
d=100;\
}\
else\
{\
c=c/3;\
}\
d=(d+100)/3;\
}\
else\
{\
c=c+3;\
}"
				 );
			p.Eval();
			*m_stream<< "Input: " <<"\r\n";
			ListFormula(p);
			*m_stream<< "Output: " <<"\r\n";
			ListVar(p);
			*m_stream<< "End " <<"\r\n\r\n";

			if(a !=10.0/3.0) iStat++;
			if(b !=0) iStat++;
			if(c !=103) iStat++;
			if(d !=200.0/3.0) iStat++;
		}
		catch(Parser::exception_type &e)
		{
			iStat += 1;
			*m_stream << " Parser exception: " << e.GetMsg() << "\r\n";
			*m_stream << " Token: " << e.GetToken() << "\r\n";
		}
		catch(std::exception &e)
		{
			iStat += 1;
			*m_stream << " std::exception: " << e.what() << "\r\n";
		}
		catch(...)
		{
			iStat += 1;
			*m_stream <<" TestComment() catch error "<<"\r\n";
		}

		if (iStat==0)
			*m_stream << "passed" << "\r\n";
		else
			*m_stream << "\n  failed with " << iStat << " errors" << "\r\n";

		return iStat;
	}

	int ParserTester::TestWhileCondition()
	{
		ParserTester::c_iCount++;
		int iStat = 0;
		*m_stream << "testing while condition...";

		Parser p;
		double a = 0;
		double b = 0;
		double c = 0;
		double d = 0;
		try
		{
			p.DefineVar("a", &a);
			p.DefineVar("b", &b);
			p.DefineVar("c", &c);
			p.DefineVar("d", &d);
			*m_stream <<"\r\n";
			*m_stream <<"***********************"<<"\r\n";
			*m_stream <<"test while condition Begin"<<"\r\n";
			p.SetExpr("a=10;b=0;c=0;d=0;while(a>0){d=d+1;a=a-1;}while(c<100){c=c+1;b=b+1;}");
			p.Eval();
			*m_stream<< "Input: " <<"\r\n";
			ListFormula(p);
			*m_stream<< "Output: " <<"\r\n";
			ListVar(p);
			*m_stream<< "End " <<"\r\n\r\n";

			if(a !=0) iStat++;
			if(b !=100) iStat++;
			if(c !=100) iStat++;
			if(d !=10) iStat++;

			p.SetExpr(
			"\
a=10;b=10;c=0;d=0;\
while(a>0) \
{ \
a=a-1; \
b=10;\
while(b>0)\
{\
b=b-1;\
c=c+1;\
}\
d=d+1;\
}"
			);
			p.Eval();
			*m_stream<< "Input: " <<"\r\n";
			ListFormula(p);
			*m_stream<< "Output: " <<"\r\n";
			ListVar(p);
			*m_stream<< "End " <<"\r\n\r\n";

			if(a !=0) iStat++;
			if(b !=0) iStat++;
			if(c !=100 ) iStat++;
			if(d !=10) iStat++;
		}
		catch(...)
		{
			iStat += 1;
			*m_stream <<" TestComment() catch error "<<"\r\n";
		}

		if (iStat==0)
			*m_stream << "passed" << "\r\n";
		else
			*m_stream << "\n  failed with " << iStat << " errors" << "\r\n";

		return iStat;
	}

	int ParserTester::TestAllCondition()
	{
		ParserTester::c_iCount++;
		int iStat = 0;
		*m_stream << "testing all condition...";

		value_type afVal[3] = {1,2,3};
		Parser p;

		int aint=0;
		double adouble=0;
		try
		{
			double *apdouble=0;
			p.DefineOrgClass("double",apdouble);
			p.UsingClass(true);
			*m_stream <<"\r\n";
			*m_stream <<"***********************"<<"\r\n";
			*m_stream <<"test All condition Begin"<<"\r\n";
			p.SetExpr("\
double a=0;\
double b=0;\
double c=0;\
double d=0;\
while(a==0)\
{\
d=d+1;\
if(d==10)\
{\
a=a+1;\
while(c<10)\
{\
c=c+1;\
if(c>5)\
{\
b=b+1;\
}\
}\
}\
}\
			");
			p.Eval();
			*m_stream<< "Input: " <<"\r\n";
			ListFormula(p);
			*m_stream<< "Output: " <<"\r\n";
			ListVar(p);
			*m_stream<< "End " <<"\r\n\r\n";

			double* pda = NULL;
			pda =(double*) p.GetClassObj("double","a");
			if(*pda !=1) iStat++;
			pda =(double*) p.GetClassObj("double","b");
			if(*pda !=5) iStat++;
			pda =(double*) p.GetClassObj("double","c");
			if(*pda !=10) iStat++;
			pda =(double*) p.GetClassObj("double","d");
			if(*pda !=10) iStat++;
		}
		catch(...)
		{
			iStat += 1;
			*m_stream <<" TestComment() catch error "<<"\r\n";
		}

		if (iStat==0)
			*m_stream << "passed" << "\r\n";
		else
			*m_stream << "\n  failed with " << iStat << " errors" << "\r\n";

		return iStat;
	}

	int ParserTester::TestReturnCondition()
	{
		ParserTester::c_iCount++;
		int iStat = 0;
		*m_stream << "testing return condition...";

		Parser p;
		double x = 0;
		double returnValue = 0;
		try
		{
			p.DefineVar("x", &x);
			p.DefineVar("returnValue", &returnValue);

			x = 0;
			p.SetExpr("x=0; return; x=1;");
			p.Eval();
			if (x != 0) iStat++;

			x = 0;
			p.SetExpr("x=0; if (1) { return; } x=1;");
			p.Eval();
			if (x != 0) iStat++;

			x = 0;
			p.SetExpr("x=0; if (0) { return; } x=1;");
			p.Eval();
			if (x != 1) iStat++;

			x = 0;
			p.SetExpr("x=0; while (x < 3) { x=x+1; return; } x=9;");
			p.Eval();
			if (x != 1) iStat++;

			x = 0;
			p.SetExpr("return ;");
			p.Eval();
			if (x != 0) iStat++;

			x = 0;
			p.SetExpr("if (0) { return; } x=1;");
			p.Eval();
			if (x != 1) iStat++;

			x = 0;
			p.SetExpr("returnValue = 2; x=3;");
			p.Eval();
			if (x != 3) iStat++;

			x = 0;
			p.SetExpr("if (1) { if (1) { return; } } x=1;");
			p.Eval();
			if (x != 0) iStat++;

			bool threw = false;
			try
			{
				p.SetExpr("return 1;");
				p.Eval();
			}
			catch (Parser::exception_type&)
			{
				threw = true;
			}
			if (!threw) iStat++;

			p.SetExpr("x=2;");
			p.Eval();
			if (x != 2) iStat++;
		}
		catch (Parser::exception_type &e)
		{
			iStat += 1;
			*m_stream << " Parser exception: " << e.GetMsg() << "\r\n";
			*m_stream << " Token: " << e.GetToken() << "\r\n";
		}
		catch (std::exception &e)
		{
			iStat += 1;
			*m_stream << " std::exception: " << e.what() << "\r\n";
		}
		catch (...)
		{
			iStat += 1;
			*m_stream << " TestReturnCondition() catch error " << "\r\n";
		}

		if (iStat==0)
			*m_stream << "passed" << "\r\n";
		else
			*m_stream << "\n  failed with " << iStat << " errors" << "\r\n";

		return iStat;
	}

    void ParserTester::AddTest(testfun_type a_pFun)
    {
      m_vTestFun.push_back(a_pFun);
    }

	void ParserTester::SetStream(std::ostream *a_stream)
	{
		assert(a_stream);
		m_stream = a_stream;
	}

    void ParserTester::Run()
    {
		int iStat = 0;
		try
		{
			for (int i=0; i<(int)m_vTestFun.size(); ++i)
				iStat += (this->*m_vTestFun[i])();
		}
		catch(Parser::exception_type &e)
		{
			*m_stream << "\r\n" << e.GetMsg() << "\r\n";
			*m_stream << e.GetToken() << "\r\n";
			Abort();
		}
		catch(std::exception &e)
		{
			*m_stream << e.what() << "\r\n";
			Abort();
		}
		catch(...)
		{
			*m_stream << "Internal error";
			Abort();
		}

		if (iStat==0)
		{
			*m_stream << "Test passed (" <<  ParserTester::c_iCount << " expressions)" << "\r\n";
		}
		else
		{
			*m_stream << "Test failed with " << iStat
				<< " errors (" <<  ParserTester::c_iCount
				<< " expressions)" << "\r\n";
		}
		ParserTester::c_iCount = 0;
    }

    int ParserTester::ThrowTest(const std::string &a_str, int a_iErrc, bool a_bFail)
    {
      ParserTester::c_iCount++;

      try
      {
        double fVal=0;
        Parser p;

        p.DefineVar("a", &fVal);
        p.DefineFun("valueof", ValueOf);
        p.SetExpr(a_str);
        p.Eval();
      }
      catch(Parser::exception_type &e)
      {

        if (a_bFail==true && a_iErrc!=e.GetCode() )
        {
          cout << "\n  "
               << "Expression: " << a_str
               << "  Code:" << e.GetCode()
               << "  Expected:" << a_iErrc;
        }

        return (a_iErrc==e.GetCode()) ? 0 : 1;
      }

      return (a_bFail==false) ? 0 : 1;
    }

    int ParserTester::EqnTest(const std::string &a_str, double a_fRes, bool a_fPass)
    {
		ParserTester::c_iCount++;
		int iRet(0);
		Parser *p1, p2, p3;
		p1=NULL;
		try
		{

			p1 = new mu::Parser();

			p1->DefineConst("pi", (value_type)PARSER_CONST_PI);
			p1->DefineConst("e", (value_type)PARSER_CONST_E);
			p1->DefineConst("const", 1);
			p1->DefineConst("const1", 2);
			p1->DefineConst("const2", 3);

			value_type vVarVal[] = { 1, 2, 3, -2};
			p1->DefineVar("a", &vVarVal[0]);
			p1->DefineVar("aa", &vVarVal[1]);
			p1->DefineVar("b", &vVarVal[1]);
			p1->DefineVar("c", &vVarVal[2]);
			p1->DefineVar("d", &vVarVal[3]);

			p1->DefineFun("f1of1", f1of1);
			p1->DefineFun("f1of2", f1of2);
			p1->DefineFun("f2of2", f2of2);
			p1->DefineFun("f1of3", f1of3);
			p1->DefineFun("f2of3", f2of3);
			p1->DefineFun("f3of3", f3of3);
			p1->DefineFun("f1of4", f1of4);
			p1->DefineFun("f2of4", f2of4);
			p1->DefineFun("f3of4", f3of4);
			p1->DefineFun("f4of4", f4of4);
			p1->DefineFun("f1of5", f1of5);
			p1->DefineFun("f2of5", f2of5);
			p1->DefineFun("f3of5", f3of5);
			p1->DefineFun("f4of5", f4of5);
			p1->DefineFun("f5of5", f5of5);

			p1->DefineFun("min", Min);
			p1->DefineFun("max", Max);
			p1->DefineFun("sum", Sum);
			p1->DefineFun("valueof", ValueOf);
			p1->DefineFun("atof", StrToFloat);

			p1->DefineInfixOprt("$", sign, prPOW+1);
			p1->DefineInfixOprt("~", plus2);
			p1->DefinePostfixOprt("m", Milli);
			p1->DefinePostfixOprt("#", times3);
			p1->DefinePostfixOprt("?", sqr);
			p1->SetExpr(a_str);

			value_type fVal[4] = {-999, -998, -997, -996};
			fVal[0] = p1->Eval();
			fVal[1] = p1->Eval();
			if (fVal[0]!=fVal[1])
			  throw Parser::exception_type("Bytecode corrupt.");

			try
			{

				std::vector<mu::Parser> vParser;
				vParser.push_back(*p1);
				mu::Parser p2 = vParser[0];

				vParser.clear();
				delete p1;
				p1 = 0;
				fVal[2] = p2.Eval();

				mu::Parser p3;
				p3 = p2;
				p3.EnableOptimizer(false);
				fVal[3] = p3.Eval();
			}
			catch(exception &e)
			{
			  *m_stream << "\n  " << e.what() << "\r\n";
			}

			bool bCloseEnough(true);
			for (int i=0; i<4; ++i)
			{
				bCloseEnough &= (fabs(a_fRes-fVal[i]) <= fabs(fVal[i]*0.0001));
			}

			iRet = ((bCloseEnough && a_fPass) || (!bCloseEnough && !a_fPass)) ? 0 : 1;
			if (iRet==1)
			{
				*m_stream << "\n  fail: " << a_str.c_str()
						<< " (incorrect result; expected: " << a_fRes
						<< " ;calculated: " << fVal[0]<< ").";
			}
		}
		catch(Parser::exception_type &e)
		{
			if (a_fPass)
			{
				*m_stream << "\n  fail: " << a_str.c_str() << " (" << e.GetMsg() << ")";
				return 1;
			}

		}
		catch(std::exception &e)
		{
			*m_stream << "\n  fail: " << a_str.c_str() << " (" << e.what() << ")";
			 return 1;
		}
		catch(...)
		{
			*m_stream << "\n  fail: " << a_str.c_str() <<  " (unexpected exception)";
			return 1;
		}

		if(NULL!=p1)
			delete p1;

		return iRet;
    }

    int ParserTester::EqnTestInt(const std::string &a_str, double a_fRes, bool a_fPass)
    {
      ParserTester::c_iCount++;

      value_type vVarVal[] = {1, 2, 3};
      value_type fVal[2] = {-99, -999};
      int iRet(0);

      try
      {
        ParserInt p;
        p.DefineConst("const1", 1);
        p.DefineConst("const2", 2);
        p.DefineVar("a", &vVarVal[0]);
        p.DefineVar("b", &vVarVal[1]);
        p.DefineVar("c", &vVarVal[2]);

        p.SetExpr(a_str);
        fVal[0] = p.Eval();
        fVal[1] = p.Eval();

        if (fVal[0]!=fVal[1])
          throw Parser::exception_type("Bytecode corrupt.");

        iRet =  ( (a_fRes==fVal[0] &&  a_fPass) ||
                  (a_fRes!=fVal[0] && !a_fPass) ) ? 0 : 1;
        if (iRet==1)
        {
          *m_stream << "\n  fail: " << a_str.c_str()
                    << " (incorrect result; expected: " << a_fRes
                    << " ;calculated: " << fVal[0]<< ").";
        }
      }
      catch(Parser::exception_type &e)
      {
        if (a_fPass)
          *m_stream << "\n  " << e.GetExpr() << " : " << e.GetMsg();
      }
      catch(...)
      {
		*m_stream << "\n  fail: " << a_str.c_str() <<  " (unexpected exception)";
		return 1;
      }

      return iRet;
    }

	int ParserTester::EqnTestClass(const std::string &a_str,
									const std::string &a_strclass,
									const std::string &a_strobj,
									double a_fRes, bool a_fPass)
	{
		ParserTester::c_iCount++;
		int iRet(0);
		Parser *p1, p2, p3;
		p1=NULL;
		try
		{

			p1 = new mu::Parser();

			p1->DefineConst("pi", (value_type)PARSER_CONST_PI);
			p1->DefineConst("e", (value_type)PARSER_CONST_E);
			p1->DefineConst("const", 1);
			p1->DefineConst("const1", 2);
			p1->DefineConst("const2", 3);

			value_type vVarVal[] = { 1, 2, 3, -2};
			p1->DefineVar("a", &vVarVal[0]);
			p1->DefineVar("aa", &vVarVal[1]);
			p1->DefineVar("b", &vVarVal[1]);
			p1->DefineVar("c", &vVarVal[2]);
			p1->DefineVar("d", &vVarVal[3]);

			p1->DefineFun("f1of1", f1of1);
			p1->DefineFun("f1of2", f1of2);
			p1->DefineFun("f2of2", f2of2);
			p1->DefineFun("f1of3", f1of3);
			p1->DefineFun("f2of3", f2of3);
			p1->DefineFun("f3of3", f3of3);
			p1->DefineFun("f1of4", f1of4);
			p1->DefineFun("f2of4", f2of4);
			p1->DefineFun("f3of4", f3of4);
			p1->DefineFun("f4of4", f4of4);
			p1->DefineFun("f1of5", f1of5);
			p1->DefineFun("f2of5", f2of5);
			p1->DefineFun("f3of5", f3of5);
			p1->DefineFun("f4of5", f4of5);
			p1->DefineFun("f5of5", f5of5);

			p1->DefineFun("min", Min);
			p1->DefineFun("max", Max);
			p1->DefineFun("sum", Sum);
			p1->DefineFun("valueof", ValueOf);
			p1->DefineFun("atof", StrToFloat);

			p1->DefineInfixOprt("$", sign, prPOW+1);
			p1->DefineInfixOprt("~", plus2);
			p1->DefinePostfixOprt("m", Milli);
			p1->DefinePostfixOprt("#", times3);
			p1->DefinePostfixOprt("?", sqr);
			double *apdouble=0;
			p1->DefineOrgClass("double",apdouble);
			p1->UsingClass(true);
			usingclass* atestclass=0;
			p1->DefineClass("usingclass",atestclass);

			p1->DefineClassFun("usingclass",atestclass,"AFUCN",&usingclass::AFUCN);
			p1->DefineClassFun("usingclass",atestclass,"BFUCN",&usingclass::BFUCN);
			p1->DefineClassFun("usingclass",atestclass,"FUCN1",&usingclass::FUCN1);
			p1->DefineClassFun("usingclass",atestclass,"FUCN2",&usingclass::FUCN2);
			p1->DefineClassFun("usingclass",atestclass,"FUCN2_2",&usingclass::FUCN2_2);
			p1->DefineClassFun("usingclass",atestclass,"FUCNX_2",&usingclass::FUCNX_2);

			p1->DefineClassFun("usingclass",atestclass,"FUCN3",&usingclass::FUCN3);
			p1->DefineClassFun("usingclass",atestclass,"FUCN4",&usingclass::FUCN4);
			p1->DefineClassFun("usingclass",atestclass,"FUCN_I1",&usingclass::FUCN_I1);
			p1->DefineClassFun("usingclass",atestclass,"FUCN_I2",&usingclass::FUCN_I2);
			p1->DefineClassFun("usingclass",atestclass,"FUCN_I3",&usingclass::FUCN_I3);
			p1->DefineClassFun("usingclass",atestclass,"FUCN_I4",&usingclass::FUCN_I4);
			p1->DefineClassFun("usingclass",atestclass,"FUCN_I5",&usingclass::FUCN_I5);
			p1->DefineClassFun("usingclass",atestclass,"FUCN_I6",&usingclass::FUCN_I6);
			p1->DefineClassFun("usingclass",atestclass,"FUCN_I7",&usingclass::FUCN_I7);
			p1->DefineClassFun("usingclass",atestclass,"FUCN_VOIDP",&usingclass::FUCN_VOIDP);
			p1->DefineClassFun("usingclass",atestclass,"FUCN_CHARP",&usingclass::FUCN_CHARP);
			p1->DefineClassFun("usingclass",atestclass,"FUCN_I1_RE_DB",&usingclass::FUCN_I1_RE_DB);
			p1->DefineClassFun("usingclass",atestclass,"FUCN_I2_RE_DB",&usingclass::FUCN_I2_RE_DB);

			p1->DefineClassFun("usingclass",atestclass,"FUN_I2_RE_INT",&usingclass::FUN_I2_RE_INT);
			p1->DefineClassFun("usingclass",atestclass,"FUN_I1_RE_INT",&usingclass::FUN_I1_RE_INT);
			p1->DefineClassFun("usingclass",atestclass,"FUN_0_RE_INT",&usingclass::FUN_0_RE_INT);
			p1->DefineClassFun("usingclass",atestclass,"FUN_0_RE_DOUBLE",&usingclass::FUN_0_RE_DOUBLE);
			p1->DefineClassFun("usingclass",atestclass,"FUN_I1_RE_DOUBLE",&usingclass::FUN_I1_RE_DOUBLE);
			p1->DefineClassFun("usingclass",atestclass,"FUN_VP_RE_DOUBLE",&usingclass::FUN_VP_RE_DOUBLE);

			p1->DefineClassFun("usingclass",atestclass,"ISCALL",&usingclass::ISCALL);

			p1->SetExpr(a_str);

			value_type fVal[4] = {-999, -998, -997, -996};

			try
			{
				p1->Eval();
				void * pvalue = p1->GetClassObj(a_strclass,a_strobj);
				fVal[0] =*((value_type*) pvalue);
			}
			catch(exception &e)
			{
				*m_stream << "\n  " << e.what() << "\r\n";
			}

			bool bCloseEnough(true);
			for (int i=0; i<4; ++i)
			{
				bCloseEnough &= (fabs(a_fRes-fVal[i]) <= fabs(fVal[i]*0.0001));
			}

			iRet = ((bCloseEnough && a_fPass) || (!bCloseEnough && !a_fPass)) ? 0 : 1;
			if (iRet==1)
			{
				*m_stream << "\n  fail: " << a_str.c_str()
					<< " (incorrect result; expected: " << a_fRes
					<< " ;calculated: " << fVal[0]<< ").";
			}
		}
		catch(Parser::exception_type &e)
		{
			if (a_fPass)
			{
				*m_stream << "\n  fail: " << a_str.c_str() << " (" << e.GetMsg() << ")";
				return 1;
			}

		}
		catch(std::exception &e)
		{
			*m_stream << "\n  fail: " << a_str.c_str() << " (" << e.what() << ")";
			return 1;
		}
		catch(...)
		{
			*m_stream << "\n  fail: " << a_str.c_str() <<  " (unexpected exception)";
			return 1;
		}

		if(NULL!=p1)
			delete p1;

		return iRet;
	}

    void ParserTester::Abort() const
    {
      *m_stream << "Test failed (internal error in test class)" << "\r\n";
      while (!getchar());
      exit(-1);
    }

	void ParserTester::ListFunction(const mu::ParserBase *pParser)
	{
		std::ostringstream os;

		using mu::funmap_type;
		funmap_type funmap = pParser->GetFunDef();
		funmap_type::const_iterator item;
		*m_stream << "\nFunctions available:\r\n";

		for (item=funmap.begin(); item!=funmap.end(); ++item)
		{
			*m_stream << "  " << item->first;
			*m_stream << "(";

			int iArgc = item->second.GetArgc();
			if (iArgc>=0)
			{

				for (int i=0; i<iArgc; ++i)
				{
					char cVar[] = "val ";
					cVar[3] = '1' + (char)i;
					*m_stream << cVar;
					if (i!=iArgc-1)
						*m_stream << ",";
				}
			}
			else
			{
				*m_stream << "...";
			}

			*m_stream << ")\r\n";
		}
	}

	void ParserTester::ListVar(mu::ParserBase  &Pparser)
	{

		mu::varmap_type variables = Pparser.GetVar();
		if (!variables.size())
			return;
		*m_stream << "Base variables:\r\n";
		*m_stream <<   "-----------------\r\n";
		*m_stream << "Number: " << (int)variables.size() << "\r\n";
		varmap_type::const_iterator item = variables.begin();
		for (; item!=variables.end(); ++item)
		{
			*m_stream << "Name: " << item->first << "   Address: [0x" << item->second << "]  ";
			*m_stream << "Result: "<<*(item->second)<<"\r\n";
		}
		*m_stream <<"\r\n";
	}

	void ParserTester::ListClass(mu::ParserBase &Pparser)
	{
		mu::classbasemap_type classmap = Pparser.GetClassMap();
		if(!classmap.size())
			return;
		*m_stream << "\nParser Class:\r\n";
		*m_stream <<   "-----------------\r\n";
		*m_stream << "Number: " << (int)classmap.size() << "\r\n";
		classbasemap_type::const_iterator item = classmap.begin();
		for (; item!=classmap.end(); ++item)
		{
			*m_stream << "Class Name: " << item->first << "   Address: [0x" << item->second << "]  "<< "\r\n";
			classbase *pclass=(item->second);

			*m_stream << "      Object Number: " <<  (int)pclass->size() << "\r\n";
			*m_stream << "      member Function Number: " <<  (int)pclass->funcsize() << "\r\n";

			for(int i=0;i<pclass->size();i++)
			{
				*m_stream << "            Object Name: " << pclass->getvar(i) <<"   Address: [0x"
					<< pclass->getvarpoint(i)<< "]  "<< "\r\n";
			}
			for(int i=0;i<pclass->funcsize();i++)
			{
				*m_stream << "             member Function: " << pclass->getfuncname(i) <<"   Type: ["
					<< pclass->getfunctype(i)<< "]  "<< "\r\n";

			}
		}
		*m_stream <<"\r\n";
	}

	void ParserTester::ListFormula(mu::ParserBase &Pparser)
	{
		mu::string_type astring=Pparser.GetFormula();
		*m_stream << " Formula: " <<  astring << "\r\n";
	}
  }
}
