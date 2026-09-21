

#include "muParserBytecode.h"

#include <cassert>
#include <string>
#include <stack>
#include <vector>
#include <iostream>

#include "muParserDef.h"
#include "muParserError.h"
#include "muParserToken.h"

namespace mu
{

  
  ParserByteCode::ParserByteCode()
    :m_iStackPos(0)
    ,m_vBase()
    ,mc_iSizeVal( sizeof(value_type) / sizeof(map_type) )
    ,mc_iSizePtr(  std::max( (int)sizeof(value_type*) /
                            (int)sizeof(map_type), 1 ) )
    ,mc_iSizeValEntry( 2 + mc_iSizeVal)
	,m_pcreateclass(NULL)
  {
    assert( sizeof(value_type)>=sizeof(map_type) );
  }

  ParserByteCode::~ParserByteCode()
  {}

  ParserByteCode::ParserByteCode(const ParserByteCode &a_ByteCode)
    :mc_iSizeVal( sizeof(value_type)/sizeof(map_type) )
    ,mc_iSizePtr( sizeof(value_type*) / sizeof(map_type) )
    ,mc_iSizeValEntry( 2 + mc_iSizeVal)
	,m_pcreateclass(NULL)
  {
    Assign(a_ByteCode);
  }

  ParserByteCode& ParserByteCode::operator=(const ParserByteCode &a_ByteCode)
  {
    Assign(a_ByteCode);
    return *this;
  }

  
  void ParserByteCode::StorePtr(void *a_pAddr)
  {
    #if defined(_MSC_VER)
      #pragma warning( disable : 4311 )
    #endif

    for (int i=0; i<mc_iSizePtr; ++i)
    {
        m_vBase.push_back( *( reinterpret_cast<map_type*>(&a_pAddr) + i ) );
    }

    #if defined(_MSC_VER)
      #pragma warning( default : 4311 )
    #endif
  }

  void ParserByteCode::Assign(const ParserByteCode &a_ByteCode)
  {
    if (this==&a_ByteCode)
      return;

    m_iStackPos = a_ByteCode.m_iStackPos;
    m_vBase = a_ByteCode.m_vBase;
  }

  void ParserByteCode::AddVar(value_type *a_pVar)
  {
    m_vBase.push_back( ++m_iStackPos );
    m_vBase.push_back( cmVAR );

    StorePtr(a_pVar);

    int iSize = GetValSize()-GetPtrSize();
    assert(iSize>=0);

    for (int i=0; i<iSize; ++i)
      m_vBase.push_back(0);
  }

  void ParserByteCode::AddVal(value_type a_fVal)
  {
    m_vBase.push_back( ++m_iStackPos );
    m_vBase.push_back( cmVAL );

    for (int i=0; i<mc_iSizeVal; ++i)
      m_vBase.push_back( *(reinterpret_cast<map_type*>(&a_fVal) + i) );
  }

  void ParserByteCode::AddOp(ECmdCode a_Oprt)
  {
    m_vBase.push_back(--m_iStackPos);
    m_vBase.push_back(a_Oprt);
  }

  void ParserByteCode::AddAssignOp(value_type *a_pVar)
  {
    m_vBase.push_back(--m_iStackPos);
    m_vBase.push_back(cmASSIGN);
    StorePtr(a_pVar);
  }

  void ParserByteCode::AddFun(void *a_pFun, int a_iArgc)
  {
    if (a_iArgc>=0)
    {
      m_iStackPos = m_iStackPos - a_iArgc + 1;
    }
    else
    {
      m_iStackPos = m_iStackPos + a_iArgc + 1;
    }

    m_vBase.push_back(m_iStackPos);
    m_vBase.push_back(cmFUNC);
	m_vBase.push_back(a_iArgc);

    StorePtr(a_pFun);
  }

  
  void ParserByteCode::AddClassMemberFunNum(void *a_pclass,void *a_pobj,void *a_pFun, int a_iArgc)
  {
	  if (a_iArgc>=0)
	  {
		  m_iStackPos = m_iStackPos - a_iArgc + 1;
	  }
	  else
	  {
		  m_iStackPos = m_iStackPos + a_iArgc + 1;
	  }

	  m_vBase.push_back(m_iStackPos);
	  m_vBase.push_back(cmClassFucNum);
	  m_vBase.push_back(a_iArgc);

	  StorePtr(a_pclass);
	  StorePtr(a_pobj);
	  StorePtr(a_pFun);

	if(NULL !=m_pcreateclass)
	  m_vBase.push_back(m_pcreateclass->findstacknum(a_pobj));
	else
	  m_vBase.push_back(-1);
 }

  
  void ParserByteCode::AddClassMemberFunVoidp(void *a_pclass,void *a_pobj,void *a_pFun, int istocknum2,void *the_pobj)
  {

	  m_vBase.push_back(++m_iStackPos);
	  m_vBase.push_back(cmClassFucVp);
	  m_vBase.push_back(istocknum2);

	  StorePtr(a_pclass);
	  StorePtr(a_pobj);
	  StorePtr(a_pFun);
	  StorePtr(the_pobj);

	  if(NULL !=m_pcreateclass)
		  m_vBase.push_back(m_pcreateclass->findstacknum(a_pobj));
	  else
		  m_vBase.push_back(-1);
  }

  
  void ParserByteCode::AddClassMemberFunStr(void *a_pclass,void *a_pobj,void *a_pFun, int a_iArgc, int a_iIdx)
  {

	  m_vBase.push_back(++m_iStackPos);
	  m_vBase.push_back(cmClassFucStr);
	  m_vBase.push_back(a_iArgc);

	  StorePtr(a_pclass);
	  StorePtr(a_pobj);
	  StorePtr(a_pFun);

	  m_vBase.push_back(a_iIdx);

	  if(NULL !=m_pcreateclass)
		  m_vBase.push_back(m_pcreateclass->findstacknum(a_pobj));
	  else
		  m_vBase.push_back(-1);
  }

  void ParserByteCode::SetCreateClassLP(CreateClass *pcreateclass)
  {
		m_pcreateclass = pcreateclass;
  }

  void ParserByteCode::AddStrFun(void *a_pFun, int a_iArgc, int a_iIdx)
  {

    m_vBase.push_back(++m_iStackPos);
    m_vBase.push_back(cmFUNC_STR);
    m_vBase.push_back(a_iArgc);

    StorePtr(a_pFun);

    m_vBase.push_back(a_iIdx);
  }

  
  void ParserByteCode::Finalize()
  {

    m_vBase.push_back(cmEND);
    m_vBase.push_back(cmEND);
    m_vBase.push_back(cmEND);

    storage_type(m_vBase).swap(m_vBase);
  }

  const ParserByteCode::map_type* ParserByteCode::GetRawData() const
  {
    assert(m_vBase.size());
    return &m_vBase[0];
  }
  void ParserByteCode::SetStorageBase(storage_type Base)
  {
		m_vBase=Base;
  }

  void ParserByteCode::CollectRawData(storage_type* pBase)
  {
	  if(m_vBase.size()>=3)
	  {

		  m_vBase.pop_back();
		  m_vBase.pop_back();
          m_vBase.pop_back();
	  }
	  m_vBase.insert(m_vBase.end() ,pBase->begin(),pBase->end());
  }

  ParserByteCode::storage_type ParserByteCode::CollectStorageData(storage_type* pBase1,storage_type *pBase2)
  {
	  storage_type collectdata;
	  if(pBase1->size()>=3)
	  {
		  pBase1->pop_back();
		  pBase1->pop_back();
		  pBase1->pop_back();
	  }
	  collectdata=*pBase1;
	  collectdata.insert(collectdata.end(),pBase2->begin(),pBase2->end());
	  return collectdata;
  }

  ParserByteCode::storage_type ParserByteCode::GetStorageBaseData()
  {
	  return m_vBase;
  }

  ParserByteCode::storage_type * ParserByteCode::GetStorageBase()
  {
	  return &m_vBase;
  }

  void ParserByteCode::clear()
  {
    m_vBase.clear();
    m_iStackPos = 0;
  }

  void ParserByteCode::RemoveValEntries(unsigned a_iNumber)
  {
    unsigned iSize = a_iNumber * mc_iSizeValEntry;

    m_vBase.resize(m_vBase.size()-iSize);

    m_iStackPos -= (a_iNumber);
  }

}
