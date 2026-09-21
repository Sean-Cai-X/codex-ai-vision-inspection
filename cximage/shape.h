#ifndef SHAPE
#define SHAPE


#include <AIS_InteractiveContext.hxx>
#include <AIS_ViewController.hxx>
#include <V3d_View.hxx>

#include <map>
#include <Standard_Handle.hxx>
#include <string>
#include "occtinclude.h"
#include "View.h"
#include "gp_path.h" 

using namespace std;

// Shape is a lightweight editable overlay built on top of gp_Path.
class Shape
{
public:
    enum Type { Rectangle = 0, Circle = 1, Triangle = 2, Font = 3 };

    Shape(Type type = Rectangle, const Quantity_Color& color = Quantity_NOC_GREEN, const gp_Rectangle& rect = gp_Rectangle(gp_Pnt(0,0,0), gp_Pnt(0,0,0)));

    virtual void drawshape();
    virtual void setrect(Standard_Real ix, Standard_Real iy, Standard_Real iw, Standard_Real ih);

    void setrect2(Standard_Real ix1, Standard_Real iy1, 
        Standard_Real ix2, Standard_Real iy2,
        Standard_Real ix3, Standard_Real iy3,
        Standard_Real ix4, Standard_Real iy4
    );

    void setcircle(Standard_Real icentx, Standard_Real icenty, Standard_Real ipax, Standard_Real ipay);
    void setellipse(Standard_Real icentx, Standard_Real icenty, Standard_Real ipax, Standard_Real ipay);

    void setcircle2(Standard_Real icentx, Standard_Real icenty, Standard_Real ipax, Standard_Real ipay, Standard_Real idis);
    void setellipse2(Standard_Real icentx, Standard_Real icenty, Standard_Real ipax, Standard_Real ipay, Standard_Real idis);

    virtual void setshowlines(int ilines) { m_ishowlines = ilines; }
    void drawshapex( double dmovx, double dmovy,
        double dangle, double dzoomx, double dzoomy) const;
    void Translate(const gp_Vec& translationVector);
    void translate(int ix, int iy);
    void setshow(int ishow);
    void settype(int itype);
    void setname(const char* pname);
    void setcolor(int ir, int ig, int ib);
    void setfont(int isize);
    void clear();
    Type type() const;

    string name() const;

    gp_Rectangle rect() const;
    gp_Rectangle resizeHandlez(double dzoomx, double dzoomy) const;
    gp_Rectangle resizeHandle() const;
    gp_Rectangle resizeHandlex(double dmovx, double dmovy,
        double dangle, double dzoomx, double dzoomy) const;

    Quantity_Color color() const;
    int show() const;
    void shapesetroi(void* pshape);
    void cutedge(int i_x0, int i_y0, int i_x1, int i_y1);
    static string typeToString(Type type);
    static Type stringToType(const string& s, bool* ok = 0);
    static const gp_Pnt minSize;
    void setresult(const string& result);
    string result() const;
    void setcontext(Handle(AIS_InteractiveContext) Context);

    Handle(AIS_Shape) getshape();
    gp_Pnt getcent() {
        return m_circlecent;
    }
    gp_Path& getpath() { return m_path; }
    gp_Path& getpath2() { return m_path2; }
private:
    gp_Path m_path;
    gp_Path m_path2; 
    Type m_type;
    gp_Rectangle m_rect;
    Quantity_Color m_color;
    string m_name;
    string m_result;
    int m_ipenwidth;
    int m_ishow = 1;
    int m_ishowlines;
    int m_ifontsize;
    gp_Pnt m_circlecent;
    gp_Pnt m_circlepa;
    gp_Pnt m_circlecent2;
    gp_Pnt m_circlepa2;
    friend class visionmanager;

};
#endif //SHAPE
