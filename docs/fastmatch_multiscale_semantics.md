# FastMatch SO(2) P0 implementation record

Specification: Sean-Cai-X/cxvision, branch codex/cxcore-integration,
Next_20260920.txt. The corrected SO(2) roadmap and development constraints
in the latter part of that document take precedence over its initial SO(3)
proposal. Implementation checkout: codex-ai-vision-inspection.

This first delivery implements source auditing and a standalone numerical
oracle. The application remains on its existing FastMatch path. Python here
is a test oracle for the later native C++ implementation, not an external
training, tuning, or inference service.

## Audited source semantics

| Code location | Verified meaning | Integration consequence |
| --- | --- | --- |
| FastMatch.cpp:5541 levelmodels_l72tol36 | SetModelWH(72,72), GridZoom(36,36), then exact feature deduplication | Grid dimensions are not harmonic orders |
| FastMatch.cpp:5564 / 5573 | m_mapl36_l72[fine_id] = coarse_id | Build a separate reverse adjacency before coarse-to-fine traversal |
| FastMatch.cpp:5653 / 5675 / 5684 | L6 to L3 follows the same direction | Preserve legacy map storage |
| FastMatch.cpp:5472 | list_duplicatesmodel_l72 groups exact modelcompare equality | This is not image-instance NMS |
| FastMatch.cpp:5978 / 5983 | m_imagefastmatchlist holds getfastmodel() features compared element-wise | Allocate a separate Harmonic candidate container later |
| Grid.cpp:2120 / 2183 | Pattern points are concatenated from four directional scans | PointsShape does not imply an ordered closed contour |
| FastMatch.cpp:4582 / 4632 | FormFit reference built from source observations, derived trace and junctions | First descriptor adapter should target the validated reference dense contour |
| CxFastMatchShapeModel.cpp:484 / 507 | Closed resampling followed by closed = dense_points.size() >= 3 | The flag alone does not prove physical closure or valid topology |
| CxFastMatchShapeModel.cpp:569 / 584 | Observation starts from reference and applies an existing seed | Cannot use that observation to independently generate the same seed |
| FastMatch.cpp:2304 | normalized_boundary divides X and Y by bbox width/height separately | Use original outer_boundary pixel coordinates for isotropic scale/pose |
| FindSegmentationTypes.h:16 / 32 | Contour has points and source refs; region has stable instance ID | Carry instance provenance; add explicit topology validation before use |

Line numbers identify the audited revision; generated inventory hashes the
source files, so later movement of lines does not imply a fresh audit.

Runtime model counts are **not observed** by this static audit. Missing
model/72x72 etc. directories do not mean a running application's model
containers have zero entries. The generated inventory uses null plus
NOT_OBSERVED instead of inventing counts.

## First numerical implementation

tests/fastmatch_harmonic/so2_prototype.py uses only the Python standard library:

- Uniform arc-length resampling and signed complex Fourier coefficients.
- Independent analytic integration of piecewise-linear contours, equivalent
  to Elliptic Fourier Descriptor coefficients in complex form.
- Separate invariant magnitudes and phase-bearing coefficients.
- Translation and optional isotropic scale normalization, retaining scale.
- Circular correlation with continuous local refinement for pose hypotheses.
- Winding normalization without silently allowing physical reflection.
- Multiple hypotheses for rectangle, ellipse, square and regular triangle.
- Explicit unobservable angle for a circle.
- Explicit fallback for open, partial, hole-containing and multiple-component
  input. P0 does not synthesize closure or combine separate instances.
- Input/configuration validation and phase-residual rejection.

The correlation value is a numerical alignment score, not a calibrated
probability. Normal-signal descriptors, topology-specific descriptors,
asset serialization, hashing in the native runtime and learned pose fusion
are not implemented in this prototype.

## Repeatable validation

Run from the source checkout:

```sh
python3 -m unittest discover -s tests/fastmatch_harmonic -p 'test_*.py' -v
python3 tests/fastmatch_harmonic/run_p0_audit.py \
  --out ../cxscript_runs/fastmatch_harmonic_p0_20260930 \
  --case-root ../cxscript_runs/mpic_open_boundary
```

The runner requires output outside the source checkout and generates:

- fastmatch_model_inventory.json: source hashes and actual audit scope.
- observation_contour_sources.json: case IDs, source hashes, topology and eligibility.
- so2_p0_report.json: replayable controlled numerical metrics and explicit gaps.

Raw images, labels, generated cases, reports and binaries remain in the
external cxscript_runs directory and are not source-control deliverables.

The controlled library has six contour types, 24 rotations and three scales,
tested with both DFT and analytic EFD. Each query uses a shifted contour start.
Recall@1 describes this six-reference synthetic library only. Circle queries
must yield no angle. These results do not establish industrial image quality,
FastMatch candidate reduction, latency improvement or production eligibility.

The Mpic case audit preserves auto-provisional review status. Open-boundary
cases are fallback cases. The unreviewed ellipse mask is not promoted to
independent ground truth.

## Follow-up gates

1. Capture actual per-level model counts and contour topology from a selected
   FastMatch session; retain null until measured.
2. Export independently obtained, topology-validated closed contours from
   FindSegmentation or original FindObject outer_boundary, with source and
   instance IDs. Benchmark noisy/partial observations against legacy results.
3. Port the chosen math into cxgeom/include and cxgeom/src/CxGeoSO2Harmonic;
   require Python/C++ coefficient, distance, pose and symmetry parity.
4. Add versioned reference assets with corruption/hash checks and an
   Audit-only CxFastMatchHarmonicAdapter. Keep candidate sets, seeds and
   FormFit results unchanged and verify this on fixed replay cases.
5. Only after fixed Recall@K, FormFit, measurement and runtime regression
   gates pass, enable Prefilter and then Pose Seed with explicit legacy fallback.
6. Route, instance-aware duplicate handling and C_N/E(2) pose fusion follow
   those gates. SO(3), bispectrum and GPU work remain later, evidence-driven work.

No APPROVED or ACTIVE status is issued by this P0 work.
