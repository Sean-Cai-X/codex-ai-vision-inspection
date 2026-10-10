业务侧集成预览交付 / ellipse-rd-preview-20261010-r1

请先读：这是待接入的研发集成预览包，不是已完成业务端验收的可直接运行安装包。
允许业务侧接入、工具测试和结构化反馈；禁止生产准入、自动激活、替换现用默认模型。
模型来自研发合成数据，不代表使用了业务 test001 的冻结数据。不得伪造业务训练来源。

包含内容
parent/: 原始父 manifest、完整 checkpoint、attestation、lineage、登记摘要。
candidate/: 候选 manifest、完整 checkpoint、真实训练曲线。
runtime/: 本次实际执行并绑定 SHA 的 DLL；尚未携带完整离线依赖闭包。
evidence/: 真实 C ABI 训练/学习率/receipt/推理/取消证据及同样本比较。
diagnostic_summary.json: 七类数量、Train/Valid/VERIFY 角色、失败及候选数量摘要。
MODE_BUDGET_EVIDENCE.txt: 同预算旧新版与 BN 模式对照，不以 loss 下降冒充效果通过。
handoff.lock.json: 此预览包全部文件 SHA；其 SHA 由交接消息单独提供。

业务侧操作顺序
1. 在独立测试环境接收，保留文件原样。用交接消息的 lock SHA 执行：
   powershell -File .\Verify-Handoff.ps1 -Root . -ExpectedLockSha256 sha256:<交接值>
   只证明完整性；校验失败停止导入，不更改摘要以绕过校验。
2. 检查现有 typed bridge / broker 是否支持导入独立研发隔离项目。
   当前尚无完成验收的 broker registry 文件，本包的 delivery_manifest 不是 registry。
   若导入入口或绑定不足，报告阶段和错误码，交回研发补齐；不得手填“已审核”。
3. 接入人员在受控 broker 中建立真实项目、请求、数据集和 runtime pin 绑定。
   不使用 test001 的业务数据摘要来冒充此合成训练的来源，不覆盖原冻结项目。
4. 完成真实注册及依赖检查后，调用训练/推理 GetCapability，核对七类顺序、
   trial-only、runtime SHA、受控模型和点击模式目录。不可用时保留错误，不用替代算法。
5. 使用业务侧自行登记/冻结的图像进行测试，保留 Train/Valid/VERIFY 的原始角色。
   检查真实曲线、学习率、取消、receipt 与单图推理；原图始终只留业务环境。
6. 模型结果一律待人工复核。回传 feedback.template.json 的结构化字段；
   不回传业务原图、路径、mask、overlay、截图或像素数据。

已知失败 / 不得隐瞒
固定类别：arc / circle / ellipse / line / open_curve / polygon / closed_curve。
当前候选 20 epochs / 40 updates / batch 1；每 epoch 两次更新，非通用“全数据一轮”定义。
loss=center_dense_balanced_bce_v2；训练 BN 更新统计量，验证及推理使用 eval。
训练图 Train0 没有 ellipse；Train1 最佳 ellipse IoU 约 .5986。
Valid / VERIFY 最佳 ellipse IoU 约 .9900 / .9889，但有重复、错类别候选。
VERIFY 数量：circle3 / ellipse7 / line2 / polygon2；不是单目标成功。
同类数量超额只是重复警示，不等同于已计算的逐对重叠重复率。
旧版相同 40 updates 也可检出 ellipse，新版不是全面优于旧版。
不得把推理改为 train 来“修复”结论，或降低阈值后宣称原问题解决。

仍未验收
真实 typed broker 注册、GetCapability 与当前候选的端到端加载；
现用业务 runtime 兼容性或干净离线机的依赖闭包；
typed conclusion 的真实桥接回执；Train0/Train1/重复候选修正。
diagnostic_summary 不是伪造的 typed receipt；取消证据只覆盖真实 C ABI 回调取消。

业务侧反馈后，研发补齐接入或执行器问题，并以相同预算继续效果修正。
