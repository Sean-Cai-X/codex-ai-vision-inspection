#ifndef FASTMATCH_H
#define FASTMATCH_H
#include <array>
#include <chrono>
#include <map>
#include <string>

#include <vector>
#include "shape.h"
#include "Image.h"
#include "shapebase.h"
#include "FindLine.h"
#include "FastMatchTransform.h"
#include "CxCalibration.h"
#include "CxFastMatchShapeModel.h"
#include "FormfitGauge.h"
#include "grid.h"
#include <opencv2/core/mat.hpp>

class Grid;
class ICxShapeSink;
typedef vector<int> Cluster;
using namespace std;

class FindObject;
class FindSegmentation;

struct FastMatchTemplateGeometrySnapshot
{
    bool available = false;
    int source_object_index = -1;
    std::string source_object_ref;
    cv::Rect2d bbox_px;
    cv::Point2d centroid_px;
    double projected_area = 0.0;
    double major_axis_length = 0.0;
    double minor_axis_length = 0.0;
    double orientation_deg = 0.0;
    double aspect_ratio = 0.0;
    double solidity = 0.0;
    std::vector<cv::Point2d> normalized_boundary;
    std::string status = "unavailable";
};

struct FastMatchPoseCandidateSnapshot
{
    int candidate_index = -1;
    int observed_geometry_index = -1;
    std::string observed_geometry_ref;
    cv::Rect2d bbox_px;
    cv::Point2d center_px;
    double angle_deg = 0.0;
    double scale_x = 1.0;
    double scale_y = 1.0;
    double appearance_score = 0.0;
    double geometry_score = -1.0;
    double combined_score = 0.0;
    std::vector<cv::Point2d> transformed_boundary;
    std::string status = "unavailable";
};


// FastMatch Learn Probe is intentionally disabled in normal builds.
// Headless/evidence runs must never show modal UI or block on debug probes.
// #define FASTMATCH_LEARN_PROBE

// fastmatch extends Findline with grid/model learning and match result helpers.
class FastMatch :public FindLine
{
public:
    struct LearnDirectionParams
    {
        int wgap = -1;
        int hgap = -1;
        int method = -1;
        int threshold = -1;
        int linegap = -1;
        int objfilter = -1;
        int compare_gap = -1;
        int edge_count = 2;
        int selected_edge = 0; // 0=all, -1=last, positive=Nth candidate.
    };

    // A directional probe is a real, isolated FindLine measurement used to
    // diagnose one physical side of the Learn ROI.  It deliberately keeps
    // evidence separate from the legacy FastMatch edgepattern template so a
    // four-tab review can never pretend that one global scan was four runs.
    struct DirectionalProbeScanLine
    {
        CxShapePoint p0;
        CxShapePoint p1;
    };

    struct DirectionalProbeEvidence
    {
        int direction = 0; // 0=Top, 1=Bottom, 2=Left, 3=Right.
        LearnDirectionParams params;
        int scan_type = 0; // FindLine W=0 for Top/Bottom, H=1 for Left/Right.
        // FindLine ordinal after translating FastMatch's physical-side
        // Point Column semantics (Bottom/Right count from the reverse end).
        int runtime_selected_edge = 0;
        int scan_line_count = 0;
        int raw_result_count = 0;
        int accepted_side_count = 0;
        int diagnostic_count = 0;
        bool executed = false;
        std::string status = "NOT_RUN";
        std::string reason;
        std::vector<DirectionalProbeScanLine> scan_lines;
        // The single selected point remains the waveform/conclusion anchor.
        // Full edge can retain multiple physical candidates per Gauge Line,
        // so template composition reads accepted_points_by_scan instead.
        std::vector<CxShapePoint> selected_point_by_scan;
        std::vector<unsigned char> selected_point_valid_by_scan;
        std::vector<std::vector<CxShapePoint>> accepted_points_by_scan;
        std::vector<CxShapePoint> accepted_points;
    };

    struct NormalTraceLearnConfig
    {
        bool enabled = false;
        int domain_overlap_radius_px = 3;
        int normal_angle_tolerance_deg = 20;
        // Legacy CxScript compatibility only. The conclusion graph performs
        // no image-gradient extraction and does not consume this value.
        int min_gradient = 20;
        int dijkstra_max_nodes = 4096;
        // Legacy CxScript compatibility only; ignored by pure conclusion-path
        // Dijkstra.
        int dijkstra_gradient_cost_weight_permille = 700;
        int dijkstra_turn_cost_weight_permille = 200;
        int dijkstra_gap_cost_weight_permille = 100;
        int dijkstra_max_trace_gap_px = 3;
        // Forward-only KNN graph degree after local XY compression. This
        // reconnects a physical edge when one or more compression bins have
        // no retained point, without allowing a trace to reverse direction.
        int dijkstra_knn_neighbors = 6;
        int ann_search_radius_px = 32;
        int ann_tangent_deviation_deg = 35;
        int ann_normal_deviation_deg = 35;
        int ann_min_component_points = 4;
        int ann_min_component_coverage_percent = 55;
        int trace_min_length_px = 20;
        // Distance from the retained conclusion point to each polarity point.
        // Learn synchronizes this from FastMatch compare_gap, so the complete
        // A-to-B distance is always 2 * compare_gap in every learn path.
        int normal_pair_offset_px = 6;
        // Legacy CxScript compatibility only. Final polarity always inherits
        // the corresponding directional FindLine conclusion domain.
        int normal_polarity = 0;
        int corner_rejection_radius_px = 4;
        int tangent_sample_step_px = 2;
        int anchor_neighborhood_radius_px = 24;
        int xy_compression_bin_px = 4;
        int min_keypoints_per_domain = 4;
        // Integer percentages keep CxScript controls and evidence deterministic.
        int endpoint_spike_ratio_percent = 250;
        int junction_tangent_window_points = 6;
        int junction_min_cross_angle_deg = 12;
        int junction_max_extrapolation_percent = 150;
        int junction_join_spacing_multiplier_percent = 800;
    };

    struct NormalTraceEvidence
    {
        bool executed = false;
        bool succeeded = false;
        std::string reason = "NOT_RUN";
        int directional_side_count = 0;
        int trace_segment_count = 0;
        double closure_error_px = -1.0;
        double max_consecutive_gap_px = -1.0;
        double gradient_coverage = 0.0;
        // Pure conclusion-graph semantics. gradient_coverage remains as a
        // legacy projection field and mirrors selected_anchor_coverage.
        double selected_anchor_coverage = 0.0;
        int generated_conclusion_count = 0;
        int normal_pair_findline_bound_count = 0;
        int normal_pair_binding_miss_count = 0;
        int normal_pair_corner_rejected_count = 0;
        int loop_erased_point_count = 0;
        std::array<int, 4> normal_pair_counts_by_direction{};
        // Candidates must first bind to the nearest selected FindLine
        // conclusion and pass its local slope/scan-normal constraints. ANN and
        // Dijkstra only consume the accepted set; these counts make that
        // ordering auditable in manual Evidence review.
        double anchor_normal_band_px = 0.0;
        std::array<int, 4> anchor_near_candidate_counts{};
        std::array<int, 4> anchor_band_rejected_counts{};
        std::array<int, 4> anchor_slope_rejected_counts{};
        std::array<int, 4> anchor_normal_rejected_counts{};
        std::array<int, 4> anchor_prefilter_accepted_counts{};
        std::array<int, 4> domain_deduplicated_counts{};
        std::array<int, 4> compressed_keypoint_counts{};
        // ANN is the neighbourhood re-clustering stage.  These facts prove
        // that Dijkstra consumed the selected ANN component rather than the
        // unclassified compressed point cloud.
        std::array<int, 4> ann_edge_counts{};
        std::array<int, 4> ann_component_counts{};
        std::array<int, 4> ann_selected_point_counts{};
        std::array<double, 4> ann_selected_coverage{};
        std::array<double, 4> domain_join_allowed_gap_px{};
        std::array<double, 4> domain_join_selected_gap_px{};
        std::array<int, 4> endpoint_spike_pruned_counts{};
        int endpoint_spike_ratio_percent = 0;
        int junction_tangent_window_points = 0;
        int junction_min_cross_angle_deg = 0;
        int junction_max_extrapolation_percent = 0;
        int junction_join_spacing_multiplier_percent = 0;
        // Junction modes: 1 = constrained tangent intersection,
        // 2 = constrained tangent blend fallback. Junctions are derived
        // geometry and never become FindLine/Dijkstra conclusions.
        std::array<int, 4> derived_junction_modes{};
        std::array<double, 4> derived_junction_cross_angle_deg{};
        std::array<double, 4> derived_junction_current_extrapolation_px{};
        std::array<double, 4> derived_junction_next_extrapolation_px{};
        std::vector<CxShapePoint> derived_junction_points;
        // Retained after domain overlap de-duplication; raw count is exposed
        // separately so the display stays bounded.
        // Raw directional FindLine anchors are retained even when every point
        // is rejected by the slope/domain prefilter.  A failed Evidence case
        // must show the last valid measurement facts instead of an empty ROI.
        std::array<std::vector<CxShapePoint>, 4> anchor_points_by_direction;
        std::vector<CxShapePoint> domain_points;
        std::array<std::vector<CxShapePoint>, 4> domain_points_by_direction;
        std::array<std::vector<CxShapePoint>, 4> compressed_points_by_direction;
        std::array<std::vector<CxShapePoint>, 4> ann_selected_points_by_direction;
        // Dijkstra points are original selected FindLine conclusions. The
        // derived trace is a render/model polyline produced by interpolation;
        // it is never allowed to become new measurement evidence.
        std::vector<CxShapePoint> dijkstra_trace_points;
        std::vector<int> dijkstra_source_directions;
        std::vector<int> dijkstra_source_scans;
        std::vector<CxShapePoint> derived_trace_points;
        std::vector<CxShapePoint> normal_pair_a;
        std::vector<CxShapePoint> normal_pair_b;
        std::vector<CxShapePoint> normal_pair_source_points;
        std::vector<int> normal_pair_source_directions;
        std::vector<int> normal_pair_source_scans;
    };

    FastMatch();
    ~FastMatch();

    void setshow(int ishow);

    virtual void setrect(int ix, int iy, int iw, int ih);
    virtual void drawshape();

    void drawshapex(double dmovx, double dmovy,
        double dangle, double dzoomx, double dzoomy);
    void getshape(void* pshape);

    void SetWHgap(int wgap = 2, int hgap = 2);
    void measure(void* pimage);
    void setlinesamplerate(double dsamplerate);
    void setlinegap(int igap);
    void setmethod(int imethod);
    void setthre(int ithre);
    void setgamarate(int igama);

    void setscanrotation(double angle_degrees);
    double getscanrotation() const;
    void setobjfilter(int ifindset);
    void setlearnwgap(int direction, int value);
    void setlearnhgap(int direction, int value);
    void setlearnmethod(int direction, int value);
    void setlearnthre(int direction, int value);
    void setlearnlinegap(int direction, int value);
    void setlearnobjfilter(int direction, int value);
    void setlearncompgap(int direction, int value);
    void setlearnedgecount(int direction, int value);
    void setlearnselectededge(int direction, int value);
    void setlearnwgap_script(int value, int direction) { setlearnwgap(direction, value); }
    void setlearnhgap_script(int value, int direction) { setlearnhgap(direction, value); }
    void setlearnmethod_script(int value, int direction) { setlearnmethod(direction, value); }
    void setlearnthre_script(int value, int direction) { setlearnthre(direction, value); }
    void setlearnlinegap_script(int value, int direction) { setlearnlinegap(direction, value); }
    void setlearnobjfilter_script(int value, int direction) { setlearnobjfilter(direction, value); }
    void setlearncompgap_script(int value, int direction) { setlearncompgap(direction, value); }
    void setlearnedgecount_script(int value, int direction) { setlearnedgecount(direction, value); }
    void setlearnselectededge_script(int value, int direction) { setlearnselectededge(direction, value); }
    LearnDirectionParams effectiveLearnDirectionParams(int direction);
    bool hasExplicitLearnDirectionParams() const;
    const DirectionalProbeEvidence& getdirectionalprobeevidence(int direction) const;
    int getdirectionalprobeacceptedcount(int direction) const;
    int getdirectionalprobescanlinecount(int direction) const;
    int getdirectionalprobediagnosticcount(int direction) const;
    int getdirectionalprobestatuscode(int direction) const;
    void setnormaltraceenabled(int enabled);
    void setnormaltraceparams(int overlap_radius_px, int min_gradient,
                              int max_nodes, int pair_offset_px);
    void setnormaltracecosts(int gradient_weight_permille,
                             int turn_weight_permille,
                             int gap_weight_permille,
                             int max_trace_gap_px);
    void setnormaltraceknn(int forward_neighbor_count);
    void setnormaltraceann(int search_radius_px, int tangent_deviation_deg,
                           int normal_deviation_deg,
                           int min_component_points,
                           int min_component_coverage_percent);
    void setnormaltracegeometry(int normal_angle_tolerance_deg,
                                int trace_min_length_px,
                                int normal_polarity,
                                int corner_rejection_radius_px,
                                int tangent_sample_step_px);
    void setnormaltracedomain(int anchor_neighborhood_radius_px,
                              int xy_compression_bin_px,
                              int min_keypoints_per_domain);
    void setnormaltracejunction(int endpoint_spike_ratio_percent,
                                int tangent_window_points,
                                int minimum_cross_angle_deg,
                                int maximum_extrapolation_percent,
                                int join_spacing_multiplier_percent);
    const NormalTraceLearnConfig& getnormaltraceconfig() const
    { return m_normal_trace_config; }
    int getnormaltracecandidatecount() { return m_normal_trace_candidate_count; }
    int getnormaltracededuplicatedcount() { return m_normal_trace_deduplicated_count; }
    int getnormaltracepointcount() { return m_normal_trace_point_count; }
    int getnormaltracepaircount() { return m_normal_trace_pair_count; }
    const NormalTraceEvidence& getnormaltraceevidence() const
    { return m_normal_trace_evidence; }
    // P0-P6 dense form-fit controls.  FastMatch owns both shape models and the
    // correspondence solve; FormfitGauge is a read-only value bridge.
    void setformfitenabled(int enabled);
    void setformfitdenseparams(int dense_step_milli_px,
                               int profile_half_width_milli_px,
                               int profile_step_milli_px,
                               int minimum_gradient,
                               int curvature_threshold_millideg);
    void setformfitannparams(int search_radius_milli_px,
                             int normal_tolerance_deg,
                             int trim_percent,
                             int minimum_mutual_pairs,
                             int maximum_iterations);
    void setformfitbudget(int maximum_elapsed_ms,
                          int maximum_structural_anchors,
                          int allow_nonuniform_affine);
    const CxFastMatchFormFitConfig& getformfitconfig() const
    { return m_formfit_config; }
    const CxFastMatchShapeModel& getreferenceshapemodel() const
    { return m_reference_shape_model; }
    const CxFastMatchShapeModel& getobservedshapemodel() const
    { return m_observed_shape_model; }
    const CxFastMatchFormFitResult& getformfitresult() const
    { return m_formfit_result; }
    const cxcore::formfit::FormfitGauge& getformfitgauge() const
    { return m_formfit_gauge; }
    int getformfitstatuscode();
    int getformfitreferencecount();
    int getformfitobservedcount();
    int getformfitmutualcount();
    int getformfitanchorcount();
    double getformfitscore();
    double getformfitresidual();
    // Model points are normalized for matching.  These values retain the
    // original-image translation needed by the Image View debug projection.
    double getlearnmodeloriginx() { return m_learn_model_origin_x; }
    double getlearnmodeloriginy() { return m_learn_model_origin_y; }

    void setgeometrysourceindex(int index);
    void setgeometryweightpercent(int percent);
    void setmaxposecandidates(int count);
    void settemplategeometryfromobject(void* pfindobject);
    void cleargeometrycandidates();
    void addgeometrycandidatesfromobject(void* pfindobject);
    int gettemplategeometryavailable();
    int gettemplategeometrysourceindex();
    int gettemplateboundarypointcount();
    double gettemplatearea();
    double gettemplateorientation();
    int getposecandidatecount();
    double getposecandidatex(int index);
    double getposecandidatey(int index);
    double getposecandidateangle(int index);
    double getposecandidateappearancescore(int index);
    double getposecandidategeometryscore(int index);
    double getposecandidatecombinedscore(int index);
    const FastMatchTemplateGeometrySnapshot& gettemplategeometry() const
    {
        return m_template_geometry;
    }
    const std::vector<FastMatchPoseCandidateSnapshot>& getposecandidates() const
    {
        return m_pose_candidates;
    }

    void setfilter(int ifilterborw, int ifiltermin, int ifiltermax);//21 w ,22 b
    void setselectedgenum(int iedgenum);

    void learn_level0(void* pimage);//5pyrDown   thre >50  linegap 3
    void learn_level0_1(void* pimage);//5pyrDown   thre >50  linegap 7
    void learn_level1(void* pimage);//5pyrDown   thre >30  linegap 7
    void learn_level2(void* pimage);//3pyrDown   thre >30
    void learn_level3(void* pimage);//1pyrDown   thre >10
    void learn_level4(void* pimage);//thre >7

    void learn(void* pimage);
    void setcomparegap(int igap);
    void savemodelfile(const char* pchar);
    void loadmodelfile(const char* pchar);

    void ABtoShape(std::vector<cv::Point2f>& points);

    std::vector<cv::Point2f> getmodel();
    int getmodelpointcount();
    int getlearnacount();
    int getlearnbcount();
    int getlearna2count();
    int getlearnb2count();
    int getlearnstatuscode() { return m_fastmatch_learn_status_code; }
    std::uintptr_t debuglastlearnargument() const noexcept
    {
        return reinterpret_cast<std::uintptr_t>(m_debug_last_learn_argument);
    }
    int getmodelwidth() const { return m_imodelwith; }
    int getmodelheight() const { return m_imodelheigh; }
    int getpatternapointcount() const;
    int getpatternbpointcount() const;
    double getpatternax() const;
    double getpatternay() const;
    double getpatternawidth() const;
    double getpatternaheight() const;
    double getpatternbx() const;
    double getpatternby() const;
    double getpatternbwidth() const;
    double getpatternbheight() const;

    int ABpatternsize();
    void loadrotatemodelfile(const char* pchar);
    void loadrotate05modelfile(const char* pchar);
    void loadrotate025modelfile(const char* pchar);

    void loadcalibration(const char* pchar);
    void savecalibration(const char* pchar);

    void setrotateangle(double danglel1);
    void setrotateanglescale(double dangle1, double dangle2);

    void patternrootgrid(double itype, double drate, double ilevel);

    void patternzoom(double dx, double dy, double igap, double itype);

    void patterntranform(int igap, int itype, int isgap, int iline);

    void patterngap2gap(int inewgap);
    void patternABgap2gap(double dnewgaprate);
    void patternABsample(int irate);
    void pattern2org();
    void org2pattern();

    void modelrotate(double dangle);
    void modelzoom(double dx, double dy);
    void setmodelwh(int iw, int ih);
    void modelzeroposition();
    void rotatemodelzeroposition();
    void rotatemodelzeropositionAB();
    void rotatemodel05zeroposition();
    void rotatemodel025zeroposition();

    void Distfilter();

    void samplemodelAB(int inum); 

    void MatchAB(Image& image);
    void match(void* pimage);

    // P0 affine search: a value-only initial transform (usually OBB-derived)
    // is evaluated sparsely, then refined with all learned A/B probes.
    void settransformsearchenabled(int enabled);
    void settransformcenter(int cx, int cy);
    void settransformextent(int half_u, int half_v);
    void settransformangle(double angle_deg);
    void settransformscalepermille(int scale_x_permille, int scale_y_permille);
    void settransformshearpermille(int shear_permille);
    void settransformprojectivepermille(int projective_u_permille, int projective_v_permille);
    // Copies the primary OBB/geometry result from the immediately preceding
    // FindSegmentation runtime object.  No parser object escapes its runtime.
    void settransformfromsegmentation(void* segmentation);
    // Copies the highest-confidence explicit OBB from a TorchTask result.
    // Plain axis-aligned detections are intentionally rejected.
    void settransformfromtorch(void* torch_task);
    // Business-layer binding: copy a frozen calibration receipt.  The caller
    // retains ownership; FastMatch never keeps a calibration object pointer.
    bool bindcalibrationsnapshot(const CxCalibrationSnapshot& snapshot);
    void clearcalibrationsnapshot();
    // Key-parameter test controls.  These build a value-only calibration
    // receipt inside FastMatch and deliberately identify it as a manual test
    // override; production code must bind a frozen CxCalibrationSnapshot.
    void setcalibrationtestenabled(int enabled);
    void setcalibrationtestxytransform(double scale_x, double scale_y,
                                       double offset_x, double offset_y,
                                       double rotation_deg,
                                       double shear_x, double shear_y);
    // CxScript's native seven-int method path; scale/shear are ppm, offsets
    // are milli-units and rotation is milli-degrees.
    void setcalibrationtestxytransformscaled(int scale_x_ppm, int scale_y_ppm,
                                             int offset_x_milliunit,
                                             int offset_y_milliunit,
                                             int rotation_millideg,
                                             int shear_x_ppm,
                                             int shear_y_ppm);
    void setcalibrationtestreprojectionrmse(double reprojection_rmse_px);
    void settransformscalerangepercent(int percent);
    void settransformanglerange(int degrees);
    void settransformcoarsesteps(int steps);
    void settransformfinerangepercent(int percent);
    void settransformfineanglerange(int degrees);
    void settransformmaxcandidates(int count);
    void settransformmaxsamples(int count);
    void settransformmaxelapsedms(int milliseconds);
    void settransformshearrangepermille(int permille);
    void settransformprojectiverangepermille(int permille);
    void transformmatch(void* pimage);
    int gettransformsearchexecuted();
    int gettransformsearchconverged();
    int gettransformsearchbudgetexceeded();
    int gettransformsearchcandidatecount();
    int gettransformsearchsamplecount();
    int gettransformsearchelapsedms();
    double gettransformsearchscore();
    double gettransformsearchscalex();
    double gettransformsearchscaley();
    double gettransformsearchangle();
    double gettransformsearchshear();
    double gettransformsearchprojectiveu();
    double gettransformsearchprojectivev();
    int gettransformsearchcalibrationapplied();
    double gettransformsearchcalibrationreprojectionrmse();
    double gettransformsearchphysicalcx();
    double gettransformsearchphysicalcy();
    double gettransformsearchgradientscore();
    double gettransformsearchresidual();
    double gettransformsearchrigidbaselinescore();
    void setrotatemaxcandidates(int count);
    void setrotatemaxelapsedms(int milliseconds);
    void setrotatemaxprobes(int count);
    void setrotatemaxsamples(int count);
    int getrotatebudgetexceeded();
    int getrotatecandidatecount();
    int getrotateprobecount();
    int getrotatesamplecount();
    int getrotateelapsedms();
    const FastMatchTransformSearchResult& gettransformsearchresult() const
    { return m_transform_search_result; }

    void MatchABMore(Image& image);
    void matchmore(void* pimage);
 

    void MultiMatch(Image& image);
    void multimatch(void* pimage);



    void rotatematch(void* pimage);
    void rotatematchAB(void* pimage);
    void rotatematchAB_upgrade(void* pimage);
    void rotatematchAB05_upgrade(void* pimage);
    void rotatematchAB025_upgrade(void* pimage);

    void setupgradenum(int iresultnum);

    void setclustergap(int ixclustergap, int iyclustergap, int iangleclustergap);

    void savematchroi(const char* pfilename);

    void imagelearn(int ithre1, int iandor);
    void imagelearnex(int ithre1, int iandor, int igrid);
    void imagelearnmass(int ithre1, int iandor, int igridwh);
    void imagelearncheck(int iimagetype, int iandor, int igridwh);

    void imagematch(int ithre1, int iandor, int igrid = 12, int ineedthre = 1);
    void imagematchex(int igrid);
    void savematchimagemodel(const char* pfilename);

    void loadfastimagemodel(const char* pfilename);
    void savefastimagemodel(const char* pfilename);
    void savefastimagepatmodel(const char* pfilename);

    void SaveMatchROI(Image& image, const char* pfilename);
    void MatchImageLearn(Image& aimage, int ithre1, int iandor);
    void MatchImageLearnEx(Image& aimage, int ithre1, int iandor, int igrid);
    void MatchImageLearnMass(Image& aimage, int ithre1, int iandor, int igrid);
    void MatchImageCheck(Image& aimage, int iimagetype, int iandor, int igrid);

    void imagematch_grid(int ithre1, int iandor, int igrid);

    void MatchImageMatch(Image& aimage, int ithre1, int iandor, int igrid = 12, int ineedthre = 1);
    void MatchImageExMatch(Image& aimage, int igrid = 12);
    void MatchGrid(Grid* pgrid);

    void setmatchrectnum(int inum);
    void setmatchrect(int ix, int iy, int iw, int ih);
    void setrectxywh(int ix, int iy, int iw, int ih);
    void setmatchrectxywh(int ix, int iy, int iw, int ih);
    void setrectxywh_script(int ih, int iw, int iy, int ix);
    void setmatchrectxywh_script(int ih, int iw, int iy, int ix);
    void setexpectedrect(double x0, double y0, double x1, double y1);

    void setmultimatchrect(int inum, int ix, int iy, int iw, int ih);
    void setmatchthre(int ithre);
    void setfindnum(int ifindnum);
    void setmatchmask(const cv::Mat* pmask);
    void clearmatchmask();
    int getrawmatchprobecount() const;
    int getrawmatchthresholdhitcount() const;
    int getmatchcallcount() const;
    int getmatchabcallcount() const;
    int getmatchsampleabcallcount() const;
    int getmatchlaststage() const;
    int getmatchimagewidth() const;
    int getmatchimageheight() const;
    int getlearnrectx0() const;
    int getlearnrecty0() const;
    int getlearnrectx1() const;
    int getlearnrecty1() const;
    int getmatchrectx0() const;
    int getmatchrecty0() const;
    int getmatchrectx1() const;
    int getmatchrecty1() const;
    int getresulttolistcallcount() const;
    int getresultcandidateinsertcount() const;
    int getresultcandidatereplacecount() const;
    int getresultcandidaterejectcount() const;
    int getresultcandidatecount();
    int getresultbestindex();
    double getresultbestscore();
    int getrawthresholdhitrecordcount() const;
    gp_Pnt getrawthresholdhitpoint(int inum) const;
    int getrawthresholdhitscore(int inum) const;
    double getresultnum(int inum);
    double getresultcentx(int inum);
    double getresultcenty(int inum);
    double getresolvedresultcentx(int inum);
    double getresolvedresultcenty(int inum);
    int getrotateresultcentx(int inum);
    int getrotateresultcenty(int inum);


    void setminscore(double dscore);

    double getmaxresult();
    double getimagemodelreslut();
    double getimagemodelreslut_check_1();

    int getmodeleasyobjectw_l72(int inum);
    int getmodeleasyobjectb_l72(int inum);

    int getmodeleasyobjectw_l36(int inum);
    int getmodeleasyobjectb_l36(int inum);

    int getmodeleasyobjectw_l12(int inum);
    int getmodeleasyobjectb_l12(int inum);

    int getmodeleasyobjectw_l6(int inum);
    int getmodeleasyobjectb_l6(int inum);

    int getmodeleasyobjectw_l3(int inum);
    int getmodeleasyobjectb_l3(int inum);

    int geteasyobjectb();
    int geteasyobjectw();

    void imagemodelcompareshow(int itype);
    void imagemodelcomparegrid(int itype);

    double imagegridresult(int itype);

    void imagemodelshow();
    void imagematchshow();

    void clearmodels_l12();
    void addmodels_l12(const char* pchar);

    void clearmodels_l36();
    void addmodels_l36(const char* pchar);

    void clearmodels_l72();
    void addmodels_l72(const char* pchar);

    void modelstocurrent_l72(int i);
    void modelstocurrent_l36(int i);
    void modelstocurrent_l12(int i);
    void modelstocurrent_l3(int i);
    void modelstocurrent_l6(int i);

    void imagemodesclear_l12();
    void addimagemodels_l12(const char* pchar);

    void imagemodesclear_l36();
    void addimagemodels_l36(const char* pchar);

    void imagemodesclear_l72();
    void addimagemodels_l72(const char* pchar);

    void clearmodels_rotate();
    void addmodels_rotate(const char* pchar);

    int GetRectGridLevel(int irectw);
    vector<int>* getcurimagemodel();
    bool modelcompare(vector<int>& modela, vector<int>& modelb);

    void clearmodel();
    void list_duplicatesmodel_l12();
    void list_duplicatesmodel_l36();
    void list_duplicatesmodel_l72();

    void levelmodels_l72tol36();
    void levelmodels_l36tol12();
    void levelmodels_l12tol6();
    void levelmodels_l6tol3();

    int imagefastmodelsize(int ilevel);
    void imagemodelstocurrent_l72(int i);
    void imagemodelstocurrent_l36(int i);
    void imagemodelstocurrent_l12(int i);
    void imagemodelstocurrent_l3(int i);
    void imagemodelstocurrent_l6(int i);

    void objectmodelstocurrent(int i);

    void setcurmodels(int inum);
    void setcurimagemodels(int inum);

    void setspecshow(int ishow);
    void setb2w(int ib2w);
    void modelmethod(int itype);
    RectsShape& getmatchrects();
    gp_Rectangle& getmatchrect();
    gp_Rectangle getresultrect(int inum) const;
    gp_Rectangle getresolvedresultrect(int inum) const;
    vector<PointsShape>& getmodels_l12();
    Grid* getgrid();
    void setgrid(int iw, int igrid);
    map<int, int >& getlevel3_6map();
    map<int, int >& getlevel6_12map();
    map<int, int >& getlevel12_36map();
    map<int, int >& getlevel36_72map();

    vector<int>& getduplicateslist_l72();
    vector<int>& getduplicateslist_l36();
    vector<int>& getduplicateslist_l12();
    void savelevel0_l1();
private:
    LearnDirectionParams& learnDirectionParams(int direction);
    void runDirectionalFindLineProbes(Image& image);
    void buildReferenceFormFitModel(Image& image);
    void runFormFitOnImage(Image& image, const FastMatchTransform& seed,
                           const char* observed_model_id);
    void refreshFormFitGauge();

    int m_istyle;
    Image* g_pmodelimage;
    Image* m_matchimage;
    const cv::Mat* m_matchmask;


    vector<int> m_imagefastmodel;

    vector<PointsShape> m_models_l72;

    vector<PointsShape> m_models_l36;

    vector<PointsShape> m_models_l12;

    vector<PointsShape> m_models_l3;

    vector<PointsShape> m_models_l6;

    typedef vector<int> IMAGEFASTMODEL;
    vector<IMAGEFASTMODEL> m_imagefastmodels_l72;

    vector<IMAGEFASTMODEL> m_imagefastmodels_l36;

    vector<IMAGEFASTMODEL> m_imagefastmodels_l12;

    vector<IMAGEFASTMODEL> m_imagefastmodels_l6;

    vector<IMAGEFASTMODEL> m_imagefastmodels_l3;


    vector<int>m_duplicates_list_l72;
    vector<int>m_duplicates_list_l36;
    vector<int>m_duplicates_list_l12;

    // QVector<IMAGEFASTMODEL> m_imagefastmodels_l12_l2;

    map<int, int > m_mapl3_l6;
    map<int, int > m_mapl6_l12;
    map<int, int > m_mapl12_l36;
    map<int, int > m_mapl36_l72;

    vector<easyobj> m_easyobjectmodels_l72;

    vector<easyobj> m_easyobjectmodels_l36;

    vector<easyobj> m_easyobjectmodels_l12;

    vector<easyobj> m_easyobjectmodels_l6;

    vector<easyobj> m_easyobjectmodels_l3;

    vector<int> m_imagefastmatchlist;

    int m_imodelwith;
    int m_imodelheigh;

    easyobj m_cureasyobject;

    easyobj m_easyobject;

    int m_imagemodelresult_NG;
    int m_imagemodelresult_OK;

    int m_easyobjectw_ng;

    int m_easyobjectb_ng;

    PointsShape m_modelpoints_sample1;//  /2
    PointsShape m_modelpoints_sample2;//  /4
    PointsShape m_modelpoints_sample3;//  /8

    int m_imaxmatchnum;
    int m_imatchthre;
    int m_iB2W;
    int m_imatchoffset;

    RectsShape m_matchrects;
    gp_Rectangle m_matchrect;
    gp_Rectangle m_expected_rect;
    int m_geometry_source_object_index = 0;
    int m_geometry_weight_percent = 25;
    int m_max_pose_candidates = 32;
    FastMatchTemplateGeometrySnapshot m_template_geometry;
    std::vector<FastMatchTemplateGeometrySnapshot> m_observed_geometries;
    std::vector<FastMatchPoseCandidateSnapshot> m_pose_candidates;

    int m_learn_roi_x = 0;
    int m_learn_roi_y = 0;
    int m_learn_roi_w = 0;
    int m_learn_roi_h = 0;
    int m_search_roi_x = 0;
    int m_search_roi_y = 0;
    int m_search_roi_w = 0;
    int m_search_roi_h = 0;

    int m_fastmatch_learn_a_count = 0;
    int m_fastmatch_learn_b_count = 0;
    int m_fastmatch_learn_a2_count = 0;
    int m_fastmatch_learn_b2_count = 0;
    int m_fastmatch_learn_status_code = 0;
    std::array<LearnDirectionParams, 4> m_learn_direction_params;
    std::array<DirectionalProbeEvidence, 4> m_directional_probe_evidence;
    NormalTraceLearnConfig m_normal_trace_config;
    int m_normal_trace_candidate_count = 0;
    int m_normal_trace_deduplicated_count = 0;
    int m_normal_trace_point_count = 0;
    int m_normal_trace_pair_count = 0;
    NormalTraceEvidence m_normal_trace_evidence;
    CxFastMatchFormFitConfig m_formfit_config;
    CxFastMatchShapeModel m_reference_shape_model;
    CxFastMatchShapeModel m_observed_shape_model;
    CxFastMatchFormFitResult m_formfit_result;
    cxcore::formfit::FormfitGauge m_formfit_gauge;
    double m_learn_model_origin_x = 0.0;
    double m_learn_model_origin_y = 0.0;

    int m_imatchrectnum;
    vector<gp_Pnt> m_resultpoints;
    vector<int> m_resultnums;

    RectsShape m_resultrects;

    int m_stepgapx;
    int m_stepgapy;

    double m_danglegap;//5

    double m_dangle_add;//10 
    double m_dangle_mud;//-10 

    vector<PointsShape> m_models_rotate;//1 degree
    vector<PointsShape> m_models_rotaterects;//4 points

    vector<PointsShape> m_models05_rotate;//1 degree
    vector<PointsShape> m_models05_rotaterects;//4 points

    vector<PointsShape> m_models025_rotate;//1 degree
    vector<PointsShape> m_models025_rotaterects;//4 points

    vector<PointsShape> m_rotateshaperesults;//4 points 
    //RectsShape m_rotateresultrects;

    vector<gp_Pnt> m_rotatereslutpoints;
    vector<double> m_rotateresults;//
    vector<double> m_rotatereslutangles;//

    int m_iupgradenum = 0;

    vector<Cluster> m_clusters;

    PointsShape m_calibration;

    int m_ixclustergap;
    int m_iyclustergap;
    int m_iangleclustergap;

    int m_iminfindnum;
    gp_Pnt m_iminpointkey;
    int m_imaxfindnum;
    gp_Pnt m_imaxpointkey;
    int m_rawmatch_probe_count;
    int m_rawmatch_threshold_hit_count;
    int m_match_call_count = 0;
    int m_matchab_call_count = 0;
    int m_matchsampleab_call_count = 0;
    int m_match_last_stage = 0;
    int m_match_debug_image_width = 0;
    int m_match_debug_image_height = 0;
    int m_match_debug_rect_x0 = 0;
    int m_match_debug_rect_y0 = 0;
    int m_match_debug_rect_x1 = 0;
    int m_match_debug_rect_y1 = 0;
    int m_resulttolist_call_count;
    int m_resultcandidate_insert_count;
    int m_resultcandidate_replace_count;
    int m_resultcandidate_reject_count;
    vector<gp_Pnt> m_rawthresholdhitpoints;
    vector<int> m_rawthresholdhitscores;

    FastMatchTransformSearchConfig m_transform_search_config;
    FastMatchTransform m_transform_search_initial;
    FastMatchTransformSearchResult m_transform_search_result;
    FastMatchTransformSeedEvidence m_transform_seed_evidence;
    CxCalibrationAdapter m_calibration_adapter;
    bool m_calibration_bound = false;
    CxCalibration m_calibration_test_override;
    bool m_calibration_test_override_enabled = false;
    int m_rotate_max_candidates = 720;
    int m_rotate_max_elapsed_ms = 1000;
    int m_rotate_max_probes = 200000;
    int m_rotate_max_samples = 2000000;
    int m_rotate_candidate_count = 0;
    int m_rotate_probe_count = 0;
    int m_rotate_sample_count = 0;
    bool m_rotate_budget_exceeded = false;
    bool m_rotate_budget_active = false;
    std::chrono::steady_clock::time_point m_rotate_budget_start;

    void* m_debug_last_learn_argument = nullptr;

    Grid* m_pgrid;//12X12

    QRootGrid* m_rootgridA;

    //  Grid *m_pgrid_l0;//3X3
    //  Grid *m_pgrid_l1;//6X6

    void Learn(Image& image);
    void RefreshPoseCandidates();
    void PublishGeometryDisplayShapes(ICxShapeSink& sink, const std::string& owner_ref) const;



    void Learn_level0(Image& image);//5pyrDown   thre >50 
    void Learn_level1(Image& image);//5pyrDown   thre >30
    void Learn_level2(Image& image);//2pyrDown   thre >30
    void Learn_level3(Image& image);//1pyrDown   thre >10
    void Learn_level4(Image& image);//thre >7
 
    void resulttolist(gp_Pnt& apoint, int inum);
    void resultclear();
    void resultsort();
    void clusterclear();
    void rotateresultsort();

    void resultcluster(int ixgap, int iygap, int ianglegap);

    void MatchSample(Image& image, gp_Path& path);
    void MatchSampleAB(Image& image, gp_Path& pathA, gp_Path& pathB);
    void MatchSampleABMore(Image& image, gp_Path& pathA, gp_Path& pathB);
    double evaluateTransformCandidate(Image& image, gp_Path& pathA,
        gp_Path& pathB, const FastMatchTransform& transform, bool sparse,
        int& hit_count, int& probe_count, double& continuity,
        double& gradient_score, double& geometric_residual_px,
        bool& budget_exceeded, const std::chrono::steady_clock::time_point& start);
    void runTransformSearch(Image& image);
    void resetRotateBudget();
    bool consumeRotateCandidate();
    bool consumeRotateProbe(int sample_cost);

    void MultiMatchSample(Image& image, gp_Path& path);
    void RotateMatch(Image& image);
    void RotateMatchAB(Image& image);

    int m_iupgradexscale;
    int m_iupgradeyscale;

    int m_iupgradeanglescale;

    void RotateMatchAB_upgrade(Image& image);
    void RotateMatchAB05_upgrade(Image& image);
    void RotateMatchAB025_upgrade(Image& image);

    void RotateMatchSample(Image& image, gp_Path& path, PointsShape& modelrect, double dangle);
    void RotateMatchSample_upgrade(Image& image, gp_Path& path, PointsShape& modelrect, double dangle, gp_Pnt& resultpoint);

    void RotateMatchSampleAB(Image& image, gp_Path& pathA,
        gp_Path& pathB, PointsShape& modelrect,
        double dangle);

    int m_ispecshow;
    double m_dminscore;

    static int m_curfastmatchnum;

    FastMatch* m_prelationmatch;

    int m_irelationresultnum;

    double m_drelationzoomx;

    double m_drelationzoomy;

    gp_Rectangle m_irelationrect;

public:
    RectsShape* getresultrects() { return &m_resultrects; }
    const RectsShape* getresultrects() const { return &m_resultrects; }

    void setrelationrectfromresultnum(int inum);
    void setrelationrectfrom_matchresult(void* pmatch);
    void setrelationxy(int iprex1, int iprey1, int iendx1, int iendy1);
    void setrelationzoom(double drelationzoomx, double drelationzoomy);
    void setrelationtorect();
    void setcolorstyle(int istyle);
    void Setupgradescale(int isx, int isy);
    void Setupgradeanglescale(int iangle);

    void MinModelLearn(Image& image);

    void shapesetroi(void* pshape);
    void matchstepgap(int ix, int iy);

    double getrotateresultx();
    double getrotateresulty();
    double getrotateresulta();
    double getrotateresultscore();
    double getrotateresultscoreA(int inum);

    void rotateresultsortfilter(int ifdx, int ifdy, int itype);

    void rotateresultsortfilterA(int ifdx, int ifdy, int itype);

    int rotateresultsize();
    void setshownum(int ishownum);
    int m_ishownum = 1;

    void getresultcentpoints(void* points);
    void getrotateresultrectpoints(std::vector<cv::Point2f>& points);
    void ZeroPOS();

    virtual void PublishDisplayShapes(ICxShapeSink& sink, const std::string& owner_ref);
    bool ApplyDisplayShapeEdit(const std::string& owner_binding, const std::string& semantic_role,
                               double x0, double y0, double x1, double y1, std::string& reason);
};

#endif //FASTMATCH_H
