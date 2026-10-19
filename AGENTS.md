# Public repository boundary

These rules supplement the parent AGENTS.md and apply to every future change in
this public repository, including commits made by other development threads.

## Project-specific work stays outside this repository

- This repository is the public common-code repository, not the storage location
  for Vision-AI or another dedicated project's private development work.
- Dedicated-project libraries, training experiments, supervision tests, synthetic
  fixture generators, model-tuning implementations, business feedback, registered
  parent bindings, delivery scripts, reports and handoff material MUST NOT be
  added, staged, committed or pushed to this repository's GitHub remote.
- Vision-AI seven-class supervision work belongs to the local sibling workspace
  `../vision-ai-supervision/`, under `D:/Codex-WorkDir/Sean_WorkDir/codex-ai-vision/`.
  It remains available for local editing, compilation and Vision-AI handoff.
  R&D testing and analysis run remotely as specified below. Do not create
  a public Git remote for that workspace or publish it as part of this repository.
- Do not copy private sources back into `tests/`, `tools/`, `docs/`, `contracts/`,
  or a new public module to make a build or commit convenient. Do not use symlinks,
  junctions, submodules, generated copies or `git add -f` to bypass this boundary.
- Public libraries may contain genuinely reusable runtime/API/algorithm code.
  Keep project-specific orchestration and experiments in the private workspace.
  A dedicated-project library must not be promoted into the common library merely
  because it is written in C++ or is needed by a local build. If ownership is
  unclear, establish the boundary with the user before public submission.

## Local build and submission checks

- Public builds must work without the private workspace. Private tests may depend
  on the public checkout using explicit configurable source paths and an optional,
  default-disabled local build extension; do not hardcode private project assets
  or credentials into public build definitions.
- New private build products, model weights, receipts and evidence stay outside
  this Git worktree. Preserve existing immutable deliveries and original images.
- Before any commit/push, inspect `git status` and the staged file list/diff.
  Stage explicit common-code files only. A general request to commit/push does not
  authorize publication of dedicated-project content. Stop if such content is
  staged; do not silently include it or use force-add/all-files staging.
- Do not rewrite or purge historical Git commits for this separation. The user
  explicitly chose to leave history alone on 2026-10-14. Future publication must
  obey this boundary. Do not claim a local move has removed past GitHub content.
- This separation does not authorize committing to the Vision-AI business repo,
  changing its default model, or publishing any private workspace elsewhere.

## Local development and handoff; remote R&D execution

The user established this execution boundary on 2026-10-14 for supervision and
dedicated-project model development. It governs future work; existing local test
records remain historical evidence, not proof of remote execution.

- LOCAL: use direct system tools to read, search, analyze and edit source and to
  compile/build deliverables. This explicit user instruction supersedes the
  earlier gateway-only policy for local work in this project. Prepare version
  manifests and dependency locks, and conduct the handoff to the local Vision-AI
  business environment. Vision-AI business collection, training, inference and
  business acceptance remain its own local workflow, separate from R&D analysis.
- REMOTE: only after the affected local build passes, copy the reviewed source
  revision through the authorized remote gateway and compile it in the existing
  remote environment. After that target-native build passes, run R&D test cases, model
  training, inference, incremental-learning experiments, baseline/candidate
  comparisons, diagnostic capture and reproducible Evidence execution. Human
  inspection and parameter experiments run remotely. Code analysis, interpretation
  of permitted returned logs/evidence, source development and fixes remain local.
  Do not edit algorithms remotely or execute R&D cases locally for convenience.
- ORDER: local analysis/edit -> successful local compile -> gateway source copy
  with hashes -> successful remote compile -> remote test/execution -> evidence
  return -> local analysis/fix. A failed local build blocks transfer of that new
  executable source revision. Record progress and blockers in remote next32.md.
- Local compile/link and package-integrity checks are preparation checks only.
  Do not treat them as remote test, model-quality or human-review acceptance.
  An exception to the execution boundary requires an explicit user instruction.
- Use a controlled private transfer to the explicitly configured remote workspace
  for project-specific sources/assets. Public GitHub must never be used as the
  transfer mechanism for private supervision code, models or case evidence.
  Confirm the remote endpoint, destination and availability before transferring
  or executing. If unavailable, report that remote execution is pending rather
  than substituting local results or assuming an old remote target is valid.
- Bind each transferred run to source revision/private-source digest, case and
  parameter revisions, dataset split/hash, model lineage/checkpoint, runtime and
  dependency hashes. Check target OS/architecture/ABI/CUDA compatibility; a local
  Windows build is not evidence of a runnable Linux artifact. Any required
  target-native build must be recorded distinctly and must not be misreported as
  validation of the local Vision-AI binary.
- Remote cases must expose inputs, actual effective parameters, intermediate
  diagnostics and baseline/candidate outputs for human analysis. Keep execution,
  evidence completeness, model quality and human review as separate statuses.
  A parameter change creates a new run; never overwrite an original image,
  frozen case, model package or previous receipt. Never fabricate missing metrics
  or automatically approve human review.
- Bring back the permitted R&D result/receipt references and artifact digests for
  local packaging and Vision-AI handoff. Preserve the remote run/environment and
  reviewer linkage. Remote R&D success does not replace local business ABI/import
  acceptance, production approval or explicit model activation.
- Do not transfer business images, masks, overlays, screenshots or business paths
  to the R&D remote environment under this workflow. Use controlled R&D assets
  and structured image-free business feedback; require separate authorization for
  any broader data transfer.
