#ifndef CXPARSER_EXT_PARSER_IMAGE_ANALYSIS_NODE_H
#define CXPARSER_EXT_PARSER_IMAGE_ANALYSIS_NODE_H

#include "../meta/parser_image_analysis_protocol.h"

namespace cxparser_ext
{
class ParserImageAnalysisNode
{
public:
  bool Execute(const ImageAnalysisRequest &request, ImageAnalysisResult &result) const;

private:
  bool HasOperation(const ImageAnalysisRequest &request, ImageAnalysisOperation operation) const;
  bool ValidateRequest(const ImageAnalysisRequest &request, ImageAnalysisResult &result) const;
  void AppendTrace(ImageAnalysisResult &result,
                   const std::string &stage,
                   const std::string &status,
                   const std::string &message) const;
  void AppendLog(ImageAnalysisResult &result,
                 const std::string &level,
                 const std::string &code,
                 const std::string &message) const;
};
}

#endif
