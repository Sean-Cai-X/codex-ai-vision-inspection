

#ifndef MU_PARSER_BYTECODE_H
#define MU_PARSER_BYTECODE_H

#include <cassert>
#include <string>
#include <stack>
#include <vector>

#include "muParserDef.h"
#include "muParserError.h"
#include "muParserToken.h"

namespace mu
{

class  ParserByteCode
{
public:

    typedef bytecode_type map_type;

    typedef std::vector<map_type> storage_type;

private:

    typedef ParserToken<value_type, string_type> token_type;

    unsigned m_iStackPos;

    storage_type m_vBase;

    const int mc_iSizeVal;

    const int mc_iSizePtr;

	const int mc_iSizeValEntry;

	CreateClass *m_pcreateclass;

    void StorePtr(void *a_pAddr);

public:
    ParserByteCode();
   ~ParserByteCode();
    ParserByteCode(const ParserByteCode &a_ByteCode);
    ParserByteCode& operator=(const ParserByteCode &a_ByteCode);
    void Assign(const ParserByteCode &a_ByteCode);

    void AddVar(value_type *a_pVar);
    void AddVal(value_type a_fVal);
    void AddOp(ECmdCode a_Oprt);
    void AddAssignOp(value_type *a_pVar);
    void AddFun(void *a_pFun, int a_iArgc);
    void AddStrFun(void *a_pFun, int a_iArgc, int a_iIdx);
    
	void AddClassMemberFunNum(void *a_pclass,void *a_pobj,void *a_pFun, int a_iArgc);
    
	void AddClassMemberFunVoidp(void *a_pclass,void *a_pobj,void *a_pFun, int istocknum,void *the_pobj);
    
    void AddClassMemberFunStr(void *a_pclass,void *a_pobj,void *a_pFun, int a_iArgc, int a_iIdx);
	void SetCreateClassLP(CreateClass *pcreateclass);
	void Finalize();
    void clear();
    const map_type* GetRawData() const;

	unsigned GetRawDataSize()const
	{
		return static_cast<unsigned>(m_vBase.size());
	}
	void CollectRawData(storage_type* pBase);
	storage_type CollectStorageData(storage_type* pBase1,storage_type *pBase2);
	storage_type* GetStorageBase();
	void SetStorageBase(storage_type Base);
	storage_type GetStorageBaseData();

    unsigned GetValSize() const
    {
      return mc_iSizeVal;
    }

    unsigned GetPtrSize() const
    {
      return mc_iSizePtr;
    }

    void RemoveValEntries(unsigned a_iNumber);
};

}

#endif
