@echo off
REM HLS 综合和测试快速开始脚本 (Windows)
REM 使用方法: 快速开始.bat

echo ==========================================
echo HLS 综合和 PYNQ 测试快速开始
echo ==========================================
echo.

REM 检查 Vitis HLS 是否在 PATH 中
where vitis_hls >nul 2>&1
if %errorlevel% neq 0 (
    echo 错误: 未找到 vitis_hls 命令
    echo 请确保已安装 Vitis HLS 并添加到 PATH
    echo 或者设置 Vitis HLS 的完整路径
    pause
    exit /b 1
)

echo 步骤 1: 运行 HLS 综合...
echo ------------------------
echo 选择要综合的版本:
echo 1) AXI Master 版本 (compute_histogram_axi_master)
echo 2) AXI Stream 版本 (compute_histogram_axi_stream) - 用于 DMA
echo.
set /p choice="请输入选择 (1 或 2): "

if "%choice%"=="1" (
    echo 运行 AXI Master 版本...
    vitis_hls -f run_hls.tcl
    set IP_DIR=..\histogram_hls_ip
) else if "%choice%"=="2" (
    echo 运行 AXI Stream 版本...
    vitis_hls -f run_hls_axi_stream.tcl
    set IP_DIR=..\histogram_hls_ip_axi_stream
) else (
    echo 无效选择，默认使用 AXI Master 版本
    vitis_hls -f run_hls.tcl
    set IP_DIR=..\histogram_hls_ip
)

echo.
echo 步骤 2: 检查 IP 导出结果...
echo ------------------------
if exist "%IP_DIR%" (
    echo ✓ IP 已成功导出到: %IP_DIR%
    dir "%IP_DIR%"
) else (
    echo ✗ IP 导出失败，请检查 HLS 综合日志
    pause
    exit /b 1
)

echo.
echo 步骤 3: 下一步操作...
echo ------------------------
echo 1. 在 Vivado 中创建项目
echo 2. 添加 IP Repository: %IP_DIR%
echo 3. 创建 Block Design 并连接 IP
echo 4. 生成 bitstream
echo 5. 在 Kria 板上运行 PYNQ 测试
echo.
echo 详细说明请参考: HLS_PYNQ_使用指南.md
echo.
pause


