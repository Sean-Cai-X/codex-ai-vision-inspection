#ifndef _findellipse_Header
#define _findellipse_Header
#include "Shape.h"
#include "Image.h"
#include "shapebase.h"
#include "metrology_analytics/CxBoundaryResponse.h"
#include <string>
#include <vector>
class FindObject;
class ICxShapeSink;

struct FindEllipseDisplaySnapshot
{
    bool has_roi = false;

    double center_x = 0.0;
    double center_y = 0.0;
    double radius_x = 0.0;
    double radius_y = 0.0;
    int inner_scale_percent = 0;
    bool has_inner_ellipse = false;
    double inner_radius_x = 0.0;
    double inner_radius_y = 0.0;

    bool has_measure_points = false;
    int measure_points_count = 0;

    bool has_fit_ellipse = false;
    double fit_center_x = 0.0;
    double fit_center_y = 0.0;
    double fit_radius_x = 0.0;
    double fit_radius_y = 0.0;
    double fit_angle_deg = 0.0;
    double fit_avgdist = 0.0;

    int gap = 0;
    int linegap = 0;
    int min_edge_run_width_px = 3;
    int threshold = 0;
    int method = 0;
    int selected_edge_index = 0;
    int scan_line_count = 0;
    int scan_line_length = 0;
    std::string measure_failure_stage;
    std::string measure_failure_reason;

    int scan_candidate_lines = 0;
    int scan_total_candidates = 0;
    int rejected_min_edge_run_width_count = 0;
    int scan_accepted_points_before_gate = 0;
    int boundary_response_enabled = 0;
    int boundary_response_mode = 0;
    int boundary_response_polarity = 2;
    int boundary_response_selection = 0;
    int boundary_response_scan_lines_evaluated = 0;
    int boundary_response_candidate_count = 0;
    int boundary_response_points_emitted = 0;
    int boundary_response_rejected_no_candidate = 0;
    int boundary_response_rejected_endpoint = 0;
    double accepted_min_boundary_ratio = 0.0;
    double accepted_max_boundary_ratio = 0.0;
    double accepted_avg_boundary_ratio = 0.0;
    std::string candidate_policy;

    int scan_lines_outside_roi_count = 0;
    int scan_lines_cross_outside_ellipse_count = 0;
    double scan_endpoint_norm_min = 0.0;
    double scan_endpoint_norm_avg = 0.0;
    double scan_endpoint_norm_max = 0.0;

    int accepted_points_outside_ellipse_count = 0;
    double accepted_point_norm_min = 0.0;
    double accepted_point_norm_avg = 0.0;
    double accepted_point_norm_max = 0.0;

    int rejected_boundary_band_candidate_count = 0;
    double rejected_boundary_band_norm_min = 0.0;
    double rejected_boundary_band_norm_avg = 0.0;
    double rejected_boundary_band_norm_max = 0.0;

    int point_consistency_enabled = 0;
    double point_consistency_range = 0.0;
    int point_consistency_input_points = 0;
    int point_consistency_output_points = 0;
    int point_consistency_removed_points = 0;

    std::string scan_geometry_policy;
};

struct FindEllipseMeasureGeometryDebug
{
    struct ScanDiagnostic
    {
        int scan_index = -1;
        int candidate_count = 0;
        bool accepted = false;
        double accepted_x = 0.0;
        double accepted_y = 0.0;
        std::vector<double> accepted_points_xy;
        int accepted_position = -1;
        int min_edge_run_width_px = 3;
        int rejected_min_edge_run_width = 0;
        bool boundary_response_used = false;
        int boundary_response_mode = 0;
        int boundary_response_candidate_count = 0;
        int boundary_response_selected_index = -1;
        double boundary_response_selected_position = -1.0;
        double boundary_response_selected_score = 0.0;
        std::string boundary_response_status;
        std::string boundary_response_reason;
        std::string reject_reason;
    };
};

struct EllipseEdgeBandCandidate
{
    int scan_index = -1;
    int candidate_index = -1;
    int start_param = 0;
    int end_param = 0;
    int center_param = 0;
    double x = 0.0;
    double y = 0.0;
    double response_strength = 0.0;
    double polarity = 0.0;
    double arc_length = 0.0;
    int edge_rank = -1;
    bool valid = false;
    std::vector<double> profile;
};

struct EllipseFitCandidateSequence
{
    std::vector<cv::Point2d> points;
    double score = 0.0;
    int node_count = 0;
    double avg_ncc = 0.0;
    double total_response = 0.0;
    bool valid = false;
};

struct EllipseFeatureNode
{
    int id = -1;
    EllipseEdgeBandCandidate candidate;
    std::vector<int> neighbors;
    bool visited = false;
    int component_id = -1;
};

struct EllipseFeatureEdge
{
    int node_a = -1;
    int node_b = -1;
    double ncc_score = 0.0;
    double angular_distance = 0.0;
    bool valid = false;
};

struct EllipseFeatureGraph
{
    std::vector<EllipseFeatureNode> nodes;
    std::vector<EllipseFeatureEdge> edges;
    int next_component_id = 0;
};

class FindEllipse:public Shape
{
public:
    FindEllipse();
    ~FindEllipse();
    int gap() { return m_igap; }
    int thre();

    void clear();
    void setshow(int ishow);
    virtual void setrect(int ix, int iy, int iw, int ih) { (void)ix; (void)iy; (void)iw; (void)ih; }
    void setellipse(int icentx, int icenty, int ipax, int ipay);
    void setellipse2(int icentx, int icenty, int ipax, int ipay,int idis);
    void setbboxx0(int ix0);
    void setbboxy0(int iy0);
    void setbboxx1(int ix1);
    void setbboxy1(int iy1);
    void buildbbox();
    void setinnerpercent(int percent);
    int getinnerpercent() { return m_inner_scale_percent; }
    virtual void drawshape();
    void drawshapex( double dmovx, double dmovy,
        double dangle, double dzoomx, double dzoomy);

    void drawpattern();
    void drawpatternx(double dmovx, double dmovy,
        double dangle,
        double dzoomx, double dzoomy);
    void edgepattern(Image& image);
    void setcomparegap(int igap);
    void patternzeroposition();
    void savepatternfile(const char* pchar);
    void loadpatternfile(const char* pchar);
    gp_Rectangle patternboundingrect();
    void patternrootgrid(double itype, double drate, double ilevel);
    void patternzoom(double dx, double dy, double igap, double itype);
    void patterntranform(int igap, int itype, int isgap, int iline);
    void patternrotate(double dangle);
    void modelzoom(double dx, double dy);
    void patterngap2gap(int inewgap);
    gp_Path& getpatternpath();
    PointsShape& getpattern();
    void setpattern(PointsShape& apattern) { m_modelpoints = apattern; }

    void Setgap(int gap = 2);
    void measure(void* pimage);
    void measureRobust(void* pimage);
    void findpattern(void* pimage);

    void setlinesamplerate(double dsamplerate);
    void setlinegap(int igap);
    void setminedgerunwidth(int width);
    void setboundaryresponseenabled(int enabled);
    int getboundaryresponseenabled() const { return m_boundary_response_enabled ? 1 : 0; }
    void setboundaryresponsemode(int mode);
    int getboundaryresponsemode() const;
    void setboundarypolarity(int polarity);
    int getboundarypolarity() const;
    void setboundaryselection(int selection);
    int getboundaryselection() const;
    void setboundarynthcandidate(int nth);
    void setboundaryreferencepositionpermille(int permille);
    void setboundarysubpixel(int mode);
    void setboundarybaseline(int mode);
    void setboundarydenoise(int mode);
    void setboundarysmoothingradius(int radius);
    void setboundarybaselinewindow(int window);
    void setboundarywaveletscale(int scale);
    void setboundarythresholdpermille(int permille);
    void setboundarylevelpermille(int permille);
    void setboundaryhysteresispermille(int permille);
    void setboundarygatestartpermille(int permille);
    void setboundarygateendpermille(int permille);
    void setboundaryminplateauwidth(int width);
    void setboundaryminamplitudepermille(int permille);
    void setboundarypairminwidth(int width);
    void setboundarypairmaxwidth(int width);
    void setboundarypairanchor(int anchor);
    void setmethod(int imethod);
    void setthre(int ithre);
    void setgamarate(int igama);
    void setfindsetting(int ifindset);
    int getfindsetting() const { return m_ifindset; }
    void setfilter(int ifilterborw, int ifiltermin, int ifiltermax);//21 w ,22 b
    int getfilterborw() const { return m_ifilterborw; }
    int getfiltermin() const { return static_cast<int>(m_ifiltermin); }
    int getfiltermax() const { return static_cast<int>(m_ifiltermax); }
    int getdebugprefilterused() const { return m_debug_prefilter_used; }
    int getdebugprefiltercomponentcount() const { return m_debug_prefilter_component_count; }
    int getdebugprefilteracceptedcount() const { return m_debug_prefilter_accepted_count; }
    int getdebugprefilterrejectedcount() const { return m_debug_prefilter_rejected_count; }
    int getdebugprefiltermaxarea() const { return m_debug_prefilter_max_area; }
    int getdebugprefiltermaxw() const { return m_debug_prefilter_max_w; }
    int getdebugprefiltermaxh() const { return m_debug_prefilter_max_h; }
    int getdebugprefilterforegroundbefore() const { return m_debug_prefilter_foreground_before; }
    int getdebugprefilterforegroundafter() const { return m_debug_prefilter_foreground_after; }
    int minedgerunwidth() const { return m_min_edge_run_width_px; }
    void setselectedgenum(int iedgenum);
    void setpointconsistency(int enabled, int range);

    void setshowlines(int ilines) { m_ishowlines = ilines; }
    PointsShape& getresultpoints();
    void Measure(Image& image);
    void MeasureRobust(Image& image);
    void fitellipse();
    double getresultcentx();
    double getresultcenty();
    double getresultradiusx();
    double getresultradiusy();
    double getresultangle();
    double getavgdist();
    int getvalidpointcount();
    double hasfitresult();
    double get_result() { return hasfitresult(); }

    void shapesetroi(void* pshape);
    void MeasureT(void* pimage);

    int getconparegap() { return m_icomparegap; }

    void translate(int ix, int iy);
    void Translate(const gp_Vec& translationVector);
private:
    int m_icomparegap;
    PointsShape m_modelpoints;    //red(white 1) gap blue(black 0) model

    PointsShape m_measurepoints;

    PointsShape m_measurepoints_;

    Image* g_pbackimage;

    LineShape m_Line;
 
    FindObject* g_pbackfindobject;
  
    LineVector m_lines;

    int m_igap;

    gp_Pnt* m_listscanorA;
    gp_Pnt* m_listcollectorA;

    int m_iSelectPointGap;
    int m_min_edge_run_width_px = 3;
    int m_iMethod;
    int m_iThreshold;
    int m_igamarate;
    double m_dsamplerate;

    int m_ifindset;
    int m_ifilterborw;//21 w ,22 b ,23
    int64 m_ifiltermax;
    int64 m_ifiltermin;
    int m_debug_prefilter_used = 0;
    int m_debug_prefilter_component_count = 0;
    int m_debug_prefilter_accepted_count = 0;
    int m_debug_prefilter_rejected_count = 0;
    int m_debug_prefilter_max_area = 0;
    int m_debug_prefilter_max_w = 0;
    int m_debug_prefilter_max_h = 0;
    int m_debug_prefilter_foreground_before = 0;
    int m_debug_prefilter_foreground_after = 0;

    int m_iselectedgenum;
    int m_ineedfixs;
    bool m_boundary_response_enabled = false;
    bool m_boundary_response_selection_explicit = false;
    cxvision::metrology_analytics::CxBoundaryResponseConfig
        m_boundary_response_config;

    int m_ncurscan;
    int m_nscansize;

    int resultsize();
     
    static int m_curfindlinenum;

    int m_ishowlines;
    gp_Rectangle m_measurepointsboundingRect;
public:
    void easycluster(int igapx = 10, int igapy = 10, int iclusternum = 5);
    gp_Rectangle measurepointsboundingrect() { return m_measurepointsboundingRect; }

    bool getdisplaysnapshot(FindEllipseDisplaySnapshot& out) const;
    int getscandiagnosticcount() const;
    bool getscandiagnostic(int index,
                           FindEllipseMeasureGeometryDebug::ScanDiagnostic& out) const;
    bool getscandiagnosticline(int scan_index,
                               CxShapePoint& p0,
                               CxShapePoint& p1) const;
    int getscanlinecount() const;
    bool getscanline(int scan_index, CxShapePoint& p0, CxShapePoint& p1) const;

    void PublishDisplayShapes(
        ICxShapeSink& sink,
        const std::string& owner_ref) const;

private:
    bool m_has_display_roi = false;
    int m_roi_x0 = 0;
    int m_roi_y0 = 0;
    int m_roi_x1 = 0;
    int m_roi_y1 = 0;
    bool m_has_pending_bbox = false;
    int m_pending_bbox_x0 = 0;
    int m_pending_bbox_y0 = 0;
    int m_pending_bbox_x1 = 0;
    int m_pending_bbox_y1 = 0;
    int m_inner_scale_percent = 0;

    bool m_has_fit_result = false;
    double m_fit_center_x = 0.0;
    double m_fit_center_y = 0.0;
    double m_fit_radius_x = 0.0;
    double m_fit_radius_y = 0.0;
    double m_fit_angle_deg = 0.0;
    double m_fit_avgdist = 0.0;
    std::string m_measure_failure_stage;
    std::string m_measure_failure_reason;

    int m_scan_candidate_lines = 0;
    int m_scan_total_candidates = 0;
    int m_rejected_min_edge_run_width_count = 0;
    int m_scan_accepted_points_before_gate = 0;
    int m_boundary_response_scan_lines_evaluated = 0;
    int m_boundary_response_candidate_count = 0;
    int m_boundary_response_points_emitted = 0;
    int m_boundary_response_rejected_no_candidate = 0;
    int m_boundary_response_rejected_endpoint = 0;
    double m_accepted_boundary_ratio_sum = 0.0;
    double m_accepted_boundary_ratio_min = 999.0;
    double m_accepted_boundary_ratio_max = -999.0;
    std::string m_candidate_policy;

    int m_scan_lines_outside_roi_count = 0;
    int m_scan_lines_cross_outside_ellipse_count = 0;
    double m_scan_endpoint_norm_min = 999.0;
    double m_scan_endpoint_norm_max = -999.0;

    int m_accepted_points_outside_ellipse_count = 0;
    double m_accepted_point_norm_sum = 0.0;
    double m_accepted_point_norm_count = 0;
    double m_accepted_point_norm_min = 999.0;
    double m_accepted_point_norm_max = -999.0;

    int m_rejected_boundary_band_candidate_count = 0;
    double m_rejected_boundary_band_norm_sum = 0.0;
    double m_rejected_boundary_band_norm_min = 999.0;
    double m_rejected_boundary_band_norm_max = -999.0;
    int m_point_consistency_enabled = 0;
    double m_point_consistency_range = 0.0;
    int m_point_consistency_input_points = 0;
    int m_point_consistency_output_points = 0;
    int m_point_consistency_removed_points = 0;
    std::string m_scan_geometry_policy;

    void CollectEllipseEdgeBandsRobust(Image& image);
    void BuildEllipseFeatureGraph();
    void FindEllipseComponentsInGraph();
    void SelectBestEllipseSequence();
    void ConvertEllipseCandidatesToMeasurePoints();
    void ConvertEllipseSequenceToMeasurePoints(int sequence_index);

    std::vector<EllipseEdgeBandCandidate> m_ellipse_edge_band_candidates;
    std::vector<EllipseFitCandidateSequence> m_ellipse_fit_candidate_sequences;
    int m_ellipse_best_sequence_index = -1;
    EllipseFeatureGraph m_ellipse_feature_graph;
    std::vector<FindEllipseMeasureGeometryDebug::ScanDiagnostic> m_scan_diagnostics;

};

#endif
