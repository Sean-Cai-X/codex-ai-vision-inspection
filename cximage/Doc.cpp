 #include "Doc.h"

 

Doc::Doc() 
{
	Handle(Aspect_DisplayConnection) aDisplayConnection;
	m_GraphicDriver = new OpenGl_GraphicDriver(aDisplayConnection);
 
	Handle(Graphic3d_GraphicDriver) theGraphicDriver = GetGraphicDriver();

	auto myViewer_OCC = new V3d_Viewer(theGraphicDriver);
	myViewer_OCC->SetDefaultLights();
	myViewer_OCC->SetLightOn();
	auto myViewer_Sub = new V3d_Viewer(theGraphicDriver);
	myViewer_Sub->SetDefaultLights();
	myViewer_Sub->SetLightOn();
	auto myViewer_Solut = new V3d_Viewer(theGraphicDriver);
	myViewer_Solut->SetDefaultLights();
	myViewer_OCC->SetLightOn();

	Handle(AIS_InteractiveContext) myAISContext_OCC = new AIS_InteractiveContext(myViewer_OCC);
	Handle(AIS_InteractiveContext) myAISContext_Sub = new AIS_InteractiveContext(myViewer_Sub);
	Handle(AIS_InteractiveContext) myAISContext_Solut = new AIS_InteractiveContext(myViewer_Solut);
	InitShowStyle(myAISContext_OCC,0);
	InitShowStyle(myAISContext_Sub,1);
	InitShowStyle(myAISContext_Solut,2);
	myAISContextMap.insert({ "m_pOCCView", myAISContext_OCC });
	myAISContextMap.insert({ "m_pOCCViewSub", myAISContext_Sub });
	myAISContextMap.insert({ "m_pOCCViewSolut", myAISContext_Solut });
	myViewerMap.insert({ "m_pOCCView", myViewer_OCC });
	myViewerMap.insert({ "m_pOCCViewSub", myViewer_Sub });
	myViewerMap.insert({ "m_pOCCViewSolut", myViewer_Solut });
}

void Doc::InitShowStyle(Handle(AIS_InteractiveContext)& myAISContext,int index)
{
	myAISContext->SetDisplayMode(AIS_WireFrame, true);
	myAISContext->SetAutomaticHilight(Standard_True);


	Handle(Prs3d_Drawer) aHighlightStyle = myAISContext->HighlightStyle();
	Handle(Prs3d_LineAspect) highlightStyle = aHighlightStyle->LineAspect();
	highlightStyle->SetWidth(3.0);
	aHighlightStyle->SetMethod(Aspect_TOHM_COLOR);
	aHighlightStyle->SetColor(Quantity_NOC_DEEPPINK);
	aHighlightStyle->SetDisplayMode(1);
	myAISContext->SetHighlightStyle(aHighlightStyle);
	Handle(Prs3d_Drawer) aSelectionStyle = myAISContext->SelectionStyle();
	Handle(Prs3d_LineAspect) lineStyle = aSelectionStyle->LineAspect();
	lineStyle->SetWidth(3.0);
	aSelectionStyle->SetMethod(Aspect_TOHM_COLOR);
	if(index == 2)
		aSelectionStyle->SetColor(Quantity_NOC_SLATEBLUE);
	else
		aSelectionStyle->SetColor(Quantity_NOC_WHITE);
	aSelectionStyle->SetDisplayMode(1);
	myAISContext->SetSelectionStyle(aSelectionStyle);

}

Doc::~Doc()
{
}

 

Handle(V3d_Viewer) Doc::GetViewer(std::string name)
{
	return myViewerMap[name];
}

void Doc::ShowShape(const Handle(AIS_InteractiveObject)& AShapes, std::string name, Standard_Boolean isShow)
{
	myAISContextMap[name]->Display(AShapes, isShow);
}

void Doc::UpDateViewer(std::string name)
{
	myAISContextMap[name]->UpdateCurrentViewer();
}

void Doc::RemoveShape(string name, const Handle(AIS_InteractiveObject)& AShape)
{
	myAISContextMap[name]->Erase(AShape, Standard_False);
}

 
 
