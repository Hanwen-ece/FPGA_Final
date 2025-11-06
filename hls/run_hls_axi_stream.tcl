# Vitis HLS TCL Script for Histogram Project (AXI Stream Version)
# 使用方法: vitis_hls -f run_hls_axi_stream.tcl
# 用于生成支持 DMA 连接的 IP

# 设置项目名称
set project_name "histogram_hls_axi_stream"
set top_function "compute_histogram_axi_stream"

# 打开或创建项目
if {[file exists $project_name]} {
    open_project $project_name
} else {
    open_project $project_name -reset
}

# 设置顶层函数（AXI Stream 版本，用于 DMA）
set_top $top_function

# 添加源文件
add_files histogram_hls.cpp
add_files -cflags "-I." histogram_hls.h

# 添加测试平台
add_files -tb histogram_hls_test.cpp
add_files -tb histogram_hls.cpp

# 打开解决方案
open_solution "solution1" -flow_target vivado

# 设置目标设备（Zynq Ultrascale+）
# 根据实际硬件修改
set_part {xczu5ev-sfvc784-1-i}

# 创建时钟约束（10ns = 100MHz）
create_clock -period 10 -name default

# 配置编译选项
config_compile -name_max_length 80
config_schedule -enable_dsp_full_reg
config_bind -effort high

# 运行 C Simulation
puts "Running C Simulation..."
csim_design

# 运行 C Synthesis
puts "Running C Synthesis..."
csynth_design

# 运行 C/RTL Cosimulation（可选，需要仿真工具）
# puts "Running C/RTL Cosimulation..."
# cosim_design -rtl verilog -tool modelsim

# 导出 IP（用于 Vivado）
puts "Exporting IP..."
export_design -format ip_catalog -output ../histogram_hls_ip_axi_stream

puts "HLS flow completed! (AXI Stream version)"
puts "IP exported to: ../histogram_hls_ip_axi_stream"

# 退出
exit


