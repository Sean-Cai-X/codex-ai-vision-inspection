#ifndef CXIMAGE_METROLOGY_ANALYTICS_CXBOUNDARYRESPONSE_H
#define CXIMAGE_METROLOGY_ANALYTICS_CXBOUNDARYRESPONSE_H

#include <string>
#include <vector>

namespace cxvision::metrology_analytics {

enum class CxBoundaryBaselineMode {
  None = 0,
  Offset = 1,
  Linear = 2,
  RobustLinear = 3,
  RollingMean = 4,
};

enum class CxBoundaryDenoiseMode {
  None = 0,
  Box = 1,
  Gaussian = 2,
  Median = 3,
  SavitzkyGolay = 4,
  HaarSoftThreshold = 5,
};

enum class CxBoundaryResponseMode {
  Auto = 0,
  LevelCrossing = 1,
  Peak = 2,
  Valley = 3,
  RisingGradient = 4,
  FallingGradient = 5,
  CurvatureZeroCrossing = 6,
  PositiveStep = 7,
  NegativeStep = 8,
  PullUp = 9,
  PullDown = 10,
  RiseFallPair = 11,
  FallRisePair = 12,
  Haar = 13,
  Daubechies4 = 14,
  Daubechies20 = 15,
  DerivativeOfGaussian = 16,
  TemplateCorrelation = 17,
};

enum class CxBoundaryPolarity {
  Rising = 0,
  Falling = 1,
  Either = 2,
};

enum class CxBoundarySelectionMode {
  Strongest = 0,
  First = 1,
  Last = 2,
  Nth = 3,
  NearestGateCenter = 4,
  NearestReference = 5,
};

enum class CxBoundarySubpixelMode {
  None = 0,
  LinearCrossing = 1,
  ParabolicResponse = 2,
};

enum class CxBoundaryPairAnchorMode {
  Center = 0,
  FirstEdge = 1,
  SecondEdge = 2,
};

struct CxBoundaryResponseConfig {
  CxBoundaryBaselineMode baseline_mode = CxBoundaryBaselineMode::Offset;
  CxBoundaryDenoiseMode denoise_mode = CxBoundaryDenoiseMode::Gaussian;
  CxBoundaryResponseMode response_mode = CxBoundaryResponseMode::Auto;
  CxBoundaryPolarity polarity = CxBoundaryPolarity::Either;
  CxBoundarySelectionMode selection_mode =
      CxBoundarySelectionMode::Strongest;
  CxBoundarySubpixelMode subpixel_mode =
      CxBoundarySubpixelMode::ParabolicResponse;

  int smoothing_radius = 2;
  int baseline_window = 12;
  int wavelet_scale = 4;
  int trigger_threshold_permille = 120;
  int level_permille = 500;
  int hysteresis_permille = 50;
  int gate_start_permille = 0;
  int gate_end_permille = 1000;
  int nth_candidate = 1;
  int min_plateau_width = 3;
  int min_amplitude_permille = 80;
  int pair_min_width = 2;
  int pair_max_width = 80;
  CxBoundaryPairAnchorMode pair_anchor_mode =
      CxBoundaryPairAnchorMode::Center;
  int reference_position_permille = 500;
  std::vector<float> reference_profile;
};

struct CxBoundaryCandidate {
  int sample_index = -1;
  double position_samples = -1.0;
  double score = 0.0;
  int polarity = 0;
  int scale = 1;
  double width_samples = 0.0;
  bool accepted = false;
  std::string reject_reason;
};

struct CxBoundaryResponseResult {
  std::vector<float> raw;
  std::vector<float> conditioned;
  std::vector<float> response;
  std::vector<float> trigger_level;
  std::vector<CxBoundaryCandidate> candidates;
  int selected_candidate = -1;
  CxBoundaryResponseMode effective_response_mode =
      CxBoundaryResponseMode::Auto;
  double noise_sigma = 0.0;
  std::string status = "PENDING";
  std::string reason;
};

CxBoundaryResponseResult
EvaluateBoundaryResponse(const std::vector<float> &profile,
                         const CxBoundaryResponseConfig &config);

const char *CxBoundaryResponseModeName(CxBoundaryResponseMode mode);
const char *CxBoundarySelectionModeName(CxBoundarySelectionMode mode);

} // namespace cxvision::metrology_analytics

#endif
