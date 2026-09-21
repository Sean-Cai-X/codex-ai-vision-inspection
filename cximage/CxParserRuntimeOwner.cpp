#include "pch.h"
#include "CxParserRuntimeOwner.h"
#include "CxAnnotationToolRegister.h"
#include "CxShapeTestRegister.h"
#include "CxScriptSuiteRuntime.h"
#include "CxScriptSuiteRegister.h"
#include "CxScriptCatalogRuntime.h"
#include "CxScriptCatalogRegister.h"
#include "CxScriptEvidenceChainRuntime.h"
#include "CxScriptEvidenceChainRegister.h"
#include "CxParameterProfileRuntime.h"
#include "CxParameterProfileRegister.h"
#include "CxParamRegressionRuntime.h"
#include "CxParamRegressionRegister.h"
#include "ParserClass.h"
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>

namespace
{
const char* const kCxScriptNumericTypes[] = { "int ", "double ", "float " };

bool FindNumericLocalDeclaration(
    const std::string& line,
    std::size_t& type_begin,
    std::size_t& type_length,
    std::string& name)
{
    type_begin = line.find_first_not_of(" \t");
    if (type_begin == std::string::npos)
        return false;

    type_length = 0;
    for (const char* type : kCxScriptNumericTypes)
    {
        const std::size_t length = std::strlen(type);
        if (line.compare(type_begin, length, type) == 0)
        {
            type_length = length;
            break;
        }
    }
    if (type_length == 0)
        return false;

    const std::size_t name_begin = type_begin + type_length;
    std::size_t name_end = name_begin;
    while (name_end < line.size() &&
           (std::isalnum(static_cast<unsigned char>(line[name_end])) ||
            line[name_end] == '_'))
    {
        ++name_end;
    }
    if (name_end == name_begin)
        return false;

    name = line.substr(name_begin, name_end - name_begin);
    return true;
}

std::string PrepareNumericLocals(
    const std::string& source,
    std::map<std::string, double>& storage)
{
    std::istringstream input(source);
    std::ostringstream prepared;
    std::string line;
    while (std::getline(input, line))
    {
        std::size_t type_begin = 0;
        std::size_t type_length = 0;
        std::string name;
        if (FindNumericLocalDeclaration(
                line, type_begin, type_length, name))
        {
            storage.emplace(name, 0.0);
            line.erase(type_begin, type_length);
        }
        prepared << line << '\n';
    }
    return prepared.str();
}

std::string RemoveCommentOnlyLines(const std::string& source)
{
    std::istringstream input(source);
    std::ostringstream out;
    std::string line;
    while (std::getline(input, line))
    {
        const std::size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos)
            continue;
        if (line.compare(first, 2, "//") == 0)
            continue;
        out << line << '\n';
    }
    return out.str();
}
}

class CxParserExecutionGuard
{
public:
    explicit CxParserExecutionGuard(CxParserRuntimeOwner& owner)
        : m_owner(owner)
    {
    }

    ~CxParserExecutionGuard()
    {
        m_owner.EndExecution();
    }

private:
    CxParserRuntimeOwner& m_owner;
};

CxParserRuntimeOwner::CxParserRuntimeOwner()
{
}

CxParserRuntimeOwner::~CxParserRuntimeOwner()
{
}

bool CxParserRuntimeOwner::Initialize(std::string& reason)
{
    if (m_initialized)
        return true;

    try
    {
        m_runtime = std::make_unique<mu::CxParserRuntime>();
        m_runtime->SetStream(&m_runtime_stream);
        m_runtime->SetCreateCodeStream(&m_code_stream);
        m_runtime->ParserInitialClassFunction(0);

        m_initialized = true;
        return true;
    }
    catch (...)
    {
        m_runtime.reset();
        reason = "parser runtime initialization failed";
        return false;
    }
}

bool CxParserRuntimeOwner::ReadScript(
    const std::string& path,
    std::string& source,
    std::string& reason) const
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        reason = "cannot open script file: " + path;
        return false;
    }

    source = std::string(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>());

    if (source.empty())
    {
        reason = "script file is empty: " + path;
        return false;
    }

    return true;
}

bool CxParserRuntimeOwner::BeginExecution(
    CxParserDocumentKind kind,
    const std::string& source_path,
    std::string& reason)
{
    if (!m_initialized)
    {
        reason = "parser owner is not initialized";
        return false;
    }

    if (m_executing)
    {
        reason = "parser re-entry is forbidden in phase 1";
        return false;
    }

    if (!EnsureBindings(kind, reason))
        return false;

    m_executing = true;
    return true;
}

bool CxParserRuntimeOwner::EnsureBindings(
    CxParserDocumentKind kind,
    std::string& reason)
{
    try
    {
        switch (kind)
        {
        case CxParserDocumentKind::AnnotationToolManifest:
            if (!m_annotation_bindings_registered)
            {
                RegisterCxAnnotationToolBindings(m_runtime->m_parser);
                m_annotation_bindings_registered = true;
            }
            break;
        case CxParserDocumentKind::ShapeInteractionSuite:
            if (!m_shape_bindings_registered)
            {
                RegisterCxShapeTestBindings(m_runtime->m_parser);
                m_shape_bindings_registered = true;
            }
            break;
        case CxParserDocumentKind::ScriptSuite:
            if (!m_suite_bindings_registered)
            {
                RegisterCxScriptSuiteBindings(m_runtime->m_parser);
                m_suite_bindings_registered = true;
            }
            break;
        case CxParserDocumentKind::ScriptCatalog:
            if (!m_catalog_bindings_registered)
            {
                RegisterCxScriptCatalogBindings(m_runtime->m_parser);
                m_catalog_bindings_registered = true;
            }
            break;
        case CxParserDocumentKind::EvidenceChain:
            if (!m_evidence_chain_bindings_registered)
            {
                RegisterCxScriptEvidenceChainBindings(m_runtime->m_parser);
                m_evidence_chain_bindings_registered = true;
            }
            break;
        case CxParserDocumentKind::ParameterProfile:
            if (!m_parameter_bindings_registered)
            {
                RegisterCxParameterProfileBindings(m_runtime->m_parser);
                m_parameter_bindings_registered = true;
            }
            break;
        case CxParserDocumentKind::ParamRegression:
            if (!m_param_regression_bindings_registered)
            {
                RegisterCxParamRegressionBindings(m_runtime->m_parser);
                m_param_regression_bindings_registered = true;
            }
            break;
        }
    }
    catch (const mu::Parser::exception_type& error)
    {
        reason = "binding registration failed: " + std::string(error.GetMsg());
        return false;
    }
    catch (const std::exception& error)
    {
        reason = "binding registration failed: " + std::string(error.what());
        return false;
    }
    catch (...)
    {
        reason = "binding registration failed: unknown exception";
        return false;
    }
    return true;
}

void CxParserRuntimeOwner::EndExecution()
{
    m_executing = false;
}

bool CxParserRuntimeOwner::IsExecuting() const
{
    return m_executing;
}

bool CxParserRuntimeOwner::Compile(const std::string& source, std::string& reason)
{
    if (!m_initialized)
    {
        reason = "parser owner is not initialized";
        return false;
    }

    try
    {
        return m_runtime->Compile(source.c_str());
    }
    catch (...)
    {
        reason = "compile failed";
        return false;
    }
}

bool CxParserRuntimeOwner::Compile(const char* source)
{
    if (!m_initialized || !m_runtime)
        return false;

    try
    {
        return m_runtime->Compile(source);
    }
    catch (...)
    {
        return false;
    }
}

bool CxParserRuntimeOwner::ExecuteScript(
    const std::string& source,
    std::string& reason)
{
    if (!m_initialized || !m_runtime)
    {
        reason = "parser owner is not initialized";
        return false;
    }

    if (!m_script_local_values.empty())
    {
        reason = "parser runtime must be cleared before executing another script";
        return false;
    }

    // Local numeric declarations are bound into muParser through DefineVar().
    // Keep the backing storage alive until ClearAll(), which clears parser
    // bindings before releasing these values.
    const std::string prepared =
        PrepareNumericLocals(source, m_script_local_values);
    for (auto& local : m_script_local_values)
    {
        if (!DefineExternalDouble(local.first, &local.second, reason))
        {
            reason = "failed to bind CxScript local '" + local.first +
                     "': " + reason;
            return false;
        }
    }

    if (!m_runtime->CompileCollectedScript(prepared, reason))
        return false;

    if (!m_runtime->RunCollectedScript(reason))
        return false;

    reason.clear();
    return true;
}

bool CxParserRuntimeOwner::CompileScriptOnly(
    const std::string& source,
    std::string& reason)
{
    if (!m_initialized || !m_runtime)
    {
        reason = "parser owner is not initialized";
        return false;
    }

    if (!m_script_local_values.empty())
    {
        reason = "parser runtime must be cleared before compiling another script";
        return false;
    }

    // Local numeric declarations are bound into muParser through DefineVar().
    // Keep the backing storage alive until ClearAll(), which clears parser
    // bindings before releasing these values.
    const std::string prepared =
        PrepareNumericLocals(source, m_script_local_values);

    for (auto& local : m_script_local_values)
    {
        if (!DefineExternalDouble(local.first, &local.second, reason))
        {
            reason = "failed to bind CxScript local '" + local.first +
                     "': " + reason;
            return false;
        }
    }

    if (!m_runtime->CompileCollectedScript(prepared, reason))
        return false;

    reason.clear();
    return true;
}

void CxParserRuntimeOwner::ConfigureStreams(std::ostream* runtime_stream, std::ostream* code_stream)
{
    if (m_runtime)
    {
        m_runtime->SetStream(runtime_stream);
        m_runtime->SetCreateCodeStream(code_stream);
    }
}

int CxParserRuntimeOwner::ObjectCount(const std::string& type) const
{
    if (!m_runtime)
        return 0;
    return m_runtime->GetClassObjSum(type);
}

std::string CxParserRuntimeOwner::ObjectName(
    const std::string& type,
    int index) const
{
    if (!m_runtime)
        return {};
    return m_runtime->GetClassObjName(type, index);
}

void* CxParserRuntimeOwner::GetClassObj(const std::string& strclass, const std::string& strobj)
{
    if (!m_runtime)
        return nullptr;
    return m_runtime->GetClassObj(strclass, strobj);
}

void* CxParserRuntimeOwner::GetClassObj(const std::string& strclass, const int& iobjnum)
{
    if (!m_runtime)
        return nullptr;
    return m_runtime->GetClassObj(strclass, iobjnum);
}

void* CxParserRuntimeOwner::GetDoubleValue(const std::string& strname)
{
    if (!m_runtime)
        return nullptr;
    return m_runtime->GetDoubleValue(strname);
}

bool CxParserRuntimeOwner::QueryExternalDouble(
    const std::string& name,
    double& value) const
{
    if (!m_runtime)
        return false;

    const mu::varmap_type vars = m_runtime->m_parser.GetVar();
    const auto it = vars.find(name);
    if (it == vars.end() || it->second == nullptr)
        return false;

    value = *(it->second);
    return true;
}

void CxParserRuntimeOwner::ClearAll()
{
    if (m_runtime)
        m_runtime->ClearAll();
    m_script_local_values.clear();
}

void CxParserRuntimeOwner::StopRun()
{
    if (m_runtime)
        m_runtime->StopRun();
}

bool CxParserRuntimeOwner::IsObjectVar(const char* sz)
{
    if (!m_runtime)
        return false;
    return m_runtime->IsObjectVar(sz);
}

bool CxParserRuntimeOwner::DefineExternalDouble(
    const std::string& name,
    double* value,
    std::string& reason)
{
    if (!m_initialized || !m_runtime)
    {
        reason = "parser owner is not initialized";
        return false;
    }
    if (name.empty() || value == nullptr)
    {
        reason = "external numeric input is invalid";
        return false;
    }
    try
    {
        m_runtime->m_parser.DefineVar(name, value);
        reason.clear();
        return true;
    }
    catch (...)
    {
        reason = "failed to bind external numeric input: " + name;
        return false;
    }
}

bool CxParserRuntimeOwner::DefineStringConstant(
    const std::string& name,
    const std::string& value,
    std::string& reason)
{
    if (!m_initialized || !m_runtime)
    {
        reason = "parser owner is not initialized";
        return false;
    }
    if (name.empty())
    {
        reason = "external string input name is empty";
        return false;
    }
    try
    {
        m_runtime->DefineStringConstant(name, value);
        reason.clear();
        return true;
    }
    catch (...)
    {
        reason = "failed to bind external string input: " + name;
        return false;
    }
}

bool CxParserRuntimeOwner::ParseAnnotationToolManifest(
    const std::string& path,
    CxAnnotationToolManifestSnapshot& snapshot,
    std::string& reason)
{
    snapshot = {};

    std::string source;
    if (!ReadScript(path, source, reason))
        return false;

    if (!BeginExecution(
            CxParserDocumentKind::AnnotationToolManifest,
            path,
            reason))
        return false;

    CxParserExecutionGuard guard(*this);

    CxAnnotationToolRuntime::Reset();

    try
    {
        if (!m_runtime->Compile(source.c_str()))
        {
            reason = "annotation manifest Compile failed";
            CxAnnotationToolRuntime::Reset();
            return false;
        }

        snapshot.source_path = path;
        snapshot.tools = CxAnnotationToolRuntime::Tools();
    }
    catch (const mu::Parser::exception_type& error)
    {
        reason = "annotation manifest parse error: " +
                 std::string(error.GetMsg());

        CxAnnotationToolRuntime::Reset();
        return false;
    }
    catch (const std::exception& e)
    {
        reason = "annotation manifest execution error: " +
                 std::string(e.what());

        CxAnnotationToolRuntime::Reset();
        return false;
    }
    catch (...)
    {
        reason = "unknown annotation manifest execution failure";

        CxAnnotationToolRuntime::Reset();
        return false;
    }

    CxAnnotationToolRuntime::Reset();

    if (snapshot.tools.empty())
    {
        reason = "annotation manifest produced no tools";
        return false;
    }

    return true;
}

bool CxParserRuntimeOwner::ParseShapeInteractionSuite(
    const std::string& path,
    CxShapeTestSuiteSnapshot& snapshot,
    std::string& reason)
{
    snapshot = {};

    std::string source;
    if (!ReadScript(path, source, reason))
        return false;

    if (!BeginExecution(
            CxParserDocumentKind::ShapeInteractionSuite,
            path,
            reason))
        return false;

    CxParserExecutionGuard guard(*this);

    CxShapeTestRuntime::Reset();

    try
    {
        if (!m_runtime->Compile(source.c_str()))
        {
            reason = "shape suite Compile failed";
            CxShapeTestRuntime::Reset();
            return false;
        }

        snapshot.source_path = path;
        snapshot.cases = CxShapeTestRuntime::Cases();
    }
    catch (const mu::Parser::exception_type& error)
    {
        reason = "shape suite parse error: " +
                 std::string(error.GetMsg());

        CxShapeTestRuntime::Reset();
        return false;
    }
    catch (const std::exception& e)
    {
        reason = "shape suite execution error: " +
                 std::string(e.what());

        CxShapeTestRuntime::Reset();
        return false;
    }
    catch (...)
    {
        reason = "unknown shape suite execution failure";

        CxShapeTestRuntime::Reset();
        return false;
    }

    CxShapeTestRuntime::Reset();

    if (snapshot.cases.empty())
    {
        reason = "shape suite produced no cases";
        return false;
    }

    return true;
}

bool CxParserRuntimeOwner::ParseScriptSuite(
    const std::string& path,
    CxScriptSuiteRuntime& snapshot,
    std::string& reason)
{
    snapshot = {};
    std::string source;
    if (!ReadScript(path, source, reason) ||
        !BeginExecution(CxParserDocumentKind::ScriptSuite, path, reason))
        return false;

    CxParserExecutionGuard guard(*this);
    g_cxscript_suite = {};
    g_current_suite_case = nullptr;

    try
    {
        if (!m_runtime->Compile(source.c_str()))
        {
            reason = "suite Compile failed";
            return false;
        }
        snapshot = g_cxscript_suite;
    }
    catch (const mu::Parser::exception_type& error)
    {
        reason = "suite parse error: " + std::string(error.GetMsg());
        return false;
    }

    g_cxscript_suite = {};
    g_current_suite_case = nullptr;
    if (snapshot.cases.empty())
    {
        reason = "suite produced no cases";
        return false;
    }
    return true;
}

bool CxParserRuntimeOwner::ParseScriptCatalog(
    const std::string& path,
    CxScriptCatalogRuntime& snapshot,
    std::string& reason)
{
    snapshot = {};
    std::string source;
    if (!ReadScript(path, source, reason) ||
        !BeginExecution(CxParserDocumentKind::ScriptCatalog, path, reason))
        return false;

    CxParserExecutionGuard guard(*this);
    g_cxscript_catalog = {};
    g_current_catalog_entry = nullptr;

    try
    {
        if (!m_runtime->Compile(source.c_str()))
        {
            reason = "catalog Compile failed";
            return false;
        }
        snapshot = g_cxscript_catalog;
    }
    catch (const mu::Parser::exception_type& error)
    {
        reason = "catalog parse error: " + std::string(error.GetMsg());
        return false;
    }

    g_cxscript_catalog = {};
    g_current_catalog_entry = nullptr;
    if (snapshot.scripts.empty())
    {
        reason = "catalog produced no entries";
        return false;
    }
    return true;
}
bool CxParserRuntimeOwner::ParseEvidenceChain(
    const std::string& path,
    CxScriptEvidenceChainRuntime& snapshot,
    std::string& reason)
{
    snapshot.Clear();
    std::string source;
    if (!ReadScript(path, source, reason) ||
        !BeginExecution(CxParserDocumentKind::EvidenceChain, path, reason))
        return false;

    CxParserExecutionGuard guard(*this);
    g_cxscript_evidence_chain.Clear();
    g_current_evidence_case = nullptr;

    try
    {
        if (!m_runtime->Compile(source.c_str()))
        {
            reason = "evidence chain Compile failed";
            return false;
        }
        snapshot = g_cxscript_evidence_chain;
    }
    catch (const mu::Parser::exception_type& error)
    {
        reason = "evidence chain parse error: " + std::string(error.GetMsg());
        return false;
    }

    g_cxscript_evidence_chain.Clear();
    g_current_evidence_case = nullptr;
    if (snapshot.cases.empty())
    {
        reason = "evidence chain produced no cases";
        return false;
    }
    return true;
}

bool CxParserRuntimeOwner::ParseParameterProfile(
    const std::string& path,
    CxParameterProfileRuntime& snapshot,
    std::string& reason)
{
    snapshot = {};
    std::string source;
    if (!ReadScript(path, source, reason) ||
        !BeginExecution(CxParserDocumentKind::ParameterProfile, path, reason))
        return false;

    CxParserExecutionGuard guard(*this);
    g_cxscript_parameter_profile = {};
    g_current_parameter_profile = nullptr;

    try
    {
        if (!m_runtime->Compile(source.c_str()))
        {
            reason = "parameter profile Compile failed";
            return false;
        }
        snapshot = g_cxscript_parameter_profile;
    }
    catch (const mu::Parser::exception_type& error)
    {
        reason = "parameter profile parse error: " + std::string(error.GetMsg());
        return false;
    }

    g_cxscript_parameter_profile = {};
    g_current_parameter_profile = nullptr;
    if (snapshot.profiles.empty())
    {
        reason = "parameter profile document produced no profiles";
        return false;
    }
    return true;
}

bool CxParserRuntimeOwner::ParseParamRegression(
    const std::string& path,
    CxParamRegressionRuntime& snapshot,
    std::string& reason)
{
    snapshot = {};
    std::string source;
    if (!ReadScript(path, source, reason) ||
        !BeginExecution(CxParserDocumentKind::ParamRegression, path, reason))
        return false;

    CxParserExecutionGuard guard(*this);
    g_cxscript_param_regression.Clear();
    g_current_param_candidate = nullptr;
    g_current_param_range = nullptr;

    try
    {
        const std::string preparedSource = RemoveCommentOnlyLines(source);
        if (!m_runtime->Compile(preparedSource.c_str()))
        {
            reason = "param regression Compile failed";
            return false;
        }
        snapshot = g_cxscript_param_regression;
    }
    catch (const mu::Parser::exception_type& error)
    {
        reason = "param regression parse error: " + std::string(error.GetMsg());
        return false;
    }

    g_cxscript_param_regression.Clear();
    g_current_param_candidate = nullptr;
    g_current_param_range = nullptr;
    if (snapshot.task.task_id.empty() && snapshot.candidates.empty() && snapshot.range_set.ranges.empty())
    {
        reason = "param regression document produced no task, ranges or candidates";
        return false;
    }
    return true;
}
