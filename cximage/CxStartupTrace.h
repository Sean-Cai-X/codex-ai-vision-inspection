#pragma once
// Async-safe enough for pre-main startup diagnostics; deliberately avoids iostream/loggers.
#if defined(__linux__)
#include <unistd.h>
inline void CxStartupTrace(const char* text) noexcept {
  if (!text) return;
  unsigned long n = 0;
  while (text[n] != '\0') ++n;
  (void)::write(STDERR_FILENO, text, n);
}
#else
inline void CxStartupTrace(const char*) noexcept {}
#endif

struct CxStartupTraceMark {
  explicit CxStartupTraceMark(const char* text) noexcept { CxStartupTrace(text); }
};
