# Next3 Measurement Semantics Stage Review

## 定性

当前任务定性为“计量行为的图像语义模型化”阶段扩展。

它不是把 cxvision 改造成独立计量系统，也不是移植外部 GPL 项目源码；本阶段只把现有 cxvision 工具运行时事实，整理成可被语义、模式识别、证据链和人工复核使用的测量语义侧车资产。

## S0 / S1 已落地范围

Headless case 输出目录会在原有资产旁边新增以下 sidecar：

- `measurement_semantic_input.json`
- `calibration_snapshot.json`
- `coordinate_transform_trace.json`
- `measurement_behavior_trace.json`
- `measurement_observations.json`
- `measurement_relations.json`
- `measurement_feature_vector.json`
- `semantic_pattern_result.json`
- `accuracy_evaluation.json`
- `uncertainty_budget.json`
- `algorithm_provenance.json`
- `measurement_semantic_contract_result.json`

这些文件只读取既有 runtime capture / shape snapshot / overlay rendering facts，不替代 Findline、Findcircle、Findellipse、FindRect、FastMatch 的原始结果。

## 人工验证点

每个抽样 case 请确认：

1. 原有 `result_summary.json`、`tool_display.png`、`result_overlay.png` 仍然存在。
2. 12 个 measurement semantic sidecar 均存在。
3. `measurement_semantic_input.json` 的 image、target、tool、script 与当前 case 一致。
4. `measurement_observations.json` 中 valid points、fit、avgdist 等指标与 `result_summary.json` 不矛盾。
5. `measurement_relations.json` 能看到 ROI / scan / result 等 shape role 的关系摘要。
6. `calibration_snapshot.json` 当前应明确为 `CALIBRATION_NOT_BOUND`，不得伪装已有标定。
7. `semantic_pattern_result.json` 当前应为 `PENDING_MODEL_BINDING`，不得伪装模型识别通过。
8. `measurement_semantic_contract_result.json` 只表示 sidecar 已生成并需要人工复核，不表示算法 contract 通过。

## 下一阶段边界

S2 才允许接入 `CxCalibration.h` 的独立标定验证。

接入前必须保持：

- 不修改 Parser 执行边界。
- 不引入 Parser 多线程。
- 不修改现有 Find* 算法主链。
- 不把 sidecar 结果写回正式参数或正式 contract。
- 不把 pending 的模型、标定、精度评估伪装成 PASS。

