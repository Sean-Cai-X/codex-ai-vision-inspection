#include "CxScriptStage25Template.h"
#include <fstream>
#include <regex>

std::string Stage25TemplateEngine::ReplaceAll(
    std::string text,
    const std::string& key,
    const std::string& value)
{
    const std::string placeholder = "{{" + key + "}}";
    size_t pos = 0;
    while ((pos = text.find(placeholder, pos)) != std::string::npos)
    {
        text.replace(pos, placeholder.length(), value);
        pos += value.length();
    }
    return text;
}

bool Stage25TemplateEngine::RenderFile(
    const std::filesystem::path& templatePath,
    const Stage25TemplateContext& context,
    const std::filesystem::path& outputPath,
    std::string& outReason)
{
    if (!std::filesystem::exists(templatePath))
    {
        outReason = "Template not found: " + templatePath.string();
        return false;
    }

    std::ifstream tplFile(templatePath);
    if (!tplFile.is_open())
    {
        outReason = "Cannot open template: " + templatePath.string();
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(tplFile)),
                        std::istreambuf_iterator<char>());

    for (const auto& kv : context.values)
    {
        content = ReplaceAll(content, kv.first, kv.second);
    }

    std::regex unprocessed("\\{\\{[^}]+\\}\\}");
    if (std::regex_search(content, unprocessed))
    {
        outReason = "Unprocessed template variables remain";
    }

    std::filesystem::create_directories(outputPath.parent_path());
    std::ofstream outFile(outputPath);
    if (!outFile.is_open())
    {
        outReason = "Cannot write output: " + outputPath.string();
        return false;
    }

    outFile << content;
    return true;
}