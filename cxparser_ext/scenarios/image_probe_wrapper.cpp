#include "image_probe_wrapper.h"

ImageProbeWrapper::ImageProbeWrapper()
  : ready_(true),
    loaded_(false),
    detected_(false),
    detect_threshold_(0.0)
{
}

bool ImageProbeWrapper::IsReady() const
{
  return ready_;
}

const std::string &ImageProbeWrapper::GetLastError() const
{
  return last_error_;
}

void ImageProbeWrapper::Load(const char *image_path)
{
  if (image_path == 0 || *image_path == '\0')
  {
    SetError("image path is empty");
    loaded_ = false;
    detected_ = false;
    image_path_.clear();
    return;
  }

  image_path_ = image_path;
  loaded_ = true;
  detected_ = false;
  detect_threshold_ = 0.0;
  ClearError();
}

void ImageProbeWrapper::Detect(double threshold)
{
  if (!loaded_)
  {
    SetError("image must be loaded before detect");
    detected_ = false;
    return;
  }

  detect_threshold_ = threshold;
  detected_ = true;
  ClearError();
}

double ImageProbeWrapper::Score()
{
  if (!loaded_)
  {
    SetError("image must be loaded before score");
    return 0.0;
  }

  if (!detected_)
  {
    SetError("detect must run before score");
    return 0.0;
  }

  ClearError();
  return detect_threshold_ * 10.0;
}

void ImageProbeWrapper::ClearError()
{
  last_error_.clear();
}

void ImageProbeWrapper::SetError(const std::string &message)
{
  last_error_ = message;
}
