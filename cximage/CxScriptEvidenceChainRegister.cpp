#include "muParser.h"
#include "CxScriptEvidenceChainRuntime.h"
#include "CxScriptEvidenceChainRegister.h"

#include <cstdlib>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

CxScriptEvidenceChainRuntime g_cxscript_evidence_chain;
CxScriptEvidenceCase* g_current_evidence_case = nullptr;

static std::unordered_map<std::string, std::string> ParseEvidenceKeyValueListLocal(
    const char* value)
{
    std::unordered_map<std::string, std::string> result;
    if (!value)
        return result;

    std::istringstream stream(value);
    std::string token;
    while (stream >> token)
    {
        const std::size_t eq = token.find('=');
        if (eq == std::string::npos)
            continue;
        const std::string key = token.substr(0, eq);
        const std::string val = token.substr(eq + 1);
        if (!key.empty())
            result[key] = val;
    }
    return result;
}

static std::string GetEvidenceKvLocal(
    const std::unordered_map<std::string, std::string>& values,
    const char* key,
    const std::string& fallback = std::string())
{
    const auto it = values.find(key);
    return it == values.end() ? fallback : it->second;
}

static double GetEvidenceKvDoubleLocal(
    const std::unordered_map<std::string, std::string>& values,
    const char* key,
    double fallback = 0.0)
{
    const auto it = values.find(key);
    if (it == values.end())
        return fallback;
    return std::strtod(it->second.c_str(), nullptr);
}

static int GetEvidenceKvIntLocal(
    const std::unordered_map<std::string, std::string>& values,
    const char* key,
    int fallback = -1)
{
    const auto it = values.find(key);
    if (it == values.end())
        return fallback;
    return static_cast<int>(std::strtol(it->second.c_str(), nullptr, 10));
}

double CxEvidenceChain_reset(double)
{
    g_cxscript_evidence_chain = CxScriptEvidenceChainRuntime{};
    g_current_evidence_case = nullptr;
    return 0.0;
}

double CxEvidenceChain_setid(const char* value)
{
    g_cxscript_evidence_chain.chain_id = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_setname(const char* value)
{
    g_cxscript_evidence_chain.chain_name = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_setcatalog(const char* value)
{
    g_cxscript_evidence_chain.catalog_path = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_setmanifest(const char* value)
{
    g_cxscript_evidence_chain.image_manifest_path = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_setoutputroot(const char* value)
{
    g_cxscript_evidence_chain.output_root = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_addcase(const char* evidence_id)
{
    if (!evidence_id || evidence_id[0] == '\0')
        return 0.0;

    CxScriptEvidenceCase case_entry;
    case_entry.evidence_id = evidence_id;
    g_cxscript_evidence_chain.cases.push_back(case_entry);
    g_current_evidence_case = &g_cxscript_evidence_chain.cases.back();
    return 0.0;
}

double CxEvidenceChain_case_setimage(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    g_current_evidence_case->image_id = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_case_settarget(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    g_current_evidence_case->target_id = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_case_setscript(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    g_current_evidence_case->script_id = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_case_setparameter(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    g_current_evidence_case->parameter_profile_id = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_case_setcontract(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    g_current_evidence_case->contract_id = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_case_setexpected(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    g_current_evidence_case->expected_result = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_case_setexpectedpolicyguard(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    g_current_evidence_case->expected_policy_guard = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_case_setrole(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    g_current_evidence_case->case_role = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_case_setdisplayname(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    g_current_evidence_case->display_name = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_case_setsourcecase(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    g_current_evidence_case->source_case_id = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_case_settool(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    g_current_evidence_case->tool = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_case_setlevel(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    g_current_evidence_case->level = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_case_setcategory(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    g_current_evidence_case->display_category = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_case_setgroup(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    g_current_evidence_case->display_group = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_case_setmanualvisible(double value)
{
    if (!g_current_evidence_case)
        return 0.0;

    g_current_evidence_case->manual_visible = value != 0.0;
    return 0.0;
}

double CxEvidenceChain_case_setworkflow(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    const auto values = ParseEvidenceKeyValueListLocal(value);
    g_current_evidence_case->workflow_id =
        GetEvidenceKvLocal(values, "id");
    g_current_evidence_case->workflow_stage =
        GetEvidenceKvLocal(values, "stage");
    g_current_evidence_case->workflow_status =
        GetEvidenceKvLocal(values, "status", "pending");
    g_current_evidence_case->workflow_prerequisites =
        GetEvidenceKvLocal(values, "prerequisites");
    g_current_evidence_case->dataset_role =
        GetEvidenceKvLocal(values, "dataset_role");
    g_current_evidence_case->annotation_policy =
        GetEvidenceKvLocal(values, "annotation_policy");
    g_current_evidence_case->gate_policy =
        GetEvidenceKvLocal(values, "gate");
    g_current_evidence_case->parent_model_ref =
        GetEvidenceKvLocal(values, "parent");
    g_current_evidence_case->child_model_ref =
        GetEvidenceKvLocal(values, "child");
    g_current_evidence_case->workflow_stage_index =
        GetEvidenceKvIntLocal(values, "stage_index", 0);
    g_current_evidence_case->workflow_stage_count =
        GetEvidenceKvIntLocal(values, "stage_count", 0);
    g_current_evidence_case->dataset_frozen =
        GetEvidenceKvIntLocal(values, "frozen", 0) != 0;
    g_current_evidence_case->promotion_candidate =
        GetEvidenceKvIntLocal(values, "promotion_candidate", 0) != 0;
    return 0.0;
}

double CxEvidenceChain_case_setadmission(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    const auto values = ParseEvidenceKeyValueListLocal(value);
    g_current_evidence_case->admission_status =
        GetEvidenceKvLocal(values, "status", "REFERENCE_ONLY");
    g_current_evidence_case->admission_reason =
        GetEvidenceKvLocal(values, "reason");
    g_current_evidence_case->dataset_summary_ref =
        GetEvidenceKvLocal(values, "dataset_summary");
    g_current_evidence_case->training_receipt_ref =
        GetEvidenceKvLocal(values, "training_receipt");
    g_current_evidence_case->candidate_artifact_ref =
        GetEvidenceKvLocal(values, "candidate_artifact");
    g_current_evidence_case->evaluation_report_ref =
        GetEvidenceKvLocal(values, "evaluation_report");
    g_current_evidence_case->evidence_bundle_ref =
        GetEvidenceKvLocal(values, "evidence_bundle");
    g_current_evidence_case->rollback_model_ref =
        GetEvidenceKvLocal(values, "rollback_model");
    g_current_evidence_case->quality_conclusion_ref =
        GetEvidenceKvLocal(values, "quality_conclusion");
    g_current_evidence_case->training_config_ref =
        GetEvidenceKvLocal(values, "training_config");
    g_current_evidence_case->model_manifest_ref =
        GetEvidenceKvLocal(values, "model_manifest");
    g_current_evidence_case->inference_config_ref =
        GetEvidenceKvLocal(values, "inference_config");
    g_current_evidence_case->quality_policy_ref =
        GetEvidenceKvLocal(values, "quality_policy");
    g_current_evidence_case->ontology_ref =
        GetEvidenceKvLocal(values, "ontology");
    g_current_evidence_case->failure_samples_ref =
        GetEvidenceKvLocal(values, "failure_samples");
    return 0.0;
}

double CxEvidenceChain_case_adddatasetimage(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    const auto values = ParseEvidenceKeyValueListLocal(value);
    CxScriptEvidenceDatasetImage image;
    image.image_id = GetEvidenceKvLocal(values, "image_id");
    image.image_path = GetEvidenceKvLocal(values, "path");
    image.split = GetEvidenceKvLocal(values, "split", "train");
    image.label = GetEvidenceKvLocal(values, "label", "unlabeled");
    image.source = GetEvidenceKvLocal(values, "source", "evidence_dataset");
    if (!image.image_id.empty() || !image.image_path.empty())
        g_current_evidence_case->dataset_images.push_back(image);
    return 0.0;
}

double CxEvidenceChain_case_addbbox_xywh_norm(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    const auto values = ParseEvidenceKeyValueListLocal(value);
    const double cx = GetEvidenceKvDoubleLocal(values, "cx", 0.0);
    const double cy = GetEvidenceKvDoubleLocal(values, "cy", 0.0);
    const double w = GetEvidenceKvDoubleLocal(values, "w", 0.0);
    const double h = GetEvidenceKvDoubleLocal(values, "h", 0.0);

    CxScriptEvidenceAnnotation annotation;
    annotation.image_id = GetEvidenceKvLocal(values, "image_id");
    annotation.shape_kind = "RectShape";
    annotation.semantic_role = GetEvidenceKvLocal(values, "role", "bbox");
    annotation.owner_binding = GetEvidenceKvLocal(values, "binding", "label_bbox");
    annotation.label = GetEvidenceKvLocal(values, "label", "anomaly");
    annotation.class_id = GetEvidenceKvIntLocal(values, "class_id", -1);
    annotation.x0 = cx - (w * 0.5);
    annotation.y0 = cy - (h * 0.5);
    annotation.x1 = cx + (w * 0.5);
    annotation.y1 = cy + (h * 0.5);
    annotation.normalized = true;
    if (!annotation.image_id.empty() && w > 0.0 && h > 0.0)
        g_current_evidence_case->annotations.push_back(annotation);
    return 0.0;
}

double CxEvidenceChain_case_addpolygon(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    const auto values = ParseEvidenceKeyValueListLocal(value);
    const std::string encoded = GetEvidenceKvLocal(values, "points");
    std::vector<double> points;
    std::istringstream pointStream(encoded);
    std::string pair;
    while (std::getline(pointStream, pair, ';'))
    {
        const std::size_t comma = pair.find(',');
        if (comma == std::string::npos)
            continue;
        points.push_back(std::strtod(pair.substr(0, comma).c_str(), nullptr));
        points.push_back(std::strtod(pair.substr(comma + 1).c_str(), nullptr));
    }
    if (points.size() < 6 || (points.size() % 2) != 0)
        return 0.0;

    CxScriptEvidenceAnnotation annotation;
    annotation.image_id = GetEvidenceKvLocal(values, "image_id");
    annotation.shape_kind = "PolylineShape";
    annotation.semantic_role =
        GetEvidenceKvLocal(values, "role", "mask_polygon");
    annotation.owner_binding =
        GetEvidenceKvLocal(values, "binding", "label_polygon");
    annotation.label = GetEvidenceKvLocal(values, "label", "anomaly");
    annotation.class_id = GetEvidenceKvIntLocal(values, "class_id", -1);
    annotation.normalized = GetEvidenceKvIntLocal(values, "normalized", 1) != 0;
    annotation.closed = true;
    annotation.points_xy = std::move(points);
    if (!annotation.image_id.empty())
        g_current_evidence_case->annotations.push_back(std::move(annotation));
    return 0.0;
}

double CxEvidenceChain_case_clone_dataset_from(const char* value)
{
    if (!g_current_evidence_case || !value || value[0] == '\0')
        return 0.0;

    const std::string source_id = value;
    for (const CxScriptEvidenceCase& source : g_cxscript_evidence_chain.cases)
    {
        if (source.evidence_id != source_id)
            continue;
        g_current_evidence_case->dataset_images = source.dataset_images;
        g_current_evidence_case->annotations = source.annotations;
        return 0.0;
    }
    return 0.0;
}

void RegisterCxScriptEvidenceChainBindings(mu::Parser& parser)
{
    parser.DefineFun("CxEvidenceChain_reset", (mu::fun_type1)&CxEvidenceChain_reset);
    parser.DefineFun("CxEvidenceChain_setid", (mu::strfun_type1)&CxEvidenceChain_setid);
    parser.DefineFun("CxEvidenceChain_setname", (mu::strfun_type1)&CxEvidenceChain_setname);
    parser.DefineFun("CxEvidenceChain_setcatalog", (mu::strfun_type1)&CxEvidenceChain_setcatalog);
    parser.DefineFun("CxEvidenceChain_setmanifest", (mu::strfun_type1)&CxEvidenceChain_setmanifest);
    parser.DefineFun("CxEvidenceChain_setoutputroot", (mu::strfun_type1)&CxEvidenceChain_setoutputroot);
    parser.DefineFun("CxEvidenceChain_addcase", (mu::strfun_type1)&CxEvidenceChain_addcase);
    parser.DefineFun("CxEvidenceChain_case_setimage", (mu::strfun_type1)&CxEvidenceChain_case_setimage);
    parser.DefineFun("CxEvidenceChain_case_settarget", (mu::strfun_type1)&CxEvidenceChain_case_settarget);
    parser.DefineFun("CxEvidenceChain_case_setscript", (mu::strfun_type1)&CxEvidenceChain_case_setscript);
    parser.DefineFun("CxEvidenceChain_case_setparameter", (mu::strfun_type1)&CxEvidenceChain_case_setparameter);
    parser.DefineFun("CxEvidenceChain_case_setcontract", (mu::strfun_type1)&CxEvidenceChain_case_setcontract);
    parser.DefineFun("CxEvidenceChain_case_setexpected", (mu::strfun_type1)&CxEvidenceChain_case_setexpected);
    parser.DefineFun("CxEvidenceChain_case_setexpectedpolicyguard", (mu::strfun_type1)&CxEvidenceChain_case_setexpectedpolicyguard);
    parser.DefineFun("CxEvidenceChain_case_setrole", (mu::strfun_type1)&CxEvidenceChain_case_setrole);
    parser.DefineFun("CxEvidenceChain_case_setdisplayname", (mu::strfun_type1)&CxEvidenceChain_case_setdisplayname);
    parser.DefineFun("CxEvidenceChain_case_setsourcecase", (mu::strfun_type1)&CxEvidenceChain_case_setsourcecase);
    parser.DefineFun("CxEvidenceChain_case_settool", (mu::strfun_type1)&CxEvidenceChain_case_settool);
    parser.DefineFun("CxEvidenceChain_case_setlevel", (mu::strfun_type1)&CxEvidenceChain_case_setlevel);
    parser.DefineFun("CxEvidenceChain_case_setcategory", (mu::strfun_type1)&CxEvidenceChain_case_setcategory);
    parser.DefineFun("CxEvidenceChain_case_setgroup", (mu::strfun_type1)&CxEvidenceChain_case_setgroup);
    parser.DefineFun("CxEvidenceChain_case_setmanualvisible", &CxEvidenceChain_case_setmanualvisible);
    parser.DefineFun("CxEvidenceChain_case_setworkflow", (mu::strfun_type1)&CxEvidenceChain_case_setworkflow);
    parser.DefineFun("CxEvidenceChain_case_setadmission", (mu::strfun_type1)&CxEvidenceChain_case_setadmission);
    parser.DefineFun("CxEvidenceChain_case_adddatasetimage", (mu::strfun_type1)&CxEvidenceChain_case_adddatasetimage);
    parser.DefineFun("CxEvidenceChain_case_addbbox_xywh_norm", (mu::strfun_type1)&CxEvidenceChain_case_addbbox_xywh_norm);
    parser.DefineFun("CxEvidenceChain_case_addpolygon", (mu::strfun_type1)&CxEvidenceChain_case_addpolygon);
    parser.DefineFun("CxEvidenceChain_case_clone_dataset_from", (mu::strfun_type1)&CxEvidenceChain_case_clone_dataset_from);
}
