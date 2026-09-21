#include "CxParamRegressionRegister.h"

#include <algorithm>
#include <sstream>

CxParamRegressionRuntime g_cxscript_param_regression;
CxParamRange* g_current_param_range = nullptr;
CxParamCandidate* g_current_param_candidate = nullptr;

namespace
{
    std::vector<double> ParseValues(const char* text)
    {
        std::vector<double> values;
        if (!text)
            return values;

        std::stringstream ss(text);
        std::string item;
        while (std::getline(ss, item, ','))
        {
            try
            {
                values.push_back(std::stod(item));
            }
            catch (...)
            {
            }
        }
        return values;
    }

    void UpdateRangeBounds(CxParamRange& range)
    {
        if (range.discrete_values.empty())
            return;
        range.min_value = *std::min_element(range.discrete_values.begin(), range.discrete_values.end());
        range.max_value = *std::max_element(range.discrete_values.begin(), range.discrete_values.end());
    }

    int NextCandidateIndex()
    {
        return static_cast<int>(g_cxscript_param_regression.candidates.size());
    }
}

double ParamRegressionTask_reset(double)
{
    g_cxscript_param_regression.Clear();
    g_current_param_range = nullptr;
    g_current_param_candidate = nullptr;
    return 0.0;
}

double ParamRegressionTask_setid(const char* value)
{
    g_cxscript_param_regression.task.task_id = value ? value : "";
    return 0.0;
}

double ParamRegressionTask_setcase(const char* value)
{
    g_cxscript_param_regression.task.case_id = value ? value : "";
    return 0.0;
}

double ParamRegressionTask_setimage(const char* value)
{
    g_cxscript_param_regression.task.image_id = value ? value : "";
    return 0.0;
}

double ParamRegressionTask_settarget(const char* value)
{
    g_cxscript_param_regression.task.target_id = value ? value : "";
    return 0.0;
}

double ParamRegressionTask_settool(const char* value)
{
    g_cxscript_param_regression.task.tool = value ? value : "";
    g_cxscript_param_regression.range_set.tool = g_cxscript_param_regression.task.tool;
    return 0.0;
}

double ParamRegressionTask_setgaugeannotation(const char* value)
{
    g_cxscript_param_regression.task.gauge_annotation_path = value ? value : "";
    return 0.0;
}

double ParamRegressionTask_setscript(const char* value)
{
    g_cxscript_param_regression.task.base_script_id = value ? value : "";
    return 0.0;
}

double ParamRegressionTask_setbaseprofile(const char* value)
{
    g_cxscript_param_regression.task.base_parameter_profile_id = value ? value : "";
    return 0.0;
}

double ParamRegressionTask_setmaxcandidates(double value)
{
    g_cxscript_param_regression.task.max_candidates = static_cast<int>(value);
    g_cxscript_param_regression.range_set.max_candidates = static_cast<int>(value);
    return 0.0;
}

double ParamRegressionTask_setmaxcaseseconds(double value)
{
    g_cxscript_param_regression.task.max_case_seconds = static_cast<int>(value);
    g_cxscript_param_regression.range_set.max_case_seconds = static_cast<int>(value);
    return 0.0;
}

double ParamRegressionTask_setmaxtotalseconds(double value)
{
    g_cxscript_param_regression.task.max_total_seconds = static_cast<int>(value);
    g_cxscript_param_regression.range_set.max_total_seconds = static_cast<int>(value);
    return 0.0;
}

double ParamRegressionTask_requiremanualgauge(double value)
{
    g_cxscript_param_regression.task.require_manual_gauge = (value != 0.0);
    return 0.0;
}

double ParamRegressionTask_enablemlpackrank(double value)
{
    g_cxscript_param_regression.task.allow_mlpack_rank = (value != 0.0);
    return 0.0;
}

double ParamRegressionTask_enableensmallenopt(double value)
{
    g_cxscript_param_regression.task.allow_ensmallen_opt = (value != 0.0);
    return 0.0;
}

double ParamRegressionTask_setpromotionallowed(double value)
{
    g_cxscript_param_regression.task.allow_promote = (value != 0.0);
    return 0.0;
}

double ParamRangeSet_reset(double)
{
    g_cxscript_param_regression.range_set = CxParamRangeSet{};
    g_current_param_range = nullptr;
    return 0.0;
}

double ParamRangeSet_setid(const char* value)
{
    g_cxscript_param_regression.range_set.range_set_id = value ? value : "";
    return 0.0;
}

double ParamRangeSet_settool(const char* value)
{
    g_cxscript_param_regression.range_set.tool = value ? value : "";
    g_cxscript_param_regression.task.tool = g_cxscript_param_regression.range_set.tool;
    return 0.0;
}

double ParamRangeSet_setmaxcandidates(double value)
{
    g_cxscript_param_regression.range_set.max_candidates = static_cast<int>(value);
    return 0.0;
}

double ParamRangeSet_setmaxcaseseconds(double value)
{
    g_cxscript_param_regression.range_set.max_case_seconds = static_cast<int>(value);
    return 0.0;
}

double ParamRangeSet_setmaxtotalseconds(double value)
{
    g_cxscript_param_regression.range_set.max_total_seconds = static_cast<int>(value);
    return 0.0;
}

double ParamRange_add(const char* name)
{
    CxParamRange range;
    range.name = name ? name : "";
    g_cxscript_param_regression.range_set.ranges.push_back(range);
    g_current_param_range = &g_cxscript_param_regression.range_set.ranges.back();
    return 0.0;
}

double ParamRange_setvalues(const char* values)
{
    if (!g_current_param_range)
        return 0.0;
    g_current_param_range->discrete_values = ParseValues(values);
    UpdateRangeBounds(*g_current_param_range);
    return 0.0;
}

double ParamRange_setrole(const char* role)
{
    if (g_current_param_range)
        g_current_param_range->role = role ? role : "";
    return 0.0;
}

double ParamRange_setenabled(double value)
{
    if (g_current_param_range)
        g_current_param_range->enabled = (value != 0.0);
    return 0.0;
}

double ParamCandidate_clear(double)
{
    g_cxscript_param_regression.candidates.clear();
    g_current_param_candidate = nullptr;
    return 0.0;
}

double ParamCandidate_add(const char* id)
{
    CxParamCandidate candidate;
    candidate.candidate_id = id && id[0] ? id : ("candidate_" + std::to_string(NextCandidateIndex()));
    g_cxscript_param_regression.candidates.push_back(candidate);
    g_current_param_candidate = &g_cxscript_param_regression.candidates.back();
    return 0.0;
}

double ParamCandidate_setsource(const char* value)
{
    if (g_current_param_candidate)
        g_current_param_candidate->source = value ? value : "";
    return 0.0;
}

double ParamCandidate_setmethod(double value)
{
    if (g_current_param_candidate) g_current_param_candidate->method = static_cast<int>(value);
    return 0.0;
}

double ParamCandidate_setthreshold(double value)
{
    if (g_current_param_candidate) g_current_param_candidate->threshold = static_cast<int>(value);
    return 0.0;
}

double ParamCandidate_setgap(double value)
{
    if (g_current_param_candidate) g_current_param_candidate->gap = static_cast<int>(value);
    return 0.0;
}

double ParamCandidate_setlinegap(double value)
{
    if (g_current_param_candidate) g_current_param_candidate->linegap = static_cast<int>(value);
    return 0.0;
}

double ParamCandidate_setminedgerunwidth(double value)
{
    if (g_current_param_candidate)
        g_current_param_candidate->min_edge_run_width_px =
            std::max(1, std::min(20, static_cast<int>(value)));
    return 0.0;
}

double ParamCandidate_setwgap(double value)
{
    if (g_current_param_candidate) g_current_param_candidate->wgap = static_cast<int>(value);
    return 0.0;
}

double ParamCandidate_sethgap(double value)
{
    if (g_current_param_candidate) g_current_param_candidate->hgap = static_cast<int>(value);
    return 0.0;
}

double ParamCandidate_setfilterprofile(double value)
{
    if (g_current_param_candidate) g_current_param_candidate->filterprofile = static_cast<int>(value);
    return 0.0;
}

double ParamCandidate_setsamplerate(double value)
{
    if (g_current_param_candidate) g_current_param_candidate->samplerate = static_cast<int>(value);
    return 0.0;
}

double ParamCandidate_setpredictedquality(double value)
{
    if (g_current_param_candidate) g_current_param_candidate->predicted_quality = value;
    return 0.0;
}

double ParamCandidate_setpredictedrisk(double value)
{
    if (g_current_param_candidate) g_current_param_candidate->predicted_risk = value;
    return 0.0;
}

double ParamCandidate_setpredictedfailureclass(const char* value)
{
    if (g_current_param_candidate) g_current_param_candidate->predicted_failure_class = value ? value : "";
    return 0.0;
}

double ParamCandidate_setselectedforprobe(double value)
{
    if (g_current_param_candidate) g_current_param_candidate->selected_for_probe = (value != 0.0);
    return 0.0;
}

void RegisterCxParamRegressionBindings(mu::Parser& parser)
{
    parser.DefineFun("ParamRegressionTask_reset", (mu::fun_type1)&ParamRegressionTask_reset);
    parser.DefineFun("ParamRegressionTask_setid", (mu::strfun_type1)&ParamRegressionTask_setid);
    parser.DefineFun("ParamRegressionTask_setcase", (mu::strfun_type1)&ParamRegressionTask_setcase);
    parser.DefineFun("ParamRegressionTask_setimage", (mu::strfun_type1)&ParamRegressionTask_setimage);
    parser.DefineFun("ParamRegressionTask_settarget", (mu::strfun_type1)&ParamRegressionTask_settarget);
    parser.DefineFun("ParamRegressionTask_settool", (mu::strfun_type1)&ParamRegressionTask_settool);
    parser.DefineFun("ParamRegressionTask_setgaugeannotation", (mu::strfun_type1)&ParamRegressionTask_setgaugeannotation);
    parser.DefineFun("ParamRegressionTask_setscript", (mu::strfun_type1)&ParamRegressionTask_setscript);
    parser.DefineFun("ParamRegressionTask_setbaseprofile", (mu::strfun_type1)&ParamRegressionTask_setbaseprofile);
    parser.DefineFun("ParamRegressionTask_setmaxcandidates", (mu::fun_type1)&ParamRegressionTask_setmaxcandidates);
    parser.DefineFun("ParamRegressionTask_setmaxcaseseconds", (mu::fun_type1)&ParamRegressionTask_setmaxcaseseconds);
    parser.DefineFun("ParamRegressionTask_setmaxtotalseconds", (mu::fun_type1)&ParamRegressionTask_setmaxtotalseconds);
    parser.DefineFun("ParamRegressionTask_requiremanualgauge", (mu::fun_type1)&ParamRegressionTask_requiremanualgauge);
    parser.DefineFun("ParamRegressionTask_enablemlpackrank", (mu::fun_type1)&ParamRegressionTask_enablemlpackrank);
    parser.DefineFun("ParamRegressionTask_enableensmallenopt", (mu::fun_type1)&ParamRegressionTask_enableensmallenopt);
    parser.DefineFun("ParamRegressionTask_setpromotionallowed", (mu::fun_type1)&ParamRegressionTask_setpromotionallowed);

    parser.DefineFun("ParamRangeSet_reset", (mu::fun_type1)&ParamRangeSet_reset);
    parser.DefineFun("ParamRangeSet_setid", (mu::strfun_type1)&ParamRangeSet_setid);
    parser.DefineFun("ParamRangeSet_settool", (mu::strfun_type1)&ParamRangeSet_settool);
    parser.DefineFun("ParamRangeSet_setmaxcandidates", (mu::fun_type1)&ParamRangeSet_setmaxcandidates);
    parser.DefineFun("ParamRangeSet_setmaxcaseseconds", (mu::fun_type1)&ParamRangeSet_setmaxcaseseconds);
    parser.DefineFun("ParamRangeSet_setmaxtotalseconds", (mu::fun_type1)&ParamRangeSet_setmaxtotalseconds);
    parser.DefineFun("ParamRange_add", (mu::strfun_type1)&ParamRange_add);
    parser.DefineFun("ParamRange_setvalues", (mu::strfun_type1)&ParamRange_setvalues);
    parser.DefineFun("ParamRange_setrole", (mu::strfun_type1)&ParamRange_setrole);
    parser.DefineFun("ParamRange_setenabled", (mu::fun_type1)&ParamRange_setenabled);

    parser.DefineFun("ParamCandidate_clear", (mu::fun_type1)&ParamCandidate_clear);
    parser.DefineFun("ParamCandidate_add", (mu::strfun_type1)&ParamCandidate_add);
    parser.DefineFun("ParamCandidate_setsource", (mu::strfun_type1)&ParamCandidate_setsource);
    parser.DefineFun("ParamCandidate_setmethod", (mu::fun_type1)&ParamCandidate_setmethod);
    parser.DefineFun("ParamCandidate_setthreshold", (mu::fun_type1)&ParamCandidate_setthreshold);
    parser.DefineFun("ParamCandidate_setgap", (mu::fun_type1)&ParamCandidate_setgap);
    parser.DefineFun("ParamCandidate_setlinegap", (mu::fun_type1)&ParamCandidate_setlinegap);
    parser.DefineFun("ParamCandidate_setminedgerunwidth",
                     (mu::fun_type1)&ParamCandidate_setminedgerunwidth);
    parser.DefineFun("ParamCandidate_setwgap", (mu::fun_type1)&ParamCandidate_setwgap);
    parser.DefineFun("ParamCandidate_sethgap", (mu::fun_type1)&ParamCandidate_sethgap);
    parser.DefineFun("ParamCandidate_setfilterprofile", (mu::fun_type1)&ParamCandidate_setfilterprofile);
    parser.DefineFun("ParamCandidate_setsamplerate", (mu::fun_type1)&ParamCandidate_setsamplerate);
    parser.DefineFun("ParamCandidate_setpredictedquality", (mu::fun_type1)&ParamCandidate_setpredictedquality);
    parser.DefineFun("ParamCandidate_setpredictedrisk", (mu::fun_type1)&ParamCandidate_setpredictedrisk);
    parser.DefineFun("ParamCandidate_setpredictedfailureclass", (mu::strfun_type1)&ParamCandidate_setpredictedfailureclass);
    parser.DefineFun("ParamCandidate_setselectedforprobe", (mu::fun_type1)&ParamCandidate_setselectedforprobe);
}
