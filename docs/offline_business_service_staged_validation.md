# 七类分割服务：阶段业务测试与升级交接包

## 使用规则

本交接包将“业务流程验证”与“真实七类算法验证”明确隔离。任何 `contract_fixture` 或现有模型的影子分析都不得作为七类模型效果、业务 PASS/FAIL 或训练完成的证据。

## A. 立即可执行：无图像流程验证

业务端按真实操作路径执行，但使用下列无图像 fixture 验证 bridge、页面状态和人工复核：

1. `GetCapability`：验证不可用时训练/推理按钮禁用，恢复说明可见。
2. `SubmitBusinessTraining`：验证不可变 request digest、busy 状态、任务 ID。
3. `PollBusinessTraining`：验证只追加服务曲线点、取消和终态 receipt。
4. `SubmitBusinessInference`：验证单图逻辑引用的提交、typed conclusion 与数值几何摘要展示。
5. 人工接受/修正/拒绝：验证无图像问题单、annotation receipt 和再训练入口。

fixture 仅用于 A 阶段，不调用模型，不包含图片、路径、mask、缩略图、overlay 或像素。

## B. 可并行执行：既有模型影子分析

既有模型只能保留在独立的 `shadow_analysis` 通道，且 receipt 必须标注其原始 capability、模型版本和 `not_business_seven_class=true`。

- 可比对：资产代理链路、运行耗时、稳定性、业务项目选择/ROI/人工复核操作、问题单导出与版本追溯。
- 不可比对：七类类别、几何结论、真实训练曲线、PASS/FAIL、模型候选资格。
- 不得把 detection、smoke、合成数据或现有模型输出映射为 `arc` 至 `closed_curve`。

## C. 项目级真实验证与跨项目七类评估

业务项目按“一个项目组对应一个目标类和一个受控图像文件夹”执行。单项目 trial 只需要其目标类具备：

| 项目目标类 | Train | Valid | VERIFY | 标注 |
| --- | ---: | ---: | ---: | --- |
| arc 或 circle 或 ellipse 或 line 或 open_curve 或 polygon 或 closed_curve | >= 2 | >= 1 | >= 1 | 人工确认的真实 annotation receipt |

单项目不需要凑齐七类，也不得因为本项目只含一个类而把模型 head 降为一类。七类全部齐备仅属于跨项目的全量 ontology 评估，不是某一个项目训练、验证或人工试用的前置条件。

每个受控资产只向业务服务登记 `asset_ref` 与 `image_sha256`。代理内部生成/核验真实 image-mask manifest；业务端和研发问题单均不取得图片路径或像素。

启用条件：

1. 受控资产代理验证 root scope、case/project、hash、真实 mask、split 和 annotation receipt。
2. executor 返回固定七类 capability、runtime SHA256 与 `trial_only=true`。
3. 训练只输出 `development_trials` / `isolated_business_validation`，返回实际 epoch 曲线和候选 artifact receipt。
4. VERIFY 推理只返回 typed conclusion、固定类别和数值几何摘要。
5. 每类至少一次成功、一次输入错误或边界错误，且所有错误均能保留业务操作并提供恢复说明。

## 交接判断

| 状态 | 可交接内容 | 禁止表述 |
| --- | --- | --- |
| A 完成 | 业务流程、页面、bridge、状态机、错误恢复和人工复核可验收 | “模型已训练”或“七类算法已验证” |
| B 完成 | 既有模型与原业务项目的影子操作/稳定性对比 | “既有模型等同七类 segmentation” |
| C 完成 | 七类真实训练、推理、曲线、receipt 和反馈闭环 | “生产发布”或“自动激活” |

## 后续增强

新增数据、模型或 runtime 时，以新的 dataset/model/runtime digest 在同一流程中并行运行。保留旧 receipt；不覆盖旧证据；候选失败则 `ROLLED_BACK`。只有改变 schema、七类定义或错误码含义时才创建新 major capability。
