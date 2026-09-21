

#include "muParserInt.h"

#include <cmath>
#include <algorithm>
#include <numeric>

using namespace std;

namespace mu
{

value_type ParserInt::Abs(value_type v)  { return  Round(fabs(v)); }
value_type ParserInt::Sign(value_type v) { return (Round(v)<0) ? -1 : (Round(v)>0) ? 1 : 0; }
inline value_type ParserInt::Ite(value_type v1,
                          value_type v2,
                          value_type v3) { return (Round(v1)==1) ? Round(v2) : Round(v3); }
value_type ParserInt::Add(value_type v1, value_type v2) { return Round(v1)  + Round(v2); }
value_type ParserInt::Sub(value_type v1, value_type v2) { return Round(v1)  - Round(v2); }
value_type ParserInt::Mul(value_type v1, value_type v2) { return Round(v1)  * Round(v2); }
value_type ParserInt::Div(value_type v1, value_type v2) { return Round(v1)  / Round(v2); }
value_type ParserInt::Mod(value_type v1, value_type v2) { return Round(v1)  % Round(v2); }
value_type ParserInt::Shr(value_type v1, value_type v2) { return Round(v1) >> Round(v2); }
value_type ParserInt::Shl(value_type v1, value_type v2) { return Round(v1) << Round(v2); }
value_type ParserInt::LogAnd(value_type v1, value_type v2) { return Round(v1) & Round(v2); }
value_type ParserInt::LogOr(value_type v1, value_type v2)  { return Round(v1) | Round(v2); }
value_type ParserInt::LogXor(value_type v1, value_type v2) { return Round(v1) ^ Round(v2); }
value_type ParserInt::And(value_type v1, value_type v2) { return Round(v1) && Round(v2); }
value_type ParserInt::Or(value_type v1, value_type v2)  { return Round(v1) || Round(v2); }
value_type ParserInt::Less(value_type v1, value_type v2)      { return Round(v1)  < Round(v2); }
value_type ParserInt::Greater(value_type v1, value_type v2)   { return Round(v1)  > Round(v2); }
value_type ParserInt::LessEq(value_type v1, value_type v2)    { return Round(v1) <= Round(v2); }
value_type ParserInt::GreaterEq(value_type v1, value_type v2) { return Round(v1) >= Round(v2); }
value_type ParserInt::Equal(value_type v1, value_type v2)     { return Round(v1) == Round(v2); }
value_type ParserInt::NotEqual(value_type v1, value_type v2)  { return Round(v1) != Round(v2); }
value_type ParserInt::Not(value_type v) { return !Round(v); }

value_type ParserInt::UnaryMinus(value_type v)
{
  return -Round(v);
}

value_type ParserInt::Sum(const value_type* a_afArg, int a_iArgc)
{
  if (!a_iArgc)
    throw ParserError("too few arguments for function sum.");

  value_type fRes=0;
  for (int i=0; i<a_iArgc; ++i) fRes += a_afArg[i];
  return fRes;
}

value_type ParserInt::Min(const value_type* a_afArg, int a_iArgc)
{
    if (!a_iArgc)
        throw ParserError("too few arguments for function min.");

    value_type fRes=a_afArg[0];
    for (int i=0; i<a_iArgc; ++i) fRes = min(fRes, a_afArg[i]);

    return fRes;
}

value_type ParserInt::Max(const value_type* a_afArg, int a_iArgc)
{
    if (!a_iArgc)
        throw ParserError("too few arguments for function min.");

    value_type fRes=a_afArg[0];
    for (int i=0; i<a_iArgc; ++i) fRes = max(fRes, a_afArg[i]);

    return fRes;
}

value_type ParserInt::AvgFilter(const value_type *a_afArg, int a_iArgc)
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

bool ParserInt::IsVal(const char_type *a_szExpr, int &a_iPos, value_type &a_fVal)
{
  stringstream_type stream(a_szExpr);
  int iVal(0);

  stream >> iVal;
  int iEnd = stream.tellg();

  if (iEnd==-1)
    return false;

  a_iPos += iEnd;
  a_fVal = (value_type)iVal;
  return true;
}

bool ParserInt::IsHexVal(const char_type *a_szExpr, int &a_iPos, value_type &a_fVal)
{
  if (a_szExpr[0]!='$')
    return false;

  unsigned iVal(0);
  int iLen(0);
  if (sscanf(a_szExpr+1, "%x%n", &iVal, &iLen)==0)
    return false;

  a_iPos += iLen+1;
  a_fVal = iVal;
  return true;
}

bool ParserInt::IsBinVal(const char_type *a_szExpr, int &a_iPos, value_type &a_fVal)
{
  if (a_szExpr[0]!='#')
    return false;

  unsigned iVal = 0, iBits = sizeof(iVal)*8, i;
  for (i=0; (a_szExpr[i+1]=='0' || a_szExpr[i+1]=='1') && i<iBits; ++i)
    iVal |= (int)(a_szExpr[i+1]=='1') << ((iBits-1)-i);

  if (i==0)
    return false;

  if (i==iBits)
    throw exception_type("Binary to integer conversion error (overflow).");

  a_fVal = (unsigned)(iVal >> (iBits-i) );
  a_iPos += i+1;

  return true;
}

ParserInt::ParserInt()
:ParserBase()
{
  AddValIdent(IsVal);
  AddValIdent(IsHexVal);
  AddValIdent(IsBinVal);

  InitCharSets();
  InitFun();
  InitOprt();
}

void ParserInt::InitConst()
{
}

void ParserInt::InitCharSets()
{
  DefineNameChars("0123456789_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ@");
  DefineOprtChars("+-*^/?<>=!%&|~'_");
  DefineInfixOprtChars("/+-*^?<>=!%&|~'_");
}

void ParserInt::InitFun()
{
  DefineFun("sign", Sign);
  DefineFun("abs", Abs);
  DefineFun("IF", Ite);
  DefineFun("sum", Sum);
  DefineFun("min", Min);
  DefineFun("max", Max);
  DefineFun("avgf", AvgFilter);

}

void ParserInt::InitOprt()
{

  EnableBuiltInOprt(false);

  DefineInfixOprt("-", UnaryMinus);
  DefineInfixOprt("!", Not);

  DefineOprt("&", LogAnd, prLOGIC);
  DefineOprt("|", LogOr, prLOGIC);
  DefineOprt("^", LogXor, prLOGIC);
  DefineOprt("&&", And, prLOGIC);
  DefineOprt("||", Or, prLOGIC);

  DefineOprt("<", Less, prCMP);
  DefineOprt(">", Greater, prCMP);
  DefineOprt("<=", LessEq, prCMP);
  DefineOprt(">=", GreaterEq, prCMP);
  DefineOprt("==", Equal, prCMP);
  DefineOprt("!=", NotEqual, prCMP);

  DefineOprt("+", Add, prADD_SUB);
  DefineOprt("-", Sub, prADD_SUB);

  DefineOprt("*", Mul, prMUL_DIV);
  DefineOprt("/", Div, prMUL_DIV);
  DefineOprt("%", Mod, prMUL_DIV);

  DefineOprt(">>", Shr, prMUL_DIV+1);
  DefineOprt("<<", Shl, prMUL_DIV+1);
}

}
