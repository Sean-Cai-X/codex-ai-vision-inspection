# 业务侧人工确认 → 研发侧七类 trial 的反馈闭环

## 职责边界

业务侧拥有原图、ROI 编辑、标注、人工确认和最终业务解释权。研发侧不索取图片；只接收 `visionai.business.annotation_feedback_receipt.v1` 中允许的逻辑 ID、digest、固定类别、数值几何摘要和审查决定。

## 业务侧操作

```text
选择当前项目/案例图片
  → 编辑或确认 ROI 与几何标注
  → 选择固定七类之一
  → 人工 ACCEPTED / CORRECTED / REJECTED
  → 生成不可变 annotation feedback receipt
  → 通过本地受控通道反馈给资产代理
```

业务端不得根据模型猜测类别；`rectangle` 或未知类别必须阻止 receipt 生成并提示更正。

## 资产代理处理

1. 核验 `asset_ref`、`image_sha256`、case/project 绑定、类别 ID/name 配对和 receipt digest。
2. `ACCEPTED`：保留现有真实 mask，标记可参与后续 revision。
3. `CORRECTED`：仅在代理内部依据业务确认的几何创建新的真实 mask；旧 receipt 保留为 superseded，不覆盖历史证据。
4. `REJECTED`：保留审查证据，排除该资产，不生成训练样本。
5. 当七类的 Train/Valid/VERIFY 规则满足时，冻结新的 dataset revision 和 SHA256。

## 研发 executor 处理

1. 只接受已冻结的 revision，不读取业务侧原图路径。
2. 训练回执绑定 dataset revision、annotation receipt digests、parent/candidate/runtime digest。
3. 训练结果仍为 trial-only；不得自动激活、发布或替代人工复核。
4. VERIFY 推理后的人工修正重复进入本闭环，形成下一 revision。

## 反馈验收

- [ ] ACCEPTED receipt 能被代理验证并进入候选 revision。
- [ ] CORRECTED receipt 产生新的 mask digest，旧 receipt 不被删除。
- [ ] REJECTED receipt 不进入训练，但可导出无图像问题单。
- [ ] 错误 hash、跨 case、类别不匹配、rectangle/未知类均被拒绝。
- [ ] 研发侧收到的任何反馈均不含图片、路径、overlay、缩略图或像素。
