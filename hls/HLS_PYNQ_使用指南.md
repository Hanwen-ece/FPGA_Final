# HLS 综合和 PYNQ 测试完整指南

本文档详细介绍如何：
1. 使用 Vitis HLS 进行 IP 综合
2. 在 Vivado 中创建项目并生成 bitstream
3. 在 Kria 板上使用 PYNQ 进行测试

---

## 第一部分：HLS 综合

### 1.1 环境准备

确保已安装：
- **Vitis HLS** (推荐 2020.1 或更新版本)
- **Vivado** (用于生成 bitstream)
- **PYNQ** (在 Kria 板上运行)

### 1.2 使用 TCL 脚本运行 HLS（推荐）

#### 方法 1：直接运行 TCL 脚本

```bash
cd hls
vitis_hls -f run_hls.tcl
```

#### 方法 2：使用 Makefile（Linux）

```bash
cd hls
make all      # 运行完整流程（sim + synth + export）
# 或分步执行：
make sim      # C 仿真
make synth    # C 综合
make export   # 导出 IP
```

#### 方法 3：交互式模式

```bash
cd hls
vitis_hls -i
```

在交互式界面中：
```tcl
source run_hls.tcl
```

### 1.3 HLS 综合步骤详解

#### 步骤 1：C 仿真（C Simulation）
- 验证算法正确性
- 检查输出：`histogram_hls.prj/solution1/csim/build/`

#### 步骤 2：C 综合（C Synthesis）
- 生成 RTL 代码
- 查看报告：`histogram_hls.prj/solution1/syn/report/`
- 检查资源使用情况（LUT、BRAM、DSP）

#### 步骤 3：导出 IP
- IP 导出到：`../histogram_hls_ip/`
- 包含所有必要的文件（`.xci`, RTL, 约束文件等）

### 1.4 修改顶层函数（如果需要测试 AXI Stream 版本）

编辑 `run_hls.tcl`，修改第 6 行：
```tcl
set top_function "compute_histogram_axi_stream"  # 用于 DMA 连接
# 或
set top_function "compute_histogram_axi_master"   # 用于直接内存访问
```

### 1.5 检查综合结果

综合完成后，检查：
- **资源使用**：`histogram_hls.prj/solution1/syn/report/compute_histogram_axi_master_csynth.rpt`
- **时序报告**：查看时钟频率是否满足要求
- **IP 文件**：确认 `../histogram_hls_ip/` 目录已生成

---

## 第二部分：在 Vivado 中创建项目

### 2.1 创建 Vivado 项目

1. 打开 **Vivado**
2. **File → New Project**
3. 选择项目名称和位置
4. 选择项目类型：**RTL Project**
5. 添加源文件：**跳过**（我们将使用 IP）
6. 选择目标设备：
   - 对于 Kria：**xczu5ev-sfvc784-1-i** (Zynq Ultrascale+)
   - 或根据实际硬件选择

### 2.2 添加 HLS IP

1. 在 Vivado 中：**Settings → IP → Repository**
2. 点击 **+** 添加 IP 仓库
3. 选择路径：`../histogram_hls_ip/`（相对于项目根目录）
4. 点击 **Apply** 和 **OK**

### 2.3 创建 Block Design

1. **Flow Navigator → IP Integrator → Create Block Design**
2. 添加以下 IP：
   - **Zynq UltraScale+ PS** (Processing System)
   - **AXI DMA** (如果需要使用 AXI Stream 接口)
   - **我们的 HLS IP**：在 IP Catalog 中搜索 `histogram` 或 `compute_histogram`

### 2.4 连接 IP（使用 AXI Stream + DMA）

#### 连接步骤：

1. **Zynq PS 配置**：
   - 双击 Zynq PS IP
   - 启用 **HP0** 和 **HP1** AXI 接口（用于 DMA）
   - 配置时钟：确保有 100MHz 或更高的时钟

2. **AXI DMA 配置**：
   - 添加 AXI DMA IP
   - 配置为 **Scatter Gather** 模式（如果需要）
   - 或使用 **Simple** 模式

3. **连接 HLS IP**：
   - 添加 `compute_histogram_axi_stream` IP
   - 连接：
     - `s_axis` → AXI DMA 的 `M_AXIS_MM2S`
     - `m_axis` → AXI DMA 的 `S_AXIS_S2MM`
     - `s_axi_control` → Zynq PS 的 `M_AXI_HPM0_LPD`（控制接口）

4. **连接时钟和复位**：
   - 所有 IP 连接到同一个时钟和复位信号
   - 时钟来自 Zynq PS

5. **连接 DMA**：
   - DMA 的 `M_AXI_MM2S` → Zynq PS 的 `S_AXI_HP0`
   - DMA 的 `M_AXI_S2MM` → Zynq PS 的 `S_AXI_HP1`
   - DMA 的 `s_axi_lite` → Zynq PS 的 `M_AXI_HPM0_LPD`

#### Block Design 示意图：

```
Zynq PS
  ├── M_AXI_HPM0_LPD ──┐
  │                    ├── AXI DMA (s_axi_lite)
  │                    └── HLS IP (s_axi_control)
  │
  ├── S_AXI_HP0 ──────── AXI DMA (M_AXI_MM2S)
  └── S_AXI_HP1 ──────── AXI DMA (M_AXI_S2MM)

AXI DMA
  ├── M_AXIS_MM2S ────── HLS IP (s_axis)
  └── S_AXIS_S2MM ────── HLS IP (m_axis)
```

### 2.5 生成 HDL Wrapper

1. 在 **Sources** 窗口中右键点击 Block Design
2. 选择 **Create HDL Wrapper**
3. 选择 **Let Vivado manage wrapper and auto-update**

### 2.6 生成 Bitstream

1. **Flow Navigator → Program and Debug → Generate Bitstream**
2. 等待综合和实现完成（可能需要较长时间）
3. 生成的文件：
   - `*.bit` - bitstream 文件
   - `*.hwh` - 硬件描述文件（PYNQ 需要）

### 2.7 导出硬件

1. **File → Export → Export Hardware**
2. 选择 **Include bitstream**
3. 导出路径：保存到方便访问的位置

---

## 第三部分：在 Kria 板上使用 PYNQ 测试

### 3.1 准备文件

将以下文件传输到 Kria 板：
1. `histogram_pynq.py` - PYNQ 测试脚本
2. `*.bit` 和 `*.hwh` - bitstream 和硬件描述文件

传输方式：
```bash
# 使用 SCP（从开发机）
scp histogram_pynq.py ubuntu@<kria-ip>:/home/ubuntu/
scp *.bit *.hwh ubuntu@<kria-ip>:/home/ubuntu/
```

### 3.2 在 Kria 板上运行

#### 方法 1：使用 Jupyter Notebook（推荐）

1. 在 Kria 板上打开 Jupyter：`http://<kria-ip>:9090`
2. 创建新的 Python Notebook
3. 复制 `histogram_pynq.py` 的内容到 Notebook 中
4. 修改 bitstream 路径：
   ```python
   BITSTREAM_PATH = '/home/ubuntu/histogram_hls.bit'
   ```
5. 运行所有单元格

#### 方法 2：直接运行 Python 脚本

```bash
# SSH 到 Kria 板
ssh ubuntu@<kria-ip>

# 运行脚本
cd /home/ubuntu
python3 histogram_pynq.py
```

### 3.3 修改脚本参数

编辑 `histogram_pynq.py`，根据需要修改：

```python
# 在 main() 函数调用中修改参数
success = main(
    width=3840,              # 图像宽度
    height=2160,             # 图像高度
    bitstream_path='/home/ubuntu/histogram_hls.bit',  # bitstream 路径
    iterations=1000         # 迭代次数
)
```

### 3.4 检查 DMA 名称

如果脚本报错找不到 DMA，检查实际的 DMA 名称：

```python
# 在 main() 函数中添加调试信息
print("Available IPs:", overlay.ip_dict)
print("Available DMAs:", overlay.dma_dict)

# 然后根据实际名称修改
# dma = overlay.axi_dma_0  # 可能需要改为其他名称
```

### 3.5 预期输出

成功运行后，应该看到：
```
=== Histogram Computation using AXI DMA ===
Image size: 3840x2160 (8.29 MP)
Iterations: 1000

Loading overlay...
Overlay loaded successfully!
DMA initialized successfully!

Allocating DMA buffers...
Preparing input data for DMA...

Starting FPGA acceleration (1000 iterations)...
Progress: 1000/1000 (100.0%)

Average FPGA time: 0.xxx seconds (xxx.xxx ms)
Throughput: xxx.xx MPixels/s

=== Verifying Results ===
==================================================
*       TEST PASSED!            *
* Histogram computation correct *
==================================================

=== Performance Comparison ===
CPU time (1 iteration): x.xxxxxx seconds (xxx.xxx ms)
FPGA time (average): x.xxxxxx seconds (xxx.xxx ms)
Speedup: x.xx
```

---

## 第四部分：常见问题排查

### 4.1 HLS 综合问题

**问题 1：资源不足**
- 解决方案：减少 `ARRAY_PARTITION` 的因子
- 例如：将 `complete` 改为 `cyclic factor=8`

**问题 2：时序不满足**
- 解决方案：增加时钟周期（降低频率）
- 例如：将 `create_clock -period 10` 改为 `-period 12`

**问题 3：IP 导出失败**
- 解决方案：检查综合是否成功完成
- 查看日志文件中的错误信息

### 4.2 Vivado 问题

**问题 1：找不到 IP**
- 解决方案：确认 IP Repository 路径正确
- 重新添加 IP Repository 并刷新

**问题 2：连接失败**
- 解决方案：检查接口类型是否匹配
- AXI Stream 只能连接 AXI Stream
- AXI Master 只能连接 AXI Slave

**问题 3：Bitstream 生成失败**
- 解决方案：检查时序约束
- 查看实现报告中的错误

### 4.3 PYNQ 问题

**问题 1：Overlay 加载失败**
- 解决方案：检查 bitstream 和 hwh 文件是否匹配
- 确认文件路径正确

**问题 2：DMA 传输失败**
- 解决方案：检查 DMA 名称是否正确
- 确认缓冲区大小和数据对齐

**问题 3：结果不匹配**
- 解决方案：检查数据格式（uint8 vs uint32）
- 确认图像大小参数正确

---

## 第五部分：性能优化建议

### 5.1 HLS 优化

1. **调整数组分区**：根据资源情况选择合适的分区策略
2. **流水线深度**：增加流水线级数以提高吞吐量
3. **循环展开**：对关键循环进行展开

### 5.2 系统级优化

1. **DMA 传输**：使用 Scatter-Gather 模式处理大数据
2. **双缓冲**：重叠数据传输和计算
3. **时钟频率**：在满足时序的前提下提高时钟频率

---

## 附录：快速参考

### A. 文件路径

```
finalproject/
├── hls/
│   ├── histogram_hls.cpp          # HLS 源代码
│   ├── histogram_hls.h            # 头文件
│   ├── histogram_hls_test.cpp    # 测试平台
│   ├── histogram_pynq.py         # PYNQ 测试脚本
│   ├── run_hls.tcl               # HLS 脚本
│   └── Makefile_hls              # Makefile
│
└── histogram_hls_ip/             # 导出的 IP（HLS 生成）
    └── ...
```

### B. 关键命令

```bash
# HLS 综合
cd hls
vitis_hls -f run_hls.tcl

# PYNQ 测试
python3 histogram_pynq.py
```

### C. 重要参数

- **时钟频率**：100MHz (10ns period)
- **目标设备**：xczu5ev-sfvc784-1-i
- **图像大小**：3840x2160 (默认)
- **迭代次数**：1000 (默认)

---

## 总结

完成以上步骤后，你应该能够：
1. ✅ 使用 Vitis HLS 综合 IP
2. ✅ 在 Vivado 中创建项目并生成 bitstream
3. ✅ 在 Kria 板上使用 PYNQ 进行测试
4. ✅ 验证 FPGA 加速器的正确性和性能

如有问题，请参考各步骤的详细说明或检查错误日志。


