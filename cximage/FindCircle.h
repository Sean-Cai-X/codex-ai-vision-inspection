#ifndef _findcircle_Header
#define _findcircle_Header
#include "Shape.h"
#include "Image.h"
#include "shapebase.h"
#include "CxAlgorithmBudget.h"
#include "metrology_analytics/CxBoundaryResponse.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>
class FindObject;

struct FindCircleMeasureGeometryRequest
{
    bool valid = false;

    int center_x = 0;
    int center_y = 0;

    int pass_x = 0;
    int pass_y = 0;

    bool has_inner_gap = false;
    int inner_gap = 0;

    int gap_degrees = 0;

    // Optional angular scan sector.  Angles are normalized internally to
    // [0, 359], while callers may provide signed degrees (for example
    // -29..18) to describe a sector that crosses the zero-degree axis.
    int arc_start_degrees = 0;
    int arc_end_degrees = 360;
    bool has_arc_window = false;

    int linegap = 0;
    int min_edge_run_width_px = 3;

    double sample_rate = 1.0;

    std::uint64_t version = 0;
};

struct FindCircleMeasureGeometryDebug
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

    bool request_valid = false;
    bool geometry_dirty = false;
    bool geometry_ready = false;

    std::uint64_t geometry_version = 0;
    std::uint64_t geometry_built_version = 0;

    int center_x = 0;
    int center_y = 0;
    int pass_x = 0;
    int pass_y = 0;
    bool has_inner_gap = false;
    int inner_gap = 0;

    int gap_degrees = 0;
    int linegap = 0;
    int min_edge_run_width_px = 3;

    int scan_line_count = 0;
    int scan_line_length = 0;
    int process_width = 0;

    bool image_ready = false;
    int image_width = 0;
    int image_height = 0;
    int image_channels = 0;

    bool backimage_ready = false;
    bool findobject_ready = false;

    int object_prefilter_requested = 0;
    int object_prefilter_applied = 0;
    int object_prefilter_restored = 0;
    int object_prefilter_runs_before = 0;
    int object_prefilter_runs_after = 0;
    int object_prefilter_effective_min = 0;

    int measure_points_count = 0;
    int valid_points_count = 0;

    // Selected-edge diagnostics.  selected_edge_index is the actual value
    // consumed by FindCircle::Measure(): 0 means every eligible candidate on
    // each radial scan line, N means the N-th candidate, and -1 means the last
    // candidate on each radial scan line.
    int selected_edge_index = 0;
    int candidate_runs_total = 0;
    int candidate_runs_max_per_line = 0;
    int selected_edge_hits = 0;
    int selected_edge_misses = 0;
    int scan_boundary_clipped_lines = 0;
    int scan_boundary_extended_samples = 0;
    int candidate_min_edge_run_width_reject_count = 0;
    int candidate_boundary_reject_count = 0;
    int candidate_endpoint_reject_count = 0;
    int boundary_response_enabled = 0;
    int boundary_response_mode = 0;
    int boundary_response_polarity = 2;
    int boundary_response_selection = 0;
    int boundary_response_scan_lines_evaluated = 0;
    int boundary_response_candidate_count = 0;
    int boundary_response_points_emitted = 0;
    int boundary_response_rejected_no_candidate = 0;
    int boundary_response_rejected_endpoint = 0;
    double selected_edge_radius_avg = 0.0;
    double selected_edge_radius_min = 0.0;
    double selected_edge_radius_max = 0.0;

    std::string measure_source;
    std::string failure_stage;
    std::string detail;

    int scan_lines_processed = 0;
    int total_samples = 0;
    int elapsed_ms = 0;

    int budget_max_scan_lines = 2048;
    int budget_max_samples = 2000000;
    int budget_max_elapsed_ms = 3000;

    std::vector<ScanDiagnostic> scan_diagnostics;
};

struct CircleEdgeBandCandidate
{
    int scan_index = -1;
    int candidate_index = -1;
    int start_angle = 0;
    int end_angle = 0;
    int center_angle = 0;
    double x = 0.0;
    double y = 0.0;
    double response_strength = 0.0;
    double polarity = 0.0;
    double arc_length = 0.0;
    int edge_rank = -1;
    bool valid = false;
    std::vector<double> profile;
};

struct CircleFitCandidateSequence
{
    std::vector<cv::Point2d> points;
    double score = 0.0;
    int node_count = 0;
    double avg_ncc = 0.0;
    double total_response = 0.0;
    bool valid = false;
};

struct CircleFeatureNode
{
    int id = -1;
    CircleEdgeBandCandidate candidate;
    std::vector<int> neighbors;
    bool visited = false;
    int component_id = -1;
};

struct CircleFeatureEdge
{
    int node_a = -1;
    int node_b = -1;
    double ncc_score = 0.0;
    double angular_distance = 0.0;
    bool valid = false;
};

struct CircleFeatureGraph
{
    std::vector<CircleFeatureNode> nodes;
    std::vector<CircleFeatureEdge> edges;
    int next_component_id = 0;
};

struct FindCircleBoundaryPointSnapshot
{
    int scan_index = -1;
    double measured_x = 0.0;
    double measured_y = 0.0;
    double refined_x = 0.0;
    double refined_y = 0.0;
    double subpixel_offset = 0.0;
    double response_strength = 0.0;
    double local_noise = 0.0;
    double localization_sigma_px = 0.0;
    double fit_residual_px = 0.0;
    int polarity = 0;
    bool interpolation_valid = false;
    bool fit_residual_valid = false;
    std::array<double, 5> profile = { 0.0, 0.0, 0.0, 0.0, 0.0 };
};

struct FindCircleBoundaryAnalysisSnapshot
{
    std::string schema = "cxvision.findcircle_boundary_analysis.v1";
    std::string status = "NOT_ANALYZED";
    std::string reliability_level = "UNAVAILABLE";
    int expected_scan_count = 0;
    int accepted_point_count = 0;
    int interpolation_valid_count = 0;
    int fit_residual_count = 0;
    double coverage_ratio = 0.0;
    double response_mean = 0.0;
    double response_median = 0.0;
    double response_cv = 0.0;
    double subpixel_offset_mean = 0.0;
    double subpixel_offset_stddev = 0.0;
    double localization_sigma_mean_px = 0.0;
    double residual_rmse_px = 0.0;
    double residual_p95_px = 0.0;
    double residual_max_px = 0.0;
    double outlier_ratio = 0.0;
    double reliability_score = 0.0;
    std::vector<FindCircleBoundaryPointSnapshot> points;
};

class ICxShapeSink;

class FindCircle:public Shape
{
public:
    FindCircle();
    ~FindCircle();

    void PublishDisplayShapes(ICxShapeSink& sink, const std::string& owner_ref) const;

    int gap() { return m_igap; }
    int thre();

    void clear();
    void setshow(int ishow);
    virtual void setrect(int ix, int iy, int iw, int ih) { (void)ix; (void)iy; (void)iw; (void)ih; }
    void setcircle(int icentx, int icenty, int ipax, int ipay);
    void setcircle2(int icentx, int icenty, int ipax, int ipay, int idis);
    // Script/UI annulus contract: both radii are absolute image-space radii.
    // The implementation converts them once to the legacy setcircle2()
    // centre/pass/band-width representation used by the measure algorithm.
    void setannulus(int icentx, int icenty, int iinnerRadius, int iouterRadius);
    void setscanarc(int startDegrees, int endDegrees, int enabled);

    // CxScript adapters for the legacy fixed-int DefineClassFun dispatcher.
    // It calls multi-argument methods in reverse order.  The adapters keep
    // the public .cxsc form natural: setcircle(cx,cy,px,py),
    // setannulus(cx,cy,inner,outer), setscanarc(start,end,enabled).
    void cxscript_setcircle(int ipay, int ipax, int icenty, int icentx);
    void cxscript_setcircle2(int idis, int ipay, int ipax, int icenty, int icentx);
    void cxscript_setannulus(int iouterRadius, int iinnerRadius, int icenty, int icentx);
    void cxscript_setscanarc(int enabled, int endDegrees, int startDegrees);
    int getscanarcstart();
    int getscanarcend();
    int hasscanarc();
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
    void automeasure(void* pimage);

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
    void setfilter(int ifilterborw, int ifiltermin, int ifiltermax);//21 w ,22 b
    void setfilter_script(int stack_filter_max, int stack_filter_min, int stack_filter_borw)
    {
        setfilter(stack_filter_borw, stack_filter_min, stack_filter_max);
    }
    void setselectedgenum(int iedgenum);
    void getshape(void* pshape);
    void setcirclegap(int ivalue);

    void setshowlines(int ilines) { m_ishowlines = ilines; }
    PointsShape& getresultpoints();
    const PointsShape& getresultpoints() const;

    void fitcircle();
    void fitcirclefiltered();
    void setpointconsistency(int enabled, int range);
    int getpointconsistencyenabled() { return m_point_consistency_enabled ? 1 : 0; }
    double getpointconsistencyrange() { return m_point_consistency_range; }
    int getpointconsistencyinputcount() { return m_point_consistency_input_count; }
    int getpointconsistencyoutputcount() { return m_point_consistency_output_count; }
    int getpointconsistencyremovedcount() { return m_point_consistency_removed_count; }

    int getfitfilterinputcount() { return m_fitfilter_input_count; }
    int getfitfilterkeptcount() { return m_fitfilter_kept_count; }
    int getfitfilterrejectedcount() { return m_fitfilter_rejected_count; }
    double getfitfiltersigma() { return m_fitfilter_sigma; }
    double getfitfilterthreshold() { return m_fitfilter_threshold; }

    void Measure(Image& image);
    void MeasureBalanced(Image& image);
    void MeasureRobust(Image& image);

    void FitResultMeasure(void* pimage);
    void setfitmeasuregap(int igap);
    int m_fitmeasuregap = 80;

    void shapesetroi(void* pshape);
    void MeasureT(void* pimage);

    int getconparegap() { return m_icomparegap; }

    void translate(int ix, int iy);
    void Translate(const gp_Vec& translationVector);
    double getresultcentx();
    double getresultcenty();
    double getradius();
    double getavgdist();

    int getvalidpointcount();
    bool hasfitresult();
    bool canfitresultmeasure();

    double getresultcentx() const;
    double getresultcenty() const;
    double getradius() const;
    bool hasfitresult() const;

    int getcirclecentx() const { return m_icentx; }
    int getcirclecenty() const { return m_icenty; }
    int getcirclepax() const { return m_ipax; }
    int getcirclepay() const { return m_ipay; }
    int getgap() const { return m_igap; }
    int getlinegap() const { return m_iSelectPointGap; }
    int minedgerunwidth() const { return m_min_edge_run_width_px; }
    int getmethod() const { return m_iMethod; }
    int getthreshold() const { return m_iThreshold; }
    int getannulusinner();
    int getannulusouter();
    int getannuluswidth();
    int hasannulus();
    int getdebugprefilterused() const { return m_last_prefilter_used; }
    int getdebugcompactpathused() const { return m_last_compact_path_used; }
    int getfindsetting() const { return m_ifindset; }
    int getfilterborw() const { return m_ifilterborw; }
    int getfiltermin() const { return static_cast<int>(m_ifiltermin); }
    int getfiltermax() const { return static_cast<int>(m_ifiltermax); }
    const FindCircleMeasureGeometryDebug& lastmeasuregeometrydebug() const
    {
        return m_lastMeasureGeometryDebug;
    }
    const FindCircleMeasureGeometryRequest& measuregeometryrequest() const
    {
        return m_measure_geometry_request;
    }
    const FindCircleMeasureGeometryRequest& lastmeasureinputrequest() const
    {
        return m_last_measure_input_request;
    }
    FindCircleBoundaryAnalysisSnapshot boundaryanalysissnapshot() const;
    GeomAdaptor_Curve GetCurve(gp_Pnt center_p, Standard_Real radius);

    gp_Pnt FindClosestPointOnCurve(GeomAdaptor_Curve myCurve,gp_Pnt externalPoint);

    void setmaxelapsedms(int value);
    void setmaxscanlines(int value);
    void setmaxsamples(int value);

    bool budgetexceeded() const;
    int getelapsedms() const;
    int getscandiagnosticcount() const;
    bool getscandiagnostic(int index,
                           FindCircleMeasureGeometryDebug::ScanDiagnostic& out) const;
    bool getscandiagnosticline(int scan_index,
                               CxShapePoint& p0,
                               CxShapePoint& p1) const;
    int getscanlinecount() const;
    bool getscanline(int scan_index, CxShapePoint& p0, CxShapePoint& p1) const;
    int getsamplecount() const;
    const std::string& getfailurestage() const;

    double get_result() const { return hasfitresult() ? 1.0 : 0.0; }
    double get_result_script() { return get_result(); }
    double hasfitresult_script() { return hasfitresult() ? 1.0 : 0.0; }
private:

    double m_dresultcentx;
    double m_dresultcenty;
    double m_dradius;

    double m_avgdist;

    int m_icentx;
    int m_icenty;
    int m_ipax;
    int m_ipay;
    int m_idisgap;

    int m_icomparegap;
    PointsShape m_modelpoints;    //red(white 1) gap blue(black 0) model

    PointsShape m_measurepoints;

    PointsShape m_measurepoints_;

    Shape m_resultcircle;

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

    int m_iselectedgenum;
    int m_ineedfixs;

    int m_scan_arc_start_degrees = 0;
    int m_scan_arc_end_degrees = 360;
    bool m_has_scan_arc_window = false;
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
    int m_last_prefilter_used;
    int m_last_compact_path_used;
    int m_fitfilter_input_count = 0;
    int m_fitfilter_kept_count = 0;
    int m_fitfilter_rejected_count = 0;
    double m_fitfilter_sigma = 0.0;
    double m_fitfilter_threshold = 0.0;

    bool m_point_consistency_enabled = false;
    double m_point_consistency_range = 0.0;
    int m_point_consistency_input_count = 0;
    int m_point_consistency_output_count = 0;
    int m_point_consistency_removed_count = 0;

    FindCircleMeasureGeometryRequest m_measure_geometry_request;
    FindCircleMeasureGeometryRequest m_last_measure_input_request;
    FindCircleMeasureGeometryDebug m_lastMeasureGeometryDebug;
    FindCircleBoundaryAnalysisSnapshot m_boundaryAnalysis;

    bool m_measure_geometry_dirty = true;
    bool m_measure_geometry_ready = false;

    std::uint64_t m_measure_geometry_version = 0;
    std::uint64_t m_measure_geometry_built_version = 0;

    CxAlgorithmBudget m_budget;
    CxAlgorithmBudgetState m_budget_state;

    void MarkCircleMeasureGeometryDirty();

    void UpdateCircleMeasureGeometryRequest(bool hasInnerGap);

    bool EnsureCircleMeasureGeometryReady();

    bool BuildCircleMeasureGeometryFromRequest(
        const FindCircleMeasureGeometryRequest& request);

    void BuildCircleMeasureGeometryCore(
        const FindCircleMeasureGeometryRequest& request);

    void ClearMeasureState();

    void BuildBoundaryAnalysisSnapshot(Image& image);

    void CollectCircleEdgeBandsRobust(Image& image);
    void BuildCircleFeatureGraph();
    void FindCircleComponentsInGraph();
    void SelectBestCircleSequence();
    void ConvertCircleSequenceToMeasurePoints(int sequence_index);

    std::vector<CircleFitCandidateSequence> m_circle_fit_candidate_sequences;
    int m_circle_best_sequence_index = -1;
    std::vector<CircleEdgeBandCandidate> m_circle_edge_band_candidates;
    CircleFeatureGraph m_circle_feature_graph;
public:
    void easycluster(int igapx = 10, int igapy = 10, int iclusternum = 5);
    gp_Rectangle measurepointsboundingrect() { return m_measurepointsboundingRect; }

};


#endif //_findline_Header
