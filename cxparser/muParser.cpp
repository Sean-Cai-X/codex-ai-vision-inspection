

#include "muParser.h"

#include <cmath>
#include <algorithm>
#include <numeric>

#define PARSER_CONST_PI  3.141592653589793238462643

#define PARSER_CONST_E   2.718281828459045235360287

using namespace std;

namespace mu
{

value_type Parser::Sin(value_type v)   { return sin(v);  }
value_type Parser::Cos(value_type v)   { return cos(v);  }
value_type Parser::Tan(value_type v)   { return tan(v);  }
value_type Parser::ASin(value_type v)  { return asin(v); }
value_type Parser::ACos(value_type v)  { return acos(v); }
value_type Parser::ATan(value_type v)  { return atan(v); }
value_type Parser::Sinh(value_type v)  { return sinh(v); }
value_type Parser::Cosh(value_type v)  { return cosh(v); }
value_type Parser::Tanh(value_type v)  { return tanh(v); }
value_type Parser::ASinh(value_type v) { return log(v + sqrt(v * v + 1)); }
value_type Parser::ACosh(value_type v) { return log(v + sqrt(v * v - 1)); }
value_type Parser::ATanh(value_type v) { return ((value_type)0.5 * log((1 + v) / (1 - v))); }

value_type Parser::Log2(value_type v)  { return log(v)/log((value_type)2); }
value_type Parser::Log10(value_type v) { return log10(v); }
value_type Parser::Ln(value_type v)    { return log(v);   }

value_type Parser::Exp(value_type v)  { return exp(v);   }
value_type Parser::Abs(value_type v)  { return fabs(v);  }
value_type Parser::Sqrt(value_type v) { return sqrt(v);  }
value_type Parser::Rint(value_type v) { return floor(v + (value_type)0.5); }
value_type Parser::Sign(value_type v) { return (value_type)((v<0) ? -1 : (v>0) ? 1 : 0); }

inline value_type Parser::Ite(value_type v1, value_type v2, value_type v3) { return (v1!=0) ? v2 : v3; }

value_type Parser::UnaryMinus(value_type v)
{
	return -v;
}
value_type Parser::Mytest(value_type v)
{

    return 0;
}

value_type* Parser::GetAddress(value_type v)
{
	(void)v;
	return 0;
}

value_type Parser::Sum(const value_type *a_afArg, int a_iArgc)
{
  if (!a_iArgc)
    throw exception_type("too few arguments for function sum.");

  value_type fRes=0;
  for (int i=0; i<a_iArgc; ++i) fRes += a_afArg[i];
  return fRes;
}

value_type Parser::Avg(const value_type *a_afArg, int a_iArgc)
{
  if (!a_iArgc)
    throw exception_type("too few arguments for function sum.");

  value_type fRes=0;
  for (int i=0; i<a_iArgc; ++i) fRes += a_afArg[i];
  return fRes/(double)a_iArgc;
}

value_type Parser::Min(const value_type *a_afArg, int a_iArgc)
{
	if (!a_iArgc)
		throw exception_type("too few arguments for function min.");

	value_type fRes=a_afArg[0];
	for (int i=0; i<a_iArgc; ++i) fRes = min(fRes, a_afArg[i]);

	return fRes;
}

value_type Parser::Max(const value_type *a_afArg, int a_iArgc)
{
	if (!a_iArgc)
		throw exception_type("too few arguments for function min.");

	value_type fRes=a_afArg[0];
	for (int i=0; i<a_iArgc; ++i) fRes = max(fRes, a_afArg[i]);

	return fRes;
}

value_type Parser::AvgFilter(const value_type *a_afArg, int a_iArgc)
{
	if (!a_iArgc)
		throw exception_type("too few arguments for function min.");

	value_type fResMax=a_afArg[0];
	for (int i=0; i<a_iArgc; ++i) fResMax = max(fResMax, a_afArg[i]);
	value_type fResMin=a_afArg[0];
	for (int i=0; i<a_iArgc; ++i) fResMin = min(fResMin, a_afArg[i]);

	value_type fRes=0;
	if(a_iArgc>2)
	{
		for (int i=0; i<a_iArgc; ++i) fRes += a_afArg[i];
		fRes = fRes - fResMax - fResMin ;
		a_iArgc = a_iArgc - 2;
	}
	else
	{
		value_type fRes=0;
		for (int i=0; i<a_iArgc; ++i) fRes += a_afArg[i];
	}

	return fRes/(double)a_iArgc;

}

value_type Parser::StrToFloat(const char *a_szMsg)
{
	using namespace std;
	return atof(a_szMsg);
}

vision_type Parser::TestForMultFunc(const vision_type*a_afArg,vision_type a_iArgc)
{
    if (!a_iArgc)
        throw exception_type("too few arguments for function min.");

    vision_type fRes=a_afArg[0];
    for (int i=0; i<a_iArgc; ++i)
		fRes = max(fRes, a_afArg[i]);

    return fRes;
}
vision_type Parser::TestForLPExchange(const vision_type*a_afArg,vision_lptype a_piArgc)
{
	if(!a_piArgc)
		throw exception_type("too few arguments for function min.");
	return *a_piArgc;
}

bool Parser::IsVal(const char_type *a_szExpr, int &a_iPos, value_type &a_fVal)
{
  stringstream_type stream(a_szExpr);
  value_type fVal(0);

  stream.seekg(0);
  stream >> fVal;
  int iEnd = stream.tellg();

  if (iEnd==-1)
    return false;

  a_iPos += iEnd;
  a_fVal = fVal;
  return true;
}

Parser::Parser()
  :ParserBase()
  ,m_fEpsilon((value_type)1e-7)
{

  AddValIdent(IsVal);

  InitCharSets();
  InitFun();
  InitConst();
  InitOprt();

}

int Parser::DogRun()
{

	return 1;
}

void Parser::InitCharSets()
{
  DefineNameChars("0123456789_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ@");
  DefineOprtChars("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ@"
	  "+-*^/?<>=#!$%&|~'_");
  DefineInfixOprtChars("+-*^/?<>=#!$%&|~'_");
}

void Parser::InitFun()
{

  DefineFun("sin", Sin);
  DefineFun("cos", Cos);
  DefineFun("tan", Tan);

  DefineFun("asin", ASin);
  DefineFun("acos", ACos);
  DefineFun("atan", ATan);

  DefineFun("sinh", Sinh);
  DefineFun("cosh", Cosh);
  DefineFun("tanh", Tanh);

  DefineFun("asinh", ASinh);
  DefineFun("acosh", ACosh);
  DefineFun("atanh", ATanh);

  DefineFun("log2", Log2);
  DefineFun("log10", Log10);
  DefineFun("log", Log10);
  DefineFun("ln", Ln);

  DefineFun("exp", Exp);
  DefineFun("sqrt", Sqrt);
  DefineFun("sign", Sign);
  DefineFun("rint", Rint);
  DefineFun("abs", Abs);
  DefineFun("IF", Ite);

  DefineFun("sum", Sum);
  DefineFun("avg", Avg);
  DefineFun("min", Min);
  DefineFun("max", Max);

  DefineFun("avgf", AvgFilter);

  DefineFun("atof", StrToFloat);

}

void Parser::InitConst()
{
  DefineConst("_pi", (value_type)PARSER_CONST_PI);
  DefineConst("_e", (value_type)PARSER_CONST_E);
}

void Parser::InitOprt()
{
  DefineInfixOprt("-", UnaryMinus);
  DefineGetAdress("&", GetAddress);
}

value_type Parser::Diff(value_type *a_Var, value_type a_fPos) const
{
  assert(m_fEpsilon);
  value_type fEpsilon( (a_fPos==0) ? (value_type)1e-10 : m_fEpsilon * a_fPos ),
             fRes(0), fBuf(*a_Var), f[4] = {0,0,0,0};

 *a_Var = a_fPos+2*fEpsilon;  f[0] = Eval();
 *a_Var = a_fPos+1*fEpsilon;  f[1] = Eval();
 *a_Var = a_fPos-1*fEpsilon;  f[2] = Eval();
 *a_Var = a_fPos-2*fEpsilon;  f[3] = Eval();
 *a_Var = fBuf;

  fRes = (-f[0] + 8*f[1] - 8*f[2] + f[3]) / (12*fEpsilon);
  return fRes;
}

}
