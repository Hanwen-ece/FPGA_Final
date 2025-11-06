// Histogram Computation using Vitis HLS
// 针对 FPGA/PL 加速器的直方图计算实现

#include "histogram_hls.h"

// 简单的 Stream 接口版本（用于测试）
void compute_histogram_hls(
    hls::stream<ap_uint<8> >& image_stream,
    hls::stream<ap_uint<32> >& histogram_stream,
    int image_size
) {
    // 使用局部数组存储直方图（BRAM）
    static unsigned int histogram_local[HISTOGRAM_BINS];
    #pragma HLS ARRAY_PARTITION variable=histogram_local cyclic factor=4
    
    // 初始化直方图
    INIT_LOOP: for (int i = 0; i < HISTOGRAM_BINS; i++) {
        #pragma HLS UNROLL factor=4
        histogram_local[i] = 0;
    }
    
    // 处理图像数据流
    PROCESS_LOOP: for (int i = 0; i < image_size; i++) {
        #pragma HLS PIPELINE II=1
        ap_uint<8> pixel_value = image_stream.read();
        histogram_local[pixel_value]++;
    }
    
    // 输出直方图结果
    OUTPUT_LOOP: for (int i = 0; i < HISTOGRAM_BINS; i++) {
        #pragma HLS PIPELINE II=1
        histogram_stream.write(histogram_local[i]);
    }
}

// 使用 AXI Stream 接口的版本（用于 DMA）- 优化热区域
void compute_histogram_axi_stream(
    hls::stream<ap_axiu<32, 0, 0, 0> >& image_stream,
    hls::stream<ap_axiu<32, 0, 0, 0> >& histogram_stream,
    int image_size
) {
    #pragma HLS INTERFACE axis port=image_stream
    #pragma HLS INTERFACE axis port=histogram_stream
    #pragma HLS INTERFACE s_axilite port=image_size bundle=control
    #pragma HLS INTERFACE s_axilite port=return bundle=control
    
    // 优化热区域：使用完全分区（complete）来减少写冲突
    // 每个bin使用独立的BRAM，提高并行度
    static unsigned int histogram_local[HISTOGRAM_BINS];
    #pragma HLS ARRAY_PARTITION variable=histogram_local complete
    
    // 初始化 - 优化：使用循环展开加快初始化
    INIT_LOOP: for (int i = 0; i < HISTOGRAM_BINS; i++) {
        #pragma HLS UNROLL
        histogram_local[i] = 0;
    }
    
    // 处理图像数据 - 热区域优化
    // AXI Stream 每次传输32位，包含4个像素（每个8位）
    int pixels_per_transfer = 4;
    int transfers = (image_size + pixels_per_transfer - 1) / pixels_per_transfer;
    
    PROCESS_LOOP: for (int i = 0; i < transfers; i++) {
        #pragma HLS PIPELINE II=1
        ap_axiu<32, 0, 0, 0> data = image_stream.read();
        ap_uint<32> pixel_data = data.data;
        
        // 优化：并行提取4个像素值，减少循环依赖
        ap_uint<8> pixel0 = pixel_data.range(7, 0);
        ap_uint<8> pixel1 = pixel_data.range(15, 8);
        ap_uint<8> pixel2 = pixel_data.range(23, 16);
        ap_uint<8> pixel3 = pixel_data.range(31, 24);
        
        // 热区域：并行更新直方图（使用完全分区，每个bin独立BRAM，避免冲突）
        int base_idx = i * pixels_per_transfer;
        
        if (base_idx < image_size) histogram_local[pixel0]++;
        if (base_idx + 1 < image_size) histogram_local[pixel1]++;
        if (base_idx + 2 < image_size) histogram_local[pixel2]++;
        if (base_idx + 3 < image_size) histogram_local[pixel3]++;
    }
    
    // 输出结果
    OUTPUT_LOOP: for (int i = 0; i < HISTOGRAM_BINS; i++) {
        #pragma HLS PIPELINE II=1
        ap_axiu<32, 0, 0, 0> output_data;
        output_data.data = histogram_local[i];
        output_data.last = (i == HISTOGRAM_BINS - 1);
        histogram_stream.write(output_data);
    }
}

// 使用 AXI Master 接口的版本（直接内存访问）
void compute_histogram_axi_master(
    unsigned char* image_data,
    unsigned int* histogram_data,
    int image_size
) {
    #pragma HLS INTERFACE m_axi port=image_data offset=slave bundle=gmem0 depth=2097152
    #pragma HLS INTERFACE m_axi port=histogram_data offset=slave bundle=gmem1 depth=256
    #pragma HLS INTERFACE s_axilite port=image_data bundle=control
    #pragma HLS INTERFACE s_axilite port=histogram_data bundle=control
    #pragma HLS INTERFACE s_axilite port=image_size bundle=control
    #pragma HLS INTERFACE s_axilite port=return bundle=control
    
    // 优化热区域：使用完全分区减少写冲突
    static unsigned int histogram_local[HISTOGRAM_BINS];
    #pragma HLS ARRAY_PARTITION variable=histogram_local complete
    
    // 初始化 - 优化：完全展开
    INIT_LOOP: for (int i = 0; i < HISTOGRAM_BINS; i++) {
        #pragma HLS UNROLL
        histogram_local[i] = 0;
    }
    
    // 处理图像数据 - 热区域优化
    // 使用流水线提高吞吐量
    PROCESS_LOOP: for (int i = 0; i < image_size; i++) {
        #pragma HLS PIPELINE II=1
        unsigned char pixel_value = image_data[i];
        histogram_local[pixel_value]++;  // 热区域：每个bin独立BRAM，无冲突
    }
    
    // 写回结果
    OUTPUT_LOOP: for (int i = 0; i < HISTOGRAM_BINS; i++) {
        #pragma HLS PIPELINE II=1
        histogram_data[i] = histogram_local[i];
    }
}
