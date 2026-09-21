

#include "muParserDef.h"
#include "muParserClass.h"

#include "muParserBase.h"
#include "muParser.h"
#include "muParserInt.h"

namespace mu
{

	
	void * CreateClass::addvar(const string_type & strobjname)
	{
		objmap_type::iterator item = m_objmap.find(strobjname);
		if (item!=m_objmap.end())
			return 0;
		objstruct * pobj =  new objstruct;
		pobj->m_ctor_ran = false;
		void *pclassvar = NULL;

		string_type astrname,astrtype;
		for(int i=0;i<m_classdefbuf.size();i++)
		{
			astrname = BuildMemberStorageName(strobjname, m_classStr[i]);
			pclassvar=m_classdefbuf[i]->addvar(astrname);
			astrtype=m_classdefbuf[i]->getclass();
			if(astrtype==typeid(value_type).name())
			{

				m_pCurParser->DefineVar(astrname,(value_type *)pclassvar);
			}

			pobj->m_objname.push_back(astrname);
			pobj->m_objbuf.push_back(pclassvar);
		}
		m_objmap[strobjname] = pobj;
		return pobj;
	};

	bool CreateClass::FindObjectBindingName(void *pobj, string_type &object_name) const
	{
		objmap_type::const_iterator item = m_objmap.begin();
		for (; item!=m_objmap.end(); ++item)
		{
			if(item->second == pobj)
			{
				object_name = item->first;
				return true;
			}
		}
		object_name.clear();
		return false;
	}

	
	double CreateClass::ApplyClassFunc(void *pobj,const string_type &a_strFuncName,paramvect& parm)
	{
		string_type strobjname;
		if(!FindObjectBindingName(pobj, strobjname))
			return 0;

		objmap_type::iterator obj_item = m_objmap.find(strobjname);
		if (obj_item == m_objmap.end() || obj_item->second == NULL)
			return 0;
		objstruct *bound_object = obj_item->second;

		if (!bound_object->m_ctor_ran &&
			a_strFuncName != "__ctor__" &&
			a_strFuncName != "__factory__" &&
			a_strFuncName != "create")
		{
			const char *ctor_script = FindCtorFuncDef();
			if (ctor_script != NULL)
			{
				paramvect empty_ctor_args;
				m_pCurParser->CompileFuncAndRunString(ctor_script, m_classname, strobjname, empty_ctor_args);
			}
			bound_object->m_ctor_ran = true;
		}

		funcodemap_type::iterator func_item = m_codemap.find(a_strFuncName);
		if (func_item == m_codemap.end() || func_item->second == NULL)
			return 0;

		m_pCurParser->CompileFuncAndRunString(func_item->second->m_strbuf,m_classname,strobjname, parm);
		if (a_strFuncName == "__ctor__" || a_strFuncName == "__factory__" || a_strFuncName == "create")
			bound_object->m_ctor_ran = true;
		return 0;

	};

	
	double CreateClass::ApplyClassFunc(void *pobj,void  *apclassfunc,paramvect& parm)
	{

		return 0;
	}

	
	double CreateClass::ApplyClassFunc(void *pobj,void  *apclassfunc,voidparamvect& parm)
	{
		return 0;
	}

	
	double CreateClass::ApplyClassFunc(void *pobj,void  *apclassfunc,charpvect& parm)
	{
		return 0;
	};
}
