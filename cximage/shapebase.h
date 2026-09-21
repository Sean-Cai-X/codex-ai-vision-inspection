#ifndef _Shapebase_Header
#define _Shapebase_Header

#include <vector>
#include <string>
#include <opencv2/opencv.hpp>
#include <AIS_InteractiveContext.hxx>
#include <AIS_ViewController.hxx>
#include <V3d_View.hxx> 
#include <Standard_Handle.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <Geom_Surface.hxx>
#include <GCPnts_QuasiUniformDeflection.hxx>
#include <Geom2d_Ellipse.hxx>
#include <Geom2dAdaptor_Curve.hxx>

#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <stdexcept>  
#include <cmath>

#include "Image.h"
#include "gp_path.h"




#define   PI 3.1415926535897
#define   RADIAN(a)     (a*PI/180.0)   
#define   ANGLE(r) (180.0*r/PI) 

using namespace std;


enum POINT_TYPE
{
  POINTTYPE_DOT=0,
  POINTTYPE_X,
  POINTTYPE_CIRCLE,
  POINTTYPE_RECT,
  POINTTYPE_UNKNOWN
};
enum HT_SHOW_SELECT
{
    POINT_SHOW,
    LINE_SHOW,
    ALL_SHOW
};
 
[[maybe_unused]] static void transformPoint(gp_Pnt& fpoint, double dzoomx, double dzoomy, double dmovx, double dmoovy)
{
 
    gp_Trsf trsf;
 
    trsf.SetScale(gp_Pnt(0, 0, 0), 1.0);  
     
    trsf.SetValues(dzoomx, 0, 0, dmovx,
        0, dzoomy, 0, dmoovy,
        0, 0, 1.0, 0);
 
    fpoint.Transform(trsf);
}

enum class CxShapeKind {
    Points,
    Line,
    Rect,
    Circle,
    Polyline,
    Ellipse,
    LineGauge
};

inline const char* CxShapeKindName(CxShapeKind kind)
{
    switch (kind)
    {
    case CxShapeKind::Points:
        return "PointsShape";
    case CxShapeKind::Line:
        return "LineShape";
    case CxShapeKind::Rect:
        return "RectShape";
    case CxShapeKind::Circle:
        return "CircleShape";
    case CxShapeKind::Polyline:
        return "PolylineShape";
    case CxShapeKind::Ellipse:
        return "EllipseShape";
    case CxShapeKind::LineGauge:
        return "LineGaugeShape";
    default:
        return "UnknownShape";
    }
}

struct CxShapePoint {
    double x = 0.0;
    double y = 0.0;
};

enum class CxShapeHandleRole {
    None,
    Body,
    Center,
    Start,
    End,
    WidthPositive,
    WidthNegative,
    Radius,
    InnerRadius,
    RadiusX,
    RadiusY,
    Corner0,
    Corner1,
    Corner2,
    Corner3,
    Vertex
};

inline const char* HandleName(CxShapeHandleRole role)
{
    switch (role)
    {
    case CxShapeHandleRole::Body: return "Body";
    case CxShapeHandleRole::Center: return "Center";
    case CxShapeHandleRole::Start: return "Start";
    case CxShapeHandleRole::End: return "End";
    case CxShapeHandleRole::WidthPositive: return "WidthPositive";
    case CxShapeHandleRole::WidthNegative: return "WidthNegative";
    case CxShapeHandleRole::Radius: return "Radius";
    case CxShapeHandleRole::InnerRadius: return "InnerRadius";
    case CxShapeHandleRole::RadiusX: return "RadiusX";
    case CxShapeHandleRole::RadiusY: return "RadiusY";
    case CxShapeHandleRole::Corner0: return "Corner0";
    case CxShapeHandleRole::Corner1: return "Corner1";
    case CxShapeHandleRole::Corner2: return "Corner2";
    case CxShapeHandleRole::Corner3: return "Corner3";
    case CxShapeHandleRole::Vertex: return "Vertex";
    case CxShapeHandleRole::None:
    default: return "None";
    }
}

struct CxShapeHandle {
    CxShapeHandleRole role = CxShapeHandleRole::None;
    int vertex_index = -1;
    CxShapePoint p;
    std::string label;
};

struct CxShapeHit {
    bool hit = false;
    CxShapeHandleRole role = CxShapeHandleRole::None;
    int vertex_index = -1;
    double distance = 0.0;
};

struct CxShapeGeometrySnapshot {
    CxShapeKind kind;
    std::vector<CxShapePoint> points;
    CxShapePoint center;
    double radius = 0.0;
    double inner_radius = 0.0;
    double half_width = 0.0;
    double radius_x = 0.0;
    double radius_y = 0.0;
    double angle = 0.0;
    bool closed = false;
};

class ShapeBase
{
    enum ShapeType { Line, Points,
                     Polyline,
                     Polygon, Rect,
                     RoundedRect,
                     Ellipse, Arc,
                 Chord, Pie, Path,
                     Text, Pixmap };

public:
    ShapeType m_shapetype;
    Quantity_Color m_color;
    bool m_antialiased;
    bool m_transformed;
    cv::Mat m_pixmap;
    int m_ishow;
    double m_dminpercent;
    int m_icount;
    int m_ishowlines;

public:
    ShapeBase();
    virtual ~ShapeBase();

    virtual CxShapeKind kind() const { return CxShapeKind::Points; }
    virtual CxShapeHit hitTest(double x, double y, double tolerance) const;
    virtual void enumerateHandles(std::vector<CxShapeHandle>& out) const;
    virtual void dragHandle(CxShapeHandleRole role, int vertex_index, double x, double y);
    virtual void translateBy(double dx, double dy);
    virtual bool snapshot(CxShapeGeometrySnapshot& out) const;

    virtual void exportPolyline(std::vector<CxShapePoint>& out, bool& closed) const;
    virtual bool exportCircle(CxShapePoint& center, double& radius, double& inner_radius) const;
    virtual bool exportLine(CxShapePoint& p0, CxShapePoint& p1) const;
    virtual bool exportEllipse(CxShapePoint& center, double& radius_x, double& radius_y, double& angle) const;
    virtual void exportPoints(std::vector<CxShapePoint>& out) const;
    int show();
    void setshow(int ishow);
    void setShape(int ishape);
    void setPen(int ir,int ig,int ib);
    void setPen(Quantity_Color qcolor);
    void setpenw(int iw);
    void setBrush(int ir,int ig, int ib);
    void setAntialiased(int antialiased);
    void setTransformed(int transformed);
    void setPercent(double dvalue);
 
    virtual void drawshape(gp_Path& painter) { (void)painter; }
    virtual void Move(int ix,int iy){ (void)ix; (void)iy; }
    virtual void Rotate(double iangle){ (void)iangle; }
    virtual void Zoom(double dx0,double dy0){ (void)dx0; (void)dy0; }
    virtual void setshowlines(int ilines){m_ishowlines=ilines;}
    virtual int getpointnum(){return 0;}
};

class QRootGrid;

// Root-grid hierarchy used by pattern grouping and lattice reconstruction.
class QRootGrid:public ShapeBase
{
public:
    QRootGrid();
    ~QRootGrid();

private:
     std::list <QRootGrid> m_glist;
     std::list <gp_Pnt> m_plist;

     int m_ilevel;
     int m_itype;
     int m_iclasstype;
     std::string m_name;
     gp_Pnt m_point;

public:
     void release();
     int level(){return m_ilevel;}
     std::string name(){return m_name;}
     void setlevel(int id){m_ilevel =id;}
     void setname(std::string name){m_name = name;}
     void settype(int itype){m_itype =itype;}
     void setshow(int ishow){m_ishow=ishow;}
     void addpoint(gp_Pnt&apoint);

     int getlevel();
     void addrootpointlist(std::list <gp_Pnt> &keypoints,int itype);
     void addpoint(gp_Pnt&arootpoint, gp_Pnt&apoint);
     virtual void drawshape(gp_Path&painter);
     void drawgrid(gp_Path&painter);
     void drawlayer(gp_Path&painter,int ilevel);

     void gridclasstype();
};
class LineShape:public ShapeBase
{
public:
    LineShape();
    ~LineShape();
private:
    SLine m_line;
    gp_Path m_path;
public:
    gp_Path& getpath() { return m_path; }
    SLine getline(){return m_line;}
    void setline(int ix0,int iy0,int ix1,int iy1);
    void setline(double x0, double y0, double x1, double y1);
    void MoveBy(double dx, double dy);
    void setshow(int ishow);

    void linecopy(Image&srcImage, Image&desImage);
    void linecopyex(Image&srcImage, Image&desImage,int ix,int iy);
    void crosspoint(LineShape &aline);
    void lineaex(int irate);
    void linebex(int irate);
    void linecv();
    gp_Pnt getlinepoint(int inum);
    int getlinesize();
    double getlinedistance();
    void copy(LineShape &aline);
    void clear();
    void setcolor(int ir, int ig, int ib);
    void linearInterp(int subPixelDensity);

    CxShapeKind kind() const override { return CxShapeKind::Line; }
    bool exportLine(CxShapePoint& p0, CxShapePoint& p1) const override;
    void exportPoints(std::vector<CxShapePoint>& out) const override;
    CxShapeHit hitTest(double x, double y, double tolerance) const override;
    void enumerateHandles(std::vector<CxShapeHandle>& out) const override;
    void dragHandle(CxShapeHandleRole role, int vertex_index, double x, double y) override;
    void translateBy(double dx, double dy) override;

    void drawshapex(gp_Path&painter,double dmovx,double dmovy,
        double dangle,double dzoomx,double dzoomy);
    virtual void drawshape(gp_Path&painter);
    virtual void Move(int ix,int iy);
    virtual void Rotate(double iangle);
    virtual void Zoom(double dx0,double dy0);
    virtual void setpenw(int iw);
    virtual void setshowlines(int ilines){m_ishowlines=ilines;}
 };
typedef std::vector<LineShape> LineVector;
class PointsShape:public ShapeBase
{
private:
    gp_Path m_path;
    gp_Path m_pathA;
    gp_Path m_pathB;

    std::vector<gp_Path> m_paths;
public:
    gp_Path& getpath() { return m_path; }
    gp_Path& getpathA() { return m_pathA; }
    gp_Path& getpathB() { return m_pathB; }

    PointsShape();

    void addpoint(gp_Pnt&apoint);  
    void addpoints(PointsShape &points);
  
    void setshow(int ishow);
    void clear();
    void copy(PointsShape& points);
    int size() const;
    int script_size();
    int ABsize() const;
    void calibration(double dx,double dy,double dangle);
    double getx(int inum) const;
    double gety(int inum) const;
    double script_getx(int inum);
    double script_gety(int inum);

    gp_Pnt getpointscent() const;
    void pointsABadd(void *pointsB);
    void pointslineadd(void *pointsB);

    void setcolor(int ir, int ig, int ib);
    void setcolorA(int ir, int ig, int ib);
    void setcolorB(int ir, int ig, int ib);
    void calculateEllipseParameters(const gp_Pnt* points,
        gp_Pnt& center, gp_Dir& dirMajorAxis, double& a, double& b);
    void ellipsepoints();

    int CircleStep(int apha,int x,int y,int i_x0,int i_y0,int  i_R_2);
    int EllipseStep(int apha,int x,int y,int i_ax,int i_ay,int i_bx,int i_by,int  i_dis, int& imx, int& imy);
    int EllipseStep_G(int apha, int x, int y, int i_ax, int i_ay, int i_bx, int i_by, int  i_dis, int& imx, int& imy);

    void arcpoints(int i_x,int i_y,int i_x0,int i_y0,int ipoinsum);

    void ellipsepointsx(int i_x, int i_y, int i_x0, int i_y0, int ix1, int iy1);

    void circlepoints(int i_x,int i_y,int i_x0,int i_y0);

    void halfcircle(int i_x,int i_y,int i_x1,int i_y1);
    
     void arccircle(int i_x,int i_y,int i_x1,int i_y1,int i_x0,int i_y0);
    
    void sample(double drate);
    void part(int ipart0,int ipart1);

    void ratetopoint(double dx,double dy,double drate);
    void addpoint(int apointx, int apointy);
    void addpoint(Standard_Real&apointx, Standard_Real&apointy);
    void addpointa(Standard_Real&apointx, Standard_Real&apointy);
    void addpointb(Standard_Real&apointx, Standard_Real&apointy);
    gp_Rectangle boundingRect();

    gp_Rectangle boundingRectA();
    gp_Rectangle boundingRectB();
    gp_Rectangle boundingRectAB();
    gp_Rectangle controlPointRect();
    void save(const char * pchar);
    void load(const char * pchar);
    void saveAB(const char* pchar);
    void loadAB(const char* pchar);
    void ABtoShape(std::vector<cv::Point2f>& points);

    void onepattern(double igap,int idirect, PointsShape& apoints);
    void doublepattern(double igap,int idirect, PointsShape& apoints);
    
    
    void patterngap2gap(int newgap);
    void patternABgap2gap(double newgaprate);
    void patternABsample(int irate);



    void doublesample(int isamplerate, PointsShape& apoints);
    void resampleAB(int inum);
    void patterntokeys(gp_Path&keypointsA, gp_Path&keypointsA_,
        gp_Path&keypointsB, gp_Path&keypointsB_,
                       int ibackgroundtype=0);
    void keystopattern(gp_Path&keypointsA, gp_Path&keypointsA_,
        gp_Path&keypointsB, gp_Path&keypointsB_,
                       int igap=2,int itype=1,int isgap =2,int iline=2);
    void keyszoom(gp_Path&keypointsA, gp_Path&keypointsA_,
        gp_Path&keypointsB, gp_Path&keypointsB_,
                  double dxz,double dyz);
    void patterntranform(int igap,int itype,int isgap,int iline);
    void patternzoom(double dxz,double dyz,int igap,int itype=0,int iline=1);

    void keysrootgrid(int ibackgroundtype,double drate =0.25,int ilevel = 4);

    void crosslinea(void *aline);
    void crosslineb(void *bline);
    void crosspoint(int inum0,int inum1,int inum2,int inum3);

    void findrightgrid(int inum,int igap,int &iminxnum,int &iminynum,int &imin3);

    void gridpoints(void *points);

    void gridrightline(void *points);
    void MakeShape(); 
    void MakeEdgeShape();
    void MakePointShape();
    void MakePointShapeAB();

    void AdaptiveDistfilter(int ik = 5);
    void ClusterPointCloud(double distanceThreshold = 1.0, double dk =5.0);
    void FftWaveletTransform(double distanceThreshold = 1.0, double waveletThreshold = 0.5);
    

    virtual void drawshape(gp_Path&painter);
    void drawshapex(gp_Path&painter,double dmovx,double dmovy,
    double dangle,double dzoomx,double dzoomy);
    virtual void Move(int ix,int iy);
    virtual void Rotate(double dangle);
    virtual void Zoom(double dx0,double dy0);

    void exportPoints(std::vector<CxShapePoint>& out) const override;

    CxShapeKind kind() const override { return CxShapeKind::Points; }
    CxShapeHit hitTest(double x, double y, double tolerance) const override;
    void enumerateHandles(std::vector<CxShapeHandle>& out) const override;
    void dragHandle(CxShapeHandleRole role, int vertex_index, double x, double y) override;
    void translateBy(double dx, double dy) override;

    void MoveAB(int ix, int iy);
    void RotateAB(double dangle);
    void ZoomAB(double dx0, double dy0);
    virtual void setshowlines(int ilines){m_ishowlines=ilines;}
    virtual int getpointnum(){return 0;}

    void getfindlinemodel(void* findline);

    void OBBCenterAngleSort(void* points);

    void PointsMaxLen(void* points);

    void ClusterPointsXX(std::vector<PointsShape>& seekpoints);
    void Sort();
    void SortPoints(int idirectionsx, int idirectionsy,int idisgap,int ianglescale);
    void SortPointsA(int idirectionsx, int idirectionsy);

    void ClusterPoints(double max_distance);
    void FilterPoints(double max_distance,double filter_points);

    void FindCrossPoints(void *points);
 
    void getSubPixelEdge(void * pimg);

};

typedef std::vector<PointsShape>  LineMeasurePoints;
class TwoPointsShape:public ShapeBase
{
public:
    TwoPointsShape();
public:
    gp_Path m_path;
    std::map<int, gp_Pnt> m_linemap;

    int m_insidewidth;
    int m_movoffset;
    int m_iheadtail;
    void setedgeoi(int insidewidth, int outsidewidth, int iheadtail);
    void addpoint(int ivaluea, int ivalueb);

    void clear();
    void makepath(int ivh);
    void edgeimage(cv::Mat &aImage,int itype);
    int size();
    virtual void drawshape(gp_Path& painter);
    virtual void Move(int ix,int iy);
    virtual void Rotate(double dangle);
    virtual void Zoom(double dx0,double dy0);
    virtual void setshowlines(int ilines){m_ishowlines=ilines;}
    virtual int getpointnum(){return 0;}
};
typedef vector<gp_Rectangle> RectVector;
typedef vector<double> AngleVector;
enum RectDirction{
    inside_dir = 0,
    outside_dir = 1,
    up_dir = 2,
    down_dir = 3,
    right_dir = 4,
    left_dir = 5,
    rightup_dir = 6,
    leftup_dir = 7,
    rightdown_dir = 8,
    leftdown_dir = 9,
    cross_dir = 10 ,
    same_dir = 11 ,
    up_pure_dir = 12,
    up_cross_dir = 13,
    down_pure_dir = 14,
    down_cross_dir = 15,
    down_purex_dir = 16
} ;
class RectsShape:public ShapeBase
{
public:
    RectsShape() {}
    void setcolor(int ir, int ig, int ib);
    void addrect(gp_Rectangle& arect);
    void addrect(gp_Rectangle& arect, std::string& astring);
    void clear();
    int size() const { return static_cast<int>(m_rects.size()); }
    gp_Rectangle getrect(int inum) const { if (m_rects.size() > inum)return m_rects[inum]; else return gp_Rectangle(gp_Pnt(0, 0, 0), gp_Pnt(0, 0, 0)); }
    void setspecshow(int ishownum){ m_ispecshow = ishownum; }
    void setrect(int inum,int ix,int iy,int iw,int ih);
    void setstring(int inum,const std::string &str);
    void setangle(int inum,double dangle);
    void removecontains_c(){}

    gp_Rectangle comb(gp_Rectangle&rect1, gp_Rectangle&rect2);
    gp_Rectangle comb(RectVector &rects1);
    RectDirction pos(gp_Rectangle&rect1, gp_Rectangle&rect2);

    void sort() {}
    void drawshapex(gp_Path&painter,double dmovx,double dmovy,
    double dangle, double dzoomx,double dzoomy){ (void)painter; (void)dmovx; (void)dmovy; (void)dangle; (void)dzoomx; (void)dzoomy; }
    virtual void drawshape(gp_Path&painter){ (void)painter; }
    virtual void Move(int ix,int iy){ (void)ix; (void)iy; }
    virtual void Rotate(double iangle){ (void)iangle; }
    virtual void Zoom(double dx0,double dy0){ (void)dx0; (void)dy0; }
    virtual int getpointnum(){return 0;}
    virtual void setshowlines(int ilines){m_ishowlines=ilines;}
    void MakeShape();
    void MakeShape(int inum);
    void MakePointShape();
private:
    gp_Path m_path;
    RectVector m_rects;
    vector<std::string> m_strlist;
    AngleVector m_angles;
    int m_ispecshow;
};
class Area: public gp_Rectangle
{
 public:
    enum RectType{
        orgarea = 0,
        unitarea = 1,
        crossarea = 2,
        multiarea = 3,
        releasearea = 4
    };

    Area(RectType type =orgarea);
    int ID(){return m_id;}
    RectType type(){return m_type;}
    std::string name(){return m_name;}
    void setid(int id){m_id =id;}
    void settype(RectType type){m_type = type;}
private:
    typedef vector<Area*> InsideAreas;
    InsideAreas m_insideareas;
    int m_id;
    RectType m_type;
    std::string m_name;

};
typedef vector<Area> AreaVector;
class QAreas
{ 
    typedef struct unitrelation
    {
        int igapx;
        int igapy;
        Area*m_pA;
        Area*m_pB;
        RectDirction m_dirs;
    } unitone;
    typedef vector<unitone> unitvect;

    typedef struct ObjectClass
    {
        vector<ObjectClass*> s_psub;
        short s_id;
        short s_level;
        ObjectClass *s_pU;
        ObjectClass *s_pD;
        ObjectClass *s_pL;
        ObjectClass *s_pR;
        ObjectClass *s_pO;
        ObjectClass *s_pRU;
        ObjectClass *s_pLU;
        ObjectClass *s_pLD;
        ObjectClass *s_pRD;


    } objectarea;

    typedef struct RecognizeObj
    {
        gp_Rectangle s_rect;
        short s_type;
        short s_relation;
        short s_singleW;
        short s_singleH;
        short s_num;
        Quantity_Color s_background;
    }recogobj;

    AreaVector m_areas;
    RectsShape m_rects;

public:
    QAreas(RectsShape &rects);
    void GenMap();
    void relation();
    void regroup();
    void sort();

};










#endif
