#include "metrology_analytics/CxBoundaryResponse.h"
#include "pch.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace cxvision::metrology_analytics {
namespace {

constexpr double kEpsilon = 1.0e-12;

int ClampInt(int value, int low, int high) {
  return std::max(low, std::min(high, value));
}

std::size_t ClampIndex(int value, std::size_t size) {
  if (size == 0)
    return 0;
  return static_cast<std::size_t>(ClampInt(value, 0, static_cast<int>(size - 1)));
}

float Median(std::vector<float> values) {
  if (values.empty())
    return 0.0f;
  const std::size_t middle = values.size() / 2;
  std::nth_element(values.begin(), values.begin() + middle, values.end());
  float result = values[middle];
  if ((values.size() & 1U) == 0U) {
    const auto lower = std::max_element(values.begin(), values.begin() + middle);
    result = 0.5f * (result + *lower);
  }
  return result;
}

std::vector<float> WindowValues(const std::vector<float> &values, int start,
                                int count) {
  std::vector<float> out;
  if (values.empty() || count <= 0)
    return out;
  const int first = ClampInt(start, 0, static_cast<int>(values.size() - 1));
  const int last = ClampInt(first + count, first + 1,
                            static_cast<int>(values.size()));
  out.reserve(static_cast<std::size_t>(last - first));
  for (int i = first; i < last; ++i)
    out.push_back(values[static_cast<std::size_t>(i)]);
  return out;
}

std::vector<float> BoxSmooth(const std::vector<float> &values, int radius) {
  if (radius <= 0 || values.size() < 3)
    return values;
  std::vector<float> out(values.size(), 0.0f);
  for (int i = 0; i < static_cast<int>(values.size()); ++i) {
    double sum = 0.0;
    int count = 0;
    for (int k = -radius; k <= radius; ++k) {
      sum += values[ClampIndex(i + k, values.size())];
      ++count;
    }
    out[static_cast<std::size_t>(i)] = static_cast<float>(sum / count);
  }
  return out;
}

std::vector<float> GaussianSmooth(const std::vector<float> &values,
                                  int radius) {
  if (radius <= 0 || values.size() < 3)
    return values;
  const double sigma = std::max(0.75, static_cast<double>(radius) * 0.6);
  std::vector<double> kernel(static_cast<std::size_t>(radius * 2 + 1), 0.0);
  double kernelSum = 0.0;
  for (int k = -radius; k <= radius; ++k) {
    const double weight = std::exp(-0.5 * k * k / (sigma * sigma));
    kernel[static_cast<std::size_t>(k + radius)] = weight;
    kernelSum += weight;
  }
  std::vector<float> out(values.size(), 0.0f);
  for (int i = 0; i < static_cast<int>(values.size()); ++i) {
    double sum = 0.0;
    for (int k = -radius; k <= radius; ++k) {
      sum += values[ClampIndex(i + k, values.size())] *
             kernel[static_cast<std::size_t>(k + radius)];
    }
    out[static_cast<std::size_t>(i)] =
        static_cast<float>(sum / std::max(kEpsilon, kernelSum));
  }
  return out;
}

std::vector<float> MedianSmooth(const std::vector<float> &values, int radius) {
  if (radius <= 0 || values.size() < 3)
    return values;
  std::vector<float> out(values.size(), 0.0f);
  std::vector<float> window;
  window.reserve(static_cast<std::size_t>(radius * 2 + 1));
  for (int i = 0; i < static_cast<int>(values.size()); ++i) {
    window.clear();
    for (int k = -radius; k <= radius; ++k)
      window.push_back(values[ClampIndex(i + k, values.size())]);
    out[static_cast<std::size_t>(i)] = Median(window);
  }
  return out;
}

std::vector<float> SavitzkyGolaySmooth(const std::vector<float> &values,
                                       int radius) {
  if (radius < 2 || values.size() < 5)
    return BoxSmooth(values, std::max(1, radius));
  static constexpr double weights[] = {-3.0, 12.0, 17.0, 12.0, -3.0};
  std::vector<float> out(values.size(), 0.0f);
  for (int i = 0; i < static_cast<int>(values.size()); ++i) {
    double sum = 0.0;
    for (int k = -2; k <= 2; ++k)
      sum += values[ClampIndex(i + k, values.size())] * weights[k + 2];
    out[static_cast<std::size_t>(i)] = static_cast<float>(sum / 35.0);
  }
  return out;
}

std::vector<float> HaarDenoise(const std::vector<float> &values,
                               int thresholdPermille) {
  if (values.size() < 2)
    return values;
  std::vector<float> out(values);
  float maximumDetail = 0.0f;
  for (std::size_t i = 0; i + 1 < values.size(); i += 2)
    maximumDetail = std::max(maximumDetail, std::abs(values[i] - values[i + 1]));
  const float threshold = maximumDetail *
                          ClampInt(thresholdPermille, 0, 1000) / 1000.0f;
  for (std::size_t i = 0; i + 1 < values.size(); i += 2) {
    const float average = 0.5f * (values[i] + values[i + 1]);
    const float detail = 0.5f * (values[i] - values[i + 1]);
    const float shrunk = std::copysign(
        std::max(0.0f, std::abs(detail) - threshold), detail);
    out[i] = average + shrunk;
    out[i + 1] = average - shrunk;
  }
  return out;
}

std::vector<float> ApplyDenoise(const std::vector<float> &values,
                                const CxBoundaryResponseConfig &config) {
  const int radius = ClampInt(config.smoothing_radius, 0, 32);
  switch (config.denoise_mode) {
  case CxBoundaryDenoiseMode::Box:
    return BoxSmooth(values, radius);
  case CxBoundaryDenoiseMode::Gaussian:
    return GaussianSmooth(values, radius);
  case CxBoundaryDenoiseMode::Median:
    return MedianSmooth(values, radius);
  case CxBoundaryDenoiseMode::SavitzkyGolay:
    return SavitzkyGolaySmooth(values, radius);
  case CxBoundaryDenoiseMode::HaarSoftThreshold:
    return HaarDenoise(values, config.trigger_threshold_permille);
  default:
    return values;
  }
}

std::vector<float> RemoveBaseline(const std::vector<float> &values,
                                  const CxBoundaryResponseConfig &config) {
  if (values.empty() || config.baseline_mode == CxBoundaryBaselineMode::None)
    return values;
  std::vector<float> out(values);
  const int window = ClampInt(config.baseline_window, 1,
                              std::max(1, static_cast<int>(values.size() / 2)));
  const std::vector<float> left = WindowValues(values, 0, window);
  const std::vector<float> right = WindowValues(
      values, static_cast<int>(values.size()) - window, window);
  const float leftLevel =
      config.baseline_mode == CxBoundaryBaselineMode::RobustLinear
          ? Median(left)
          : static_cast<float>(std::accumulate(left.begin(), left.end(), 0.0) /
                               std::max<std::size_t>(1, left.size()));
  const float rightLevel =
      config.baseline_mode == CxBoundaryBaselineMode::RobustLinear
          ? Median(right)
          : static_cast<float>(std::accumulate(right.begin(), right.end(), 0.0) /
                               std::max<std::size_t>(1, right.size()));

  if (config.baseline_mode == CxBoundaryBaselineMode::Offset) {
    for (float &value : out)
      value -= leftLevel;
    return out;
  }
  if (config.baseline_mode == CxBoundaryBaselineMode::RollingMean) {
    const std::vector<float> background = BoxSmooth(values, window);
    for (std::size_t i = 0; i < out.size(); ++i)
      out[i] -= background[i];
    return out;
  }

  const double denominator = static_cast<double>(std::max<std::size_t>(1, out.size() - 1));
  for (std::size_t i = 0; i < out.size(); ++i) {
    const double t = static_cast<double>(i) / denominator;
    out[i] -= static_cast<float>(leftLevel + (rightLevel - leftLevel) * t);
  }
  return out;
}

void Normalize(std::vector<float> &values) {
  if (values.empty())
    return;
  const auto minmax = std::minmax_element(values.begin(), values.end());
  const float span = *minmax.second - *minmax.first;
  if (span <= 1.0e-9f) {
    std::fill(values.begin(), values.end(), 0.0f);
    return;
  }
  for (float &value : values)
    value = (value - *minmax.first) / span;
}

std::vector<float> Gradient(const std::vector<float> &values, int scale) {
  std::vector<float> out(values.size(), 0.0f);
  const int s = ClampInt(scale, 1, 64);
  for (int i = 0; i < static_cast<int>(values.size()); ++i) {
    out[static_cast<std::size_t>(i)] =
        0.5f * (values[ClampIndex(i + s, values.size())] -
                values[ClampIndex(i - s, values.size())]);
  }
  return out;
}

std::vector<float> Curvature(const std::vector<float> &values, int scale) {
  std::vector<float> out(values.size(), 0.0f);
  const int s = ClampInt(scale, 1, 64);
  for (int i = 0; i < static_cast<int>(values.size()); ++i) {
    out[static_cast<std::size_t>(i)] =
        values[ClampIndex(i + s, values.size())] - 2.0f * values[i] +
        values[ClampIndex(i - s, values.size())];
  }
  return out;
}

std::vector<float> WaveletResponse(const std::vector<float> &values,
                                   const std::vector<double> &lowPass,
                                   int scale) {
  std::vector<double> highPass(lowPass.size(), 0.0);
  for (std::size_t i = 0; i < lowPass.size(); ++i) {
    const std::size_t reversed = lowPass.size() - 1 - i;
    highPass[i] = (i & 1U) == 0U ? lowPass[reversed] : -lowPass[reversed];
  }
  std::vector<float> out(values.size(), 0.0f);
  const int dilation = ClampInt(scale, 1, 32);
  const int center = static_cast<int>(highPass.size() / 2);
  for (int i = 0; i < static_cast<int>(values.size()); ++i) {
    double sum = 0.0;
    for (int k = 0; k < static_cast<int>(highPass.size()); ++k) {
      const int offset = (k - center) * dilation;
      sum += highPass[static_cast<std::size_t>(k)] *
             values[ClampIndex(i + offset, values.size())];
    }
    out[static_cast<std::size_t>(i)] = static_cast<float>(sum);
  }
  return out;
}

std::vector<float> DogResponse(const std::vector<float> &values, int scale) {
  const int radius = ClampInt(scale * 3, 2, 64);
  const double sigma = std::max(1.0, static_cast<double>(scale));
  std::vector<float> out(values.size(), 0.0f);
  for (int i = 0; i < static_cast<int>(values.size()); ++i) {
    double sum = 0.0;
    double norm = 0.0;
    for (int k = -radius; k <= radius; ++k) {
      const double weight = -static_cast<double>(k) *
                            std::exp(-0.5 * k * k / (sigma * sigma));
      sum += values[ClampIndex(i + k, values.size())] * weight;
      norm += std::abs(weight);
    }
    out[static_cast<std::size_t>(i)] =
        static_cast<float>(sum / std::max(kEpsilon, norm));
  }
  return out;
}

std::vector<float> TemplateResponse(const std::vector<float> &values,
                                    const std::vector<float> &reference) {
  std::vector<float> out(values.size(), 0.0f);
  if (reference.size() < 3 || reference.size() > values.size())
    return out;
  const int half = static_cast<int>(reference.size() / 2);
  const double referenceMean =
      std::accumulate(reference.begin(), reference.end(), 0.0) /
      static_cast<double>(reference.size());
  double referenceEnergy = 0.0;
  for (float value : reference) {
    const double centered = value - referenceMean;
    referenceEnergy += centered * centered;
  }
  for (int center = 0; center < static_cast<int>(values.size()); ++center) {
    double sampleMean = 0.0;
    for (int k = 0; k < static_cast<int>(reference.size()); ++k)
      sampleMean += values[ClampIndex(center + k - half, values.size())];
    sampleMean /= static_cast<double>(reference.size());
    double numerator = 0.0;
    double sampleEnergy = 0.0;
    for (int k = 0; k < static_cast<int>(reference.size()); ++k) {
      const double sample =
          values[ClampIndex(center + k - half, values.size())] - sampleMean;
      const double model = reference[static_cast<std::size_t>(k)] - referenceMean;
      numerator += sample * model;
      sampleEnergy += sample * sample;
    }
    out[static_cast<std::size_t>(center)] = static_cast<float>(
        numerator / std::sqrt(std::max(kEpsilon, sampleEnergy * referenceEnergy)));
  }
  return out;
}

double EstimateNoiseSigma(const std::vector<float> &values) {
  if (values.size() < 3)
    return 0.0;
  std::vector<float> differences;
  differences.reserve(values.size() - 1);
  for (std::size_t i = 1; i < values.size(); ++i)
    differences.push_back(std::abs(values[i] - values[i - 1]));
  return static_cast<double>(Median(differences)) / 0.67448975 / std::sqrt(2.0);
}

CxBoundaryResponseMode ChooseAutoMode(const std::vector<float> &values,
                                      const CxBoundaryResponseConfig &config,
                                      int scale) {
  if (values.size() < 3)
    return CxBoundaryResponseMode::Auto;
  const std::vector<float> gradient = Gradient(values, scale);
  const auto minmax = std::minmax_element(gradient.begin(), gradient.end());
  const float rising = *minmax.second;
  const float falling = -*minmax.first;
  const int window = std::max(1, static_cast<int>(values.size() / 8));
  const float left = Median(WindowValues(values, 0, window));
  const float right = Median(WindowValues(
      values, static_cast<int>(values.size()) - window, window));
  if (std::abs(right - left) >= 0.2f)
    return right > left ? CxBoundaryResponseMode::PositiveStep
                        : CxBoundaryResponseMode::NegativeStep;
  const double targetPermille =
      config.selection_mode == CxBoundarySelectionMode::NearestReference
          ? ClampInt(config.reference_position_permille, 0, 1000)
          : 0.5 * (ClampInt(config.gate_start_permille, 0, 1000) +
                   ClampInt(config.gate_end_permille, 0, 1000));
  const int targetIndex = static_cast<int>(ClampIndex(
      static_cast<int>(std::lround(
          targetPermille * static_cast<double>(values.size() - 1) / 1000.0)),
      values.size()));
  const int radius = std::max(scale * 4, static_cast<int>(values.size() / 12));
  const int first = std::max(1, targetIndex - radius);
  const int last =
      std::min(static_cast<int>(gradient.size()) - 2, targetIndex + radius);
  float localSignedStep = 0.0f;
  for (int i = first; i <= last; ++i) {
    const float value = gradient[static_cast<std::size_t>(i)];
    if (std::abs(value) > std::abs(localSignedStep))
      localSignedStep = value;
  }
  const double localNoise = EstimateNoiseSigma(gradient);
  if (std::abs(localSignedStep) >=
      std::max(0.10, localNoise * 6.0)) {
    return localSignedStep >= 0.0f ? CxBoundaryResponseMode::PositiveStep
                                   : CxBoundaryResponseMode::NegativeStep;
  }
  const auto profileMinmax = std::minmax_element(values.begin(), values.end());
  const float endpointHigh = std::max(left, right);
  const float endpointLow = std::min(left, right);
  if (*profileMinmax.second - endpointHigh > endpointLow - *profileMinmax.first)
    return CxBoundaryResponseMode::Peak;
  if (endpointLow - *profileMinmax.first > 0.15f)
    return CxBoundaryResponseMode::Valley;
  return rising >= falling ? CxBoundaryResponseMode::RisingGradient
                           : CxBoundaryResponseMode::FallingGradient;
}

double LocalPlateauAmplitude(const std::vector<float> &values, int index,
                             int width) {
  const int w = ClampInt(width, 1, 64);
  const std::vector<float> left = WindowValues(values, index - w, w);
  const std::vector<float> right = WindowValues(values, index + 1, w);
  if (left.empty() || right.empty())
    return 0.0;
  return static_cast<double>(Median(right) - Median(left));
}

double ParabolicPosition(const std::vector<float> &response, int index) {
  if (index <= 0 || index + 1 >= static_cast<int>(response.size()))
    return static_cast<double>(index);
  const double left = response[static_cast<std::size_t>(index - 1)];
  const double center = response[static_cast<std::size_t>(index)];
  const double right = response[static_cast<std::size_t>(index + 1)];
  const double denominator = left - 2.0 * center + right;
  if (std::abs(denominator) < kEpsilon)
    return static_cast<double>(index);
  return static_cast<double>(index) +
         std::max(-0.5, std::min(0.5, 0.5 * (left - right) / denominator));
}

void AddCandidate(std::vector<CxBoundaryCandidate> &out, int index,
                  double position, double score, int polarity, int scale,
                  double width = 0.0) {
  CxBoundaryCandidate candidate;
  candidate.sample_index = index;
  candidate.position_samples = position;
  candidate.score = score;
  candidate.polarity = polarity;
  candidate.scale = scale;
  candidate.width_samples = width;
  out.push_back(candidate);
}

void AddLocalMaximaCandidates(const std::vector<float> &response,
                              std::vector<CxBoundaryCandidate> &out,
                              int polarity, int scale,
                              CxBoundarySubpixelMode subpixelMode) {
  if (response.size() < 3)
    return;
  for (int i = 1; i + 1 < static_cast<int>(response.size()); ++i) {
    const float value = response[static_cast<std::size_t>(i)];
    if (value <= 0.0f || value < response[static_cast<std::size_t>(i - 1)] ||
        value < response[static_cast<std::size_t>(i + 1)])
      continue;
    const double position =
        subpixelMode == CxBoundarySubpixelMode::ParabolicResponse
            ? ParabolicPosition(response, i)
            : static_cast<double>(i);
    AddCandidate(out, i, position, value, polarity, scale);
  }
}

void AddLevelCandidates(const std::vector<float> &values,
                        const CxBoundaryResponseConfig &config,
                        std::vector<CxBoundaryCandidate> &out) {
  const double level = ClampInt(config.level_permille, 0, 1000) / 1000.0;
  for (int i = 1; i < static_cast<int>(values.size()); ++i) {
    const double previous = values[static_cast<std::size_t>(i - 1)];
    const double current = values[static_cast<std::size_t>(i)];
    const bool rising = previous < level && current >= level;
    const bool falling = previous > level && current <= level;
    if ((!rising && !falling) ||
        (config.polarity == CxBoundaryPolarity::Rising && !rising) ||
        (config.polarity == CxBoundaryPolarity::Falling && !falling))
      continue;
    double position = static_cast<double>(i);
    if (config.subpixel_mode == CxBoundarySubpixelMode::LinearCrossing ||
        config.subpixel_mode == CxBoundarySubpixelMode::ParabolicResponse) {
      const double delta = current - previous;
      if (std::abs(delta) > kEpsilon)
        position = static_cast<double>(i - 1) + (level - previous) / delta;
    }
    AddCandidate(out, i, position, std::abs(current - previous),
                 rising ? 1 : -1, 1);
  }
}

void AddCurvatureZeroCrossings(const std::vector<float> &gradient,
                               const std::vector<float> &curvature,
                               const CxBoundaryResponseConfig &config,
                               std::vector<CxBoundaryCandidate> &out) {
  for (int i = 1; i < static_cast<int>(curvature.size()); ++i) {
    const float a = curvature[static_cast<std::size_t>(i - 1)];
    const float b = curvature[static_cast<std::size_t>(i)];
    if ((a > 0.0f && b > 0.0f) || (a < 0.0f && b < 0.0f) || a == b)
      continue;
    const float slope = gradient[static_cast<std::size_t>(i)];
    if ((config.polarity == CxBoundaryPolarity::Rising && slope <= 0.0f) ||
        (config.polarity == CxBoundaryPolarity::Falling && slope >= 0.0f))
      continue;
    const double fraction =
        std::abs(a) /
        std::max(kEpsilon,
                 static_cast<double>(std::abs(a) + std::abs(b)));
    AddCandidate(out, i, static_cast<double>(i - 1) + fraction,
                 std::abs(slope), slope >= 0.0f ? 1 : -1,
                 ClampInt(config.wavelet_scale, 1, 64));
  }
}

void BuildPairCandidates(const std::vector<float> &gradient,
                         const CxBoundaryResponseConfig &config, bool riseFirst,
                         std::vector<CxBoundaryCandidate> &out) {
  std::vector<CxBoundaryCandidate> rising;
  std::vector<CxBoundaryCandidate> falling;
  std::vector<float> positive(gradient.size(), 0.0f);
  std::vector<float> negative(gradient.size(), 0.0f);
  for (std::size_t i = 0; i < gradient.size(); ++i) {
    positive[i] = std::max(0.0f, gradient[i]);
    negative[i] = std::max(0.0f, -gradient[i]);
  }
  AddLocalMaximaCandidates(positive, rising, 1, config.wavelet_scale,
                           config.subpixel_mode);
  AddLocalMaximaCandidates(negative, falling, -1, config.wavelet_scale,
                           config.subpixel_mode);
  const auto &first = riseFirst ? rising : falling;
  const auto &second = riseFirst ? falling : rising;
  for (const auto &a : first) {
    for (const auto &b : second) {
      const double width = b.position_samples - a.position_samples;
      if (width < config.pair_min_width || width > config.pair_max_width)
        continue;
      double position = 0.5 * (a.position_samples + b.position_samples);
      int sampleIndex = static_cast<int>(std::lround(position));
      int polarity = riseFirst ? 1 : -1;
      if (config.pair_anchor_mode == CxBoundaryPairAnchorMode::FirstEdge) {
        position = a.position_samples;
        sampleIndex = a.sample_index;
        polarity = riseFirst ? 1 : -1;
      } else if (config.pair_anchor_mode ==
                 CxBoundaryPairAnchorMode::SecondEdge) {
        position = b.position_samples;
        sampleIndex = b.sample_index;
        polarity = riseFirst ? -1 : 1;
      }
      AddCandidate(out, sampleIndex, position, std::min(a.score, b.score),
                   polarity, config.wavelet_scale, width);
      break;
    }
  }
}

void ValidateCandidates(const std::vector<float> &conditioned,
                        const CxBoundaryResponseConfig &config,
                        CxBoundaryResponseMode mode,
                        std::vector<CxBoundaryCandidate> &candidates) {
  const int last = std::max(0, static_cast<int>(conditioned.size()) - 1);
  const int gateStart = static_cast<int>(std::floor(
      last * ClampInt(config.gate_start_permille, 0, 999) / 1000.0));
  const int gateEnd = static_cast<int>(std::ceil(
      last * ClampInt(config.gate_end_permille, 1, 1000) / 1000.0));
  double maximumScore = 0.0;
  for (const auto &candidate : candidates)
    maximumScore = std::max(maximumScore, candidate.score);
  const double threshold = maximumScore *
                           ClampInt(config.trigger_threshold_permille, 0, 1000) /
                           1000.0;
  const bool plateauMode =
      mode == CxBoundaryResponseMode::PositiveStep ||
      mode == CxBoundaryResponseMode::NegativeStep ||
      mode == CxBoundaryResponseMode::PullUp ||
      mode == CxBoundaryResponseMode::PullDown;
  for (auto &candidate : candidates) {
    if (candidate.position_samples < gateStart ||
        candidate.position_samples > gateEnd) {
      candidate.reject_reason = "outside_position_gate";
      continue;
    }
    if (candidate.score + kEpsilon < threshold) {
      candidate.reject_reason = "below_trigger_threshold";
      continue;
    }
    if (plateauMode) {
      const double amplitude = LocalPlateauAmplitude(
          conditioned, candidate.sample_index, config.min_plateau_width);
      const int requiredAmplitude =
          mode == CxBoundaryResponseMode::PullUp ||
                  mode == CxBoundaryResponseMode::PullDown
              ? std::max(config.min_amplitude_permille,
                         config.hysteresis_permille)
              : config.min_amplitude_permille;
      if (std::abs(amplitude) * 1000.0 < requiredAmplitude) {
        candidate.reject_reason = "below_min_plateau_amplitude";
        continue;
      }
      if ((mode == CxBoundaryResponseMode::PositiveStep ||
           mode == CxBoundaryResponseMode::PullUp) &&
          amplitude <= 0.0) {
        candidate.reject_reason = "wrong_step_polarity";
        continue;
      }
      if ((mode == CxBoundaryResponseMode::NegativeStep ||
           mode == CxBoundaryResponseMode::PullDown) &&
          amplitude >= 0.0) {
        candidate.reject_reason = "wrong_step_polarity";
        continue;
      }
    }
    candidate.accepted = true;
  }
}

int SelectCandidate(const std::vector<CxBoundaryCandidate> &candidates,
                    const CxBoundaryResponseConfig &config,
                    std::size_t sampleCount) {
  std::vector<int> accepted;
  for (int i = 0; i < static_cast<int>(candidates.size()); ++i) {
    if (candidates[static_cast<std::size_t>(i)].accepted)
      accepted.push_back(i);
  }
  if (accepted.empty())
    return -1;
  if (config.selection_mode == CxBoundarySelectionMode::First)
    return *std::min_element(accepted.begin(), accepted.end(), [&](int a, int b) {
      return candidates[a].position_samples < candidates[b].position_samples;
    });
  if (config.selection_mode == CxBoundarySelectionMode::Last)
    return *std::max_element(accepted.begin(), accepted.end(), [&](int a, int b) {
      return candidates[a].position_samples < candidates[b].position_samples;
    });
  if (config.selection_mode == CxBoundarySelectionMode::Nth) {
    std::sort(accepted.begin(), accepted.end(), [&](int a, int b) {
      return candidates[a].position_samples < candidates[b].position_samples;
    });
    const int ordinal = ClampInt(config.nth_candidate, 1,
                                 static_cast<int>(accepted.size()));
    return accepted[static_cast<std::size_t>(ordinal - 1)];
  }
  if (config.selection_mode == CxBoundarySelectionMode::NearestGateCenter ||
      config.selection_mode == CxBoundarySelectionMode::NearestReference) {
    const double targetPermille =
        config.selection_mode == CxBoundarySelectionMode::NearestReference
            ? ClampInt(config.reference_position_permille, 0, 1000)
            : 0.5 * (ClampInt(config.gate_start_permille, 0, 1000) +
                     ClampInt(config.gate_end_permille, 0, 1000));
    const double target = targetPermille *
                          static_cast<double>(std::max<std::size_t>(1, sampleCount - 1)) /
                          1000.0;
    return *std::min_element(accepted.begin(), accepted.end(), [&](int a, int b) {
      return std::abs(candidates[a].position_samples - target) <
             std::abs(candidates[b].position_samples - target);
    });
  }
  return *std::max_element(accepted.begin(), accepted.end(), [&](int a, int b) {
    return candidates[a].score < candidates[b].score;
  });
}

} // namespace

CxBoundaryResponseResult
EvaluateBoundaryResponse(const std::vector<float> &profile,
                         const CxBoundaryResponseConfig &config) {
  CxBoundaryResponseResult result;
  result.raw = profile;
  if (profile.size() < 3) {
    result.status = "INVALID_INPUT";
    result.reason = "profile_requires_at_least_three_samples";
    return result;
  }
  for (float value : profile) {
    if (!std::isfinite(value)) {
      result.status = "INVALID_INPUT";
      result.reason = "profile_contains_non_finite_sample";
      return result;
    }
  }

  result.conditioned = ApplyDenoise(profile, config);
  result.conditioned = RemoveBaseline(result.conditioned, config);
  Normalize(result.conditioned);
  result.noise_sigma = EstimateNoiseSigma(result.conditioned);

  const int scale = ClampInt(config.wavelet_scale, 1, 64);
  result.effective_response_mode = config.response_mode;
  if (result.effective_response_mode == CxBoundaryResponseMode::Auto)
    result.effective_response_mode = ChooseAutoMode(result.conditioned, config,
                                                    scale);

  const std::vector<float> gradient = Gradient(result.conditioned, scale);
  const std::vector<float> curvature = Curvature(result.conditioned, scale);
  result.response.assign(profile.size(), 0.0f);

  switch (result.effective_response_mode) {
  case CxBoundaryResponseMode::LevelCrossing:
    for (std::size_t i = 0; i < result.response.size(); ++i)
      result.response[i] = 1.0f - std::abs(
          result.conditioned[i] - ClampInt(config.level_permille, 0, 1000) / 1000.0f);
    AddLevelCandidates(result.conditioned, config, result.candidates);
    break;
  case CxBoundaryResponseMode::Peak:
    result.response = result.conditioned;
    AddLocalMaximaCandidates(result.response, result.candidates, 1, scale,
                             config.subpixel_mode);
    break;
  case CxBoundaryResponseMode::Valley:
    for (std::size_t i = 0; i < result.response.size(); ++i)
      result.response[i] = 1.0f - result.conditioned[i];
    AddLocalMaximaCandidates(result.response, result.candidates, -1, scale,
                             config.subpixel_mode);
    break;
  case CxBoundaryResponseMode::CurvatureZeroCrossing:
    for (std::size_t i = 0; i < result.response.size(); ++i)
      result.response[i] = std::abs(gradient[i]);
    AddCurvatureZeroCrossings(gradient, curvature, config, result.candidates);
    break;
  case CxBoundaryResponseMode::RiseFallPair:
  case CxBoundaryResponseMode::FallRisePair:
    for (std::size_t i = 0; i < result.response.size(); ++i)
      result.response[i] = std::abs(gradient[i]);
    BuildPairCandidates(gradient, config,
                        result.effective_response_mode ==
                            CxBoundaryResponseMode::RiseFallPair,
                        result.candidates);
    break;
  case CxBoundaryResponseMode::Haar:
    result.response = WaveletResponse(result.conditioned,
                                      {0.7071067811865476,
                                       0.7071067811865476},
                                      scale);
    break;
  case CxBoundaryResponseMode::Daubechies4:
    result.response = WaveletResponse(
        result.conditioned,
        {0.4829629131445341, 0.8365163037378079, 0.2241438680420134,
         -0.1294095225512604},
        scale);
    break;
  case CxBoundaryResponseMode::Daubechies20:
    result.response = WaveletResponse(
        result.conditioned,
        {0.0266700579009508, 0.1881768000776213, 0.5272011889317256,
         0.6884590394536035, 0.2811723436604265, -0.2498464243264887,
         -0.1959462743773771, 0.1273693403357427, 0.0930573646035724,
         -0.0713941471663501, -0.0294575368218758, 0.0332126740589332,
         0.0036065535669562, -0.0107331754833306, 0.0013953517470529,
         0.0019924052951851, -0.0006858566949597, -0.0001164668551293,
         0.0000935886703201, -0.0000132642028945},
        scale);
    break;
  case CxBoundaryResponseMode::DerivativeOfGaussian:
    result.response = DogResponse(result.conditioned, scale);
    break;
  case CxBoundaryResponseMode::TemplateCorrelation:
    if (config.reference_profile.size() < 3) {
      result.status = "REFERENCE_REQUIRED";
      result.reason = "template_correlation_requires_reference_profile";
      return result;
    }
    result.response = TemplateResponse(result.conditioned,
                                       config.reference_profile);
    AddLocalMaximaCandidates(result.response, result.candidates, 0, scale,
                             config.subpixel_mode);
    break;
  default: {
    const bool falling =
        result.effective_response_mode == CxBoundaryResponseMode::FallingGradient ||
        result.effective_response_mode == CxBoundaryResponseMode::NegativeStep ||
        result.effective_response_mode == CxBoundaryResponseMode::PullDown;
    for (std::size_t i = 0; i < result.response.size(); ++i)
      result.response[i] = falling ? -gradient[i] : gradient[i];
    AddLocalMaximaCandidates(result.response, result.candidates,
                             falling ? -1 : 1, scale, config.subpixel_mode);
    break;
  }
  }

  if ((result.effective_response_mode == CxBoundaryResponseMode::Haar ||
       result.effective_response_mode == CxBoundaryResponseMode::Daubechies4 ||
       result.effective_response_mode == CxBoundaryResponseMode::Daubechies20 ||
       result.effective_response_mode ==
           CxBoundaryResponseMode::DerivativeOfGaussian) &&
      result.candidates.empty()) {
    std::vector<float> selectedResponse(result.response.size(), 0.0f);
    for (std::size_t i = 0; i < result.response.size(); ++i) {
      if (config.polarity == CxBoundaryPolarity::Rising)
        selectedResponse[i] = std::max(0.0f, result.response[i]);
      else if (config.polarity == CxBoundaryPolarity::Falling)
        selectedResponse[i] = std::max(0.0f, -result.response[i]);
      else
        selectedResponse[i] = std::abs(result.response[i]);
    }
    result.response = selectedResponse;
    AddLocalMaximaCandidates(result.response, result.candidates,
                             config.polarity == CxBoundaryPolarity::Falling ? -1
                                                                           : 1,
                             scale, config.subpixel_mode);
  }

  float responseMaximum = 0.0f;
  for (float value : result.response)
    responseMaximum = std::max(responseMaximum, value);
  const float trigger = responseMaximum *
                        ClampInt(config.trigger_threshold_permille, 0, 1000) /
                        1000.0f;
  result.trigger_level.assign(result.response.size(), trigger);

  ValidateCandidates(result.conditioned, config,
                     result.effective_response_mode, result.candidates);
  result.selected_candidate =
      SelectCandidate(result.candidates, config, profile.size());
  if (result.selected_candidate < 0) {
    result.status = "NO_TRIGGER";
    result.reason = result.candidates.empty()
                        ? "response_produced_no_candidates"
                        : "all_candidates_rejected";
    return result;
  }
  result.status = "PREVIEW_READY";
  result.reason = "boundary_response_preview_requires_human_review";
  return result;
}

const char *CxBoundaryResponseModeName(CxBoundaryResponseMode mode) {
  switch (mode) {
  case CxBoundaryResponseMode::Auto:
    return "Auto";
  case CxBoundaryResponseMode::LevelCrossing:
    return "Level crossing";
  case CxBoundaryResponseMode::Peak:
    return "Peak";
  case CxBoundaryResponseMode::Valley:
    return "Valley";
  case CxBoundaryResponseMode::RisingGradient:
    return "Rising gradient";
  case CxBoundaryResponseMode::FallingGradient:
    return "Falling gradient";
  case CxBoundaryResponseMode::CurvatureZeroCrossing:
    return "Curvature zero crossing";
  case CxBoundaryResponseMode::PositiveStep:
    return "Positive step";
  case CxBoundaryResponseMode::NegativeStep:
    return "Negative step";
  case CxBoundaryResponseMode::PullUp:
    return "Pull up";
  case CxBoundaryResponseMode::PullDown:
    return "Pull down";
  case CxBoundaryResponseMode::RiseFallPair:
    return "Rise then fall";
  case CxBoundaryResponseMode::FallRisePair:
    return "Fall then rise";
  case CxBoundaryResponseMode::Haar:
    return "Haar wavelet";
  case CxBoundaryResponseMode::Daubechies4:
    return "Daubechies 4";
  case CxBoundaryResponseMode::Daubechies20:
    return "Daubechies 20";
  case CxBoundaryResponseMode::DerivativeOfGaussian:
    return "Derivative of Gaussian";
  case CxBoundaryResponseMode::TemplateCorrelation:
    return "Template correlation";
  }
  return "Unknown";
}

const char *CxBoundarySelectionModeName(CxBoundarySelectionMode mode) {
  switch (mode) {
  case CxBoundarySelectionMode::Strongest:
    return "Strongest";
  case CxBoundarySelectionMode::First:
    return "First";
  case CxBoundarySelectionMode::Last:
    return "Last";
  case CxBoundarySelectionMode::Nth:
    return "Nth";
  case CxBoundarySelectionMode::NearestGateCenter:
    return "Nearest gate center";
  case CxBoundarySelectionMode::NearestReference:
    return "Nearest reference";
  }
  return "Unknown";
}

} // namespace cxvision::metrology_analytics
