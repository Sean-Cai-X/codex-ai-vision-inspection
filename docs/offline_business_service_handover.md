# RND-VAI-001 / RND-VAI-002 阶段业务验证服务交接

## 1. 交接结论与范围

交付对象：Vision-AI 业务层线程、离线运行时研发、测试与案例分析人员。

交付物是可演进的本地离线业务服务流程，而不是正式模型发布服务。当前可用于项目、案例、数据集、训练任务、曲线、候选回执、单图推理、人工复核和无图像问题单的端到端流程验证。

所有输出限定为 `development_trials` 或 `isolated_business_validation`。不得写入正式模型库，不得返回或触发 `APPROVED`、`ACTIVE`、生产发布、detection、smoke 或合成数据 fallback。

训练或推理 capability 未绑定真实七类分割执行器时，必须报告 `CAPABILITY_UNAVAILABLE`。业务端保留当前输入和原图控制权，并显示恢复说明；不得以任意替代算法伪造成功。

## 2. 业务层可依赖的不变契约

| 范畴 | 固定规则 | 后续增强时的规则 |
| --- | --- | --- |
| 训练 capability | `torch.train.segmentation.business_incremental.v1` | 新实现先作为同流程的候选执行器验证；破坏性变更才使用新 major capability。 |
| 推理 capability | `torch.infer.segmentation.business_isolated.v1` | 只替换已登记 candidate artifact，不变更业务请求/回执流程。 |
| 类别 | `0 arc`、`1 circle`、`2 ellipse`、`3 line`、`4 open_curve`、`5 polygon`、`6 closed_curve` | 类别顺序、含义和 ID 不变；未知类别或 `rectangle` 一律结构化失败。 |
| 资产边界 | 仅 `asset_ref + image_sha256` | 资产代理可换实现，但业务请求、日志和 receipt 不得包含路径、像素、图片、mask、overlay 或缩略图。 |
| 可追溯性 | request、dataset、parent、candidate、runtime 和 capability digest | 每一次模型/数据升级新增不可变版本与 receipt；不覆盖旧证据。 |
| 人工复核 | 推理后必须接受、修正或拒绝 | 精度提升不能跳过或自动通过人工复核。 |

业务端以 `dataset_revision + dataset_sha256 + parent_model_digest + candidate_model_digest + runtime_sha256` 识别一次可比较的运行，不以“最新模型”或目录扫描决定版本。

## 3. 业务端交接流程

### A. 能力探测与页面状态

1. 页面加载时调用 `GetCapability`，分别查询训练和推理 capability。
2. 校验 capability ID、版本、DLL/runtime SHA256、七类列表和 `trial_only=true`。
3. 任一 capability 不可用时，训练/推理按钮不可执行；业务端仍可保存项目、标注、数据划分和无图像研发问题单。
4. UI 显示并可提交训练比例、Epoch、迭代次数、批量大小；试用模型与点击模式只能从 `GetCapability` 的受控目录选择。UI 实时显示 executor 返回的有效学习率和曲线。不得显示或编辑 checkpoint、任意模型路径、优化器内部配置、模型规模或生产晋级操作。

验收：不可用状态必须显示 `CAPABILITY_UNAVAILABLE`、failure stage、恢复说明；不调用替代模型。

### B. 数据集与训练提交

1. 用户在当前项目和案例中选择已登记图片，并固化 Train/Valid 划分。
2. 业务核心生成不可变 dataset revision、`dataset_sha256`、ROI/标注 receipt digests 和 `normalized_asset_root_ref`。
3. 业务端提交逻辑引用、digest、固定七类、父模型 digest，以及训练比例、Epoch、迭代次数、批量大小、受控试用模型、受控点击模式。
4. 服务经受控资产代理检查：案例绑定、受控根、真实 image/mask manifest、Train/Valid、标注 receipt、点击模式与冻结 revision 的语义一致性，以及 hash。
5. 提交成功后保存 `training_task_id` 与 request digest，并将页面置为 busy；提交失败时保留用户输入。

验收：缺少真实 mask、Valid 集、标注 receipt、数据 hash 或父模型 artifact 时，显示对应结构化错误，不生成任务成功回执。

### C. 训练状态、曲线与取消

1. 以 `training_task_id` 轮询 `GetBusinessTrainingStatus`。
2. `DEVELOPMENT_TRIAL_READY` 表示验证通过且等待执行；`RUNNING` 必须追加服务实际返回的曲线点和有效学习率。
3. UI 仅绘制服务返回的 `{epoch, training_loss, validation_score, effective_learning_rate}`，并显示当前有效学习率；禁止固定折线、客户端补点或伪造学习率。
4. 用户取消时调用 `CancelBusinessTraining(task_id, reason_code)`；取消只停止后续执行，不删除已产生证据。
5. 终态只允许 `CANDIDATE`、`REVIEWED`、`REJECTED`、`ROLLED_BACK`、`CANCELLED`，且不出现生产激活状态。

验收：训练 receipt 必须绑定 request、dataset、parent、candidate、runtime、capability 和 SHA256。未验证 receipt 显示 `ATTESTATION_PENDING`，不得作为可用候选。

### D. 候选模型与单图隔离推理

1. 仅当存在经验证的 `CANDIDATE` 或 `REVIEWED` receipt 时，用户可选择一张 VERIFY 图片。
2. 业务端以当前项目/案例、`asset_ref`、`image_sha256`、ROI 数值摘要、geometry target、candidate digest 与 dataset revision 提交 `InferenceRequest`。
3. 状态查询期间显示 `QUEUED` / `RUNNING`，不显示预测图像载荷。
4. 完成后读取 receipt 的 `PASS`、`FAIL` 或 `REVIEW` typed conclusion、耗时、模型/数据版本和数值几何摘要。
5. UI 通过本地资产代理加载当前受控图片，再根据数值摘要绘制自身 overlay；服务回执不得携带 overlay 或像素。

验收：prediction 只能使用固定七类的 `class_id/class_name` 配对。推理失败应保留当前图片选择与 ROI 操作，并返回可恢复错误。

### E. 人工复核、案例闭环与问题单

1. 人工对每个 inference receipt 选择接受、修正或拒绝。
2. 修正后的标注产生新 annotation receipt；需要训练时生成新的 dataset revision，而不修改旧版本。
3. 问题单只导出项目/案例逻辑 ID、请求/任务 ID、digest、错误码、failure stage、恢复建议、曲线与数值几何摘要。
4. 问题单、日志、研发材料严禁包含业务图片、路径、缩略图、mask、overlay 或可逆像素信息。

## 4. 当前与后续增强保持一致的验证方式

每个当前阶段案例都必须保留以下无图像基线：请求 digest、数据集 hash、标注 receipt digests、曲线、candidate receipt、推理 receipt、人工复核结果和错误码。

后续模型、数据或运行时升级时，采用并行 candidate 验证：

```text
冻结既有案例基线
  -> 生成新 dataset revision 或 candidate model digest
  -> 使用相同训练提交、状态/曲线、receipt 规则
  -> 使用相同 VERIFY 推理和 typed conclusion 规则
  -> 人工复核新旧 receipt 的版本化、无图像对比证据
  -> 保留新 candidate，或标记 ROLLED_BACK
```

不得通过“最新数据集/模型”自动选取、覆盖旧 receipt、修改类别表或自动 APPROVED/ACTIVE 来完成升级。

## 5. 阶段验收清单

- [ ] capability 返回正确的 ID、版本、SHA256、固定七类、trial-only、可用性、受控试用模型目录与受控点击模式目录。
- [ ] 训练提交与 receipt 固化训练比例、Epoch、迭代次数、批量大小、试用模型、点击模式；任何未发布的模型/模式均返回 `TRAINING_PARAMETER_INVALID`。
- [ ] 训练请求拒绝缺失的 case、受控根、真实 mask、Train/Valid、数据集 hash、标注 receipt、父模型或类别契约。
- [ ] `RUNNING` 返回真实曲线增量；取消后旧证据仍可查询。
- [ ] receipt 可追溯且未验证状态不可被当作候选使用。
- [ ] 单图推理只接收资产逻辑引用和 hash，返回 typed conclusion 与数值几何摘要。
- [ ] 每一个标准错误码均可保留业务操作并生成无图像问题单。
- [ ] 同一案例可在当前与后续模型版本上按完全相同的业务动作、测试脚本和人工复核流程运行。
- [ ] 任何生产发布、自动晋级、detection/smoke 或合成 fallback 都被拒绝。

## 6. 当前接入状态的表述

当前状态应标记为“阶段业务验证服务”。当真实七类 segmentation 执行器、受控资产代理、真实数据 manifest 和已登记模型 artifact 完成绑定后，只将 capability 的 `available` 更新为 `true`；业务端页面、请求格式、测试用例和交接流程不需要重做。
