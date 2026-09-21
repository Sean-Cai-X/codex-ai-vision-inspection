#include "CxScriptTypeRegistry.h"

std::string_view CxScriptTypeName(
    CxScriptTypeId typeId) noexcept
{
    switch (typeId)
    {
    case CxScriptTypeId::Image:
        return "Image";

    case CxScriptTypeId::FindLine:
        return "FindLine";

    case CxScriptTypeId::FindCircle:
        return "FindCircle";

    case CxScriptTypeId::FindEllipse:
        return "FindEllipse";

    case CxScriptTypeId::FindRect:
        return "FindRect";

    case CxScriptTypeId::FindObject:
        return "FindObject";

    case CxScriptTypeId::FindSegmentation:
        return "FindSegmentation";

    case CxScriptTypeId::FastMatch:
        return "FastMatch";

    case CxScriptTypeId::CxTextInspect:
        return "CxTextInspect";

    case CxScriptTypeId::FastMatchDiagnostic:
        return "FastMatchDiagnostic";

    case CxScriptTypeId::CircleRingGauge:
        return "CircleRingGauge";

    case CxScriptTypeId::GridPatternClassTool:
        return "GridPatternClassTool";

    case CxScriptTypeId::RegionPatternTool:
        return "RegionPatternTool";

    case CxScriptTypeId::Shape:
        return "Shape";

    case CxScriptTypeId::ShapeBase:
        return "ShapeBase";

    case CxScriptTypeId::PointsShape:
        return "PointsShape";

    case CxScriptTypeId::LineShape:
        return "LineShape";

    case CxScriptTypeId::SmartDouble:
        return "SmartDouble";

    default:
        return {};
    }
}

std::optional<CxScriptTypeId>
ParseCanonicalCxScriptTypeName(
    std::string_view name) noexcept
{
    if (name == "Image")
        return CxScriptTypeId::Image;

    if (name == "FindLine")
        return CxScriptTypeId::FindLine;

    if (name == "FindCircle")
        return CxScriptTypeId::FindCircle;

    if (name == "FindEllipse")
        return CxScriptTypeId::FindEllipse;

    if (name == "FindRect")
        return CxScriptTypeId::FindRect;

    if (name == "FindObject")
        return CxScriptTypeId::FindObject;

    if (name == "FindSegmentation")
        return CxScriptTypeId::FindSegmentation;

    if (name == "FastMatch")
        return CxScriptTypeId::FastMatch;

    if (name == "CxTextInspect")
        return CxScriptTypeId::CxTextInspect;

    if (name == "FastMatchDiagnostic")
        return CxScriptTypeId::FastMatchDiagnostic;

    if (name == "CircleRingGauge")
        return CxScriptTypeId::CircleRingGauge;

    if (name == "GridPatternClassTool")
        return CxScriptTypeId::GridPatternClassTool;

    if (name == "RegionPatternTool")
        return CxScriptTypeId::RegionPatternTool;

    if (name == "Shape")
        return CxScriptTypeId::Shape;

    if (name == "ShapeBase")
        return CxScriptTypeId::ShapeBase;

    if (name == "PointsShape")
        return CxScriptTypeId::PointsShape;

    if (name == "LineShape")
        return CxScriptTypeId::LineShape;

    if (name == "SmartDouble")
        return CxScriptTypeId::SmartDouble;

    return std::nullopt;
}

std::optional<std::string_view>
DeprecatedTypeNameSuggestion(
    std::string_view name) noexcept
{
    if (name == "Findline" ||
        name == "findline" ||
        name == "find_line")
    {
        return "FindLine";
    }

    if (name == "findcircle" ||
        name == "find_circle")
    {
        return "FindCircle";
    }

    if (name == "findellipse")
    {
        return "FindEllipse";
    }

    if (name == "Findrect" ||
        name == "findrect")
    {
        return "FindRect";
    }

    if (name == "Match" ||
        name == "fastmatch" ||
        name == "CFastMatch")
    {
        return "FastMatch";
    }

    return std::nullopt;
}
