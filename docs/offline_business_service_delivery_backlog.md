# 离线业务服务：交接与后续追进提案

## 已可交接的实现

- RND-VAI-001/002 typed service contract、固定七类、错误码、trial-only receipt。
- Vision-AI `OfflineBusinessServiceBridge`：`GetCapability / Submit / Poll / Cancel / Receipt` 的 transport 边界与测试。
- 业务人工确认→无图像 feedback receipt→资产代理→新 dataset revision 的闭环合同。
- Evidence Case 的 T0 迁移评估、真实七类资产登记模板和 Devuan 编译环境待人工安装计划。

## 业务侧下一次实现：独立业务服务操作面板

Owner：Vision-AI 业务端。依赖：已绑定的 `IServiceTransport`。

| 面板 | 固定操作 | 未绑定状态 | 验收 |
| --- | --- | --- | --- |
| Business training | 选择已冻结 revision，提交、查看真实曲线、取消、读取 candidate receipt | 禁用提交；显示 `CAPABILITY_UNAVAILABLE` 和无图像问题单 | 不调用旧 YOLO/provider；曲线只来自 Poll。 |
| Incremental trial | 选择父 candidate 与业务确认后的新 revision，提交并比较 receipts | 保留输入与恢复建议 | 新 profile/model/revision 均可追溯，失败可 `ROLLED_BACK`。 |
| VERIFY inference & review | 选择受控 VERIFY asset，提交、读取 typed conclusion/几何摘要，接受/修正/拒绝 | 禁用提交；可保存人工确认和问题单 | 不接收服务图像/overlay；仅本地资产代理绘制。 |

业务 UI 必须能编辑并提交训练比例、Epoch、迭代次数、批量大小，并从 `GetCapability` 选择受控试用模型和点击模式；必须可视化有效学习率、参数摘要、版本和真实曲线。不得编辑 learning rate、checkpoint 或任意模型路径/模型规模。

## 研发侧后续追进

| 提案 | Owner | 完成条件 |
| --- | --- | --- |
| 受控资产代理 | R&D + 业务资产负责人 | 内部登记路径，外部仅 `asset_ref + image_sha256`；验证 scope/hash/case/mask/split/receipt。 |
| 七类 executor | R&D | 真实 Train/Valid mask 训练、真实曲线、candidate artifact、trial-only receipt。 |
| IPC/DLL transport | R&D | 实现 Vision-AI `IServiceTransport`；不暴露图片、路径、旧 detection/smoke 能力。 |
| trial profile / capability catalogue | R&D | 发布受控 trial_model_ids、annotation_click_modes；业务提交六项基础参数后生成不可变 profile digest、参数摘要与 receipt。 |
| 真实资产迁移 | 业务资产负责人 | 七类均具备真实 Train/Valid/VERIFY、人工确认 receipt 与冻结 revision。 |
| 跨平台构建 | R&D | Devuan/Win11 各自构建并对同一 artifact 做加载/typed receipt 一致性验收。 |

## 交接状态

当前状态：`FLOW_AND_BRIDGE_READY / REAL_EXECUTOR_PENDING`。

业务层可立即验收接口、页面状态、不可用恢复、问题单与人工反馈闭环。真实训练、增量训练、曲线、模型 receipt 和 typed inference 在 executor/asset broker/真实资产绑定后启用；不改业务流程、测试脚本或人工复核规则。
