#include "CxScriptImageManifestRuntime.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <opencv2/opencv.hpp>

namespace
{
    bool ParseDoubleRange(
        const std::string& text,
        double& out_min,
        double& out_max,
        std::string& reason)
    {
        const auto dot_pos = text.find("..");
        if (dot_pos == std::string::npos)
        {
            reason = "invalid range format, expected '..' separator";
            return false;
        }

        try
        {
            out_min = std::stod(text.substr(0, dot_pos));
            out_max = std::stod(text.substr(dot_pos + 2));
        }
        catch (...)
        {
            reason = "cannot parse range values";
            return false;
        }

        return true;
    }

    bool SkipWhitespace(const std::string& text, size_t& pos)
    {
        while (pos < text.size() && std::isspace(text[pos]))
            ++pos;
        return pos < text.size();
    }

    bool ExpectChar(const std::string& text, size_t& pos, char ch)
    {
        SkipWhitespace(text, pos);
        if (pos >= text.size() || text[pos] != ch)
            return false;
        ++pos;
        return true;
    }

    std::string ParseString(const std::string& text, size_t& pos)
    {
        SkipWhitespace(text, pos);
        if (!ExpectChar(text, pos, '"'))
            return "";

        std::string result;
        bool escaped = false;
        while (pos < text.size())
        {
            const char ch = text[pos++];
            if (escaped)
            {
                result += ch;
                escaped = false;
            }
            else if (ch == '\\')
            {
                escaped = true;
            }
            else if (ch == '"')
            {
                break;
            }
            else
            {
                result += ch;
            }
        }
        return result;
    }

    int ParseInt(const std::string& text, size_t& pos)
    {
        SkipWhitespace(text, pos);
        const auto start = pos;
        if (pos < text.size() && text[pos] == '-')
            ++pos;
        while (pos < text.size() && std::isdigit(text[pos]))
            ++pos;
        if (pos == start)
            return 0;
        try
        {
            return std::stoi(text.substr(start, pos - start));
        }
        catch (...)
        {
            return 0;
        }
    }

    bool ParseJsonDouble(
        const std::string& text,
        size_t& pos,
        double& out_value,
        std::string& out_reason)
    {
        SkipWhitespace(text, pos);
        const auto start = pos;

        if (pos >= text.size())
        {
            out_reason = "unexpected end of input";
            return false;
        }

        if (text[pos] == '-')
            ++pos;

        bool has_integer_part = false;
        while (pos < text.size() && std::isdigit(text[pos]))
        {
            ++pos;
            has_integer_part = true;
        }

        if (pos < text.size() && text[pos] == '.')
        {
            ++pos;
            bool has_fractional_part = false;
            while (pos < text.size() && std::isdigit(text[pos]))
            {
                ++pos;
                has_fractional_part = true;
            }
            if (!has_integer_part && !has_fractional_part)
            {
                out_reason = "missing numeric value";
                return false;
            }
        }

        if (!has_integer_part)
        {
            out_reason = "missing numeric value";
            return false;
        }

        if (pos < text.size())
        {
            if (text[pos] == 'e' || text[pos] == 'E')
            {
                ++pos;
                if (pos < text.size() && (text[pos] == '+' || text[pos] == '-'))
                    ++pos;
                bool has_exponent = false;
                while (pos < text.size() && std::isdigit(text[pos]))
                {
                    ++pos;
                    has_exponent = true;
                }
                if (!has_exponent)
                {
                    out_reason = "missing exponent value";
                    return false;
                }
            }
        }

        try
        {
            out_value = std::stod(text.substr(start, pos - start));
        }
        catch (...)
        {
            out_reason = "cannot parse numeric value";
            return false;
        }

        if (std::isnan(out_value) || std::isinf(out_value))
        {
            out_reason = "NaN or Infinity not allowed";
            return false;
        }

        return true;
    }

    size_t FindArrayStart(const std::string& text, size_t pos, const std::string& arrayName)
    {
        const std::string pattern = "\"" + arrayName + "\"";
        const auto keyPos = text.find(pattern, pos);
        if (keyPos == std::string::npos)
            return std::string::npos;

        const auto colon = text.find(":", keyPos + pattern.size());
        if (colon == std::string::npos)
            return std::string::npos;

        SkipWhitespace(text, pos = colon + 1);
        if (pos >= text.size() || text[pos] != '[')
            return std::string::npos;

        return pos;
    }

    size_t FindObjectStart(const std::string& text, size_t pos, const std::string& keyName)
    {
        const std::string pattern = "\"" + keyName + "\"";
        const auto keyPos = text.find(pattern, pos);
        if (keyPos == std::string::npos)
            return std::string::npos;

        const auto colon = text.find(":", keyPos + pattern.size());
        if (colon == std::string::npos)
            return std::string::npos;

        SkipWhitespace(text, pos = colon + 1);
        if (pos >= text.size() || text[pos] != '{')
            return std::string::npos;

        return pos;
    }

    size_t FindNextArrayElement(const std::string& text, size_t pos)
    {
        SkipWhitespace(text, pos);
        if (pos >= text.size())
            return std::string::npos;

        if (text[pos] == ']')
            return std::string::npos;

        if (text[pos] == ',')
            ++pos;

        SkipWhitespace(text, pos);
        if (pos >= text.size())
            return std::string::npos;

        return pos;
    }

    size_t FindObjectEnd(const std::string& text, size_t start)
    {
        int depth = 1;
        bool inString = false;
        bool escaped = false;

        for (size_t i = start + 1; i < text.size(); ++i)
        {
            const char ch = text[i];
            if (inString)
            {
                if (escaped)
                {
                    escaped = false;
                }
                else if (ch == '\\')
                {
                    escaped = true;
                }
                else if (ch == '"')
                {
                    inString = false;
                }
                continue;
            }

            if (ch == '"')
            {
                inString = true;
                continue;
            }

            if (ch == '{')
                ++depth;
            else if (ch == '}')
            {
                --depth;
                if (depth == 0)
                    return i;
            }
        }
        return std::string::npos;
    }

    size_t FindArrayEnd(const std::string& text, size_t start)
    {
        int depth = 1;
        bool inString = false;
        bool escaped = false;

        for (size_t i = start + 1; i < text.size(); ++i)
        {
            const char ch = text[i];
            if (inString)
            {
                if (escaped)
                {
                    escaped = false;
                }
                else if (ch == '\\')
                {
                    escaped = true;
                }
                else if (ch == '"')
                {
                    inString = false;
                }
                continue;
            }

            if (ch == '"')
            {
                inString = true;
                continue;
            }

            if (ch == '[')
                ++depth;
            else if (ch == ']')
            {
                --depth;
                if (depth == 0)
                    return i;
            }
        }
        return std::string::npos;
    }

    bool ParseManifestRectObject(
        const std::string& text,
        size_t pos,
        CxManifestRect& out_rect)
    {
        SkipWhitespace(text, pos);
        if (pos >= text.size() || text[pos] != '{')
            return false;

        const auto rect_end = FindObjectEnd(text, pos);
        if (rect_end == std::string::npos)
            return false;

        size_t rp = pos + 1;
        SkipWhitespace(text, rp);
        while (rp < rect_end)
        {
            const std::string rkey = ParseString(text, rp);
            if (rkey.empty())
                break;
            ExpectChar(text, rp, ':');

            double value = 0.0;
            std::string reason;
            if (!ParseJsonDouble(text, rp, value, reason))
                return false;

            if (rkey == "x")
                out_rect.x = value;
            else if (rkey == "y")
                out_rect.y = value;
            else if (rkey == "width")
                out_rect.width = value;
            else if (rkey == "height")
                out_rect.height = value;

            if (!ExpectChar(text, rp, ','))
                break;
        }
        return true;
    }

    bool ParseImageObject(
        const std::string& text,
        size_t pos,
        CxScriptImageManifestEntry& entry)
    {
        const auto end = FindObjectEnd(text, pos);
        if (end == std::string::npos)
            return false;

        SkipWhitespace(text, pos);
        if (pos < end && text[pos] == '{')
            ++pos;

        while (pos < end)
        {
            const std::string key = ParseString(text, pos);
            if (key.empty())
                break;

            ExpectChar(text, pos, ':');

            if (key == "image_id")
                entry.image_id = ParseString(text, pos);
            else if (key == "level")
                entry.level = ParseString(text, pos);
            else if (key == "path")
                entry.path = ParseString(text, pos);
            else if (key == "source")
                entry.source_path = ParseString(text, pos);
            else if (key == "width")
                entry.width = ParseInt(text, pos);
            else if (key == "height")
                entry.height = ParseInt(text, pos);
            else if (key == "raw_not_cropped")
            {
                SkipWhitespace(text, pos);
                entry.raw_not_cropped = (text[pos] == 't' || text[pos] == 'T');
                while (pos < end && text[pos] != ',' && text[pos] != '}')
                    ++pos;
            }
            else if (key == "raw_not_enhanced")
            {
                SkipWhitespace(text, pos);
                entry.raw_not_enhanced = (text[pos] == 't' || text[pos] == 'T');
                while (pos < end && text[pos] != ',' && text[pos] != '}')
                    ++pos;
            }
            else if (key == "raw_not_rotated")
            {
                SkipWhitespace(text, pos);
                entry.raw_not_rotated = (text[pos] == 't' || text[pos] == 'T');
                while (pos < end && text[pos] != ',' && text[pos] != '}')
                    ++pos;
            }
            else if (key == "tool_targets")
            {
                SkipWhitespace(text, pos);
                if (text[pos] == '[')
                {
                    ++pos;
                    while (true)
                    {
                        const auto elemPos = FindNextArrayElement(text, pos);
                        if (elemPos == std::string::npos)
                            break;

                        if (text[elemPos] == '{')
                        {
                            CxScriptImageTargetRoi target;
                            const auto targetEnd = FindObjectEnd(text, elemPos);
                            if (targetEnd != std::string::npos)
                            {
                                size_t tp = elemPos + 1;
                                SkipWhitespace(text, tp);
                                while (tp < targetEnd)
                                {
                                    const std::string tkey = ParseString(text, tp);
                                    if (tkey.empty())
                                        break;
                                    ExpectChar(text, tp, ':');

                                    if (tkey == "tool")
                                        target.tool = ParseString(text, tp);
                                    else if (tkey == "roi_name")
                                        target.target_id = ParseString(text, tp);

                                    // Advance past the separator following the
                                    // first target field before parsing the
                                    // remaining fields in the same object.
                                    ExpectChar(text, tp, ',');

                                    bool seen_x0 = false, seen_y0 = false, seen_x1 = false, seen_y1 = false;
                                    bool seen_cx = false, seen_cy = false, seen_px = false, seen_py = false;
                                    bool seen_major_radius = false, seen_minor_radius = false, seen_angle_deg = false;
                                    bool seen_width = false, seen_height = false;

                                    while (tp < targetEnd)
                                    {
                                        SkipWhitespace(text, tp);
                                        if (tp >= targetEnd || text[tp] == '}')
                                            break;

                                        const std::string tkey = ParseString(text, tp);
                                        if (tkey.empty())
                                            break;

                                        ExpectChar(text, tp, ':');

                                        if (tkey == "tool")
                                            target.tool = ParseString(text, tp);
                                        else if (tkey == "roi_name")
                                            target.target_id = ParseString(text, tp);
                                        else if (tkey == "x0")
                                        {
                                            seen_x0 = true;
                                            target.x0 = ParseInt(text, tp);
                                        }
                                        else if (tkey == "y0")
                                        {
                                            seen_y0 = true;
                                            target.y0 = ParseInt(text, tp);
                                        }
                                        else if (tkey == "x1")
                                        {
                                            seen_x1 = true;
                                            target.x1 = ParseInt(text, tp);
                                        }
                                        else if (tkey == "y1")
                                        {
                                            seen_y1 = true;
                                            target.y1 = ParseInt(text, tp);
                                        }
                                        else if (tkey == "cx")
                                        {
                                            seen_cx = true;
                                            target.cx = ParseInt(text, tp);
                                        }
                                        else if (tkey == "cy")
                                        {
                                            seen_cy = true;
                                            target.cy = ParseInt(text, tp);
                                        }
                                        else if (tkey == "px")
                                        {
                                            seen_px = true;
                                            target.px = ParseInt(text, tp);
                                        }
                                        else if (tkey == "py")
                                        {
                                            seen_py = true;
                                            target.py = ParseInt(text, tp);
                                        }
                                        else if (tkey == "major_radius")
                                        {
                                            seen_major_radius = true;
                                            std::string reason;
                                            double val = 0.0;
                                            if (ParseJsonDouble(text, tp, val, reason))
                                                target.ellipse_major_radius = val;
                                        }
                                        else if (tkey == "minor_radius")
                                        {
                                            seen_minor_radius = true;
                                            std::string reason;
                                            double val = 0.0;
                                            if (ParseJsonDouble(text, tp, val, reason))
                                                target.ellipse_minor_radius = val;
                                        }
                                        else if (tkey == "angle_deg")
                                        {
                                            seen_angle_deg = true;
                                            std::string reason;
                                            double val = 0.0;
                                            if (ParseJsonDouble(text, tp, val, reason))
                                            {
                                                target.ellipse_angle_deg = val;
                                                target.rect_angle_deg = val;
                                            }
                                        }
                                        else if (tkey == "width")
                                        {
                                            seen_width = true;
                                            std::string reason;
                                            double val = 0.0;
                                            if (ParseJsonDouble(text, tp, val, reason))
                                                target.rect_width = val;
                                        }
                                        else if (tkey == "height")
                                        {
                                            seen_height = true;
                                            std::string reason;
                                            double val = 0.0;
                                            if (ParseJsonDouble(text, tp, val, reason))
                                                target.rect_height = val;
                                        }
                                        else if (tkey == "wgap")
                                        {
                                            target.wgap = ParseInt(text, tp);
                                            target.has_wgap = true;
                                        }
                                        else if (tkey == "hgap")
                                        {
                                            target.hgap = ParseInt(text, tp);
                                            target.has_hgap = true;
                                        }
                                        else if (tkey == "gap")
                                        {
                                            target.gap = ParseInt(text, tp);
                                            target.has_gap = true;
                                        }
                                        else if (tkey == "linegap")
                                        {
                                            target.linegap = ParseInt(text, tp);
                                            target.has_linegap = true;
                                        }
                                        else if (tkey == "min_edge_run_width_px")
                                        {
                                            target.min_edge_run_width_px =
                                                std::max(1, std::min(20, ParseInt(text, tp)));
                                            target.has_min_edge_run_width_px = true;
                                        }
                                        else if (tkey == "tool_half_width")
                                        {
                                            target.tool_half_width = ParseInt(text, tp);
                                            target.has_tool_half_width = true;
                                        }
                                        else if (tkey == "threshold")
                                        {
                                            target.threshold = ParseInt(text, tp);
                                            target.has_threshold = true;
                                        }
                                        else if (tkey == "method")
                                        {
                                            target.method = ParseInt(text, tp);
                                            target.has_method = true;
                                        }
                                        else if (tkey == "expected_edge")
                                            target.expected_edge = ParseString(text, tp);
                                        else if (tkey == "edge_polarity_hint")
                                            target.edge_polarity_hint = ParseString(text, tp);
                                        else if (tkey == "comment")
                                            target.comment = ParseString(text, tp);
                                        else
                                        {
                                            SkipWhitespace(text, tp);
                                            if (tp < targetEnd && text[tp] == '"')
                                                ParseString(text, tp);
                                            else
                                            {
                                                while (tp < targetEnd && text[tp] != ',' && text[tp] != '}')
                                                    ++tp;
                                            }
                                        }

                                        if (!ExpectChar(text, tp, ','))
                                            break;
                                    }

                                    if (target.tool == "FindLine")
                                    {
                                        target.has_line = seen_x0 && seen_y0 && seen_x1 && seen_y1;
                                    }
                                    else if (target.tool == "FindSegmentation")
                                    {
                                        target.has_line = seen_x0 && seen_y0 && seen_x1 && seen_y1;
                                    }
                                    else if (target.tool == "FindCircle")
                                    {
                                        target.has_circle = seen_cx && seen_cy && seen_px && seen_py;
                                    }
                                    else if (target.tool == "FindEllipse")
                                    {
                                        target.has_ellipse = seen_cx && seen_cy && seen_major_radius && seen_minor_radius && seen_angle_deg;
                                    }
                                    else if (target.tool == "FindRect")
                                    {
                                        target.has_rect = seen_cx && seen_cy && seen_width && seen_height && seen_angle_deg;
                                    }

                                    if (!ExpectChar(text, tp, ','))
                                        break;
                                }
                                entry.targets.push_back(target);
                            }
                            pos = targetEnd + 1;
                        }
                        else
                        {
                            break;
                        }
                    }
                }
            }
            else
            {
                SkipWhitespace(text, pos);
                if (pos < end && text[pos] == '"')
                    ParseString(text, pos);
                else
                {
                    while (pos < end && text[pos] != ',' && text[pos] != '}')
                        ++pos;
                }
            }

            if (!ExpectChar(text, pos, ','))
                break;
        }

        return true;
    }

    bool ParseImageEntryForDimensions(
        const CxScriptImageManifestRuntime& manifest,
        const std::string& image_id,
        int& out_width,
        int& out_height)
    {
        for (const auto& img : manifest.images)
        {
            if (img.image_id == image_id)
            {
                out_width = img.width;
                out_height = img.height;
                return true;
            }
        }
        return false;
    }

    bool ParseMatchCaseObject(
        const std::string& text,
        size_t pos,
        CxScriptFastMatchCase& case_entry)
    {
        const auto end = FindObjectEnd(text, pos);
        if (end == std::string::npos)
            return false;

        SkipWhitespace(text, pos);
        if (pos < end && text[pos] == '{')
            ++pos;

        while (pos < end)
        {
            const std::string key = ParseString(text, pos);
            if (key.empty())
                break;

            ExpectChar(text, pos, ':');

            if (key == "case_id")
                case_entry.case_id = ParseString(text, pos);
            else if (key == "level")
                case_entry.level = ParseString(text, pos);
            else if (key == "tool")
                case_entry.tool = ParseString(text, pos);
            else if (key == "template_image_id")
                case_entry.template_image_id = ParseString(text, pos);
            else if (key == "test_image_id")
                case_entry.test_image_id = ParseString(text, pos);
            else if (key == "template_rect")
            {
                size_t rect_pos = pos;
                SkipWhitespace(text, rect_pos);
                const auto rect_end = FindObjectEnd(text, rect_pos);
                if (!ParseManifestRectObject(text, rect_pos, case_entry.template_rect) ||
                    rect_end == std::string::npos)
                    return false;
                pos = rect_end + 1;
            }
            else if (key == "search_rect")
            {
                case_entry.has_search_rect = true;
                size_t rect_pos = pos;
                SkipWhitespace(text, rect_pos);
                const auto rect_end = FindObjectEnd(text, rect_pos);
                if (!ParseManifestRectObject(text, rect_pos, case_entry.search_rect) ||
                    rect_end == std::string::npos)
                    return false;
                pos = rect_end + 1;
            }
            else if (key == "expected_rect")
            {
                size_t rect_pos = pos;
                SkipWhitespace(text, rect_pos);
                const auto rect_end = FindObjectEnd(text, rect_pos);
                if (!ParseManifestRectObject(text, rect_pos, case_entry.expected_rect) ||
                    rect_end == std::string::npos)
                    return false;
                pos = rect_end + 1;
            }
            else if (key == "rotation_range_deg")
            {
                const std::string range_str = ParseString(text, pos);
                std::string reason;
                ParseDoubleRange(range_str, case_entry.rotation_min_deg, case_entry.rotation_max_deg, reason);
            }
            else if (key == "scale_range")
            {
                const std::string range_str = ParseString(text, pos);
                std::string reason;
                ParseDoubleRange(range_str, case_entry.scale_min, case_entry.scale_max, reason);
            }
            else if (key == "candidate_budget")
                case_entry.candidate_budget = ParseInt(text, pos);
            else if (key == "expected_variation")
                case_entry.expected_variation = ParseString(text, pos);
            else if (key == "review_focus")
                case_entry.review_focus = ParseString(text, pos);
            else if (key == "comment")
                case_entry.comment = ParseString(text, pos);
            else
            {
                SkipWhitespace(text, pos);
                if (pos < end && text[pos] == '"')
                    ParseString(text, pos);
                else
                {
                    while (pos < end && text[pos] != ',' && text[pos] != '}')
                        ++pos;
                }
            }

            if (!ExpectChar(text, pos, ','))
                break;
        }

        return true;
    }
}

bool LoadStage25ImageManifestJson(
    const std::string& manifest_path,
    CxScriptImageManifestRuntime& out_manifest,
    std::string& out_reason)
{
    std::filesystem::path path(manifest_path);
    if (!std::filesystem::exists(path))
    {
        out_reason = "Manifest file not found: " + manifest_path;
        return false;
    }

    std::ifstream file(path);
    if (!file.is_open())
    {
        out_reason = "Cannot open manifest file: " + manifest_path;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string text = buffer.str();
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF)
    {
        text.erase(0, 3);
    }

    out_manifest = CxScriptImageManifestRuntime{};
    out_manifest.manifest_path = manifest_path;

    size_t pos = 0;
    SkipWhitespace(text, pos);
    if (!ExpectChar(text, pos, '{'))
    {
        out_reason = "Invalid JSON format: expected '{'";
        return false;
    }

    while (pos < text.size())
    {
        const std::string key = ParseString(text, pos);
        if (key.empty())
            break;

        ExpectChar(text, pos, ':');

        if (key == "schema_version")
            out_manifest.schema_version = ParseString(text, pos);
        else if (key == "purpose")
            out_manifest.purpose = ParseString(text, pos);
        else if (key == "selection_policy")
            out_manifest.selection_policy = ParseString(text, pos);
        else if (key == "images")
        {
            SkipWhitespace(text, pos);
            if (text[pos] == '[')
            {
                const auto array_end = FindArrayEnd(text, pos);
                ++pos;
                while (true)
                {
                    const auto elemPos = FindNextArrayElement(text, pos);
                    if (elemPos == std::string::npos)
                        break;

                    if (text[elemPos] == '{')
                    {
                        CxScriptImageManifestEntry entry;
                        if (ParseImageObject(text, elemPos, entry))
                        {
                            out_manifest.images.push_back(entry);
                            pos = FindObjectEnd(text, elemPos) + 1;

                            if (entry.level == "L0_basic")
                                out_manifest.l0_count++;
                            else if (entry.level.find("L1") == 0)
                                out_manifest.l1_count++;
                            else if (entry.level.find("L2") == 0)
                                out_manifest.l2_count++;
                            else if (entry.level.find("L3") == 0)
                                out_manifest.l3_count++;
                        }
                        else
                        {
                            break;
                        }
                    }
                    else
                    {
                        break;
                    }
                }
                if (array_end != std::string::npos)
                    pos = array_end + 1;
            }
        }
        else if (key == "match_cases")
        {
            SkipWhitespace(text, pos);
            if (text[pos] == '[')
            {
                const auto array_end = FindArrayEnd(text, pos);
                ++pos;
                while (true)
                {
                    const auto elemPos = FindNextArrayElement(text, pos);
                    if (elemPos == std::string::npos)
                        break;

                    if (text[elemPos] == '{')
                    {
                        CxScriptFastMatchCase case_entry;
                        if (ParseMatchCaseObject(text, elemPos, case_entry))
                        {
                            out_manifest.match_cases.push_back(case_entry);
                            pos = FindObjectEnd(text, elemPos) + 1;
                        }
                        else
                        {
                            break;
                        }
                    }
                    else
                    {
                        break;
                    }
                }
                if (array_end != std::string::npos)
                    pos = array_end + 1;
            }
        }
        else
        {
            SkipWhitespace(text, pos);
            if (pos < text.size() && text[pos] == '"')
                ParseString(text, pos);
            else if (pos < text.size() && text[pos] == '{')
            {
                const auto objEnd = FindObjectEnd(text, pos);
                if (objEnd != std::string::npos)
                    pos = objEnd + 1;
                else
                    break;
            }
            else if (pos < text.size() && text[pos] == '[')
            {
                int depth = 1;
                for (size_t i = pos + 1; i < text.size() && depth > 0; ++i)
                {
                    if (text[i] == '[') ++depth;
                    else if (text[i] == ']') --depth;
                    pos = i;
                }
                ++pos;
            }
            else
            {
                while (pos < text.size() && text[pos] != ',' && text[pos] != '}')
                    ++pos;
            }
        }

        if (!ExpectChar(text, pos, ','))
            break;
    }

    out_manifest.total_images = static_cast<int>(out_manifest.images.size());

    for (auto& mc : out_manifest.match_cases)
    {
        if (!mc.has_search_rect)
        {
            for (const auto& img : out_manifest.images)
            {
                if (img.image_id == mc.test_image_id)
                {
                    mc.search_rect_defaulted = true;
                    mc.search_rect_source = "runtime_default_full_image";
                    mc.search_rect.x = 0.0;
                    mc.search_rect.y = 0.0;
                    mc.search_rect.width = static_cast<double>(img.width);
                    mc.search_rect.height = static_cast<double>(img.height);
                    break;
                }
            }
        }
    }

    return true;
}

namespace
{
    void AddImageManifestIssue(
        CxScriptImageManifestValidationResult& result,
        const std::string& severity,
        const std::string& image_id,
        const std::string& target_id,
        const std::string& message)
    {
        CxScriptImageManifestValidationIssue issue;
        issue.severity = severity;
        issue.image_id = image_id;
        issue.target_id = target_id;
        issue.message = message;
        result.issues.push_back(issue);

        if (severity == "error")
            result.ok = false;
    }

    bool IsPointInsideImage(int x, int y, int w, int h)
    {
        return x >= 0 && y >= 0 && x < w && y < h;
    }

    void BuildRotatedRectCorners(
        double cx, double cy, double width, double height, double angle_deg,
        std::vector<std::pair<double, double>>& corners)
    {
        corners.clear();
        const double angle_rad = angle_deg * 3.14159265358979323846 / 180.0;
        const double cos_a = std::cos(angle_rad);
        const double sin_a = std::sin(angle_rad);
        const double hw = width / 2.0;
        const double hh = height / 2.0;

        const double dx[] = { -hw, hw, hw, -hw };
        const double dy[] = { -hh, -hh, hh, hh };

        for (int i = 0; i < 4; ++i)
        {
            double x = cx + dx[i] * cos_a - dy[i] * sin_a;
            double y = cy + dx[i] * sin_a + dy[i] * cos_a;
            corners.emplace_back(x, y);
        }
    }

    bool IsPolygonIntersectingImage(
        const std::vector<std::pair<double, double>>& points,
        int image_width, int image_height)
    {
        for (const auto& p : points)
        {
            if (p.first >= 0 && p.first < image_width &&
                p.second >= 0 && p.second < image_height)
            {
                return true;
            }
        }
        return false;
    }

    void ValidateTargetRoi(
        const CxScriptImageManifestEntry& image,
        const CxScriptImageTargetRoi& target,
        CxScriptImageManifestValidationResult& result)
    {
        if (target.tool == "FindLine")
        {
            if (!target.has_line)
            {
                AddImageManifestIssue(
                    result,
                    "error",
                    image.image_id,
                    target.target_id,
                    "Findline ROI missing required fields");
                return;
            }

            if (target.x0 == target.x1 && target.y0 == target.y1)
            {
                AddImageManifestIssue(
                    result,
                    "error",
                    image.image_id,
                    target.target_id,
                    "Findline ROI start/end points are identical");
            }

            if (!IsPointInsideImage(target.x0, target.y0, image.width, image.height) ||
                !IsPointInsideImage(target.x1, target.y1, image.width, image.height))
            {
                AddImageManifestIssue(
                    result,
                    "error",
                    image.image_id,
                    target.target_id,
                    "Findline ROI is outside image");
            }
        }

        if (target.tool == "FindSegmentation")
        {
            if (!target.has_line)
            {
                AddImageManifestIssue(
                    result,
                    "error",
                    image.image_id,
                    target.target_id,
                    "FindSegmentation ROI missing required rectangle fields");
                return;
            }

            if (target.x1 <= target.x0 || target.y1 <= target.y0)
            {
                AddImageManifestIssue(
                    result,
                    "error",
                    image.image_id,
                    target.target_id,
                    "FindSegmentation ROI must have positive width and height");
            }

            const bool intersects =
                target.x1 > 0 && target.y1 > 0 &&
                target.x0 < image.width && target.y0 < image.height;
            if (!intersects)
            {
                AddImageManifestIssue(
                    result,
                    "error",
                    image.image_id,
                    target.target_id,
                    "FindSegmentation ROI is completely outside image");
            }
        }

        if (target.tool == "FindCircle")
        {
            if (!target.has_circle)
            {
                AddImageManifestIssue(
                    result,
                    "error",
                    image.image_id,
                    target.target_id,
                    "FindCircle ROI missing required fields");
                return;
            }

            if (!IsPointInsideImage(target.cx, target.cy, image.width, image.height) ||
                !IsPointInsideImage(target.px, target.py, image.width, image.height))
            {
                AddImageManifestIssue(
                    result,
                    "error",
                    image.image_id,
                    target.target_id,
                    "FindCircle ROI is outside image");
            }

            const int dx = target.px - target.cx;
            const int dy = target.py - target.cy;
            if (dx * dx + dy * dy <= 4)
            {
                AddImageManifestIssue(
                    result,
                    "error",
                    image.image_id,
                    target.target_id,
                    "FindCircle radius is too small");
            }
        }

        if (target.tool == "FindEllipse")
        {
            if (!target.has_ellipse)
            {
                AddImageManifestIssue(
                    result,
                    "error",
                    image.image_id,
                    target.target_id,
                    "FindEllipse ROI missing required fields");
                return;
            }

            if (std::abs(target.cx) > 10000 || std::abs(target.cy) > 10000)
            {
                AddImageManifestIssue(
                    result,
                    "error",
                    image.image_id,
                    target.target_id,
                    "FindEllipse center coordinates out of reasonable range");
            }

            if (target.ellipse_major_radius <= 1)
            {
                AddImageManifestIssue(
                    result,
                    "error",
                    image.image_id,
                    target.target_id,
                    "FindEllipse major radius must be > 1");
            }

            if (target.ellipse_minor_radius <= 1)
            {
                AddImageManifestIssue(
                    result,
                    "error",
                    image.image_id,
                    target.target_id,
                    "FindEllipse minor radius must be > 1");
            }

            if (std::abs(target.ellipse_angle_deg) > 180)
            {
                AddImageManifestIssue(
                    result,
                    "error",
                    image.image_id,
                    target.target_id,
                    "FindEllipse angle out of valid range");
            }

            const double bounding_radius = std::max(target.ellipse_major_radius, target.ellipse_minor_radius);
            const double min_x = target.cx - bounding_radius;
            const double max_x = target.cx + bounding_radius;
            const double min_y = target.cy - bounding_radius;
            const double max_y = target.cy + bounding_radius;

            if (max_x < 0 || min_x >= image.width ||
                max_y < 0 || min_y >= image.height)
            {
                AddImageManifestIssue(
                    result,
                    "error",
                    image.image_id,
                    target.target_id,
                    "FindEllipse ROI is completely outside image");
            }
        }

        if (target.tool == "FindRect")
        {
            if (!target.has_rect)
            {
                AddImageManifestIssue(
                    result,
                    "error",
                    image.image_id,
                    target.target_id,
                    "FindRect ROI missing required fields");
                return;
            }

            if (target.rect_width <= 1)
            {
                AddImageManifestIssue(
                    result,
                    "error",
                    image.image_id,
                    target.target_id,
                    "FindRect width must be > 1");
            }

            if (target.rect_height <= 1)
            {
                AddImageManifestIssue(
                    result,
                    "error",
                    image.image_id,
                    target.target_id,
                    "FindRect height must be > 1");
            }

            if (std::abs(target.rect_angle_deg) > 180)
            {
                AddImageManifestIssue(
                    result,
                    "error",
                    image.image_id,
                    target.target_id,
                    "FindRect angle out of valid range");
            }

            std::vector<std::pair<double, double>> corners;
            BuildRotatedRectCorners(
                target.cx, target.cy,
                target.rect_width, target.rect_height,
                target.rect_angle_deg,
                corners);

            if (corners.size() != 4)
            {
                AddImageManifestIssue(
                    result,
                    "error",
                    image.image_id,
                    target.target_id,
                    "FindRect failed to build corners");
                return;
            }

            double min_x = corners[0].first, max_x = corners[0].first;
            double min_y = corners[0].second, max_y = corners[0].second;
            for (const auto& p : corners)
            {
                min_x = std::min(min_x, p.first);
                max_x = std::max(max_x, p.first);
                min_y = std::min(min_y, p.second);
                max_y = std::max(max_y, p.second);
            }

            if (max_x - min_x < 1 || max_y - min_y < 1)
            {
                AddImageManifestIssue(
                    result,
                    "error",
                    image.image_id,
                    target.target_id,
                    "FindRect has degenerate bounding box");
            }

            if (!IsPolygonIntersectingImage(corners, image.width, image.height))
            {
                AddImageManifestIssue(
                    result,
                    "error",
                    image.image_id,
                    target.target_id,
                    "FindRect ROI is completely outside image");
            }
        }
    }
}

CxScriptImageManifestValidationResult ValidateStage25ImageManifest(
    const CxScriptImageManifestRuntime& manifest)
{
    CxScriptImageManifestValidationResult result;

    std::set<std::string> image_ids;
    for (const auto& image : manifest.images)
    {
        if (image.image_id.empty())
        {
            AddImageManifestIssue(
                result,
                "error",
                "",
                "",
                "Image entry has empty image_id");
        }
        else if (image_ids.count(image.image_id) > 0)
        {
            AddImageManifestIssue(
                result,
                "error",
                image.image_id,
                "",
                "Duplicate image_id detected");
        }
        else
        {
            image_ids.insert(image.image_id);
        }

        if (!std::filesystem::exists(image.path))
        {
            AddImageManifestIssue(
                result,
                "error",
                image.image_id,
                "",
                "Image path does not exist: " + image.path);
            continue;
        }

        cv::Mat img = cv::imread(image.path);
        if (img.empty())
        {
            AddImageManifestIssue(
                result,
                "error",
                image.image_id,
                "",
                "Cannot read image: " + image.path);
            continue;
        }

        if (img.cols != image.width || img.rows != image.height)
        {
            AddImageManifestIssue(
                result,
                "error",
                image.image_id,
                "",
                "Image size mismatch: manifest (" + std::to_string(image.width) + "x" +
                    std::to_string(image.height) + "), actual (" +
                    std::to_string(img.cols) + "x" + std::to_string(img.rows) + ")");
        }

        std::set<std::string> target_ids;
        for (const auto& target : image.targets)
        {
            if (target.target_id.empty())
            {
                AddImageManifestIssue(
                    result,
                    "error",
                    image.image_id,
                    "",
                    "Target entry has empty target_id");
            }
            else if (target_ids.count(target.target_id) > 0)
            {
                AddImageManifestIssue(
                    result,
                    "error",
                    image.image_id,
                    target.target_id,
                    "Duplicate target_id within image");
            }
            else
            {
                target_ids.insert(target.target_id);
            }

            if (target.tool.empty())
            {
                AddImageManifestIssue(
                    result,
                    "error",
                    image.image_id,
                    target.target_id,
                    "Target has empty tool");
            }
            else if (target.tool != "FindLine" &&
                     target.tool != "FindCircle" &&
                     target.tool != "FindEllipse" &&
                     target.tool != "FindRect" &&
                     target.tool != "FindSegmentation")
            {
                AddImageManifestIssue(
                    result,
                    "error",
                    image.image_id,
                    target.target_id,
                    "Unknown tool: " + target.tool);
            }

            ValidateTargetRoi(image, target, result);
        }
    }

    std::set<std::string> match_case_ids;
    for (const auto& mc : manifest.match_cases)
    {
        if (mc.case_id.empty())
        {
            AddImageManifestIssue(
                result,
                "error",
                "",
                "",
                "Match case has empty case_id");
        }
        else if (match_case_ids.count(mc.case_id) > 0)
        {
            AddImageManifestIssue(
                result,
                "error",
                "",
                mc.case_id,
                "Duplicate match case_id detected");
        }
        else
        {
            match_case_ids.insert(mc.case_id);
        }

        if (mc.template_image_id.empty())
        {
            AddImageManifestIssue(
                result,
                "error",
                "",
                mc.case_id,
                "Match case missing template_image_id");
        }
        else if (!FindImageById(manifest, mc.template_image_id))
        {
            AddImageManifestIssue(
                result,
                "error",
                mc.template_image_id,
                mc.case_id,
                "Template image_id not found in manifest");
        }

        if (mc.test_image_id.empty())
        {
            AddImageManifestIssue(
                result,
                "error",
                "",
                mc.case_id,
                "Match case missing test_image_id");
        }
        else if (!FindImageById(manifest, mc.test_image_id))
        {
            AddImageManifestIssue(
                result,
                "error",
                mc.test_image_id,
                mc.case_id,
                "Test image_id not found in manifest");
        }

        if (mc.template_rect.width <= 0 || mc.template_rect.height <= 0)
        {
            AddImageManifestIssue(
                result,
                "error",
                "",
                mc.case_id,
                "Template rect has invalid dimensions");
        }

        if (!mc.has_search_rect)
        {
            AddImageManifestIssue(
                result,
                "error",
                "",
                mc.case_id,
                "Match case missing search_rect");
        }
        else if (mc.search_rect.width <= 0 || mc.search_rect.height <= 0)
        {
            AddImageManifestIssue(
                result,
                "error",
                "",
                mc.case_id,
                "Search rect has invalid dimensions");
        }

        if (mc.expected_rect.width <= 0 || mc.expected_rect.height <= 0)
        {
            AddImageManifestIssue(
                result,
                "error",
                "",
                mc.case_id,
                "Expected rect has invalid dimensions");
        }

        if (mc.rotation_min_deg > mc.rotation_max_deg)
        {
            AddImageManifestIssue(
                result,
                "error",
                "",
                mc.case_id,
                "rotation_min_deg > rotation_max_deg");
        }

        if (mc.scale_min > mc.scale_max)
        {
            AddImageManifestIssue(
                result,
                "error",
                "",
                mc.case_id,
                "scale_min > scale_max");
        }

        if (mc.candidate_budget <= 0)
        {
            AddImageManifestIssue(
                result,
                "error",
                "",
                mc.case_id,
                "candidate_budget must be > 0");
        }
    }

    return result;
}

const CxScriptImageManifestEntry* FindImageById(
    const CxScriptImageManifestRuntime& manifest,
    const std::string& image_id)
{
    for (const auto& image : manifest.images)
    {
        if (image.image_id == image_id)
            return &image;
    }
    return nullptr;
}

const CxScriptImageTargetRoi* FindTargetRoiByImageAndTargetId(
    const CxScriptImageManifestRuntime& manifest,
    const std::string& image_id,
    const std::string& target_id)
{
    const CxScriptImageManifestEntry* image = FindImageById(manifest, image_id);
    if (!image)
        return nullptr;

    for (const auto& target : image->targets)
    {
        if (target.target_id == target_id)
            return &target;
    }
    return nullptr;
}

const CxScriptFastMatchCase* FindMatchCaseById(
    const CxScriptImageManifestRuntime& manifest,
    const std::string& case_id)
{
    for (const auto& mc : manifest.match_cases)
    {
        if (mc.case_id == case_id)
            return &mc;
    }
    return nullptr;
}

bool ParseDoubleRange(
    const std::string& text,
    double& out_min,
    double& out_max,
    std::string& reason)
{
    const auto dot_pos = text.find("..");
    if (dot_pos == std::string::npos)
    {
        reason = "invalid range format, expected '..' separator";
        return false;
    }

    try
    {
        out_min = std::stod(text.substr(0, dot_pos));
        out_max = std::stod(text.substr(dot_pos + 2));
    }
    catch (...)
    {
        reason = "cannot parse range values";
        return false;
    }

    return true;
}

bool WriteManifestDryRunReport(
    const CxScriptImageManifestRuntime& manifest,
    const std::string& output_dir)
{
    std::filesystem::path out_path(output_dir);
    std::filesystem::create_directories(out_path);

    std::map<std::string, int> level_counts;
    std::map<std::string, int> tool_target_counts;

    for (const auto& image : manifest.images)
    {
        level_counts[image.level]++;
        for (const auto& target : image.targets)
        {
            tool_target_counts[target.tool]++;
        }
    }

    CxScriptImageManifestValidationResult validation = ValidateStage25ImageManifest(manifest);

    std::vector<std::string> unresolved_refs;
    std::vector<std::string> invalid_rois;

    for (const auto& issue : validation.issues)
    {
        if (issue.severity == "error")
        {
            if (issue.message.find("not found") != std::string::npos ||
                issue.message.find("path does not exist") != std::string::npos)
            {
                unresolved_refs.push_back(issue.image_id + "/" + issue.target_id + ": " + issue.message);
            }
            else if (issue.message.find("ROI") != std::string::npos ||
                     issue.message.find("radius") != std::string::npos ||
                     issue.message.find("width") != std::string::npos ||
                     issue.message.find("height") != std::string::npos)
            {
                invalid_rois.push_back(issue.image_id + "/" + issue.target_id + ": " + issue.message);
            }
        }
    }

    bool parse_ok = validation.issues.empty();

    std::filesystem::path resolved_path = out_path / "manifest_resolved_snapshot.json";
    std::ofstream resolved_file(resolved_path);
    if (!resolved_file.is_open())
        return false;

    resolved_file << "{\n";
    resolved_file << "  \"image_count\": " << manifest.total_images << ",\n";
    resolved_file << "  \"level_counts\": {\n";
    for (auto it = level_counts.begin(); it != level_counts.end(); ++it)
    {
        resolved_file << "    \"" << it->first << "\": " << it->second;
        if (std::next(it) != level_counts.end())
            resolved_file << ",";
        resolved_file << "\n";
    }
    resolved_file << "  },\n";
    resolved_file << "  \"tool_target_counts\": {\n";
    for (auto it = tool_target_counts.begin(); it != tool_target_counts.end(); ++it)
    {
        resolved_file << "    \"" << it->first << "\": " << it->second;
        if (std::next(it) != tool_target_counts.end())
            resolved_file << ",";
        resolved_file << "\n";
    }
    resolved_file << "  },\n";
    resolved_file << "  \"match_case_count\": " << manifest.match_cases.size() << ",\n";
    resolved_file << "  \"unresolved_image_refs\": [\n";
    for (size_t i = 0; i < unresolved_refs.size(); ++i)
    {
        resolved_file << "    \"" << unresolved_refs[i] << "\"";
        if (i < unresolved_refs.size() - 1)
            resolved_file << ",";
        resolved_file << "\n";
    }
    resolved_file << "  ],\n";
    resolved_file << "  \"invalid_rois\": [\n";
    for (size_t i = 0; i < invalid_rois.size(); ++i)
    {
        resolved_file << "    \"" << invalid_rois[i] << "\"";
        if (i < invalid_rois.size() - 1)
            resolved_file << ",";
        resolved_file << "\n";
    }
    resolved_file << "  ],\n";
    resolved_file << "  \"parse_ok\": " << (parse_ok ? "true" : "false") << "\n";
    resolved_file << "}\n";

    std::filesystem::path validation_path = out_path / "manifest_validation_report.json";
    std::ofstream validation_file(validation_path);
    if (!validation_file.is_open())
        return false;

    validation_file << "{\n";
    validation_file << "  \"total_issues\": " << validation.issues.size() << ",\n";
    validation_file << "  \"issues\": [\n";
    for (size_t i = 0; i < validation.issues.size(); ++i)
    {
        const auto& issue = validation.issues[i];
        validation_file << "    {\n";
        validation_file << "      \"severity\": \"" << issue.severity << "\",\n";
        validation_file << "      \"image_id\": \"" << issue.image_id << "\",\n";
        validation_file << "      \"target_id\": \"" << issue.target_id << "\",\n";
        validation_file << "      \"message\": \"" << issue.message << "\"\n";
        validation_file << "    }";
        if (i < validation.issues.size() - 1)
            validation_file << ",";
        validation_file << "\n";
    }
    validation_file << "  ],\n";
    validation_file << "  \"validation_ok\": " << (parse_ok ? "true" : "false") << "\n";
    validation_file << "}\n";

    return true;
}
