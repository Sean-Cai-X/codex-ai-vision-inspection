#include "../image_probe_wrapper.h"

extern "C"
{
__declspec(dllexport) ImageProbeWrapper *CreateImageProbeWrapper()
{
  return new ImageProbeWrapper();
}

__declspec(dllexport) void DestroyImageProbeWrapper(ImageProbeWrapper *probe)
{
  delete probe;
}
}
