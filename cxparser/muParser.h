

#ifndef MU_PARSER_H
#define MU_PARSER_H

#include "muParserBase.h"
#include <vector>

namespace mu
{

class  Parser : public ParserBase
{
private:

	static value_type  Sin(value_type);
	static value_type  Cos(value_type);
	static value_type  Tan(value_type);

	static value_type  ASin(value_type);
	static value_type  ACos(value_type);
	static value_type  ATan(value_type);

	static value_type  Sinh(value_type);
	static value_type  Cosh(value_type);
	static value_type  Tanh(value_type);

	static value_type  ASinh(value_type);
	static value_type  ACosh(value_type);
	static value_type  ATanh(value_type);

	static value_type  Log2(value_type);
	static value_type  Log10(value_type);
	static value_type  Ln(value_type);

	static value_type  Exp(value_type);
	static value_type  Abs(value_type);
	static value_type  Sqrt(value_type);
	static value_type  Rint(value_type);
	static value_type  Sign(value_type);
	static value_type  Ite(value_type, value_type, value_type);

	static value_type  UnaryMinus(value_type);
	static value_type  Mytest(value_type v) ;
	static value_type *GetAddress(value_type);

	static value_type Sum(const value_type*, int);
	static value_type Avg(const value_type*, int);
	static value_type Min(const value_type*, int);
	static value_type Max(const value_type*, int);
	static value_type AvgFilter(const value_type*, int);
	static value_type StrToFloat(const char *a_szMsg);

	static vision_type TestForMultFunc(const vision_type*,vision_type);
	static vision_type TestForLPExchange(const vision_type*,vision_lptype);

	static bool IsVal(const char_type *a_szExpr, int &a_iPos, value_type &a_fVal);
	value_type m_fEpsilon;

public:

	
	Parser();
	~Parser(){};
	virtual void InitCharSets();
	virtual void InitFun();
	virtual void InitConst();
	virtual void InitOprt();

	
	value_type Diff(value_type *a_Var, value_type a_fPos) const;

	int DogRun();
};
}

#endif
