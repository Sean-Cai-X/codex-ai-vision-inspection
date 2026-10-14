# Corner support diagnostic, 2026-10-13

Status: development diagnostic only. No runtime gap acceptance, pose integration,
GUI control changes or production admission.

## Cause and approach

All 3,152 rejected points in the existing 96-case matrix are rejected by the
observed-normal CORNER_OR_HIGH_CURVATURE gate, before gray profile sampling.
Do not raise the turn threshold: a corner does not have a unique smooth normal.

CxGeoCornerSupport fits TLS lines to two independently supplied support sets,
checks minimum support/span, RMS, intersection conditioning and extrapolation.
The native diagnostic replays the actual GrayBoundaryAudit image entry and
asserts full receipt equality with its prior baseline. It groups rejected indices,
takes at most eight consecutive accepted refined points on each side, and
records an explicit derived intersection hypothesis. Raw/rejection status is unchanged.
Truth vertices are read only after estimation for scoring, never for support selection.

Configuration: minimum_points=5, minimum_span_px=4, maximum_rms_px=.15,
minimum_cross_sine=.25, maximum_extrapolation_px=8. These are explicit native
diagnostic defaults, not newly released GUI/self-tuning controls.

## Observed results

- 96 replayed image/configuration cases; 852 rejected intervals.
- 706 hypotheses; 146 intervals rejected for insufficient/nonlinear side support.
- 706 distinct nearest truth vertices out of 768; no duplicate assignments in this fixture.
- Derived vertex error: mean 0.042316 px, maximum 0.195473 px.
- On 314 same-support pairs for which both raw and refined fits pass identical
  gates: raw mean 0.617571 px versus refined mean 0.038601 px.
  This subset is conditional; it is not an all-sample comparison.
- Original rejected intervals closed: zero. No calibrated uncertainty claim.
- Nine exact rotation/scale guards and four rejection guards pass.
- Five targeted CTest tests pass, including old normal/refinement/chunk/frame guards.

The raster matrix remains synthetic: one asymmetric polygon, two orientations,
three scales, four subpixel phases, two blur levels and two recenter settings.
Nearest-truth scoring is not runtime landmark identity confirmation.
A low line residual is not a calibrated uncertainty bound.

External artifacts: cxscript_runs/harmonic_features_20261008/corner_support_v2_20261013.
Each diagnostic retains gap/support indices, gray SHA, fit residuals, span,
conditioning, extrapolation and posthoc errors. Magenta crosses in the overlay
are derived hypotheses; red rejected points remain. No asset belongs in Git.

## Next gates

1. Vary side-support extent and independently measure intersection stability;
   reject unstable estimates rather than enlarging limits.
2. Add smooth arcs/rounded corners, short supports, low contrast, missing and
   extra fragments as negative/ambiguity controls; current polygon success is insufficient.
3. Confirm identity and local uncertainty before any pose integration.
4. Only then expose the new branch/configuration through CxScript, Key Parameter
   Controls and separate Evidence cases. Preserve Raw/Refined/hypothesis separation.
