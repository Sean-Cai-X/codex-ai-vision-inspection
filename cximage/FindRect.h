#ifndef _findrect_Header
#define _findrect_Header

#include "Shape.h"
#include "Image.h"
#include "shapebase.h"
#include "FindLine.h"
#include "../cxgeom/include/CxGeomElementBody.h"

class ICxShapeSink;

class FindRect : public Shape
{
public:
    FindRect();
    ~FindRect();

    void clear();
    void setshow(int ishow);
    virtual void setrect(int ix, int iy, int iw, int ih);
    void setrotatedrect(double cx, double cy, double width, double height, double angle_deg);
    virtual void drawshape();
    void shapesetroi(void* pshape);

    void setthre(int ithre);
    int thre() const;
    void setcomparegap(int igap);
    int getcomparegap() const;
    void setlinegap(int igap);
    int linegap() const;
    void setmethod(int imethod);
    int method() const;
    void setgauge(int igauge);
    int gauge() const;
    void setfindsetting(int ifindset);
    void setfilter(int ifilterborw, int ifiltermin, int ifiltermax);

    void setminmaxarea(int imin, int imax);
    void setminmaxwh(int iminw, int imaxw, int iminh, int imaxh);
    void setpolygonepsilon(double epsilon_ratio);
    void setfillratio(double min_fill_ratio);

    void measure(void* pimage);
    void Measure(Image& image);

    RectsShape& getresultrects();
    gp_Rectangle getresultrect(int inum) const;
    int getresultobjsnum() const;
    cxgeom::CxSurfaceElement makeresultelement(int inum, int entity_id) const;
    const char* getfailurestage() const { return m_last_failure_stage.c_str(); }
    int getdebugseedvalid() const { return m_debug_seed_valid ? 1 : 0; }
    int getdebugtopvalid() const { return m_debug_top_valid ? 1 : 0; }
    int getdebugbottomvalid() const { return m_debug_bottom_valid ? 1 : 0; }
    int getdebugleftvalid() const { return m_debug_left_valid ? 1 : 0; }
    int getdebugrightvalid() const { return m_debug_right_valid ? 1 : 0; }
    int getdebugtoppoints() const { return m_debug_top_points; }
    int getdebugbottompoints() const { return m_debug_bottom_points; }
    int getdebugleftpoints() const { return m_debug_left_points; }
    int getdebugrightpoints() const { return m_debug_right_points; }
    double getdebugcoarsescore() const { return m_debug_coarse_score; }
    double getdebugrefinescore() const { return m_debug_refine_score; }

    bool hasresult() const
    {
        return m_lastresult.valid && m_resultrects.size() > 0;
    }

    void PublishDisplayShapes(
        ICxShapeSink& sink,
        const std::string& owner_ref) const;

    struct EdgeLearnResult
    {
        bool valid = false;
        struct FittedLineData
        {
            bool valid = false;
            double nx = 0.0;
            double ny = 0.0;
            double c = 0.0;
            int point_count = 0;
            double fit_error = 0.0;
            double angle = 0.0;
        } line;
        FindLineMeasureProfileStats stats;
        gp_Rectangle roi = gp_Rectangle(gp_Pnt(0, 0, 0), gp_Pnt(0, 0, 0));
    };

    struct RectLearnResult
    {
        bool valid = false;
        gp_Rectangle rect = gp_Rectangle(gp_Pnt(0, 0, 0), gp_Pnt(0, 0, 0));
        gp_Rectangle seed_rect = gp_Rectangle(gp_Pnt(0, 0, 0), gp_Pnt(0, 0, 0));
        EdgeLearnResult top;
        EdgeLearnResult bottom;
        EdgeLearnResult left;
        EdgeLearnResult right;
        int gauge = 0;
        double score = 0.0;
    };

private:
    bool LearnRectOnce(Image& image, const gp_Rectangle& working_rect, RectLearnResult& result);
    bool RefineRectOnce(Image& image, const gp_Rectangle& coarse_rect, RectLearnResult& result);

    RectsShape m_resultrects;
    RectLearnResult m_lastresult;
    std::string m_last_failure_stage;
    bool m_debug_seed_valid = false;
    bool m_debug_top_valid = false;
    bool m_debug_bottom_valid = false;
    bool m_debug_left_valid = false;
    bool m_debug_right_valid = false;
    int m_debug_top_points = 0;
    int m_debug_bottom_points = 0;
    int m_debug_left_points = 0;
    int m_debug_right_points = 0;
    double m_debug_coarse_score = 0.0;
    double m_debug_refine_score = 0.0;

    bool m_has_rotated_rect = false;
    double m_rotated_cx = 0.0;
    double m_rotated_cy = 0.0;
    double m_rotated_width = 0.0;
    double m_rotated_height = 0.0;
    double m_rotated_angle_deg = 0.0;
    Image* g_pbackimage;
    FindObject* g_pbackfindobject;
    int m_ithreshold;
    int m_icomparegap;
    int m_ilinegap;
    int m_imethod;
    int m_igauge;
    int m_ifindset;
    int m_ifilterborw;
    int64 m_ifiltermin;
    int64 m_ifiltermax;
    int m_iminarea;
    int m_imaxarea;
    int m_iminobjw;
    int m_imaxobjw;
    int m_iminobjh;
    int m_imaxobjh;
    double m_depsilonratio;
    double m_dminfillratio;
    FindLine m_topfinder;
    FindLine m_bottomfinder;
    FindLine m_leftfinder;
    FindLine m_rightfinder;
    static int m_curfindrectnum;
};

#endif
