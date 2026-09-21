# 模型层、调优层与稀疏分叉增量闭环

## 1. 目标

本流程面向交付后的真实使用：用户从任意历史发布模型出发，使用少量新增现场
样本生成一个或多个候选增量模型，经新场景收益、历史能力保持、部署预算和人工
审核后，在指定分支内晋升。

当前受控几何图例不是交付案例，也不定义未来模型类别。它们只验证数据转换、
C++ 训练、checkpoint、动态曲线、推理比较、几何关联和人工证据链是否可运行。

## 2. 核心模型

模型版本不是线性序列，也不存在全局 `latest`。每个发布模型和候选模型都是不可变
节点，节点之间通过 `parent_model_ids` 构成有向无环图。

```text
                              branch_line_a / candidate_01
                            /
release_root / release_01 -- branch_product_b / candidate_01 -- candidate_02
                            \
                              branch_obb_migration / candidate_01
```

普通增量候选只能有一个父模型。多个父模型的组合必须显式创建 `merge_candidate`，
重新训练并分别执行所有父分支回归；禁止隐式权重平均。

## 3. 不可变模型节点

每个节点由 `model_node.json` 定义，至少包含：

```text
model_id                 稳定内部标识
display_name             界面可点击名称
node_kind                release/incremental/architecture/merge
root_release_id          所属发布根
branch_id                所属分支
parent_model_ids         父模型列表
task                     detection/obb/segmentation/...
architecture_id          结构定义标识
capability_manifest_ref  AABB/OBB/angle/DFL/export 等能力
model_manifest_ref       现有 cxvision.torch_model_manifest
checkpoint_ref/hash      权重与内容 Hash
class_ontology_ref       类别与兼容映射
dataset_binding_ref      本轮数据绑定
training_plan_ref        训练计划
metric_bundle_ref        训练与评估事实
promotion_status         分支内状态
```

模型节点一旦形成不得覆盖。重训、调参、换数据或改结构都会产生新节点。

## 4. 增量类型

### 4.1 data_increment

结构、类别和能力不变，只增加经人工确认的新样本。允许完整 checkpoint 热启动。

### 4.2 domain_increment

相机、光照、材质、分辨率、背景或产线变化。必须保留父域 replay，重点评估域间
遗忘和误检迁移。

### 4.3 class_increment

新增类别或类别语义变化。必须建立 ontology 映射，检测头不兼容部分重新初始化，
并对所有旧类别执行 per-class retention。

### 4.4 geometry_capability_increment

AABB 迁移到 OBB、mask 或 keypoint。它是能力分支，不是普通续训。必须显式记录
角度表示、输出契约和导出能力，不能用 AABB 指标代替角度指标。

### 4.5 architecture_increment

修改 Backbone、Neck、Head 或模型家族。只继承形状和语义兼容的参数；未匹配层
随机初始化的事实必须写入 transfer report。

## 5. 运行资产目录

目录由运行资产驱动，不在 C++ 中维护分支表、案例表或固定运行 ID。

```text
<RUN_ROOT>/model_incremental/<branch_folder>/<RUN_ID>/
  model_node.json
  parent_binding.json
  capability_manifest.json
  class_ontology_binding.json
  dataset_binding.json
  training_plan.json
  training_trace.json
  learning_curve.csv
  validation_curve.csv
  metric_bundle.json
  base_vs_candidate.json
  forgetting_report.json
  deployment_budget.json
  promotion_gate.json
  human_review.json
  model_package/
    model_manifest.json
    weights/
```

界面名称优先取 manifest 的 `display_name` 或 `review_item`，内部使用 `model_id`、
`branch_id` 和规范化路径去重。

## 6. 稀疏候选增量流程

### G0 选择父模型

操作员从模型谱系中选择历史发布节点或已晋升分支节点。系统验证 checkpoint、Hash、
task、capability、ontology 和运行时可用性。

结论：`PARENT_MODEL_RESOLVED` 或 `PARENT_MODEL_BLOCKED`。

### G1 新增资产接入

递归扫描用户提供的现场目录，验证图片、标注、来源、Hash、split 和显示名称。
缺失资产只记录 `ASSET_MISSING`，不使用其他案例 fallback。

### G2 父模型预推理与人工修正

父模型对新增样本生成候选标注。预标注不是 ground truth，必须由人工确认、修正或
拒绝后才能进入 incremental dataset。

### G3 数据角色绑定

一次训练必须绑定五类数据：

```text
incremental_train
incremental_validation
parent_replay
frozen_release_validation
negative_and_hard_samples
```

稀疏新样本不得单独训练。replay 可按类别、域、难度和历史错误分层抽样，但抽样
策略及 Hash 必须保存。

### G4 候选计划生成

从同一个父节点生成多个独立候选计划。候选之间只改变一个变量族：

```text
optimizer/scheduler
augmentation
freeze policy
loss configuration
architecture delta
```

结构、数据和优化器不得在同一候选中同时无约束变化。

### G5 C++ 训练

每个候选使用独立输出目录。逐 Epoch 写出 train/validation 曲线、LR、box/cls/DFL/
angle/total loss、正样本数和 batch 波动。父 checkpoint 保持只读。

### G6 多集合评估

父模型与候选模型必须使用同一指标 contract，分别运行：

1. 新增场景验证集：判断增量收益。
2. parent replay：判断历史能力保持。
3. frozen release validation：判断发布级回归。
4. negative/hard samples：判断误检扩散。
5. 能力专项集：例如 OBB 角度、尺度、开放边界或 mask。

### G7 计算增量事实

至少输出：

```text
new_domain_gain = candidate_on_new - parent_on_new
parent_retention_delta = candidate_on_parent - parent_on_parent
forgetting_delta = max(0, -parent_retention_delta)
worst_class_regression
false_positive_delta
geometry_or_angle_delta
latency_delta
model_size_delta
```

阈值来自 promotion policy 资产，不写死在 C++。训练 loss 下降不能单独形成晋升结论。

### G8 自动门禁

系统只计算事实并给出 `GATE_READY` 或 `GATE_FAILED`。任一强制资产缺失、Hash 不符、
回归超限或部署预算超限，候选不能进入人工接受状态。

### G9 人工审核与分支晋升

人工在界面中查看父/候选 overlay、曲线、指标、遗忘报告和部署预算，保存：

```text
HUMAN_ACCEPTED
HUMAN_REJECTED
PENDING_HUMAN_REVIEW
```

人工接受后才允许在当前 `branch_id` 中创建新的 release 节点。晋升不改变其他分支，
也不覆盖父节点。

## 7. 模型层与调优层的职责

模型层负责：结构、能力、权重加载、兼容迁移、推理和导出。

调优层负责：训练计划、候选搜索、曲线、指标、诊断假设和候选排序。

调优图样只能形成待验证假设。例如 loss 锯齿首先检查 LR、scheduler、batch 和损失
统计口径；只有固定结构的超参对照仍证明容量瓶颈时，才建立结构候选。

禁止把“图样→结构处方”当成自动确定性映射。

## 8. 分支合并

分支默认不合并。确需合并时：

1. 建立 `merge_candidate`，列出全部父模型。
2. 显式映射类别 ontology 和 capability。
3. 绑定每个父分支的 replay 与 frozen validation。
4. 使用联合数据重新训练，不隐式平均权重。
5. 分别通过每个父分支门禁。
6. 保存独立人工 merge 决定。

## 9. GUI 投影

模型增量界面按以下视图组织：

```text
Lineage       父节点、分支和候选 DAG
Dataset       新增、replay、frozen、negative 数据角色
Training      计划、动态 Epoch、loss 分量、checkpoint
Evaluation    新域收益、历史保持、类别回归、部署预算
Comparison    parent vs candidate 推理证据
Human Review  Accept/Reject/保持 Pending
```

不得只显示一条全局训练曲线或一个 latest 模型路径。

## 10. 当前项目推进顺序

### P0 基础设施 smoke（当前）

保留当前 YOLOv8-n AABB 受控案例，只用于验证真实 C++ 训练、动态曲线、模型保存、
base/candidate 推理和人工证据链。不得作为交付精度或 OBB 结论。

### P1 模型谱系资产化

为当前 base 和 incremental 输出补齐 `model_node.json`、父模型、branch、checkpoint
Hash、dataset binding 和 promotion status。GUI 从目录扫描谱系。

### P2 防遗忘评估

补齐 replay/frozen/negative 数据角色及 new-domain gain、retention、forgetting、
per-class regression。当前受控数据可以验证计算链，但阈值不代表交付阈值。

### P3 多候选分支

从同一父节点创建至少两个单变量训练计划，验证候选并存、互不覆盖、独立比较、
人工只晋升一个或全部拒绝。

### P4 OBB 能力适配

将 YOLOX+OBB、MMRotate 或 PP-YOLOE-R 作为 capability branch 接入同一节点与门禁
contract。AABB 与 OBB 不共享角度指标结论。

### P5 交付现场验证

使用真实用户资产重新建立 dataset、policy 和 frozen validation。当前受控图例不进入
交付案例列表，只保留为开发回归资产。

## 11. 三项强制人工验证门禁

以下三项不是界面提示，而是每个候选进入 `HUMAN_ACCEPTED` 前必须保存的人工审核
记录。缺少任一记录时，`promotion_allowed` 必须保持 false。

### 11.1 衍生基础模型类型选择

#### 目标

面对新的案例图像，操作员必须能明确知道“有哪些历史模型可选、为什么选择当前父
模型、它能输出什么、不能输出什么”。模型选择依据来自 capability、ontology 和
运行资产，不得根据案例文件夹名称在 C++ 中路由。

#### 界面必须明示

```text
模型显示名称 / model_id / 所属分支 / 发布状态
任务类型：AABB、OBB、segmentation、keypoint 等
输出契约：bbox、angle、mask、polyline 等
类别 ontology 版本及可识别类别
输入尺寸、颜色、归一化、resize/letterbox
运行后端、设备、导出格式和部署兼容性
父模型在当前新增图像上的预推理结果
选择原因和人工决定
```

#### 人工步骤

1. 加载一组新增图像，不先绑定固定模型。
2. 界面只列出资产完整、运行时可用且 capability 可解释的历史模型。
3. 操作员查看模型任务、类别、输入和输出契约。
4. 对必要的 2～3 个候选父模型执行同图预推理。
5. 对比 overlay、类别、置信度、几何输出和失败原因。
6. 人工选择一个父模型，并保存 `parent_model_selection_review.json`。

#### 阻断条件

- 新案例要求 OBB，但候选父模型只有 AABB。
- ontology 无映射或目标类别不存在。
- 输入或部署 runtime 不兼容。
- 模型 manifest、checkpoint 或 Hash 缺失。
- 只能通过内部 ID 判断模型用途，界面没有可理解名称。
- 系统根据案例名称自动选择并直接训练。

### 11.2 增量分支可靠性与冻结机制

#### 目标

冻结不能只表现为配置项。必须证明被冻结参数没有更新、可训练参数确实得到梯度和
更新，并证明增量后没有不可接受的历史能力遗忘。

#### 冻结策略资产

每个候选必须有 `freeze_policy.json`，至少定义：

```text
freeze_policy_id
parent_model_id
frozen_parameter_groups
trainable_parameter_groups
group_match_rules
expected_transfer_mode
unmatched_parameter_policy
unfreeze_schedule
```

参数组使用稳定模块语义，例如 `backbone`、`neck`、`box_head`、`class_head`、
`angle_head`，不得依赖一次运行中的指针或无记录序号。

#### 自动事实检查

训练前后必须输出：

```text
checkpoint_transfer_report.json
parameter_hash_before.json
parameter_hash_after.json
gradient_by_group.json
parameter_update_by_group.json
replay_retention_report.json
```

冻结组的验收事实：

- 参数成功从父 checkpoint 加载。
- `requires_grad=false` 或等效优化器排除事实存在。
- 每轮 `grad_defined=false` 或 grad norm 为零。
- update norm 为零。
- 训练前后参数 Hash/逐张量数值一致。

可训练组的验收事实：

- 应训练参数至少一个有效梯度。
- update norm 非零且全部为有限值。
- 未匹配的新层被明确列出，不能静默忽略。
- 新域指标有可复核变化。

冻结机制通过仍不等于候选有效。候选还必须通过 parent replay、frozen release、
per-class regression 和误检门禁。

#### 人工步骤

1. 查看父 checkpoint transfer report。
2. 核对冻结组和训练组是否符合本轮增量目标。
3. 核对每组 grad/update/hash 事实。
4. 查看新增场景收益与历史保持率。
5. 决定冻结策略有效、需要重训或拒绝候选。
6. 保存 `freeze_policy_review.json`。

#### 阻断条件

- 配置称已冻结，但参数 Hash 或 update norm 发生变化。
- 所有参数都没有梯度，形成无效训练。
- 新 head 未加载且未记录随机初始化事实。
- 只在新增稀疏数据上训练，没有 replay。
- 新域提升但历史类别退化超出 policy。

### 11.3 推理差异解决路径

#### 目标

父模型和候选模型在同一图像上出现差异时，必须先定位差异发生阶段，再选择解决
方式。禁止仅通过调低阈值、隐藏框或换 overlay 掩盖模型问题。

#### 差异定位顺序

```text
D0 资产：是否同一物理输入、同一标注和 Hash
D1 输入：颜色、尺寸、归一化、resize/letterbox 是否一致
D2 模型：task、ontology、capability、checkpoint 是否正确
D3 原始输出：logits、box、angle、mask 是否存在差异
D4 坐标：缩放、padding、旋转角和原图回映射是否一致
D5 后处理：confidence、NMS、IoU、max detections 是否一致
D6 指标：匹配规则、类别规则、角度周期和 mask 阈值是否一致
D7 质量：确认是新增能力、历史回归、误检扩散或未解决差异
```

必须先完成 D0～D6 的契约排查，才能把差异归因于模型训练或结构。

#### 差异分类

```text
EXPECTED_NEW_DOMAIN_GAIN
PARENT_CAPABILITY_RETAINED
HISTORICAL_REGRESSION
FALSE_POSITIVE_EXPANSION
CLASS_ONTOLOGY_MISMATCH
PREPROCESS_MISMATCH
COORDINATE_OR_GEOMETRY_MISMATCH
POSTPROCESS_OR_THRESHOLD_MISMATCH
MODEL_CAPABILITY_MISMATCH
UNRESOLVED_DIFFERENCE
```

#### 解决方式

- 输入契约错误：修复 preprocess contract，原候选重新评估。
- 坐标错误：修复坐标/letterbox/angle contract，不用训练掩盖。
- 阈值问题：在 frozen calibration set 上校准，并重新跑父域和新域。
- ontology 问题：修复类别映射，禁止按名称猜测。
- 数据/标注问题：修正资产并产生新 dataset version。
- 确认模型能力不足：建立新的 incremental 或 capability candidate。
- 历史回归或误检无法消除：拒绝候选。
- 业务上接受的差异：必须记录理由、影响范围和剩余风险。

#### 界面必须提供

```text
同图 parent/candidate 并排 overlay
原始输出与后处理结果切换
类别、置信度、bbox/angle/mask 数值差异
preprocess/coordinate/postprocess contract 对照
差异分类菜单
root cause stage
resolution kind
证据引用和剩余风险
```

人工保存 `inference_difference_review.json` 后，该差异才能进入已解决或明确接受
状态。`UNRESOLVED_DIFFERENCE` 必须阻断晋升。

## 12. 当前人工研判点

1. 是否接受“模型版本为 DAG、无全局 latest”的原则。
2. 是否接受普通候选单父节点、合并必须显式重训。
3. 五类数据角色是否满足现场数据管理方式。
4. 是否允许分支独立晋升并长期并存。
5. 是否确认父模型类型选择、冻结执行和推理差异为三项强制人工门禁。
6. forgetting、类别退化、误检和未解决推理差异是否作为硬阻断项。
7. OBB 迁移是否作为 capability branch，并在 GUI 中按
   Lineage/Dataset/Training/Evaluation/Comparison/Review 投影。

本文件先定义流程和资产 contract，不代表已完成对应 C++ 扫描器、谱系界面或晋升
执行器。任何自动晋升仍保持禁止。

## 13. 2026-09-04 发布就绪收口清单

### 13.1 当前已形成的事实

| 项目 | 当前结论 | 可复核证据 | 发布含义 |
|---|---|---|---|
| 父 checkpoint 加载与冻结入口 | 已接入真实 checkpoint、`requires_grad` 和优化器参数过滤 | `checkpoint_transfer_report.json`、`freeze_execution_audit.json` | 仅证明执行入口存在；尚缺训练前后参数 Hash、梯度和 update norm，不能宣称冻结验收通过 |
| 圆形旋转案例差异定位 | 父/候选差异首先出现在 raw confidence/threshold 阶段，NMS 继续收敛；该案例未显示坐标回映射差异 | `raw_output_manifest.json`、`threshold_candidates.json`、`postprocess_trace.json` | 可支持 calibration 候选假设，不足以直接晋升 |
| 同源图像增强 | 可用于模糊、噪声、旋转、位移等受控鲁棒性回归 | 原图/生成图 Hash、seed、operation order、affine 和变换后 target | 不能替代 source-disjoint 发布验证 |
| source-disjoint 验证 | 当前各几何类别缺少不同 source-image 和显式 `source_split` | `SOURCE_SPLIT_DECLARATION_REQUIRED` | 发布门禁保持阻断 |
| 场景路由、CanonicalBoundaryRecord、钳位一致性 | 契约已定义，运行编排尚未接通 | `contracts/model_scene_router_contract.v1.json`、`contracts/canonical_boundary_record_contract.v1.json`、`contracts/clamp_consistency_contract.v1.json` | `CONTRACT_READY_RUNTIME_PENDING` |
| GUI 快捷键 | F2/F4/F5/F7-F12 已有有效落点；F1 Location、F6 Annotation Tools 焦点仍有缺口 | GUI 截图、Location 回执、顶层焦点记录 | `MANUAL_GUI_PARTIAL`，不得最终验收 |

### 13.2 GUI 操作与结论数据映射

| 操作面板 | 操作按钮或入口 | 必须展示/生成的数据 | 允许结论 |
|---|---|---|---|
| F9 `Torch Training Image Set` | `Verify Assets` | 图片/标注 Hash、`source_split`、train/validation/replay/frozen/negative 角色及拒绝原因 | `ASSET_PREFLIGHT_PASS/FAIL`、`SOURCE_SPLIT_DECLARATION_REQUIRED` |
| F9 `Lineage / Training` | `Verify Parent`、`Verify Freeze` | 父节点、checkpoint Hash、transfer report、冻结/训练参数组、grad/update/hash 审计 | `PARENT_MODEL_RESOLVED/BLOCKED`、`FREEZE_EXECUTION_VERIFIED/PENDING` |
| F9 `Training` | `Prepare Feature Variant` | source/generated Hash、操作、seed、target 变换、同源标记 | `CONTROLLED_REGRESSION_READY`；不得显示 release-ready |
| F9 `Training` | `Train Candidate` | 单一 changed family、参数 delta、学习率/损失曲线、候选 checkpoint | `TRAINING_EXECUTION_COMPLETE/FAIL` |
| F9/F10 `Evaluation / Parameter Tuning Map` | `Compare Parent / Candidate` | raw-output、threshold、NMS、坐标、CanonicalBoundary、clamp 六阶段差异和未改变参数审计 | `ANALYSIS_EXECUTION_COMPLETE` 或明确 failure stage |
| F9 `Routing` | `Route Eval` | closure、linearity、curvature、circle/ellipse fit、corner/convexity、候选分支和置信度 | `ROUTE_SELECTED/AMBIGUOUS/FALLBACK/BLOCKED` |
| F9 `Geometry` | `Build Canonical`、`Clamp Eval` | mask→boundary 记录、拓扑、几何类别、closure gap、钳位前后参数、逆向干预次数、一致性分数 | `CANONICAL_BOUNDARY_READY/REJECTED`、`CLAMP_CONSISTENCY_PASS/FAIL` |
| F8 `Torch Runtime / Evidence` | stage 切换 | 同图 parent/candidate 的 raw、threshold、NMS、坐标、canonical、pre/post-clamp overlay 和事实 | 只能给执行或差异分类结论 |
| F4/F5 `Manual Review / Evidence` | `Accept`、`Reject`、保持 Pending | 全部证据引用、差异分类、风险、人工身份/时间、决定 | `HUMAN_ACCEPTED/REJECTED/PENDING` |
| F9 `Lineage` | `Promote`、`Rollback` | source-disjoint、clamp consistency、冻结审计、人工决定、不可变新节点或回滚目标 | `PROMOTION_ELIGIBLE/BLOCKED`；禁止自动晋升 |

### 13.3 后续主力推进顺序

1. 为开放/封闭每类几何补充不同 source-image 的原始验证资产并声明
   `source_split`，先通过 source-disjoint 门禁。
2. 从同一人工确认的 release 父节点建立只改 `calibration` 的候选；checkpoint、
   数据、冻结策略、训练参数保持不变并生成 unchanged audit。
3. 串行执行 raw-output → threshold → NMS → coordinate →
   CanonicalBoundaryRecord → precision clamp，输出逐阶段 parent/candidate 对照。
4. 用钳位一致性作为几何主门禁，同时保留延迟、误检、历史保持和 source-disjoint
   指标；mAP 仅作辅助观察。
5. 再实现资产驱动场景路由编排和完整增量训练审计；不得按 case/display name
   在 C++ 中分派。
6. 修复 F1 Location 与 F6 首个 Annotation Tools 控件焦点，完成 T8 人工 GUI 复核，
   最后执行 T9 Contract/Promotion gate。

任何一步缺失强制资产、使用同源增强冒充独立验证、混改多个变量族或仍存在
`UNRESOLVED_DIFFERENCE` 时，`promotion_allowed` 必须保持 `false`。
