# YOLOv8n / YOLOv8n-Seg reuse audit

## Decision

The verified simplified detection implementation remains the regression baseline. The restored segmentation state dict remains on its exact `model.0..22` registration path. They share training infrastructure and loss semantics, but their registered model modules are not silently interchanged.

## Module comparison

| Area | Existing `torch_v8` | Strict `torch_yolov8_seg` | Classification |
|---|---|---|---|
| Nano channels | width-scaled 16/32/64/128/256 | fixed 16/32/64/128/256 | numerically same for nano |
| Depths | scaled 1/2/2/1 backbone, 1 PAN | fixed 1/2/2/1 and 1 PAN | same for nano |
| Conv | Conv2d + BN + SiLU | same `ConvModule` | shared implementation |
| Bottleneck | second Conv created with activation disabled | both Conv stages use SiLU | parameterized semantic difference |
| C2f storage | `bottleneck_0...` | Ultralytics-compatible `m.0...` | registration/key difference |
| Backbone registration | `backbone.stem/conv*/c2f*` | `model.0...9` | registration/key difference |
| PAN registration | `neck.c2f_*/down_*` | `model.10...21` | registration/key difference |
| PAN topology | nearest upsample, concat, C2f, downsample | same topology | functionally same intent |
| Detection head | configurable direct box or DFL, class branch | fixed DFL64 + class branch | parameterized difference |
| Segment additions | none | `cv4`, `proto`, mask coefficient output | segmentation-only |
| Executors | detection executor | instance-segmentation executor | intentionally independent |

## Reuse boundary

Reused without changing the accepted detection model:

- `TrainConfig`, `YoloTrainRuntimeConfig`, device selection and SGD options;
- `YoloDatasetPaths`, split layout and loader budget;
- `YOLOv8Loss` box/class/DFL/TaskAlignedAssigner path;
- epoch/batch budget, checkpoint and progress conventions;
- detection regression cases and Evidence presentation.

Segmentation delta:

- polygon label validation and prototype-resolution instance masks;
- head-only freeze policy for `model.22`;
- coefficient/prototype mask reconstruction;
- BCE + Dice mask loss and assigned-mask metrics;
- candidate state-dict export and paired instance inference Evidence.

## Guard

The existing detection modules are not replaced by strict segmentation modules because doing so would change registered keys and Bottleneck activation semantics. The two executors remain independent. No ignored key, guessed rename, or bbox-to-mask fallback is allowed.
