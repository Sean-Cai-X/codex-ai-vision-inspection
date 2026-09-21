#include "pch.h"
#include "CxManifestProjectionRequestResolver.h"

#include <filesystem>

namespace
{
    std::string ResolveManifestImagePath(
        const CxScriptImageManifestRuntime& manifest,
        const std::string& image_path)
    {
        if (image_path.empty())
            return {};

        std::filesystem::path path(image_path);
        if (path.is_absolute())
            return path.string();

        if (manifest.image_root.empty())
            return path.string();

        return (std::filesystem::path(manifest.image_root) / path).string();
    }
}

bool ResolveManifestProjectionRequest(
    const CxScriptImageManifestRuntime& manifest,
    const CxShapeTestCase& test_case,
    CxRuntimeProjectionRequest& out_request,
    std::string& out_reason)
{
    out_request.case_id = test_case.case_id;
    out_request.tool_id = test_case.tool_id;
    out_request.owner_type = test_case.tool_id;

    if (test_case.image_manifest_path.empty())
    {
        out_reason = "manifest path not set";
        return false;
    }

    if (!test_case.manifest_target_id.empty())
    {
        const CxScriptImageManifestEntry* image_entry = nullptr;
        for (const auto& entry : manifest.images)
        {
            if (entry.image_id == test_case.manifest_image_id)
            {
                image_entry = &entry;
                break;
            }
        }

        if (!image_entry)
        {
            out_reason = "image_id not found in manifest: " + test_case.manifest_image_id;
            return false;
        }

        out_request.image_path = ResolveManifestImagePath(manifest, image_entry->path);

        bool target_found = false;
        for (const auto& target : image_entry->targets)
        {
            if (target.target_id == test_case.manifest_target_id)
            {
                out_request.threshold = target.threshold;
                out_request.method = target.method;
                out_request.wgap = target.wgap;
                out_request.hgap = target.hgap;
                out_request.gap = target.gap;
                out_request.linegap = target.linegap;
                out_request.tool_half_width = target.tool_half_width;

                if (target.has_line)
                {
                    out_request.roi_x0 = target.x0;
                    out_request.roi_y0 = target.y0;
                    out_request.roi_x1 = target.x1;
                    out_request.roi_y1 = target.y1;
                }
                else if (target.has_circle)
                {
                    out_request.circle_cx = target.cx;
                    out_request.circle_cy = target.cy;
                    out_request.circle_px = target.px;
                    out_request.circle_py = target.py;
                }
                else if (target.has_ellipse)
                {
                    out_request.has_ellipse_roi = true;
                    out_request.ellipse_cx = target.cx;
                    out_request.ellipse_cy = target.cy;
                    out_request.ellipse_rx = target.ellipse_major_radius;
                    out_request.ellipse_ry = target.ellipse_minor_radius;
                    out_request.ellipse_angle_deg = target.ellipse_angle_deg;
                }
                else if (target.has_rect)
                {
                    out_request.has_rotated_rect_roi = true;
                    out_request.rect_cx = target.cx;
                    out_request.rect_cy = target.cy;
                    out_request.rect_width = target.rect_width;
                    out_request.rect_height = target.rect_height;
                    out_request.rect_angle_deg = target.rect_angle_deg;
                }
                target_found = true;
                break;
            }
        }
        if (!target_found)
        {
            out_reason = "target_id not found in image: " + test_case.manifest_target_id;
            return false;
        }
    }

    if (!test_case.manifest_match_case_id.empty())
    {
        bool match_case_found = false;
        for (const auto& match_case : manifest.match_cases)
        {
            if (match_case.case_id == test_case.manifest_match_case_id)
            {
                const CxScriptImageManifestEntry* template_image = nullptr;
                for (const auto& entry : manifest.images)
                {
                    if (entry.image_id == match_case.template_image_id)
                    {
                        template_image = &entry;
                        break;
                    }
                }
                if (!template_image)
                {
                    out_reason = "template_image_id not found: " + match_case.template_image_id;
                    return false;
                }

                const CxScriptImageManifestEntry* test_image = nullptr;
                for (const auto& entry : manifest.images)
                {
                    if (entry.image_id == match_case.test_image_id)
                    {
                        test_image = &entry;
                        break;
                    }
                }
                if (!test_image)
                {
                    out_reason = "test_image_id not found: " + match_case.test_image_id;
                    return false;
                }

                out_request.template_image_path = ResolveManifestImagePath(manifest, template_image->path);
                out_request.test_image_path = ResolveManifestImagePath(manifest, test_image->path);
                out_request.image_path = out_request.test_image_path;

                out_request.has_learn_roi = true;
                out_request.learn_roi.x = match_case.template_rect.x;
                out_request.learn_roi.y = match_case.template_rect.y;
                out_request.learn_roi.width = match_case.template_rect.width;
                out_request.learn_roi.height = match_case.template_rect.height;

                if (match_case.has_search_rect)
                {
                    out_request.has_search_roi = true;
                    out_request.search_roi.x = match_case.search_rect.x;
                    out_request.search_roi.y = match_case.search_rect.y;
                    out_request.search_roi.width = match_case.search_rect.width;
                    out_request.search_roi.height = match_case.search_rect.height;
                }

                if (match_case.expected_rect.width > 0 && match_case.expected_rect.height > 0)
                {
                    out_request.has_expected_rect = true;
                    out_request.expected_rect = match_case.expected_rect;
                }

                match_case_found = true;
                break;
            }
        }
        if (!match_case_found)
        {
            out_reason = "match_case_id not found: " + test_case.manifest_match_case_id;
            return false;
        }
    }

    return true;
}
