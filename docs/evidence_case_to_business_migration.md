# Evidence Case 到业务七类基础案例的迁移执行说明

## 已完成的研判

已对 Geometry Evidence Case 合同完成准入评估，结果见 `contracts/evidence_case_business_migration_assessment.v1.json`。

现有 arc、circle、ellipse、line、open_curve、polygon 案例只能作为 T0 回归/流程候选；它们的源合同明确声明 `training_enabled=0` 和 `production_ground_truth=false`。`rectangle_closed_region` 因不属于业务七类被拒绝；`closed_curve` 没有对应源案例。

## 迁移执行顺序

1. 业务资产代理在业务受控根内登记每个候选的真实替换资产，产生 `asset_ref` 与 `image_sha256`。不得把研发目录路径交给业务端或写入 receipt。
2. 业务人员针对每个资产确认类别。不得依据 case 文件夹或几何相似性把 rectangle 变成 polygon/closed_curve。
3. 人工确认 ROI/标注，形成业务 annotation receipt。该 receipt 与 case、project、asset hash 绑定。
4. 代理内部由原始确认标注形成并校验真实 segmentation mask；不得使用 Evidence overlay、派生可视图或旧推理结果作为 mask。
5. 为每类建立至少一个 Train、一个 Valid、一个 VERIFY 资产，固化 immutable dataset revision 与 SHA256。
6. 资产代理对 scope、hash、mask、split、annotation receipt 和 revision 通过后，才允许 `SubmitBusinessTraining`。
7. 将原 Evidence Case 保留为 T0 回归证据；真实业务资产作为 T1。T0 不进入模型质量、生产准确率或晋级结论。

## 交接条件

完成步骤 1–5 后，研发可绑定七类 executor 并开始 C 阶段真实 trial。完成步骤 6–7 的成功与失败用例后，交接业务层的真实七类训练/推理反馈闭环。
