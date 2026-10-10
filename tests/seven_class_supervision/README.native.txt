Seven Class Native Supervision Conversion - development trial
============================================================

Follow-up: dataset freezing and the v2 runtime guard are described in
README.dataset.txt. The conversion-only acceptance boundary below describes
the first step; consult that follow-up for dataset-level behavior.

Scope
-----
The production adapter source is cximage/CxSevenClassSupervision.cpp.
The application ParserClass and native test share exactly the same binding
registration. Tests execute saved .cxsc files with the repository's mu::Parser.
No Python, model training, fake learning curve, DLL acceptance or production
approval is involved in this step. The old uncommitted Python probes are not
part of this CMake target or its evidence.

Build and reproduce (PowerShell, repository root)
-----------------------------------------------
cmake -S tests/seven_class_supervision -B ../build_seven_class_supervision_native -G "Visual Studio 17 2022" -A x64
cmake --build ../build_seven_class_supervision_native --config Release --target seven_class_supervision_native_test --parallel 4
ctest --test-dir ../build_seven_class_supervision_native -C Release --output-on-failure

Each run creates a new directory below:
  ../build_seven_class_supervision_native/native-evidence/<unique-time>/
Inspect native_test_receipt.json and case_index.json first. Each English-named
case has a source JSON, a replayable .cxsc, a lossless PGM supervision mask and
a conversion_receipt.json. Outputs never replace test images/history. A PGM
contains class IDs 0..6, not display-normalized grayscale; view with a palette
if needed. Background is 255. These are generated masks, not modified images.

Case names
----------
Seven Class Native arc Conversion
Seven Class Native circle Conversion
Seven Class Native ellipse Conversion
Seven Class Native line Conversion
Seven Class Native open_curve Conversion
Seven Class Native polygon Conversion
Seven Class Native closed_curve Conversion

Manual replay
-------------
Copy a generated script to a new working script. Set load() to the desired
development annotation JSON; set save() to a NEW output directory whose parent
already exists. Replay using:
  ../build_seven_class_supervision_native/Release/seven_class_supervision_native_test.exe --script <working.cxsc>
Existing output directories are deliberately rejected. Use forward slashes in
CxScript paths. A minimal script is:
  SevenClassSupervision conversion;
  conversion.loadrules("D:/.../contracts/seven_class_supervision_rules.v1.json");
  conversion.load("D:/.../line.json");
  conversion.run();
  conversion.expectstatus("CONVERTED");
  conversion.save("D:/.../new-run");

Source data semantics
---------------------
Fixed IDs: arc=0, circle=1, ellipse=2, line=3, open_curve=4, polygon=5,
closed_curve=6. All input vertices are original-image XY coordinates, with
integer pixel centers and fractional annotation vertices permitted. Circle,
ellipse and arc inputs are explicitly sampled human contours; this converter
does not infer a shape from a box, fit a primitive, or invent control points.

arc/line/open_curve must explicitly set closed=false. line has two endpoints.
Their mask is the round-capped/round-joined segment stroke, default FULL width
3 pixels (radius 1.5), not an area formed by connecting the last point to the
first. Original endpoints and vertices remain in the receipt. Samples include
each original vertex; successive distances <= sample_max_step_px (default 1).
Rasterization uses original segments, so changing sampling does not distort
the mask. A region boundary around a stroke is not a closed centerline.

circle/ellipse/polygon/closed_curve require closed=true and a simple ordered
ring without a repeated terminal vertex. Raster fill uses even-odd pixel-center
inclusion plus boundary inclusion. Holes, self-intersections, backtracking,
repeated vertices, empty/disconnected rasters and cross-instance 8-connected
touch/overlap fail explicitly. Open strokes that create a raster hole fail;
the converter does not silently close or repair them. Border strokes are
clipped in the raster only, with unchanged endpoints and a border flag.

Train/Valid require an annotation receipt digest and nonempty annotations.
Unlabelled VERIFY requires empty annotations, no annotation receipt field,
returns VERIFY_NO_MASK and never emits a fake all-background training mask.
Sample JSON fields and rule fields are exact, unknown/duplicate keys rejected.

Rule and evidence binding
-------------------------
Only the checked-in version 1.0.0 semantics are supported. Width [1,128] and
sampling step [0.125,64] can be set in a separate rules file; both values are
hashed. Digests use nlohmann JSON dump() canonical object-key order and SHA256,
not hashes of filenames. The raw mask digest covers row-major uint8 pixels,
WITHOUT the PGM header. sample_binding_sha256 includes rules_sha256,
source_sha256 and mask_sha256; source includes dimensions, split, source group,
image digest, annotations and annotation receipt digest.

Input SHA values are declarations at this conversion boundary. The generated
synthetic fixtures use placeholder source digests; the converter explicitly
reports source_image_digest_verified=false. This is NOT lineage closure or a
dataset-level frozen revision. The runtime must still verify actual image
bytes, bind these sample/rule hashes into the dataset revision and reject
cross-split same-source leakage. These are the next integration steps; do not
put these isolated sample receipts into an accepted training manifest as-is.

Limits/failure behavior
-----------------------
Input JSON <=4 MiB, raster <=16,777,216 pixels, <=128 instances, <=2048 points
per instance, <=1,000,000 intermediate samples. Deterministic work accounting
also bounds point validation, raster evaluation and topology walks at 200M
estimated operations; a large/complex input can hit this limit before its
dimension limit. There is NO automatic downscale or geometry replacement.
Load and rule changes invalidate prior results, including unsuccessful loads.
run() reports a SUPERVISION_* code and clears stale masks on validation failure.
save() requires success and exclusively creates a new directory. Failed export
can leave an incomplete directory; do not reuse it or treat it as accepted.

Acceptance boundary
-------------------
Native Release/Debug tests cover seven classes, open endpoints/no closing/no
box replacement, width and sample spacing, border clipping, SHA known vectors,
rule/split digest changes, VERIFY exclusion, malformed topology, overlapping
instances, invalid dimensions, duplicate JSON keys and stale-state rejection.
No real business image is required or requested.
The full GUI executable, LibTorch training, dataset manifest v2 and offline
runtime DLL package require their own build and integration evidence.
