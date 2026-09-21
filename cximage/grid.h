#ifndef GRID_H
#define GRID_H

#include <vector>
#include <string>
#include <map>
#include "Shape.h"
#include "Image.h"
#include "shapebase.h"


typedef struct objectrelation
{

    short m_itopx;
    short m_itopy;
    short m_ibottomx;
    short m_ibottomy;
    short m_ileftx;
    short m_ilefty;
    short m_irightx;
    short m_irighty;

    short m_igdigridx;
    short m_igdigridy;
    short m_ishowgridx;
    short m_ishowgridy;

    int m_isize;

    int m_itype;
}ObjRelation;

typedef struct gd_Rectangle
{
    gp_Pnt x;
    gp_Pnt y;
    int iw;
    int ih;

}gd_Rectangle;

typedef std::map<std::string, gd_Rectangle> RectMap;
typedef std::map<std::string, gd_Rectangle>::iterator RectMapItor;

typedef std::map<std::string, int> GridType;
typedef std::map<std::string, int>::iterator GridTypeItor;

typedef std::map<std::string, PointsShape> GridPoints;
typedef std::map<std::string, PointsShape>::iterator GridPointsItor;

typedef vector<ObjRelation> RelationVect;
typedef vector<ObjRelation>::iterator RelationVectItor;

typedef std::map<std::string, RelationVect> GridObjects;
typedef std::map<std::string, RelationVect>::iterator GridObjectsItor;

typedef struct easyobject
{
    int s_iwobjnum;
    int s_ibobjnum;

}easyobj;


class FindObject;

// Grid stores rasterized model data and grid-based pattern metadata.
class Grid
{
public:
    Grid();
    ~Grid();

    enum eGRIDType {
        GRIDType_THING = 1,
        GRIDType_GROUP = 2,
        GRIDType_EDGE = 4,
        GRIDType_EDGE_A = 8,
        GRIDType_EDGE_B = 16,
        GRIDType_FAIL = 32,
        GRIDType_NOVALID = 64,
        GRIDType_EDGE_C = 128,
        GRIDType_EDGE_D = 256,
        GRIDType_NONE = 0
    };

private:
     
    int m_ishow;
    RectMap m_grid;
    GridType m_gridT;
    GridObjects m_gridR;//result

    PointsShape m_modelpoints; 
    FindObject* g_pbackfindobject;
    int m_iorgx;
    int m_iorgy;
    int m_iw;
    int m_ih;

    int m_igridw;
    int m_igridh;
    int m_iwgridsum;
    int m_ihgridsum;
    int m_iwgridgap;
    int m_ihgridgap;

    int m_irealw;
    int m_irealh;

    int m_ishowpicw;
    int m_ishowpich;

    int m_imodelmatrixsum;

    int m_imapmodelw;
    int m_imapmodelh;

    int m_iunitw;
    int m_iunith;


    vector<int> m_fastmodel; 

public:
    void setroi(int iOrgX, int iOrgY, int iWidth, int iHeight);
    void setgrid(int ih, int iw, int iwsum, int ihsum, int ihgap, int iwgap);
    virtual void drawshape();
    virtual void Move(int ix, int iy) { (void)ix; (void)iy; }
    virtual void Rotate(double iangle) { (void)iangle; }
    virtual void Zoom(double dx0, double dy0) { (void)dx0; (void)dy0; }
    virtual int getpointnum() { return 0; }

    void settype(int igdix, int igdiy, int ivalue);
    void settypevalue(int igdix, int igdiy, int ivalue);
    int gettypevalue(int igridx, int igridy);
    void clear();

    int show() const;
    void setshow(int ishow);
    void setbrush(int itype, int icolor);

    void roiimagetomodel(void* pimage);
    void ROIImagetoModel(Image& aimage);
    void ROIImagetoModel_gray(Image& aimage);

    void savemodelfile(const char* pchar);
    void loadmodelfile(const char* pchar);
    void modeltogrid(int iatype, int ibtype);

    void savemapmodel(const char* pchar);
    void loadmapmodel(const char* pchar);

    void ReSetModelGrid();

    void UnitGrid();
    void ZeroModel();
    void EdgeGrid();
    void ReGrid(int iw, int ih);

    void ModelGridMethod_Gauss();
    void ModelGridMethod_Object();//
    void ModelGridMethod_Inside();
    void ModelGridMethod_Outside();

    int getfastmodelvalue(int ix, int iy);
    int getfastmodelw();
    int getfastmodelh();

    int getobject_areagrid_max();
    int getobject_totalvalue_max();


    void SetModelWH(int iw, int ih);
    easyobj ModelGridMethod_ObjectA();

    gp_Rectangle CentRect(gp_Rectangle& rect);
    gp_Rectangle CentRect_Condition(gp_Rectangle& rect, int imax);
    void CentGrid(int iw, int ih);
    void SetUnit(int iw, int ih);
    string GetGridString();
    void setfastlistvalue(int inum, int ivalue);
    vector<int>& getfastmodel();
    PointsShape& getpatmodel();
    void SetFastModel(vector<int>& fastmodel);
    void GridZoom(int iw, int ih);
    void Grid2PattenModel(int icompgap);
    void Grid2PattenModel_org(int icompgap);
    void GridFastModel(Grid* pgrid);
    bool GridCompare(Grid* pgrid);
};

#endif //GRID_H
