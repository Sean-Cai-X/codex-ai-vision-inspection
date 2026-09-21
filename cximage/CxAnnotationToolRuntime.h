#ifndef CXIMAGE_CX_ANNOTATION_TOOL_RUNTIME_H
#define CXIMAGE_CX_ANNOTATION_TOOL_RUNTIME_H

#include <string>
#include <vector>

struct CxAnnotationToolSpec {
    std::string id;
    std::string label;
    std::string shape_type;
    std::string semantic_role;
    std::string interaction;
    std::string owner_tool;
    std::string owner_binding;
    std::string source_script;
    bool manual_visible = true;
    bool editable = true;
};

class CxAnnotationToolRuntime
{
public:
    static void Reset();
    static void Add(const std::string& id);
    static void SetLabel(const std::string& label);
    static void SetShape(const std::string& shape_type);
    static void SetRole(const std::string& semantic_role);
    static void SetInteraction(const std::string& interaction);
    static void SetOwnerTool(const std::string& owner_tool);
    static void SetOwnerBinding(const std::string& owner_binding);
    static void SetSourceScript(const std::string& source_script);
    static void SetManualVisible(int visible);
    static void SetEditable(int editable);

    static const std::vector<CxAnnotationToolSpec>& Tools();
    static const CxAnnotationToolSpec* FindById(const std::string& id);

private:
    static CxAnnotationToolSpec* Current();
};

#endif