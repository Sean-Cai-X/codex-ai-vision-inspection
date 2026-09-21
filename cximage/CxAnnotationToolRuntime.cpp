#include "pch.h"
#include "CxAnnotationToolRuntime.h"

namespace {
    std::vector<CxAnnotationToolSpec> g_tools;
    CxAnnotationToolSpec* g_current = nullptr;
}

void CxAnnotationToolRuntime::Reset()
{
    g_tools.clear();
    g_current = nullptr;
}

void CxAnnotationToolRuntime::Add(const std::string& id)
{
    g_tools.push_back({});
    g_tools.back().id = id;
    g_current = &g_tools.back();
}

void CxAnnotationToolRuntime::SetLabel(const std::string& label)
{
    if (g_current)
        g_current->label = label;
}

void CxAnnotationToolRuntime::SetShape(const std::string& shape_type)
{
    if (g_current)
        g_current->shape_type = shape_type;
}

void CxAnnotationToolRuntime::SetRole(const std::string& semantic_role)
{
    if (g_current)
        g_current->semantic_role = semantic_role;
}

void CxAnnotationToolRuntime::SetInteraction(const std::string& interaction)
{
    if (g_current)
        g_current->interaction = interaction;
}

void CxAnnotationToolRuntime::SetOwnerTool(const std::string& owner_tool)
{
    if (g_current)
        g_current->owner_tool = owner_tool;
}

void CxAnnotationToolRuntime::SetOwnerBinding(const std::string& owner_binding)
{
    if (g_current)
        g_current->owner_binding = owner_binding;
}

void CxAnnotationToolRuntime::SetSourceScript(const std::string& source_script)
{
    if (g_current)
        g_current->source_script = source_script;
}

void CxAnnotationToolRuntime::SetManualVisible(int visible)
{
    if (g_current)
        g_current->manual_visible = (visible != 0);
}

void CxAnnotationToolRuntime::SetEditable(int editable)
{
    if (g_current)
        g_current->editable = (editable != 0);
}

const std::vector<CxAnnotationToolSpec>& CxAnnotationToolRuntime::Tools()
{
    return g_tools;
}

const CxAnnotationToolSpec* CxAnnotationToolRuntime::FindById(const std::string& id)
{
    for (const auto& tool : g_tools)
    {
        if (tool.id == id)
            return &tool;
    }
    return nullptr;
}

CxAnnotationToolSpec* CxAnnotationToolRuntime::Current()
{
    return g_current;
}