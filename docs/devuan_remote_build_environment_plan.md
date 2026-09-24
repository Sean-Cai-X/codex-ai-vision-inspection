# Devuan 远端编译环境计划（待人工安装）

远端网关：`192.168.9.100:18080`。代码目录：

`/media/devuan/249C15F29C15BF6C/Codex-WorkDir/Sean_WorkDir/codex-ai-vision/codex-ai-vision-inspection`

编译环境目录：

`/media/devuan/249C15F29C15BF6C/Codex-WorkDir/Sean_WorkDir/codex-ai-vision`

## 当前 Win11 对齐基线

| 项目 | 当前值 | Linux 目标 |
| --- | --- | --- |
| LibTorch | `2.2.2+cu121` | 相同 PyTorch/LibTorch 版本与 CUDA variant |
| CUDA Toolkit | `12.1.66` | CUDA 12.1 runtime/toolkit；先以 CPU build 验证，再启用 CUDA |
| C++ 标准 | C++17 | C++17 |
| OpenCV | 工程使用 OpenCV 4.x CMake package | `libopencv-dev`，确认实际版本与 ABI |
| 构建系统 | CMake 3.21+ | CMake 3.21+、Ninja、GCC/G++ |

Linux 与 Windows **不能**互移编译出的 DLL/SO 或可执行文件。可在两端交接的是：模型权重/候选 artifact（需加载测试）、模型与数据集 SHA256、manifest、训练曲线和 receipt。每个 artifact 必须在目标端重新执行加载与推理验收。

## 生成的命令

见 `tools/linux/devuan_libtorch_compatibility_plan.sh`。它只打印以下待人工批准的命令：

1. 无修改的 OS、磁盘、编译器、CUDA/GPU、OpenCV 与目录盘点；
2. 带 SOCKS5h 代理变量的 apt 安装命令；
3. LibTorch `2.2.2+cu121` 下载、SHA256 人工核验、解压命令；
4. CPU-first 的 CMake/Ninja/CTest 命令；当前工程仍要求安装 CUDA Toolkit 12.1 才能完成 CMake configure。

脚本不执行安装、下载、解压、构建或测试。下载前必须从 PyTorch 官方发布页/发布校验信息人工确认 archive SHA256；不得因代理可用而跳过完整性验证。

## 建议安装顺序

1. 运行只读 preflight，记录 Devuan 版本、glibc、GCC、磁盘容量和 NVIDIA 驱动/CUDA 版本。
2. 人工确认 apt 软件源、代理可用性和足够空间后安装基础工具链与 OpenCV。
3. 下载并校验 LibTorch；保留 archive 和 SHA256 记录。
4. 先构建 CPU 目标，完成 CTest。
5. 仅在远端驱动兼容 CUDA 12.1 且 CPU 结果通过后，配置 CUDA build。
6. 用候选模型在 Linux 和 Win11 分别进行加载/推理一致性测试；比较 typed receipt，不比较二进制文件。

## 当前远端已知状态

网关运行 Devuan/Linux，远程命令能力标记为 enabled，但公开路由没有暴露可解析的只读 shell 工具配置。因此本次只生成计划，未对远端执行下载、安装或写入操作。
