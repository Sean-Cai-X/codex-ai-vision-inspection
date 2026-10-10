# 真实七类 executor 与资产代理：绑定收件清单

此清单用于把下一阶段从“迁移候选”推进到真实 trial；它不要求业务方把图片交给研发，也不允许图片离开受控环境。

## 业务资产代理负责人提供

1. 在受控本机登记 `normalized_asset_root_ref`，并将实际路径保留在代理内部。
2. 按 `business_asset_broker_registration_template.v1.json` 登记七类真实资产；每类单项目至少 2 Train、1 Valid、1 未标注 VERIFY。
3. 每条资产生成 `asset_ref` 和原图 SHA256；仅 Train/Valid 要求人工确认 annotation receipt digest 与监督 mask 绑定。未标注 VERIFY 不生成训练 mask。
4. 冻结 manifest 后生成 dataset revision 和 dataset SHA256。
5. 只向研发服务 bridge 提供逻辑 ID/digest；不提供 source path、图片、mask、overlay 或缩略图。

## 研发 executor 负责人提供

1. 以固定七类顺序加载已登记的真实 dataset revision。
2. 导出训练 capability、runtime SHA256 和 trial-only 标记。
3. 输出真实曲线、候选 artifact digest 和可验证 training receipt。
4. 对 VERIFY asset_ref 执行隔离推理，并只输出 typed conclusion 与数值几何摘要。
5. 将 executor/asset broker 实现为 Vision-AI bridge 的 `IServiceTransport`。

## 绑定前检查

- 七类均齐全；无 rectangle 和未知类。
- Train/Valid 记录具备原图与人工标注转换出的监督 mask 配对，不使用 overlay 或预测结果替代人工监督。VERIFY 单独绑定原图，不要求 mask。研发合成数据只验证流程，不作为业务图集或独立效果证据。
- Train、Valid 均非空；VERIFY 不混入训练。
- capability 不产生 APPROVED、ACTIVE 或生产发布状态。
- 提供一个成功训练、一个取消、一个 hash 失败、一个类别不匹配、一个单图 REVIEW 的无图像 receipt。

## 可宣布交接的条件

上述登记完成、broker 校验通过、executor 真实运行并由 bridge 完成一次训练和一次 VERIFY 推理后，可向业务层宣布“真实七类阶段 trial 服务已绑定”。在此之前只能宣布流程与接口就绪。
