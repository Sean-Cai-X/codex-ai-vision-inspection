#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

enum class CxScriptTypeId : std::uint16_t
{
    Unknown = 0,

    Image,

    FindLine,
    FindCircle,
    FindEllipse,
    FindRect,
    FindObject,
    FindSegmentation,

    FastMatch,
    CxTextInspect,
    FastMatchDiagnostic,
    CircleRingGauge,

    Shape,
    ShapeBase,
    PointsShape,
    LineShape,

    SmartDouble,

    GridPatternClassTool,
    RegionPatternTool
};

std::string_view CxScriptTypeName(
    CxScriptTypeId typeId) noexcept;

std::optional<CxScriptTypeId>
ParseCanonicalCxScriptTypeName(
    std::string_view name) noexcept;

std::optional<std::string_view>
DeprecatedTypeNameSuggestion(
    std::string_view name) noexcept;

template<typename T>
struct CxScriptTypeTraits;

#define CXSCRIPT_TYPE_TRAITS(Type, Enum) \
template<> \
struct CxScriptTypeTraits<Type> \
{ \
    static constexpr CxScriptTypeId id = \
        CxScriptTypeId::Enum; \
    static constexpr std::string_view name = \
        #Type; \
};
