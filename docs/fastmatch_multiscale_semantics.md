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
