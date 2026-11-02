<#
.SYNOPSIS
    启动测试：验证 motionGloveSDK_example_vtk 能正常初始化并打开 3D 窗口。

.DESCRIPTION
    1) 执行 `--self-test`：确认 UDP 绑定与 VTK 场景搭建（渲染器/窗口/交互器/
       42 段骨骼 actor/地平面网格/坐标轴）全部成功，退出码为 0；
    2) 正常启动程序，等待标题为 `MotionGloveSDK VTK Example` 的窗口出现，
       出现即认为「能正常启动」，随后结束该进程。

    示例在检测不到 OpenGL 3.2+ 时会以退出码 2 干净退出（而不是崩溃）。
    托管 CI runner 上可能装不上软件 OpenGL，此时用 -AllowNoWindow 把
    「环境不支持 OpenGL」降级为警告：启动路径仍由第 1 步的 self-test 覆盖。
    除该情况外的任何提前退出都视为失败。

.PARAMETER Exe
    示例可执行文件路径。

.PARAMETER TimeoutSeconds
    等待窗口出现的超时时间（秒），默认 30。

.PARAMETER WindowTitle
    期望的窗口标题，默认与示例中的 SetWindowName 一致。

.PARAMETER AllowNoWindow
    允许因「环境不支持 OpenGL 3.2+」而跳过窗口检测（仅此一种情况）。

.EXAMPLE
    ./tools/test_vtk_example_launch.ps1 -Exe out/build/x64-Release/Release/motionGloveSDK_example_vtk.exe

.EXAMPLE
    ./tools/test_vtk_example_launch.ps1 -Exe out/build/ci-vtk/Release/motionGloveSDK_example_vtk.exe -AllowNoWindow
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Exe,
    [int]$TimeoutSeconds = 30,
    [string]$WindowTitle = 'MotionGloveSDK VTK Example',
    [switch]$AllowNoWindow
)

$ErrorActionPreference = 'Stop'

$exePath = (Resolve-Path -LiteralPath $Exe -ErrorAction Stop).Path

Write-Host "[1/2] self-test: $exePath --self-test"
& $exePath --self-test
$selfTestCode = $LASTEXITCODE
if ($selfTestCode -ne 0) {
    throw "self-test 失败，退出码 $selfTestCode"
}

Write-Host "[2/2] 启动程序并等待窗口 '$WindowTitle'（最多 $TimeoutSeconds 秒）..."
$outFile = Join-Path ([System.IO.Path]::GetTempPath()) 'mgsdk-vtk-launch.out'
$errFile = Join-Path ([System.IO.Path]::GetTempPath()) 'mgsdk-vtk-launch.err'
Remove-Item $outFile, $errFile -ErrorAction SilentlyContinue

$proc = Start-Process -FilePath $exePath -PassThru -RedirectStandardOutput $outFile -RedirectStandardError $errFile

$deadline = (Get-Date).AddSeconds($TimeoutSeconds)
$windowFound = $false
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 500
    $proc.Refresh()
    if ($proc.HasExited) { break }
    if ($proc.MainWindowTitle -eq $WindowTitle) { $windowFound = $true; break }
}

$programOutput = @()
if (Test-Path $outFile) { $programOutput += Get-Content -LiteralPath $outFile }
if (Test-Path $errFile) { $programOutput += Get-Content -LiteralPath $errFile }

if ($windowFound) {
    Write-Host "窗口已打开：$($proc.MainWindowTitle)"
    Stop-Process -Id $proc.Id -Force
    Remove-Item $outFile, $errFile -ErrorAction SilentlyContinue
    Write-Host 'VTK 示例启动正常。'
    exit 0
}

$exitedEarly = $proc.HasExited
$exitCode = if ($exitedEarly) { $proc.ExitCode } else { $null }
if (-not $exitedEarly) { Stop-Process -Id $proc.Id -Force }

if ($programOutput.Count -gt 0) {
    Write-Host '--- 程序输出 ---'
    $programOutput | ForEach-Object { Write-Host $_ }
}

if ($exitedEarly) {
    if ($exitCode -eq 2) {
        if ($AllowNoWindow) {
            Write-Warning '环境不支持 OpenGL 3.2+，已跳过窗口检测（初始化路径已由 self-test 验证）。'
            Remove-Item $outFile, $errFile -ErrorAction SilentlyContinue
            exit 0
        }
        throw '环境不支持 OpenGL 3.2+（退出码 2），无法验证窗口；无显卡机器可安装 Mesa 软件 OpenGL。'
    }
    throw "程序在窗口出现前退出（退出码 $exitCode），启动失败。"
}

if ($AllowNoWindow) {
    Write-Warning "在 $TimeoutSeconds 秒内未检测到窗口 '$WindowTitle'；初始化路径已由 self-test 验证。"
    exit 0
}

throw "在 $TimeoutSeconds 秒内未检测到窗口 '$WindowTitle'，启动失败。"
