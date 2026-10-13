# Full observed gray boundary: acceptance and limits

Date: 2026-10-13. Development audit only; not production admission.

## Application and operator path

Evidence search: `Gray Full Boundary`. Four self-contained external cases:
`full_case_0_r0`, `full_case_0_r8`, `full_case_24_r0`, `full_case_24_r8`.
Select the thumbnail, open Key Parameter Controls (F7), inspect Image / anchor,
Observed normals and Gray profile, then click Run Gray Boundary.
Raw is cyan, accepted refinement green, rejected samples red. Rejected intervals
are not bridged. Source geometry is never replaced with fitted/synthetic corners.

Shared parameters: full_boundary_enabled=0 (legacy default), chunk_core_points=100,
maximum_chunks=64. Halo derives from normal_half_window; core+2*halo must be <=128.
Whole mode rejects multiple components, holes, border touch, endpoint selection
conflicts and budget overflow. Paired pose spans explicitly reject this mode.

## Verified facts

- Native whole-entry matrix: 96 cases, 49,194 raw core points, 46,042 accepted,
  3,152 rejected; per-point parity with independent chunk orchestration.
- Actual CxScript/headless: 96 complete receipts equal native receipts.
- Desktop: all four named cases selected and executed in a fresh isolated Linux
  process. Persisted GUI receipts exactly match the native baseline.
- case_0 r0/r8: 424 raw / 390 accepted / 34 rejected.
- case_24 r0/r8: 343 raw / 309 accepted / 34 rejected.
- Editing maximum_chunks to 1 invalidates the prior result; execution reports
  FULL_GRAY_MAXIMUM_CHUNKS and displays source-only imagery. Restoring 64 and
  rerunning restores the baseline receipt.
- Current fastmatch_harmonic CMake suite: 117/117 tests pass.
- Existing operator GUI processes were not closed or replaced.

External evidence is below cxscript_runs/harmonic_features_20261008:
gray_full_app_v2_20261013, desktop_full_boundary_20261013,
gray_full_app_acceptance_20261013.json and gray_full_boundary_desktop_acceptance_20261013.json.
These assets are deliberately excluded from Git.

## Boundaries and next step

This verifies full Raw coverage and an ordered collection of usable open Refined
runs. It does not prove a complete refined cycle, landmark identity, calibrated
uncertainty, or accepted pose. Corner rejection remains real.
Next: analyze the rejected corner neighborhoods with independent gray raster
truth and rotation/scale coverage; keep identity support and uncertainty separate,
and do not relax gates to manufacture closure.

The isolated launch still reports missing relative annotation-tool/other-tool
asset manifests because of its external working directory. This acceptance is
limited to GrayBoundaryAudit; it does not certify the annotation tool palette,
Torch training, Windows GUI, or an entire production distribution.
