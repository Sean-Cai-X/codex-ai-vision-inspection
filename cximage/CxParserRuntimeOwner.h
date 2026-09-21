#ifndef CXIMAGE_CXPARSER_RUNTIME_OWNER_H
#define CXIMAGE_CXPARSER_RUNTIME_OWNER_H

#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <map>

#include "CxParserSnapshotTypes.h"

namespace mu
{
    class CxParserRuntime;
}

enum class CxParserDocumentKind
{
    AnnotationToolManifest,
    ShapeInteractionSuite,
    ScriptSuite,
    ScriptCatalog,
    EvidenceChain,
    ParameterProfile,
    ParamRegression
};

struct CxScriptSuiteRuntime;
struct CxScriptCatalogRuntime;
struct CxScriptEvidenceChainRuntime;
struct CxParameterProfileRuntime;
struct CxParamRegressionRuntime;

class CxParserExecutionGuard;

class CxParserRuntimeOwner
{
    friend class CxParserExecutionGuard;

public:
    CxParserRuntimeOwner();
    ~CxParserRuntimeOwner();

    CxParserRuntimeOwner(const CxParserRuntimeOwner&) = delete;
    CxParserRuntimeOwner& operator=(const CxParserRuntimeOwner&) = delete;
    CxParserRuntimeOwner(CxParserRuntimeOwner&&) = delete;
    CxParserRuntimeOwner& operator=(CxParserRuntimeOwner&&) = delete;

    bool Initialize(std::string& reason);

    bool ParseAnnotationToolManifest(
        const std::string& path,
        CxAnnotationToolManifestSnapshot& snapshot,
        std::string& reason);

    bool ParseShapeInteractionSuite(
        const std::string& path,
        CxShapeTestSuiteSnapshot& snapshot,
        std::string& reason);

    bool ParseScriptSuite(
        const std::string& path,
        CxScriptSuiteRuntime& snapshot,
        std::string& reason);

    bool ParseScriptCatalog(
        const std::string& path,
        CxScriptCatalogRuntime& snapshot,
        std::string& reason);

    bool ParseEvidenceChain(
        const std::string& path,
        CxScriptEvidenceChainRuntime& snapshot,
        std::string& reason);

    bool ParseParameterProfile(
        const std::string& path,
        CxParameterProfileRuntime& snapshot,
        std::string& reason);

    bool ParseParamRegression(
        const std::string& path,
        CxParamRegressionRuntime& snapshot,
        std::string& reason);

    bool IsExecuting() const;

    bool Compile(const std::string& source, std::string& reason);
    bool Compile(const char* source);
    bool ExecuteScript(const std::string& source, std::string& reason);
    bool CompileScriptOnly(const std::string& source, std::string& reason);
    void ConfigureStreams(std::ostream* runtime_stream, std::ostream* code_stream);
    int ObjectCount(const std::string& type) const;
    std::string ObjectName(const std::string& type, int index) const;
    void* GetClassObj(const std::string& strclass, const std::string& strobj);
    void* GetClassObj(const std::string& strclass, const int& iobjnum);
    void* GetDoubleValue(const std::string& strname);
    bool QueryExternalDouble(const std::string& name, double& value) const;
    void ClearAll();
    void StopRun();
    bool IsObjectVar(const char* sz);
    bool DefineExternalDouble(
        const std::string& name,
        double* value,
        std::string& reason);
    bool DefineStringConstant(
        const std::string& name,
        const std::string& value,
        std::string& reason);

private:
    bool ReadScript(
        const std::string& path,
        std::string& source,
        std::string& reason) const;

    bool BeginExecution(
        CxParserDocumentKind kind,
        const std::string& source_path,
        std::string& reason);

    bool EnsureBindings(
        CxParserDocumentKind kind,
        std::string& reason);

    void EndExecution();

private:
    std::unique_ptr<mu::CxParserRuntime> m_runtime;
    std::ostringstream m_runtime_stream;
    std::ostringstream m_code_stream;
    bool m_initialized = false;
    bool m_executing = false;
    bool m_annotation_bindings_registered = false;
    bool m_shape_bindings_registered = false;
    bool m_suite_bindings_registered = false;
    bool m_catalog_bindings_registered = false;
    bool m_evidence_chain_bindings_registered = false;
    bool m_parameter_bindings_registered = false;
    bool m_param_regression_bindings_registered = false;
    // Numeric locals declared by the currently executing CxScript. Their
    // addresses must remain stable until RunCollectedScript() completes.
    std::map<std::string, double> m_script_local_values;
};

#endif
