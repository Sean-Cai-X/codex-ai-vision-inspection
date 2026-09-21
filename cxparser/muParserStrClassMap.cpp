

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

#include "muParserStrClassMap.h"

namespace mu
{

	
	muParserStrClassMap::muParserStrClassMap():
	m_pclassgroup(NULL),
	m_iclassnum(0)
	{
	}

	
	muParserStrClassMap::~muParserStrClassMap()
	{
		if(NULL!=m_pclassgroup)
			delete m_pclassgroup;
		m_iclassnum=0;
	}

}
