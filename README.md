# 图像直方图计算加速项目

## 项目结构

```
finalproject/
├── histogram_cpu.c          # x86_64 CPU版本
├── histogram_kria_ps.c      # Kria PS (Cortex-A53) CPU版本
├── README_Kria.md           # Kria平台说明
│
├── opencl/                   # OpenCL GPU加速版本
│   ├── histogram_gpu.c      # OpenCL主机代码
│   ├── histogram.cl         # OpenCL kernel代码
│   ├── compile_opencl.bat   # Windows编译脚本
│   ├── compile_opencl.sh    # Linux编译脚本
│   ├── test_opencl.bat      # Windows测试脚本
│   ├── test_opencl.sh       # Linux测试脚本
│   └── README_OpenCL.md     # OpenCL使用说明
│
├── hls/                      # FPGA/PL加速器版本
│   ├── histogram_hls.cpp     # HLS实现代码
│   ├── histogram_hls.h      # HLS头文件
│   ├── histogram_hls_test.cpp  # HLS测试平台
│   ├── histogram_fpga_host.c   # FPGA主机代码
│   ├── run_hls.tcl          # HLS运行脚本
│   ├── Makefile_hls         # HLS Makefile
│   └── README_FPGA.md       # FPGA使用说明
│
└── .vscode/                  # VS Code配置文件
    ├── tasks.json
    ├── launch.json
    └── c_cpp_properties.json
```

## 实现版本

### 1. CPU版本（基准）
- **x86_64**: `histogram_cpu.c` - 纯C实现，用于PC平台
- **ARM Cortex-A53**: `histogram_kria_ps.c` - 适配Kria PS平台

### 2. OpenCL GPU加速版本
- 位置：`opencl/` 文件夹
- 支持GPU和CPU设备
- 包含简单版本和局部内存优化版本

### 3. FPGA/PL加速器版本
- 位置：`hls/` 文件夹
- 使用Vitis HLS实现
- 支持AXI Stream和AXI Master接口
- 包含主机代码

## 快速开始

### CPU版本
```bash
gcc histogram_cpu.c -o histogram_cpu.exe
./histogram_cpu.exe 1920 1080
```

### OpenCL GPU版本
```bash
cd opencl
g++ histogram_gpu.c -lOpenCL -o histogram_gpu.exe
./histogram_gpu.exe 1920 1080
```

### FPGA HLS版本
```bash
cd hls
# 使用Vitis HLS工具
vitis_hls -f run_hls.tcl
```

## 性能对比

运行各版本后，可以对比：
- x86_64 CPU性能
- ARM Cortex-A53性能
- GPU加速性能
- FPGA加速性能

## 编译和运行

详细说明请参考各子文件夹中的README：
- `README_Kria.md` - Kria平台说明
- `opencl/README_OpenCL.md` - OpenCL使用说明
- `hls/README_FPGA.md` - FPGA使用说明

## 开发环境

- VS Code配置文件在 `.vscode/` 文件夹
- 支持一键编译和调试
- 按 `Ctrl+Shift+B` 编译，按 `F5` 调试

## 注意事项

1. OpenCL版本需要OpenCL SDK
2. FPGA HLS版本需要Vitis HLS工具
3. 确保编译前已安装相应依赖



