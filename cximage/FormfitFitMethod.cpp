#include "pch.h"

#include "FormfitFitMethod.h"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <limits>
#include <opencv2/core.hpp>

#if defined(CXCORE_HAS_EIGEN)
#include <Eigen/Dense>
#endif

namespace cxcore
{
namespace formfit
{

namespace
{
double ClampNonNegative(double value)
{
    return value < 0.0 ? 0.0 : value;
}

bool IsFinite(double value)
{
    return std::isfinite(value);
}

double InfiniteCost()
{
    return std::numeric_limits<double>::infinity();
}

bool HasUsableBounds(const FitVariable& variable)
{
    return IsFinite(variable.lower_bound) && IsFinite(variable.upper_bound) && variable.lower_bound < variable.upper_bound;
}

double ClampToVariableBounds(const FitVariable& variable, double value)
{
    if (!HasUsableBounds(variable))
    {
        return value;
    }
    return std::min(std::max(value, variable.lower_bound), variable.upper_bound);
}

double ResidualScale(const FitProblem& problem, const FitStage& stage, const FitResidual& residual)
{
    if (!IsFinite(residual.value) || !IsFinite(residual.weight) || !IsFinite(residual.tolerance) ||
        !IsFinite(problem.global_weight_scale) || !IsFinite(problem.global_tolerance_scale) ||
        !IsFinite(stage.stage_weight) || !IsFinite(stage.tolerance_scale))
    {
        return 0.0;
    }
    const double weight = ClampNonNegative(residual.weight) *
                          std::max(0.0, problem.global_weight_scale) *
                          std::max(0.0, stage.stage_weight);
    if (weight <= 0.0)
    {
        return 0.0;
    }
    const double tolerance_scale = std::max(1e-9, problem.global_tolerance_scale) *
                                   std::max(1e-9, stage.tolerance_scale);
    const double denominator = std::max(1e-9,
        residual.tolerance > 0.0 ? residual.tolerance * tolerance_scale : tolerance_scale);
    return std::sqrt(weight) / denominator;
}

int FindVariableIndex(const std::vector<const FitVariable*>& variables, const std::string& variable_name)
{
    for (size_t i = 0; i < variables.size(); ++i)
    {
        if (variables[i] && variables[i]->name == variable_name)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

FitVariable* FindVariable(FitProblem& problem, const std::string& variable_name)
{
    for (FitVariable& variable : problem.variables)
    {
        if (variable.name == variable_name)
        {
            return &variable;
        }
    }
    return nullptr;
}

FitResidual* FindResidual(FitProblem& problem, const std::string& residual_name)
{
    for (FitResidual& residual : problem.residuals)
    {
        if (residual.name == residual_name)
        {
            return &residual;
        }
    }
    return nullptr;
}

FitStage* FindStage(FitProblem& problem, const std::string& stage_id)
{
    for (FitStage& stage : problem.stages)
    {
        if (stage.stage_id == stage_id)
        {
            return &stage;
        }
    }
    return nullptr;
}

bool SolveLeastSquares(const std::vector<std::vector<double>>& rows, const std::vector<double>& rhs, std::vector<double>& solution)
{
    solution.clear();
    if (rows.empty() || rhs.size() != rows.size() || rows.front().empty()) return false;
    const int row_count = static_cast<int>(rows.size());
    const int col_count = static_cast<int>(rows.front().size());
    for (int row = 0; row < row_count; ++row)
    {
        if (static_cast<int>(rows[static_cast<size_t>(row)].size()) != col_count) return false;
        if (!IsFinite(rhs[static_cast<size_t>(row)])) return false;
        for (int col = 0; col < col_count; ++col)
        {
            if (!IsFinite(rows[static_cast<size_t>(row)][static_cast<size_t>(col)])) return false;
        }
    }
#if defined(CXCORE_HAS_EIGEN)
    Eigen::MatrixXd eigen_matrix(row_count, col_count);
    Eigen::VectorXd eigen_rhs(row_count);
    for (int row = 0; row < row_count; ++row)
    {
        for (int col = 0; col < col_count; ++col) eigen_matrix(row, col) = rows[static_cast<size_t>(row)][static_cast<size_t>(col)];
        eigen_rhs(row) = rhs[static_cast<size_t>(row)];
    }
    Eigen::ColPivHouseholderQR<Eigen::MatrixXd> qr(eigen_matrix);
    if (qr.rank() > 0)
    {
        const Eigen::VectorXd eigen_solution = qr.solve(eigen_rhs);
        if (eigen_solution.size() == col_count && eigen_solution.allFinite())
        {
            solution.assign(eigen_solution.data(), eigen_solution.data() + eigen_solution.size());
            return true;
        }
    }
#endif
    cv::Mat matrix(row_count, col_count, CV_64F);
    cv::Mat vector(row_count, 1, CV_64F);
    for (int row = 0; row < row_count; ++row)
    {
        for (int col = 0; col < col_count; ++col) matrix.at<double>(row, col) = rows[static_cast<size_t>(row)][static_cast<size_t>(col)];
        vector.at<double>(row, 0) = rhs[static_cast<size_t>(row)];
    }
    cv::Mat solved;
    if (!cv::solve(matrix, vector, solved, cv::DECOMP_SVD)) return false;
    solution.assign(static_cast<size_t>(col_count), 0.0);
    for (int i = 0; i < col_count; ++i)
    {
        const double value = solved.at<double>(i, 0);
        if (!IsFinite(value)) return false;
        solution[static_cast<size_t>(i)] = value;
    }
    return true;
}

bool StageContainsResidual(const FitStage& stage, const FitResidual& residual)
{
    if (stage.active_relation_ids.empty() && stage.active_constraint_ids.empty())
    {
        return true;
    }

    return std::find(stage.active_relation_ids.begin(), stage.active_relation_ids.end(), residual.name) != stage.active_relation_ids.end() ||
           std::find(stage.active_constraint_ids.begin(), stage.active_constraint_ids.end(), residual.name) != stage.active_constraint_ids.end();
}

double ComputeStageWeightedResidualCost(const FitProblem& problem, const FitStage& stage)
{
    double cost = 0.0;
    const double global_weight = std::max(0.0, problem.global_weight_scale);
    const double global_tolerance = std::max(1e-9, problem.global_tolerance_scale * std::max(1e-9, stage.tolerance_scale));
    for (const FitResidual& residual : problem.residuals)
    {
        if (!StageContainsResidual(stage, residual))
        {
            continue;
        }
        const double weight = ClampNonNegative(residual.weight) * global_weight * std::max(0.0, stage.stage_weight);
        const double denominator = std::max(1e-9, residual.tolerance > 0.0 ? residual.tolerance * global_tolerance : global_tolerance);
        const double scaled = residual.value / denominator;
        cost += weight * scaled * scaled;
    }
    return cost;
}

double ComputeStageMaxResidualValue(const FitProblem& problem, const FitStage& stage)
{
    double max_value = 0.0;
    for (const FitResidual& residual : problem.residuals)
    {
        if (!StageContainsResidual(stage, residual))
        {
            continue;
        }
        max_value = std::max(max_value, std::fabs(residual.value));
    }
    return max_value;
}

bool AppendLinearizedResidualRow(const FitProblem& problem,
                                 const FitStage& stage,
                                 const FitResidual& residual,
                                 const std::vector<const FitVariable*>& variables,
                                 std::vector<std::vector<double>>& rows,
                                 std::vector<double>& rhs)
{
    if (!StageContainsResidual(stage, residual) || residual.jacobian_terms.empty() || !IsFinite(residual.value))
    {
        return false;
    }
    const double scale = ResidualScale(problem, stage, residual);
    if (scale <= 0.0 || !IsFinite(scale))
    {
        return false;
    }
    std::vector<double> row(variables.size(), 0.0);
    bool used = false;
    for (const FitJacobianTerm& term : residual.jacobian_terms)
    {
        const int index = FindVariableIndex(variables, term.variable_name);
        if (index < 0 || !IsFinite(term.coefficient))
        {
            continue;
        }
        row[static_cast<size_t>(index)] += term.coefficient * scale;
        used = true;
    }
    if (!used) return false;
    rows.push_back(row);
    rhs.push_back(-residual.value * scale);
    return true;
}

void BuildLinearizedLeastSquaresSystem(const FitProblem& problem,
                                       const FitStage& stage,
                                       const std::vector<const FitVariable*>& variables,
                                       std::vector<std::vector<double>>& rows,
                                       std::vector<double>& rhs)
{
    rows.clear();
    rhs.clear();
    for (const FitResidual& residual : problem.residuals)
    {
        AppendLinearizedResidualRow(problem, stage, residual, variables, rows, rhs);
    }
}

std::vector<double> ScaleDelta(const std::vector<double>& delta, double scale)
{
    std::vector<double> scaled = delta;
    for (double& value : scaled)
    {
        value *= scale;
    }
    return scaled;
}

double DeltaNorm(const std::vector<double>& delta)
{
    double sum = 0.0;
    for (double value : delta)
    {
        if (!IsFinite(value)) return InfiniteCost();
        sum += value * value;
    }
    return std::sqrt(sum);
}

void ApplyLinearizedDelta(FitProblem& problem,
                          const std::vector<const FitVariable*>& variables,
                          const std::vector<double>& delta)
{
    for (FitVariable& variable : problem.variables)
    {
        const int index = FindVariableIndex(variables, variable.name);
        if (variable.locked || index < 0 || static_cast<size_t>(index) >= delta.size())
        {
            continue;
        }
        const double next_value = variable.value + delta[static_cast<size_t>(index)];
        if (IsFinite(next_value))
        {
            variable.value = ClampToVariableBounds(variable, next_value);
        }
    }
}

void UpdateLinearizedResiduals(FitProblem& problem,
                               const FitStage& stage,
                               const std::vector<const FitVariable*>& variables,
                               const std::vector<double>& delta)
{
    for (FitResidual& residual : problem.residuals)
    {
        if (!StageContainsResidual(stage, residual) || residual.jacobian_terms.empty())
        {
            continue;
        }
        double correction = 0.0;
        for (const FitJacobianTerm& term : residual.jacobian_terms)
        {
            const int index = FindVariableIndex(variables, term.variable_name);
            if (index < 0 || static_cast<size_t>(index) >= delta.size() || !IsFinite(term.coefficient))
            {
                continue;
            }
            correction += term.coefficient * delta[static_cast<size_t>(index)];
        }
        if (IsFinite(correction))
        {
            residual.value += correction;
        }
    }
}
}

CompositeFitter::CompositeFitter(std::shared_ptr<IOptimizerBackend> backend)
    : m_backend(std::move(backend))
{
}

FitResult CompositeFitter::SolveComposite(const FitProblem& problem)
{
    if (!m_backend)
    {
        return MakeEmptyFitResult(problem, "no_backend", "optimizer_backend_missing");
    }

    if (problem.stages.empty())
    {
        return MakeEmptyFitResult(problem, "no_stage", "fit_stage_missing");
    }

    FitResult final_result = MakeEmptyFitResult(problem, "empty", "no_stage_executed");
    FitProblem stage_problem = problem;
    final_result.solved_variables = stage_problem.variables;
    final_result.residuals = stage_problem.residuals;

    for (const FitStage& stage : problem.stages)
    {
        if (!stage.enabled)
        {
            continue;
        }

        FitResult stage_result = m_backend->Optimize(stage_problem, stage);

        FitStageSummary stage_summary;
        stage_summary.stage_id = stage.stage_id;
        stage_summary.stage_name = stage.stage_name;
        stage_summary.method_type = stage.method_type;
        stage_summary.order_index = stage.order_index;
        stage_summary.stage_weight = stage.stage_weight;
        stage_summary.tolerance_scale = stage.tolerance_scale;
        stage_summary.enabled = stage.enabled;
        stage_summary.stage_cost = stage_result.final_cost;
        stage_summary.stage_max_residual = stage_result.max_residual;
        stage_summary.iterations = stage_result.iterations;
        if (!stage_result.success)
        {
            stage_result.stage_summaries = final_result.stage_summaries;
            stage_result.stage_summaries.push_back(stage_summary);
            return stage_result;
        }

        const std::vector<FitStageSummary> accumulated_summaries = final_result.stage_summaries;
        final_result = stage_result;
        stage_problem.variables = stage_result.solved_variables;
        stage_problem.residuals = stage_result.residuals;
        final_result.stage_summaries = accumulated_summaries;
        final_result.stage_summaries.push_back(stage_summary);
    }

    if (final_result.status.empty())
    {
        final_result.status = "ok";
    }
    if (final_result.failure_mode.empty())
    {
        final_result.failure_mode = "none";
    }
    return final_result;
}

FitResult PassthroughOptimizerBackend::Optimize(const FitProblem& problem, const FitStage& stage)
{
    FitResult result;
    result.success = true;
    result.status = "ok";
    result.failure_mode = "none";
    result.final_cost = ComputeStageWeightedResidualCost(problem, stage);
    result.max_residual = ComputeStageMaxResidualValue(problem, stage);
    result.iterations = std::max(1, stage.max_iterations);
    result.solved_variables = problem.variables;
    result.residuals = problem.residuals;
    return result;
}

FitResult LinearizedLeastSquaresOptimizerBackend::Optimize(const FitProblem& problem, const FitStage& stage)
{
    FitProblem current = problem;
    double current_cost = ComputeStageWeightedResidualCost(current, stage);
    if (!IsFinite(current_cost))
    {
        return MakeEmptyFitResult(problem, "invalid_residual", "linearized_least_squares_non_finite_cost");
    }

    const int max_iterations = std::max(1, stage.max_iterations);
    int completed_iterations = 0;
    for (int iteration = 0; iteration < max_iterations; ++iteration)
    {
        std::vector<const FitVariable*> active_variables;
        for (const FitVariable& variable : current.variables)
        {
            if (!variable.locked && IsFinite(variable.value))
            {
                active_variables.push_back(&variable);
            }
        }
        if (active_variables.empty())
        {
            break;
        }

        std::vector<std::vector<double>> rows;
        std::vector<double> rhs;
        BuildLinearizedLeastSquaresSystem(current, stage, active_variables, rows, rhs);
        if (rows.empty())
        {
            break;
        }

        std::vector<double> delta;
        if (!SolveLeastSquares(rows, rhs, delta) || delta.size() != active_variables.size())
        {
            if (completed_iterations == 0)
            {
                return MakeEmptyFitResult(problem, "linear_solver_failed", "linearized_least_squares_singular");
            }
            break;
        }

        const double raw_delta_norm = DeltaNorm(delta);
        if (!IsFinite(raw_delta_norm))
        {
            if (completed_iterations == 0)
            {
                return MakeEmptyFitResult(problem, "linear_solver_failed", "linearized_least_squares_non_finite_delta");
            }
            break;
        }

        bool accepted = false;
        for (int damping_attempt = 0; damping_attempt < 8; ++damping_attempt)
        {
            const double damping = 1.0 / static_cast<double>(1 << damping_attempt);
            const std::vector<double> damped_delta = ScaleDelta(delta, damping);
            FitProblem candidate = current;
            ApplyLinearizedDelta(candidate, active_variables, damped_delta);
            UpdateLinearizedResiduals(candidate, stage, active_variables, damped_delta);
            const double candidate_cost = ComputeStageWeightedResidualCost(candidate, stage);
            if (!IsFinite(candidate_cost))
            {
                continue;
            }
            if (candidate_cost <= current_cost || std::fabs(candidate_cost - current_cost) <= 1e-12)
            {
                const double improvement = current_cost - candidate_cost;
                current = candidate;
                current_cost = candidate_cost;
                ++completed_iterations;
                accepted = true;
                if (raw_delta_norm * damping <= 1e-9 || std::fabs(improvement) <= 1e-12)
                {
                    iteration = max_iterations;
                }
                break;
            }
        }

        if (!accepted)
        {
            break;
        }
    }

    if (completed_iterations == 0)
    {
        return PassthroughOptimizerBackend().Optimize(problem, stage);
    }

    FitResult result;
    result.success = true;
    result.status = "ok";
    result.failure_mode = "none";
    result.final_cost = ComputeStageWeightedResidualCost(current, stage);
    result.max_residual = ComputeStageMaxResidualValue(current, stage);
    result.iterations = completed_iterations;
    result.solved_variables = current.variables;
    result.residuals = current.residuals;
    return result;
}

DefaultMethodExecutor::DefaultMethodExecutor(std::shared_ptr<ICompositeFitter> fitter)
    : m_fitter(std::move(fitter))
{
}

FitMethodResult DefaultMethodExecutor::Execute(const FitTaskSpec& task,
                                               const FitMethodDescriptor& method,
                                               const FitOperationConfig& operation_config,
                                               FitProblem problem)
{
    (void)task;
    FitOperationController::Apply(operation_config, problem);

    const auto begin = std::chrono::steady_clock::now();
    FitResult fit_result = m_fitter
        ? m_fitter->SolveComposite(problem)
        : MakeEmptyFitResult(problem, "no_fitter", "composite_fitter_missing");
    const auto end = std::chrono::steady_clock::now();
    const double elapsed_ms = std::chrono::duration<double, std::milli>(end - begin).count();
    return MakeFitMethodResult(method, fit_result, elapsed_ms);
}

void FitOperationController::Apply(const FitOperationConfig& config, FitProblem& problem)
{
    if (config.has_selected_method)
    {
        for (FitStage& stage : problem.stages)
        {
            if (config.selected_stage_id.empty() || stage.stage_id == config.selected_stage_id)
            {
                stage.method_type = config.selected_method;
            }
        }
    }

    if (config.has_global_tolerance_scale)
    {
        problem.global_tolerance_scale = std::max(1e-9, config.global_tolerance_scale);
    }
    if (config.has_global_weight_scale)
    {
        problem.global_weight_scale = std::max(0.0, config.global_weight_scale);
    }

    for (const FitVariableSetting& setting : config.variable_settings)
    {
        FitVariable* variable = FindVariable(problem, setting.variable_name);
        if (!variable)
        {
            continue;
        }
        if (setting.has_value)
        {
            variable->value = setting.value;
        }
        if (setting.has_lower_bound)
        {
            variable->lower_bound = setting.lower_bound;
        }
        if (setting.has_upper_bound)
        {
            variable->upper_bound = setting.upper_bound;
        }
        if (setting.has_weight)
        {
            variable->weight = std::max(0.0, setting.weight);
        }
        if (setting.has_locked)
        {
            variable->locked = setting.locked;
        }
    }

    for (const FitResidualSetting& setting : config.residual_settings)
    {
        FitResidual* residual = FindResidual(problem, setting.residual_name);
        if (!residual)
        {
            continue;
        }
        if (setting.has_weight)
        {
            residual->weight = std::max(0.0, setting.weight);
        }
        if (setting.has_tolerance)
        {
            residual->tolerance = std::max(0.0, setting.tolerance);
        }
    }

    for (const FitStageSetting& setting : config.stage_settings)
    {
        FitStage* stage = FindStage(problem, setting.stage_id);
        if (!stage)
        {
            continue;
        }
        if (setting.has_method)
        {
            stage->method_type = setting.method_type;
        }
        if (setting.has_max_iterations)
        {
            stage->max_iterations = std::max(0, setting.max_iterations);
        }
        if (setting.has_order_index)
        {
            stage->order_index = std::max(0, setting.order_index);
        }
        if (setting.has_stage_weight)
        {
            stage->stage_weight = std::max(0.0, setting.stage_weight);
        }
        if (setting.has_tolerance_scale)
        {
            stage->tolerance_scale = std::max(1e-9, setting.tolerance_scale);
        }
        if (setting.has_enabled)
        {
            stage->enabled = setting.enabled;
        }
    }

    std::stable_sort(problem.stages.begin(),
                     problem.stages.end(),
                     [](const FitStage& lhs, const FitStage& rhs)
                     {
                         return lhs.order_index < rhs.order_index;
                     });
}

FitStage MakeFitStage(const char* stage_id,
                      const char* stage_name,
                      FitMethodType method_type,
                      int max_iterations)
{
    FitStage stage;
    stage.stage_id = stage_id ? stage_id : "";
    stage.stage_name = stage_name ? stage_name : "";
    stage.method_type = method_type;
    stage.max_iterations = std::max(0, max_iterations);
    stage.order_index = 0;
    stage.stage_weight = 1.0;
    stage.tolerance_scale = 1.0;
    stage.enabled = true;
    return stage;
}

FitTaskSpec MakeFitTaskSpec(const char* task_id, FitTaskType task_type)
{
    FitTaskSpec task;
    task.task_id = task_id ? task_id : "";
    task.task_type = task_type;
    return task;
}

FitMethodDescriptor MakeFitMethodDescriptor(const char* method_name,
                                            FitMethodCategory method_category,
                                            FitMethodType method_type)
{
    FitMethodDescriptor descriptor;
    descriptor.method_name = method_name ? method_name : "";
    descriptor.method_category = method_category;
    descriptor.supported_input_types.push_back(FitTaskType::ElementFit);
    descriptor.supported_input_types.push_back(FitTaskType::GaugeFit);
    descriptor.supported_output_types.push_back("variables");
    descriptor.supported_output_types.push_back("residuals");

    FitParameterSpec method_parameter;
    method_parameter.name = "method_type";
    method_parameter.default_value = static_cast<double>(method_type);
    descriptor.parameters.push_back(method_parameter);
    return descriptor;
}

FitResult MakeEmptyFitResult(const FitProblem& problem,
                             const char* status,
                             const char* failure_mode)
{
    FitResult result;
    result.success = false;
    result.status = status ? status : "";
    result.failure_mode = failure_mode ? failure_mode : "";
    result.final_cost = ComputeWeightedResidualCost(problem);
    result.max_residual = ComputeMaxResidualValue(problem);
    result.iterations = 0;
    result.solved_variables = problem.variables;
    result.residuals = problem.residuals;
    return result;
}

FitMethodResult MakeFitMethodResult(const FitMethodDescriptor& method,
                                    const FitResult& fit_result,
                                    double elapsed_ms)
{
    FitMethodResult result;
    result.fit_result = fit_result;
    result.method_name = method.method_name;
    result.valid = fit_result.success;
    result.iterations = fit_result.iterations;
    result.elapsed_ms = elapsed_ms;
    result.residuals = fit_result.residuals;

    FitResidual final_cost_metric;
    final_cost_metric.name = "final_cost";
    final_cost_metric.residual_type = ResidualType::Penalty;
    final_cost_metric.value = fit_result.final_cost;
    result.quality_metrics.push_back(final_cost_metric);

    FitResidual max_residual_metric;
    max_residual_metric.name = "max_residual";
    max_residual_metric.residual_type = ResidualType::Penalty;
    max_residual_metric.value = fit_result.max_residual;
    result.quality_metrics.push_back(max_residual_metric);

    for (size_t i = 0; i < fit_result.stage_summaries.size(); ++i)
    {
        const FitStageSummary& summary = fit_result.stage_summaries[i];

        FitResidual stage_cost_metric;
        stage_cost_metric.name = summary.stage_id + ".stage_cost";
        stage_cost_metric.residual_type = ResidualType::Penalty;
        stage_cost_metric.value = summary.stage_cost;
        stage_cost_metric.weight = summary.stage_weight;
        stage_cost_metric.tolerance = summary.tolerance_scale;
        result.quality_metrics.push_back(stage_cost_metric);

        FitResidual stage_max_metric;
        stage_max_metric.name = summary.stage_id + ".stage_max_residual";
        stage_max_metric.residual_type = ResidualType::Penalty;
        stage_max_metric.value = summary.stage_max_residual;
        stage_max_metric.weight = summary.stage_weight;
        stage_max_metric.tolerance = summary.tolerance_scale;
        result.quality_metrics.push_back(stage_max_metric);
    }
    return result;
}

double ComputeWeightedResidualCost(const FitProblem& problem)
{
    double cost = 0.0;
    for (const FitResidual& residual : problem.residuals)
    {
        if (!IsFinite(residual.value) || !IsFinite(residual.weight) || !IsFinite(residual.tolerance) ||
            !IsFinite(problem.global_weight_scale) || !IsFinite(problem.global_tolerance_scale))
        {
            return InfiniteCost();
        }
        const double weight = ClampNonNegative(residual.weight) * std::max(0.0, problem.global_weight_scale);
        const double scaled = residual.value / std::max(1e-9, residual.tolerance > 0.0 ? residual.tolerance * problem.global_tolerance_scale : problem.global_tolerance_scale);
        cost += weight * scaled * scaled;
        if (!IsFinite(cost)) return InfiniteCost();
    }
    return cost;
}

double ComputeMaxResidualValue(const FitProblem& problem)
{
    double max_value = 0.0;
    for (const FitResidual& residual : problem.residuals)
    {
        if (!IsFinite(residual.value))
        {
            return InfiniteCost();
        }
        max_value = std::max(max_value, std::fabs(residual.value));
    }
    return max_value;
}

double EvaluateFitTask(const FitTaskSpec& task, const FitMethodResult& result)
{
    const double task_scale = task.reference_residuals.empty() ? 1.0 : static_cast<double>(task.reference_residuals.size());
    const double relation_scale = task.relation_ids.empty() ? 1.0 : static_cast<double>(task.relation_ids.size());
    const double stage_scale = task.preferred_stage_ids.empty() ? 1.0 : static_cast<double>(task.preferred_stage_ids.size());
    const double penalty = result.valid ? 0.0 : 1e6;
    return result.fit_result.final_cost +
           result.fit_result.max_residual * task_scale +
           0.1 * relation_scale +
           0.05 * stage_scale +
           penalty;
}

const char* FitMethodTypeName(FitMethodType method_type)
{
    switch (method_type)
    {
    case FitMethodType::LeastSquares:
        return "least_squares";
    case FitMethodType::WeightedLeastSquares:
        return "weighted_least_squares";
    case FitMethodType::Chebyshev:
        return "chebyshev";
    case FitMethodType::Robust:
        return "robust";
    case FitMethodType::Constrained:
        return "constrained";
    default:
        return "unknown";
    }
}

const char* FitTaskTypeName(FitTaskType task_type)
{
    switch (task_type)
    {
    case FitTaskType::ElementFit:
        return "element_fit";
    case FitTaskType::GaugeFit:
        return "gauge_fit";
    case FitTaskType::StructureFit:
        return "structure_fit";
    case FitTaskType::ComparativeFit:
        return "comparative_fit";
    case FitTaskType::Unknown:
    default:
        return "unknown";
    }
}

const char* FitMethodCategoryName(FitMethodCategory method_category)
{
    switch (method_category)
    {
    case FitMethodCategory::LeastSquaresFamily:
        return "least_squares_family";
    case FitMethodCategory::RobustFamily:
        return "robust_family";
    case FitMethodCategory::ConstrainedFamily:
        return "constrained_family";
    case FitMethodCategory::CompositeFamily:
        return "composite_family";
    case FitMethodCategory::Unknown:
    default:
        return "unknown";
    }
}

const char* FitOperationKindName(FitOperationKind operation_kind)
{
    switch (operation_kind)
    {
    case FitOperationKind::SelectMethod:
        return "select_method";
    case FitOperationKind::SetVariable:
        return "set_variable";
    case FitOperationKind::SetResidual:
        return "set_residual";
    case FitOperationKind::SetStage:
        return "set_stage";
    case FitOperationKind::SetGlobalScale:
        return "set_global_scale";
    case FitOperationKind::LockVariable:
        return "lock_variable";
    case FitOperationKind::UnlockVariable:
        return "unlock_variable";
    case FitOperationKind::Unknown:
    default:
        return "unknown";
    }
}

} // namespace formfit
} // namespace cxcore
