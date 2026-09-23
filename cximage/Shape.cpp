#include "pch.h"

#include "Shape.h"

static const int resizeHandleWidth = 10;
 

const gp_Pnt Shape::minSize(30, 20, 0);

Shape::Shape(Type type, const Quantity_Color& color, const gp_Rectangle& rect)
    : m_type(type),
    m_rect(rect),
    m_color(color),
    m_ipenwidth(1),
    m_ishow(1),
    m_ifontsize(8)
{
}
Handle(AIS_Shape) Shape::getshape()
{ 
    return m_path.getshape(); 
};
void Shape::setcontext(Handle(AIS_InteractiveContext) Context)
{ 
    m_path.SetContext(Context); 
}
void Shape::setshow(int ishow)
{
    m_ishow = ishow;

    m_path.PathShow(ishow);
    m_path2.PathShow(ishow);
    
}
void Shape::translate(int ix, int iy)
{
    Translate(gp_Vec(ix, iy, 0));
}
void Shape::Translate(const gp_Vec& translationVector)
{
    m_path.Translate(translationVector);
    m_rect.setrect(m_path.ElementAt(0), m_path.ElementAt(2));
}
void Shape::settype(int itype)
{
    switch (itype)
    {
    case 0:
        m_type = Shape::Rectangle;
        break;
    case 1:
        m_type = Shape::Circle;
        break;
    case 2:
        m_type = Shape::Triangle;
        break;
    case 3:
        m_type = Shape::Triangle;
        break;
    default:
        m_type = Shape::Rectangle;
        break;
    }

}
void Shape::setname(const char* pname)
{
    m_name = string(pname);
}
void Shape::clear()
{
    m_path.Clear();
    m_path2.Clear(); 
}
void Shape::setrect(Standard_Real ix, Standard_Real iy, Standard_Real iw, Standard_Real ih)
{
    m_path.Clear();
    m_rect = gp_Rectangle(gp_Pnt(ix, iy,0), gp_Pnt(ix+iw, iy+ih,0));
    m_path.AddRect(m_rect);
}
void Shape::setrect2(Standard_Real ix1, Standard_Real iy1,
    Standard_Real ix2, Standard_Real iy2,
    Standard_Real ix3, Standard_Real iy3,
    Standard_Real ix4, Standard_Real iy4
)
{
    m_path.Clear(); 
    m_path.AddRect2(gp_Pnt(ix1,iy1,0), gp_Pnt(ix2, iy2, 0),
        gp_Pnt(ix3, iy3, 0), gp_Pnt(ix4, iy4, 0));
}
void Shape::setcircle(Standard_Real icentx, Standard_Real icenty, Standard_Real ipax, Standard_Real ipay)
{
    if (icentx == ipax && icenty == ipay)
        return;
    m_path.Clear();
    m_circlecent = gp_Pnt(icentx, icenty, 0); 
    m_circlepa = gp_Pnt(ipax, ipay, 0);
    double radius = sqrt((icentx - ipax) * (icentx - ipax) + (icenty - ipay) * (icenty - ipay));
    m_path.AddCircle(m_circlecent, radius);
}
void Shape::setellipse(Standard_Real icentx, Standard_Real icenty, Standard_Real ipax, Standard_Real ipay)
{
    if (icentx == ipax && icenty == ipay)
        return;
    m_path.Clear();
    m_circlecent = gp_Pnt(icentx, icenty, 0);
    m_circlepa = gp_Pnt(ipax, ipay, 0); 
    m_path.AddRectangularEllipse(m_circlecent, m_circlepa);
}

void Shape::setcircle2(Standard_Real icentx, Standard_Real icenty, Standard_Real ipax, Standard_Real ipay, Standard_Real idis)
{
    if (icentx == ipax && icenty == ipay)
        return;

    m_path.Clear();
    m_path2.Clear();
    m_circlecent = gp_Pnt(icentx, icenty, 0);
    m_circlepa = gp_Pnt(ipax, ipay, 0);
    double radius = sqrt((icentx - ipax) * (icentx - ipax) + (icenty - ipay) * (icenty - ipay));
    if (radius <= 0 || idis <= 0)
        return;
    m_path.AddCircle(m_circlecent, radius);
    if(radius - idis>0)
    m_path2.AddCircle(m_circlecent, radius- idis);
}
void Shape::setellipse2(Standard_Real icentx, Standard_Real icenty, Standard_Real ipax, Standard_Real ipay, Standard_Real idis)
{
    if (icentx == ipax && icenty == ipay)
        return;
    m_path.Clear();
    m_path2.Clear();
    m_circlecent = gp_Pnt(icentx, icenty, 0);
    m_circlepa = gp_Pnt(ipax, ipay, 0);

    const double x0 = std::min(icentx, ipax);
    const double y0 = std::min(icenty, ipay);
    const double x1 = std::max(icentx, ipax);
    const double y1 = std::max(icenty, ipay);
    const double cx = (x0 + x1) * 0.5;
    const double cy = (y0 + y1) * 0.5;
    const double radiusX = (x1 - x0) * 0.5;
    const double radiusY = (y1 - y0) * 0.5;
    const double innerRadiusX = radiusX - idis;
    const double innerRadiusY = radiusY - idis;

    m_circlecent = gp_Pnt(x0, y0, 0);
    m_circlepa = gp_Pnt(x1, y1, 0);
    m_path.AddRectangularEllipse(m_circlecent, m_circlepa, 0.02);

    if (innerRadiusX > 0.0 && innerRadiusY > 0.0)
    {
        m_circlecent2 = gp_Pnt(cx - innerRadiusX, cy - innerRadiusY, 0);
        m_circlepa2 = gp_Pnt(cx + innerRadiusX, cy + innerRadiusY, 0);
        m_path2.AddRectangularEllipse(m_circlecent2, m_circlepa2, 0.02);
    }
}
 
void Shape::setcolor(int ir, int ig, int ib)
{
    m_color = Quantity_Color(ir/255.0, ig / 255.0, ib / 255.0, Quantity_TypeOfColor::Quantity_TOC_RGB);
    m_path.setcolor(ir, ig, ib);
}

Shape::Type Shape::type() const
{
    return m_type;
}

gp_Rectangle Shape::rect() const
{
    return m_rect;
}

Quantity_Color Shape::color() const
{
    return m_color;
}

string Shape::name() const
{
    return m_name;
}
int Shape::show() const
{
    return m_ishow;
}
gp_Rectangle Shape::resizeHandle() const
{
    Standard_Real brX0 = m_rect.BottomRight().X()- resizeHandleWidth;
    Standard_Real brY0 = m_rect.BottomRight().Y()- resizeHandleWidth;
    Standard_Real brX1 = m_rect.BottomRight().X()+ resizeHandleWidth;
    Standard_Real brY1 = m_rect.BottomRight().Y()+ resizeHandleWidth;
 
    
    return gp_Rectangle(gp_Pnt(brX0, brY0, 0), gp_Pnt(brX1, brY1, 0));
}
gp_Rectangle Shape::resizeHandlez(double dzoomx, double dzoomy) const
{
    Standard_Real brX0 = m_rect.BottomRight().X() - resizeHandleWidth / dzoomx;
    Standard_Real brY0 = m_rect.BottomRight().Y() - resizeHandleWidth / dzoomy;
    Standard_Real brX1 = m_rect.BottomRight().X() + resizeHandleWidth / dzoomx;
    Standard_Real brY1 = m_rect.BottomRight().Y() + resizeHandleWidth / dzoomy;

    return gp_Rectangle(gp_Pnt(brX0, brY0, 0), gp_Pnt(brX1, brY1, 0));
 }
gp_Rectangle Shape::resizeHandlex(double dmovx, double dmovy,
    double dangle, double dzoomx, double dzoomy) const
{
    (void)dangle;

    Standard_Real brX0 = m_rect.BottomRight().X() * dzoomx + dmovx - resizeHandleWidth / dzoomx;
    Standard_Real brY0 = m_rect.BottomRight().Y() * dzoomy + dmovy - resizeHandleWidth / dzoomy;
    Standard_Real brX1 = m_rect.BottomRight().X() * dzoomx + dmovx + resizeHandleWidth / dzoomx;
    Standard_Real brY1 = m_rect.BottomRight().Y() * dzoomy + dmovy + resizeHandleWidth / dzoomy;

    return gp_Rectangle(gp_Pnt(brX0, brY0, 0), gp_Pnt(brX1, brY1, 0));
}
string Shape::typeToString(Type type)
{
    string result;

    switch (type) {
    case Rectangle:
        result = string("rect");
        break;
    case Circle:
        result = string("circle");
        break;
    case Triangle:
        result = string("triangle");
        break;
    case Font:
        result = string("font");
        break;
    }

    return result;
}
void Shape::setresult(const string& result)
{
    m_result = result;
}
string Shape::result() const
{
    return m_result;
}
Shape::Type Shape::stringToType(const string& s, bool* ok)
{
    if (ok != 0)
        *ok = true;

    if (s == string("Rectangle"))
        return Rectangle;
    if (s == string("Circle"))
        return Circle;
    if (s == string("Triangle"))
        return Triangle;
    if (s == string("Font"))
        return Font;

    if (ok != 0)
        *ok = false;
    return Rectangle;
}
void Shape::drawshape() 
{
    switch (type())
    {
        case Shape::Rectangle: 
            m_path.DrawRect(rect());
            break;
        case Shape::Circle:
        {
            gp_Pnt apoint0 = rect().TopLeft();
            double dh = rect().Height();
            double dw = rect().Width();
            gp_Pnt acent(rect().TopLeft().X() + dw/2, rect().TopLeft().Y()+ dh/2, 0);

            m_path.DrawCircle(acent, dw/2);

        }
            break;
        case Shape::Triangle:
           
            break;
        case Shape::Font:
            break;
        default:
            break;
    }
  
}
void Shape::drawshapex( double dmovx, double dmovy,
    double dangle, double dzoomx, double dzoomy) const
{
    (void)dmovx;
    (void)dmovy;
    (void)dangle;
    (void)dzoomx;
    (void)dzoomy;
}
void Shape::setfont(int isize)
{
    m_ifontsize = isize;
}
void Shape::shapesetroi(void* pshape)
{
    Shape* ashp = (Shape*)pshape;
    if (nullptr != ashp)
    {
        gp_Rectangle arect = ashp->rect();
        setrect(arect.TopLeft().X(), arect.TopLeft().Y(), arect.Width(), arect.Height());
    }
}

void Shape::cutedge(int i_x0 ,int i_y0, int i_x1, int i_y1)
{
   m_path.Clear();
   const int ix = static_cast<int>(m_rect.TopLeft().X()) + i_x0;
   const int iy = static_cast<int>(m_rect.TopLeft().Y()) + i_y0;
   const int iw0 = static_cast<int>(m_rect.Width()) - i_x1 - i_x0;
   const int ih0 = static_cast<int>(m_rect.Height()) - i_y1 - i_y0;
   m_rect = gp_Rectangle(gp_Pnt(ix, iy,0), gp_Pnt(ix+iw0, iy+ih0,0));
   m_path.AddRect(m_rect);
}

