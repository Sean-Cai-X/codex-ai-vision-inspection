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

## P1c fixed FastMatch/FormFit replay and snapshot integrity (2026-09-30)

The project headless path now exercises an actual nonempty FastMatch model,
not only a FindObject pipeline. C++ fixture generation writes a deterministic
320x240 raster outside the checkout; four cxscript files cover baseline,
Audit, deliberately changed result and empty-baseline rejection.

New HarmonicAudit methods:

- fromreference(fastmatch): copy reference dense pixel coordinates and model
  ID without trusting the legacy closed flag. Input remains unverified.
- snapshotfastmatch(fastmatch): require an available reference, at least one
  candidate, successful executed FormFit, a non-exhausted budget and mutual
  pairs, then retain a value snapshot (no object pointer is retained).
- expectunchanged(fastmatch): compare candidate count/score/boxes, reference
  and observed dense geometry, FormFit pose/affine/residual and correspondence
  records against that snapshot.

The fixed rectangle requires falling-edge polarity for Top/Left and rising
polarity for Bottom/Right. The initial all-rising configuration still
returned a legacy candidate but had no FormFit reference; the new readiness
assertion rejected it. The replay scripts specify all four directional
profiles explicitly. This is a controlled test profile, not an automatic
business-image tuning policy or a change to product defaults.

Verified final replay: one candidate, FORM_FIT_COMPLETE, 440 reference and
440 observed dense points, 349 mutual pairs, symmetric residual about
0.040215 px, FormFit score about 0.989671. The Harmonic reference-copy
operation correctly returns UNVERIFIED_TOPOLOGY_LEGACY_FALLBACK.
No successful Harmonic matching of that reference is claimed.

Baseline versus Audit result/evidence/tool-display images and shape_model.json
are byte-identical. object_state.json is compared after normalizing only
elapsed_ms fields; timing is not a determinism requirement. The same-instance
native snapshot comparison independently guards the geometric results.
Disabling FormFit after taking the snapshot is rejected; a fresh empty
FastMatch object is also rejected. These are separate processes/cases.

Replay in the configured offline Linux runtime:

```sh
# Build with the installed toolchain. Python is OFF by default.
cmake -G Ninja -S tests/fastmatch_harmonic -B /external/native-build \
  -DSO2_PYTHON_ORACLE=OFF
cmake --build /external/native-build
/external/native-build/make_fastmatch_fixture /external/rectangle.pgm
sh tests/fastmatch_harmonic/run_fastmatch_replay.sh \
  /absolute/app /external/rectangle.pgm /external/fresh-run
# Optional fourth argument gates execution on a previously trusted hash list:
sh tests/fastmatch_harmonic/run_fastmatch_replay.sh \
  /absolute/app /external/rectangle.pgm /external/another-fresh-run \
  /external/fresh-run/reference_bundle.sha256
```

The source-only runner emits reference_bundle_manifest.json
(schema cxvision.fastmatch_reference_bundle.v1), shape snapshots, Audit
receipt and reference_bundle.sha256. It checks hashes and deliberately
tests a wrong executable digest: that child replay must fail before creating
its output directory. The manifest is emitted only after all checks pass.
Hashes cover the executable, fixture, scripts, reference snapshot and receipt.
This is integrity checking against a caller-trusted list, NOT a digital
signature or authentication of the list itself.

Evidence remains at external cxscript_runs/fastmatch_harmonic_replay_20260930/
final_replay; earlier replay_v1/v2 failures remain for diagnosis. Fixture
images, snapshots, binaries and receipts are not committed.

Current asset role is reference_snapshot_only, version 1, production=false.
A native persisted-descriptor loader, signed asset trust, independent physical
closure/provenance checks, occlusion/noise/rotation image regressions and
live Prefilter/Pose Seed are still separate gates. This single fixed raster
does not establish general business accuracy or production readiness.
The optional Python oracle now requires SO2_PYTHON_ORACLE=ON explicitly.

## P1d native descriptor assets and ordinary-user replay (2026-09-30)

CxGeoSO2ReferenceAsset adds a bounded, canonical, locale-independent text
codec (cxvision.so2_reference.v1, AUDIT_ONLY). It retains all configuration,
DFT/EFD method, signed frequencies, complex coefficients (including phase),
centroid, scale, perimeter and an ASCII provenance label (1..1024 bytes).
Maximum payload size is 256 KiB. The codec has no image, GUI, Python or model
registry dependency.

DecodeReference requires a caller-trusted, lowercase SHA-256 digest and
checks it BEFORE parsing. It rejects unknown schema/mode, oversized,
truncated, trailing/noncanonical input, invalid descriptors, incompatible
configuration/method and missing/wrong hashes. It returns a temporary value;
validation failure does not replace an existing descriptor. SHA-256 is
integrity against that supplied digest, NOT signing or authentication.
Trust distribution, production approval and independent topology evidence
are not supplied by this codec.

HarmonicAudit script methods:

- saveasset(path): serialize the selected valid descriptor (or Build from
  the selected explicitly verified contour), refuse existing output/pending
  files, write a pending file then rename; remember its SHA for same-instance
  reloading. Failed I/O may leave a pending file for diagnosis.
- trustedsha(hex): set the expected digest for subsequent loading. In a new
  process obtain it from the trusted evidence manifest, not from the file
  being loaded as a substitute for trust.
- loadasset(path): bounded read, SHA/schema/descriptor/config checks, then
  replace only the selected Audit slot. Failure propagates to headless
  execution and does not install a candidate in FastMatch.
- clear/point/topology reset the selected loaded descriptor; configuration
  changes invalidate results, and run rechecks loaded config compatibility.

Headless injects global_harmonic_asset_path under its output directory.
Receipts record save/load SHA events and identify loaded inputs as
sha_checked_descriptor_asset with topology_evidence=not_embedded.
A provenance string is a trace label, not physical closure proof.
No live FastMatch pointer, Prefilter, Pose Seed or production state is changed.

Reproduce in the configured runtime (all output paths external):

```sh
cmake -G Ninja -S tests/fastmatch_harmonic -B /external/native-build \
  -DSO2_PYTHON_ORACLE=OFF
cmake --build /external/native-build
ctest --test-dir /external/native-build --output-on-failure
/external/native-build/make_fastmatch_fixture /external/rectangle.pgm
sh tests/fastmatch_harmonic/run_reference_asset_replay.sh \
  /absolute/app /external/rectangle.pgm /external/fresh-asset-run
```

Use ONLY the generated solid rectangle fixture for this suite: its script
explicitly declares known closure. The image is actually processed by
FindObject; its 436-point contour is encoded, cleared, loaded and compared.
This is not an automatically verified arbitrary business contour.

Verified this stage:

- Native guard + asset guard tests: 2/2, also 2/2 with ASan/UBSan.
  SHA known-answer vectors, DFT and EFD exact roundtrip, retained value after
  rejected decode, schema/truncation/trailing/size/trust/config/method checks.
- Actual application headless/cxscript: same-process roundtrip and independent
  process reload, each 3 assertions; correct rejection of modified bytes and
  incompatible configuration. External sha256sum agrees with native SHA.
- Baseline/roundtrip/reload image overlays, tool display, object state and
  measurement observations compare byte-for-byte.
- Existing FastMatch/FormFit baseline-versus-Audit and deliberate failure
  regressions pass; existing 56-run / 160-assertion Audit suite also passes.

Evidence is outside the checkout:
../cxscript_runs/fastmatch_harmonic_asset_20260930/
(final_replay, final_fastmatch_regression, final_audit_regression,
native-build, asan-build and build/test logs).
This stage built and ran as devuan using bwrap with the existing mounted
runtime, not sudo/chroot; mounting that runtime remains an administrator
setup step. Python mediated gateway commands only; acceptance execution
was C++ binaries, CTest, shell and cxscript (Python oracle OFF).

Next gates remain rotated/scaled/noisy/occluded raster regressions, independently
verified reference topology/provenance, signed trust distribution if required,
and separately gated live Prefilter/Pose Seed. Successful asset replay is
not business accuracy, APPROVED or ACTIVE.

## P1e separated rotation/scale raster regressions (2026-09-30)

make_raster_fixtures is a C++ pixel generator; run_raster_replay.sh invokes
the actual application headless/cxscript pipeline on each generated PGM.
It does not feed precomputed contour points as image-recognition results.
The base image is measured by FindObject, saved as a SHA-checked descriptor,
then loaded in a separate application process for every observation.

The 14 controlled cases are:

- Base 140x80 rectangle on a 320x240 image.
- Rotation-only: 15, 30, 60, 90 and 135 degrees, scale fixed at 1.
- Scale-only: 0.65, 0.85 and 1.20, angle fixed at 0.
- 3x3 mean blur and deterministic +/-12 intensity noise.
- Same raster occlusion with three different metadata tests: declared
  incomplete, unverified source, and deliberately over-trusted completeness.

The new expectposebounds(angle,scale,angle_tolerance,scale_tolerance) assertion
keeps angle and scale limits independent. The legacy expectpose API delegates
to it with equal limits. Parser argument reversal is handled by its wrapper.
This suite uses 1 degree and 0.02 absolute scale error; deliberate wrong-angle
and wrong-scale assertions both fail. No matching threshold/product default
was changed to pass these tests.

Observed final results:

| Channel | Cases | Result |
| --- | ---: | --- |
| Rotation only | 5 | Maximum angle error about 0.020468 degrees |
| Scale only | 3 | Maximum absolute scale error about 0.007805 |
| Base, mild blur, intensity noise | 3 | Pose hypotheses accepted |
| Declared incomplete | 1 | PARTIAL_CONTOUR_LEGACY_FALLBACK |
| Unverified occlusion | 1 | UNVERIFIED_TOPOLOGY_LEGACY_FALLBACK |
| Deliberately unmarked occlusion | 1 | POSE_RESIDUAL_REJECTED |

For the scale cases, inferred scales were approximately 0.642195, 0.844013,
and 1.201819. Finite pixel sampling changes the measured contour relative
to ideal continuous scaling. All accepted rectangle observations preserve
two 180-degree-separated hypotheses: this is not unique orientation.
The unmarked occlusion shortened the measured rectangle from 140x80 to
100x80 and gave invariant distance about 0.137228; it is rejected as a
shape mismatch. This is NOT a general automatic occlusion detector and
does not make incorrect completeness metadata safe.

The noise amplitude does not cross the fixed segmentation threshold for
this high-contrast fixture. Thus its unchanged contour demonstrates only
limited photometric robustness, not tolerance of jagged edges, texture
confusion or strong noise. Rotation and scale remain separate here; combined
perturbations, other/asymmetric shapes, open boundaries, real business data
and general topology verification are not covered by this gate.

Replay in the configured offline runtime:

```sh
cmake --build /external/native-build
sh tests/fastmatch_harmonic/run_raster_replay.sh \
  /absolute/app /external/native-build/make_raster_fixtures \
  /external/fresh-raster-run
```

The runner requires a fresh external output, generates the image set and
per-case scripts, checks expected status/pose counts/bounds, and only then
emits suite_receipt.json plus raster_sha256.txt. FindObject overlays and
measurements and Audit receipts remain available per case for manual review.
A Linux/NTFS sed -i permission-preservation warning discovered on the first
run was removed by generating scripts in one stream, without changing mount
permissions or elevating execution.

Final evidence: ../cxscript_runs/fastmatch_harmonic_raster_20260930/verified_replay.
Native CTest 2/2 passed; existing reference-asset, FastMatch/FormFit and
56-run/160-assertion Audit regressions also passed with the new application.
All were run as devuan, with Python oracle OFF. Generated pixels, descriptors,
executables and reports are outside Git.

Scope remains AUDIT_ONLY, production_eligible=false. These are controlled
pixel-to-contour SO2 checks, not learned Torch/segmentation training, live
FastMatch rotation-seed integration, business accuracy, APPROVED or ACTIVE.

## P1f asymmetric/complex raster and combined transforms (2026-09-30)

The source-only make_complex_raster_fixtures executable generates 24
400x400 raster images outside the checkout. Three controlled simple-closed
families are covered: bevel (irregular convex polygon), notched (asymmetric
concave polygon) and wavy (smooth undulating asymmetric outline).
The wavy fixture is rasterized from a sampled polygon; the actual matcher
still consumes the independently measured pixel contour and arc-length
DFT/EFD descriptors, not a radial shape representation.

Each family has base, rotation-only 37 degrees, scale-only 0.75, combined
37 degrees/0.75, combined 123 degrees/1.25, blurred combined 65 degrees/0.90,
mirror and partial-occlusion images. Rotation-only and scale-only channels
remain present as controls for combined-transform tests.

run_complex_raster_replay.sh measures each family's base image, saves its
reference asset, and loads the SHA-checked reference in a fresh process
for every observed image. It runs both DFT and EFD: 48 cases total,
36 accepted unique-pose observations, 6 mirror rejections and 6 declared
partial fallbacks. Per-case raw receipts are saved before assertions so
quality failures remain inspectable. A failing case does not hide the
remaining cases; the final suite exits nonzero and reports a failure count.
Reference-creation failure stops the suite because no valid baseline exists.

Verified maximum errors across each family's accepted variants:

| Family | DFT angle deg | EFD angle deg | DFT absolute scale | EFD absolute scale |
| --- | ---: | ---: | ---: | ---: |
| bevel | 0.129051 | 0.108601 | 0.002578 | 0.002579 |
| notched | 0.530896 | 0.529371 | 0.003332 | 0.003325 |
| wavy | 0.112106 | 0.105317 | 0.003371 | 0.003403 |

All 48 expected outcomes passed at the existing 1-degree/0.02-scale
assertion limits. The matching residual threshold remains 0.035; no native
matching logic or product default was changed. Native CTest remains 2/2
and the previous 14-case rectangle raster gate also passes.

An important negative result: mirrored inputs had very small invariant
magnitude distances (EFD about 1.5e-15 to 2.7e-15), but all were rejected by
the phase-based pose residual gate (POSE_RESIDUAL_REJECTED). Magnitude
similarity alone is not sufficient to accept a pose or exclude reflection.
Accepted asymmetric inputs produce one hypothesis, unlike the rectangle's
two 180-degree-separated alternatives.

Run using the configured offline runtime:

```sh
cmake --build /external/native-build
sh tests/fastmatch_harmonic/run_complex_raster_replay.sh \
  /absolute/app /external/native-build/make_complex_raster_fixtures \
  /external/fresh-complex-run
```

The external run contains generated images, per-family reference assets,
per-case scripts/overlays/measurements/Audit receipts, case_results.txt,
suite_receipt.json and complex_sha256.txt. Current evidence:
../cxscript_runs/fastmatch_harmonic_complex_20260930/verified_replay.
Builds and execution use devuan through the gateway and ordinary-user
runtime isolation. The generators explicitly require C++17 for Windows/
Linux portability; Windows execution is not verified in this stage.

Scope remains AUDIT_ONLY. Topology/completeness is declared from the known
controlled fixture; partial fallback does NOT demonstrate automatic
occlusion detection. The suite covers simple closed boundaries, not open
line/arc/open_curve semantics, holes, multi-component association, automatic
boundary selection, strong texture/noise, or arbitrary business photographs.
It is not a learned Torch model upgrade and does not activate production
Prefilter/Pose Seed. Images, assets and binaries are not committed.

## P1g measured topology and explicit source selection (2026-09-30)

This stage fixes two Audit adapter gaps, without enabling a production hook:

- fromobject previously defaulted to source index 0 even when FindObject
  retained several measurements. It now rejects ambiguous selection with
  HARMONIC_AMBIGUOUS_SOURCE_SELECTION. Call sourceindex(index) explicitly
  before EACH multi-source import. The selection is consumed on successful
  import and resets to the default; it is not a persistent object identity.
- topology previously could overwrite the measured hole count. It now
  rejects a contradictory hole count or component count for an imported
  measurement with HARMONIC_TOPOLOGY_CONTRADICTS_MEASUREMENT. A selected
  FindObject measurement is one component; the retained source count is
  recorded separately from that selected contour's component count.

fromobject also rejects a configuration that disables hole-boundary evidence.
Topology booleans/counts are validated. Raw-input receipts now record
measured_source_count, measured_holes and selected_source_index; -1 means
not a measured input. Clear, explicit point editing and loaded descriptor
replacement discard that measurement metadata. Point editing changes the
source label to script_points rather than retaining measured provenance.

The source count is FindObject's retained, filtered measurement count,
not a guarantee about every object in the image. Hole protection preserves
the actual reported selected-measurement evidence; it does not prove perfect
segmentation or completeness. Imported inputs still start unverified/open.

New C++ fixtures and the actual headless/cxscript suite cover a half-plane
interface, a ring with one hole, and two unequal disconnected rectangles.
Eleven scenarios run under DFT and EFD (22 expected outcomes):

- Unverified half-plane: UNVERIFIED_TOPOLOGY_LEGACY_FALLBACK.
- Explicit open interface: OPEN_CONTOUR_LEGACY_FALLBACK.
- Measured ring: UNSUPPORTED_TOPOLOGY_LEGACY_FALLBACK.
- Attempt to declare that ring hole-free: rejected.
- Multiple sources without selection: rejected.
- Explicit source 0 and explicit source 1: valid self-matches.
- Source 0 versus source 1: shape residual rejection.
- Attempt to reuse a consumed source selection: rejected.
- Out-of-range index: rejected.
- Invalid topology boolean: rejected.

All 22 passed. The suite also checks that ring and selection receipts contain
the measured hole count, source count and selected index. Existing 48-case
complex raster, 14-case rectangle raster, descriptor asset, FastMatch/FormFit,
and 56-run/160-assertion Audit suites pass with the rebuilt application.
Native CTest remains 2/2. The Audit suite was additionally rerun using the
existing external Mpic arc image and passed its expected unverified fallback
and unchanged FindObject output checks. Its explicit index selection is now
repeated before each import to conform to the one-shot contract.

```sh
cmake --build /external/native-build
sh tests/fastmatch_harmonic/run_topology_replay.sh \
  /absolute/app /external/native-build/make_topology_fixtures \
  /external/fresh-topology-run
```

Evidence: ../cxscript_runs/fastmatch_harmonic_topology_20260930/verified_replay
and sibling regression directories, including mpic_regression. Source-only
fixtures/runner are committed; generated images, receipts, assets and binaries
remain outside the checkout.

Important boundary: the half-plane raster's extracted region outline is
closed by the image edges, whereas the intended physical interface is open.
The explicit open declaration keeps that interface out of closed SO2
matching; automatic physical-interface selection has NOT been implemented.
This stage supplies safe rejection and traceable selection, not open-curve
matching, hole-aware shape matching, multi-component association or automatic
anchor tracking. Those require dedicated observation/selection semantics.
No APPROVED/ACTIVE state or production Prefilter/Pose Seed is enabled.

## P1h measured-boundary anchor selection (2026-09-30)

HarmonicAudit adds script-configurable anchorrect(x,y,width,height) and
anchorpoints(minimum). Coordinates are original image pixels, with a
half-open rectangle [x,x+width) x [y,y+height). All values must be finite,
x/y nonnegative and width/height positive. Minimum support defaults to 3
measured points and accepts 1..4096. Parser numeric reversal is handled
by an explicit wrapper, as with the existing pose/topology APIs.

This is an outer-boundary point selector, NOT bounding-box overlap or
centroid selection. On the next fromobject call it examines retained
FindObject measurements:

1. Count components with at least one measured outer-boundary point inside
   the anchor. Zero is HARMONIC_ANCHOR_NO_BOUNDARY; more than one is
   HARMONIC_ANCHOR_AMBIGUOUS_COMPONENTS.
2. For the unique component, count cyclic contiguous runs of inside points.
   More than one is HARMONIC_ANCHOR_AMBIGUOUS_ARCS. An entirely contained
   contour counts as one run.
3. Require the configured minimum number of inside points, otherwise return
   HARMONIC_ANCHOR_INSUFFICIENT_SUPPORT.
4. Import the selected complete outer contour with the existing measured
   hole protections. Selection is consumed only after successful import.
   It does not change topology verification, closed/completeness flags.

Explicit sourceindex and pending anchor selection cannot coexist; either
order raises HARMONIC_SELECTION_CONFLICT. Reusing a successfully consumed
anchor does not silently reselect in a multi-component image. Before import,
another valid anchorrect call can replace the pending rectangle. After a
failed import the pending selector remains for explicit correction/retry.
No persistent cross-image object identity or automatic tracking is implied.

Successful input receipts include anchor_selection with basis
outer_boundary_points, rect_half_open (x,y,width,height), minimum_points,
candidate_count, hit_points and contiguous_runs. Existing source ID,
generation, mask hash and selected index remain recorded. Explicit point
editing, clearing or loading an asset removes live anchor evidence. The
descriptor asset does not pretend to embed that live image-selection proof.

Headless verification: 15 scenarios x DFT/EFD = 30 expected outcomes passed:

- Left/right component boundary anchors and a full-component anchor select
  the expected source and preserve valid self-matching.
- A rectangle covering both components rejects ambiguity.
- A strip hitting two separated arcs of the same contour rejects ambiguity.
- Interior-only and background-only anchors reject instead of selecting by
  bounding box. An inner-hole-only anchor does not select the outer boundary.
- Insufficient support, invalid rectangles and invalid support counts reject.
- Both index/anchor conflict orders and stale anchor reuse reject.
- An anchor on a physical interface selects its region source but retains
  UNVERIFIED_TOPOLOGY_LEGACY_FALLBACK; anchoring is not closure evidence.

The test checks the serialized rectangle, selected indices and actual
boundary-point basis, not just application exit status. Run with:

```sh
sh tests/fastmatch_harmonic/run_anchor_replay.sh \
  /absolute/app /external/native-build/make_topology_fixtures \
  /external/fresh-anchor-run
```

Evidence: ../cxscript_runs/fastmatch_harmonic_anchor_20260930/verified_replay.
The application was rebuilt as devuan. Native CTest 2/2 and the topology,
complex raster, rectangle raster, reference asset, FastMatch/FormFit and
Audit suites passed, including the existing Mpic arc fallback regression.
All acceptance execution remains native application/headless/cxscript;
images, binaries and receipts remain outside Git.

Limitations: this selects an outer-contour source and does not yet extract
the anchored subcurve. A segment crossing a tiny anchor without a sampled
point may be missed (safe rejection); there is no segment interpolation.
Coordinates are not automatically transformed across rotations/scales.
There is no automatic closed/open determination, inner-hole selection,
open-curve matcher, GUI integration or production activation. The next
stage is a distinct open-subcurve observation contract with endpoint/order/
provenance preservation; it must not reuse closed SO2 matching by forcibly
closing the selected boundary segment.

## P1i open-subcurve observations (2026-09-30)

`HarmonicAudit.fromobjectarc(FindObject)` consumes a boundary anchor and
retains exactly one contiguous run of measured outer-boundary samples.
`CxGeoOpenBoundary.h` is the pure numerical extraction contract. It retains
the parent's forward cyclic order, original sample indices and endpoint
coordinates, including a run crossing the parent's index-zero seam.
No sorting, reversing, endpoint interpolation, fitting or forced closure is
performed. Whole-contour selection, multiple arcs/components, coincident
endpoints, missing anchors and point-budget violations are rejected.

`saveopen(global_open_boundary_receipt_path)` writes
`cxvision.open_boundary_observation.v1`: source object/generation/mask trace,
parent point count, parent hole count, selected component index, anchor
selection, ordered samples with parent indices, start/end, and seam-wrap
flag. Files are published from a pending file and refuse overwrite.
`closed=false`, `physical_boundary_verified=false` and
`production_eligible=false` remain explicit.

An extracted run cannot be declared closed/complete, edited in place, or
exported as a closed SO2 descriptor. Call `clear()` before independent point
editing. `run()` returns `OPEN_CONTOUR_LEGACY_FALLBACK` with no SO2 poses;
this is intentional, not a successful open-curve match. Ring-source hole
evidence is retained even when only an outer arc is selected.

Native extraction guards and 28 actual headless/cxscript cases pass,
covering left/right/corner, physical interface and ring arcs, cyclic order,
stale/absent anchor, whole contour, separated arcs, force-close/complete,
descriptor export and edit/clear rejection. Existing anchor, topology,
complex combined transform, raster, reference-asset, FastMatch/FormFit and
Mpic Audit regressions also pass.

## P1j Harmonic Audit Evidence / Key Parameter Controls (2026-09-30)

Three external cases have been published under
`../cxscript_runs/harmonic_audit_evidence_20260930/` and the root added to
`../cxscript_runs/_shared/evidence_case_roots.json`, retaining existing roots:

| case_id | Evidence display name | Meaning |
| --- | --- | --- |
| harmonic_audit_closed_reference | Harmonic Audit - Closed Reference | Controlled rectangle self-comparison; explicit fixture topology, not business-image verification |
| harmonic_audit_open_interface | Harmonic Audit - Open Interface | Anchored straight physical interface, open output |
| harmonic_audit_open_arc | Harmonic Audit - Open Arc | Anchored outer arc of a ring; parent hole evidence retained |

These are `REFERENCE_ONLY`/AUDIT fixtures, not Torch training candidates,
approved models or production matches. Labels remain proposed and are not
human-accepted training annotations. Each package includes the unmodified
source raster, typed fixture label, actual observation/Audit receipt,
headless overlay, result summary, replay script, manifest and SHA list.
The inherited headless overlay shows source-tool evidence; it must not be
interpreted as a dedicated live open-subcurve overlay.

Operator flow:

1. Run the rebuilt application with the established external
   `CXVISION_RUN_ROOT`. In Evidence click **Reload Evidence Assets**.
2. Select **CxFastMatchHarmonicAudit / Audit Cases** and one of the names above.
   These registered asset cases are scanned into the current Evidence list.
   If manually hidden, restore through the existing visibility controls.
3. Return the annotation tool to Pointer/Pan if Magic Wand/Auto Boundary
   controls currently own the parameter window.
4. Open **Key Parameter Controls**. It has a dedicated Harmonic Audit path,
   not the ordinary FastMatch panel.
5. Change parameters and click **Run Harmonic Audit**. The existing serial
   Debug Compiler consumes a frozen input snapshot. **Reset Harmonic Case
   Defaults** restores the selected script's defaults.
6. Review **Audit result**, status/reason and **Last output** in that panel.
   Each run gets a new external `cxscript_runs/harmonic_audit_manual/run_*/`
   directory containing input_parameters.json, replayable run.cxsc and
   harmonic_audit_receipt.json; open cases also contain
   open_boundary_observation.json. Failed runs retain inputs and do not
   fabricate a successful receipt. Inputs are local and not uploaded.

Shared schema: `cximage/CxHarmonicEvidenceParameters.h`, consumed by both
GUI seeding and headless replay. All 21 controls have defaults and bounds:

| Control | Default / unit |
| --- | --- |
| Method | 0 DFT; 1 EFD |
| Resample points / maximum harmonic order | 256 / 12; order must be less than half the sample count |
| Minimum source points / minimum perimeter | 16 / 20000 milli-pixels = 20 px |
| Normalize scale | 1 |
| Maximum pose residual | 35000 ppm = 0.035 |
| Symmetry relative amplitude | 2000 ppm = 0.002 |
| Peak relative tolerance | 100 ppm = 0.0001 |
| Maximum pose hypotheses | 16 |
| Anchor x/y/width/height | Interface 150/100/20/20 px; rectangle 35/55/15/20; ring arc 210/105/20/30 |
| Minimum anchor points | 3 |
| Foreground threshold / minimum component area | 128 / 20 pixels |
| Source ROI x/y/width/height | 0/0/320/240 px |

SO2 descriptor/pose settings are recorded but cannot turn an open observation
into a closed match. The extraction and source-image settings control the
open cases. Topology verification is not a permissive UI checkbox.
The rectangle self-comparison script's explicit topology declaration is
only justified by its controlled fixture, and must not be reused to
declare arbitrary business images verified.

Verification commands (native application, no Python test harness):

```sh
ctest --test-dir /external/native-build --output-on-failure
sh tests/fastmatch_harmonic/run_evidence_replay.sh \
  /absolute/app /external/native-build/make_topology_fixtures /external/new-run
sh tests/fastmatch_harmonic/publish_evidence_cases.sh \
  /external/new-run /external/cxscript_runs/new-case-root
CXVISION_RUN_ROOT=/external/cxscript_runs /absolute/app \
  --harmonic-evidence-catalog-smoke
```

The publisher creates local assets only; register its relative destination
in evidence_case_roots.json before refreshing Evidence. No images or
generated packages belong in the source repository.

Evidence is recorded under
`../cxscript_runs/fastmatch_open_boundary_20260930/`.
Native CTest: 4/4. Evidence-script replay: 6/6, including changed method,
sample count and residual recorded in the receipt, invalid-order rejection
and ambiguous-anchor rejection. Catalog smoke uses the real asset scanner,
decodes thumbnails/images and renders the actual ImGui parameter component
without a display. It also exercises the GUI ParserDebugBridge with defaults
and edited sample count, verifies receipts and replayed defaults.
This is not desktop click/screenshot acceptance; that remains human review.

## GSM0 / next1.md foundation (2026-10-08)

Scope: the first code deliverable for the external next1.md roadmap is P0,
not completion of the P1-P4 matching roadmap. Existing Harmonic numerical
and application behavior is preserved. No new GUI tool, matching service,
segmentation refinement or production activation is exposed in this phase.

Corrections required before implementing that roadmap:

- The existing implementations are CxGeoSO2Harmonic and CxGeoOpenBoundary,
  not an existing cxgeom/harmonic tree. Open-boundary extraction does not
  implement harmonic continuation or an open-curve matcher.
- A polygon's coordinate sequence cannot prove that it originated from a
  continuous physical boundary. A sorted point set can have exactly the
  same coordinates as a legitimate polygon. Explicit representation,
  connectivity/order evidence and source provenance must therefore survive
  ingestion. "Monotonic point order" must not mean monotonic image x/y.
- A short endpoint distance cannot prove closure. The observed closing edge
  and completeness must be explicitly established; do not fill gaps.
- A 2D translation/rotation/uniform-scale model is Similarity2d, not an
  unrestricted affine matrix. Collinearity alone is not an automatic
  failure for every similarity-estimation problem; observability must be
  evaluated for the particular correspondences and requested parameters.
- "10 elements", "50% missing" and "no false match" are controlled test
  conditions, not universal guarantees. Symmetry, spatial support and
  near-duplicate structures require separate ambiguity tests.
- Runtime/resource failure is not geometric INSUFFICIENT. Budget exhaustion
  must remain separate from the four geometric solvability conclusions.

Implemented source layout:

- cxgeom/geometric_set_matching/types.h: independent point, line and signed
  arc elements, per-element stable_id/source_ref/quality, set-local ROI,
  request identity, explicit search and transform bounds, bounded policy,
  correspondence/candidate/result value types.
- validation.cpp: nonmutating request validation. IDs are unique within each
  side, not artificially unique across both sides. Checks include finite
  bounded coordinates, quality, nonzero segments/arcs, entire arc support
  inside ROI (not just endpoints), parameter ranges, target search ROI and
  element budget. Duplicate coordinates with distinct IDs are not magically
  resolved: correspondence/observability remain future solver work.
- topology.h/.cpp: ContourTopologyValidator for explicit branch routing.
  Unordered points cannot enter contour branches; multiple parts cannot be
  silently concatenated. Ordered chains require order evidence. Checks
  reject self-intersection/touching nonadjacent edges, adjacent backtracking,
  duplicate neighbors, unsupported holes and configured edge gaps.
  Open chains remain open; closed chains require caller confirmation,
  observed closure and completeness. Pair-check and point caps fail closed.
- contracts/geometric_set_match.schema.json: versioned JSON Schema with
  request and result envelopes, primitive geometry and parameter units.
  Geometry-semantic checks are the C++ typed validator's responsibility;
  P0 does not add a JSON request deserializer or a generic JSON Schema
  validation service.
- CMake and GN pure-library / native-test targets. No GUI, OpenCV or Torch
  dependency in the core; the test uses the existing vendored JSON reader
  to check schema syntax and selected contract/default consistency.

P0 status semantics:

1. Validate(request) accepted means structurally valid typed input ONLY.
2. Match(valid_request) returns NOT_IMPLEMENTED and
   GSM_P0_MATCHER_NOT_IMPLEMENTED, with absent solvability, empty candidates,
   search_complete=false and production_eligible=false.
3. Invalid input and element-budget exhaustion return distinct execution
   statuses. No identity-transform placeholder or fabricated match score.
4. TopologyReport retains source and order evidence references and always
   leaves physical_boundary_verified=false. It detects contradictions,
   not the truth of arbitrary caller assertions.
5. The new topology gate is an explicit API for the future typed intake.
   It is NOT silently retrofitted into legacy HarmonicAudit.topology().
   Existing callers still carry their existing topology-proof obligation.

Default thresholds are provisional parameters, not a calibrated solvability
domain. Search ROI has no implicit whole-image default; the caller must
supply it. Coordinates and residuals use input-image pixels; angles use
degrees; scale is positive and uniform. Angle bounds currently describe
one nonwrapping interval in [-180,180]. Element ROIs use inclusive bounds
for geometric support, independently of the existing pixel-anchor half-open
convention.

Native verification:

```sh
cmake -S tests/geometric_set_matching -B /external/gsm-build
cmake --build /external/gsm-build
ctest --test-dir /external/gsm-build --output-on-failure
# In the configured Linux GN build environment:
gn gen out/linux
ninja -C out/linux geometric_set_contract_test
out/linux/geometric_set_contract_test contracts/geometric_set_match.schema.json
```

GSM0 tests include order permutation, ID/source preservation, malformed
geometry/ROI/parameters, curve-routing and closure guards, budget status and
the explicit NOT_IMPLEMENTED contract. Use GSM_SANITIZERS=ON for native
address/undefined-behavior checks. Generated receipts and binaries belong
under ../cxscript_runs/geometric_set_p0_20261008/, never in Git.

Next bounded stage: P1 full-set point/line correspondence and similarity
estimation with original-element IDs retained. Add non-symmetric controlled
fixtures, arbitrary input permutations and explicit symmetric negatives;
do not publish uniqueness based on one arbitrarily selected hypothesis.
Only after that core has native evidence should FindSetMatch, cxscript,
GUI controls and refined model/FastMatch integration be connected. P2
partial matching, P3 calibrated solvability and P4 business end-to-end
acceptance remain pending.

2026-10-08 verification: GSM0 76 native assertions passed in both CMake and
GN builds; address/undefined sanitizer CTest passed. Existing Harmonic CTest
4/4, open-boundary headless 28 cases and Evidence-script replay 6 cases
passed. Logs and test receipts are in the external run directory above.

## GSM1 / next1.md full-set algorithm core (2026-10-08)

This section supersedes GSM0's "Match returns NOT_IMPLEMENTED for every valid
input" statement. P0 preflight/topology contracts remain, but Match now executes
a **bounded P1 full-set development matcher**. This is not completion of the
whole P1 application integration or of the next1.md roadmap.

### Implemented

- Pure native C++17 positive-scale 2D similarity least squares in
  `cxgeom/geometric_set_matching/pose_estimation.{h,cpp}`.
  No reflection, anisotropic scale or affine shear.
- `matching.cpp`: stable-ID-sorted working indices (caller arrays untouched),
  farthest reference representative pair, type-compatible target pair enumeration,
  reciprocal unique-nearest full-geometry correspondence, one bounded
  least-squares refinement and reciprocal reprojection check.
- Point representative = point; line = midpoint; arc = center. Line residual
  compares both endpoint assignments; arc residual checks center/radius,
  radius-scaled sweep difference and five support samples in both traversal
  directions. Full circles ignore start/traversal. Set elements are never
  connected into an invented contour.
- Explicit angle/scale bounds, entire target support inside search ROI, positive
  element quality, minimum element count, hypothesis/work/time caps.
- Results retain reference/target IDs, set provenance references, per-element
  scores/residuals, candidate poses, elapsed time and work count. Source details
  remain in the input elements addressed by those IDs.
- Multiple valid correspondences are retained, not replaced by the lowest
  residual candidate as a purported unique result. Budget exhaustion clears
  candidates and never reports completed search.
- CMake and GN compile the same native core and native regression executable.

### Exact capability and parameter semantics

This is complete-set matching with equal element counts, no missing elements and
no clutter-removal stage. Different counts, zero-quality inputs, insufficient
anchor count, coincident representatives or out-of-search support return explicit
NOT_IMPLEMENTED reasons; these are P1 capability exclusions, **not proofs that the
underlying geometry is unsolvable**. Concentric arcs and common-midpoint segment
sets may carry orientation information but are not handled by this anchor
generator yet.

For noiseless nondegenerate full sets, enumeration covers the target assignments
of the selected representative pair within its budget. For noisy sets this is
not an exhaustive continuous optimizer or a calibrated robust estimator.
Reciprocal distance ties are refused; duplicate elements cannot be assigned an
arbitrary unique correspondence. No candidate means this search found no
reciprocal full-set candidate, not a global impossibility certificate.

- `search_complete`: finite P1 enumeration completed; not P3 global uniqueness.
- `solvability` remains null; observability flags remain false until P3.
- `production_eligible` remains false in code and schema.
- Candidate `residual_px` is maximum support residual, not a probability or mean.
- Quality influences correspondence score, not the unweighted pose fit.
- Full-set coverage/span equal 1 because every reference element is matched.
- Coverage/span thresholds are automatically satisfied for this full-set path.
- `min_candidate_score_gap` and `seed` remain reserved: no candidate pruning or
  randomized sampling is performed by P1.
- Translation follows the existing schema coordinate bound of +/-1e9.
- Existing Harmonic GUI/cxscript behavior is not rerouted by this new module.

### Verification and external evidence

Native test `tests/geometric_set_matching/matching_test.cpp` runs 192 exact
controlled configurations: 10 elements, four types (points / segments / arcs /
mixed), eight angles including both +/-180 endpoints, scales 0.8/1.0/1.2,
and two independent array shuffles. Target IDs differ from reference IDs; matching
does not use identical IDs as supervision.

Tests require correct ID correspondences for every element and subpixel pose
accuracy. Negative/control checks include hypothesis/work exhaustion, search and
transform bounds, withheld partial matching, coincident representatives, four
square symmetries, reflection/shear/anisotropic scale, duplicate ties, altered
line orientation, altered arc extent, full circles, collinear asymmetric sets,
input immutability and renamed target IDs.

Both CMake and GN receipts record each configuration's pose, correspondences,
residuals, evidence references and elapsed time. These are synthetic algorithm
checks, **not image-based business accuracy, GUI acceptance or production timing**.
No Python matcher/oracle is used; Python only mediates gateway editing/build
orchestration. Existing Harmonic regression uses native CTest and headless
cxscript scripts.

Local-only outputs:
`../cxscript_runs/geometric_set_p1_20261008/`.
Generated fixtures, receipts, binaries and images stay outside the checkout and
are not published to GitHub.

### Next steps (not implemented by GSM1 core)

1. P1 application/typed-result adapter and FindSetMatch integration, with evidence
   snapshots and parameter controls; do not expose an unimplemented image
   extractor or route a set into closed Harmonic.
2. P2 robust partial matching, missing/clutter accounting and bounded estimation.
3. P3 calibrated observability/solvability, ambiguity distinction and acceptance
   domain matrices; do not turn these 192 fixtures into a universal guarantee.
4. P4 user-visible Evidence cases and replay/GUI handoff after those adapters
   operate on real structured inputs.

Verified outcome: P1 2,901 native assertions / 192 matrix runs; P0 76 assertions.
CMake and GN passed, ASan/UBSan CTest 2/2 passed. Existing Harmonic CTest 4/4,
open-boundary headless replay 28 cases and Evidence replay 6 cases passed.
The exact synthetic maximum residual was below 1e-8 pixel; this is numerical
recovery on generated geometry, not measured-image accuracy.
