# UX Contract

Scope: FindSetMatch development Evidence flow in the existing desktop application.

## Flow and ownership
Evidence selection stages a complete case before commit. Key Parameter Controls uses the shared parameter schema. Run freezes globals through the existing serial parser-owner chain. Native FindSetMatch produces a receipt. The UI displays execution status, every retained candidate, pose and reference-to-target correspondence IDs/residuals.

## Input and validation
All twelve implemented numeric controls have explicit units, defaults and bounds. Angle, scale and element minimum must not exceed their maxima. Invalid defaults block execution; no silent fallback. Source/search ROI, geometry and provenance remain request-owned, not fabricated numeric controls. Seed and score-gap are reserved, not executable knobs. Script and effective-request receipts are authoritative.

## State transitions
Case selection clears previous results and globals. Parameter edits clear the displayed receipt. Run clears current result and reserves a unique external output directory. Errors remain inline. Historical output paths are labeled historical, not current success. Failed execution must not show a previous case's receipt. No automatic APPROVED/ACTIVE transition.

## Results
COMPLETED indicates bounded execution, not unique solvability. Multiple candidates are retained. Candidate details use a clipped scrollable correspondence table. No forced closure, no invented connecting edges, no model-training or image-extraction claim. Scores and residuals are not probabilities. Raw and effective hashes identify integrity, not signed provenance.

## Persistence and privacy
Input scripts, effective parameters, diagrams and receipts stay outside the repository. Every run uses fresh receipt paths; overwrite is refused. Registration adds roots without removing existing cases. No images, cases, binaries, libraries or generated run packages enter Git.

## Verification
Native parameter tests, actual headless cxscript execution, catalog discovery and ImGui draw/ParserDebugBridge smoke precede handoff. Mouse clicking and visual screen capture remain explicitly unverified until performed. The web-oriented UI audit is supplementary only.

2026-10-08 update: actual Linux X11 mouse/capture acceptance completed for all three Set Match cases, parameter invalidation, invalid-scale blocking, reset, shared top Run and Clear Result. Queued-consumer native regressions now supplement direct bridge smoke. Windows and broader desktop usability are not certified. Private evidence: ../cxscript_runs/setmatch_desktop_20261008/.

Harmonic audit variant (2026-10-08): shared CxHarmonicEvidenceParameters.h validation and RequestHarmonicAuditRun own both Run entry points. Four parameter groups plus actual-receipt debug disclosure preserve native ImGui ownership. Invalid values are not silently clamped; edits/selection/clear invalidate the receipt. Open observations disable closed-pose controls. Unsupported target-design parameters are explanatory text, not enabled controls. No topology bypass or completion claim.
