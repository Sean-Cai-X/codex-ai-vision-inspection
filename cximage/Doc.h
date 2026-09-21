#ifndef _Doc_Header
#define _Doc_Header

#include "occtinclude.h"

#include <map>
using namespace std;
class Doc  
{
public:
	Doc();
	~Doc();
	 
// app
	Handle(Graphic3d_GraphicDriver) m_GraphicDriver;
	Handle(Graphic3d_GraphicDriver) GetGraphicDriver() { return m_GraphicDriver; }


// 操作
public:
	map<string, Handle(AIS_InteractiveContext)> myAISContextMap;
	map<string, Handle(V3d_Viewer)> myViewerMap;
	Handle(V3d_Viewer) GetViewer(std::string name);
	Handle(AIS_InteractiveContext) GetAISContext(std::string name)
	{
		return  myAISContextMap[name];
	}
	void ShowShape(const Handle(AIS_InteractiveObject)& AShapes, std::string name, Standard_Boolean isShow = Standard_True);
	void UpDateViewer(std::string name);
	void InitShowStyle(Handle(AIS_InteractiveContext) &myAISContext, int index);
	void RemoveShape(std::string name, const Handle(AIS_InteractiveObject)& AShape);

 
   
	  
};


#endif