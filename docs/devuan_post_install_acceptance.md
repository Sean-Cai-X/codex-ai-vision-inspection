# Devuan 人工安装后的验收与模型交接

人工执行 `tools/linux/devuan_libtorch_compatibility_plan.sh` 所打印的命令后，填写 `contracts/devuan_build_environment_receipt_template.v1.json` 的副本；不要在回执写入代理凭据。

## 验收顺序

1. 记录 `os-release`、内核、架构、磁盘、GCC/G++、CMake、Ninja、Python、OpenCV、`nvcc` 和 NVIDIA driver。
2. 记录 LibTorch archive SHA256、解压目录和 `TorchConfig.cmake` 存在性，确认版本为 `2.2.2+cu121`。
3. 执行 CPU-first CMake configure、build 和 CTest；将命令退出码与测试汇总写入回执。
4. 仅在 CUDA 12.1/driver 兼容且 CPU 测试通过后，进行 CUDA configure/build。
5. 从 Win11 交接一个候选模型 artifact、model SHA256、manifest 与一张受控 VERIFY asset 的逻辑引用；图片不离开受控资产代理。
6. 两端各自通过本地资产代理执行加载/推理，比较 model SHA256、capability/runtime 版本、typed conclusion、类别 ID 与数值几何摘要。

## 通过标准

- 两端均加载相同的候选模型 digest；不得复制 DLL/SO 或可执行文件。
- 七类 ID 与含义一致，不接受 rectangle 或未知类。
- 模型差异仅通过版本化 receipt 记录；出现数值差异进入 `REVIEW`，不得自动 PASS 或发布。
- 远端构建通过不等于真实七类模型验收；真实业务数据、资产代理和 executor 仍按 RND-VAI-001/002 单独验收。
