#ifndef CXPARSER_EXT_IMAGE_PROBE_WRAPPER_H
#define CXPARSER_EXT_IMAGE_PROBE_WRAPPER_H

#include <string>

class ImageProbeWrapper
{
public:
  ImageProbeWrapper();

  bool IsReady() const;
  const std::string &GetLastError() const;

  void Load(const char *image_path);
  void Detect(double threshold);
  double Score();

private:
  void ClearError();
  void SetError(const std::string &message);

  bool ready_;
  bool loaded_;
  bool detected_;
  std::string image_path_;
  double detect_threshold_;
  std::string last_error_;
};

#endif
