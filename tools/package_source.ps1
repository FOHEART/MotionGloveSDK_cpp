<#
.SYNOPSIS
    把当前工程打成可分发给客户的源码压缩包。

.DESCRIPTION
    产出的压缩包只包含“能编译 + 能看文档”的内容，剥离内部资料与版本控制信息：
      * 不包含 .git —— 客户无法从包里取到仓库地址、历史与远端配置；
      * 不包含 .github（CI 流程与内部 agent 配置）、openspec、docs/superpowers 等内部资料；
      * 不包含 git 操作脚本（[Windows]git_pull_latest.cmd / [Linux]git_pull_latest.sh），
        客户无法通过脚本拉取你的代码；
      * 不包含构建缓存（out 下的 build/package 等）、third_party、docs/html、*.zip、*.log。

    压缩包内有一层顶层目录 MotionGloveSDK_cpp_v<版本号>，解压后不会散落在当前目录。
    版本号取自 MotionGloveSDK_examples/inc/motionGloveSDK_version.h。

    若已执行过一键构建脚本，还会把预编译产物一并放进包内的 preBuild 目录，
    客户解压即得可直接运行的示例，无需自己再调 VTK / 工具链：
      preBuild\install\           <- out\install（SDK 的 dll/lib/头文件，按 x64-Debug / x64-Release 分目录）
      preBuild\install_example\   <- out\install_example（示例 exe + 运行库，按配置分目录）

.PARAMETER OutputDir
    压缩包输出目录，默认 out\pack（仓库根目录只留源码）。

.PARAMETER Version
    覆盖版本号（默认读取版本头文件）。

.PARAMETER SkipPreBuild
    不打包 out\install 与 out\install_example（只出纯源码包）。

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\package_source.ps1

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\package_source.ps1 -Version 0.2.2 -SkipPreBuild
#>
[CmdletBinding()]
param(
    [string]$OutputDir = '',
    [string]$Version = '',
    [switch]$SkipPreBuild
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$packageBaseName = 'MotionGloveSDK_cpp'

if (-not $Version) {
    $versionHeader = Join-Path $repoRoot 'MotionGloveSDK_examples/inc/motionGloveSDK_version.h'
    $text = Get-Content -LiteralPath $versionHeader -Raw
    $parts = foreach ($part in 'MAJOR', 'MINOR', 'PATCH') {
        if ($text -notmatch "#define\s+MOTIONGLOVE_EXAMPLE_VERSION_$part\s+(\d+)") {
            throw "无法从 $versionHeader 解析 MOTIONGLOVE_EXAMPLE_VERSION_$part"
        }
        $Matches[1]
    }
    $Version = $parts -join '.'
}

if (-not $OutputDir) { $OutputDir = Join-Path $repoRoot 'out/pack' }
if (-not (Test-Path -LiteralPath $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}
$OutputDir = (Resolve-Path -LiteralPath $OutputDir).Path

$packageName = "${packageBaseName}_v$Version"
$stageRoot = Join-Path $repoRoot 'out/pack/staging'
$stageDir = Join-Path $stageRoot $packageName
$zipPath = Join-Path $OutputDir "$packageName.zip"

# 不纳入分发包的目录 / 文件（robocopy 的 /XD、/XF 对任意层级生效）
$excludeDirs = @(
    '.git', '.github', '.vs', '.vscode',
    'openspec', 'superpowers', '.superpowers', '.workbuddy',
    'out', 'build', 'third_party', (Join-Path $repoRoot 'docs/html')
)
$excludeFiles = @(
    '.gitignore', '.gitattributes',
    '*git_pull_latest*', 'autotools_env.bat', '*package_source*',
    'doxygen_warnings.log', '*.log', '*.zip', '*.7z', '*.user', 'Thumbs.db', '.DS_Store'
)

Write-Host "[1/5] 准备暂存目录：$stageDir"
if (Test-Path -LiteralPath $stageDir) { Remove-Item -LiteralPath $stageDir -Recurse -Force }
New-Item -ItemType Directory -Path $stageDir -Force | Out-Null

$robocopyArgs = @(
    $repoRoot, $stageDir, '/E', '/NFL', '/NDL', '/NJH', '/NJS', '/NP',
    '/XD'
) + $excludeDirs + @('/XF') + $excludeFiles
& robocopy @robocopyArgs | Out-Null
# robocopy 用 0~7 表示成功（存在差异/复制完成），>= 8 才是失败
if ($LASTEXITCODE -ge 8) { throw "复制工程文件失败（robocopy 退出码 $LASTEXITCODE）" }

Write-Host '[2/5] 复制预编译产物到包内 preBuild（install / install_example）'
$preBuildRoot = Join-Path $stageDir 'preBuild'
$preBuildItems = @(
    @{ Name = 'install';         Source = Join-Path $repoRoot 'out/install' }
    @{ Name = 'install_example'; Source = Join-Path $repoRoot 'out/install_example' }
)
$preBuildCopied = @()
if ($SkipPreBuild) {
    Write-Host '  已指定 -SkipPreBuild，跳过预编译产物'
} else {
    foreach ($item in $preBuildItems) {
        if (-not (Test-Path -LiteralPath $item.Source)) {
            Write-Host "  [WARN] 未找到 $($item.Source)，包内不含 preBuild\$($item.Name)" -ForegroundColor Yellow
            Write-Host "         需要时先运行 [Windows]build_vs2022.bat 生成，再用 -SkipPreBuild 可跳过本步" -ForegroundColor Yellow
            continue
        }
        $dst = Join-Path $preBuildRoot $item.Name
        New-Item -ItemType Directory -Path $dst -Force | Out-Null
        & robocopy $item.Source $dst /E /NFL /NDL /NJH /NJS /NP | Out-Null
        if ($LASTEXITCODE -ge 8) { throw "复制 $($item.Source) 失败（robocopy 退出码 $LASTEXITCODE）" }
        $stat = Get-ChildItem -LiteralPath $dst -Recurse -File | Measure-Object -Property Length -Sum
        $sizeMb = [math]::Round($stat.Sum / 1MB, 1)
        Write-Host ("  preBuild\{0}：{1} 个文件，{2} MB" -f $item.Name, $stat.Count, $sizeMb)
        $preBuildCopied += $item.Name
    }
    # 两个来源都不存在时不留空目录
    if ($preBuildCopied.Count -eq 0 -and (Test-Path -LiteralPath $preBuildRoot)) {
        Remove-Item -LiteralPath $preBuildRoot -Recurse -Force
    }
}

Write-Host '[3/5] 清理 README 中仅面向内部的说明'
$readmePath = Join-Path $stageDir 'README.md'
if (Test-Path -LiteralPath $readmePath) {
    $readmeLines = @(Get-Content -LiteralPath $readmePath -Encoding UTF8)
    # 1) 去掉列在目录树里的 git 拉取脚本
    $keptLines = @($readmeLines | Where-Object { $_ -notmatch 'git_pull_latest' })

    # 2) 去掉仅面向内部的整节（发布流程 / 给客户打包的说明）
    $internalSections = @('^##\s*发布', '^##\s*源码分发包')
    foreach ($sectionPattern in $internalSections) {
        $startIdx = -1
        for ($i = 0; $i -lt $keptLines.Count; $i++) {
            if ($keptLines[$i] -match $sectionPattern) { $startIdx = $i; break }
        }
        if ($startIdx -lt 0) { continue }

        $endIdx = $keptLines.Count
        for ($j = $startIdx + 1; $j -lt $keptLines.Count; $j++) {
            if ($keptLines[$j] -match '^##\s') { $endIdx = $j; break }
        }
        $keptLines = @($keptLines[0..($startIdx - 1)]) + @($keptLines[$endIdx..($keptLines.Count - 1)])
    }

    if ($keptLines.Count -ne $readmeLines.Count) {
        [System.IO.File]::WriteAllLines($readmePath, $keptLines, (New-Object System.Text.UTF8Encoding($false)))
        Write-Host "  已移除 $($readmeLines.Count - $keptLines.Count) 行内部说明"
    }
}

Write-Host '[4/5] 校验分发包里没有残留 git 相关内容'
$leaks = @()
$leaks += Get-ChildItem -LiteralPath $stageDir -Recurse -Force -Directory -Filter '.git*' -ErrorAction SilentlyContinue
$leaks += Get-ChildItem -LiteralPath $stageDir -Recurse -Force -File -Filter '*git*' -ErrorAction SilentlyContinue
$scriptPatterns = @(
    (Join-Path $stageDir '*.bat'),
    (Join-Path $stageDir '*.cmd'),
    (Join-Path $stageDir '*.sh'),
    (Join-Path $stageDir 'tools/*.bat'),
    (Join-Path $stageDir 'tools/*.ps1'),
    (Join-Path $stageDir 'tools/*.sh')
)
$gitUses = Select-String -Path $scriptPatterns -Pattern '\bgit(\.exe)?\s+(fetch|pull|clone|push|remote|checkout|submodule)\b' -ErrorAction SilentlyContinue
$readmeMentions = Select-String -LiteralPath $readmePath -Encoding UTF8 -Pattern 'git_pull_latest|git clone|git pull|git tag|源码分发包|package_source|\.github/' -ErrorAction SilentlyContinue
if ($leaks.Count -gt 0 -or $gitUses -or $readmeMentions) {
    $leaks | ForEach-Object { Write-Host "  残留：$($_.FullName)" }
    $gitUses | ForEach-Object { Write-Host "  残留：$($_.Filename):$($_.LineNumber)" }
    $readmeMentions | ForEach-Object { Write-Host "  残留：README.md:$($_.LineNumber)" }
    throw '分发包里仍有 git 相关内容，请检查排除规则'
}
Write-Host '  未发现 .git 目录、git 操作脚本、脚本内的 git 命令或相关说明'

Write-Host "[5/5] 压缩为 $zipPath"
if (Test-Path -LiteralPath $zipPath) { Remove-Item -LiteralPath $zipPath -Force }
$sevenZip = (Get-Command 7z -ErrorAction SilentlyContinue).Source
if (-not $sevenZip) { $sevenZip = Join-Path $env:ProgramFiles '7-Zip\7z.exe' }
if (-not (Test-Path $sevenZip)) { $sevenZip = Join-Path ${env:ProgramFiles(x86)} '7-Zip\7z.exe' }
# 工程内的离线工具箱（含预编译运行库时压缩量较大，7za 比 Compress-Archive 快得多）
if (-not (Test-Path $sevenZip)) { $sevenZip = 'D:\autotools\7z\7za.exe' }
if (Test-Path $sevenZip) {
    Push-Location $stageRoot
    & $sevenZip a -tzip -mx=5 $zipPath $packageName | Out-Null
    Pop-Location
} else {
    Compress-Archive -Path $stageDir -DestinationPath $zipPath -Force
}
if (-not (Test-Path -LiteralPath $zipPath)) { throw "压缩失败：未生成 $zipPath" }

$fileCount = (Get-ChildItem -LiteralPath $stageDir -Recurse -File | Measure-Object).Count
Remove-Item -LiteralPath $stageDir -Recurse -Force

$sizeMb = [math]::Round((Get-Item -LiteralPath $zipPath).Length / 1MB, 1)
Write-Host "完成：$zipPath"
Write-Host "  版本：v$Version　顶层目录：$packageName　文件数：$fileCount　大小：$sizeMb MB"
if ($preBuildCopied.Count -gt 0) {
    Write-Host "  包内预编译产物：preBuild\$($preBuildCopied -join '、preBuild\')"
}
Write-Host "  已排除目录：$($excludeDirs -join '、')"
Write-Host "  已排除文件：$($excludeFiles -join '、')"
