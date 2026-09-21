#ifndef CXPARSER_EXT_IMAGE_PROBE_DLL_H
#define CXPARSER_EXT_IMAGE_PROBE_DLL_H

#ifdef _WIN32
#define IMAGE_PROBE_API extern "C" __declspec(dllexport)
#else
#define IMAGE_PROBE_API extern "C"
#endif

IMAGE_PROBE_API void* CreateImageProbe();
IMAGE_PROBE_API void DestroyImageProbe(void* handle);
IMAGE_PROBE_API int ImageProbeLoad(void* handle, const char* path);
IMAGE_PROBE_API int ImageProbeDetect(void* handle, double threshold);
IMAGE_PROBE_API double ImageProbeScore(void* handle);

#endif
