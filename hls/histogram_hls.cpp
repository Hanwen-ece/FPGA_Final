// Histogram Computation using Vitis HLS
// 针对 FPGA/PL 加速器的直方图计算实现
#include "ap_int.h"
#include "ap_axi_sdata.h"
#include "hls_stream.h"

#define HISTOGRAM_BINS 256

// 使用 AXI Stream 接口的版本
void Hanwenip_v1_0_HLS(
    hls::stream<ap_axiu<32, 0, 0, 0> >& image_stream,
    hls::stream<ap_axiu<32, 0, 0, 0> >& histogram_stream,
    int image_size
);

// Histogram Computation using Vitis HLS
// 优化版：32x32小图像，使用ap_ctrl_none自动运行
#include "ap_int.h"
#include "ap_axi_sdata.h"
#include "hls_stream.h"

#define HISTOGRAM_BINS 256

void Hanwenip_v1_0_HLS(
    hls::stream<ap_axiu<32, 0, 0, 0> >& image_stream,
    hls::stream<ap_axiu<32, 0, 0, 0> >& histogram_stream
) {
    // 使用 ap_ctrl_none：自动运行，不需要启动信号
    #pragma HLS INTERFACE ap_ctrl_none port=return
    #pragma HLS INTERFACE axis port=image_stream
    #pragma HLS INTERFACE axis port=histogram_stream
    
    // 4个独立的累加器（避免写冲突）
    unsigned int hist_acc0[HISTOGRAM_BINS];
    unsigned int hist_acc1[HISTOGRAM_BINS];
    unsigned int hist_acc2[HISTOGRAM_BINS];
    unsigned int hist_acc3[HISTOGRAM_BINS];
    
    // 部分分区（平衡资源和性能）
    #pragma HLS ARRAY_PARTITION variable=hist_acc0 cyclic factor=16
    #pragma HLS ARRAY_PARTITION variable=hist_acc1 cyclic factor=16
    #pragma HLS ARRAY_PARTITION variable=hist_acc2 cyclic factor=16
    #pragma HLS ARRAY_PARTITION variable=hist_acc3 cyclic factor=16
    
    // 初始化累加器
    INIT_LOOP: for (int i = 0; i < HISTOGRAM_BINS; i++) {
        #pragma HLS PIPELINE II=1
        hist_acc0[i] = 0;
        hist_acc1[i] = 0;
        hist_acc2[i] = 0;
        hist_acc3[i] = 0;
    }
    
    // 处理输入数据流，直到检测到TLAST
    PROCESS_LOOP: while(true) {
        #pragma HLS PIPELINE II=1
        #pragma HLS DEPENDENCE variable=hist_acc0 inter false
        #pragma HLS DEPENDENCE variable=hist_acc1 inter false
        #pragma HLS DEPENDENCE variable=hist_acc2 inter false
        #pragma HLS DEPENDENCE variable=hist_acc3 inter false
        
        // 读取一个32位数据（包含4个像素）
        ap_axiu<32, 0, 0, 0> data = image_stream.read();
        ap_uint<32> pixel_data = data.data;
        
        // 提取4个像素
        ap_uint<8> pixel0 = pixel_data.range(7, 0);
        ap_uint<8> pixel1 = pixel_data.range(15, 8);
        ap_uint<8> pixel2 = pixel_data.range(23, 16);
        ap_uint<8> pixel3 = pixel_data.range(31, 24);
        
        // 使用独立累加器避免冲突
        hist_acc0[pixel0]++;
        hist_acc1[pixel1]++;
        hist_acc2[pixel2]++;
        hist_acc3[pixel3]++;
        
        // 检测TLAST信号（数据流结束）
        if (data.last) {
            break;
        }
    }
    
    // 合并累加器并输出结果
    OUTPUT_LOOP: for (int i = 0; i < HISTOGRAM_BINS; i++) {
        #pragma HLS PIPELINE II=1
        
        // 合并4个累加器
        unsigned int sum = hist_acc0[i] + hist_acc1[i] + 
                          hist_acc2[i] + hist_acc3[i];
        
        // 输出直方图值
        ap_axiu<32, 0, 0, 0> output_data;
        output_data.data = sum;
        output_data.last = (i == HISTOGRAM_BINS - 1);
        output_data.keep = -1;
        output_data.strb = -1;
        histogram_stream.write(output_data);
    }
}
