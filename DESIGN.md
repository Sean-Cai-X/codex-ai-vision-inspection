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
