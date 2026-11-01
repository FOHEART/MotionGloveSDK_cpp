@echo off
REM autotools 环境注入 —— 仅修改当前终端会话的 PATH，不影响系统环境。
REM 用法：在 cmd 中执行  call tools\autotools_env.bat
REM 之后即可直接使用短命令：pandoc / ffmpeg / wkhtmltopdf / wget / aria2c /
REM                        mutool / iperf3 / 7za / doxygen / gallery-dl / python

set "AUTOTOOLS=D:\autotools"
set "PY=C:\python"

set "PATH=%AUTOTOOLS%\Pandoc;%PATH%"
set "PATH=%AUTOTOOLS%\wkhtmltopdf\bin;%PATH%"
set "PATH=%AUTOTOOLS%\ffmpeg\bin;%PATH%"
set "PATH=%AUTOTOOLS%\wget;%PATH%"
set "PATH=%AUTOTOOLS%\aria2-1.37.0-win-64bit-build1;%PATH%"
set "PATH=%AUTOTOOLS%\mupdf;%PATH%"
set "PATH=%AUTOTOOLS%\iperf;%PATH%"
set "PATH=%AUTOTOOLS%\pdftohtml;%PATH%"
set "PATH=%AUTOTOOLS%\7z;%PATH%"
set "PATH=%AUTOTOOLS%\doxygen-1.11.0;%PATH%"
set "PATH=%AUTOTOOLS%\gallery_dl;%PATH%"
set "PATH=%PY%;%PATH%"

echo [autotools] 已注入 PATH（当前会话生效）。可用命令：
echo   pandoc  wkhtmltopdf  wkhtmltoimage  ffmpeg  ffprobe  wget  aria2c
echo   mutool  iperf3  pdftotext  pdftoppm  pdftocairo  pdfimages  pdfinfo
echo   7za  doxygen  gallery-dl  python
echo.
echo [autotools] 注意：D:\autotools\gost\gost.exe 本体缺失，Gost 暂不可用。
