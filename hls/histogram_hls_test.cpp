// HLS Testbench for Histogram Computation
// 用于 Vitis HLS 综合和仿真的测试平台

#include "histogram_hls.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_IMAGE_WIDTH 1920
#define TEST_IMAGE_HEIGHT 1080

int main() {
    printf("=== HLS Histogram Testbench ===\n");
    
    // 创建测试图像
    int image_size = TEST_IMAGE_WIDTH * TEST_IMAGE_HEIGHT;
    unsigned char *image_data = (unsigned char*)malloc(image_size);
    unsigned int *histogram_result = (unsigned int*)malloc(HISTOGRAM_BINS * sizeof(unsigned int));
    unsigned int *histogram_reference = (unsigned int*)malloc(HISTOGRAM_BINS * sizeof(unsigned int));
    
    // 生成测试图像（渐变图案）
    for (int i = 0; i < image_size; i++) {
        image_data[i] = i % 256;
    }
    
    // 计算参考直方图（CPU版本）
    memset(histogram_reference, 0, HISTOGRAM_BINS * sizeof(unsigned int));
    for (int i = 0; i < image_size; i++) {
        histogram_reference[image_data[i]]++;
    }
    
    // 调用 HLS 函数（使用 AXI Master 接口）
    compute_histogram_axi_master(image_data, histogram_result, image_size);
    
    // 验证结果
    int errors = 0;
    for (int i = 0; i < HISTOGRAM_BINS; i++) {
        if (histogram_result[i] != histogram_reference[i]) {
            printf("Error at bin %d: expected %u, got %u\n", 
                   i, histogram_reference[i], histogram_result[i]);
            errors++;
        }
    }
    
    if (errors == 0) {
        printf("Test PASSED: All histogram bins match reference!\n");
    } else {
        printf("Test FAILED: %d errors found\n", errors);
    }
    
    // 打印部分统计信息
    printf("\nSample histogram values:\n");
    for (int i = 0; i < 10; i++) {
        printf("Bin %d: %u (reference: %u)\n", 
               i, histogram_result[i], histogram_reference[i]);
    }
    
    free(image_data);
    free(histogram_result);
    free(histogram_reference);
    
    return (errors == 0) ? 0 : 1;
}
