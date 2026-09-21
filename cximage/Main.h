
#ifndef MAIN_H
#define MAIN_H
 
#ifdef __cplusplus
extern "C" {
#endif 
    int glfw_occ_main();

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <string>

struct CxScriptHeadlessOptions;
struct CxScriptHeadlessResult;
struct CxShapeInteractionBatchResult;

bool ParseCxScriptHeadlessArgs(int argc, char** argv, CxScriptHeadlessOptions& options);

struct ShapeInteractionTestOptions
{
    bool enabled = false;
    bool parse_ok = true;
    std::string manifest_path;
    std::string suite_path;
    std::string image_manifest_path;
    std::string out_dir;
    std::string parse_reason;
};

bool ParseShapeInteractionTestArgs(int argc, char** argv, ShapeInteractionTestOptions& options);

bool RunShapeInteractionSmokeCli(
    const std::string& manifest_path,
    const std::string& suite_path,
    const std::string& image_manifest_path,
    const std::string& out_dir,
    CxShapeInteractionBatchResult& result);
#endif

#endif
