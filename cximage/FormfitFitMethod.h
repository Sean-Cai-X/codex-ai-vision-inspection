#ifndef CXCORE_CORE_FORMFITFITMETHOD_H
#define CXCORE_CORE_FORMFITFITMETHOD_H

#include <memory>
#include <string>
#include <vector>

namespace cxcore
{
namespace formfit
{

enum class FitMethodType
{
    LeastSquares = 0,
    WeightedLeastSquares,
    Chebyshev,
    Robust,
    Constrained
};

enum class ResidualType
{
    Element = 0,
    Relation,
    Constraint,
    Penalty
};

enum class FitTaskType
{
    Unknown = 0,
    ElementFit,
    GaugeFit,
    StructureFit,
    ComparativeFit
};

enum class FitMethodCategory
{
    Unknown = 0,
    LeastSquaresFamily,
    RobustFamily,
    ConstrainedFamily,
    CompositeFamily
};

enum class FitOperationKind
{
    Unknown = 0,
    SelectMethod,
    SetVariable,
    SetResidual,
    SetStage,
    SetGlobalScale,
    LockVariable,
    UnlockVariable
};

struct FitVariable
{
    std::string name;
    double value = 0.0;
    double lower_bound = 0.0;
    double upper_bound = 0.0;
    double weight = 1.0;
    bool locked = false;
};

struct FitJacobianTerm
{
    std::string variable_name;
    double coefficient = 0.0;
};

struct FitResidual
{
    std::string name;
    ResidualType residual_type = ResidualType::Element;
    double value = 0.0;
    double weight = 1.0;
    double tolerance = 0.0;
    std::vector<FitJacobianTerm> jacobian_terms;
};

struct FitParameterSpec
{
    std::string name;
    double default_value = 0.0;
    double lower_bound = 0.0;
    double upper_bound = 0.0;
    bool bounded = false;
};

struct FitStage
{
    std::string stage_id;
    std::string stage_name;
    FitMethodType method_type = FitMethodType::LeastSquares;
    std::vector<std::string> active_element_ids;
    std::vector<std::string> active_relation_ids;
    std::vector<std::string> active_constraint_ids;
    int max_iterations = 0;
    int order_index = 0;
    double stage_weight = 1.0;
    double tolerance_scale = 1.0;
    bool enabled = true;
};

struct FitProblem
{
    std::string problem_id;
    std::vector<std::string> element_ids;
    std::vector<std::string> relation_ids;
    std::vector<std::string> constraint_ids;
    std::vector<FitVariable> variables;
    std::vector<FitResidual> residuals;
    std::vector<FitStage> stages;
    double global_tolerance_scale = 1.0;
    double global_weight_scale = 1.0;
};

struct FitStageSummary
{
    std::string stage_id;
    std::string stage_name;
    FitMethodType method_type = FitMethodType::LeastSquares;
    int order_index = 0;
    double stage_weight = 1.0;
    double tolerance_scale = 1.0;
    bool enabled = true;
    double stage_cost = 0.0;
    double stage_max_residual = 0.0;
    int iterations = 0;
};

struct FitResult
{
    bool success = false;
    std::string status;
    double final_cost = 0.0;
    double max_residual = 0.0;
    int iterations = 0;
    std::vector<FitVariable> solved_variables;
    std::vector<FitResidual> residuals;
    std::vector<FitStageSummary> stage_summaries;
    std::string failure_mode;
};

struct FitTaskSpec
{
    std::string task_id;
    FitTaskType task_type = FitTaskType::Unknown;
    std::vector<std::string> input_element_ids;
    std::vector<std::string> gauge_ids;
    std::vector<std::string> relation_ids;
    std::vector<std::string> constraint_ids;
    std::vector<std::string> preferred_stage_ids;
    std::vector<FitResidual> reference_residuals;
    std::vector<std::string> output_target_ids;
};

struct FitMethodDescriptor
{
    std::string method_name;
    FitMethodCategory method_category = FitMethodCategory::Unknown;
    std::vector<FitTaskType> supported_input_types;
    std::vector<std::string> supported_output_types;
    std::vector<FitParameterSpec> parameters;
};


struct FitMethodResult
{
    std::string method_name;
    bool valid = false;
    FitResult fit_result;
    double final_cost = 0.0;
    double max_residual = 0.0;
    int iterations = 0;
    double elapsed_ms = 0.0;
    std::vector<FitResidual> residuals;
    std::vector<FitResidual> quality_metrics;
};

struct FitCandidate
{
    std::string candidate_id;
    std::string source_stage_id;
    FitMethodType source_method_type = FitMethodType::LeastSquares;
    bool valid = false;
    double final_cost = 0.0;
    double max_residual = 0.0;
    int iterations = 0;
    double elapsed_ms = 0.0;
};

struct FitCandidateRankItem
{
    std::string candidate_id;
    int rank_index = 0;
    double score = 0.0;
};

struct FitCandidateGroup
{
    std::string group_id;
    std::string task_id;
    std::string problem_id;
    std::string config_id;
    std::vector<FitCandidate> candidates;
    int candidate_count = 0;
    std::vector<std::string> ordered_candidate_ids;
    int ordered_candidate_count = 0;
    std::vector<FitCandidateRankItem> baseline_rank;
    int ranked_candidate_count = 0;
    std::string best_candidate_id;
    double group_score = 0.0;
    double rank_delta = 0.0;
    bool fixed_point_converged = false;
    int fixed_point_iterations = 0;
    double fixed_point_delta = 0.0;
    std::string convergence_status;
};

struct FitVariableSetting
{
    std::string variable_name;
    bool has_value = false;
    double value = 0.0;
    bool has_lower_bound = false;
    double lower_bound = 0.0;
    bool has_upper_bound = false;
    double upper_bound = 0.0;
    bool has_weight = false;
    double weight = 1.0;
    bool has_locked = false;
    bool locked = false;
};

struct FitResidualSetting
{
    std::string residual_name;
    bool has_weight = false;
    double weight = 1.0;
    bool has_tolerance = false;
    double tolerance = 0.0;
};

struct FitStageSetting
{
    std::string stage_id;
    bool has_method = false;
    FitMethodType method_type = FitMethodType::LeastSquares;
    bool has_max_iterations = false;
    int max_iterations = 0;
    bool has_order_index = false;
    int order_index = 0;
    bool has_stage_weight = false;
    double stage_weight = 1.0;
    bool has_tolerance_scale = false;
    double tolerance_scale = 1.0;
    bool has_enabled = false;
    bool enabled = true;
};

struct FitOperationConfig
{
    std::string config_id;
    FitOperationKind operation_kind = FitOperationKind::Unknown;
    std::string selected_stage_id;
    bool has_selected_method = false;
    FitMethodType selected_method = FitMethodType::LeastSquares;
    bool has_global_tolerance_scale = false;
    double global_tolerance_scale = 1.0;
    bool has_global_weight_scale = false;
    double global_weight_scale = 1.0;
    std::vector<FitVariableSetting> variable_settings;
    std::vector<FitResidualSetting> residual_settings;
    std::vector<FitStageSetting> stage_settings;
};

class IOptimizerBackend
{
public:
    virtual ~IOptimizerBackend() = default;
    virtual FitResult Optimize(const FitProblem& problem, const FitStage& stage) = 0;
};

class IFitter
{
public:
    virtual ~IFitter() = default;
    virtual FitResult SolveStage(const FitProblem& problem, const FitStage& stage) = 0;
};

class ICompositeFitter
{
public:
    virtual ~ICompositeFitter() = default;
    virtual FitResult SolveComposite(const FitProblem& problem) = 0;
};

class IMethodExecutor
{
public:
    virtual ~IMethodExecutor() = default;
    virtual FitMethodResult Execute(const FitTaskSpec& task,
                                    const FitMethodDescriptor& method,
                                    const FitOperationConfig& operation_config,
                                    FitProblem problem) = 0;
};

class CompositeFitter : public ICompositeFitter
{
public:
    explicit CompositeFitter(std::shared_ptr<IOptimizerBackend> backend);

    FitResult SolveComposite(const FitProblem& problem) override;

private:
    std::shared_ptr<IOptimizerBackend> m_backend;
};

class PassthroughOptimizerBackend : public IOptimizerBackend
{
public:
    FitResult Optimize(const FitProblem& problem, const FitStage& stage) override;
};

class LinearizedLeastSquaresOptimizerBackend : public IOptimizerBackend
{
public:
    FitResult Optimize(const FitProblem& problem, const FitStage& stage) override;
};

class DefaultMethodExecutor : public IMethodExecutor
{
public:
    explicit DefaultMethodExecutor(std::shared_ptr<ICompositeFitter> fitter);

    FitMethodResult Execute(const FitTaskSpec& task,
                            const FitMethodDescriptor& method,
                            const FitOperationConfig& operation_config,
                            FitProblem problem) override;

private:
    std::shared_ptr<ICompositeFitter> m_fitter;
};

class FitOperationController
{
public:
    static void Apply(const FitOperationConfig& config, FitProblem& problem);
};

FitStage MakeFitStage(const char* stage_id,
                      const char* stage_name,
                      FitMethodType method_type,
                      int max_iterations);
FitTaskSpec MakeFitTaskSpec(const char* task_id, FitTaskType task_type);
FitMethodDescriptor MakeFitMethodDescriptor(const char* method_name,
                                            FitMethodCategory method_category,
                                            FitMethodType method_type);

FitResult MakeEmptyFitResult(const FitProblem& problem,
                             const char* status,
                             const char* failure_mode);
FitMethodResult MakeFitMethodResult(const FitMethodDescriptor& method,
                                    const FitResult& fit_result,
                                    double elapsed_ms);

double ComputeWeightedResidualCost(const FitProblem& problem);
double ComputeMaxResidualValue(const FitProblem& problem);
double EvaluateFitTask(const FitTaskSpec& task, const FitMethodResult& result);
const char* FitMethodTypeName(FitMethodType method_type);
const char* FitTaskTypeName(FitTaskType task_type);
const char* FitMethodCategoryName(FitMethodCategory method_category);
const char* FitOperationKindName(FitOperationKind operation_kind);

} // namespace formfit
} // namespace cxcore

#endif
