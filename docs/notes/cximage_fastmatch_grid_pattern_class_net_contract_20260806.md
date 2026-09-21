# FastMatch + GridPatternClassNet 隔离与融合合同

## 1. 目标

在不修改 FastMatch 已验证 `learn/match/rotate/scale` 主路径的前提下，增加一条面向字符、手写字、符号和区域类别识别的网格层级网络。

三类能力保持独立：

- `FastMatch`：边缘/结构模板定位，输出候选位置、角度、尺度和结构分数。
- `RegionPatternNet`：区域内容辅助描述，输出灰度或二值池化描述子。
- `GridPatternClassNet`：网格局部方向与占用特征、3 至 5 层确定性汇聚、类别原型、top-k 和拒识信号。

禁止用 `GridPatternClassNet` 隐式覆盖 FastMatch 结果或修改 FastMatch 产品默认参数。

## 2. 类型边界

### GridPatternClassNet

输入：经过明确 ROI 截取的 `cv::Mat`。

输出：

- `GridFeatureMap`：单元位置、前景占比、灰度均值/方差、边缘密度、主方向、方向直方图和 active 状态。
- `GridPatternHierarchy`：3 至 5 层节点、父子索引和可供 mlpack/ensmallen 消费的确定性描述子。
- `GridClassResult`：类别、top-k、类别分数、margin、拒识分数、异常标记和运行时间。
- 网格方向 overlay：灰色/蓝色单元框与黄色方向线，用于复现旧网格调试语义。

### FastMatchGridClassAdapter

只消费 FastMatch 事实结果和 `GridClassResult`，不持有或调用 FastMatch 对象。

显式模式：

- `structural_only`
- `grid_class_only`
- `fastmatch_then_grid_class`
- `score_fusion`

统一证据链：

```text
edge_or_region
-> fastmatch_candidate
-> normalized_grid
-> hierarchy_activation
-> class_match
-> review_signal
```

## 3. mlpack / ensmallen 消费合同

`CxCoreAiBoundary` 将网格层级描述子暴露为：

```text
name   = grid_pattern_hierarchy_descriptor
role   = class_hierarchy_primary
source = GridPatternClassNet
```

FastMatch 仍使用：

```text
name   = fastmatch_structural_feature
role   = structural_primary
source = FastMatch
```

两个向量在 `MakeFastMatchGridClassEnvelope()` 中并列。优化层只能优化类别权重、层权重和拒识阈值，不得反向修改 FastMatch 固化参数。

## 4. 第一阶段验证

独立目标：

```text
cximage_grid_pattern_class_net_test
```

覆盖：

- 同图重复构建的描述子完全一致。
- 默认层级为 `12x12 -> 6x6 -> 3x3`，父子节点映射存在。
- A/B 合成字形类别原型可构建并可区分。
- 网格与方向 overlay 可生成。
- FastMatch cascade 结果保留类别事实和完整证据阶段。

本文件落地时只完成代码与测试目标接入。按照项目编译约束，尚未由人工指定构建目录执行编译，因此当前结论为：

```text
COMPILE_NOT_RUN
HEADLESS_EXECUTION_NOT_RUN
MANUAL_GUI_NOT_RUN
```

## 5. 后续推进顺序

1. 人工指定 canonical build 后编译 `cximage_grid_pattern_class_net_test` 和 `cxvision_imgui_acceptance`。
2. 先运行独立确定性测试，不进入 FastMatch 产品路径。
3. 增加真实字符、手写字和工业符号样本，分开统计 top-1、top-k、拒识率和混淆矩阵。
4. 将 FastMatch 输出 ROI 做角度/尺度归一化后再送入类别网络，分别记录归一化前后结果。
5. 将网格、方向、层级激活、候选框和类别结果接入统一 evidence package。
6. 经 L1/L2/L3 和人工观察后，才允许评估是否增加显式 cxscript 方法。
