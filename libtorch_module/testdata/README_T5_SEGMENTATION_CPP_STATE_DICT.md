# T5 Segmentation C++ State Dict Smoke

This smoke case verifies the C++ DeepLabV3Plus segmentation load/forward/artifact path.

## Purpose

- Keep `python_state_dict` blocked from real C++ forward.
- Verify `cpp_state_dict` can load through `DeepLabV3Plus(backbone, num_classes) + torch::load`.
- Verify cxscript `TorchTask` emits mask/result/overlay refs for headless attach checks.

## Versioned Inputs

- Manifest: `libtorch_module/testdata/manifests/deeplab_cpp_state_dict_smoke_v1.json`
- CxScript: `cxparser/cxscript/module/torch/torch_segmentation_cpp_state_dict_cpu_direct.cxsc`
- Input image: `D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/test_images/L1_high_contrast/line_high_contrast_001.jpg`

## Local Generated Weight

The smoke weight is intentionally kept under ignored local model storage:

`libtorch_module/models/deeplab_cpp_state_dict_smoke_v1/weights/deeplab_cpp_state_dict_smoke.pt`

Generate it with the existing VS target:

```powershell
cmake --build <BUILD_DIR> --config Release --target libtorch_module_segmentation_stage_tests

$env:PATH='D:\libtorch\lib;D:\opencv4.9\opencv\build\x64\vc16\bin;C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.1\bin;' + $env:PATH
& '<BUILD_DIR>\_deps\libtorch_module\Release\libtorch_module_segmentation_stage_tests.exe' `
  --mode export-cpp-state-dict `
  --export-path '<REPO_ROOT>\libtorch_module\models\deeplab_cpp_state_dict_smoke_v1\weights\deeplab_cpp_state_dict_smoke.pt'
```

## Headless Validation

```powershell
& '<BUILD_DIR>\Release\cxvision_imgui_acceptance.exe' `
  --headless `
  --cxscript-headless `
  --image 'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\L1_high_contrast\line_high_contrast_001.jpg' `
  --script '<REPO_ROOT>\cxparser\cxscript\module\torch\torch_segmentation_cpp_state_dict_cpu_direct.cxsc' `
  --case-name 'T5_torch_segmentation_cpp_state_dict_cpu' `
  --out '<RUN_ROOT>\headless\T5_torch_segmentation_cpp_state_dict_cpu' `
  --max-steps 10000 `
  --unified-log '<RUN_ROOT>\_shared\cxvision_imgui_acceptance.jsonl'
```

## Expected Outputs

- `torch_segmentation_task_request.json`
- `torch_segmentation_task_result.json`
- `torch_runtime_evidence.json`
- `mask_labels.png`
- `mask_binary.png`
- `mask_overlay.png`
- `contours.json`
- `segmentation_metrics.json`

## Expected Checks

- `cxscript_headless_ok=true`
- `global_torch_ok=1`
- `global_torch_mask_available=1`
- `segmentation_result_ref` is non-empty
- `segmentation_mask_ref` is non-empty and points to `mask_binary.png`
- `segmentation_overlay_ref` is non-empty and points to `mask_overlay.png`

The generated smoke weight is randomly initialized, so it validates runtime mechanics only. It does not validate production segmentation quality.
