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

## P1a native numerical module (2026-09-30)

Implemented `cxgeom/include/CxGeoSO2Harmonic.h` and
`cxgeom/src/CxGeoSO2Harmonic.cpp`, namespace `cxgeom::so2`.
This is a dependency-free C++17 numerical library, built by the isolated
`tests/fastmatch_harmonic/CMakeLists.txt`. It is not yet linked into the
application and does not constitute the full P1 Audit adapter.

The native API exposes Build, Distance and Match, with a configurable sample
count, order, minimum point count/perimeter, scale normalization, phase
residual limit, symmetry threshold, peak tolerance and hypothesis budget.
All public descriptor operations validate dimensions, finite values,
configuration compatibility, method identity and nonzero energy.

Contour input requires an explicit topology_verified assertion, closed and
complete flags, zero holes and one component. Defaults fail closed.
The caller must establish those facts independently: this assertion is not
proof of provenance. The library additionally rejects nonfinite points,
self-intersections/touches/backtracking, insufficient source points and
inputs over the 4096-point work budget. It never infers physical closure
from point count. More detailed topology/provenance acquisition remains an
adapter responsibility.

Coordinates remain isotropic original pixel coordinates. Internal translation
before energy calculations avoids cancellation at large coordinate origins.
The module retains complex phase, handles winding as traversal order,
preserves discrete symmetry branches and declines an observable pose for
a circle. Reflection is not an enabled hypothesis. Geometrically symmetric
shapes can be indistinguishable from their mirrors; this is not a universal
chirality detector. Scores are numerical correlations, not calibrated confidence.

Validation:

- Release build: GCC 14.2, -Wall -Wextra -Werror -pedantic.
- Three CTest groups: native guards, nine Python oracle tests, native parity.
- Native parity: 864 controlled rotation/scale/winding scenarios plus four
  asymmetric mirror/wrong-shape rejection cases.
- Compare coefficients, centroid, perimeter, RMS scale, invariant distance,
  result status, symmetry order, number of poses and pose values.
- Debug build with AddressSanitizer + UndefinedBehaviorSanitizer: same suite.
- Guard tests include malformed descriptors, unverified/open/partial/hole/
  multicomponent inputs, self-intersections, nonfinite input, hypothesis
  exhaustion and a large coordinate translation.

A floating-point false self-intersection of rotated collinear rectangle
segments was found by parity testing and corrected with an orientation
tolerance. This case remains in the rotation sweep.

Portable replay (use an existing compiler toolchain; output stays external):

```sh
cmake -G Ninja -S tests/fastmatch_harmonic \
  -B ../cxscript_runs/fastmatch_harmonic_p1_20260930/build-ninja \
  -DCMAKE_BUILD_TYPE=Release
cmake --build ../cxscript_runs/fastmatch_harmonic_p1_20260930/build-ninja
ctest --test-dir ../cxscript_runs/fastmatch_harmonic_p1_20260930/build-ninja --output-on-failure
python3 tests/fastmatch_harmonic/run_native_parity.py \
  --binary ../cxscript_runs/fastmatch_harmonic_p1_20260930/build-ninja/so2_native_probe \
  --report ../cxscript_runs/fastmatch_harmonic_p1_20260930/native_parity.json
```

For sanitizers, configure a separate Debug build with SO2_SANITIZERS=ON.
This remote host uses the already installed portable host-gcc/host-tools and
its sysroot under /mnt/codex-gpu-runtime/opt/codex-ai-vision; the exact build
commands and logs are recorded in the external evidence directory.
Windows compilation and full application GN integration are not validated
by this isolated Linux test.

Remaining gates are unchanged: validated real observation contours and live
model inventory; versioned reference assets/hash checks; native Audit adapter
and legacy-output replay; then evidence-gated Prefilter/Pose Seed. No actual
image accuracy, speedup, APPROVED or ACTIVE claim follows from this delivery.

## P1b application headless + cxscript gate (2026-09-30)

The primary acceptance path now executes the actual application, not the
Python oracle. GN and CMake include the native math and
`CxFastMatchHarmonicAudit`; `ParserClass` exposes it as `HarmonicAudit`.

Script API (all arguments below are in script order):

- `select(0|1)`: select reference/observation; `clear()` resets that input.
- `point(x,y)`: append original isotropic pixel coordinates.
- `topology(verified,closed,complete,holes,components)`: explicit caller facts.
- `sourceindex(index)`, `fromobject(findobject)`: copy measured outer_boundary,
  hole count and source identity/generation/mask hash; topology stays unverified.
- `parameter(value,"name")`: method (0 DFT / 1 EFD), sample_count, max_order,
  minimum_source_points, minimum_perimeter, normalize_scale,
  maximum_pose_residual, symmetry_relative_amplitude,
  peak_relative_tolerance, maximum_hypotheses.
- `run()`: append one structured Audit result.
- `expectstatus("...")`, `expectcount(n)`,
  `expectpose(angle_deg,scale,tolerance)`: fail the script on mismatch.
- `save(global_harmonic_receipt_path)`: write the receipt into the current
  headless output directory, with parameters, input provenance/topology,
  hypotheses, fallback reason and assertion count.

Input/configuration changes invalidate the current result. The class owns
copies and has no mutable FastMatch reference, no seed/candidate setters and
no production activation method. Explicit topology flags are caller
assertions, not automatic approval of business labels. The receipt is
`cxvision.harmonic_audit_receipt.v1`, always AUDIT and non-production.

Four source-only replay scripts live under tests/fastmatch_harmonic:

1. headless_image_baseline.cxsc: actual FindObject connected-components run.
2. headless_audit.cxsc: the same image operation plus 56 controlled Audit
   runs and 160 assertions (rotation/scale, rectangle/square/asymmetric
   shapes, circle ambiguity, topology rejection and hypothesis budget).
3. headless_object_source.cxsc: copy the actual measured pixel boundary;
   two assertions require unverified-topology fallback.
4. headless_assertion_failure.cxsc: deliberately wrong expectation must
   fail with HARMONIC_STATUS_ASSERTION_FAILED and a nonzero application exit.

Linux offline replay, inside the existing application's runtime environment:

```sh
sh tests/fastmatch_harmonic/run_headless_audit.sh \
  /absolute/path/to/cxvision_imgui_acceptance \
  /external/path/to/input.png \
  /external/path/to/a-new-output-directory 800 600
```

Use an absolute executable/image path, fresh output directory and actual ROI
width/height. No Python is needed by this runner. The supplied image must
produce at least one FindObject component for the source-copy test; arbitrary
images are not promised to pass the component configuration. This is a
replay harness, not an image-quality acceptance policy.

Verified on the external Mpic arc image in the configured Linux runtime:
three successful headless runs; expected failing script rejected; 56 Audit
runs / 160 assertions and two real-source assertions. Baseline, Audit and
source-copy variants produced byte-identical result_overlay.png,
evidence_overlay.png, tool_display.png, object_state.json and
measurement_observations.json. SHA256 receipts cover the actual executable,
input image, scripts and Audit receipts. Files remain outside Git under
cxscript_runs/fastmatch_harmonic_headless_20260930/verified_replay.

The measured source component contained 2311 outer-boundary points and
25 holes and touched the image boundary. It is deliberately NOT admitted as
a valid simple closed Harmonic target. This demonstrates actual image-to-
contour-to-fallback execution, not successful matching of the Mpic arc.
The mathematical pose assertions use scripted controlled contours and must
not be presented as image-derived recognition accuracy.

Integration issues found and fixed by actual replay:

- Numeric class-call arguments are reversed by the legacy parser ABI.
  New Audit wrappers preserve natural script point/topology/pose order.
  The baseline uses FindObject setrect(x,y,width,height), as verified by
  the real binding, rather than the misleading reversed-order old example.
- DefineStrConst used the transient expression-literal buffer size as the
  index into the persistent constant buffer. A second constant resolved to
  the Torch request context instead of its receipt path. Registration now
  indexes the correct buffer; these replay scripts exercise that regression.
- RunCollectedScript now retains std::exception diagnostics so assertions
  and I/O failures are distinguishable from an unknown native failure.

Scope: this is an explicit script-side Audit adapter. It is not automatic
FastMatch reference-asset registration, not a live candidate Prefilter/Pose
Seed hook, and not a complete FastMatch/FormFit regression. The unchanged
output check above is for the exercised FindObject pipeline only. Those
remaining gates and industrial image-quality evaluation are still required.
