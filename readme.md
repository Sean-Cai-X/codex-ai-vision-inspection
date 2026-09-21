# 项目迁移与品牌独立声明

> **迁移时间**: 2026-09-21
> **原来源仓库**: https://github.com/Sean-Cai-X/cxvision 分支 `codex/cxcore-integration`
> **完整 HEAD 基线**: `c8fd780` — `add Next`（1527 files → 清理 5 个 Next_*.txt 后 1522）
> **本仓库种子提交**: `e4a19a5` — 全新独立 Git 历史，无上游关联
> **本版本定位**: Phase-1 仓库级重制基线（纯仓库变更 / 无驱动 / 无内核）

---

## 一、迁移背景与核心目的

本次将原 `cxvision` 代码库的 `codex/cxcore-integration` 分支完整迁移至独立仓库 `codex-ai-vision-inspection`，核心目的是**终结开发阶段的临时命名耦合，建立完全独立的开源项目标识与技术体系，为后续面向大模型视觉推理、AI 自动开发的演进路线提供独立的开源承载主体**。

本次迁移不是简单的仓库改名，而是项目定位从「特定厂商 SDK 的二次开发分支」升级为「通用端侧视觉检测开源框架」的正式起点，所有品牌标识、命名体系、架构边界都将以独立开源项目的标准完成重构。

## 二、品牌与归属独立声明

**本项目与「cxvision」「晶视科技（Cvitek）」「cxcore 官方 SDK」不存在任何官方隶属、品牌授权、商业合作或官方关联关系。**

1. 早期开发阶段的代码基于上游开源版本进行技术衍生，仅用于技术验证与功能迭代，不代表任何官方品牌立场；
2. 本项目不使用、不授权、不代表任何原厂商业标识与产品品牌，所有涉及原厂的技术接口仅作为通用硬件适配方案之一存在；
3. 重构完成后，项目所有命名、标识、文档、输出物将彻底剥离所有原厂相关词根与品牌暗示，形成完全独立的项目知识产权体系。

> Codex AI Vision Inspection 缩写 cxvision / cxcore 均为开发期临时对齐手段，与上述品牌无任何关联。

## 三、开发阶段命名耦合的历史说明

前期开发过程中临时沿用 `cx` 前缀的命名体系，属于**开发阶段的过渡性技术对齐手段**，而非品牌归属声明：

- 目的是快速对齐原厂 SDK 的接口定义、调用逻辑与数据结构，降低早期适配的沟通与联调成本；
- 该命名仅存在于开发期内部代码，从未作为官方品牌对外使用，也不代表项目对原厂品牌的从属关系；
- 随着项目功能闭环完成、独立架构逐步成型，开发阶段的临时命名已成为项目独立演进、社区化发展与大模型方向拓展的阻碍，因此必须进行系统性的命名重构与解耦。

## 四、本次重构的处理范围

本次仓库迁移阶段将完成第一层基础解耦，全部处理仅在仓库层面执行，不涉及底层架构与驱动改造：

1. **全量命名替换**：所有目录、文件、代码符号、构建脚本、配置文档中的 `cx` / `cxcore` / `cxvision` 系列标识，统一替换为项目专属 `codex_ai` 命名体系；
2. **品牌痕迹清理**：移除所有文档、注释、配置中的原厂商标、专属商业表述与身份暗示，替换为通用技术描述；
3. **冗余资产剥离**：清理开发阶段的个人临时文件、硬编码本地路径、专属业务代码，保留通用视觉检测核心能力；
4. **项目身份重制**：重写项目文档、README、版本说明，以独立开源项目的身份正式对外发布。

## 五、后续演进方向

本次命名与身份重构，核心是为后续的项目演进扫清品牌与定位障碍，后续本项目将重点向三个方向升级：

1. **大模型视觉检测融合**：引入多类视觉大模型、端侧分割模型，构建「传统几何测量 + 大模型语义感知」的双轨检测体系；
2. **AI 自动开发体系**：建立原子化代码标准与语义化目录架构，支撑 AI 自动生成代码、自动迭代算法、自动验证指标，逐步实现「机器做开发、人做审核与运维」的开发模式；
3. **跨平台通用适配**：解耦硬件绑定，支持多类端侧芯片与推理引擎，打造中立、通用的端侧视觉检测开源工具集。

当前版本为项目功能基线版本，完整继承原分支的所有算法能力与测量体系；后续将按独立项目的演进路线，逐步完成架构升级与能力拓展。

---
# CxVision Code Wiki v2.6

> **文档版本**：v2.6
> **对应分支**：`codex/cxcore-integration`
> **核验日期**：2026-09-16
> **公开代码基线**：`a69c284` — `add Controlled circle closed-region case`
> **替代版本**：v2.5 / 2026-08-18 / `3b260e2`
> **文档标题**：量测模型算法层、FindSegmentation 融合链、受控几何训练数据与诊断闭环基线
> **核验方式**：公开分支源码、CMake、任务路由、数据合同和 Evidence 资产静态核验
> **核验边界**：本文不将源码存在直接等同于目标 Windows、LibTorch、CUDA 环境中已经完成 Clean Build、GPU 运行和精度验收

---

## 目录

0. [文档状态规则](#0-文档状态规则)
1. [当前项目总体判断](#1-当前项目总体判断)
2. [v2.5 → v2.6 的关键变化](#2-v25--v26-的关键变化)
3. [v2.6 完整逻辑架构](#3-v26-完整逻辑架构)
4. [各模块职责边界](#4-各模块职责边界)
5. [Model System 必须正式分成两层](#5-model-system-必须正式分成两层)
6. [YOLOv8-Seg 当前准确状态](#6-yolov8-seg-当前准确状态)
7. [受控几何与去干扰增强当前状态](#7-受控几何与去干扰增强当前状态)
8. [FindSegmentation 当前详细状态](#8-findsegmentation-当前详细状态)
9. [Canonical Result 当前瓶颈](#9-canonical-result-当前瓶颈)
10. [建议冻结的六条标准执行链](#10-建议冻结的六条标准执行链)
11. [Model Stage Inspection](#11-model-stage-inspection)
12. [YOLOv8-Seg Metrology V2 建议](#12-yolov8-seg-metrology-v2-建议)
13. [正式 Target Assignment](#13-正式-target-assignment)
14. [正式 Loss 体系](#14-正式-loss-体系)
15. [图像稀疏化核心](#15-图像稀疏化核心)
16. [Development Case 生命周期](#16-development-case-生命周期)
17. [Build / 工程化现状](#17-build--工程化现状)
18. [Current Status Matrix — v2.6](#18-current-status-matrix--v26)
19. [后续推进优先级](#19-后续推进优先级)
20. [Test Taxonomy v2.6](#20-test-taxonomy-v26)
21. [必须执行的验证 Gate](#21-必须执行的验证-gate)
22. [Architecture Rules v2.6](#22-architecture-rules-v26)
23. [CODE_WIKI.md 更新清单](#23-code_wikimd-更新清单)
24. [最终结论](#24-最终结论)

A. [Core File Index](#appendix-a-core-file-index)
B. [CxScript Asset Index](#appendix-b-cxscript-asset-index)
C. [Artifact Schemas](#appendix-c-artifact-schemas)
D. [CLI Options](#appendix-d-cli-options)
E. [Placeholder Register](#appendix-e-placeholder-register)
F. [Legacy Stage25 C++](#appendix-f-legacy-stage25-c)

---

## 0. 文档状态规则

| 状态                          | 定义                                          |
| --------------------------- | ------------------------------------------- |
| **[Verified]**              | 正式构建、真实数据、固定 Case、结果、几何、量测、Evidence 和验收全部通过 |
| **[Module Verified]**       | 模块内部测试完成，但完整应用链尚未通过                         |
| **[Implemented]**           | 实现已经进入正式构建源集                                |
| **[Source Implemented]**    | 源码存在，但缺少完整运行或固定回归证明                         |
| **[Partial]**               | 主链存在，但仍有明确的数据、执行或验收断点                       |
| **[Contract]**              | 数据结构、任务、Schema 或模型目标已经定义                    |
| **[Scaffold]**              | 运行骨架或诊断资产存在，但尚未接入真正业务目标                     |
| **[Verification Pending]**  | 实现存在，但缺少固定参考数据和验收                           |
| **[Pending Model Binding]** | 数据和合同已形成，但尚未绑定到真实模型结构与训练                    |
| **[Pending Data Binding]**  | 需要真实权重、标注、Ground Truth、标定或设备数据              |
| **[Replay Only]**           | 使用预计算结果回放，不代表模型在本次运行中真实执行                   |
| **[Placeholder]**           | 当前仍由固定参数、规则或临时数据产生                          |
| **[Policy Defined]**        | 开发规则已经确定，但代码尚未完全执行该规则                       |
| **[Legacy]**                | 兼容旧入口，不作为新功能主线                              |
| **[Disabled]**              | 实现存在但默认关闭                                   |
| **[Not Implemented]**       | 当前模型或运行链中尚不存在该能力                            |

状态晋级必须遵循：

```text
Source Exists
→ Build Registered
→ Runtime Reachable
→ Real Data Executed
→ Model Objective Correct
→ Canonical Result Complete
→ Geometry / Measurement Complete
→ Evidence Complete
→ Fixed Regression Verified
```

以后不得再用：

```text
能编译
能 forward
能 backward
有 JSON
有 Overlay
```

替代：

```text
模型算法完成
模型质量通过
量测目标通过
```

---

# 1. 当前项目总体判断

## 1.1 当前阶段

当前代码已经形成较完整的工程运行平台：

```text
CxScript
→ Manual / Headless / Suite
→ 传统视觉 / Torch Runtime
→ Canonical Result
→ Geometry / Measurement
→ Evidence / Diagnostic
→ Review
```

根 CMake 已将传统视觉工具、FindSegmentation、Torch 接入、CxCore、Calibration、Measurement Semantics、Metrology Analytics、几何参考评价、Predictive Geometry Gate、Automatic Diagnostic Closure 和 Precision Evaluation 等能力纳入主目标。

但项目尚未完整形成的，是**面向精密量测的模型算法层**：

```text
受控几何 Target
→ 量测保持型干扰增强
→ 正式实例目标分配
→ 几何回归 Head
→ Boundary / Geometry / Consistency Loss
→ 真实量测目标训练
→ 增量训练抗遗忘
```

因此当前最准确的结论是：

> **工程框架、模型运行基础和诊断资产已经较完整；YOLOv8-Seg 的任务专用模型结构、训练目标和量测损失仍未完成。**

## 1.2 当前三条发展线并不同步

```text
第一条：Runtime / Evidence / Contract
推进较快

第二条：Controlled Geometry / Augmentation / Training Target
已经形成较多数据与合同

第三条：Geometry Head / Assignment / Loss / Formal Training
仍未真正绑定
```

`CxGeometryReferenceEvaluator` 已能够生成受控几何样本、增强变体、训练/验证/留出集划分和 Geometry Head 训练合同；但生成资产仍明确记录 `training_enabled=0`、`model_executed=0` 和 `PENDING_GEOMETRY_HEAD_BINDING`。

因此下一阶段不应继续优先增加：

```text
Runtime
Bridge
Manager
Evidence Framework
```

而应把已经存在的：

```text
几何目标
增强数据
训练合同
量测评价
```

真正接入：

```text
模型结构
正样本分配
Loss
训练
应用验证
```

---

# 2. v2.5 → v2.6 的关键变化

## 2.1 FindSegmentation 合同明显扩展

当前 `FindSegmentation` 已支持：

```text
backend
model_path
task_id
model_id
model_package_ref
manifest_path
output_root
postprocess_profile
parameter_profile_ref
device
threshold
ROI / Prompt
geometry_type
```

结果合同也已定义：

```text
regions
primitive_hypotheses
raw_result_available
refined_result_available
fallback_used
raw_*_ref
refined_*_ref
geometry_fit_status
oriented_box_evidence_ref
```

说明 FindSegmentation 已经开始承担统一的应用级分割合同。

## 2.2 `extractboundary()` 已实现传统几何拟合

当前 `FindSegmentation::extractboundary()` 已能够：

```text
选择主要轮廓
→ 根据 geometry_type
→ 拟合 Circle / Ellipse / Oriented Box / Line
→ 生成 CxGeometryPrimitiveHypothesis
→ 保存 residual、support 和几何参数
```

但它仍然只选择面积最大的轮廓，并且代码明确指出 OBB 来自 `mask-to-minAreaRect postprocess`，不是模型原生角度回归。

因此必须严格区分：

```text
FindSegmentation 传统轮廓几何拟合       [Implemented]

YOLOv8-Seg 原生几何回归 Head           [Not Implemented]
```

## 2.3 YOLOv8-Seg 训练任务已进入 Dispatcher

Production Dispatcher 当前正式路由：

```text
DeepLabV3Plus Segmentation
EdgeSAM Prompt Segmentation
YOLOv8 Detection
YOLOv8 Instance Segmentation
YOLOv8-Seg Backward Smoke
Segmentation Training Lifecycle
Prototype Lifecycle
Incremental Package Validation
```

这说明 YOLOv8-Seg 已经不再只是孤立测试文件。

不过当前任务名称仍为：

```text
backward_smoke
```

它证明的是：

```text
数据能够读入
Loss 能够计算
梯度能够产生
optimizer.step 能够执行
Checkpoint 能够生成
```

而不是：

```text
正式目标分配已经完成
几何 Head 已经完成
任务专用 Loss 已经完成
量测模型质量已经通过
```

## 2.4 受控几何和增强数据资产已经形成

当前受控数据生成器支持或识别：

```text
Gaussian Blur
Sensor Noise
Brightness Scale
Side Appearance Shift
Boundary Sharpen
Rotate
Translate
Scene Scale
Elastic Deform
Local Gap
Edge Jagged Cut
Line Break
```

并可强制 train、validation、holdout 来源分离和类别覆盖一致。

但当前这些资产仍属于：

```text
数据和训练合同
```

而不是：

```text
已被 YOLOv8-Seg 正式 DataLoader 和 Loss 消费的训练输入
```

## 2.5 自动诊断闭环已进入构建，但不自动发布模型

Automatic Diagnostic Closure 已进入根 CMake，能够处理 Typed Label、Parent/Child 绑定、预计算结果、Mask 比较和 Promotion Gate；但代码始终将 `promotion_allowed` 写为 `false`，最终仍要求人工审核。

这一定义是合理的：

```text
自动诊断
→ 形成 Promotion Candidate

人工审核
→ 决定 Accept / Reject
```

它不应演变为：

```text
自动训练
自动替换生产模型
```

---

# 3. v2.6 完整逻辑架构

以下八个域是职责边界，不代表继续增加八套中间框架。

```text
┌────────────────────────────────────────────────────────────┐
│ 1. Application / Workbench                                 │
│ Manual Console / Image View / Annotation / Review UI       │
└──────────────────────────┬─────────────────────────────────┘
                           ▼
┌────────────────────────────────────────────────────────────┐
│ 2. CxScript / Case / Asset System                          │
│ Parser / Globals / Catalog / Manifest / Suite / Contract   │
│ Development Case / Released Revision                       │
└──────────────────────────┬─────────────────────────────────┘
                           ▼
┌────────────────────────────────────────────────────────────┐
│ 3. Unified Execution                                      │
│ Manual / Headless / Suite / Evidence / Runtime Capture     │
└───────────────┬───────────────────────────┬────────────────┘
                ▼                           ▼
┌──────────────────────────┐   ┌─────────────────────────────┐
│ 4. Traditional Vision    │   │ 5. Model System            │
│ Find* / Gauge / CxCore   │   │ Runtime Foundation         │
│ Edge / Geometry Fit      │   │ Task-specific Algorithm    │
└───────────────┬──────────┘   └─────────────┬───────────────┘
                └──────────────┬─────────────┘
                               ▼
┌────────────────────────────────────────────────────────────┐
│ 6. FindSegmentation Fusion / Sparsification                │
│ Traditional / DeepLab / YOLOv8-Seg / EdgeSAM              │
│ Region / Boundary / Sparse Points / Hybrid Refinement      │
└──────────────────────────┬─────────────────────────────────┘
                           ▼
┌────────────────────────────────────────────────────────────┐
│ 7. Measurement / Diagnostic / Promotion                    │
│ Geometry Hypothesis / Calibration / Metrology              │
│ Reference Evaluation / Predictive Gate / Human Review      │
└──────────────────────────┬─────────────────────────────────┘
                           ▼
┌────────────────────────────────────────────────────────────┐
│ 8. Foundation / Build / Observability                      │
│ OpenCV / OCCT / LibTorch / cxgeom / CMake / Log / Trace    │
└────────────────────────────────────────────────────────────┘
```

---

# 4. 各模块职责边界

## 4.1 Application / Workbench

负责：

```text
定义 ROI / Gauge
选择脚本和模型
修改参数
执行
观察 Shape / Overlay
人工审核
```

不得负责：

```text
重新实现算法
重新做模型后处理
重新计算量测结果
```

## 4.2 CxScript / Case / Asset System

负责：

```text
对象
参数
调用顺序
条件
结果引用
Case
Manifest
Suite
Contract
Evidence Chain
```

CxScript 是逻辑层，不是算法或模型实现层。

## 4.3 Traditional Vision / CxCore

负责：

```text
灰度和梯度分析
边缘采样
参数整定
亚像素定位
异常点过滤
几何拟合
量测结果
```

传统算法长期价值不是取代模型，而是把模型产生的密集 Mask 或边界先验转换为：

```text
稀疏
精确
稳定
可解释
可量测
```

的几何点和参数。

## 4.4 `libtorchsegmentation`

定位为：

> **可复用语义分割模型结构库和 Stage 设计参考。**

它已经定义 `Backbone::features()` 和 `features_at()`，适合作为模型 Stage 解构思路。

当前根工程只直接：

```cmake
add_subdirectory("libtorch_module")
```

没有把 `libtorchsegmentation` 作为应用主运行路径接入。

因此正确关系是：

```text
libtorchsegmentation
→ 模型结构复用和 Stage 设计参考
→ libtorch_module
→ 正式训练 / 推理 / 模型包 / Runtime
```

禁止建立：

```text
cximage → libtorchsegmentation
```

第二套应用推理路径。

## 4.5 `libtorch_module`

定位为：

> **模型定义、训练、推理、权重、Model Package、性能和生命周期的唯一正式模型边界。**

它负责：

```text
模型构造
权重加载
Forward / Backward
Optimizer
Checkpoint
推理后处理
Runtime Artifact
Model Package
性能分析
```

不负责：

```text
最终 Case 比较
传统参数整定
业务 UI
最终量测结论
```

## 4.6 `cximage::FindSegmentation`

定位为：

> **传统分割、语义分割、实例分割、边界稀疏化和几何量测的应用级交汇点。**

负责：

```text
图像 / ROI / Prompt
模型或传统策略选择
Raw Result 标准化
RegionSet
边界提取
稀疏化
传统边缘精修
Primitive Hypothesis
量测交接
```

不负责：

```text
Optimizer
Loss
Backward
Checkpoint Training
权重转换
网络定义
```

## 4.7 Measurement / Diagnostic / Promotion

负责：

```text
Calibration
Physical Unit
Geometry Reference
Precision
Repeatability
Uncertainty
Parent / Child 比较
Promotion Candidate
Human Review
```

不得自动修改生产模型。

---

# 5. Model System 必须正式分成两层

## 5.1 Model Runtime Foundation

当前已经较完整：

```text
网络构造
State Dict 加载
Forward
Backward
Optimizer
Checkpoint
Model Package
推理后处理
Runtime Artifact
Evidence
```

## 5.2 Task-specific Model Algorithm

当前仍是主要缺口：

```text
模型究竟要学习什么
几何 Target 如何编码
实例正样本如何分配
Head 输出哪些几何参数
Loss 如何围绕边界和量测设计
干扰增强如何保持真实几何
增量训练如何避免旧能力退化
```

以后所有进度汇报必须分开写：

```text
Runtime Foundation 状态
Model Algorithm 状态
```

不得再用：

```text
Backward 成功
```

替代：

```text
任务专用模型训练完成
```

---

# 6. YOLOv8-Seg 当前准确状态

## 6.1 当前模型输出

当前 `YoloV8SegRawOutput` 包含：

```text
box_logits[3]
class_logits[3]
mask_coefficients[3]
prototypes
```

Head 内部只有：

```text
Box Branch
Class Branch
Mask Coefficient Branch
Proto Branch
DFL
```

没有：

```text
primitive type
center
radius
axes
angle
line endpoints
geometry quality
geometry uncertainty
```

等原生几何回归输出。

所以当前：

```text
YOLOv8-Seg Instance Segmentation Head    [Implemented]

Native Geometry Regression Head          [Not Implemented]
```

## 6.2 当前几何参数来自传统后处理

当前 Circle、Ellipse、Oriented Box 和 Line 等参数来自：

```text
Mask / Contour
→ OpenCV 或传统几何拟合
```

而不是模型直接回归。`FindSegmentation.cpp` 对 OBB 明确记录为 `postprocess_not_native_angle_regression`。

这条混合链本身有价值：

```text
模型定位区域
→ 传统算法精确量测
```

但不能将其宣传为：

```text
模型原生预测量测几何
```

## 6.3 当前严格权重加载不是原子操作

`load_state_dict_strict()` 当前流程是：

```text
遍历 Target
→ 找到 Source
→ Shape 匹配
→ 立即 copy_
→ 最后再检查 complete
```

因此如果后续发现 missing key 或 shape mismatch，模型可能已经被部分写入。

应调整为：

```text
Phase A
检查全部 Key / Shape / Dtype
→ 生成完整 Mapping Report

complete = false
→ 不修改模型
→ 落盘失败报告

complete = true
→ Phase B 统一 copy
```

## 6.4 当前训练确实有真实参数更新

Production Dispatcher 已正式路由 `YoloV8SegBackwardSmoke`。当前代码还具备实例多边形数据读取、Mask Target、Loss、Backward 和 Optimizer 等训练基础，并拒绝 bbox-only 标注。

因此它不是完全空壳。

但其准确定位应是：

```text
YOLOv8-Seg 工程训练链           [Implemented / Experimental]

正式量测模型算法                [Not Completed]
```

## 6.5 正式 Task-Aligned Assignment 尚未接入

当前 Dispatcher 和训练执行链中未见 `TaskAlignedAssigner` 的使用；现有训练主要围绕实例中心和各尺度 Cell 计算 Class、Mask、Box 与 DFL 监督。

因此当前状态是：

```text
中心 Cell Smoke Assignment       [Implemented]

正式 Task-Aligned Assignment     [Not Integrated]

Mask / Boundary / Geometry
统一 GT 身份传播                 [Not Implemented]
```

## 6.6 当前损失仍是基础版

当前训练主体仍围绕：

```text
Class BCE
Mask BCE
Box / Distance
DFL
```

尚未形成正式的：

```text
Dice Loss
Boundary Distance Loss
Chamfer / Hausdorff Loss
Geometry Parameter Loss
Mask-Geometry Consistency
Clean-Augmented Consistency
Incremental Distillation
Uncertainty Calibration
```

受控几何合同虽然已经列出 `type_classification`、`parameter_regression`、`mask_contour_consistency`、`fit_residual` 和 `uncertainty_calibration`，但同时明确标记为 `PENDING_GEOMETRY_HEAD_BINDING`。

---

# 7. 受控几何与去干扰增强当前状态

## 7.1 已实现的部分

当前几何评价与数据生成代码已经能够：

```text
读取受控几何事实
生成 Typed Label
评价 Circle / Ellipse / Rectangle / Polygon
评价 Line / Arc / Open Curve
生成增强图像和标签
生成 train / validation / holdout
检查来源独立性
生成 Training Target 和 Metrology Target
```

并能够处理光度、几何和结构性干扰。

## 7.2 尚未完成的部分

当前生成资产明确记录：

```text
training_enabled = 0
human_review_required = 1
promotion_allowed = 0
model_executed = 0
PENDING_GEOMETRY_HEAD_BINDING
```

说明增强资产尚未正式进入 YOLOv8-Seg DataLoader、Assignment、Head 和 Loss。

准确状态应是：

| 能力                          | 状态                                     |
| --------------------------- | -------------------------------------- |
| 受控几何资产                      | **[Implemented]**                      |
| 增强数据生成                      | **[Implemented]**                      |
| Train/Validation/Holdout 分割 | **[Implemented/Verification Pending]** |
| 几何训练合同                      | **[Contract]**                         |
| 增强数据进入 YOLOv8-Seg 训练        | **[Pending Model Binding]**            |
| Geometry Head               | **[Not Implemented]**                  |
| Geometry Loss               | **[Not Implemented]**                  |

---

# 8. FindSegmentation 当前详细状态

## 8.1 输入合同已形成

输入已经能够表达：

```text
backend
task
model
model package
manifest
postprocess profile
traditional parameter profile
device
threshold
prompt
geometry type
```

这是正确方向。

## 8.2 实际 Torch 路由仍硬编码 DeepLab

当前：

```text
backend=torch
backend=edgesam
backend=libtorch_segmentation
```

都会进入同一个 `FindSegmentationEdgeSamBackend`。

该 Backend 随后仍将 Runtime Task 写死为：

```text
torch.infer.segmentation.deeplabv3plus.v1
```

没有使用已经存在的 `input.task_id`。

因此：

```text
FindSegmentation Task/Model 合同       [Implemented]

DeepLab 执行                           [Implemented]

YOLOv8-Seg 经 FindSegmentation 路由     [Not Completed]

EdgeSAM 精确路由                       [Not Completed]
```

近期应将该类调整为通用：

```text
FindSegmentationTorchBackend
```

并由：

```text
task_id
model_id
model_package_ref
manifest_path
```

决定实际模型。

## 8.3 Raw、Refined 和 Fallback 仍混合

当前 Torch Backend 在得到模型 Mask 后还会执行：

```text
ROI 约束
GrabCut Prompt Refinement
Positive / Negative Prompt 检查
```

并可能生成 `prompt_roi_fallback_for_libtorch_smoke` 的矩形 Fallback。

必须固定三种结果：

```text
Raw Model Result
→ 模型原始能力

Refined Result
→ 模型 + 传统后处理

Fallback Result
→ 流程保底
```

规则：

```text
fallback_used = true
→ 流程可以继续
→ 模型质量 Gate 必须失败
```

## 8.4 RegionSet 合同存在，但实际未填充

`FindSegmentationRegion` 已定义：

```text
stable_id
class_id
class_name
confidence
bbox
mask
contour
mask_ref
contour_ref
```

但当前 Backend 中没有发现 `output.regions` 的实际构造。

因此：

```text
RegionSet Contract       [Contract]

RegionSet Population     [Not Completed]
```

## 8.5 多实例仍被压缩为主要轮廓

`extractboundary()` 目前选择面积最大的 contour；显示层也主要发布一组边界和第一个几何假设。

这适合单目标语义分割，但不适合：

```text
instance_001
instance_002
instance_003
```

的 YOLOv8-Seg 应用。

## 8.6 传统基线仍只是 Smoke

当前 OpenCV 后端仍是确定性 Smoke 基线，不等同于经过人工或参数搜索得到的最优传统策略。

正式比较必须建立：

```text
traditional_best_profile
```

并确保传统、模型和混合方案使用同一个：

```text
Region
Boundary
Geometry
Measurement
Evaluator
```

---

# 9. Canonical Result 当前瓶颈

当前 `CxInferenceResult` 只有：

```text
detections[]
optional mask
metrics
artifact refs
```

没有结构化的实例对象。

`TorchRuntimeResultAdapter` 虽然会读取 `stable_id` 区块，但最终只提取：

```text
bbox
confidence
class_id
```

并写入普通 `CxTorchDetection`；实例 Mask、Contour、Centroid、Area 和 Stable ID 没有进入正式结果。

建议只扩展现有结果，不新建 V2：

```cpp
struct CxTorchInstance
{
    std::string stable_id;

    int class_id = -1;
    std::string class_name;
    double confidence = 0.0;

    CxTorchDetection bbox;

    std::string mask_ref;
    std::string contour_ref;
    std::string refined_boundary_ref;
    std::string measurement_ref;

    double centroid_x = 0.0;
    double centroid_y = 0.0;
    double pixel_area = 0.0;

    std::map<std::string, double> metrics;
};
```

目标链：

```text
Runtime Instance
→ CxTorchInstance
→ FindSegmentationRegion
→ Sparse Boundary
→ Geometry
→ Measurement
→ Evidence
```

必须全程保持同一个 `stable_id`。

---

# 10. 建议冻结的六条标准执行链

## 10.1 传统视觉量测链

```text
Image
→ ROI / Gauge
→ CxScript
→ Traditional Tool
→ Edge Samples
→ Filter
→ Geometry Fit
→ Measurement
→ Precision Gate
→ Evidence
```

## 10.2 Model Package Gate

```text
Architecture
→ Stage Description
→ Atomic Weight Mapping
→ Raw Tensor Parity
→ Postprocess Parity
→ Fixed Inference
→ Package Accepted
```

## 10.3 Model Algorithm Gate

```text
Geometry Target
→ Augmentation
→ Assignment
→ Geometry Head
→ Specialized Loss
→ Synthetic Overfit
→ Real Dataset Train
→ Ablation
```

## 10.4 FindSegmentation 融合链

```text
Traditional / DeepLab / YOLOv8-Seg / EdgeSAM
→ RegionSet
→ Boundary
→ Sparse Points
→ Traditional Edge Refinement
→ Geometry
→ Measurement
→ Strategy Comparison
```

## 10.5 增量训练链

```text
Failure Case
→ Human Corrected Label
→ Dataset Snapshot
→ Parent Model
→ Child Training
→ Distillation
→ Parent / Child Paired Inference
→ Frozen Holdout
→ Promotion Candidate
→ Human Review
```

## 10.6 诊断与 Evidence 链

```text
Controlled Reference
→ Runtime or Replay
→ Mask / Geometry Evaluation
→ Precision / Stability
→ Failure Classification
→ Suggested Action
→ Human Review
→ Closure Evidence
```

---

# 11. Model Stage Inspection

当前不应建设第二套 Darknet Runtime。

应从 `libtorchsegmentation` 已有的：

```text
features()
features_at()
Backbone / Decoder / Head
```

经验出发，在 `libtorch_module` 中建立轻量 Model Stage Inspection。

第一版只描述：

```text
stage name
module path
input stages
weight prefixes
input/output shape
parameter count
dtype
device
runtime
```

用途限定为：

```text
结构检查
权重映射
Tensor Trace
性能 Profiling
量化规划
```

禁止扩张成：

```text
Graph Executor
Tensor Scheduler
Alternative Runtime
Graph Compiler
```

真实执行继续由 LibTorch Module 完成。

---

# 12. YOLOv8-Seg Metrology V2 建议

原始模型应冻结为：

```text
YoloV8SegmentImpl V1
```

用途：

```text
原始权重兼容
严格映射基线
标准实例分割基线
回归比较
```

新增：

```text
YoloV8SegMetrologyImpl V2
```

建议结构：

```text
Backbone
→ PAN / FPN
→ Standard YOLOv8-Seg Head
   ├─ Box
   ├─ Class
   ├─ Mask Coefficient
   └─ Proto

→ Geometry Regression Head
   ├─ Primitive Type
   ├─ Center Offset
   ├─ Size / Radius / Axes
   ├─ Orientation sin/cos
   ├─ Geometry Quality
   └─ Uncertainty
```

建议统一输出：

```text
primitive_type_logits
center_dx
center_dy
size_a
size_b
sin_theta
cos_theta
geometry_quality
log_sigma
```

通过 `geometry_valid_mask` 支持：

```text
Circle
Ellipse
Rectangle / OBB
Line
Arc
Freeform
```

不同实例只监督其有效参数。

---

# 13. 正式 Target Assignment

正式训练必须统一展开 P3/P4/P5 候选，然后得到：

```text
foreground_mask
assigned_gt_indices
target_scores
target_boxes
target_labels
```

同一个 `assigned_gt_index` 必须继续传递到：

```text
Mask Target
Boundary Target
Geometry Target
Measurement Target
```

即：

```text
Positive Anchor
        ↓
同一 GT Instance
        ├─ Class
        ├─ Box
        ├─ DFL
        ├─ Mask
        ├─ Boundary
        └─ Geometry
```

当前三尺度中心 Cell 监督只保留为 Smoke，不作为正式量测模型训练策略。

---

# 14. 正式 Loss 体系

建议按消融顺序逐项加入：

```text
L_total =
    λ_cls       L_cls
  + λ_box       L_ciou
  + λ_dfl       L_dfl
  + λ_mask      L_mask
  + λ_boundary  L_boundary
  + λ_geometry  L_geometry
  + λ_cons      L_consistency
```

## 14.1 Mask

第一步：

```text
BCE + Dice
```

小目标和严重不平衡时，再评估：

```text
Focal / Tversky
```

## 14.2 Boundary

只选择一种首版方案进行验证：

```text
Distance Transform Boundary Loss
```

或：

```text
Sampled Contour / Chamfer Loss
```

不要一次堆入多种边界 Loss。

## 14.3 Geometry

```text
Primitive Type      Cross Entropy
Center              SmoothL1
Radius / Axes       SmoothL1 或相对误差
Angle               sin/cos vector loss
Uncertainty         Gaussian NLL 或校准损失
```

## 14.4 Mask-Geometry Consistency

将预测几何渲染成：

```text
Geometry Mask / Geometry Boundary
```

再与实例 Mask 或 Boundary Target 比较。

## 14.5 增量训练 Distillation

Child Model 对旧样本至少保持：

```text
Class Logits
Mask Logits
Feature
Geometry Parameters
```

一致。

---

# 15. 图像稀疏化核心

在本项目中，图像稀疏化应正式定义为：

> 从密集图像、概率图、Mask 或完整轮廓中，提取能够稳定表达目标几何和量测关系的最小有效点集。

统一路径：

```text
Dense Image / Probability / Mask
→ Region
→ Boundary Candidate
→ Raw Boundary Points
→ Sparse Boundary Points
→ Subpixel Refinement
→ Outlier Rejection
→ Primitive Hypothesis
→ Geometry / Measurement
```

统一评价字段：

```text
raw_point_count
sparse_point_count
retention_ratio
coverage_ratio
max_boundary_gap
outlier_ratio
fit_residual
measurement_error
```

不同策略只能在：

```text
候选区域和边界先验如何产生
```

上不同。

后续：

```text
稀疏化
精修
拟合
量测
```

必须共享同一个执行核心。

---

# 16. Development Case 生命周期

当前开发规则已经确定：

```text
Development
→ 同一 case_id 原位覆盖

Released
→ revision + 1
```

但代码仍通过：

```text
candidate_YYYYMMDD_HHMMSS
```

创建时间戳 Candidate。

准确状态：

```text
Development Overwrite Policy     [Policy Defined]

Current Timestamp Storage        [Still Implemented]

Lifecycle Migration              [Pending]
```

在建立多策略比较前，必须完成：

```text
同一 Development Case 保存 20 次
→ Disk Current Case = 1
→ UI Case = 1
→ Headless Case = 1
→ Suite Case = 1
```

多模型或多策略应表示为：

```text
case_id
└─ strategy_runs[]
```

而不是：

```text
多个同名 Case
多个 Candidate
多个时间戳 Revision
```

---

# 17. Build / 工程化现状

根 CMake 当前：

```text
直接构建 libtorch_module
未直接构建 libtorchsegmentation
正式注册 Geometry Reference、Predictive Gate 和 Diagnostic Closure
自动复制 libtorch_module_runtime.dll
```

但 GLFW、OCCT、OpenCV、LibTorch、Eigen 和 ALGLIB 等依赖仍默认指向本机绝对路径，部分 CTest 输入图片和输出目录也使用固定 `D:/...` 路径。

当前状态：

```text
本机开发构建          [Implemented]

Runtime 源集           [Implemented]

Portable Build         [Partial]

CI Reproducibility     [Partial]

Release Packaging      [Partial]
```

工程化治理不应打断模型算法主线，但进入正式持续回归前必须完成。

---

# 18. Current Status Matrix — v2.6

| 子系统                                  | 当前状态                                   | 结论                             |
| ------------------------------------ | -------------------------------------- | ------------------------------ |
| Workbench / Annotation               | **[Implemented]**                      | 主要人工调试与审核表面                    |
| CxScript / Manual / Headless / Suite | **[Implemented/Partial]**              | 主链形成，跨入口固定一致性待验                |
| Development Case 覆盖                  | **[Policy Defined/Not Implemented]**   | 底层仍使用时间戳 Candidate             |
| Traditional Find*                    | **[Implemented/Verification Pending]** | 精度与稳定性继续固定                     |
| FindSegmentation 输入合同                | **[Implemented]**                      | Task/Model/Package/Profile 已拆分 |
| FindSegmentation 精确任务路由              | **[Partial]**                          | Backend 仍硬编码 DeepLab           |
| Raw / Refined / Fallback             | **[Contract/Partial]**                 | 字段存在，评价链仍混合                    |
| RegionSet                            | **[Contract/Partial]**                 | Backend 未实际填充                  |
| Classical Geometry Fit               | **[Implemented]**                      | Circle/Ellipse/OBB/Line        |
| Multi-instance Geometry              | **[Partial]**                          | 仍优先主要轮廓                        |
| Traditional Tuned Baseline           | **[Placeholder]**                      | 当前主要为 Smoke Backend            |
| Torch Production Dispatcher          | **[Implemented]**                      | 多任务正式路由                        |
| YOLOv8-Seg Inference                 | **[Implemented/Verification Pending]** | 推理和后处理存在                       |
| YOLOv8-Seg Backward                  | **[Implemented/Experimental]**         | 真实参数更新基础存在                     |
| Atomic Strict Weight Mapping         | **[Not Implemented]**                  | 当前可能部分加载                       |
| Model Stage Inspection               | **[Planned]**                          | 应复用 Stage 思想                   |
| Native Geometry Regression Head      | **[Not Implemented]**                  | 当前模型无几何输出                      |
| Formal Task-Aligned Assignment       | **[Not Integrated]**                   | Seg 训练未使用正式分配                  |
| Dice / Boundary Loss                 | **[Not Implemented]**                  | 当前仍以基础 Loss 为主                 |
| Geometry / Consistency Loss          | **[Not Implemented]**                  | 仅合同存在                          |
| Controlled Geometry Assets           | **[Implemented]**                      | 受控参考和评价存在                      |
| Controlled Augmentation              | **[Implemented/Verification Pending]** | 数据生成存在                         |
| Augmentation → Model Training        | **[Pending Model Binding]**            | 未接当前训练                         |
| Geometry Head Contract               | **[Contract]**                         | 明确 Pending Binding             |
| Canonical Instance Result            | **[Partial]**                          | 实例被压缩为 Detection               |
| Predictive Geometry Gate             | **[Implemented/Verification Pending]** | Gate 存在，模型原生几何尚缺               |
| Automatic Diagnostic Closure         | **[Implemented/Partial]**              | 不自动 Promotion                  |
| Calibration / Measurement Semantics  | **[Implemented/Partial]**              | 物理量闭环待验证                       |
| Metrology Analytics                  | **[Implemented/Verification Pending]** | 真实设备数据待验                       |
| Incremental Distillation             | **[Not Implemented]**                  | Prototype 更新不等于网络蒸馏            |
| Portable Build / CI                  | **[Partial]**                          | 依赖路径本机化                        |

---

# 19. 后续推进优先级

## P0 — 冻结开发基线

完成：

```text
Clean Build
固定 YOLOv8-Seg V1
固定 Weight Hash
固定 Controlled Dataset
固定 Train / Validation / Holdout
修改 Development Case 为原位覆盖
同步更新 CODE_WIKI
```

Gate：

```text
同一代码、模型、数据和 Case 可重复执行
```

## P1 — FindSegmentation 应用闭合

完成：

```text
通用 Torch Backend
实际使用 input.task_id
填充 RegionSet
Raw / Refined / Fallback 分离
多实例显示与几何
```

Gate：

```text
DeepLab
YOLOv8-Seg
EdgeSAM
OpenCV
```

都能进入同一：

```text
Region → Boundary → Geometry
```

链。

## P2 — Canonical Instance Result

完成：

```text
Runtime Instance
→ CxTorchInstance
→ FindSegmentationRegion
→ Geometry
→ Measurement
→ Evidence
```

Gate：

```text
stable_id 全程不丢失
```

## P3 — Model Package Gate

完成：

```text
Model Stage Description
Atomic Strict Loader
真实 Weight Mapping Report
Python/C++ Raw Tensor Parity
DFL/NMS/Mask Parity
Checkpoint Reload
```

Gate：

```text
结构、权重、RawOutput、后处理全部通过
```

## P4 — 量测模型算法层

固定顺序：

```text
Geometry Target Tensor 化
→ 受控增强接入 DataLoader
→ 正式 Task-Aligned Assignment
→ Geometry Head
→ Dice / Boundary / Geometry Loss
→ Synthetic Overfit
→ Real Dataset Training
→ Ablation
```

Gate：

```text
模型真正学习几何，而不是只靠后处理拟合
```

## P5 — 图像稀疏化和公平比较

同一个 Case 运行：

```text
traditional_tuned
deeplab_mobilenet
yolov8seg_v1
yolov8seg_metrology_v2
v2 + traditional refinement
```

统一比较：

```text
漏检
Mask
Boundary
Sparse Points
Geometry
Measurement
Repeatability
Runtime
```

## P6 — 真实增量训练

```text
Failure Case
→ Human Corrected Label
→ Dataset Snapshot
→ Parent
→ Child
→ Distillation
→ Frozen Holdout
→ Promotion Candidate
→ Human Review
```

## P7 — 性能和工程化

只有模型和应用正确性通过后：

```text
Model Resident
→ Warmup
→ Reduce D2H
→ Stage Profiling
→ FP16
→ 必要时 INT8
→ Portable Build
→ CI
```

---

# 20. Test Taxonomy v2.6

## Build

```text
B0 Source / Asset Presence
B1 CMake Configure
B2 Clean Release Build
B3 Runtime DLL Deployment
B4 Portable Build
```

## Model Package

```text
MP0 Architecture Baseline
MP1 Stage Description
MP2 Strict Weight Mapping
MP3 Raw Tensor Parity
MP4 Postprocess Parity
MP5 Checkpoint Reload
MP6 Fixed Inference
```

## Model Algorithm

```text
MA0 Target Schema
MA1 Target Transform
MA2 Assignment Visualization
MA3 Geometry Head Forward
MA4 Loss Gradient
MA5 Synthetic Overfit
MA6 Controlled Interference
MA7 Real Dataset
MA8 Ablation
```

## FindSegmentation

```text
FS0 Task Routing
FS1 Raw Result
FS2 RegionSet
FS3 Boundary
FS4 Sparsification
FS5 Traditional Refinement
FS6 Multi-instance Geometry
FS7 Measurement
FS8 Strategy Comparison
```

## Incremental / Promotion

```text
IT0 Dataset Snapshot
IT1 Parent Model
IT2 Child Training
IT3 Distillation
IT4 Paired Inference
IT5 Frozen Holdout
IT6 Diagnostic Closure
IT7 Human Promotion
```

## Case Lifecycle

```text
CL0 Development Overwrite
CL1 Restart Persistence
CL2 Manual / Headless / Suite Identity
CL3 Review Invalidation
CL4 Released Revision
```

---

# 21. 必须执行的验证 Gate

## G1 — Target Transform

旋转、缩放、翻转后同步验证：

```text
BBox
Mask
Boundary
Center
Axes
Angle
Line Endpoints
```

## G2 — Synthetic Overfit

少量圆、椭圆、矩形和线样本上：

```text
Mask
Primitive Type
Center
Radius / Axes
Angle
```

误差必须明显趋近于零。

## G3 — Assignment Visualization

必须可视化：

```text
GT Instance
Positive Anchors
Assigned Scale
Alignment Score
Conflict Resolution
```

## G4 — Loss Gradient

分别单独开启：

```text
Mask
Boundary
Geometry
Consistency
```

确认梯度到达正确 Head、Neck 和 Backbone。

## G5 — 干扰鲁棒性

固定同一元素比较：

```text
Clear
Blur
Noise
Brightness
Gap
Jagged Edge
Partial Visibility
```

## G6 — 应用价值

比较：

```text
漏检率
Boundary Error
Geometry Error
Measurement Error
Repeatability
Sparse Point Coverage
Runtime
```

## G7 — 增量回归

Child 必须：

```text
目标问题改善
旧能力不过度退化
Holdout 通过
人工审核通过
```

## G8 — 性能

必须同时满足：

```text
Accuracy Gate
+
Performance Gate
```

---

# 22. Architecture Rules v2.6

1. **工程 Runtime 完成不等于模型算法完成。**

2. **几何合同存在不等于 Geometry Head 已实现。**

3. **传统轮廓拟合不得被描述为模型原生几何回归。**

4. **任何新增 Head 必须同时定义 Target、Assignment、Loss、Decode、Result 和验收指标。**

5. **受控增强数据必须真正进入训练，不能只生成 Evidence。**

6. **所有几何变化增强必须同步变换完整 Target。**

7. **当前三尺度中心 Cell 监督只能作为 Smoke。**

8. **Mask BCE 不得作为最终唯一分割 Loss。**

9. **模型最终验收以几何和量测误差为主。**

10. **V1 基线必须冻结，V2 改造不得破坏原始回归。**

11. **Model Stage Inspection 只做观察，不成为第二套 Runtime。**

12. **`libtorchsegmentation` 不作为第二条应用推理路径。**

13. **Semantic、Instance 和 Traditional 结果统一进入 RegionSet。**

14. **Stable Instance ID 贯穿 Result、Geometry、Measurement 和 Evidence。**

15. **Raw、Refined、Fallback 分开保存和评价。**

16. **Fallback 不得获得模型质量 PASS。**

17. **Prototype Index 更新不得称为神经网络增量训练。**

18. **自动诊断不得自动发布模型。**

19. **Development Case 原位覆盖；Release 后才允许 Revision。**

20. **没有消融证据的新 Head 和新 Loss 不进入默认 Runtime。**

21. **FP32 正确性和应用价值未通过前，不推进 FP16/INT8。**

22. **Source Exists、Build Registered、Runtime PASS 和 Accuracy PASS 必须分别记录。**

---

# 23. CODE_WIKI.md 更新清单

当前文档需要一次完整清理，而不是继续追加尾部说明。

必须更新：

```text
1. v2.5 → v2.6
2. 核验日期 → 2026-09-16
3. 代码基线 → a69c284
4. 删除内部残留的 Current Status Matrix — v2.4
5. 删除 Architecture Rules v2.4
6. 删除文件末尾 v2.4 / 8c77433 旧元信息
7. 增加 Model Runtime Foundation / Model Algorithm 两层
8. 增加 FindSegmentation 实际硬编码 DeepLab 的断点
9. 更新 extractboundary 已实现状态
10. 明确传统几何拟合不是 Native Geometry Regression
11. 增加 Controlled Geometry 和 Augmentation Dataset
12. 增加 Geometry Head Pending Binding 状态
13. 增加 Formal Assignment 与 Specialized Loss 缺口
14. 增加 Canonical Instance Result 缺口
15. 增加 Development Case 生命周期冲突
16. 增加 Model Stage Inspection
17. 更新 Current Status Matrix
18. 更新 Test Taxonomy
19. 更新推进优先级
20. 更新 Architecture Rules
```

建议新增 Artifact：

```text
model_baseline.json
model_stage_report.json
weight_mapping_report.json
augmentation_trace.json
assignment_trace.json
loss_breakdown.json
geometry_head_output.json
instance_result.json
sparse_boundary.json
strategy_comparison.json
parent_child_regression.json
promotion_gate.json
development_case_state.json
```

---

# 24. 最终结论

当前代码已经具备：

```text
运行传统算法
运行多个 Torch 模型
训练 YOLOv8-Seg 的工程能力
生成受控几何数据
生成干扰样本
生成几何训练合同
从 Mask 做传统几何拟合
执行 Predictive Geometry Gate
生成 Evidence
执行自动诊断和人工审核
```

但当前尚未具备：

```text
模型原生预测量测几何
正式实例目标分配
边界和几何联合 Loss
受控增强驱动的正式模型训练
实例结果完整进入应用层
模型与传统算法在同一 Case 下公平比较
抗遗忘的神经网络增量训练
完整 Parent / Child Promotion
```

所以当前最准确的管理结论是：

> **Codex 已完成大量工程框架、数据合同和诊断资产；面向精密量测的模型算法本体与 FindSegmentation 纵向闭环仍是下一阶段主任务。**

下一阶段主线应冻结为：

```text
Target
→ Augmentation
→ Assignment
→ Geometry Head
→ Specialized Loss
→ Training
→ Canonical Instance
→ RegionSet
→ Sparse Boundary
→ Traditional Refinement
→ Measurement
→ Incremental Promotion
```

最终系统价值不是简单地同时拥有 OpenCV 和 Torch，而是：

> **让深度学习负责发现、分类和拆分元素，让传统算法把密集模型输出收敛为稀疏、精确、可解释的量测几何，再由受控参考、精度评价和 Evidence 判断哪一种模型或参数方案真正可用。**


---

## Appendix A. Core File Index

### A.1 cximage 

|  |  |
|------|------|
| `GuiMain.cpp` | GUI  |
| `ViewController.h/cpp` |  |
| `ManualStateTestConsole.h/cpp` |  |
| `ManualConsoleGauge.h/cpp` | Gauge  |
| `ManualConsoleParamRegressionPanel.h/cpp` |  |
| `ManualConsoleEvidenceChain.h/cpp` |  |
| `ParserDebugBridge.h/cpp` |  |
| `CxParserRuntimeOwner.h/cpp` |  |
| `CxScriptHeadlessRunner.h/cpp` |  Headless  |
| `CxScriptSuiteRunner.h/cpp` | Suite  |
| `CxParamProbeRunner.h/cpp` |  |
| `TorchRuntimeBridge.h/cpp` | Torch  |
| `TorchRuntimeResultAdapter.h/cpp` | Torch  |
| `CxUnifiedLog.h/cpp` |  |
| `CxCrashLog.h/cpp` | Crash  |

### A.2 cxparser_ext 

|  |  |
|------|------|
| `parser_pipeline.h` |  |
| `parser_runtime_facade.h` |  |
| `cxscript_runtime.h` | CxScript  |
| `parser_binding_builder.h` |  |
| `parser_flow_router.h` |  |
| `parser_validation_engine.h` |  |

---

## Appendix B. CxScript Asset Index

### B.1 cximage 

|  |  |
|------|------|
| Catalog | `cxparser/cxscript/module/cximage/catalog/` |
| Stage25 | `cxparser/cxscript/module/cximage/stage25/` |
| Frozen | `cxparser/cxscript/module/cximage/frozen/` |
| Tests | `cxparser/cxscript/module/cximage/tests/` |
| Frame Probe | `cxparser/cxscript/module/cximage/frame_probe/` |
| Diagnostic | `cxparser/cxscript/module/cximage/diagnostic/` |

### B.2 torch 

|  |  |
|------|------|
| Detection | `cxparser/cxscript/module/torch/detect_direct_test.cxsc` |
| Segmentation | `cxparser/cxscript/module/torch/segmentation_direct_test.cxsc` |

### B.3 mlpack 

|  |  |
|------|------|
| Logistic Regression | `cxparser/cxscript/module/mlpack/logreg_predict_direct_test.cxsc` |
| Handoff | `cxparser/cxscript/module/mlpack/mlpack_logreg_predict_direct_test.cxsc` |

### B.4 ensmallen 

|  |  |
|------|------|
| Geometry Tuning | `cxparser/cxscript/module/ensmallen/ensmallen_geometry_tuning_direct_test.cxsc` |
| Parameter Optimization | `cxparser/cxscript/module/ensmallen/geometry_tuning_direct_test.cxsc` |

---

## Appendix C. Artifact Schemas

### C.1 result_summary.json

```json
{
  "case_id": "...",
  "script_path": "...",
  "executed": true,
  "runtime_ok": true,
  "assets_complete": true,
  "exit_code": 0,
  "timeout": false,
  "valid_points": 10,
  "fit_line": { "slope": 0.5, "intercept": 1.0 },
  "fit_circle": { "cx": 100, "cy": 200, "radius": 50, "mean_distance": 2.3 },
  "support_score": 0.95,
  "failure_stage": ""
}
```

### C.2 gauge_annotation.json

```json
{
  "gauge_type": "line",
  "geometry": { "x0": 100, "y0": 50, "x1": 200, "y1": 150 },
  "parameters": { "threshold": 50, "linegap": 10 },
  "accepted": true,
  "manual_accepted": true,
  "dirty": false
}
```

---

## Appendix D. CLI Options

### D.1 

```
cxvision_imgui_acceptance [options]

Options:
  --script <path>    Load and execute specified CxScript
  --manifest <path>  Load specified image manifest
  --catalog <path>   Load specified script catalog
  --debug            Enable debug mode
  --log <path>       Set unified log output path
```

---

## Appendix E. Placeholder Register

### E.1 

|  |  |  |
|--------|----------|------|
| `AddMlpackRankPlaceholderCandidates()` | `ManualConsoleParamRegressionPanel.cpp` | [Placeholder] |
| `AddEnsmallenOptPlaceholderCandidates()` | `ManualConsoleParamRegressionPanel.cpp` | [Placeholder] |
| Tuning Map Animate | `ManualConsoleParamRegressionPanel.cpp` | [Visual Placeholder] |
| Hit Distribution bins | `CxParamRegressionRuntime.cpp` | [Placeholder] |

### E.2 Placeholder 

|  |  |  |
|--------|-------------|----------|
| mlpack Rank | v2.5 | mlpack  + ELPV Headless  |
| ensmallen Optimize | v2.5 | ensmallen  +  Probe Objective  |
| Hit Distribution | v2.4.1 | FindLine/FindCircle  Case  |
| Tuning Map Animate | v2.4.1 |  Panel Probe  |

---

## Appendix F. Legacy Stage25 C++

 Legacy `CXVISION_ENABLE_LEGACY_STAGE25_CPP=ON` �：

| 文件 | 说明 |
|------|------|
| `CxScriptStage25Manifest.cpp` | Stage25 清单实现 |
| `CxScriptStage25Template.cpp` | Stage25 模板实现 |
| `CxScriptStage25Runner.cpp` | Stage25 运行器 |
| `CxScriptStage25ReportWriter.cpp` | Stage25 报告输出 |
| `CxScriptStage25Register.cpp` | Stage25 注册 |
| `CxScriptStage25JsonLite.cpp` | Stage25 JSON 轻量解析 |
| `CxScriptStage25PolicyValidator.cpp` | Stage25 策略验证 |
| `CxScriptStage25CaseMatrix.cpp` | Stage25 Case 矩阵 |

#### 状态
- **[Legacy/Disabled]**：默认不编译，仅兼容保留

---
<p align="center">
<img src="https://raw.githubusercontent.com/Sean-Cai-X/cxvision/codex/cxcore-integration/diagram2.png" width="100%">
</p>


 FindSegmentation->torch /  FindLine->FindGauge 
 两种工具在深度学习和传统分析上的交汇,
 FindLine和其工具参数链路代表的分析测量的原子语义链路的可解释性,
 而FindSegmentation开始的模型和增量训练带来的边界深度学习构建化,
 这里恰恰是Ensmallen 和Mlpack关键的两个原子路径的开端,
 所以当前的推进进入一个融合语义和特征和参数链路交错的节点,用理解上说,
 深度学习的神经网络的不可理解在分析网络的可理解进行了稀疏化处理,这是系统智能化关键路径 

这里的关键并不是简单地把“传统视觉 + 深度学习”放到同一个框架里，
而是要建立一条能够在两者之间转换、约束和验证的中间语义层。

  可以把当前节点理解为：

  像素 / 图像
     │
     ├─ FindSegmentation → Torch → 特征与边界概率
     │                         │
     │                         ▼
     │                 可学习但弱可解释的表征
     │
     └─ FindLine → FindGauge → 点、边、距离、方向、容差
                               │
                               ▼
                       可解释的测量原子语义

  真正需要融合的是下面这一层：

  模型特征
     ↓ 投影、筛选、约束
  测量原子
     ↓ 组合
  工程语义、Contract、PASS/FAIL

  ### 1. FindLine → FindGauge 是“可解释原子链”

  FindLine 不应只被理解为一个检测算法，它实际上定义了一组稳定、可追踪的分析原子：

  - 输入区域是什么；
  - 扫描方向是什么；
  - 极性、阈值、Gap、采样密度是什么；
  - 得到了哪些测量点；
  - 哪些点被过滤；
  - 使用什么拟合方法；
  - 输出直线的角度、位置、残差和置信度是什么；
  - 结果如何参与距离、角度、平行度等 Gauge 结论。

  因此 FindGauge 不是另一个孤立工具，而应是 FindLine、FindCircle、Shape、几何关系等原子的组合层：

  ROI
  → Scan
  → Edge Samples
  → Filtered Samples
  → Geometric Primitive
  → Gauge Relation
  → Tolerance Decision

  这条链的价值在于每个中间状态都能快照、比较、回放和解释。参数也不是零散 UI 数字，而是这个语义链上各阶段的控制变量。

  ### 2. FindSegmentation → Torch 是“可学习边界构造链”

  FindSegmentation 的意义也不应只停留在输出 mask。它提供的是传统固定算子难以稳定构造的边界先验：

  图像
  → 神经网络特征
  → 类别概率 / Mask
  → 边界概率
  → 连通区域或候选轮廓
  → 工程测量候选

  模型可以解决纹理复杂、对比度不稳定、边缘局部缺失、背景干扰等问题。但模型输出本身不等于最终测量事实。

  例如模型输出一块区域，不代表已经得到了可验收的宽度、圆心、直线或间距。它仍需投影到传统分析原子：

  Segmentation Mask
  → Boundary Candidate
  → FindLine/FindCircle Sampling
  → Robust Fit
  → Gauge Measurement
  → Contract

  这样 Torch 负责“在哪里找”和“哪些像素更可能属于目标”，FindLine/FindGauge 负责“最终测量了什么、为何得到这个数值”。

  ### 3. “不可理解被可理解网络稀疏化”需要进一步精确定义

   这里的“稀疏化”最好不要只理解成数学意义上的稀疏参数或稀疏权重。它更接近三种稀疏化：

  1. 空间稀疏化

     神经网络把整幅图压缩成有限的候选区域、边界或关键点。

  2. 语义稀疏化

     高维特征最终被投影成少量工程原子：线、圆、区域、中心、方向、距离。

  3. 决策稀疏化

     最终 Contract 不直接解释数百万个网络参数，而解释少量可验证事实：

     左边界 = ...
     右边界 = ...
     拟合残差 = ...
     宽度 = ...
     容差范围 = ...

  所以更准确的描述是：

  > 深度学习并未被完整解释，而是通过可解释测量原子形成了一个受约束的语义瓶颈。工程结论只允许从这个瓶颈之后产生。

  这很重要。不能声称 FindLine 解释了神经网络内部为什么作出某个像素分类；它解释的是模型输出如何被转换成最终测量事实。

  ### 4. Ensmallen 与 mlpack 的位置

  这两个路径不宜被放成 Torch 的替代品，而应成为连接模型特征和分析参数的两类基础能力。

  Ensmallen 更适合承担“参数优化原子”：

  固定输入与固定 ROI
  → 参数向量
  → 执行统一测量链
  → 计算可解释目标函数
  → 优化器提出下一组参数

  目标函数不能只写成“是否通过”，而应分解为：

  边界响应强度
  + 有效点覆盖率
  - 拟合残差
  - 参数敏感度
  - ROI 扰动敏感度
  - 超时和失败惩罚

  这样优化结果仍能映射回 threshold、gap、filter、sample interval 等明确参数。

  mlpack 更适合承担“轻量特征学习原子”：

  传统特征 / Torch embedding / 测量摘要
  → 分类、聚类、异常检测或回归
  → 候选排序、失败分类、参数建议

  它可以处在两个位置：

  - Torch 之前或旁边：用低成本模型判断是否需要运行重模型；
  - Torch 与 Gauge 之间：基于模型特征和传统测量特征选择算法分支、参数 Profile 或候选排序。

  因此三者可形成分层关系：

  Torch
  高维视觉表征与复杂分割
          │
          ▼
  mlpack
  特征选择、分类、回归、异常检测
          │
          ▼
  Ensmallen
  可解释参数链上的约束优化
          │
          ▼
  FindLine / FindCircle / FindGauge
  确定性测量与工程事实

  但它们都不能各自建设独立执行链，必须共享统一请求、执行核心、结果和证据结构。

  ### 5. 后续架构重点不是增加工具，而是定义公共中间表示

  [下一步]最有价值的不是马上实现更多模型或优化器，而是先固定一套跨工具的中间结果。

  建议至少包括四层：

  Layer 1：Perception Evidence
  概率图、Mask、Embedding、响应图、候选区域

  Layer 2：Measurement Evidence
  扫描线、采样点、边缘点、过滤点、拟合输入

  Layer 3：Geometric Facts
  Line、Circle、Contour、Center、Angle、Distance、Residual

  Layer 4：Business Facts
  尺寸、容差、缺陷类别、Contract 结论

  每层都要携带：

  - 来源工具和对象引用；
  - 输入图片及 ROI 标识；
  - 参数快照；
  - 坐标系；
  - 置信度或质量指标；
  - 上游证据引用；
  - stale/version 状态；
  - elapsed、timeout、failure stage；
  - 可回放所需信息。

  由此形成真正的证据图：

  模型版本
    → segmentation candidate
    → boundary candidate
    → measurement samples
    → fitted line
    → gauge value
    → contract conclusion

  UI、Headless、Suite 和参数回归只是这张证据图的不同消费者，不再自行解释结果。

  ### 6. 增量训练必须与在线测量链隔离

  FindSegmentation 引入增量训练后，最危险的问题不是训练失败，而是运行语义漂移：

  - 同一个 case 因模型更新得到不同 mask；
  - ROI 和参数未变，但候选边界改变；
  - 人工修正数据被立即用于模型更新；
  - 新模型覆盖旧模型后无法复现历史结果；
  - Contract 的变化被模型版本变化掩盖。

  因此增量训练应当是明确的离线闭环：

  运行证据
  → 人工审核
  → 标注修正
  → 生成候选训练集版本
  → 训练候选模型
  → 固定 L1/L2/L3 回归
  → 人工批准
  → 模型 Promotion

  在线执行只能消费已经批准、不可变的模型版本。一次运行必须冻结：

  - 模型 ID 与 Hash；
  - 数据集版本；
  - 预处理 Profile；
  - 后处理参数；
  - 测量参数；
  - 脚本快照；
  - Contract 版本。

  模型建议和参数优化都不能直接改变当前运行对象，也不能绕过人工选择与 Promotion gate。

  ### 7.  推进顺序

  第一阶段先固定 FindLine → FindGauge 的原子语义链，明确每个阶段的输入、输出、参数和证据。这是整个系统的解释基准。

  第二阶段让 FindSegmentation 只提供候选区域、概率图和边界先验，不直接输出最终业务 PASS/FAIL。

  第三阶段建立从 segmentation evidence 到 measurement evidence 的标准投影，使模型结果能够进入相同的 FindLine/FindCircle/
  Gauge 链。

  第四阶段引入 mlpack，先做失败分类、候选排序和参数建议，不直接改变 Gauge。

  第五阶段引入 Ensmallen，在固定图片、固定 ROI、固定 Contract 和严格预算内优化可解释参数；候选结果仍走 L1 → L2 → L3 → 人
  工审核。

  第六阶段才开展增量训练，并把数据集、模型、参数 Profile、Contract 的版本治理纳入统一证据链。

  最终目标可以概括为：

  > 深度学习负责扩大可感知边界，传统分析负责收敛为可解释测量；mlpack负责学习两者之间的映射，Ensmallen负责在约束内优化显
  > 式参数。所有结果最终必须落到统一的几何事实、证据引用和 Contract 上。

  这也意味着下一阶段最关键的设计对象不是某个新算法类，而是“模型证据如何可靠地成为测量证据”的统一契约。只要这个契约稳定，
  FindSegmentation、Torch、mlpack、Ensmallen 和 FindGauge 才会成为同一条开发链，而不是四套继续扩张的框架。

  
*文档版本: v2.4 | 对应分支: codex/cxcore-integration | 核验日期: 2026-08-16 | 代码基线: 8c77433 | 基于仓库: cxvision_repo*


---

## GUI 快捷键控制与窗口链路验收（2026-08-27）

### Environment

- Repo：`D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo`
- Build Dir：`D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\build01`
- Binary：`D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\build01\Release\cxvision_imgui_acceptance.exe`
- Binary 状态：存在；网关本轮未返回可用修改时间
- Working Directory：Repo 根目录
- GUI Window：`glfw occt image ai`
- PID：`18040`
- HWND：`6882852`
- Window Class：`GLFW30`
- Window Rect：`(-9,-9)–(1929,1029)`
- Worktree：dirty，保留现有用户修改
- 截图证据：`D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxscript_runs\gui_shortcut_validation\run_20260827_135800`

### Compile

- 本轮 GUI 控制分析未重新编译。
- 前序同一目标编译退出码：`0`
- 前序编译结论：`COMPILE_PASS`
- Build Log：`D:\Codex-WorkDir\Sean_WorkDir\codex-lan-agent\logs\build_target_cmake_20260827_134757.log`

### GUI 控制方式

系统界面工具已闭环验证以下能力：

- 按精确标题激活窗口；
- 查询前台窗口、HWND、PID、窗口类和窗口矩形；
- 控制鼠标移动与单击；
- 注入 F1–F12、Tab、Esc 等键盘输入；
- 每次输入后截图；
- 读取右上角 `AI GUI NAVIGATION / Location` 回执；
- 通过画面焦点框判断 ImGui 内部落点。

控制时必须使用精确标题：

```text
glfw occt image ai
```

不得使用 `cxcore` 子串定位。资源管理器窗口标题可能包含该分支名，会造成误激活。

推荐的单次快捷键控制顺序：

```text
1. 精确激活 "glfw occt image ai"
2. 注入一个功能键
3. 等待至少 1 秒，让 ImGui 完成一帧更新
4. 截图
5. 核对 Location、窗口/页签和可视焦点框
```

不能只根据键盘注入命令退出码为 0 判断成功；必须核对 GUI 回执。读取截图或切换到其他工具后，目标 GUI 可能失去前台焦点，因此每个快捷键前都应重新激活目标窗口。

### Shortcut Results

| 快捷键 | 实际落点与 Location 回执 | 窗口/页签 | 焦点 | 结果 |
|---|---|---|---|---|
| F1 | 打开 `AI GUI Shortcut Help` | 帮助窗口正确前置 | 帮助窗口可操作 | Location 仍显示未使用快捷键，缺少 F1 回执 |
| F2 | `Evidence Chain UI -> Image Set tab/list` | Image Set 选中 | 可见焦点框 | 符合 |
| F4 | `Manual Review / Evidence -> Evidence tab/items` | Evidence 选中并显示证据列表 | 页签/列表定位正确 | 符合 |
| F5 | `Manual Review / Evidence -> Review decision controls` | Review 选中，审核按钮可见 | 控件区域可操作 | 符合 |
| F6 | `Image Evidence / Annotation Tools (focus pending)` | Annotation Tools 区域可见 | 未落到工具；按 Tab 跳到左侧 Case 页签 | 不符合焦点要求 |
| F7 | `Key Parameter Controls -> active tool parameter controls` | 参数窗口前置 | 首个布尔控件有焦点框 | 符合 |
| F8 | `Torch Runtime / Evidence -> runtime status and review controls` | Evidence 窗口前置 | 首控件区域可见 | 符合 |
| F9 | `Torch Training Image Set -> dataset actions and image rails` | Training Image Set 前置 | 首按钮有焦点框 | 符合 |
| F10 | `Parameter Tuning Map / Result Conclusion -> first available control` | 参数调优窗口前置 | 首控件有焦点框 | 符合 |
| F11 | `Manual State Test Console -> script editor and debug compiler` | Console 前置 | 编辑框有焦点框 | 符合 |
| F12 | `Analytics Smoke / Metrology Bridge -> analytics controls` | Analytics 窗口前置 | Analytics 折叠栏有焦点框 | 符合 |

F3 不在本轮用户指定复核范围内。

### Mouse Control

鼠标链路已验证：移动到 F1 帮助窗口右上角关闭按钮并单击后，帮助窗口正常关闭。控制过程中没有触发运行、保存、接受、拒绝或参数写回。

### Focus Semantics

Windows 系统焦点查询确认前台窗口与 focused control 均为：

```text
title = glfw occt image ai
class = GLFW30
pid   = 18040
hwnd  = 6882852
```

ImGui 内部控件不是原生 Windows 子控件，因此系统工具只能识别顶层 GLFW 窗口。内部焦点必须通过以下两项共同判断：

1. 右上角 `Location` 语义回执；
2. 截图中的 ImGui 可视焦点框。

### Confirmed Gaps

1. F1 能打开帮助窗口，但没有把 Location 更新为 F1 帮助语义。
2. F6 回执明确为 `focus pending`。
3. F6 后按 Tab，焦点跳到 Evidence Chain 的 Case 页签，没有进入 Annotation Tools 的第一个可操作控件。
4. F6 需要补齐明确的窗口前置、滚动位置和首个可操作控件焦点。

### Human Review

- Required：是
- Performed：仅由人工启动程序；其余步骤由系统 GUI 控制工具执行
- Decision：`MANUAL_GUI_PARTIAL`
- 未执行任何人工审核决定或算法运行

### Final Conclusion

- Code：`PARTIAL`
- 已闭环：窗口激活、鼠标移动/单击、键盘注入、截图、图像判读和顶层焦点查询。
- Remaining blockers：F1 Location 回执缺失；F6 控件焦点链缺失。
- Manual GUI acceptance：`PENDING_HUMAN_REVIEW`
- Overall：尚未达到最终验收条件。

##### 2026-08-27 16:41 Gauge Line 真实曲线补充修正

针对人工截图 `D:\Screenshot 2026-08-27 161957.png` 中 Find Peaks、Curve Fitting、Critical Dimension 区域没有曲线的问题，定位到预览曲线仍依赖已运行的 `height_peak_analysis_ready` 分布数据；当用户只打开参数页、尚未点击 `Analyze Current Surface` 时，界面没有可绘制 source。此前为避免假曲线已经移除公式 fallback，因此暴露为“无曲线”。

本轮修正为：三个预览图优先从当前图像和当前 Gauge 几何按 `Gauge Line NUM` 实时采样真实 profile。FindCircle 按圆心到轮廓方向选择对应扫描线并沿半径方向采样；FindLine 按线方向和法向偏移选择对应扫描线并沿线采样。source ref 使用 `runtime:gauge_line:FindCircle:num=n/total` 或 `runtime:gauge_line:FindLine:num=n/total`，并附带当前 image path。只有在实时 profile 不可用且已有真实高度分布分析结果时，才回退绘制分析后的 ADF/BCDF；不再绘制公式生成曲线。

当前保持的边界：Curve Fitting 的拟合曲线、Critical Dimension 的模型/结果曲线仍标记 `PENDING_BINDING`；Find Peaks 的峰标记只来自真实 `Analyze Current Surface` 后的峰值结果。界面出现 profile 曲线不等于算法精度验收通过。

本轮构建：`cmake --build D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\build01 --config Release --target cxvision_imgui_acceptance`，退出码 0，构建日志 `D:\Codex-WorkDir\Sean_WorkDir\codex-lan-agent\logs\build_target_cmake_20260827_164054.log`，结论 `COMPILE_PASS`。

网关截图记录：`D:\Codex-WorkDir\Sean_WorkDir\codex-lan-agent\logs\cxvision_ui_bridge_20260827_164147.log` 生成截图 `D:/Codex-WorkDir/Sean_WorkDir/codex-lan-agent/cxvision_ui_runs/20260827_084147_812.png`。`ui_screenshot_analyze` 当前存在工具参数不一致问题：无参数调用报 `cxvision exe path is required`，传入 `--exe` 又报 `Unknown option 'exe'`；该工具问题已记录，不作为算法或 UI 结果 PASS 依据。

```text
Metrology live Gauge Line profile preview  IMPLEMENTED
Release target compile                     COMPILE_PASS
GUI screenshot capture                     CAPTURED
GUI screenshot semantic analysis           TOOL_BLOCKED
Find Peaks runtime peak markers            PENDING_HUMAN_REVIEW
Curve Fitting runtime fit curve            PENDING_BINDING
Critical Dimension runtime result curve    PENDING_BINDING
Final acceptance                           NOT_ACCEPTED
```
