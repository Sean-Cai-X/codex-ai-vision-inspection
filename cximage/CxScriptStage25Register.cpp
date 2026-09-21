#include "muParser.h"
#include "CxScriptStage25Manifest.h"
#include "CxScriptStage25Runner.h"

Stage25Manifest g_stage25_manifest;
Stage25ImageCase* g_current_image = nullptr;
Stage25FindlineProfile* g_current_findline_profile = nullptr;
Stage25FindcircleProfile* g_current_findcircle_profile = nullptr;
Stage25EvidenceProfile* g_current_evidence_profile = nullptr;

class Stage25ManifestBinding
{
public:
    void reset()
    {
        g_stage25_manifest = Stage25Manifest{};
        g_current_image = nullptr;
        g_current_findline_profile = nullptr;
        g_current_findcircle_profile = nullptr;
        g_current_evidence_profile = nullptr;
    }

    void setname(const char* value)
    {
        g_stage25_manifest.name = value ? value : "";
    }

    void setoutroot(const char* value)
    {
        g_stage25_manifest.outroot = value ? value : "";
    }

    void setimageroot(const char* value)
    {
        g_stage25_manifest.imageroot = value ? value : "";
    }

    void setminimagesforstability(double value)
    {
        g_stage25_manifest.min_images_for_stability =
            static_cast<int>(value);
    }

    void setminlevelsforstability(double value)
    {
        g_stage25_manifest.min_levels_for_stability =
            static_cast<int>(value);
    }

    void addimage(mu::charpvect& params)
    {
        if (params.size() < 3)
            return;

        Stage25ImageCase img;
        img.image_id = params[0];
        img.level = params[1];
        img.path = params[2];

        g_stage25_manifest.images.push_back(img);
        g_current_image = &g_stage25_manifest.images.back();
    }

    void image_addtag(const char* value)
    {
        if (!g_current_image)
            return;

        g_current_image->tags.push_back(value ? value : "");
    }

    void image_setexpectededge(const char* value)
    {
        if (!g_current_image)
            return;

        g_current_image->expected_edge = value ? value : "";
    }

    void image_setlighting(const char* value)
    {
        if (!g_current_image)
            return;

        g_current_image->lighting = value ? value : "";
    }

    void image_setcontrast(const char* value)
    {
        if (!g_current_image)
            return;

        g_current_image->contrast = value ? value : "";
    }

    void image_addfindlinetarget(mu::charpvect& params)
    {
        if (!g_current_image || params.size() < 7)
            return;

        Stage25ImageTarget target;
        target.target_id = params[0];
        target.tool = "FindLine";
        target.x0 = std::stoi(params[1]);
        target.y0 = std::stoi(params[2]);
        target.x1 = std::stoi(params[3]);
        target.y1 = std::stoi(params[4]);
        target.wgap = std::stoi(params[5]);
        target.hgap = std::stoi(params[6]);

        g_current_image->targets.push_back(target);
    }

    void image_addfindcircletarget(mu::charpvect& params)
    {
        if (!g_current_image || params.size() < 7)
            return;

        Stage25ImageTarget target;
        target.target_id = params[0];
        target.tool = "Findcircle";
        target.cx = std::stoi(params[1]);
        target.cy = std::stoi(params[2]);
        target.px = std::stoi(params[3]);
        target.py = std::stoi(params[4]);
        target.gap = std::stoi(params[5]);
        target.linegap = std::stoi(params[6]);

        g_current_image->targets.push_back(target);
    }

    void image_lasttarget_setorientation(const char* value)
    {
        if (!g_current_image || g_current_image->targets.empty())
            return;
        g_current_image->targets.back().orientation = value ? value : "";
    }

    void image_lasttarget_settargetlevel(const char* value)
    {
        if (!g_current_image || g_current_image->targets.empty())
            return;
        g_current_image->targets.back().target_level = value ? value : "";
    }

    void image_lasttarget_settargetrole(const char* value)
    {
        if (!g_current_image || g_current_image->targets.empty())
            return;
        g_current_image->targets.back().target_role = value ? value : "";
    }

    void addfindlineprofile(const char* value)
    {
        Stage25FindlineProfile profile;
        profile.profile_id = value ? value : "";

        g_stage25_manifest.findline_profiles.push_back(profile);
        g_current_findline_profile =
            &g_stage25_manifest.findline_profiles.back();
    }

    void findline_setmethod(double value)
    {
        if (g_current_findline_profile)
            g_current_findline_profile->method = static_cast<int>(value);
    }

    void findline_setthreshold(double value)
    {
        if (g_current_findline_profile)
            g_current_findline_profile->threshold = static_cast<int>(value);
    }

    void findline_setlinegap(double value)
    {
        if (g_current_findline_profile)
            g_current_findline_profile->linegap = static_cast<int>(value);
    }

    void findline_setfitmode(double value)
    {
        if (g_current_findline_profile)
            g_current_findline_profile->fitmode = static_cast<int>(value);
    }

    void findline_setscript_scale(double value)
    {
        if (g_current_findline_profile)
            g_current_findline_profile->script_scale = static_cast<int>(value);
    }

    void findline_setfilterprofile(double value)
    {
        if (g_current_findline_profile)
            g_current_findline_profile->filter_profile = static_cast<int>(value);
    }

    void findline_setobjfilter(double value)
    {
        if (!g_current_findline_profile)
            return;

        g_current_findline_profile->has_explicit_filter = true;
        g_current_findline_profile->objfilter = static_cast<int>(value);
    }

    void findline_setfilter(mu::charpvect& params)
    {
        if (!g_current_findline_profile || params.size() < 3)
            return;

        g_current_findline_profile->has_explicit_filter = true;
        g_current_findline_profile->filter_borw = std::stoi(params[0]);
        g_current_findline_profile->filter_min = std::stoi(params[1]);
        g_current_findline_profile->filter_max = std::stoi(params[2]);
    }

    void findline_setgamarate(double value)
    {
        if (!g_current_findline_profile)
            return;

        g_current_findline_profile->has_gamma = true;
        g_current_findline_profile->gamma = static_cast<int>(value);
    }

    void findline_setpolicy(const char* value)
    {
        if (g_current_findline_profile)
            g_current_findline_profile->policy = value ? value : "";
    }

    void findline_setparameterpolicyid(const char* value)
    {
        if (g_current_findline_profile)
            g_current_findline_profile->parameter_policy_id = value ? value : "";
    }

    void findline_setparameterrole(const char* value)
    {
        if (g_current_findline_profile)
            g_current_findline_profile->parameter_role = value ? value : "";
    }

    void findline_setisproductdefault(double value)
    {
        if (g_current_findline_profile)
            g_current_findline_profile->is_product_default = (value != 0);
    }

    void findline_setisstage25default(double value)
    {
        if (g_current_findline_profile)
            g_current_findline_profile->is_stage25_default = (value != 0);
    }

    void addfindcircleprofile(const char* value)
    {
        Stage25FindcircleProfile profile;
        profile.profile_id = value ? value : "";

        g_stage25_manifest.findcircle_profiles.push_back(profile);
        g_current_findcircle_profile =
            &g_stage25_manifest.findcircle_profiles.back();
    }

    void findcircle_setmethod(double value)
    {
        if (g_current_findcircle_profile)
            g_current_findcircle_profile->method = static_cast<int>(value);
    }

    void findcircle_setthreshold(double value)
    {
        if (g_current_findcircle_profile)
            g_current_findcircle_profile->threshold = static_cast<int>(value);
    }

    void findcircle_setgap(double value)
    {
        if (g_current_findcircle_profile)
            g_current_findcircle_profile->gap = static_cast<int>(value);
    }

    void findcircle_setlinegap(double value)
    {
        if (g_current_findcircle_profile)
            g_current_findcircle_profile->linegap = static_cast<int>(value);
    }

    void findcircle_setfindsetting(double value)
    {
        if (!g_current_findcircle_profile)
            return;

        g_current_findcircle_profile->has_filter = true;
        g_current_findcircle_profile->findsetting = static_cast<int>(value);
    }

    void findcircle_setfilter(mu::charpvect& params)
    {
        if (!g_current_findcircle_profile || params.size() < 3)
            return;

        g_current_findcircle_profile->has_filter = true;
        g_current_findcircle_profile->filter_borw = std::stoi(params[0]);
        g_current_findcircle_profile->filter_min = std::stoi(params[1]);
        g_current_findcircle_profile->filter_max = std::stoi(params[2]);
    }

    void findcircle_setsamplerate(double value)
    {
        if (!g_current_findcircle_profile)
            return;

        g_current_findcircle_profile->has_samplerate = true;
        g_current_findcircle_profile->samplerate = value;
    }

    void findcircle_setpolicy(const char* value)
    {
        if (g_current_findcircle_profile)
            g_current_findcircle_profile->policy = value ? value : "";
    }

    void findcircle_setparameterpolicyid(const char* value)
    {
        if (g_current_findcircle_profile)
            g_current_findcircle_profile->parameter_policy_id = value ? value : "";
    }

    void findcircle_setparameterrole(const char* value)
    {
        if (g_current_findcircle_profile)
            g_current_findcircle_profile->parameter_role = value ? value : "";
    }

    void findcircle_setisproductdefault(double value)
    {
        if (g_current_findcircle_profile)
            g_current_findcircle_profile->is_product_default = (value != 0);
    }

    void findcircle_setisstage25default(double value)
    {
        if (g_current_findcircle_profile)
            g_current_findcircle_profile->is_stage25_default = (value != 0);
    }

    void addevidenceprofile(const char* value)
    {
        Stage25EvidenceProfile profile;
        profile.name = value ? value : "";

        g_stage25_manifest.evidence_profiles.push_back(profile);
        g_current_evidence_profile =
            &g_stage25_manifest.evidence_profiles.back();
    }

    void evidence_setnearestpointsupportpx(double value)
    {
        if (g_current_evidence_profile)
            g_current_evidence_profile->nearest_point_support_px = value;
    }

    void evidence_setlinedistancesupportpx(double value)
    {
        if (g_current_evidence_profile)
            g_current_evidence_profile->line_distance_support_px = value;
    }

    void evidence_setmingradient(double value)
    {
        if (g_current_evidence_profile)
            g_current_evidence_profile->min_gradient = value;
    }

    void evidence_setmingradientratio(double value)
    {
        if (g_current_evidence_profile)
            g_current_evidence_profile->min_gradient_ratio = value;
    }
};

void RegisterStage25CxScriptBindings(mu::Parser& parser)
{
    double* org_double = nullptr;
    parser.DefineOrgClass("double", org_double);

    parser.UsingClass(true);

    Stage25ManifestBinding* manifest = nullptr;

    parser.DefineClass("Stage25Manifest", manifest);

    parser.DefineClassFun("Stage25Manifest", manifest, "reset",
                          &Stage25ManifestBinding::reset);

    parser.DefineClassFun("Stage25Manifest", manifest, "setname",
                          &Stage25ManifestBinding::setname);

    parser.DefineClassFun("Stage25Manifest", manifest, "setoutroot",
                          &Stage25ManifestBinding::setoutroot);

    parser.DefineClassFun("Stage25Manifest", manifest, "setimageroot",
                          &Stage25ManifestBinding::setimageroot);

    parser.DefineClassFun("Stage25Manifest", manifest, "setminimagesforstability",
                          &Stage25ManifestBinding::setminimagesforstability);

    parser.DefineClassFun("Stage25Manifest", manifest, "setminlevelsforstability",
                          &Stage25ManifestBinding::setminlevelsforstability);

    parser.DefineClassFun("Stage25Manifest", manifest, "addimage",
                          &Stage25ManifestBinding::addimage);

    parser.DefineClassFun("Stage25Manifest", manifest, "image_addtag",
                          &Stage25ManifestBinding::image_addtag);

    parser.DefineClassFun("Stage25Manifest", manifest, "image_setexpectededge",
                          &Stage25ManifestBinding::image_setexpectededge);

    parser.DefineClassFun("Stage25Manifest", manifest, "image_setlighting",
                          &Stage25ManifestBinding::image_setlighting);

    parser.DefineClassFun("Stage25Manifest", manifest, "image_setcontrast",
                          &Stage25ManifestBinding::image_setcontrast);

    parser.DefineClassFun("Stage25Manifest", manifest, "image_addfindlinetarget",
                          &Stage25ManifestBinding::image_addfindlinetarget);

    parser.DefineClassFun("Stage25Manifest", manifest, "image_addfindcircletarget",
                          &Stage25ManifestBinding::image_addfindcircletarget);

    parser.DefineClassFun("Stage25Manifest", manifest, "image_lasttarget_setorientation",
                          &Stage25ManifestBinding::image_lasttarget_setorientation);

    parser.DefineClassFun("Stage25Manifest", manifest, "image_lasttarget_settargetlevel",
                          &Stage25ManifestBinding::image_lasttarget_settargetlevel);

    parser.DefineClassFun("Stage25Manifest", manifest, "image_lasttarget_settargetrole",
                          &Stage25ManifestBinding::image_lasttarget_settargetrole);

    parser.DefineClassFun("Stage25Manifest", manifest, "addfindlineprofile",
                          &Stage25ManifestBinding::addfindlineprofile);

    parser.DefineClassFun("Stage25Manifest", manifest, "findline_setmethod",
                          &Stage25ManifestBinding::findline_setmethod);

    parser.DefineClassFun("Stage25Manifest", manifest, "findline_setthreshold",
                          &Stage25ManifestBinding::findline_setthreshold);

    parser.DefineClassFun("Stage25Manifest", manifest, "findline_setlinegap",
                          &Stage25ManifestBinding::findline_setlinegap);

    parser.DefineClassFun("Stage25Manifest", manifest, "findline_setfitmode",
                          &Stage25ManifestBinding::findline_setfitmode);

    parser.DefineClassFun("Stage25Manifest", manifest, "findline_setscript_scale",
                          &Stage25ManifestBinding::findline_setscript_scale);

    parser.DefineClassFun("Stage25Manifest", manifest, "findline_setfilterprofile",
                          &Stage25ManifestBinding::findline_setfilterprofile);

    parser.DefineClassFun("Stage25Manifest", manifest, "findline_setobjfilter",
                          &Stage25ManifestBinding::findline_setobjfilter);

    parser.DefineClassFun("Stage25Manifest", manifest, "findline_setfilter",
                          &Stage25ManifestBinding::findline_setfilter);

    parser.DefineClassFun("Stage25Manifest", manifest, "findline_setgamarate",
                          &Stage25ManifestBinding::findline_setgamarate);

    parser.DefineClassFun("Stage25Manifest", manifest, "findline_setpolicy",
                          &Stage25ManifestBinding::findline_setpolicy);

    parser.DefineClassFun("Stage25Manifest", manifest, "findline_setparameterpolicyid",
                          &Stage25ManifestBinding::findline_setparameterpolicyid);

    parser.DefineClassFun("Stage25Manifest", manifest, "findline_setparameterrole",
                          &Stage25ManifestBinding::findline_setparameterrole);

    parser.DefineClassFun("Stage25Manifest", manifest, "findline_setisproductdefault",
                          &Stage25ManifestBinding::findline_setisproductdefault);

    parser.DefineClassFun("Stage25Manifest", manifest, "findline_setisstage25default",
                          &Stage25ManifestBinding::findline_setisstage25default);

    parser.DefineClassFun("Stage25Manifest", manifest, "addfindcircleprofile",
                          &Stage25ManifestBinding::addfindcircleprofile);

    parser.DefineClassFun("Stage25Manifest", manifest, "findcircle_setmethod",
                          &Stage25ManifestBinding::findcircle_setmethod);

    parser.DefineClassFun("Stage25Manifest", manifest, "findcircle_setthreshold",
                          &Stage25ManifestBinding::findcircle_setthreshold);

    parser.DefineClassFun("Stage25Manifest", manifest, "findcircle_setgap",
                          &Stage25ManifestBinding::findcircle_setgap);

    parser.DefineClassFun("Stage25Manifest", manifest, "findcircle_setlinegap",
                          &Stage25ManifestBinding::findcircle_setlinegap);

    parser.DefineClassFun("Stage25Manifest", manifest, "findcircle_setfindsetting",
                          &Stage25ManifestBinding::findcircle_setfindsetting);

    parser.DefineClassFun("Stage25Manifest", manifest, "findcircle_setfilter",
                          &Stage25ManifestBinding::findcircle_setfilter);

    parser.DefineClassFun("Stage25Manifest", manifest, "findcircle_setsamplerate",
                          &Stage25ManifestBinding::findcircle_setsamplerate);

    parser.DefineClassFun("Stage25Manifest", manifest, "findcircle_setpolicy",
                          &Stage25ManifestBinding::findcircle_setpolicy);

    parser.DefineClassFun("Stage25Manifest", manifest, "findcircle_setparameterpolicyid",
                          &Stage25ManifestBinding::findcircle_setparameterpolicyid);

    parser.DefineClassFun("Stage25Manifest", manifest, "findcircle_setparameterrole",
                          &Stage25ManifestBinding::findcircle_setparameterrole);

    parser.DefineClassFun("Stage25Manifest", manifest, "findcircle_setisproductdefault",
                          &Stage25ManifestBinding::findcircle_setisproductdefault);

    parser.DefineClassFun("Stage25Manifest", manifest, "findcircle_setisstage25default",
                          &Stage25ManifestBinding::findcircle_setisstage25default);

    parser.DefineClassFun("Stage25Manifest", manifest, "addevidenceprofile",
                          &Stage25ManifestBinding::addevidenceprofile);

    parser.DefineClassFun("Stage25Manifest", manifest, "evidence_setnearestpointsupportpx",
                          &Stage25ManifestBinding::evidence_setnearestpointsupportpx);

    parser.DefineClassFun("Stage25Manifest", manifest, "evidence_setlinedistancesupportpx",
                          &Stage25ManifestBinding::evidence_setlinedistancesupportpx);

    parser.DefineClassFun("Stage25Manifest", manifest, "evidence_setmingradient",
                          &Stage25ManifestBinding::evidence_setmingradient);

    parser.DefineClassFun("Stage25Manifest", manifest, "evidence_setmingradientratio",
                          &Stage25ManifestBinding::evidence_setmingradientratio);
}