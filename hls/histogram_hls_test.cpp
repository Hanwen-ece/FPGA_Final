// HLS Testbench for Histogram Computation
// 用于 Vitis HLS 综合和仿真的测试平台

#include "histogram_hls.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hls_stream.h"

#define TEST_IMAGE_WIDTH 1920
#define TEST_IMAGE_HEIGHT 1080

// 测试 AXI Master 接口
int test_axi_master() {
    printf("\n=== Testing AXI Master Interface ===\n");
    
    int image_size = TEST_IMAGE_WIDTH * TEST_IMAGE_HEIGHT;
    unsigned char *image_data = (unsigned char*)malloc(image_size);
    unsigned int *histogram_result = (unsigned int*)malloc(HISTOGRAM_BINS * sizeof(unsigned int));
    unsigned int *histogram_reference = (unsigned int*)malloc(HISTOGRAM_BINS * sizeof(unsigned int));
    
    // 生成测试图像
    for (int i = 0; i < image_size; i++) {
        image_data[i] = (i * 13 + 7) % 256;  // 与 Python 代码保持一致
    }
    
    // 计算参考直方图（CPU版本）
    memset(histogram_reference, 0, HISTOGRAM_BINS * sizeof(unsigned int));
    for (int i = 0; i < image_size; i++) {
        histogram_reference[image_data[i]]++;
    }
    
    // 调用 HLS 函数
    compute_histogram_axi_master(image_data, histogram_result, image_size);
    
    // 验证结果
    int errors = 0;
    for (int i = 0; i < HISTOGRAM_BINS; i++) {
        if (histogram_result[i] != histogram_reference[i]) {
            if (errors < 10) {
                printf("Error at bin %d: expected %u, got %u\n", 
                       i, histogram_reference[i], histogram_result[i]);
            }
            errors++;
        }
    }
    
    if (errors == 0) {
        printf("AXI Master Test PASSED!\n");
    } else {
        printf("AXI Master Test FAILED: %d errors found\n", errors);
    }
    
    free(image_data);
    free(histogram_result);
    free(histogram_reference);
    
    return errors;
}

// 测试 AXI Stream 接口（用于 DMA）
int test_axi_stream() {
    printf("\n=== Testing AXI Stream Interface (for DMA) ===\n");
    
    int image_size = TEST_IMAGE_WIDTH * TEST_IMAGE_HEIGHT;
    unsigned char *image_data = (unsigned char*)malloc(image_size);
    unsigned int *histogram_reference = (unsigned int*)malloc(HISTOGRAM_BINS * sizeof(unsigned int));
    
    // 生成测试图像
    for (int i = 0; i < image_size; i++) {
        image_data[i] = (i * 13 + 7) % 256;
    }
    
    // 计算参考直方图
    memset(histogram_reference, 0, HISTOGRAM_BINS * sizeof(unsigned int));
    for (int i = 0; i < image_size; i++) {
        histogram_reference[image_data[i]]++;
    }
    
    // 创建 AXI Stream
    hls::stream<ap_axiu<32, 0, 0, 0> > image_stream;
    hls::stream<ap_axiu<32, 0, 0, 0> > histogram_stream;
    
    // 将图像数据写入 stream（每32位包含4个像素）
    int pixels_per_transfer = 4;
    int transfers = (image_size + pixels_per_transfer - 1) / pixels_per_transfer;
    
    for (int i = 0; i < transfers; i++) {
        ap_axiu<32, 0, 0, 0> data;
        ap_uint<32> pixel_data = 0;
        
        for (int j = 0; j < pixels_per_transfer && (i * pixels_per_transfer + j) < image_size; j++) {
            int idx = i * pixels_per_transfer + j;
            pixel_data.range(7 + j*8, j*8) = image_data[idx];
        }
        
        data.data = pixel_data;
        data.last = (i == transfers - 1);
        image_stream.write(data);
    }
    
    // 调用 HLS 函数
    compute_histogram_axi_stream(image_stream, histogram_stream, image_size);
    
    // 从 stream 读取结果
    unsigned int histogram_result[HISTOGRAM_BINS];
    for (int i = 0; i < HISTOGRAM_BINS; i++) {
        ap_axiu<32, 0, 0, 0> data = histogram_stream.read();
        histogram_result[i] = data.data;
    }
    
    // 验证结果
    int errors = 0;
    for (int i = 0; i < HISTOGRAM_BINS; i++) {
        if (histogram_result[i] != histogram_reference[i]) {
            if (errors < 10) {
                printf("Error at bin %d: expected %u, got %u\n", 
                       i, histogram_reference[i], histogram_result[i]);
            }
            errors++;
        }
    }
    
    if (errors == 0) {
        printf("AXI Stream Test PASSED!\n");
    } else {
        printf("AXI Stream Test FAILED: %d errors found\n", errors);
    }
    
    free(image_data);
    free(histogram_reference);
    
    return errors;
}

int main() {
    printf("=== HLS Histogram Testbench ===\n");
    printf("Image size: %dx%d (%d pixels)\n", 
           TEST_IMAGE_WIDTH, TEST_IMAGE_HEIGHT, 
           TEST_IMAGE_WIDTH * TEST_IMAGE_HEIGHT);
    
    int total_errors = 0;
    
    // 测试 AXI Master 接口
    total_errors += test_axi_master();
    
    // 测试 AXI Stream 接口（用于 DMA）
    total_errors += test_axi_stream();
    
    // 总结
    printf("\n=== Test Summary ===\n");
    if (total_errors == 0) {
        printf("All tests PASSED!\n");
    } else {
        printf("Total errors: %d\n", total_errors);
    }
    
    return (total_errors == 0) ? 0 : 1;
}
