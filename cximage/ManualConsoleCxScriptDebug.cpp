#include "ManualConsoleCxScriptDebug.h"
#include "ManualConsoleUtils.h"
#include "ManualConsoleRuntimeView.h"
#include "ManualConsoleFindCircleDebug.h"
#include "ManualConsoleFindLineDebug.h"
#include "CxScriptRunTraceRuntime.h"

#include <sstream>
#include <fstream>
#include <cmath>

std::unordered_map<ManualTestContext*, DebugCximageRuntime> g_cximageRuntime;

DebugCximageRuntime& CxRuntime(ManualTestContext& context)
{
    return g_cximageRuntime[&context];
}

bool IsBraceOpenLine(const std::string& line)
{
    return TrimLine(line) == "{";
}

bool IsBraceCloseLine(const std::string& line)
{
    return TrimLine(line) == "}";
}

bool IsIfLine(const std::string& line)
{
    const std::string s = TrimLine(line);
    return s.rfind("if", 0) == 0 &&
        s.find('(') != std::string::npos &&
        s.rfind(')') != std::string::npos;
}

std::string ExtractIfCondition(const std::string& line)
{
    const std::size_t l = line.find('(');
    const std::size_t r = line.rfind(')');

    if (l == std::string::npos || r == std::string::npos || r <= l)
        return {};

    return TrimLine(line.substr(l + 1, r - l - 1));
}

ParsedMethodCall ParseMethodCall(const std::string& statement)
{
    ParsedMethodCall result;

    std::string s = TrimLine(statement);
    if (!s.empty() && s.back() == ';')
        s.pop_back();

    const std::size_t open = s.find('(');
    const std::size_t close = s.rfind(')');

    if (open == std::string::npos || close == std::string::npos || close <= open)
        return result;

    const std::string callable = TrimLine(s.substr(0, open));
    const std::size_t dot = callable.rfind('.');

    if (dot == std::string::npos)
        return result;

    result.object = TrimLine(callable.substr(0, dot));
    result.method = TrimLine(callable.substr(dot + 1));
    result.params = s.substr(open + 1, close - open - 1);
    result.args = SplitArgs(result.params);
    result.valid = !result.object.empty() && !result.method.empty();

    return result;
}

void PrepareFindCircleDebugRuntime()
{
    ImageManager::m_imodulid = 1;
    ImageManager::GetBackImage(1);
}

std::string GetGlobalMatInputPath(const ManualTestContext& context)
{
    if (!context.image_file_path.empty())
        return context.image_file_path;

    for (const ScriptVariableView& variable : context.global_variable_views)
    {
        if (variable.name == "global_matInput")
        {
            if (!variable.image_path.empty())
                return variable.image_path;

            if (!variable.value.empty() &&
                variable.value != "uninitialized" &&
                variable.value != "none")
                return variable.value;
        }
    }

    return {};
}

std::string StripAddressPrefix(std::string s)
{
    s = TrimLine(s);
    if (!s.empty() && s.front() == '&')
        s.erase(s.begin());
    return TrimLine(s);
}

void UpsertGlobalVariableViewCore(
    ManualTestContext& context,
    const std::string& type,
    const std::string& name,
    const std::string& value,
    int lineNo,
    const std::string& status,
    const std::string& imagePath,
    bool imageInitialized)
{
    for (ScriptVariableView& variable : context.global_variable_views)
    {
        if (variable.name == name)
        {
            variable.type = type;
            variable.value = value;
            variable.declared_line = lineNo;
            variable.status = status;

            if (!imagePath.empty())
            {
                variable.image_path = imagePath;
            }

            if (imageInitialized)
            {
                variable.image_initialized = true;
            }

            return;
        }
    }

    ScriptVariableView variable;
    variable.type = type;
    variable.name = name;
    variable.value = value;
    variable.declared_line = lineNo;
    variable.status = status;
    variable.image_path = imagePath;
    variable.image_initialized = imageInitialized;

    context.global_variable_views.push_back(variable);
}

void UpsertGlobalVariableView(
    ManualTestContext& context,
    const std::string& type,
    const std::string& name,
    const std::string& value,
    int lineNo,
    const std::string& status)
{
    UpsertGlobalVariableViewCore(
        context,
        type,
        name,
        value,
        lineNo,
        status,
        std::string(),
        false);
}

void UpsertGlobalImageVariableView(
    ManualTestContext& context,
    const std::string& name,
    const std::string& imagePath,
    int lineNo)
{
    UpsertGlobalVariableViewCore(
        context,
        "string",
        name,
        imagePath,
        lineNo,
        "image_ready",
        imagePath,
        true);
}

void UpsertVariableView(
    ManualTestContext& context,
    const std::string& type,
    const std::string& name,
    const std::string& value,
    int lineNo,
    const std::string& status)
{
    for (ScriptVariableView& variable : context.variable_views)
    {
        if (variable.name == name)
        {
            variable.type = type;
            variable.value = value;
            variable.declared_line = lineNo;
            variable.status = status;
            return;
        }
    }

    context.variable_views.push_back({ type, name, value, lineNo, status });
}

void ResetDebugRuntimeForReplay(ManualTestContext& context)
{
    g_cximageRuntime.erase(&context);

    context.runtime_objects.clear();
    context.debug_snapshots.clear();
    context.current_debug_snapshot = DebugStepSnapshot();
    context.runtime_int_vars.clear();

    context.runtime_int_vars["m_isetcircle"] = 0;

    context.variable_views.clear();
    UpsertVariableView(context, "int", "m_isetcircle", "0", 0, "runtime_initialized");
    UpsertVariableView(context, "string", "global_current_status", "PENDING", 0, "runtime_initialized");
    for (ScriptVariableView& variable : context.global_variable_views)
    {
        if (variable.name == "global_matInput") continue;
        variable.value = "uninitialized";
        variable.status = "observed_source";
    }

    for (ScriptLineView& line : context.line_views)
    {
        if (!TrimLine(line.statement).empty())
        {
            line.status = "source_analyzed";
            line.reason = "not executed";
            line.timestamp.clear();
        }
    }

    context.current_line = 0;
    context.run_state = "ready";
    context.debug_status = "PENDING";
    context.debug_reason = "runtime reset for replay";
    context.runtime_current_status = "PENDING";

    ResetCxDebugRuntimeLog(context, "ResetDebugRuntimeForReplay");
    AppendCxDebugEvent(
        context,
        "runtime_reset",
        0,
        "",
        "",
        "",
        context.debug_status,
        context.debug_reason,
        "runtime reset for replay");
}

int FindNextNonEmptyLine(const ManualTestContext& context, int fromIndex)
{
    for (int i = fromIndex; i < static_cast<int>(context.line_views.size()); ++i)
    {
        if (!TrimLine(context.line_views[static_cast<std::size_t>(i)].statement).empty())
            return i;
    }

    return static_cast<int>(context.line_views.size());
}

int FindMatchingBraceLine(const ManualTestContext& context, int openBraceIndex)
{
    int depth = 0;
    bool seenOpenBrace = false;

    for (int i = openBraceIndex; i < static_cast<int>(context.line_views.size()); ++i)
    {
        const std::string s = TrimLine(context.line_views[static_cast<std::size_t>(i)].statement);

        for (char ch : s)
        {
            if (ch == '{')
            {
                ++depth;
                seenOpenBrace = true;
            }
            else if (ch == '}' && seenOpenBrace)
            {
                --depth;
                if (depth == 0)
                    return i;
            }
        }
    }

    return -1;
}

int FindIfBodyStartLine(const ManualTestContext& context, int ifIndex)
{
    const int next = FindNextNonEmptyLine(context, ifIndex + 1);

    if (next < static_cast<int>(context.line_views.size()) &&
        IsBraceOpenLine(context.line_views[static_cast<std::size_t>(next)].statement))
    {
        return FindNextNonEmptyLine(context, next + 1);
    }

    return next;
}

int FindIfAfterBlockLine(const ManualTestContext& context, int ifIndex)
{
    const std::string ifStatement =
        TrimLine(context.line_views[static_cast<std::size_t>(ifIndex)].statement);

    if (ifStatement.find('{') != std::string::npos)
    {
        const int close = FindMatchingBraceLine(context, ifIndex);
        if (close >= 0)
            return FindNextNonEmptyLine(context, close + 1);
    }

    const int next = FindNextNonEmptyLine(context, ifIndex + 1);

    if (next < static_cast<int>(context.line_views.size()) &&
        IsBraceOpenLine(context.line_views[static_cast<std::size_t>(next)].statement))
    {
        const int close = FindMatchingBraceLine(context, next);
        if (close >= 0)
            return FindNextNonEmptyLine(context, close + 1);
    }

    return FindNextNonEmptyLine(context, ifIndex + 1);
}

void MarkDebugRunFinishedIfAtEnd(ManualTestContext& context)
{
    const int next = FindNextNonEmptyLine(context, context.current_line);

    if (context.current_line >= static_cast<int>(context.line_views.size()) ||
        next >= static_cast<int>(context.line_views.size()))
    {
        context.current_line = static_cast<int>(context.line_views.size());
        context.run_state = "runtime_finished";
        context.debug_status = "PENDING";
        context.debug_reason =
            "script finished; global_current_status remains PENDING; judge/rule not executed";

        AppendCxDebugEvent(
            context,
            "debug_run_finished",
            context.current_line,
            "",
            "",
            "",
            context.debug_status,
            context.debug_reason,
            "script reached end");
    }
}

void MarkLineAsStructural(ManualTestContext& context,
    int lineIndex,
    const std::string& reason)
{
    if (lineIndex < 0 ||
        lineIndex >= static_cast<int>(context.line_views.size()))
    {
        return;
    }

    ScriptLineView& line =
        context.line_views[static_cast<std::size_t>(lineIndex)];

    line.status = "structural";
    line.reason = reason;
    line.timestamp = CurrentTimestamp();

    AppendCxDebugEvent(
        context,
        "structural_line",
        line.line_no,
        line.statement,
        line.object,
        line.method,
        line.status,
        line.reason,
        "structural line marked by debugger");
}

void MarkIfBlockBracesStructural(ManualTestContext& context,
    int ifLineIndex,
    bool markCloseBrace)
{
    const int next = FindNextNonEmptyLine(context, ifLineIndex + 1);

    if (next >= static_cast<int>(context.line_views.size()))
        return;

    if (!IsBraceOpenLine(context.line_views[static_cast<std::size_t>(next)].statement))
        return;

    MarkLineAsStructural(context, next, "open brace skipped by if debugger");

    if (markCloseBrace)
    {
        const int close = FindMatchingBraceLine(context, next);

        if (close >= 0)
        {
            MarkLineAsStructural(context, close, "close brace skipped by if debugger");
        }
    }
}

bool ReadRuntimeInt(ManualTestContext& context,
    const std::string& name,
    int& value)
{
    const auto it = context.runtime_int_vars.find(name);
    if (it != context.runtime_int_vars.end())
    {
        value = it->second;
        return true;
    }

    if (name == "m_isetcircle")
    {
        value = 0;
        context.runtime_int_vars[name] = 0;
        return true;
    }

    return false;
}

std::string StripCxScriptQuotes(std::string value)
{
    value = TrimLine(value);
    if (!value.empty() && value.back() == ';')
        value.pop_back();
    value = TrimLine(value);
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
        value = value.substr(1, value.size() - 2);
    return value;
}

bool ReadRuntimeVariableValue(const ManualTestContext& context,
    const std::string& name,
    std::string& value)
{
    const std::string key = TrimLine(name);

    for (const auto& variable : context.variable_views)
    {
        if (variable.name == key)
        {
            value = variable.value;
            return true;
        }
    }

    for (const auto& variable : context.global_variable_views)
    {
        if (variable.name == key)
        {
            value = variable.value;
            return true;
        }
    }

    return false;
}

bool ReadRuntimeNumber(ManualTestContext& context,
    const std::string& token,
    double& value)
{
    const std::string key = TrimLine(token);

    int intValue = 0;
    if (ReadRuntimeInt(context, key, intValue))
    {
        value = static_cast<double>(intValue);
        return true;
    }

    std::string variableValue;
    if (ReadRuntimeVariableValue(context, key, variableValue))
    {
        char* end = nullptr;
        const double parsed = std::strtod(variableValue.c_str(), &end);
        if (end != variableValue.c_str())
        {
            value = parsed;
            return true;
        }
    }

    char* end = nullptr;
    const double parsed = std::strtod(key.c_str(), &end);
    if (end == key.c_str() || *end != '\0')
        return false;

    value = parsed;
    return true;
}

bool ReadRuntimeString(ManualTestContext& context,
    const std::string& token,
    std::string& value)
{
    const std::string key = TrimLine(token);

    if (key.size() >= 2 && key.front() == '"' && key.back() == '"')
    {
        value = StripCxScriptQuotes(key);
        return true;
    }

    return ReadRuntimeVariableValue(context, key, value);
}

bool EvalSimpleCondition(ManualTestContext& context,
    const std::string& condition,
    bool& value)
{
    const std::string s = TrimLine(condition);

    const char* ops[] = {"==", "!=", "<=", ">=", "<", ">"};
    std::string opText;
    std::size_t op = std::string::npos;

    for (const char* candidate : ops)
    {
        op = s.find(candidate);
        if (op != std::string::npos)
        {
            opText = candidate;
            break;
        }
    }

    if (op == std::string::npos || opText.empty())
        return false;

    const std::string lhs = TrimLine(s.substr(0, op));
    const std::string rhs = TrimLine(s.substr(op + opText.size()));

    if (lhs.empty() || rhs.empty())
        return false;

    if (lhs.find('"') != std::string::npos ||
        rhs.find('"') != std::string::npos)
    {
        std::string lv;
        std::string rv;
        if (!ReadRuntimeString(context, lhs, lv) ||
            !ReadRuntimeString(context, rhs, rv))
            return false;

        if (opText == "==")
            value = (lv == rv);
        else if (opText == "!=")
            value = (lv != rv);
        else
            return false;

        return true;
    }

    double lv = 0.0;
    double rv = 0.0;
    if (!ReadRuntimeNumber(context, lhs, lv) ||
        !ReadRuntimeNumber(context, rhs, rv))
        return false;

    if (opText == "==")
        value = (lv == rv);
    else if (opText == "!=")
        value = (lv != rv);
    else if (opText == "<")
        value = (lv < rv);
    else if (opText == ">")
        value = (lv > rv);
    else if (opText == "<=")
        value = (lv <= rv);
    else if (opText == ">=")
        value = (lv >= rv);
    else
        return false;

    return true;
}

bool TryExecuteIntDeclarationAssignment(ManualTestContext& context,
    int lineIndex,
    const std::string& statement)
{
    std::string s = TrimLine(statement);

    if (s.rfind("int ", 0) != 0)
        return false;

    if (!s.empty() && s.back() == ';')
        s.pop_back();

    const std::size_t eq = s.find('=');

    if (eq == std::string::npos)
        return false;

    std::string lhs = TrimLine(s.substr(4, eq - 4));
    std::string rhs = TrimLine(s.substr(eq + 1));

    if (lhs.empty())
        return false;

    int v = 0;
    if (!ReadRuntimeInt(context, rhs, v))
    {
        char* end = nullptr;
        const long parsed = std::strtol(rhs.c_str(), &end, 10);
        if (end == rhs.c_str() || *end != '\0')
        {
            ScriptLineView& line =
                context.line_views[static_cast<std::size_t>(lineIndex)];
            line.status = "BLOCKED";
            line.reason = "int initializer unresolved: " + rhs;
            line.return_variable = lhs;
            line.timestamp = CurrentTimestamp();

            context.run_state = "blocked";
            context.debug_status = "BLOCKED";
            context.debug_reason = line.reason;
            return true;
        }

        v = static_cast<int>(parsed);
    }

    context.runtime_int_vars[lhs] = v;

    ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
    line.status = "runtime_executed";
    line.reason =
        "int variable initialized | " + lhs + "=" + std::to_string(v);
    line.return_variable = lhs;
    line.timestamp = CurrentTimestamp();

    UpsertVariableView(
        context,
        "int",
        lhs,
        std::to_string(v),
        line.line_no,
        "runtime_value");

    context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
    context.run_state = "runtime_step";
    context.debug_status = "PENDING";
    context.debug_reason = line.reason;

    AppendCxDebugEvent(
        context,
        "int_variable_initialized",
        line.line_no,
        statement,
        lhs,
        "int_init",
        line.status,
        line.reason,
        lhs + "=" + std::to_string(v));

    return true;
}

bool TryExecuteSimpleAssignment(ManualTestContext& context,
    int lineIndex,
    const std::string& statement)
{
    std::string s = TrimLine(statement);

    if (s.empty() ||
        s.find('=') == std::string::npos ||
        s.find("==") != std::string::npos)
    {
        return false;
    }

    if (!s.empty() && s.back() == ';')
        s.pop_back();

    const std::size_t eq = s.find('=');

    if (eq == std::string::npos)
        return false;

    const std::string lhs = TrimLine(s.substr(0, eq));
    std::string rhs = TrimLine(s.substr(eq + 1));

    ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];

    if (lhs == "m_isetcircle")
    {
        const int v = std::atoi(rhs.c_str());

        context.runtime_int_vars[lhs] = v;

        line.status = "runtime_executed";
        line.reason = "assignment executed";

        line.return_variable = lhs;
        line.timestamp = CurrentTimestamp();
        AppendCxDebugEvent(
            context,
            "global_current_status_set",
            line.line_no,
            statement,
            lhs,
            "assignment",
            line.status,
            line.reason,
            "current_status=" + rhs);
        UpsertVariableView(
            context,
            "int",
            lhs,
            std::to_string(v),
            line.line_no,
            "runtime_value");

        context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
        context.run_state = "runtime_step";
        context.debug_status = "PENDING";
        context.debug_reason = "assignment executed";
        MarkDebugRunFinishedIfAtEnd(context);
        return true;
    }

    if (lhs == "global_current_status")
    {
        if (!rhs.empty() && rhs.front() == '"')
            rhs.erase(rhs.begin());

        if (!rhs.empty() && rhs.back() == '"')
            rhs.pop_back();

        context.runtime_current_status = rhs;

        UpsertGlobalVariableView(
            context,
            "string",
            lhs,
            rhs,
            line.line_no,
            "runtime_value");

        UpsertVariableView(
            context,
            "string",
            lhs,
            rhs,
            line.line_no,
            "runtime_initialized");

        line.status = "runtime_executed";
        line.reason = "global_current_status remains " + rhs + "; judge/rule not executed";
        line.return_variable = lhs;
        line.timestamp = CurrentTimestamp();

        context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
        MarkDebugRunFinishedIfAtEnd(context);
        context.run_state = "runtime_step";

        context.debug_status = "PENDING";
        context.debug_reason = line.reason;

        return true;
    }

    if (lhs.rfind("global.", 0) == 0)
    {
        const bool isStringValue =
            rhs.size() >= 2 && rhs.front() == '"' && rhs.back() == '"';

        if (isStringValue)
        {
            const std::string stringValue = StripCxScriptQuotes(rhs);

            UpsertGlobalVariableView(
                context,
                "string",
                lhs,
                stringValue,
                line.line_no,
                "runtime_value");

            UpsertVariableView(
                context,
                "string",
                lhs,
                stringValue,
                line.line_no,
                "runtime_value");

            line.status = "runtime_executed";
            line.reason = "global string assignment executed | " + lhs + "=" + stringValue;
            line.return_variable = lhs;
            line.timestamp = CurrentTimestamp();

            context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
            context.run_state = "runtime_step";
            context.debug_status = "PENDING";
            context.debug_reason = line.reason;
            MarkDebugRunFinishedIfAtEnd(context);
            return true;
        }

        double numericValue = 0.0;
        if (ReadRuntimeNumber(context, rhs, numericValue))
        {
            const int intValue = static_cast<int>(numericValue);
            const bool integerLike =
                std::fabs(numericValue - static_cast<double>(intValue)) < 0.000001;

            if (integerLike)
                context.runtime_int_vars[lhs] = intValue;

            std::ostringstream valueStream;
            valueStream << numericValue;
            const std::string valueText = integerLike
                ? std::to_string(intValue)
                : valueStream.str();

            UpsertGlobalVariableView(
                context,
                integerLike ? "int" : "double",
                lhs,
                valueText,
                line.line_no,
                "runtime_value");

            UpsertVariableView(
                context,
                integerLike ? "int" : "double",
                lhs,
                valueText,
                line.line_no,
                "runtime_value");

            line.status = "runtime_executed";
            line.reason = "global numeric assignment executed | " + lhs + "=" + valueText;
            line.return_variable = lhs;
            line.timestamp = CurrentTimestamp();

            context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
            context.run_state = "runtime_step";
            context.debug_status = "PENDING";
            context.debug_reason = line.reason;
            MarkDebugRunFinishedIfAtEnd(context);
            return true;
        }
    }

    return false;
}

bool TryExecuteCurrentStatusAssignment(ManualTestContext& context,
    int lineIndex,
    const std::string& statement)
{
    const std::string trimmed = TrimLine(statement);
    if (trimmed.find("global_current_status") == std::string::npos ||
        trimmed.find("PENDING") == std::string::npos)
        return false;

    ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
    UpsertGlobalVariableView(context, "string", "global_current_status",
        "PENDING", line.line_no, "runtime_value");
    context.runtime_current_status = "PENDING";
    line.status = "runtime_executed";
    line.reason = "global_current_status remains PENDING; judge/rule not executed";
    line.timestamp = CurrentTimestamp();
    context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
    context.run_state = "runtime_step";
    context.debug_status = "PENDING";
    context.debug_reason = line.reason;
    return true;
}

bool TryExecuteDeclaration(ManualTestContext& context,
    int lineIndex,
    const std::string& statement)
{
    std::istringstream tokens(TrimLine(statement));

    std::string type;
    std::string name;

    tokens >> type >> name;

    if (type.empty() || name.empty())
        return false;

    if (statement.find('(') != std::string::npos)
        return false;

    if (!name.empty() && name.back() == ';')
        name.pop_back();

    if (!IsObjectType(type))
        return false;

    RuntimeObjectView& object = EnsureRuntimeObject(
        context,
        name,
        type,
        context.line_views[static_cast<std::size_t>(lineIndex)].line_no);

    DebugCximageRuntime& runtime = CxRuntime(context);

    if (type == "Image")
    {
        runtime.images[name] = std::make_unique<Image>();
        object.exists_in_parser = true;
        object.runtime_state = "runtime_object_created";
        object.last_runtime_status = "PENDING";
        object.display_summary = "Image runtime object created";
        object.visualizable = false;
    }
    else if (type == "FindCircle")
    {
        PrepareFindCircleDebugRuntime();
        runtime.circles[name] = std::make_unique<FindCircle>();
        object.exists_in_parser = true;
        object.runtime_state = "runtime_object_created";
        object.last_runtime_status = "PENDING";
        object.display_summary = "FindCircle runtime object created";
        object.visualizable = false;
        object.has_circle = false;
        object.has_measure_points = false;
        object.has_fit_result = false;
        object.has_result_measure = false;
    }
    else if (type == "FindLine")
    {
        runtime.lines[name] = std::make_unique<FindLine>();
        object.exists_in_parser = true;
        object.runtime_state = "runtime_object_created";
        object.last_runtime_status = "runtime_executed";
        object.display_summary = "Findline runtime object created";
        object.visualizable = false;
    }


    object.exists_in_parser = true;
    object.runtime_state = "declared";
    object.last_runtime_status = "runtime_executed";
    object.last_method = "declare";
    object.display_summary = "declared only; no visual geometry";
    object.last_update_line = context.line_views[static_cast<std::size_t>(lineIndex)].line_no;

    object.visualizable = false;
    object.has_circle = false;
    object.stale = false;

    ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
    line.status = "runtime_executed";
    line.reason = "object declared";
    line.timestamp = CurrentTimestamp();

    context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
    context.run_state = "runtime_step";
    context.debug_status = "PENDING";
    context.debug_reason = "object declaration executed";

    return true;
}

void AddObservedGlobalVariables(ManualTestContext& context, const std::string& statement)
{
    const std::vector<std::string> globalNames = ExtractGlobalNames(statement);
    for (const std::string& name : globalNames)
    {
        bool found = false;
        for (const ScriptVariableView& v : context.global_variable_views)
        {
            if (v.name == name) { found = true; break; }
        }
        if (!found)
        {
            context.global_variable_views.push_back({ "string", name, "uninitialized", 0, "observed_source" });
        }
    }
}

void AnalyzeScript(ManualTestContext& context)
{
    if (context.analyzed_text == context.editor_text) return;
    const int previousCurrentLine = context.current_line;
    const bool preserveRuntimeCursor =
        !context.line_views.empty() &&
        (context.run_state == "runtime_step" ||
         context.run_state == "running");

    context.analyzed_text = context.editor_text;
    context.line_views.clear();
    context.cxparser_ext_line_views.clear();
    context.cxparser_ext_statement_views.clear();
    context.cxparser_ext_object_assignments.clear();
    context.cxparser_ext_debug_ok = false;
    context.cxparser_ext_debug_status.clear();
    context.cxparser_ext_debug_reason.clear();
    context.variable_views.clear();
    context.object_views.clear();
    if (context.global_variable_views.size() > 1)
        context.global_variable_views.erase(
            context.global_variable_views.begin() + 1,
            context.global_variable_views.end());
    context.current_line = 0;

    std::istringstream input(context.editor_text);
    std::string raw;
    int line_no = 1;
    while (std::getline(input, raw))
    {
        ScriptLineView line;
        line.line_no = line_no++;
        line.status = "source_analyzed";
        line.reason = "not_executed";
        line.statement = raw;
        const std::string statement = TrimLine(raw);
        AddObservedGlobalVariables(context, statement);
        line.module = ModuleForStatement(statement);

        std::istringstream tokens(statement);
        std::string declared_type;
        std::string declared_name;
        tokens >> declared_type >> declared_name;
        const bool declaration = !declared_type.empty() && !declared_name.empty() &&
            statement.find('(') == std::string::npos && declared_type != "if" &&
            declared_type != "else" && declared_type != "return";
        if (declaration)
        {
            const std::size_t suffix = declared_name.find_first_of("=;");
            if (suffix != std::string::npos) declared_name.erase(suffix);
            line.object_type = declared_type;
            line.object = declared_name;
            if (IsObjectType(declared_type))
            {
                line.module = ModuleForType(declared_type);

                ScriptObjectView object;
                object.module = line.module;
                object.type = declared_type;
                object.name = declared_name;
                object.status = line.module == "cximage" ? "declared_source_only" : "pending_binding";
                object.runtime_state = "not_executed";
                object.runtime_source_line = 0;
                object.declared_line = line.line_no;
                context.object_views.push_back(object);

                if (declared_type == "Image")
                    context.variable_views.push_back(
                        { declared_type, declared_name, "uninitialized", line.line_no,
                         "not_initialized", context.image_file_path, false });
            }
            else
            {
                const std::size_t equal = statement.find('=');
                const std::string value = equal == std::string::npos ? "uninitialized" :
                    TrimLine(statement.substr(equal + 1, statement.size() - equal - 2));
                context.variable_views.push_back({ declared_type, declared_name, value,
                                                  line.line_no, "observed_source" });
            }
        }

        const std::size_t assign = statement.find('=');
        const std::size_t open = statement.find('(');
        const std::size_t close = statement.rfind(')');
        const bool has_assignment = assign != std::string::npos &&
            (open == std::string::npos || assign < open);
        const std::size_t callable_start = has_assignment ? assign + 1 : 0;
        if (has_assignment) line.return_variable = TrimLine(statement.substr(0, assign));
        if (open != std::string::npos)
        {
            const std::string callable = TrimLine(statement.substr(callable_start, open - callable_start));
            const std::size_t dot = callable.rfind('.');
            if (dot != std::string::npos)
            {
                line.object = TrimLine(callable.substr(0, dot));
                line.method = TrimLine(callable.substr(dot + 1));
            }
            else line.method = callable;
            if (close != std::string::npos && close > open)
                line.params = statement.substr(open + 1, close - open - 1);
        }
        context.line_views.push_back(line);
    }
    context.trace_status = "PENDING";
    context.trace_reason = "source analyzed; runtime line callbacks unavailable";

    if (preserveRuntimeCursor)
    {
        if (previousCurrentLine < 0)
            context.current_line = 0;
        else if (previousCurrentLine > static_cast<int>(context.line_views.size()))
            context.current_line = static_cast<int>(context.line_views.size());
        else
            context.current_line = previousCurrentLine;
    }
}

void DebugScriptLineEnd(ManualTestContext& context, int lineIndex, const std::string& status)
{
    if (lineIndex >= 0 && lineIndex < static_cast<int>(context.line_views.size()))
    {
        ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
        std::cout << "[DEBUG SCRIPT LINE] line=" << line.line_no << " end status=" << status << "\n" << std::flush;
    }
}

void DebugStepOnce(ManualTestContext& context)
{
    AnalyzeScript(context);

    if (context.line_views.empty())
    {
        context.run_state = "blocked";
        context.debug_status = "BLOCKED";
        context.debug_reason = "no script lines";
        return;
    }

    if (context.current_line < 0)
        context.current_line = 0;

    if (context.current_line >= static_cast<int>(context.line_views.size()))
    {
        context.run_state = "finished";
        context.debug_status = "PENDING";
        context.debug_reason = "end of script";
        return;
    }

    try
    {

    const int lineIndex = context.current_line;
    ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
    const std::string statement = TrimLine(line.statement);
    
    std::cout << "[DEBUG SCRIPT LINE] line=" << line.line_no << " begin: " << statement << "\n" << std::flush;

    AppendCxDebugEvent(
        context,
        "step_enter",
        line.line_no,
        statement,
        line.object,
        line.method,
        line.status,
        line.reason,
        "enter debug step");

    if (statement.empty())
    {
        line.status = "skipped_empty";
        line.reason = "empty line";
        context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
        DebugScriptLineEnd(context, lineIndex, "skipped_empty");
        return;
    }

    if (IsBraceOpenLine(statement) || IsBraceCloseLine(statement))
    {
        line.status = "structural";
        line.reason = "brace skipped by debugger";

        line.timestamp = CurrentTimestamp();

        AppendCxDebugEvent(
            context,
            "structural_brace",
            line.line_no,
            statement,
            "",
            "brace",
            line.status,
            line.reason,
            "brace skipped");

        context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
        context.run_state = "runtime_step";
        context.debug_status = "PENDING";
        context.debug_reason = line.reason;

        DebugScriptLineEnd(context, lineIndex, "structural");
        return;
    }

    if (IsIfLine(statement))
    {
        bool conditionValue = false;
        const std::string condition = ExtractIfCondition(statement);

        if (!EvalSimpleCondition(context, condition, conditionValue))
        {
            line.status = "BLOCKED";
            line.reason = "cannot evaluate if condition: " + condition;
            line.timestamp = CurrentTimestamp();

            context.run_state = "blocked";
            context.debug_status = "BLOCKED";
            context.debug_reason = line.reason;
            return;
        }

        line.status = conditionValue ? "control_true" : "control_false";
        line.reason = conditionValue ? "if condition true" : "if condition false";
        line.timestamp = CurrentTimestamp();

        MarkIfBlockBracesStructural(context, lineIndex, !conditionValue);

        context.current_line = conditionValue ?
            FindIfBodyStartLine(context, lineIndex) :
            FindIfAfterBlockLine(context, lineIndex);

        context.run_state = "runtime_step";
        context.debug_status = "PENDING";
        context.debug_reason = line.reason;

        AppendCxDebugEvent(
            context,
            "if_control",
            line.line_no,
            statement,
            "",
            "if",
            line.status,
            line.reason,
            condition);

        return;
    }

    if (TryExecuteCurrentStatusAssignment(context, lineIndex, statement))
    {
        DebugScriptLineEnd(context, lineIndex, "current_status_assignment");
        return;
    }

    if (TryExecuteSimpleAssignment(context, lineIndex, statement))
    {
        DebugScriptLineEnd(context, lineIndex, "simple_assignment");
        return;
    }

    if (TryExecuteDeclaration(context, lineIndex, statement))
        return;

    if (TryExecuteImageCopyFromMat(context, lineIndex, statement))
        return;

    if (TryExecuteFindLineSetline(context, lineIndex, statement))
        return;

    if (TryExecuteFindLineParamMethod(context, lineIndex, statement))
        return;

    if (TryExecuteFindLineRuntimeMethod(context, lineIndex, statement))
        return;

    if (TryExecuteFindCircleSetcircle(context, lineIndex, statement))
        return;

    if (TryExecuteFindCircleParamMethod(context, lineIndex, statement))
        return;

    if (TryExecuteFindCircleRuntimeMethod(context, lineIndex, statement))
    {
        DebugScriptLineEnd(context, lineIndex, "runtime_executed");
        return;
    }
    if (TryExecuteIntDeclarationAssignment(context, lineIndex, statement))
    {
        DebugScriptLineEnd(context, lineIndex, "executed");
        return;
    }
    if (TryExecuteGetResultBinding(context, lineIndex, statement))
    {
        DebugScriptLineEnd(context, lineIndex, "get_result_bound");
        return;
    }
    if (TryHandleFindCircleGetResult(context, lineIndex, statement))
    {
        DebugScriptLineEnd(context, lineIndex, "findcircle_get_result");
        return;
    }

    if (TryExecutePendingRuntimeMethod(context, lineIndex, statement))
        return;

    if (ParseMethodCall(statement).valid)
    {
        const ParsedMethodCall call = ParseMethodCall(statement);

        const bool isRuntimeAlgorithmCall =
            call.method == "measure" ||
            call.method == "fitcircle" ||
            call.method == "FitResultMeasure" ||
            call.method == "match" ||
            call.method == "learn" ||
            call.method == "infer" ||
            call.method == "predict" ||
            call.method == "optimize_step";

        if (isRuntimeAlgorithmCall)
        {
            if (TryExecutePendingRuntimeMethod(context, lineIndex, statement))
                return;
        }

        RuntimeObjectView& object = EnsureRuntimeObject(
            context,
            call.object,
            call.object.find("circle") != std::string::npos ? "FindCircle" : "unknown",
            line.line_no);

        object.last_method = call.method;
        object.last_runtime_status = "PENDING";
        object.runtime_state = "runtime_param_set";
        object.last_update_line = line.line_no;
        object.display_summary = call.method + "(" + call.params + ")";
        object.stale = false;

        line.status = "runtime_executed";
        line.reason = "method parameter line executed in debug shim";
        line.timestamp = CurrentTimestamp();

        context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
        context.run_state = "runtime_step";
        context.debug_status = "PENDING";
        context.debug_reason = "method parameter line executed";
        return;
    }


    line.status = "source_analyzed";
    line.reason = "statement not executable by debug shim";
    line.timestamp = CurrentTimestamp();
    context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
    }
    catch (const std::exception& ex)
    {
        const int lineIndex = context.current_line;
        ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
        line.status = "BLOCKED";
        line.reason = std::string("runtime exception: ") + ex.what();
        line.timestamp = CurrentTimestamp();

        context.run_state = "blocked";
        context.debug_status = "BLOCKED";
        context.debug_reason = line.reason;

        AppendCxDebugEvent(
            context,
            "runtime_exception",
            line.line_no,
            line.statement,
            "",
            "",
            line.status,
            line.reason,
            "");
    }
    catch (...)
    {
        const int lineIndex = context.current_line;
        ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
        line.status = "BLOCKED";
        line.reason = "runtime exception: unknown";
        line.timestamp = CurrentTimestamp();

        context.run_state = "blocked";
        context.debug_status = "BLOCKED";
        context.debug_reason = line.reason;

        AppendCxDebugEvent(
            context,
            "runtime_exception",
            line.line_no,
            line.statement,
            "",
            "",
            line.status,
            line.reason,
            "");
    }
}

void CaptureDebugStepSnapshot(ManualTestContext& context, int lineIndex)
{
    if (lineIndex < 0 ||
        lineIndex >= static_cast<int>(context.line_views.size()))
        return;

    const ScriptLineView& line =
        context.line_views[static_cast<std::size_t>(lineIndex)];
    DebugStepSnapshot snapshot;
    snapshot.script_path = context.loaded_script_path;
    snapshot.flow_block_id = "cximage_find_circle_explore.N0";
    snapshot.current_line = line.line_no;
    snapshot.statement = line.statement;
    snapshot.object = line.object;
    snapshot.method = line.method;
    snapshot.params = line.params;
    snapshot.reason = line.reason;
    snapshot.last_debug_result = context.debug_status + ": " + context.debug_reason;
    for (const ScriptVariableView& variable : context.global_variable_views)
        if (variable.name == "global_circle_ref")
            snapshot.current_result_ref = variable.value;

    RuntimeObjectView* object = line.object.empty() ? nullptr :
        FindRuntimeObject(context, line.object);
    if (object == nullptr)
        object = FindRuntimeObject(context, "afindcircle0");
    if (object != nullptr)
    {
        snapshot.runtime_state = object->runtime_state;
        snapshot.object_summary = object->display_summary;
        std::ostringstream geometry;
        geometry << "object=" << object->name << " | roi_circle=";
        if (object->has_circle)
            geometry << "(" << object->circle_cx << "," << object->circle_cy
                     << ",r=" << object->circle_radius << ")";
        else geometry << "none";
        geometry << " | measure_points_count=" << object->measure_points_count
                 << " | valid_points_count=" << object->valid_points_count
                 << " | fit_circle=";
        if (object->has_fit_result)
            geometry << "(" << object->fit_cx << "," << object->fit_cy
                     << ",r=" << object->fit_radius << ")";
        else geometry << "none";
        geometry << " | avgdist=" << object->fit_avgdist
                 << " | has_result_measure="
                 << (object->has_result_measure ? "true" : "false")
                 << " | " << object->display_summary;
        snapshot.geometry_summary = geometry.str();

        std::ostringstream overlay;
        overlay << "green_roi_circle=" << (object->has_circle ? "true" : "false")
                << " | red_measure_points=" << object->valid_points_count
                << " | yellow_fit_circle=" << (object->has_fit_result ? "true" : "false")
                << " | source_preview_enabled="
                << (context.source_preview_enabled ? "true" : "false")
                << " | manual_elements_count=" << context.manual_elements_count;
        snapshot.image_overlay_summary = overlay.str();
    }
    else
    {
        snapshot.runtime_state = line.status;
        snapshot.object_summary = "no runtime object for current line";
        snapshot.geometry_summary = "none";
        snapshot.image_overlay_summary = "none";
    }

    context.current_debug_snapshot = snapshot;
    context.debug_snapshots.push_back(snapshot);
}

void DebugStepOnceWithSnapshot(ManualTestContext& context)
{
    const int lineIndex = context.current_line;
    DebugStepOnce(context);
    CaptureDebugStepSnapshot(context, lineIndex);
}

void SetTraceStatus(ManualTestContext& context,
                    const std::string& status,
                    const std::string& reason)
{
  AnalyzeScript(context);
  const std::string timestamp = CurrentTimestamp();
  for (ScriptLineView& line : context.line_views)
  {
    if (TrimLine(line.statement).empty()) continue;
    line.status = status;
    line.reason = reason;
  line.timestamp = timestamp;
  }
  context.trace_status = status;
  context.trace_reason = reason;
}

std::filesystem::path CxDebugLogDirectory()
{
    return std::filesystem::path("docs") / "notes" / "cxscript_case";
}

std::filesystem::path CxDebugRuntimeLogPath()
{
    return CxDebugLogDirectory() / "cxscript_debug_runtime_latest.jsonl";
}

std::filesystem::path CxDebugSnapshotPath()
{
    return CxDebugLogDirectory() / "cxscript_debug_snapshot_latest.txt";
}

void ResetCxDebugRuntimeLog(const ManualTestContext& context,
    const std::string& reason)
{
    try
    {
        std::filesystem::create_directories(CxDebugLogDirectory());

        std::ofstream file(CxDebugRuntimeLogPath().string(),
            std::ios::out | std::ios::trunc);

        if (!file.is_open())
            return;

        file << "{"
            << "\"event\":\"log_reset\","
            << "\"time\":\"" << CxDebugJsonEscape(CurrentTimestamp()) << "\","
            << "\"reason\":\"" << CxDebugJsonEscape(reason) << "\","
            << "\"script_path\":\"" << CxDebugJsonEscape(context.loaded_script_path) << "\","
            << "\"flow_block\":\"cximage_find_circle_explore.N0\""
            << "}\n";
    }
    catch (...)
    {
    }
}

void AppendCxDebugEvent(const ManualTestContext& context,
    const std::string& event,
    int lineNo,
    const std::string& statement,
    const std::string& object,
    const std::string& method,
    const std::string& status,
    const std::string& reason,
    const std::string& summary)
{
    try
    {
        std::filesystem::create_directories(CxDebugLogDirectory());

        std::ofstream file(CxDebugRuntimeLogPath().string(),
            std::ios::out | std::ios::app);

        if (!file.is_open())
            return;

        file << "{"
            << "\"event\":\"" << CxDebugJsonEscape(event) << "\","
            << "\"time\":\"" << CxDebugJsonEscape(CurrentTimestamp()) << "\","
            << "\"flow_block\":\"cximage_find_circle_explore.N0\","
            << "\"script_path\":\"" << CxDebugJsonEscape(context.loaded_script_path) << "\","
            << "\"line_no\":" << lineNo << ","
            << "\"statement\":\"" << CxDebugJsonEscape(statement) << "\","
            << "\"object\":\"" << CxDebugJsonEscape(object) << "\","
            << "\"method\":\"" << CxDebugJsonEscape(method) << "\","
            << "\"status\":\"" << CxDebugJsonEscape(status) << "\","
            << "\"reason\":\"" << CxDebugJsonEscape(reason) << "\","
            << "\"run_state\":\"" << CxDebugJsonEscape(context.run_state) << "\","
            << "\"runtime_current_status\":\"" << CxDebugJsonEscape(context.runtime_current_status) << "\","
            << "\"debug_status\":\"" << CxDebugJsonEscape(context.debug_status) << "\","
            << "\"debug_reason\":\"" << CxDebugJsonEscape(context.debug_reason) << "\","
            << "\"summary\":\"" << CxDebugJsonEscape(summary) << "\""
            << "}\n";
    }
    catch (...)
    {
    }
}

void AppendCxDebugRuntimeObjectsSnapshot(const ManualTestContext& context,
    const std::string& event)
{
    try
    {
        std::filesystem::create_directories(CxDebugLogDirectory());

        std::ofstream file(CxDebugRuntimeLogPath().string(),
            std::ios::out | std::ios::app);

        if (!file.is_open())
            return;

        file << "{"
            << "\"event\":\"" << CxDebugJsonEscape(event) << "\","
            << "\"time\":\"" << CxDebugJsonEscape(CurrentTimestamp()) << "\","
            << "\"script_path\":\"" << CxDebugJsonEscape(context.loaded_script_path) << "\","
            << "\"flow_block\":\"cximage_find_circle_explore.N0\","
            << "\"runtime_objects\":[";

        for (std::size_t i = 0; i < context.runtime_objects.size(); ++i)
        {
            const RuntimeObjectView& object = context.runtime_objects[i];

            file << "{"
                << "\"name\":\"" << CxDebugJsonEscape(object.name) << "\","
                << "\"type\":\"" << CxDebugJsonEscape(object.type) << "\","
                << "\"declared_line\":" << object.declared_line << ","
                << "\"exists_in_parser\":" << (object.exists_in_parser ? "true" : "false") << ","
                << "\"runtime_state\":\"" << CxDebugJsonEscape(object.runtime_state) << "\","
                << "\"last_runtime_status\":\"" << CxDebugJsonEscape(object.last_runtime_status) << "\","
                << "\"last_method\":\"" << CxDebugJsonEscape(object.last_method) << "\","
                << "\"last_update_line\":" << object.last_update_line << ","
                << "\"display_summary\":\"" << CxDebugJsonEscape(object.display_summary) << "\","
                << "\"visualizable\":" << (object.visualizable ? "true" : "false") << ","
                << "\"visual_source\":\"" << CxDebugJsonEscape(object.visual_source) << "\","
                << "\"stale\":" << (object.stale ? "true" : "false") << ","
                << "\"has_circle\":" << (object.has_circle ? "true" : "false") << ","
                << "\"circle\":[" << object.circle_cx << ","
                << object.circle_cy << ","
                << object.circle_inner << ","
                << object.circle_radius << "],"
                << "\"has_measure_points\":" << (object.has_measure_points ? "true" : "false") << ","
                << "\"measure_points_count\":" << object.measure_points_count << ","
                << "\"valid_points_count\":" << object.valid_points_count << ","
                << "\"has_fit_result\":" << (object.has_fit_result ? "true" : "false") << ","
                << "\"fit_circle\":[" << object.fit_cx << ","
                << object.fit_cy << ","
                << object.fit_radius << "],"
                << "\"avgdist\":" << object.fit_avgdist
                << ",\"has_circle_roi_outer_polyline\":" << (object.has_circle_roi_outer_polyline ? "true" : "false")
                << ",\"has_circle_roi_inner_polyline\":" << (object.has_circle_roi_inner_polyline ? "true" : "false")
                << ",\"circle_roi_segment_count\":" << object.circle_roi_segment_count
                << ",\"has_fit_circle_polyline\":" << (object.has_fit_circle_polyline ? "true" : "false")
                << ",\"fit_circle_segment_count\":" << object.fit_circle_segment_count
                << ",\"display_version\":" << object.display_version
                << ",\"has_line_roi\":" << (object.has_line_roi ? "true" : "false")
                << ",\"line_roi\":[" << object.line_x0 << ","
                << object.line_y0 << ","
                << object.line_x1 << ","
                << object.line_y1 << "]"
                << ",\"has_line_scan_box\":" << (object.has_line_scan_box ? "true" : "false")
                << ",\"line_scan_half_width\":" << object.line_scan_half_width
                << ",\"line_scan_box_xy\":["
                << object.line_scan_box_xy[0] << "," << object.line_scan_box_xy[1] << ","
                << object.line_scan_box_xy[2] << "," << object.line_scan_box_xy[3] << ","
                << object.line_scan_box_xy[4] << "," << object.line_scan_box_xy[5] << ","
                << object.line_scan_box_xy[6] << "," << object.line_scan_box_xy[7] << "]"
                << ",\"line_pointsw_count\":" << object.line_pointsw_count
                << ",\"line_pointsh_count\":" << object.line_pointsh_count
                << ",\"line_measure_points_count\":" << object.line_measure_points_count
                << ",\"valid_line_points_count\":" << object.valid_line_points_count
                << ",\"has_fit_line\":" << (object.has_fit_line ? "true" : "false")
                << ",\"fit_line\":[" << object.fit_line_x0 << ","
                << object.fit_line_y0 << ","
                << object.fit_line_x1 << ","
                << object.fit_line_y1 << "]"
                << ",\"line_avgdist\":" << object.line_avgdist
                << ",\"line_fit_mode\":\"" << CxDebugJsonEscape(object.line_fit_mode) << "\""
                << ",\"line_fit_status\":\"" << CxDebugJsonEscape(object.line_fit_status) << "\""
                << ",\"line_measure_status\":\"" << CxDebugJsonEscape(object.line_measure_status) << "\""
                << ",\"line_measure_hint\":\"" << CxDebugJsonEscape(object.line_measure_hint) << "\""
                << "}";

            if (i + 1 < context.runtime_objects.size())
                file << ",";
        }

        file << "]}";
        file << "\n";
    }
    catch (...)
    {
    }
}

bool SaveCxDebugSnapshotText(ManualTestContext& context,
    const std::filesystem::path& path,
    std::string& outReason)
{
    try
    {
        std::filesystem::create_directories(path.parent_path());

        std::ofstream file(path.string(), std::ios::out | std::ios::trunc);

        if (!file.is_open())
        {
            outReason = "failed to open debug snapshot file: " + path.string();
            return false;
        }

        file << "CxScript Debug Snapshot\n";
        file << "=======================\n";
        file << "time: " << CurrentTimestamp() << "\n";
        file << "flow_block: cximage_find_circle_explore.N0\n";
        file << "script_path: " << context.loaded_script_path << "\n";

        if (!context.active_script_case_name.empty())
        {
            file << "active_script_case_name: "
                 << context.active_script_case_name << "\n";

            file << "active_script_case_path: "
                 << context.active_script_case_path << "\n";

            file << "active_script_case_purpose: "
                 << context.active_script_case_purpose << "\n";
        }

        file << "run_state: " << context.run_state << "\n";
        file << "runtime_current_status: " << context.runtime_current_status << "\n";
        file << "debug_status: " << context.debug_status << "\n";
        file << "debug_reason: " << context.debug_reason << "\n";
        file << "cxparser_ext_debug_ok: "
             << (context.cxparser_ext_debug_ok ? "true" : "false") << "\n";
        file << "cxparser_ext_debug_status: "
             << context.cxparser_ext_debug_status << "\n";
        file << "cxparser_ext_debug_reason: "
             << context.cxparser_ext_debug_reason << "\n";
        file << "current_line_index: " << context.current_line << "\n";
        file << "execution_cursor: " << BuildDebugCursorText(context) << "\n";
        file << "last_executed_line_no: " << LastExecutedLineNo(context) << "\n\n";

        file << "CxParserExt Debug Layer\n";
        file << "------------------------\n";
        file << "line_views: " << context.cxparser_ext_line_views.size() << "\n";
        for (const CxScriptLineView& line : context.cxparser_ext_line_views)
        {
            file << "* line " << line.line_no
                 << " type=" << line.statement_type
                 << " status=" << line.status << "\n";
            file << "  source: " << line.source_line << "\n";
            file << "  reason: " << line.reason << "\n";
        }
        file << "statement_views: " << context.cxparser_ext_statement_views.size() << "\n";
        for (const CxScriptStatementView& stmt : context.cxparser_ext_statement_views)
        {
            file << "* #" << stmt.statement_id
                 << " line=" << stmt.line_no
                 << " type=" << stmt.statement_type
                 << " status=" << stmt.status << "\n";
            file << "  lhs: " << stmt.lhs_variable << " : " << stmt.lhs_type << "\n";
            file << "  call: " << stmt.source_object << "." << stmt.method_name << "()\n";
            file << "  return_ref: " << stmt.returned_object_ref << "\n";
            file << "  reason: " << stmt.reason << "\n";
        }
        file << "object_assignments: " << context.cxparser_ext_object_assignments.size() << "\n";
        for (const CxScriptObjectAssignmentView& item : context.cxparser_ext_object_assignments)
        {
            file << "* " << item.lhs_variable << " : " << item.lhs_type << "\n";
            file << "  from: " << item.source_object << "." << item.method_name << "()\n";
            file << "  ref: " << item.returned_object_ref << "\n";
            file << "  line " << item.line_no << ": " << item.source_line << "\n";
            file << "  status: " << item.status << "\n";
            file << "  reason: " << item.reason << "\n";
        }
        file << "\n";
        file << "Current Result Ref\n";
        file << "------------------\n";
        file << "name: " << context.current_result_ref.name << "\n";
        file << "value: " << context.current_result_ref.value << "\n";
        file << "source_object: " << context.current_result_ref.source_object << "\n";
        file << "result_type: " << context.current_result_ref.result_type << "\n";
        file << "status: " << context.current_result_ref.status << "\n";
        file << "reason: " << context.current_result_ref.reason << "\n";
        file << "fit: (" << context.current_result_ref.fit_cx
            << ", " << context.current_result_ref.fit_cy
            << ", r=" << context.current_result_ref.fit_radius << ")\n";
        file << "avgdist: " << context.current_result_ref.avgdist << "\n";
        file << "points_count: " << context.current_result_ref.points_count << "\n";
        file << "valid_points_count: " << context.current_result_ref.valid_points_count << "\n\n";

        file << "Geometry Summary\n";
        file << "----------------\n";
        file << context.geometry_summary << "\n\n";

        file << "Image Overlay Summary\n";
        file << "---------------------\n";
        file << context.image_overlay_summary << "\n\n";

        file << "Runtime Objects\n";
        file << "---------------\n";

        for (const RuntimeObjectView& object : context.runtime_objects)
        {
            file << "* " << object.type << " " << object.name << "\n";
            file << "  declared_line: " << object.declared_line << "\n";
            file << "  exists_in_parser: " << (object.exists_in_parser ? "true" : "false") << "\n";
            file << "  runtime_state: " << object.runtime_state << "\n";
            file << "  last_runtime_status: " << object.last_runtime_status << "\n";
            file << "  last_method: " << object.last_method << "\n";
            file << "  last_update_line: " << object.last_update_line << "\n";
            file << "  display_summary: " << object.display_summary << "\n";
            file << "  visualizable: " << (object.visualizable ? "true" : "false") << "\n";
            file << "  stale: " << (object.stale ? "true" : "false") << "\n";
            file << "  roi_circle: "
                << object.circle_cx << ", "
                << object.circle_cy << ", "
                << object.circle_inner << ", "
                << object.circle_radius << "\n";
            file << "  measure_points_count: " << object.measure_points_count << "\n";
            file << "  valid_points_count: " << object.valid_points_count << "\n";
            file << "  fit_circle: "
                << object.fit_cx << ", "
                << object.fit_cy << ", r="
                << object.fit_radius << "\n";
            file << "  avgdist: " << object.fit_avgdist << "\n";
            file << "  display_version: " << object.display_version << "\n";

            if (object.type == "FindCircle")
            {
                file << "  circle_roi_outer_polyline: "
                     << (object.has_circle_roi_outer_polyline ? "true" : "false") << "\n";
                file << "  circle_roi_inner_polyline: "
                     << (object.has_circle_roi_inner_polyline ? "true" : "false") << "\n";
                file << "  circle_roi_segment_count: "
                     << object.circle_roi_segment_count << "\n";
                file << "  fit_circle_polyline: "
                     << (object.has_fit_circle_polyline ? "true" : "false") << "\n";
                file << "  fit_circle_segment_count: "
                     << object.fit_circle_segment_count << "\n";

                file << "  circle_measure_geometry_request_valid: "
                     << (object.circle_measure_geometry_request_valid ? "true" : "false")
                     << "\n";

                file << "  circle_measure_geometry_dirty: "
                     << (object.circle_measure_geometry_dirty ? "true" : "false")
                     << "\n";

                file << "  circle_measure_geometry_ready: "
                     << (object.circle_measure_geometry_ready ? "true" : "false")
                     << "\n";

                file << "  circle_measure_geometry_version: "
                     << object.circle_measure_geometry_version << "\n";

                file << "  circle_measure_geometry_built_version: "
                     << object.circle_measure_geometry_built_version << "\n";

                file << "  circle_scan_line_count: "
                     << object.circle_scan_line_count << "\n";

                file << "  circle_scan_line_length: "
                     << object.circle_scan_line_length << "\n";

                file << "  circle_process_width: "
                     << object.circle_process_width << "\n";

                file << "  circle_selected_edge_index: "
                     << object.circle_selected_edge_index << "\n";

                file << "  circle_candidate_runs_total: "
                     << object.circle_candidate_runs_total << "\n";

                file << "  circle_candidate_runs_max_per_line: "
                     << object.circle_candidate_runs_max_per_line << "\n";

                file << "  circle_selected_edge_hits: "
                     << object.circle_selected_edge_hits << "\n";

                file << "  circle_selected_edge_misses: "
                     << object.circle_selected_edge_misses << "\n";

                file << "  circle_selected_edge_radius_avg: "
                     << object.circle_selected_edge_radius_avg << "\n";

                file << "  circle_selected_edge_radius_min: "
                     << object.circle_selected_edge_radius_min << "\n";

                file << "  circle_selected_edge_radius_max: "
                     << object.circle_selected_edge_radius_max << "\n";

                file << "  circle_measure_image_ready: "
                     << (object.circle_measure_image_ready ? "true" : "false")
                     << "\n";

                file << "  circle_measure_image_size: "
                     << object.circle_measure_image_width << "x"
                     << object.circle_measure_image_height << "x"
                     << object.circle_measure_image_channels << "\n";

                file << "  circle_measure_backimage_ready: "
                     << (object.circle_measure_backimage_ready ? "true" : "false")
                     << "\n";

                file << "  circle_measure_findobject_ready: "
                     << (object.circle_measure_findobject_ready ? "true" : "false")
                     << "\n";

                file << "  circle_measure_source: "
                     << object.circle_measure_source << "\n";

                file << "  circle_measure_failure_stage: "
                     << object.circle_measure_failure_stage << "\n";

                file << "  circle_measure_detail: "
                     << object.circle_measure_detail << "\n";
            }

            if (object.type == "FindLine")
            {
                file << "  line_roi: "
                     << object.line_x0 << ", "
                     << object.line_y0 << " -> "
                     << object.line_x1 << ", "
                     << object.line_y1 << "\n";

                file << "  line_scale: " << object.line_scale << "\n";
                file << "  linegap: " << object.linegap << "\n";
                file << "  line_tool_wgap: " << object.line_tool_wgap << "\n";
                file << "  line_tool_hgap: " << object.line_tool_hgap << "\n";
                file << "  line_display_source: " << object.line_display_source << "\n";
                file << "  has_line_scan_box: "
                     << (object.has_line_scan_box ? "true" : "false") << "\n";
                file << "  line_scan_half_width: "
                     << object.line_scan_half_width << "\n";
                file << "  line_orientation: " << object.line_orientation << "\n";
                file << "  line_dx: " << object.line_dx << "\n";
                file << "  line_dy: " << object.line_dy << "\n";
                file << "  line_length: " << object.line_length << "\n";
                file << "  requested_tool_half_width: "
                     << object.requested_tool_half_width << "\n";
                file << "  effective_tool_half_width: "
                     << object.effective_tool_half_width << "\n";

                file << "  line_pointsw_count: "
                     << object.line_pointsw_count << "\n";
                file << "  line_pointsh_count: "
                     << object.line_pointsh_count << "\n";
                file << "  line_measure_points_count: "
                     << object.line_measure_points_count << "\n";
                file << "  valid_line_points_count: "
                     << object.valid_line_points_count << "\n";

                file << "  has_fit_line: "
                     << (object.has_fit_line ? "true" : "false") << "\n";
                file << "  fit_line: "
                     << object.fit_line_x0 << ", "
                     << object.fit_line_y0 << " -> "
                     << object.fit_line_x1 << ", "
                     << object.fit_line_y1 << "\n";

                file << "  line_scan_box_xy: "
                     << object.line_scan_box_xy[0] << ", "
                     << object.line_scan_box_xy[1] << " | "
                     << object.line_scan_box_xy[2] << ", "
                     << object.line_scan_box_xy[3] << " | "
                     << object.line_scan_box_xy[4] << ", "
                     << object.line_scan_box_xy[5] << " | "
                     << object.line_scan_box_xy[6] << ", "
                     << object.line_scan_box_xy[7] << "\n";

                file << "  line_avgdist: " << object.line_avgdist << "\n";
                file << "  line_fit_mode: " << object.line_fit_mode << "\n";
                file << "  line_fit_status: " << object.line_fit_status << "\n";
                file << "  line_measure_status: " << object.line_measure_status << "\n";
                file << "  line_measure_hint: " << object.line_measure_hint << "\n";
                file << "  line_filter_min_exceeds_component_p90: "
                     << (object.line_filter_min_exceeds_component_p90 ? "true" : "false") << "\n";
                file << "  line_measure_failure_hint: "
                     << object.line_measure_failure_hint << "\n";
                file << "  line_seek_points_count: " << object.line_seek_points_count << "\n";
                file << "  line_edgeband_count: " << object.line_edgeband_count << "\n";
                file << "  line_chain_length: " << object.line_chain_length << "\n";
                file << "  line_profile_point_count: " << object.line_profile_point_count << "\n";
                file << "  line_measure_failure_stage: " << object.line_measure_failure_stage << "\n";

                file << "  line_measure_image_ready: "
                     << (object.line_measure_image_ready ? "true" : "false") << "\n";

                file << "  line_measure_image_size: "
                     << object.line_measure_image_width << "x"
                     << object.line_measure_image_height << "x"
                     << object.line_measure_image_channels << "\n";

                file << "  line_measure_image_type: "
                     << object.line_measure_image_type << "\n";

                file << "  line_measure_image_source: "
                     << object.line_measure_image_source << "\n";

                file << "  line_measure_roi_intersects_image: "
                     << (object.line_measure_roi_intersects_image ? "true" : "false") << "\n";

                file << "  line_measure_roi_fully_inside_image: "
                     << (object.line_measure_roi_fully_inside_image ? "true" : "false") << "\n";

                file << "  line_measure_method: "
                     << object.line_measure_method << "\n";

                file << "  line_measure_threshold: "
                     << object.line_measure_threshold << "\n";

                file << "  line_measure_linegap: "
                     << object.line_measure_linegap << "\n";

                file << "  line_measure_wgap: "
                     << object.line_measure_wgap << "\n";

                file << "  line_measure_hgap: "
                     << object.line_measure_hgap << "\n";

                file << "  line_measure_backimage_ready: "
                     << (object.line_measure_backimage_ready ? "true" : "false") << "\n";

                file << "  line_measure_findobject_ready: "
                     << (object.line_measure_findobject_ready ? "true" : "false") << "\n";

                file << "  line_measure_objfilterset: "
                     << object.line_measure_objfilterset << "\n";

                file << "  line_measure_filter_borw: "
                     << object.line_measure_filter_borw << "\n";

                file << "  line_measure_filter_min: "
                     << object.line_measure_filter_min << "\n";

                file << "  line_measure_filter_max: "
                     << object.line_measure_filter_max << "\n";

                file << "  line_measure_filter_profile: "
                     << object.line_measure_filter_profile << "\n";

                file << "  line_measure_filter_explicit: "
                     << (object.line_measure_filter_explicit ? "true" : "false") << "\n";

                file << "  line_measure_effective_filter_borw: "
                     << object.line_measure_effective_filter_borw << "\n";

                file << "  line_measure_effective_filter_min: "
                     << object.line_measure_effective_filter_min << "\n";

                file << "  line_measure_effective_filter_max: "
                     << object.line_measure_effective_filter_max << "\n";

                file << "  line_measure_findobject_called: "
                     << (object.line_measure_findobject_called ? "true" : "false") << "\n";

                file << "  line_measure_findobject_skipped: "
                     << (object.line_measure_findobject_skipped ? "true" : "false") << "\n";

                file << "  line_measure_binary_foreground_pixels: "
                     << object.line_measure_binary_foreground_pixels << "\n";

                file << "  line_measure_binary_roi: "
                     << object.line_measure_binary_roi_width
                     << "x"
                     << object.line_measure_binary_roi_height
                     << "\n";

                file << "  line_measure_result_empty_reason: "
                     << object.line_measure_result_empty_reason << "\n";

                file << "  line_findobject_component_total: "
                     << object.line_findobject_component_total << "\n";

                file << "  line_findobject_component_accepted: "
                     << object.line_findobject_component_accepted << "\n";

                file << "  line_findobject_component_rejected_by_min: "
                     << object.line_findobject_component_rejected_by_min << "\n";

                file << "  line_findobject_component_rejected_by_max: "
                     << object.line_findobject_component_rejected_by_max << "\n";

                file << "  line_findobject_component_rejected_by_borw: "
                     << object.line_findobject_component_rejected_by_borw << "\n";

                file << "  line_findobject_area_min_observed: "
                     << object.line_findobject_area_min_observed << "\n";

                file << "  line_findobject_area_max_observed: "
                     << object.line_findobject_area_max_observed << "\n";

                file << "  line_findobject_area_mean_observed: "
                     << object.line_findobject_area_mean_observed << "\n";

                file << "  line_findobject_area_median: "
                     << object.line_findobject_area_median << "\n";

                file << "  line_findobject_area_p90: "
                     << object.line_findobject_area_p90 << "\n";

                file << "  line_measure_cc_selected_foreground: "
                     << object.line_measure_cc_selected_foreground << "\n";

                file << "  line_measure_cc_white_total: "
                     << object.line_measure_cc_white_total << "\n";
                file << "  line_measure_cc_white_accepted: "
                     << object.line_measure_cc_white_accepted << "\n";
                file << "  line_measure_cc_white_rejected_min: "
                     << object.line_measure_cc_white_rejected_min << "\n";
                file << "  line_measure_cc_white_area_median: "
                     << object.line_measure_cc_white_area_median << "\n";
                file << "  line_measure_cc_white_area_p90: "
                     << object.line_measure_cc_white_area_p90 << "\n";

                file << "  line_measure_cc_black_total: "
                     << object.line_measure_cc_black_total << "\n";
                file << "  line_measure_cc_black_accepted: "
                     << object.line_measure_cc_black_accepted << "\n";
                file << "  line_measure_cc_black_rejected_min: "
                     << object.line_measure_cc_black_rejected_min << "\n";
                file << "  line_measure_cc_black_area_median: "
                     << object.line_measure_cc_black_area_median << "\n";
                file << "  line_measure_cc_black_area_p90: "
                     << object.line_measure_cc_black_area_p90 << "\n";

                file << "  line_measure_cc_selected_total: "
                     << object.line_measure_cc_selected_total << "\n";
                file << "  line_measure_cc_selected_accepted: "
                     << object.line_measure_cc_selected_accepted << "\n";
                file << "  line_measure_cc_selected_rejected_min: "
                     << object.line_measure_cc_selected_rejected_min << "\n";
                file << "  line_measure_cc_selected_area_median: "
                     << object.line_measure_cc_selected_area_median << "\n";
                file << "  line_measure_cc_selected_area_p90: "
                     << object.line_measure_cc_selected_area_p90 << "\n";

                file << "  line_measure_profile_count: "
                     << object.line_measure_profile_count << "\n";

                file << "  line_measure_sampled_pixel_count: "
                     << object.line_measure_sampled_pixel_count << "\n";

                file << "  line_measure_gray_min: "
                     << object.line_measure_gray_min << "\n";

                file << "  line_measure_gray_max: "
                     << object.line_measure_gray_max << "\n";

                file << "  line_measure_gray_mean: "
                     << object.line_measure_gray_mean << "\n";

                file << "  line_measure_max_gradient: "
                     << object.line_measure_max_gradient << "\n";

                file << "  line_measure_input_failure_stage: "
                     << object.line_measure_input_failure_stage << "\n";

                file << "  line_measure_input_detail: "
                     << object.line_measure_input_detail << "\n";

                file << "  line_measure_source: "
                     << object.line_measure_source << "\n";

                file << "  line_measure_fallback_allowed: "
                     << (object.line_measure_fallback_allowed ? "true" : "false") << "\n";

                file << "  line_measure_fallback_used: "
                     << (object.line_measure_fallback_used ? "true" : "false") << "\n";

                file << "  line_measure_original_point_count: "
                     << object.line_measure_original_point_count << "\n";

                file << "  line_measure_original_edgeband_count: "
                     << object.line_measure_original_edgeband_count << "\n";

                file << "  line_measure_original_chain_length: "
                     << object.line_measure_original_chain_length << "\n";

                file << "  line_measure_original_failure_stage: "
                     << object.line_measure_original_failure_stage << "\n";

                file << "  line_measure_original_detail: "
                     << object.line_measure_original_detail << "\n";

                file << "  line_measure_geometry_request_valid: "
                     << (object.line_measure_geometry_request_valid ? "true" : "false")
                     << "\n";

                file << "  line_measure_geometry_dirty: "
                     << (object.line_measure_geometry_dirty ? "true" : "false")
                     << "\n";

                file << "  line_measure_geometry_ready: "
                     << (object.line_measure_geometry_ready ? "true" : "false")
                     << "\n";

                file << "  line_measure_geometry_version: "
                     << object.line_measure_geometry_version << "\n";

                file << "  line_measure_geometry_built_version: "
                     << object.line_measure_geometry_built_version << "\n";

                file << "  line_measure_geometry_half_width: "
                     << object.line_measure_geometry_half_width << "\n";

                file << "  line_original_scan_w_count: "
                     << object.line_original_scan_w_count << "\n";

                file << "  line_original_scan_h_count: "
                     << object.line_original_scan_h_count << "\n";

                file << "  line_original_scan_w_length: "
                     << object.line_original_scan_w_length << "\n";

                file << "  line_original_scan_h_length: "
                     << object.line_original_scan_h_length << "\n";

                file << "  line_original_process_width: "
                     << object.line_original_process_width << "\n";

                file << "  display_version: " << object.display_version << "\n";
            }

            if (object.type == "CircleRingGauge")
            {
                file << "  ring_outer_radius: " << object.ring_outer_radius << "\n";
                file << "  ring_inner_radius: " << object.ring_inner_radius << "\n";
                file << "  ring_thickness: " << object.ring_thickness << "\n";
                file << "  ring_center_distance: " << object.ring_center_distance << "\n";
                file << "  ring_concentric_ok: " << (object.ring_concentric_ok ? "true" : "false") << "\n";
                file << "  ring_inside_ok: " << (object.ring_inside_ok ? "true" : "false") << "\n";
                file << "  ring_thickness_ok: " << (object.ring_thickness_ok ? "true" : "false") << "\n";
                file << "  ring_score: " << object.ring_score << "\n";
                file << "  ring_status: " << object.ring_status << "\n";
                file << "  ring_reason: " << object.ring_reason << "\n";
                file << "  ring_result_ref: " << object.ring_result_ref << "\n";
            }

            file << "\n";
        }

        file << "Line Trace\n";
        file << "----------\n";

        for (const ScriptLineView& line : context.line_views)
        {
            file << line.line_no
                << " [" << line.status << "] "
                << line.statement
                << " | object=" << line.object
                << " | method=" << line.method
                << " | params=" << line.params
                << " | reason=" << line.reason
                << "\n";
        }

        outReason = "debug snapshot saved";
        return true;
    }
    catch (const std::exception& e)
    {
        outReason = std::string("debug snapshot exception: ") + e.what();
        return false;
    }
    catch (...)
    {
        outReason = "debug snapshot unknown exception";
        return false;
    }
}

bool SaveCxDebugSnapshotText(ManualTestContext& context,
    std::string& outPath,
    std::string& outReason)
{
    std::filesystem::path path = CxDebugSnapshotPath();
    outPath = path.string();
    return SaveCxDebugSnapshotText(context, path, outReason);
}

void ApplyCxParserExtDebugResultToManualConsole(
    ManualTestContext& context,
    const CxScriptSemanticBridgeResult& result)
{
    context.cxparser_ext_debug_ok = result.ok;
    context.cxparser_ext_debug_status = result.status;
    context.cxparser_ext_debug_reason = result.reason;
    context.debug_parser_output = result.raw_log;
    context.cxparser_ext_line_views = result.line_views;
    context.cxparser_ext_statement_views = result.statement_views;
    context.cxparser_ext_object_assignments = result.object_assignments;

    if (!result.ok)
    {
        context.debug_status = "CXPARSER_EXT_DEBUG_FAILED";
        context.debug_reason = result.reason;
        return;
    }

    context.debug_status = "CXPARSER_EXT_DEBUG_OK";
    context.debug_reason = result.status;

    for (const CxScriptObjectAssignmentView& assignment :
         result.object_assignments)
    {
        UpsertGlobalVariableView(
            context,
            assignment.lhs_type,
            assignment.lhs_variable,
            assignment.returned_object_ref,
            assignment.line_no,
            "cxparser_ext_debug_object_assignment");

        RuntimeObjectView& object = EnsureRuntimeObject(
            context,
            assignment.lhs_variable,
            assignment.lhs_type,
            assignment.line_no);

        object.exists_in_parser = true;
        object.type = assignment.lhs_type;
        object.runtime_state = "cxparser_ext_debug_assigned";
        object.last_runtime_status = assignment.status;
        object.last_method = assignment.method_name;
        object.last_update_line = assignment.line_no;
        object.display_summary =
            assignment.lhs_variable +
            " <= " +
            assignment.source_object +
            "." +
            assignment.method_name +
            "()";
        object.visualizable = false;
        object.visual_source = "cxparser_ext_debug";
        object.stale = false;
    }
}
