 
#include "view.h"
#include <AIS_Shape.hxx>
#include <Aspect_Handle.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <Message.hxx>
#include <Message_Messenger.hxx> 
#include <OpenGl_GraphicDriver.hxx>
#include <TopAbs_ShapeEnum.hxx>
 
#include <AIS_InteractiveContext.hxx> 
#include <V3d_View.hxx> 
#include <AIS_TexturedShape.hxx>
#include <Image_AlienPixMap.hxx>
#include <Geom_RectangularTrimmedSurface.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>

V3d_CustomView::V3d_CustomView(const Handle(V3d_Viewer)& theViewer)
        : V3d_View(theViewer), myBackgroundImage(nullptr)
{

}
void V3d_CustomView::SetBackgroundImage(const Handle(Image_PixMap)& img)
{   
 
}
 
void V3d_CustomView::loadimage()
{
    Handle(Image_AlienPixMap) anImage = new Image_AlienPixMap;
    if (anImage->Load("0.jpg"))
    { 
        myBackgroundImage = anImage;
    }
}


void V3d_CustomView::Redraw()
{
 
    V3d_View::Redraw();
}


