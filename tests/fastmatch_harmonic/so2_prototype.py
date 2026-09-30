"""P0 numerical oracle only. Never used by the FastMatch runtime or admission gates."""
import cmath
import math
from dataclasses import dataclass
from functools import reduce
from math import gcd

TAU = 2 * math.pi


@dataclass(frozen=True)
class Config:
    sample_count: int = 256
    max_order: int = 12
    minimum_source_points: int = 16
    minimum_perimeter: float = 20.0
    normalize_scale: bool = True
    maximum_pose_residual: float = 0.035
    symmetry_relative_amplitude: float = 0.002
    peak_relative_tolerance: float = 0.0001
    maximum_hypotheses: int = 16

    def validate(self):
        if not 32 <= self.sample_count <= 1024:
            raise ValueError("INVALID_SAMPLE_COUNT")
        if not 1 <= self.max_order < self.sample_count // 2:
            raise ValueError("INVALID_MAX_ORDER")
        if not 3 <= self.minimum_source_points <= 4096:
            raise ValueError("INVALID_MINIMUM_POINTS")
        for value in (self.minimum_perimeter, self.maximum_pose_residual,
                      self.symmetry_relative_amplitude, self.peak_relative_tolerance):
            if not math.isfinite(value) or value <= 0:
                raise ValueError("INVALID_NUMERIC_CONFIG")
        if self.maximum_hypotheses < 1:
            raise ValueError("INVALID_HYPOTHESIS_COUNT")


def prepare(points, config, *, closed, complete=True, holes=0, components=1):
    config.validate()
    if not closed:
        raise ValueError("OPEN_CONTOUR_LEGACY_FALLBACK")
    if not complete:
        raise ValueError("PARTIAL_CONTOUR_LEGACY_FALLBACK")
    if holes or components != 1:
        raise ValueError("UNSUPPORTED_TOPOLOGY_LEGACY_FALLBACK")
    values = [complex(float(p[0]), float(p[1])) for p in points]
    if any(not math.isfinite(z.real) or not math.isfinite(z.imag) for z in values):
        raise ValueError("NONFINITE_CONTOUR")
    clean = []
    for z in values:
        if not clean or abs(z - clean[-1]) > 1e-9:
            clean.append(z)
    if len(clean) > 1 and abs(clean[-1] - clean[0]) <= 1e-9:
        clean.pop()
    if len(clean) < config.minimum_source_points:
        raise ValueError("INSUFFICIENT_SOURCE_POINTS")
    area = sum((a.conjugate() * b).imag
               for a, b in zip(clean, clean[1:] + clean[:1])) / 2
    if abs(area) <= 1e-9:
        raise ValueError("DEGENERATE_CONTOUR")
    # Winding is a traversal convention, not physical reflection.
    if area < 0:
        clean.reverse()
    lengths = [abs(b-a) for a, b in zip(clean, clean[1:] + clean[:1])]
    perimeter = sum(lengths)
    if perimeter < config.minimum_perimeter:
        raise ValueError("PERIMETER_TOO_SMALL")
    return clean, lengths, perimeter


def resample(points, config, **topology):
    values, lengths, perimeter = prepare(points, config, **topology)
    out, segment, start = [], 0, 0.0
    for j in range(config.sample_count):
        position = perimeter * j / config.sample_count
        while segment < len(values)-1 and start + lengths[segment] < position:
            start += lengths[segment]
            segment += 1
        fraction = (position-start)/lengths[segment]
        out.append(values[segment] +
                   fraction*(values[(segment+1) % len(values)]-values[segment]))
    return out, perimeter


def descriptor(points, config=Config(), *, method="dft", **topology):
    """Signed complex harmonics; no phase information is discarded in storage."""
    values, lengths, perimeter = prepare(points, config, **topology)
    frequencies = [k for k in range(-config.max_order, config.max_order+1) if k]
    if method == "dft":
        samples, _ = resample(points, config, **topology)
        center = sum(samples)/len(samples)
        scale = math.sqrt(sum(abs(z-center)**2 for z in samples)/len(samples))
        coefficients = [
            sum((z-center)*cmath.exp(-1j*TAU*k*j/len(samples))
                for j, z in enumerate(samples))/len(samples)
            for k in frequencies]
    elif method == "efd":
        # Exact integration of a piecewise-linear contour over arc length.
        # This is EFD in equivalent signed complex coefficient form.
        pairs = list(zip(values, values[1:]+values[:1], lengths))
        center = sum((a+b)*length/2 for a,b,length in pairs)/perimeter
        energy = sum(length*(abs(a)**2 + (a.conjugate()*b).real + abs(b)**2)/3
                     for a,b,length in pairs)/perimeter
        scale = math.sqrt(max(0.0, energy-abs(center)**2))
        coefficients = []
        for k in frequencies:
            omega = TAU*k/perimeter
            start, total = 0.0, 0j
            for a,b,length in pairs:
                total -= ((b-a)/length *
                          (cmath.exp(-1j*omega*start) -
                           cmath.exp(-1j*omega*(start+length))) /
                          (omega*omega*perimeter))
                start += length
            coefficients.append(total)
    else:
        raise ValueError("UNSUPPORTED_METHOD")
    if scale <= 1e-9:
        raise ValueError("DEGENERATE_SCALE")
    if config.normalize_scale:
        coefficients = [c/scale for c in coefficients]
    return {
        "schema": "cxgeometry.so2_prototype.v1", "method": method,
        "descriptor_role": "model_screening", "measurement_evidence": False,
        "frequencies": frequencies, "coefficients": coefficients,
        "magnitude": [abs(c) for c in coefficients],
        "centroid": center, "scale": scale, "perimeter": perimeter,
        "config": config,
    }


def distance(reference, observation):
    if reference["frequencies"] != observation["frequencies"]:
        raise ValueError("INCOMPATIBLE_DESCRIPTOR")
    return math.sqrt(sum((a-b)**2 for a,b in
                         zip(reference["magnitude"], observation["magnitude"])))


def _maximize(function, left, right):
    ratio = (math.sqrt(5)-1)/2
    a, b = right-ratio*(right-left), left+ratio*(right-left)
    fa, fb = function(a), function(b)
    for _ in range(40):
        if fa < fb:
            left, a, fa = a, b, fb
            b = left+ratio*(right-left)
            fb = function(b)
        else:
            right, b, fb = b, a, fa
            a = right-ratio*(right-left)
            fa = function(a)
    return (left+right)/2


def match(reference, observation):
    config = reference["config"]
    if observation["config"] != config:
        raise ValueError("INCOMPATIBLE_CONFIG")
    invariant_distance = distance(reference, observation)
    frequencies = reference["frequencies"]
    rc, oc = reference["coefficients"], observation["coefficients"]
    top = max(map(abs, rc))
    active = [k for k,c in zip(frequencies,rc)
              if abs(c) > top*config.symmetry_relative_amplitude]
    result = {"executed": True, "succeeded": False,
              "invariant_distance": invariant_distance,
              "pose_hypotheses": [], "angle_ambiguity": False,
              "measurement_evidence": False, "used_for_seed": False,
              "used_for_prefilter": False, "fallback_reason": ""}
    if len(active) < 2:
        result.update(status="ORIENTATION_UNOBSERVABLE",
                      fallback_reason="CONTINUOUS_ROTATIONAL_SYMMETRY",
                      symmetry_order=0, orientation_period_deg=0,
                      angle_ambiguity=True)
        return result
    symmetry = reduce(gcd, (abs(k-active[0]) for k in active[1:]))
    result.update(symmetry_order=symmetry,
                  orientation_period_deg=360.0/symmetry)
    cross = [a.conjugate()*b for a,b in zip(rc,oc)]
    er, eo = sum(abs(c)**2 for c in rc), sum(abs(c)**2 for c in oc)
    def correlation(shift):
        return sum(c*cmath.exp(1j*TAU*k*shift) for k,c in zip(frequencies,cross))
    def objective(shift):
        return abs(correlation(shift))
    count = config.sample_count
    scores = [objective(i/count) for i in range(count)]
    peaks = [i for i in range(count)
             if scores[i] >= scores[(i-1)%count] and
             scores[i] >= scores[(i+1)%count]]
    candidates = []
    for i in peaks:
        shift = _maximize(objective, (i-1)/count, (i+1)/count)
        value = correlation(shift)
        strength = min(1.0, abs(value)/math.sqrt(er*eo))
        residual = math.sqrt(max(0, 2-2*strength))
        candidates.append({"angle_deg": math.degrees(cmath.phase(value)) % 360,
                           "scale": observation["scale"]/reference["scale"],
                           "correlation": strength, "residual": residual,
                           "reflected": False, "cyclic_shift": shift % 1})
    candidates.sort(key=lambda p: (-p["correlation"], p["angle_deg"]))
    if not candidates or candidates[0]["residual"] > config.maximum_pose_residual:
        result.update(status="POSE_RESIDUAL_REJECTED",
                      fallback_reason="SHAPE_OR_REFLECTION_MISMATCH")
        return result
    best = candidates[0]["correlation"]
    selected = []
    for pose in candidates:
        if best-pose["correlation"] > config.peak_relative_tolerance:
            continue
        if any(abs((pose["angle_deg"]-p["angle_deg"]+180)%360-180) < 0.25
               for p in selected):
            continue
        selected.append(pose)
    if len(selected) > config.maximum_hypotheses:
        result.update(status="HYPOTHESIS_BUDGET_EXCEEDED",
                      fallback_reason="PRESERVE_SYMMETRY_BRANCHES")
        return result
    result.update(succeeded=True, status="AUDIT_POSE_HYPOTHESES",
                  pose_hypotheses=selected, angle_ambiguity=len(selected)>1)
    return result
