#include "MainFrm.h"

CElementBase* MainFrm::focusEle;
list<CElementBase*> MainFrm::list_Elements;
list<CElementBase*> MainFrm::selectElements;
list<CElementBase*> MainFrm::fullElements;
vector<CElementBase*> MainFrm::mySolutionList;
MainFrm::MainFrm()
{
  
	m_pProc = NULL;
}
MainFrm::~MainFrm()
{ 
 
}
void MainFrm::AddElement(ElementType typeName, ShapeRecognition rec, tuple<string, string, string> tuples, bool isUpdate)
{
	CElementBase* ptr_Ele = InitElement(typeName, rec, tuples);
	if (nullptr == ptr_Ele)
	{
		return;
	}
	if (!rec.IsScan)
	{
		focusEle = ptr_Ele;
		if (focusEle->m_strElementUid == "" || focusEle->m_strElementUid == get<0>(tuples))
		{
			list_Elements.push_back(focusEle);
		}
	}
	else
	{
		TryAddToListElement(fullElements, ptr_Ele);
	}

}
void MainFrm::ShowSolutionDialog(vector<CElementBase*> solutionList)
{
	string sulutionsCheck[4]{ "MIN","MID","MAX","SUPPER" };
	{
		for each(CElementBase * obj in solutionList)
		{
			
			auto myShape = obj->Draw();
			if (!myShape)
				continue;
			if (obj->Tag == Special)
			{
				myShape->SetSelect(true);
				if (obj->solution_Tag != "")
				{}
			}

			myShape->SetName(obj->m_strElementUid);
		}

		mySolutionList = solutionList;
	}
}
void MainFrm::TryAddToListElement(list<CElementBase*>& elements, CElementBase* element)
{
	string uid = element->m_strElementUid;
	auto findIdx = find_if(elements.begin(), elements.end(), [uid](const CElementBase* ele)
		{
			return ele->m_strElementUid == uid;
		});
	int i = -1;
	if (findIdx != elements.end())
		i = distance(elements.begin(), findIdx);
	if (i != -1)
	{
		auto it = elements.begin();
		std::advance(it, i);
		*it = element;
	}
	else
		elements.push_back(element);
}

void MainFrm::OCCViewEraseAll()
{
}

CElementBase* MainFrm::InitElement(ElementType typeName, ShapeRecognition rec, tuple<string, string, string> tuples)
{
	CElementBase* ptr_Element = ElementFactory::EleCreate(typeName);
	if (nullptr == ptr_Element)
	{
		return ptr_Element;
	}
	string uid = get<0>(tuples);
	string name = get<1>(tuples);
	string drawMode = get<2>(tuples);
	ptr_Element->EleInit(fullElements.size(), uid, rec);
	ptr_Element->SetSlutionCallBack(ShowSolutionDialog);
	ptr_Element->m_strElementName = name;
	ptr_Element->m_bIsScan = rec.IsScan;
	ptr_Element->m_strDrawMode = drawMode;
	return ptr_Element;
}
void MainFrm::EraseALL()
{
	OCCViewEraseAll();
	list_Elements.clear();
	selectElements.clear();
	fullElements.clear();
}

