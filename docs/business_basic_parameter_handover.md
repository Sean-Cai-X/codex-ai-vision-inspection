# 业务训练/测试基础参数交接

本交接以 `set0000.png` 的基础字段及已确认的 Epoch、模型选择为范围，参数合同见 `contracts/business_trial_parameter_profile.v1.json`。不在下表中的模型开发参数、路径、图片、mask、checkpoint、优化器、阈值与隐藏配置不交给业务侧。

| 区域 | 字段 | 来源/权限 | 示例 |
| --- | --- | --- | --- |
| 数据概览 | 总共、已标记 | 服务/资产代理只读 | 48、8 |
| 数据拆分 | 训练集比例 | 业务可调，提交前校验 | 70% |
| 数据概览 | 训练集数量、验证集数量 | 服务按已标记数据和拆分规则只读返回 | 7、1 |
| 训练参数 | Epoch | 业务可调，形成不可变 trial profile | 120 |
| 训练参数 | 迭代次数 | 业务可调，形成不可变 trial profile | 500 |
| 训练参数 | 批量大小 | 业务可调，形成不可变 trial profile | 2 |
| 训练参数 | 试用模型 | 业务从 `GetCapability.trial_model_ids` 受控选择 | segmentation-small-v1 |
| 训练状态 | 有效学习率 | executor 对当前不可变 trial profile 的只读回执 | 0.001 |
| 标注/测试参数 | 点击模式 | 业务从 `GetCapability.annotation_click_modes` 受控选择；当前 `center`；与冻结数据集的标注语义绑定 | center |
| 训练结果 | 训练/验证分数、Epoch 曲线、训练进度 | 服务只读回执 | 0.938、0.000、100% |
| 测试结果 | 测试进度、typed conclusion、几何摘要 | 服务只读回执 | 100% |

## 运行流程

```text
读取总数/已标记
  → 设置训练集比例、Epoch、迭代次数、批量大小、试用模型、点击模式
  → 服务计算 Train / Valid 数量并校验
  → 资产代理校验点击模式与冻结 dataset revision 的标注语义
  → 固化 business-basic-profile-v1 + dataset revision
  → 提交训练、显示有效学习率与真实训练/验证曲线
  → 选择 center 测试模式并提交 VERIFY 推理
  → 显示测试进度、typed conclusion、几何摘要
  → 人工接受 / 修正 / 拒绝，反馈进入下一增量 trial
```

## 明确边界

- 这些基础参数可用于阶段业务 trial 与流程分析；每次变更必须生成新 profile digest 和 receipt。
- 曲线、训练/验证分数、训练/测试进度必须来自 executor 的 `Poll`，UI 不得伪造。
- 有效学习率是当前模型训练状态和增量 trial 对比依据之一；业务侧只读查看，研发侧调整后必须生成新的 profile digest、曲线和 receipt。
- 点击模式不是页面展示项：它同时绑定标注数据、训练和 VERIFY 人机交互。模式不一致时资产代理必须拒绝提交并返回 `TRAINING_PARAMETER_INVALID`，不得将旧标注静默用于新模式训练。
- executor 未绑定时保留参数与当前操作，显示 `CAPABILITY_UNAVAILABLE` 和无图像问题单。
- 当前模型的缺陷由后续研发 trial 解决，不改变本参数范围、业务运行流程和人工反馈闭环。
