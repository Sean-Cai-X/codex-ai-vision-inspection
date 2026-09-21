# Torch Runtime UI Evidence Cases

This local Evidence Chain package gives the UI torch thread a stable board-level image and case set for small CPU validation.

## Local Images

- image_id: `torch_deeppcb_g1_00041000_input`
- image_path: `D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/evidence_images/torch_deeppcb_g1_00041000_input.jpg`
- sha256: `624B448F0737AAA8BDC53963A752800147BFFB2591365B9810B3F2A83B956C4A`
- source: copied from `D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/evidence_assets/ensmallen/ENS_L1_deeppcb_template_match_phase1_opt/candidate_20260807_ensmallen_phase1/deeppcb_g1_00041000_input.jpg`
- label_source: `D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/evidence_assets/ensmallen/ENS_L1_deeppcb_template_match_phase1_opt/candidate_20260807_ensmallen_phase1/deeppcb_g1_00041000_label.txt`

- image_id: `torch_deeppcb_g0_00041008_input`
- image_path: `D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/evidence_images/torch_deeppcb_g0_00041008_input.jpg`
- sha256: `A4C2801292898B9F5A85120D2441C4C33903F226BE1832862497656C3E0BC5DB`
- source: copied from `D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/evidence_assets/ensmallen/ENS_L1_deeppcb_template_match_phase1_opt/candidate_20260807_ensmallen_phase1/deeppcb_g0_00041008_input.jpg`
- label_source: `D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/evidence_assets/ensmallen/ENS_L1_deeppcb_template_match_phase1_opt/candidate_20260807_ensmallen_phase1/deeppcb_g0_00041008_label.txt`

- image_id: `torch_deeppcb_g1_00041000_template`
- image_path: `D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/evidence_images/torch_deeppcb_g1_00041000_template.jpg`
- source: copied from `D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/evidence_assets/ensmallen/ENS_L1_deeppcb_template_match_phase1_opt/candidate_20260807_ensmallen_phase1/deeppcb_g1_00041000_template.jpg`
- label_source: none; reference/template image only.

- image_id: `torch_deeppcb_g0_00041008_template`
- image_path: `D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/evidence_images/torch_deeppcb_g0_00041008_template.jpg`
- source: copied from `D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/evidence_assets/ensmallen/ENS_L1_deeppcb_template_match_phase1_opt/candidate_20260807_ensmallen_phase1/deeppcb_g0_00041008_template.jpg`
- label_source: none; reference/template image only.

- storage rule: images stay under `D:/Codex-WorkDir/Sean_WorkDir/cxvisionai` and must not be placed under `cxvision_repo` or committed to Git.
- migration note: the earlier `torch_l1_line_high_contrast_001` binding was only a temporary UI plumbing image; torch candidate cases now use DeepPCB board evidence images.
- dataset note: the current torch dataset binding is `torch_deeppcb_dataset_smoke_v1`; it exists to validate UI image-set and annotation flow before a full training dataset is promoted.
- dataset limitation: only `torch_deeppcb_g1_00041000_input` and `torch_deeppcb_g0_00041008_input` currently have label-backed bbox annotations. Template images are reference/unlabeled context and must not be counted as accepted supervised training labels.

## Manual UI Dataset Binding

- train: `torch_deeppcb_g1_00041000_input`, `torch_deeppcb_g0_00041008_input`, `torch_deeppcb_g1_00041000_template`, `torch_deeppcb_g0_00041008_template`
- validation: `torch_deeppcb_g0_00041008_input`, `torch_deeppcb_g1_00041000_template`
- test: `torch_deeppcb_g1_00041000_input`, `torch_deeppcb_g0_00041008_template`
- annotations: YOLO-style normalized `class_id cx cy w h` records from the DeepPCB label files are converted to editable `RectShape` overlays when the image is loaded into Image View.
- expected UI count: train 4, validation 2, test 2.
- label-backed count: 2 unique input images with 15 bbox annotations total.
- scope: this split is only for manual UI chain verification; it is not a final statistical train/validation/test split and must not be used as model quality acceptance.

## Evidence Chain

- chain: `cxparser/cxscript/module/cximage/evidence/torch_runtime_ui_evidence_chain.cxsc`
- output_root: `cxscript_runs/evidence/torch_runtime_ui`
- owner: `testing UI torch thread`
- scope: small local UI Evidence Chain validation only
- UI category: declared by each case through `CxEvidenceChain_case_setcategory("Torch Evidence Candidates")`
- UI group: declared by each case through `CxEvidenceChain_case_setgroup(...)`
- UI dataset: declared by `CxEvidenceChain_case_adddatasetimage(...)`, `CxEvidenceChain_case_addbbox_xywh_norm(...)`, and reused with `CxEvidenceChain_case_clone_dataset_from(...)`.

## Cases

| case_id | level | script | expected UI evidence |
|---|---|---|---|
| `torch_segmentation_cpp_state_dict_cpu_artifact_evidence` | T5 | `cxparser/cxscript/module/torch/torch_segmentation_cpp_state_dict_cpu_direct.cxsc` | `torch_result_ref`, `torch_evidence_ref`, `torch_mask_ref`, `torch_overlay_ref` |
| `torch_segmentation_contract_cpu_evidence` | T4 | `cxparser/cxscript/module/torch/torch_segmentation_contract_direct.cxsc` | segmentation contract route status |
| `torch_detection_yolov8_cpu_artifact_evidence` | T6 | `cxparser/cxscript/module/torch/torch_detection_cpu_direct.cxsc` | detection result refs when detector artifacts are produced |
| `torch_detection_contract_cpu_evidence` | T4 | `cxparser/cxscript/module/torch/torch_detection_contract_direct.cxsc` | detection contract route status |
| `torch_train_lifecycle_cpu_evidence` | T3 | `cxparser/cxscript/module/torch/torch_train_lifecycle_direct_test.cxsc` | `global_headless_ok` and `global_torch_ok` for tiny CPU train lifecycle route |
| `torch_resnet18_baseline_feature_evidence` | T5 | `cxparser/rag_script_cases/torch_module/feature/torch_resnet18_baseline_feature.cxsc` | `classifier_output_shape`, `p3_p4_p5_feature_shapes`, `baseline_feature_ref` |
| `torch_resnet18_baseline_infer_evidence` | T5 | `cxparser/rag_script_cases/torch_module/infer/torch_resnet18_baseline_infer.cxsc` | `classifier_output_shape`, `baseline_class_ref` |
| `torch_resnet50_baseline_feature_evidence` | T5 | `cxparser/rag_script_cases/torch_module/feature/torch_resnet50_baseline_feature.cxsc` | `classifier_output_shape`, `p3_p4_p5_feature_shapes`, `baseline_feature_ref` |
| `torch_resnet50_baseline_infer_evidence` | T5 | `cxparser/rag_script_cases/torch_module/infer/torch_resnet50_baseline_infer.cxsc` | `classifier_output_shape`, `baseline_class_ref` |

## Guardrails

- These cases are local Evidence Chain cases; do not write remote code or remote evidence for this package.
- New torch cases must be added in `torch_*.cxsc`; C++ must not hard-code concrete torch case names, model paths, objectives, or OK/NG rules.
- The UI category and group are file-driven by this evidence chain; `ManualConsoleEvidenceChain.cpp` must remain a generic scanner/parser/display layer.
- The training/validation/test rails must be populated from evidence file declarations, not from hard-coded torch case names.
- The segmentation smoke case validates runtime/artifact plumbing, not semantic segmentation accuracy.
- The detection case remains a CPU smoke route until the model/thread resolves the known weights/class compatibility issue.
- The train lifecycle case is a tiny CPU route check only; it is not a full training quality acceptance case.
- The UI pass condition is evidence visibility and artifact binding, not final model quality acceptance.

## Manual Test Steps

1. Open the UI Evidence Chain panel and expand `Torch Evidence Candidates`.
2. Confirm these four rows exist: `torch_resnet18_baseline_feature_evidence`, `torch_resnet18_baseline_infer_evidence`, `torch_resnet50_baseline_feature_evidence`, and `torch_resnet50_baseline_infer_evidence`.
3. Select each ResNet row and confirm its script path, source case, contract, expected metrics, and DeepPCB image binding match the Cases table above.
4. In `Torch Training Image Set`, click `Sync Selected Evidence Case`.
5. Confirm the train, validation, and test rails are no longer empty.
6. Click a training/validation/test thumbnail and confirm Image View loads the DeepPCB image.
7. Confirm `Annotated Regions / Features` reports non-zero rect count and the image overlay shows editable bbox regions.
8. Keep the final state as `PENDING_HUMAN_REVIEW` until a human accepts the dataset split and model task semantics.

## Incremental Training + Inference Closed Loop

| case_id | model | incremental stage | paired inference | current gate |
|---|---|---|---|---|
| `torch_train_lifecycle_cpu_evidence` | DeepLabV3+ | persistent optimizer step, checkpoint and manifest export | mask + overlay validation in the same lifecycle | `PENDING_HUMAN_REVIEW` |
| `torch_prototype_incremental_train_infer_evidence` | PrototypeIndex | merge two evidence descriptors and persist vector tensor | top-1 fused query + overlay in the same TorchTask | `PENDING_HUMAN_REVIEW` |
| `torch_edgesam_incremental_package_evidence` | EdgeSAM-3x | decoder incremental trainer/export package gate | `torch_edgesam_3x_prompt_cpu_evidence` | `PENDING_BINDING` until real encoder/decoder TorchScript weights exist |
| `torch_yolov8_incremental_package_evidence` | YOLOv8 | incremental TorchScript package gate | `torch_detection_yolov8_cpu_artifact_evidence` | `PENDING_BINDING` until real incremental export exists |

The package-gate cases never substitute fixed parameters or placeholder output for a trained model. Missing or unloadable weights must remain `PENDING_BINDING`.

## Manual Closed-Loop Verification

1. Build and launch `<BUILD_DIR>/Release/cxvision_imgui_acceptance.exe` from `<REPO_ROOT>`.
2. Open the Evidence Chain panel and load `torch_runtime_ui_evidence_chain.cxsc`.
3. Expand `Incremental Training + Inference Closed Loop`; confirm the four model rows above are visible.
4. Select `torch_train_lifecycle_cpu_evidence`, sync its dataset, run it once, and verify optimizer-step metrics, model weights, model manifest, inference mask, overlay, and Evidence references all point to the new run directory.
5. Compare the DeepLab result summary with the overlay. Record human accept/reject; do not infer semantic acceptance from finite loss alone.
6. Select `torch_prototype_incremental_train_infer_evidence`, run it once, and verify `incremental_update_executed=true`, `paired_inference_executed=true`, `updated_sample_count=2`, `network_weights_updated=false`, a persisted vector tensor, top-1 score, and `prototype_overlay.png`.
7. Confirm the Prototype overlay is shown in Image View and its top-1 class agrees with the result/evidence JSON before recording human accept/reject.
8. Select `torch_edgesam_incremental_package_evidence`. Before export it must show `PENDING_BINDING`; after a real `encoder.ts` and `decoder.ts` export, rerun the package gate and then the paired prompt inference case.
9. For EdgeSAM, verify the positive point is foreground, the negative point is background, the mask/overlay refs exist, and any Python/C++ consistency tolerance is not exceeded.
10. Select `torch_yolov8_incremental_package_evidence`. Before a real exported weight it must show `PENDING_BINDING`; after export, rerun the gate and then `torch_detection_yolov8_cpu_artifact_evidence`.
11. For YOLOv8, compare candidate boxes, class/score values, Evidence overlay and result summary against the editable DeepPCB annotations.
12. Save a separate human review decision for each model. Keep the overall state `PENDING_HUMAN_REVIEW` unless all required artifacts are from the same run ID and the reviewer explicitly accepts them.


## YOLOv8n-Seg automatic instance segmentation review

Evidence case: `torch_yolov8n_seg_cpu_evidence`.

This case is the primary automatic segmentation path. It requires strict 417/417 state-dict mapping, per-instance masks and bbox geometry, `SegmentationEvidence v2`, original-image edge `MeasurementEvidence v1`, tensor shape trace, weight mapping report, and both segmentation and measurement overlays.

The restored weights are COCO80 pretrained weights. This case validates the C++ binding and evidence closure; it does not claim DeepPCB semantic accuracy. Keep the decision `PENDING_HUMAN_REVIEW` until the reviewer confirms the visible instance boundaries, coordinates, artifacts, and UI result count.

The paired incremental case is `torch_yolov8n_seg_incremental_head_proto_evidence`. It must remain `PENDING_BINDING` until a real polygon or instance-mask dataset is supplied and a candidate package is produced by box/class/DFL/mask/assignment training. Bbox-only DeepPCB annotations are not accepted as segmentation training labels.

Manual steps:

1. Launch `<BUILD_DIR>/Release/cxvision_imgui_acceptance.exe` from `<REPO_ROOT>`.
2. Open the Evidence Chain panel and load `torch_runtime_ui_evidence_chain.cxsc`.
3. Select `torch_yolov8n_seg_cpu_evidence`, sync the dataset, and run once.
4. Verify Image View shows `mask_overlay.png`; switch to `measurement_overlay.png` and confirm refined green source-edge points remain inside the mask boundary search band.
5. Confirm ToolDisplay reports a non-zero result count and the same run directory for `instances.json`, `torch_runtime_evidence.json`, `measurement_evidence.json`, and overlays.
6. Inspect one selected instance: bbox, class/score, mask quality, stability, contour, centroid, pixel area, oriented rectangle axes, rejected points, and uncertainty must refer to the same stable ID.
7. Record human accept/reject for runtime/evidence correctness separately from semantic model quality. Do not promote a profile from this COCO80 review case.
