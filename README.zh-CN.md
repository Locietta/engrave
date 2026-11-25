<div align="center">
  <picture>
    <source
      media="(prefers-color-scheme: dark)"
      srcset="https://raw.githubusercontent.com/engrave-music/.github/main/assets/logo/engrave-music-dark-framed.svg"
    />
    <source
      media="(prefers-color-scheme: light)"
      srcset="https://raw.githubusercontent.com/engrave-music/.github/main/assets/logo/engrave-music-light-framed.svg"
    />
    <img
      alt="engrave"
      src="https://raw.githubusercontent.com/engrave-music/.github/main/assets/logo/engrave-music-light-framed.svg"
      width="128"
    />
  </picture>

  <h1>engrave</h1>

  <p>原生、无头的 C++23 音乐制谱、渲染与音频合成引擎。</p>

  <p>
    <a href="https://github.com/engrave-music/engrave/actions/workflows/ci.yml"><img alt="CI" src="https://github.com/engrave-music/engrave/actions/workflows/ci.yml/badge.svg" /></a>
    <a href="LICENSE.txt"><img alt="License: GPL-3.0-only" src="https://img.shields.io/badge/License-GPL--3.0--only-blue.svg" /></a>
    <img alt="C++23" src="https://img.shields.io/badge/C%2B%2B-23-00599C.svg" />
    <img alt="Bazel" src="https://img.shields.io/badge/Build-Bazel-43A047.svg" />
    <img alt="Platform: Windows | Linux" src="https://img.shields.io/badge/Platform-Windows%20%7C%20Linux-555555.svg" />
  </p>

  <p>
    <a href="README.md">English</a> · <strong>简体中文</strong>
  </p>
</div>

`engrave` 是一个原生、无头的 C++ 乐谱引擎库，提供核心记谱、排版、制谱、渲染、导入/导出与音频合成能力。项目采用 [Bazel](https://bazel.build/) 和隔离的 Clang/`libc++`（`xclang`）工具链构建。

该库作为 [`engrave-mcp`](https://github.com/engrave-music/engrave-mcp) Model Context Protocol（MCP）宿主的核心制谱与回放引擎使用。

## 与 MuseScore 的关系

`engrave` 是 MuseScore 核心的硬分叉（源自上游 MuseScore 5.0 开发分支）。

- **上游基准**：基于上游 MuseScore 提交 `32f0a26e5b`，并将 MuseScore 框架（提交 `da3885cb`）吸收至 `muse/` 目录。后续开发独立演进，上游修复仅通过 cherry-pick 引入。
- 本库移除了桌面界面、交互式控件与云服务。
- **署名与免责声明**：`engrave` 致谢 MuseScore Limited 与 MuseScore 社区的原始版权与贡献。本项目为独立引擎库项目，与 MuseScore 或 Muse Group 无隶属关系，亦未获其背书或维护。

## 支持的格式

可通过 `Engine::capabilities()` 以编程方式查询引擎能力。

### 输入格式

- **MuseScore**：`.mscz`（压缩归档）与 `.mscx`（未压缩 XML），支持 4.60 至 5.00 版本。低于 4.60 的文件（`mscVersion < 460`）返回 `ErrorCode::UNSUPPORTED`；高于 5.00 的版本（`mscVersion > 500`）会被拒绝。
- **MusicXML**：未压缩的 `.musicxml` / `.xml` 以及压缩的 `.mxl`。
- **MIDI**：标准 MIDI 文件（`.mid`、`.midi`、`.kar`）。
- **Guitar Pro**：Guitar Pro 1–7（`.gtp`、`.gp3`、`.gp4`、`.gp5`、`.gpx`、`.gp`、`.gp6`、`.gp7`）与 PowerTab（`.ptb`）。
- **MNX**：W3C MNX JSON（schema 27；公共 API 仅支持导入，详见 [docs/mnx.md](docs/mnx.md)）。

### 导出格式

- **乐谱文件**（`Document::export_score`、`Document::export_score_with_warnings`）：
  - **MuseScore**：`.mscz` 与 `.mscx`（默认原生版本为 `"5.00"`）。同时提供面向 `"4.70"` 版本的有界兼容写入器（可与 MuseScore 4.7.0–4.7.6 互操作；详见 [docs/write470.md](docs/write470.md)）。
  - **MusicXML**：未压缩 MusicXML 与压缩 `.mxl`。
  - **MIDI**：标准 MIDI 文件，支持可配置的反复展开与 RPN 控制。
  - **分谱导出**：通过 `ExportOptions::score` 将单个关联分谱或摘录导出为独立乐谱。
- **视觉渲染**（经由 Cairo 的 `Document::render`）：
  - **PDF**：多页矢量文档。
  - **PNG**：按页输出的高分辨率栅格图像（36 至 1200 DPI；支持透明或乐谱样式背景），启用多线程时支持多页并行 PNG 编码。
  - **SVG**：按页输出的可缩放矢量图形。
  - 排版始终按页进行；读写时会保留原始 `layoutMode` 值。
  - 支持闭区间页码范围选择（`first_page` 至 `last_page`）、协作式取消（`RenderOptions::cancelled`）以及流式字节预算（`RenderOptions::max_bytes`）。
- **音频合成**（经由 FluidSynth 的 `Document::render_audio` 与两阶段 `Document::prepare_audio` + `PreparedAudio::run`）：
  - 编码器：**WAV**（16、24、32 位浮点）、**FLAC**（16、24 位）、**Opus**（Ogg 容器，48 kHz）、**MP3**（Opus 与 MP3 均支持 `bitrate_kbps`，默认 128 kbps，范围 32–512 kbps）。
  - 多轨混音：按音轨控制音量（dB）、立体声声像/平衡、静音、独奏与 SoundFont 选择。
  - 混响：基于 Muse Reverb DSP 的辅助发送/返回混响。
  - 输出路径：`Document::render_audio` 将编码字节分块流式写入调用方提供的 `ByteSink`；`Document::prepare_audio` + `PreparedAudio::run` 将 owner 线程乐谱准备阶段与非 owner 线程直接写盘合成编码阶段解耦。
- **几何与查询**：
  - `Document::outline`、`Document::query` 与 `Document::get`：通过稳定的强类型句柄结构化查询乐谱元数据、元素与属性。
  - `Document::read_music`：按小节窗口读回结构化音乐内容（音符、节奏、拍号调号、表情记号与跨度记号）。
  - `Document::positions`：以精确的 1200-DPI 排版单位输出页面局部小节/段包围盒，以及反复展开后的回放事件位置。

## 构建

`engrave` 在 Windows 与 Linux 上均使用 [Bazel](https://bazel.build/)（版本固定于 `.bazelversion`）及隔离的 Clang 23 / `libc++`（`xclang`）C++23 工具链构建。`xclang` 模块元数据（`MODULE.bazel` 与 SHA-256 固定的 `source.json`）已直接提交在 `bazel/registry` 本地注册表覆盖层中，优先于 Bazel Central Registry 解析；请保持独立仓库路径不含空格，以便 Bazel 正确解析 `file:///%workspace%/bazel/registry`。

### 环境要求

- **Bazelisk**（或与 `.bazelversion` 匹配的 Bazel）
- **Python 3** 与 **Git**（用于资源安装与清单校验）
- **[Pixi](https://pixi.sh/)**：Cairo、FreeType 与 zlib 使用固定在 `pixi.toml` / `pixi.lock` 中的 conda-forge 构建。运行一次 `pixi install --locked` 后，`bazel/deps.bzl` 会将它们暴露为 `@pixi_deps//:cairo`、`@pixi_deps//:freetype` 与 `@pixi_deps//:zlib`。通过 Bzlmod 引入 `@engrave` 的宿主项目可通过 `--repo_env=ENGRAVE_PIXI_PREFIX=...` 指定自身的 Pixi 前缀。Windows 二进制文件运行时需要将 `.pixi/envs/default/Library/bin` 加入 `PATH`；Linux 二进制文件从 `.pixi/envs/default/lib` 加载动态库。

其余 C/C++ 第三方依赖（HarfBuzz、libflac、LAME、libopus、libopusenc、pugixml、utfcpp、glaze、proxy、amc、unordered_dense、gtl、GoogleTest）均在 `MODULE.bazel`（`bazel/deps.bzl`）中通过哈希固定的归档拉取，并由 `bazel/third_party/*.BUILD.bazel` 构建。`glaze` 是引擎唯一的 JSON 库。

### 构建命令（Windows 与 Linux）

```bash
pixi install --locked
bazelisk build --config=release //:engrave
```

### 配置标志

- `--config=<release|debug|releasedbg|asan|tsan>`：选择优化/调试/消毒器配置（定义于 `bazel/engrave.bazelrc`）。
- `--//bazel:pch=false`：为 `//:engraving` 禁用二进制预编译头（默认启用）。
- `--//bazel:playback_harness=true`：启用可选的回放采集插桩（`ENGRAVE_PLAYBACK_HARNESS`）。

`asan` 配置在 Linux 与 Windows x86-64 上均启用 ASan 与致命 UBSan；Linux x86-64 上 ASan 会同时启用 LSan，Windows x86-64 不支持 LSan。`tsan` 配置仅在 glibc Linux 与 macOS 上启用 ThreadSanitizer（Windows 目标会拒绝 `--features=tsan`），并与 `asan` 互斥。

## 运行时资源安装

运行时资源（SMuFL 音乐字体、文本字体、乐器模板、制谱样式与 MPE 配置文件）通过 `tools/install_assets.py` 安装。该安装器直接读取已提交的 Git blob，确保在任何本地换行符配置下都保留精确字节。

```bash
# 安装资源到目标目录
python tools/install_assets.py <destination_dir>

# 按固定清单（engrave/assets.json）校验已安装文件
python tools/install_assets.py <destination_dir> --verify
```

初始化引擎时，将 `<destination_dir>` 传入 `EngineOptions::resources`。详见 [docs/assets.md](docs/assets.md)。

## 公共 API

公共头文件位于 `engrave/include/engrave/`：

- `engrave/engrave.h`：汇总头文件。
- `engrave/engine.h`：`Engine` 运行时生命周期、字体注册、SoundFont 配置，以及文档创建/打开。
- `engrave/document.h`：`Document` 操作：排版、编辑（`edit`、`write_phrase`）、原子回滚与命名内存快照（`EditMode::Atomic`、`checkpoint`、`restore_checkpoint`、`drop_checkpoint`）、渲染、音频合成（`render_audio`、`prepare_audio`）、导出、位置数据、音乐读回（`read_music`）与查询（`outline`、`query`、`get`）。
- `engrave/output.h`：导出与渲染选项（`ExportOptions`、`RenderOptions`、格式定义）。
- `engrave/audio.h`：音频渲染选项、混音器参数、音轨选择、格式设置与两阶段渲染句柄（`AudioOptions`、`AudioFormat`、`AudioRenderResult`、`ByteSink`、`PreparedAudio`）。
- `engrave/edit.h`：批量乐谱编辑操作变体与结果类型（`Op`、`Batch`、`OpResult`、`BatchRejection`、`EditResult`），用于原子或前缀批量执行。
- `engrave/ops/*.h`：强类型批量编辑操作分组（`measures.h`、`notes.h`、`parts.h`、`property.h`、`spanners.h`、`text.h`）。
- `engrave/query.h`：乐谱结构查询、元素句柄与属性查询。
- `engrave/music.h`：结构化音乐窗口（音符、记号、跨度记号）与乐句录入类型（`MusicRange`、`MusicWindow`、`MeasureMusic`、`Phrase`、`PhraseLane`）。
- `engrave/handle.h`：文档作用域 `ElementId`、乐谱/排版地址（`ScoreSel`、`SegmentAddress`、`PageAddress`、`SystemAddress`、`SpannerSegmentAddress`）与编辑引用（`OpRef`、`MeasureIndex`、`StaffIndex`、`RelativePosition`、`Ref`、`EditAddress`、`EntryAddress`）。
- `engrave/value.h`：强类型属性与样式值（`Value`、`ValueKind`、`Unit`、`Fraction`、`Point`、`Size`、`Color`、`Enum`、`Transposition`）。
- `engrave/describe.h`：元素类型、属性与样式目录描述符（`PropInfo`、`TypeInfo`、`StyleInfo`）。
- `engrave/export_warning.h`：兼容导出器发出的结构化警告。
- `engrave/error.h`：错误码（`ErrorCode`）与 `Result<T>` 单子封装。
- `engrave/version.h`：引擎版本信息。
- `engrave/testing/write470.h`：用于 MuseScore 4.70 兼容导出测试的诊断进度探针（`Write470Progress`、`write470_progress()`）。

### 执行模型与生命周期规则

引擎在每个进程内仅在单个专用 owner 线程上访问 DOM，同时支持分离式音频执行与无 DOM 的内部工作线程（见 `engrave/include/engrave/engine.h`、`engrave/include/engrave/audio.h` 与 [docs/playback.md](docs/playback.md)）：

- **每进程单活跃运行时**：当进程内仍有活跃的 `Engine` 或其派生的 `Document` 时，`Engine::create()` 会校验参数并返回 `ErrorCode::UNSUPPORTED`。`Engine` 销毁后，既有 `Document` 仍可在原 owner 线程上继续使用。当 `Engine` 及其最后一个 `Document` 全部销毁后，进程可重新创建新的 `Engine`。初始化失败同样允许重试。
- **Owner 线程亲和性**：所有 `Engine` 与 `Document` 调用（包括 `Document::prepare_audio()`）都必须发生在创建引擎的同一线程上；跨线程调用会直接中止进程。
- **预准备音频与内部工作线程**：`Document::prepare_audio()` 在 owner 线程上与 `Document` 解耦；`PreparedAudio::run()` 与 `~PreparedAudio()` 可在任意线程上执行，期间 owner 线程可继续处理其他请求。每个 `Engine` 同一时间至多允许一个活跃音频渲染（并发调用 `prepare_audio` 或 `render_audio` 会返回 `ErrorCode::RESOURCE_LIMIT`）。必须在销毁最后一个 `Engine`/`Document` 之前销毁所有 `PreparedAudio`，否则析构会直接中止进程。在渲染内部，`EngineOptions::audio_threads` / `ENGRAVE_AUDIO_THREADS` 会将无 DOM 的按轨合成、异步音频编码与多页 PNG 编码扇出到内部工作线程。

## 测试

### 测试布局

- `tests/integration/`：独立可执行的集成测试脚本（`test_assets.py`、`test_excerpts.py`、`test_engine_audio.py`、`test_roundtrip.py`、`test_archives.py`、`test_export_options.py`、`test_oracle_harness.py`、`test_public_output.py`、`test_upstream_gtest.py`、`test_playback_dod.py`、`test_playback_harness.py`）。直接执行，切勿作为 Python 模块导入。
- `tests/engrave_testing/`：共享测试辅助包（`golden`、`paths`、`public_output`、`dependencies`、`inventory`、`links`、`mscx`、`export_formats` 等）。
- `tests/gate.py`：公共 C++ 边界门禁运行器。
- `tests/test_inventory.py`：面向黄金比对器、清单完整性与基线变异的校验套件。
- `tests/data/`：黄金基线、测试输入与平台允许列表（`tests/data/golden/allow.json`）。

### 构建测试二进制

```bash
bazelisk build --config=release //:engine_tests
```

### 运行公共边界门禁

`tests/gate.py` 会构建并运行公共 API、导入、回放与适配器测试套件，执行资源校验、MNX schema 校验、独立语义检查以及索引化公共输出比对（`gate-inventory.json` 从已校验的平台索引中选取 251 项核心 API 产物）。

```bash
python tools/install_assets.py .runtime/assets
python tests/gate.py --build-dir build/gate --assets .runtime/assets --soundfont /path/to/FluidR3Mono_GM.sf3 --output notes/runs/gate
python tests/test_inventory.py
```

关键选项与环境要求：

- **全新输出目录**：始终在 `notes/runs/<name>` 下指定新的 `--output` 目录（不要使用仅存放运行时资源的 `.runtime/`）。
- **快速模式与完整模式**：`--quick` 会跳过 `engine-audio` 语料检查以加快单步验证；在里程碑与引擎版本推进时请不带 `--quick` 运行完整门禁。
- `--no-audio` 运行无需 SoundFont 的门禁，覆盖 25 个测试夹具下的 226 项索引输出。追加 `--goldens-only` 可仅执行清单校验与比对。
- **构建与缓存**：门禁默认基于 `--pixi-manifest`（默认 `pixi.toml`，缺失时通过 `pixi install --locked` 安装）构建，并将其动态库加入运行时搜索路径。`--skip-build` 复用已暂存于 `--build-dir` 中的二进制文件（`engrave-mcp` 封装脚本默认启用该选项）。`--repo-cache` 与 `--disk-cache` 指定 Bazel 缓存目录（默认为 `build/.bazel_repo_cache` 与 `build/.bazel_disk_cache`）。
- **面向 Windows Agent 会话的常驻 Bazel JVM**：可在 IDE/Agent 的 MCP 配置中注册 `tools/bazel_server_mcp.py`（仅依赖标准库的 stdio MCP 服务，暴露 `bazel_server_start` 与 `bazel_server_status`，支持 `--warm` 后台保活及 `--project NAME=PATH` / `--config projects.json` 映射），使 Bazel JVM 在单次命令的 Windows Job Object 之外保持预热。
- **独立 Python 环境**：在仅拉取引擎分叉仓库的检出中，可创建 Python 3.13 虚拟环境并安装固定版本依赖及可编辑模式的 `engrave_testing`：
  ```bash
  python -m venv .venv
  .venv/bin/python -m pip install -r tests/requirements.txt
  .venv/bin/python -m pip install -e .
  ```
  （Windows 路径为 `.venv/Scripts/python.exe`）。所有规范化与校验依赖均与宿主 `pixi.lock` 固定版本一致（`jsonschema 4.26.0`、`numpy 2.5.4`、`soundfile 0.14.0`、`Pillow 13.0.0`、`PyMuPDF 2.0`、`psutil 7.2.2`）；门禁会拒绝版本不匹配的环境（PyMuPDF 2.1 会改变规范化后的 PDF 字节）。Linux 环境下还需要安装系统 DejaVu 字体（例如 `fonts-dejavu-core`）；Cairo、FreeType 与 zlib 均由 Pixi 提供。

### 直接运行单个分叉测试脚本

通过 `pip install -e .` 安装 `engrave-testing` 后可直接运行脚本；若未进行可编辑安装，则需将 `tests` 与 `tools` 前置到 `PYTHONPATH`：

```bash
PYTHONPATH="tests:tools${PYTHONPATH:+:$PYTHONPATH}" python tests/integration/test_assets.py --binary build/gate/assets_test --assets .runtime/assets --font /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf --soundfont /path/to/FluidR3Mono_GM.sf3 --output notes/runs/test-assets
```

```powershell
$env:PYTHONPATH = "tests;tools;$env:PYTHONPATH"
python tests/integration/test_assets.py --binary build/gate/assets_test.exe --assets .runtime/assets --font C:/Windows/Fonts/arial.ttf --soundfont /path/to/FluidR3Mono_GM.sf3 --output notes/runs/test-assets
```

### 持续集成

`.github/workflows/ci.yml` 在推送到 `main`、发起 Pull Request 或手动触发时运行：

- **`python-checks`**：安装 `tests/requirements.txt` 与 `pip install -e .`，随后运行 `tools/test_install_assets.py`、`tools/test_gen_pid_support.py`、`tools/gen_pid_support.py --check`、`tools/test_bazel_server_mcp.py`、`tools/test_prune_bazel_cache.py`、`tests/test_inventory.py`、`tools/mnx_codegen.py --check` 以及 `tests/test_playback_witness.py`（同时以 `python -O` 运行）。
- **`build-test`**：在 Windows 与 Linux 上使用 Bazel、`xclang` 与 Pixi 提供的 Cairo/FreeType/zlib 构建 release 单元测试与 `example_embedder`，校验运行时资源后运行 `engrave_core_tests`、`engrave_output_test`（使用 `tests/data/engine_audio/sine.sf2` 与生成的第二个正弦 SoundFont）、MNX schema 检查、两套上游 GTest 套件以及无 SoundFont 黄金比对。
- **`asan`**：在 Linux 上启用 ASan、LSan 与 UBSan（`--config=asan`），运行 4 个分片的 `engrave_core_tests`、2 个分片的 `engrave_output_test`（使用两份正弦 SoundFont）、两套上游 GTest 套件及无 SoundFont 黄金探针。
- **`tsan`**：在 Linux 上启用 ThreadSanitizer（`--config=tsan`），运行 2 个分片的 `engrave_output_test`（配合两份正弦 SoundFont 与 `engrave_output_probe`），覆盖并行音频合成、异步编码、后台 `PreparedAudio::run` 与多页 PNG 编码。各消毒器任务均使用与 release 分离的磁盘缓存键。
- 包含音频黄金与音频语料的完整门禁作为里程碑手动检查项运行。

## 文档与代码风格工具

- [docs/code-style-guide.md](docs/code-style-guide.md)：代码风格、命名规范与贡献指南。
- [docs/playback.md](docs/playback.md)：离线音频渲染管线、合成架构与协作式取消契约。
- [docs/playback-harness.md](docs/playback-harness.md)：可选的回放标识、计时与内存分配计数测试框架（`ENGRAVE_PLAYBACK_HARNESS`）。
- [docs/mnx.md](docs/mnx.md)：引擎自有的 MNX 文档模型与 schema 差异说明。
- [docs/write470.md](docs/write470.md)：有界 MuseScore 4.70 兼容导出契约。
- [docs/assets.md](docs/assets.md)：运行时资源清单与安装说明。

代码风格工具使用 `PATH` 中的 `clang-format` / `clang-tidy` 23.1.4。在外层 `engrave-mcp` 检出中，通过 `pixi run compdb` 生成编译数据库，然后在宿主根目录运行 `pixi run lint -- --engine` 检查引擎。在独立分叉仓库检出中（且仓库根目录或 `build/<profile>/` 下已有 `compile_commands.json` 时），可在分叉根目录运行：

```bash
python tools/format.py --check .
python tools/lint.py .
python tools/setup_hooks.py
```

安装后的 pre-commit 钩子仅检查暂存区的 blob，不会修改索引。命名静态检查针对已编译的自有翻译单元运行，拒绝空选集与 `--fix`。第三方目录会被自动排除。

## 许可证

`engrave` 是基于 **GNU General Public License version 3.0（GPLv3）** 发布的自由软件。详见 [LICENSE.txt](LICENSE.txt)。

随附字体家族与第三方组件按各自许可条款授权（包括带嵌入例外的 GNU FreeFont、带字体例外的 MScore、SIL Open Font License 1.1、LGPL 2.1、MIT 与 BSD）。完整的署名与许可证清单见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
