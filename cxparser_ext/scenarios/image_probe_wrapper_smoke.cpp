#include <cmath>
#include <iostream>

#include "image_probe_wrapper.h"

namespace
{
bool NearlyEqual(double lhs, double rhs, double eps = 1e-9)
{
  return std::fabs(lhs - rhs) <= eps;
}

int RunWrapperSuccessCase()
{
  ImageProbeWrapper probe;
  if (!probe.IsReady())
  {
    std::cerr << "[FAIL] wrapper not ready: " << probe.GetLastError() << "\n";
    return 1;
  }

  probe.Load("sample.png");
  if (!probe.GetLastError().empty())
  {
    std::cerr << "[FAIL] Load failed: " << probe.GetLastError() << "\n";
    return 1;
  }

  probe.Detect(0.8);
  if (!probe.GetLastError().empty())
  {
    std::cerr << "[FAIL] Detect failed: " << probe.GetLastError() << "\n";
    return 1;
  }

  const double score = probe.Score();
  if (!probe.GetLastError().empty())
  {
    std::cerr << "[FAIL] Score failed: " << probe.GetLastError() << "\n";
    return 1;
  }

  if (!NearlyEqual(score, 8.0))
  {
    std::cerr << "[FAIL] unexpected score: " << score << "\n";
    return 1;
  }

  std::cout << "[PASS] wrapper success score=" << score << "\n";
  return 0;
}

int RunWrapperEmptyPathCase()
{
  ImageProbeWrapper probe;
  if (!probe.IsReady())
  {
    std::cerr << "[FAIL] wrapper not ready for empty-path case: " << probe.GetLastError() << "\n";
    return 1;
  }

  probe.Load("");
  if (probe.GetLastError().empty())
  {
    std::cerr << "[FAIL] empty path should be rejected\n";
    return 1;
  }

  std::cout << "[PASS] wrapper empty-path rejected: " << probe.GetLastError() << "\n";
  return 0;
}

int RunWrapperDetectWithoutLoadCase()
{
  ImageProbeWrapper probe;
  if (!probe.IsReady())
  {
    std::cerr << "[FAIL] wrapper not ready for detect-without-load case: " << probe.GetLastError() << "\n";
    return 1;
  }

  probe.Detect(0.8);
  if (probe.GetLastError().empty())
  {
    std::cerr << "[FAIL] detect without load should fail\n";
    return 1;
  }

  const double score = probe.Score();
  if (!probe.GetLastError().empty())
  {
    std::cerr << "[FAIL] Score should still be readable after detect failure: " << probe.GetLastError() << "\n";
    return 1;
  }

  if (!NearlyEqual(score, 0.0))
  {
    std::cerr << "[FAIL] score should remain zero after detect failure: " << score << "\n";
    return 1;
  }

  std::cout << "[PASS] wrapper detect-without-load rejected\n";
  return 0;
}
}

int main()
{
  if (RunWrapperSuccessCase() != 0)
    return 1;
  if (RunWrapperEmptyPathCase() != 0)
    return 1;
  if (RunWrapperDetectWithoutLoadCase() != 0)
    return 1;
  return 0;
}
