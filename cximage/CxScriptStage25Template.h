#ifndef CXIMAGE_CXSCRIPT_STAGE25_TEMPLATE_H
#define CXIMAGE_CXSCRIPT_STAGE25_TEMPLATE_H

#include <string>
#include <map>
#include <filesystem>

struct Stage25TemplateContext
{
    std::map<std::string, std::string> values;
};

class Stage25TemplateEngine
{
public:
    bool RenderFile(
        const std::filesystem::path& templatePath,
        const Stage25TemplateContext& context,
        const std::filesystem::path& outputPath,
        std::string& outReason);

private:
    static std::string ReplaceAll(
        std::string text,
        const std::string& key,
        const std::string& value);
};

#endif