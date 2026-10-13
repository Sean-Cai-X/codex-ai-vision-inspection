# Design Context

Scope: existing C++/Dear ImGui engineering workbench; this increment covers FindSetMatch Evidence and Key Parameter Controls only. Other screens are not re-audited.

## Intent
Operators inspect local geometric-set cases, edit explicit bounded parameters, run native matching, and inspect provenance-preserving receipts. A development match is never presented as production approval or image segmentation.

## Visual language and tokens
Model B: preserve the application's existing ImGui runtime style, font atlas, docking and widget sizing. Canonical owners are the existing GuiMain/ViewController initialization and bundled imgui runtime; this feature introduces no independent palette, fonts, spacing scale, or theme override. Semantic state must be readable as text rather than color alone. No decorative assets or new icon library.

## Component map
| Capability | Canonical owner | Source of truth | Allowed variants | Verification |
| --- | --- | --- | --- | --- |
| Form | ManualConsoleParamRegressionPanel.cpp | CxSetMatchEvidenceParameters.h | Existing two-column ImGui parameter rows | Native bounds checks and catalog smoke |
| Table Selection | Existing Evidence selection transaction | ManualConsoleEvidenceChain.cpp | FindSetMatch synthetic local fixtures | Catalog decode / native bridge smoke |
| Select/Listbox | Existing Evidence catalog | External case manifest | Development Full Set group only | Catalog discovery |
| Scrollbar | Dear ImGui | Runtime style | Clipped correspondence table | ImGui draw smoke |
| Toast | None in this flow | Inline debug_status/debug_reason | No new toast system | Visible textual errors |
| CRUD | Existing local Evidence loader | External case roots registry | Additive registration only | Preserve other roots |
| Date | Not applicable | None | No date editor | Not applicable |

## Layout
Reuse Key Parameter Controls. Parameter labels wrap; values use remaining column width. Candidate details are expandable. Element correspondence rows are clipped in a bounded 200-unit scroll region, avoiding unbounded layout growth. Original stable IDs remain visible. Keep original window navigation and ImGui keyboard behavior.

## Content
UI follows existing English engineering labels with units. Handoff prose may be Chinese. Explicitly identify synthetic diagrams, NOT_RUN/COMPLETED/BUDGET_EXHAUSTED, no image extraction, unsupported partial matching and production=false.

## Verification boundary
Native ImGui/bridge smoke is not desktop click acceptance. The frontend audit script is web-source oriented and cannot certify C++ rendering, accessibility or visual quality. Actual screen capture / Windows verification remain separate acceptance steps.

## Harmonic audit variant (2026-10-08)
Scope also includes Harmonic Key Parameter Controls. Reuse the same ImGui field/table/disclosure and inline status owners. CxHarmonicEvidenceParameters.h owns bounds/defaults; RequestHarmonicAuditRun owns pending transitions. Group controls by source, anchor, encoding and pose, with a separate actual-receipt debug disclosure. No runtime token or palette changes.

## Harmonic full-pose display (2026-10-09)
Use the existing actual-receipt disclosure; do not add an independent theme.
Show candidate index, angle, scale, correlation, residual, translation, units and cyclic shift.
Keep ambiguous candidates; do not auto-select a pose or imply production approval.
Legacy missing fields are unavailable, never zero-filled; partial/malformed mappings are errors.
Render only the current receipt; parameter edits keep existing invalidation behavior.
Long candidate lists scroll within a bounded region. Desktop visual acceptance remains separate.

## Harmonic staged-search controls (2026-10-09)
Keep the existing two-column controls and inline invalidation. The staged group exposes eight bounded controls, OFF by default.
Only canonical bound closed-contour scripts enable the group; open observations remain endpoint/order preserving.
Actual receipt display separates requested and executed configurations, budget use, search completion and reason.
Coarse/fine curves show recorded cyclic-shift correlations, not probabilities or physical angle-grid responses.
Response tables scroll within 160 units. Partial traces explicitly mean incomplete search, never accepted production poses.
Native guards, real CxScript and catalog/ImGui draw smoke are required; desktop click/screenshot acceptance remains separate.

## Evidence current-case search (2026-10-09)
Search edits reveal matching folder paths without activating cases or replacing Image View.
Filtered trees use a separate ImGui ID scope so clearing search restores browse fold state.
Hide Recent Cases during search to avoid unmatched and duplicate results; restore it when empty.
Hidden-catalog lookup and current-case search have distinct explanatory text.
Desktop verification: harmonic search reveals cases; clearing search restores collapsed browsing.
Native regression: 17/17 CTest plus three Harmonic catalog/bridge cases.
Remaining gaps: panel overlap, Chinese glyphs, independent-pair accuracy acceptance.

## Harmonic workspace focus (2026-10-09)
Focus Geometry Workspace explicitly collapses four auxiliary panels; no automatic case change.
Torch training/runtime, analytics smoke and parameter conclusion windows allow title-bar expansion.
Training process polling precedes Begin so collapsing the training image window cannot skip polling.
Desktop: collapse, restore training panel, refocus, and native Harmonic execution verified.
No live training job was started for this layout check. Chinese font coverage remains unresolved.

## Offline CJK font fallback (2026-10-09)
Both GUI initialization paths use CxUiFont.h; default Latin font and 13px sizing are retained.
CXVISION_UI_FONT selects a trusted local CJK TTF/TTC (not downloaded or committed).
Without an override, try standard local Droid/Noto/WenQuanYi paths on Linux and system fonts on Windows.
Explicit missing override keeps Latin fallback and logs UI_FONT_CJK_MISSING with configuration guidance.
Startup logs font path and CJK probe status; native tests inspect glyphs, not file existence alone.
Linux desktop verified Chinese Evidence labels and unchanged case search/focus behavior.
Windows candidate build and visual validation remain unperformed in this increment.

## Paired boundary / landmark controls (2026-10-13)
Reuse Evidence and Key Parameter Controls with one Reference and one Observation group.
Expose image path, dataset revision, explicit span, continuous endpoints, gray parameters,
landmark source/trace/SHA/declared localization error, common-support mode and solver gates.
Binding SHA records bytes; source confirmation and error calibration remain separate facts.
Store the readable execution calls plus an opaque GUI recipe token; reject custom script drift
rather than overwriting it. The token never supplies geometry or execution settings.
Run goes through the existing queued Parser owner. Freeze both image hashes, reject source/case
changes, archive actual runtime receipts and two overlays. Parameter edits invalidate old results.
Image View overlays use only the matching input SHA and current receipt. No fitted pose is drawn
on an unrelated image. Results scroll in a bounded region and preserve all rejection reasons.
Independent asymmetric raster quality is reported separately from native/UI execution success.
Native ImGui/queued execution acceptance is separate from desktop click/screenshot acceptance.
Linux desktop acceptance on 2026-10-13 verified F4 case search/selection, F7 pair controls,
successful Run, stale overlay removal after error edits, localization-error rejection,
parameter restoration, and SHA rejection with actual persisted receipts. Screenshots and
run records stay outside the source repository. Initial auxiliary-panel overlap still needs
workspace focusing; Windows desktop acceptance remains unperformed.
Set CXVISION_TEST_CJK_FONT at CMake configure time to include the local CJK glyph test.

### Continuous matching-landmark arc mode
ROI selection endpoints and physical matching landmarks are separate controls.
Matching start/end, projection distance limit, tie tolerance and provenance are shown per side.
The optional matching_landmark_arc_enabled mode is off by default; legacy recipe tokens
remain byte-for-byte readable when this field is absent. Straight projection and curved
matching-arc modes are mutually exclusive. Raw fraction anchors apply to legacy mode only.
Draw matching landmarks as yellow circles/text on the actual source-bound Image View.
New Linux desktop screenshots verify a combined rotation/scale run, source-SHA rejection,
and the visible wrong-identity counterexample. A local fit may still accept a tangentially
misidentified landmark; production remains false and controlled truth scoring is separate.
Do not represent the test scorer as a runtime physical-identity detector.

### Landmark identity and conditional uncertainty controls (2026-10-13)
CxLandmarkQualityParameters.h is the shared default/range source for fourteen controls.
Keep the existing parameter widgets in a separate collapsible identity/bounds group.
Old canonical scripts without these fields retain exact text and disabled gates.
identity_required and uncertainty_required are explicit development gates, never production approval.
Require matching-arc mode plus image/source binding before enabling either gate.
Separate absolute context mismatch from the near-best-score ambiguity tolerance.
A score is unavailable if no search ran; do not display a budget rejection as a zero-error match.
Freeze executed quality parameters with the image hashes and verify them on actual receipt collection.
Results show both-direction identity statuses and conditional angle/scale/origin/center bounds.
Changing any control invalidates results; no automatic rerun or overwriting prior evidence.
Linux desktop verified Evidence search/selection, visible controls, successful paired Run,
editing uncertainty_required, stale-result invalidation, rejected rerun and actual bounds.
Native catalog/owner tests additionally verify parameter edits change actual execution/gate results.
The initial overlapping utility windows still require workspace focus / window selection.
Windows visual verification and localization-error calibration remain outstanding.

### Boundary input refusal evidence (2026-10-13)
Optional input_rejection_receipt_enabled defaults to off, preserving legacy exception behavior.
When enabled, expected selection/span failures archive the actual decoded image hashes,
executed parameters, candidate indices/coordinates and reasons, with pose_evaluated=false.
This is successful evidence collection, not successful matching; accepted pose candidates remain empty.
Never automatically resolve equal-distance anchors, bridge disconnected components, or draw stale matches.
Results expose refusal details under Executed pose / observed error / fallback; both original source images remain bound.
Linux desktop verified Boundary Stress - roi_snap_ambiguous selection, Run and visible two-alternative refusal.
Ten native-generated stress cases run through the real CxScript/headless path; full suite 115/115 passed.
Moderate blur still causes identity rejection in legacy mode; Windows visual checks and physical error calibration remain open.

### Optional gray-window recentering (2026-10-13)
Expose recenter_iterations (0..16, default 0) and recenter_tolerance_px (default .001 px) in the existing per-side Gray extraction / refinement controls.
Zero/absent iterations preserves legacy scripts and receipts. Editing invalidates results; runtime receipts record the optional method, convergence offsets and final profile center.
No automatic fallback from failed convergence, no quality threshold relaxation, no Raw mutation.
Use the two Gray Localization - Blur legacy/recenter8 Evidence cases for review. Results remain development-only.
Native parameter and legacy-script checks passed; new controls have not received desktop click acceptance.
Moderate blur improves in the controlled fixture; noisy pose error increases and severe blur can fail convergence. Do not enable globally by default.


### Explicit gray foreground polarity (2026-10-13)
foreground_polarity uses the existing shared parameter model: 0 bright > threshold,
1 dark <= threshold. Default/absent stays bright for old scripts and recipes.
Use a two-option combo in standalone and per-side gray controls, without new layout.
No source image inversion; decoded-gray SHA remains unchanged while mask identity changes.
Changing the control invalidates current results and requires an explicit rerun.
Runtime binding records the actual threshold mode. Legacy bright receipt omits the new
parameter for compatibility; requested/actual comparison normalizes absent to zero.
Gray Polarity Evidence cases select a frozen observed open segment then reextract it
from the original image; they do not claim full-contour matching or pose acceptance.
Native and actual headless replay are verified separately from desktop click acceptance.

### Full observed gray contour mode (2026-10-13)
Reuse GrayBoundaryAudit controls; full_boundary_enabled defaults off. chunk_core_points=100 and maximum_chunks=64 are exposed in existing groups; halo derives from normal_half_window and core+2*halo<=128. Optional fields preserve old scripts. Show open accepted runs, never bridge rejected points or silently imply pose-ready closure. Paired pose UI/runtime rejects this mode explicitly. New Gray Full Boundary cases use full_case_* IDs and self-contained assets. Headless ImGui/bridge verified; fresh desktop click acceptance pending.
