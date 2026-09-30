#pragma once
#include "../cxgeom/include/CxGeoSO2Harmonic.h"
#include <string>
#include <optional>
#include "../cxgeom/include/CxGeoSO2ReferenceAsset.h"
#include <vector>

// Explicit script-side observer. Owns copies only; no FastMatch pointer or seed setter.
class CxFastMatchHarmonicAudit {
public:
    // cxscript numeric arguments arrive reversed; keep script API in natural order.
    void point_script(double y,double x) { point(x,y); }
    void topology_script(int components,int holes,int complete,int closed,int verified) { topology(verified,closed,complete,holes,components); }
    void expectposebounds_script(double scale_tolerance,double angle_tolerance,double scale,double angle) { expectposebounds(angle,scale,angle_tolerance,scale_tolerance); }
    void expectpose_script(double tolerance,double scale,double angle) { expectpose(angle,scale,tolerance); }
    void select(int side);
    void clear();
    void point(double x, double y);
    void topology(int verified, int closed, int complete, int holes, int components);
    void fromreference(void* object);
    void snapshotfastmatch(void* object);
    void expectunchanged(void* object);
    void sourceindex(int index);
    void fromobject(void* object);
    void parameter(double value, const char* name);
    void run();
    void expectstatus(const char* expected);
    void expectpose(double angle, double scale, double tolerance);
    void expectcount(int count);
    void expectposebounds(double angle,double scale,double angle_tolerance,double scale_tolerance);
    void save(const char* path);
    void trustedsha(const char* hash);
    void saveasset(const char* path);
    void loadasset(const char* path);
private:
    void invalidate();
    cxgeom::so2::Contour contours_[2];
    std::string sources_[2] = {"script_points", "script_points"};
    cxgeom::so2::Config config_;
    cxgeom::so2::Method method_ = cxgeom::so2::Method::Dft;
    cxgeom::so2::Result result_;
    int selected_ = 0;
    int source_index_ = 0;
    bool ran_ = false;
    int assertions_ = 0;
    std::vector<std::string> history_;
    std::string fastmatch_snapshot_;
    std::optional<cxgeom::so2::Descriptor> loaded_[2];
    std::string trusted_sha_;
    std::vector<std::string> asset_events_;
};
