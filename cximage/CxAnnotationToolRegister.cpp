#include "pch.h"
#include "CxAnnotationToolRuntime.h"
#include "muParser.h"

static double CxAnnotationTool_reset(double)
{
    CxAnnotationToolRuntime::Reset();
    return 0.0;
}

static double CxAnnotationTool_add(const char* id)
{
    CxAnnotationToolRuntime::Add(id ? id : "");
    return 0.0;
}

static double CxAnnotationTool_setlabel(const char* label)
{
    CxAnnotationToolRuntime::SetLabel(label ? label : "");
    return 0.0;
}

static double CxAnnotationTool_setshape(const char* shape_type)
{
    CxAnnotationToolRuntime::SetShape(shape_type ? shape_type : "");
    return 0.0;
}

static double CxAnnotationTool_setrole(const char* semantic_role)
{
    CxAnnotationToolRuntime::SetRole(semantic_role ? semantic_role : "");
    return 0.0;
}

static double CxAnnotationTool_setinteraction(const char* interaction)
{
    CxAnnotationToolRuntime::SetInteraction(interaction ? interaction : "");
    return 0.0;
}

static double CxAnnotationTool_setownertool(const char* owner_tool)
{
    CxAnnotationToolRuntime::SetOwnerTool(owner_tool ? owner_tool : "");
    return 0.0;
}

static double CxAnnotationTool_setownerbinding(const char* owner_binding)
{
    CxAnnotationToolRuntime::SetOwnerBinding(owner_binding ? owner_binding : "");
    return 0.0;
}

static double CxAnnotationTool_setmanualvisible(int visible)
{
    CxAnnotationToolRuntime::SetManualVisible(visible);
    return 0.0;
}

static double CxAnnotationTool_seteditable(int editable)
{
    CxAnnotationToolRuntime::SetEditable(editable);
    return 0.0;
}

void RegisterCxAnnotationToolBindings(mu::Parser& parser)
{
    parser.DefineFun("CxAnnotationTool_reset", (mu::fun_type1)&CxAnnotationTool_reset);
    parser.DefineFun("CxAnnotationTool_add", (mu::strfun_type1)&CxAnnotationTool_add);
    parser.DefineFun("CxAnnotationTool_setlabel", (mu::strfun_type1)&CxAnnotationTool_setlabel);
    parser.DefineFun("CxAnnotationTool_setshape", (mu::strfun_type1)&CxAnnotationTool_setshape);
    parser.DefineFun("CxAnnotationTool_setrole", (mu::strfun_type1)&CxAnnotationTool_setrole);
    parser.DefineFun("CxAnnotationTool_setinteraction", (mu::strfun_type1)&CxAnnotationTool_setinteraction);
    parser.DefineFun("CxAnnotationTool_setownertool", (mu::strfun_type1)&CxAnnotationTool_setownertool);
    parser.DefineFun("CxAnnotationTool_setownerbinding", (mu::strfun_type1)&CxAnnotationTool_setownerbinding);
    parser.DefineFun("CxAnnotationTool_setmanualvisible", (mu::fun_type1)&CxAnnotationTool_setmanualvisible);
    parser.DefineFun("CxAnnotationTool_seteditable", (mu::fun_type1)&CxAnnotationTool_seteditable);
}