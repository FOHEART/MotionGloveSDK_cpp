@echo off
REM ============================================================================
REM  add_firewall_rule.bat  --  为 MotionGloveSDK 接收端放行 Windows 防火墙入站 UDP
REM
REM  用法（必须以管理员身份运行）：
REM    tools\add_firewall_rule.bat              放行 UDP 5001（仅限本地子网）
REM    tools\add_firewall_rule.bat 5009         指定端口
REM    tools\add_firewall_rule.bat 5001 any     允许任意来源（默认只允许本地子网）
REM
REM  说明：Wireshark（Npcap）在防火墙之前抓包，所以"抓包看得到、程序收不到"
REM        的常见原因就是缺少入站规则。删除规则：
REM        netsh advfirewall firewall delete rule name="MotionGloveSDK UDP <端口>"
REM ============================================================================
setlocal
chcp 65001 >nul

set "PORT=%~1"
if "%PORT%"=="" set "PORT=5001"
set "SCOPE=%~2"
if /I "%SCOPE%"=="any" (
    set "REMOTEIP=any"
) else (
    set "REMOTEIP=LocalSubnet"
)

net session >nul 2>&1
if errorlevel 1 (
    echo [ERROR] 需要管理员权限：请右键选择“以管理员身份运行”，或在管理员命令提示符中执行。
    exit /b 1
)

set "RULE=MotionGloveSDK UDP %PORT%"
netsh advfirewall firewall delete rule name="%RULE%" >nul 2>&1
netsh advfirewall firewall add rule name="%RULE%" dir=in action=allow protocol=UDP localport=%PORT% remoteip=%REMOTEIP% profile=any
if errorlevel 1 (
    echo [FAILED] 添加入站规则失败
    exit /b 1
)

echo [OK] 已放行入站 UDP %PORT%（来源范围：%REMOTEIP%）
echo      规则名：%RULE%
exit /b 0
