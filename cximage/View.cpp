
#include "pch.h"

#include "View.h"
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
   // Handle(Image_AlienPixMap) anImage = new Image_AlienPixMap;
   // if (anImage->Load("0.jpg"))
    {
   //     myBackgroundImage = anImage;
    }  
    myBackgroundImage = img;
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
    /* if(0)
    if (!myBackgroundImage.IsNull())
    {
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glEnable(GL_TEXTURE_2D);

        GLuint textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);

        //       ܵ    ݶ       
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        // ȷ  ʹ    ȷ   ڲ   ʽ ͸ ʽ
        GLint internalFormat = myBackgroundImage->IsTopDown() ? GL_RGB : GL_BGR; //      Image_PixMap ʹ   BGR   ʽ
        GLenum format = myBackgroundImage->IsTopDown() ? GL_RGB : GL_BGR;

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, myBackgroundImage->SizeX(), myBackgroundImage->SizeY(), 0,
            format, GL_UNSIGNED_BYTE, myBackgroundImage->Data());

        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f, -1.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(1.0f, -1.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(1.0f, 1.0f);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f, 1.0f);
        glEnd();

        glDeleteTextures(1, &textureID);
        glPopAttrib();
    }
  */  //    û     Redraw      Լ             ά    
    V3d_View::Redraw();
}


