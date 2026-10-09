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
