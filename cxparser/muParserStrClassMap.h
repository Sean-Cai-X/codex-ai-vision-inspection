

#if !defined(__PARSERTREENODE_H__)
#define __PARSERTREENODE_H__

#include "muParserBytecode.h"

namespace mu
{

        
        class   muParserStrClassMap
	{
		private:

			typedef std::map<string_type,string_type> strmapfunc_type;

			typedef struct stringvirclass
			{
				string_type m_classdef;
				strmapfunc_type m_classfuncdef;

			}strclass;

			typedef std::map<string_type,strclass*> classmap_type;

		public:

			classmap_type m_strclassmap;
			strclass *m_pclassgroup;
			int m_iclassnum;

			muParserStrClassMap();
			virtual ~muParserStrClassMap();
			
			void AddClass(const string_type &a_strClassName)
			{
					EnsureClass(a_strClassName);
			}
			
			strclass *EnsureClass(const string_type &a_strClassName)
			{
					classmap_type::iterator item = m_strclassmap.find(a_strClassName);
					if(item!=m_strclassmap.end())
						return item->second;
					strclass *apclass =new strclass;
					m_strclassmap[a_strClassName]=apclass;
					return apclass;
			}
			
			void AddClassDef(const string_type &a_strClassName,
							const string_type &a_strclassdef)
			{
				strclass *apclass = EnsureClass(a_strClassName);
				apclass->m_classdef	= a_strclassdef;
			}
			
			void AddClassFun(const string_type &a_strClassName,
							const string_type &a_strclassfuncname,
							const string_type &a_strclassfuncdef)
			{
				strclass *apclass = EnsureClass(a_strClassName);
				apclass->m_classfuncdef[a_strclassfuncname]=a_strclassfuncdef;
			}
	};
}
#endif
