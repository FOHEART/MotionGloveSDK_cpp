---
name: autotools
description: >-
  本工程专属的离线工具箱指引。当需要文档转换（pandoc / wkhtmltopdf）、音视频处理（ffmpeg）、
  下载（wget / aria2 / gallery-dl）、PDF 处理（mupdf / poppler / doxygen）、网络工具（gost / iperf3）、
  压缩解压（7-Zip）、Python 运行环境、网页爬取（crawl4ai）、Rhino 三维建模（rhinoai）等能力时，
  加载本技能即可获得各工具的精确路径与即用命令模板。
  Use for offline toolchain paths and ready-to-run command templates
  (pandoc, wkhtmltopdf, ffmpeg, mupdf/mutool, poppler, 7-Zip, doxygen, aria2, wget, iperf3).
  工具本体位于 D:\autotools，Python 位于 C:\python。
---

# autotools — 工程内离线工具箱

本技能把 `D:\autotools\autotools.md` 的工具清单落地到当前工程，供按需调用。
所有工具均为绿色版（无需安装），路径已逐一核验。直接以绝对路径调用即可，
也可先运行 `tools/autotools_env.bat`（Windows）把各 bin 目录加入当前会话的 PATH。

> 注意：本机未安装加密软件，新建/落地文件无需任何中转或加密处理，直接落盘即可。

---

## 1. 目录总览

| 工具 | 用途 | 主程序绝对路径 |
| --- | --- | --- |
| pandoc | Markdown ↔ Word/HTML/PDF 等格式转换 | `D:\autotools\Pandoc\pandoc.exe` |
| wkhtmltopdf | HTML → PDF / 图片 | `D:\autotools\wkhtmltopdf\bin\wkhtmltopdf.exe` |
| ffmpeg | 音视频转码、剪辑、探针 | `D:\autotools\ffmpeg\bin\ffmpeg.exe` |
| python | Python 3 运行环境 | `C:\python\python.exe` |
| Gost | 端口转发 / 代理隧道 | `D:\autotools\gost\gost.exe`（见下方说明） |
| wget | 命令行单文件下载 | `D:\autotools\wget\wget.exe` |
| aria2 | 多线程/多协议下载（HTTP/FTP/BT/磁力） | `D:\autotools\aria2-1.37.0-win-64bit-build1\aria2c.exe` |
| MuPDF | PDF 查看与处理（mutool） | `D:\autotools\mupdf\mutool.exe` |
| iperf3 | 网络带宽/性能测试 | `D:\autotools\iperf\iperf3.exe` |
| Poppler | PDF 工具集（pdftotext/pdftoppm/pdftocairo/pdfimages…） | `D:\autotools\pdftohtml\pdftotext.exe` 等 |
| 7-Zip | 命令行压缩/解压（7z/zip/gz/xz/tar；ARM64 见 `7z\arm64\`） | `D:\autotools\7z\7za.exe` |
| Doxygen | 源码文档生成（C/C++/Java/Python 等；附 `doxywizard` / `doxyindexer`） | `D:\autotools\doxygen-1.11.0\doxygen.exe` |
| gallery-dl | 图集下载（独立版 v1.32.18，1000+ 站点，用法见同目录 README.md） | `D:\autotools\gallery_dl\gallery-dl.exe` |
| Crawl4AI | 网页爬取与结构化提取（Crawl4AI v0.7.4+ 技能包） | `D:\autotools\crawl4ai\SKILL.md` |
| RhinoAI | Rhino 8 三维建模 MCP（技能包） | `D:\autotools\rhinoai\SKILL.md` |
| claude-skills | 参考技能包（pdf / pptx / video-downloader） | `D:\autotools\claude-skills\` |

---

## 2. 即用命令模板

### 文档转换
```bat
REM Markdown → Word
D:\autotools\Pandoc\pandoc.exe in.md -o out.docx

REM Markdown → PDF（经 LaTeX，若已装）或经 HTML
D:\autotools\Pandoc\pandoc.exe in.md -o out.pdf

REM HTML → PDF
D:\autotools\wkhtmltopdf\bin\wkhtmltopdf.exe https://example.com out.pdf
D:\autotools\wkhtmltopdf\bin\wkhtmltopdf.exe page.html out.pdf

REM HTML → 图片
D:\autotools\wkhtmltopdf\bin\wkhtmltoimage.exe page.html out.png
```

### 音视频（ffmpeg）
```bat
REM 格式转换
D:\autotools\ffmpeg\bin\ffmpeg.exe -i in.mp4 out.gif

REM 抽取音频
D:\autotools\ffmpeg\bin\ffmpeg.exe -i in.mp4 -vn out.mp3

REM 探针信息
D:\autotools\ffmpeg\bin\ffprobe.exe in.mp4
```

### 下载
```bat
REM 单文件
D:\autotools\wget\wget.exe -O out.zip https://example.com/file.zip

REM 多线程/断点续传
D:\autotools\aria2-1.37.0-win-64bit-build1\aria2c.exe -x 16 -s 16 -c "URL"

REM 图集下载（详见同目录 README.md）
D:\autotools\gallery_dl\gallery-dl.exe "URL"
```

### PDF 处理
```bat
REM 提取文本
D:\autotools\pdftohtml\pdftotext.exe in.pdf out.txt

REM 渲染为图片
D:\autotools\pdftohtml\pdftoppm.exe -png -r 150 in.pdf page

REM 页面信息
D:\autotools\pdftohtml\pdfinfo.exe in.pdf

REM mutool 处理（合并/拆分/清洗）
D:\autotools\mupdf\mutool.exe merge out.pdf 1.pdf 2.pdf
```

### 压缩 / 解压（7-Zip）
```bat
REM 解压
D:\autotools\7z\7za.exe x archive.7z -oC:\out

REM 压缩
D:\autotools\7z\7za.exe a out.7z folder\
```

### 网络
```bat
REM 端口转发（UDP 示例）
D:\autotools\gost\gost.exe -L=udp://:5010/192.168.3.129:5010

REM 带宽测试（服务端 / 客户端）
D:\autotools\iperf\iperf3.exe -s
D:\autotools\iperf\iperf3.exe -c 192.168.x.x
```

### 源码文档（Doxygen）
```bat
REM 1) 生成配置：D:\autotools\doxygen-1.11.0\doxygen.exe -g Doxyfile
REM 2) 编辑 INPUT 等字段后生成：
D:\autotools\doxygen-1.11.0\doxygen.exe Doxyfile

REM 本工程已封装好一键脚本（含零告警验收）
gen_docs.bat
```

### Python
```bat
C:\python\python.exe script.py
```

---

## 3. 技能包（需按目录 README/SKILL 加载）

- **Crawl4AI**：`D:\autotools\crawl4ai\SKILL.md`（网页爬取与结构化提取，基于 Python 库，含 scripts/references/tests）。
- **RhinoAI**：`D:\autotools\rhinoai\SKILL.md`（Rhino 8 三维建模的 MCP 连接 + 技能包，含 mcp/skill/examples/templates）。
- **claude-skills**：`D:\autotools\claude-skills\`（pdf / pptx / video-downloader 参考技能，落地为 .txt 备忘）。
- **skills/script — 模块化 Python 脚本工具箱**：`D:\autotools\skills\script\README.md`。
  三个已实战验证、可直接 CI 化的脚本（纯标准库，退出码 0/1/2）：
  - `gen_docs.py` — Doxygen 生成 + 零告警三查（exit code / 告警日志 0 字节 / HTML 产出）；
  - `ctest_run.py` — CMake 构建 + CTest 一键验证（`--build`、`--filter`、`--repeat` 抖动排查、失败用例汇总）；
  - `doxygen_doc_audit.py` — 注释规范预检（缺 @file、@cond 带文字、@brief_cn、.h/.cpp 重复文档、引用后中文标点、裸宏/typedef；自动豁免 include guard 与 VS 生成文件）。

  典型三连（改完代码后全绿才算完成）：
  ```bat
  python D:\autotools\skills\script\doxygen_doc_audit.py <src目录> && ^
  python D:\autotools\skills\script\ctest_run.py --build-dir out\build\test --build && ^
  python D:\autotools\skills\script\gen_docs.py --project-dir .
  ```

  本工程已提供等价封装：`tools\build_and_test.bat`（构建 + ctest）与 `gen_docs.bat`（文档零告警验收）。

---

## 4. 已知缺口

- **Gost**：`D:\autotools\gost\` 内仅有转发 `.bat` 脚本与文档，**`gost.exe` 本体当前缺失**。
  使用前需先放入 `D:\autotools\gost\gost.exe`，或改用已配置的 `forward_*.bat`（脚本已假定该路径）。
- **Doxygen 搜索工具**：`doxywizard.exe` / `doxyindexer.exe` 可用，**`doxysearch.exe` 缺失**，
  故 `Doxyfile` 中 `SEARCHENGINE` 只影响 HTML 内置搜索，不要依赖外部搜索程序。
- **加密软件**：本机未安装，故新建/落地文件无需加密中转，直接写入即可。

---

## 5. 快速 PATH 注入（可选）

Windows 下运行一次即可在当前终端会话用短命令（如 `pandoc`、`ffmpeg`）：
```bat
call tools\autotools_env.bat
```
（该脚本仅修改当前会话 PATH，不影响系统环境。）
