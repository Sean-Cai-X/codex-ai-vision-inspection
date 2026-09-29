# Windows / Linux Evidence 资产布局

Linux 候选程序的源码目录是 `.../Sean_WorkDir/codex-ai-vision/codex-ai-vision-inspection`。案例、图像、标注工作态和训练输出不放入该 Git 目录，也不推送 GitHub。

启动脚本将宿主机 `.../Codex-WorkDir` 私有挂载到隔离环境的 `/codex-data`，保留 Windows `D:/Codex-WorkDir` 后面的相对路径。例如：

| Windows | Linux 宿主机 | 隔离程序内 |
| --- | --- | --- |
| `D:/Codex-WorkDir/Sean_WorkDir/images/...` | `.../Codex-WorkDir/Sean_WorkDir/images/...` | `/codex-data/Sean_WorkDir/images/...` |
| `D:/Codex-WorkDir/Sean_WorkDir/codex-ai-vision/cxscript_runs/...` | `.../Codex-WorkDir/Sean_WorkDir/codex-ai-vision/cxscript_runs/...` | `/codex-data/Sean_WorkDir/codex-ai-vision/cxscript_runs/...` |
| `D:/Codex-WorkDir/Sean_WorkDir/analysis_workspace/Mpic/...` | `.../Codex-WorkDir/Sean_WorkDir/analysis_workspace/Mpic/...` | `/codex-data/Sean_WorkDir/analysis_workspace/Mpic/...` |

启动脚本设置 `CXVISION_WINDOWS_CODEX_ROOT`、`CXVISION_EVIDENCE_ROOT` 和 `CXVISION_RUN_ROOT`。Evidence 脚本与图片绑定使用外部 `cxvision_repo/.../evidence`；打包案例由外部 `codex-ai-vision/cxscript_runs/_shared/evidence_case_roots.json` 登记。GUI 的 **Reload Evidence Assets** 重扫案例和缩略图，不修改标注。

私有传输至少应覆盖 Windows 上的 `Sean_WorkDir/images`、`Sean_WorkDir/cxvisionai/evidence_images`、`Sean_WorkDir/cxvisionai/evidence_assets`、`Sean_WorkDir/codex-ai-vision/cxscript_runs`，以及需要的 `Sean_WorkDir/analysis_workspace/Mpic`。不要将图片或案例包复制进源码目录。传输前后按相对路径核对文件数量与 SHA-256；缺失文件不能用空白图或生成图替代。

验收分三层：① Evidence 脚本/打包案例索引数量与 Windows 一致；② 每个案例引用的图片实际可读，缩略图不是 `NO IMG`；③ Image View 中 Seg +/-、Auto Boundary、Magic Wand Boundary、Accept Boundary 及对应参数面板可操作并能显示真实边界。仅编译通过或只出现案例名称，不等于图像与标注链路通过。

2026-09-29 私有同步记录：源 `codex-ai-vision/cxscript_runs` 全量约 70.8 GB / 270159 文件，但当前 Evidence 登记表只包含 `mpic_open_boundary`。已通过一次性局域网 HTTPS 传入该目录与登记表，共 117 文件，其中 5 个 `case_manifest.json`；压缩包 SHA-256 与逐文件解包 SHA-256 均核对通过。回执位于项目外 `cxscript_runs/_shared/mpic_private_transfer_receipt.json`。其余历史运行目录未同步；HD 绑定表的其他原图仍须另行传入，不能将本次 Mpic 验收解释为全部 Evidence 图片就绪。
