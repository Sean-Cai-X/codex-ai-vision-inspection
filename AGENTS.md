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
  It remains available for local editing, compilation and testing. Do not create
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
