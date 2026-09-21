# YOLOv8n-Seg M0 Asset Preflight

Date: 2026-08-18
Repo: D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo

## Scope and safety

- Read-only asset analysis only.
- The existing YOLOv8 detection executor was not invoked.
- Neither .pt file was deserialized.
- No unknown or missing key was ignored.
- No model source, manifest, build configuration, or weight file was modified.
- The accidental gateway code-format route remained dry-run only; no apply call was made.

## Confirmed assets

- libtorch_module/models/yolov8n-seg_dict.pt: exists.
- libtorch_module/models/yolov8n-seg.pt: exists.
- YOLOv8 detection and pose weights: exist.
- DeepLab model directories: exist.
- EdgeSAM package currently contains model_manifest.json only; encoder.ts and decoder.ts are absent.

## Confirmed incompatibility with current C++ detection path

1. torch_yolo_head.h defines only box and class branches.
2. torch_v8.h builds YOLOv8Detect and YOLOv8Loss.
3. torch_runtime_detection_executor.cpp normalizes output around 4 + num_classes.
4. There is no YOLOv8 segmentation task ID, segment head, proto branch, mask coefficient branch, instance-mask decoder, or segmentation manifest.
5. YOLOv8Impl::load_weights uses torch::pickle_load.
6. Unmapped keys are warning-only.
7. Training may catch pretrained-weight load failure and continue from scratch.

Consequently, yolov8n-seg_dict.pt and yolov8n-seg.pt are forbidden inputs to the current detection model and executor.

## Safe inspector

Tool: tools/yolov8_seg_asset_preflight.py

Behavior:

- Computes SHA256 and size by streaming bytes.
- Reads ZIP member names without unpickling.
- Never unpickles the full checkpoint.
- Loads only the *_dict.pt asset with torch.load(weights_only=True).
- Requires a flat tensor-only state dict.
- Emits every tensor key, shape, dtype and numel.
- Emits candidate model.22/cv4/proto tensors.
- Performs no key remapping and ignores no key.

Intended invocation:

python tools/yolov8_seg_asset_preflight.py \
  --state-dict libtorch_module/models/yolov8n-seg_dict.pt \
  --checkpoint libtorch_module/models/yolov8n-seg.pt \
  --out cxscript_runs/model_preflight/yolov8n_seg_m0/preflight.json

## Remaining M0 gates

- Safe inspector has not run because codex-lan-agent does not allow arbitrary local Python execution.
- The previously recorded system Python lacks torch, so tensor inventory may additionally return BLOCKED_ENV.
- File SHA256, sizes, checkpoint archive type, Ultralytics version, tensor keys/shapes, nc, nm, npr, letterbox metadata and source relationship remain unverified.
- No M1 or M2 implementation is authorized until these fields are verified and a one-to-one module/tensor mapping is reviewed.

## Conclusion

- Asset existence: CONFIRMED
- Existing detection executor compatibility: REJECTED
- M0 model format: PENDING_PREFLIGHT
- M0 execution: BLOCKED_ENV
- YOLOv8n-Seg C++ inference: PENDING_BINDING
- Compile: COMPILE_NOT_RUN
- Overall: NOT ACCEPTED
