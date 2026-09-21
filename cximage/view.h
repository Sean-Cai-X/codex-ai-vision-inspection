#ifndef _View_Header
#define _View_Header
  
#include <V3d_View.hxx>
 


class V3d_CustomView : public V3d_View
{
public:
    V3d_CustomView(const Handle(V3d_Viewer)& theViewer);

    void SetBackgroundImage(const Handle(Image_PixMap)& img);

    void loadimage();

    virtual void Redraw();

private:
    Handle(Image_PixMap) myBackgroundImage;
};



#endif // _View_Header
