

#ifndef MU_PARSER_INT_H
#define MU_PARSER_INT_H

#include "muParserBase.h"
#include <vector>

namespace mu
{

class ParserInt : public ParserBase
{
private:

    static int  Round(value_type v) { return (int)(v + ((v>=0) ? 0.5 : -0.5) ); };

    static value_type  Abs(value_type);
	  static value_type  Sign(value_type);
    static value_type  Ite(value_type, value_type, value_type);

    static value_type  UnaryMinus(value_type);

    static value_type  Sum(const value_type* a_afArg, int a_iArgc);
    static value_type  Min(const value_type* a_afArg, int a_iArgc);
    static value_type  Max(const value_type* a_afArg, int a_iArgc);
	static value_type AvgFilter(const value_type* a_afArg, int);

    static value_type  Add(value_type v1, value_type v2);
    static value_type  Sub(value_type v1, value_type v2);
    static value_type  Mul(value_type v1, value_type v2);
    static value_type  Div(value_type v1, value_type v2);
    static value_type  Mod(value_type v1, value_type v2);
    static value_type  Shr(value_type v1, value_type v2);
    static value_type  Shl(value_type v1, value_type v2);
    static value_type  LogAnd(value_type v1, value_type v2);
    static value_type  LogOr(value_type v1, value_type v2);
    static value_type  LogXor(value_type v1, value_type v2);
    static value_type  And(value_type v1, value_type v2);
    static value_type  Or(value_type v1, value_type v2);
    static value_type  Xor(value_type v1, value_type v2);
    static value_type  Less(value_type v1, value_type v2);
    static value_type  Greater(value_type v1, value_type v2);
    static value_type  LessEq(value_type v1, value_type v2);
    static value_type  GreaterEq(value_type v1, value_type v2);
    static value_type  Equal(value_type v1, value_type v2);
    static value_type  NotEqual(value_type v1, value_type v2);
    static value_type  Not(value_type v1);

    static bool IsHexVal(const char_type *a_szExpr, int &a_iPos, value_type &a_iVal);
    static bool IsBinVal(const char_type *a_szExpr, int &a_iPos, value_type &a_iVal);
    static bool IsVal(const char_type *a_szExpr, int &a_iPos, value_type &a_iVal);

public:

    
    ParserInt();

    virtual void InitFun();
	  virtual void InitOprt();
    virtual void InitConst();
    virtual void InitCharSets();
};

}

#endif
