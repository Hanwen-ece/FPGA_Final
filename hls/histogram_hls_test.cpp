// HLS Testbench for Histogram Computation
// 用于 Vitis HLS 综合和仿真的测试平台

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hls_stream.h"

#define TEST_IMAGE_WIDTH 1920
#define TEST_IMAGE_HEIGHT 1080

#include "ap_int.h"
#include "ap_axi_sdata.h"
#include "hls_stream.h"

#define HISTOGRAM_BINS 256



// Testbench for Histogram HLS
#include "ap_int.h"
#include "ap_axi_sdata.h"
#include "hls_stream.h"
#include <iostream>
#include <cstdlib>

#define HISTOGRAM_BINS 256
#define IMAGE_WIDTH 32
#define IMAGE_HEIGHT 32
#define IMAGE_SIZE (IMAGE_WIDTH * IMAGE_HEIGHT)

// 函数声明
void Hanwenip_v1_0_HLS(
    hls::stream<ap_axiu<32, 0, 0, 0> >& image_stream,
    hls::stream<ap_axiu<32, 0, 0, 0> >& histogram_stream
);

int main() {
    std::cout << "=== Histogram HLS Testbench ===" << std::endl;
    std::cout << "Image size: " << IMAGE_WIDTH << "x" << IMAGE_HEIGHT 
              << " = " << IMAGE_SIZE << " pixels" << std::endl;
    
    // 创建测试图像
    unsigned char test_image[IMAGE_SIZE];
    for (int i = 0; i < IMAGE_HEIGHT; i++) {
        for (int j = 0; j < IMAGE_WIDTH; j++) {
            test_image[i * IMAGE_WIDTH + j] = (i * 13 + j * 7) % 256;
        }
    }
    
    // 计算CPU参考结果
    unsigned int cpu_histogram[HISTOGRAM_BINS] = {0};
    for (int i = 0; i < IMAGE_SIZE; i++) {
        cpu_histogram[test_image[i]]++;
    }
    
    // 准备输入流
    hls::stream<ap_axiu<32, 0, 0, 0> > image_stream;
    hls::stream<ap_axiu<32, 0, 0, 0> > histogram_stream;
    
    int pixels_per_word = 4;
    int num_words = (IMAGE_SIZE + pixels_per_word - 1) / pixels_per_word;
    
    std::cout << "Preparing input stream: " << num_words << " words" << std::endl;
    
    for (int i = 0; i < num_words; i++) {
        ap_axiu<32, 0, 0, 0> data;
        
        // 打包4个像素到1个32位数据
        int base_idx = i * pixels_per_word;
        ap_uint<32> packed = 0;
        
        for (int j = 0; j < pixels_per_word; j++) {
            int idx = base_idx + j;
            if (idx < IMAGE_SIZE) {
                packed.range((j+1)*8-1, j*8) = test_image[idx];
            } else {
                packed.range((j+1)*8-1, j*8) = 0;  // padding
            }
        }
        
        data.data = packed;
        data.last = (i == num_words - 1) ? 1 : 0;  // 最后一个数据设置TLAST
        data.keep = -1;
        data.strb = -1;
        image_stream.write(data);
    }
    
    // 调用HLS函数
    std::cout << "Calling HLS function..." << std::endl;
    Hanwenip_v1_0_HLS(image_stream, histogram_stream);
    
    // 读取并验证结果
    std::cout << "Verifying results..." << std::endl;
    int errors = 0;
    
    for (int i = 0; i < HISTOGRAM_BINS; i++) {
        ap_axiu<32, 0, 0, 0> output_data = histogram_stream.read();
        unsigned int hw_value = output_data.data;
        unsigned int cpu_value = cpu_histogram[i];
        
        if (hw_value != cpu_value) {
            if (errors < 10) {
                std::cout << "Error at bin " << i << ": HW=" << hw_value 
                         << ", CPU=" << cpu_value << std::endl;
            }
            errors++;
        }
    }
    
    // 显示前10个结果
    std::cout << "\nFirst 10 histogram values:" << std::endl;
    std::cout << "Bin\tHW\tCPU\tMatch" << std::endl;
    
    // 重新运行以获取结果显示
    for (int i = 0; i < num_words; i++) {
        ap_axiu<32, 0, 0, 0> data;
        int base_idx = i * pixels_per_word;
        ap_uint<32> packed = 0;
        
        for (int j = 0; j < pixels_per_word; j++) {
            int idx = base_idx + j;
            if (idx < IMAGE_SIZE) {
                packed.range((j+1)*8-1, j*8) = test_image[idx];
            }
        }
        
        data.data = packed;
        data.last = (i == num_words - 1) ? 1 : 0;
        data.keep = -1;
        data.strb = -1;
        image_stream.write(data);
    }
    
    Hanwenip_v1_0_HLS(image_stream, histogram_stream);
    
    for (int i = 0; i < 10; i++) {
        ap_axiu<32, 0, 0, 0> output_data = histogram_stream.read();
        unsigned int hw_value = output_data.data;
        std::cout << i << "\t" << hw_value << "\t" << cpu_histogram[i] 
                 << "\t" << (hw_value == cpu_histogram[i] ? "✓" : "✗") << std::endl;
    }
    
    // 清空剩余数据
    for (int i = 10; i < HISTOGRAM_BINS; i++) {
        histogram_stream.read();
    }
    
    // 显示结果
    if (errors == 0) {
        std::cout << "\n==================================" << std::endl;
        std::cout << "    TEST PASSED!" << std::endl;
        std::cout << "==================================" << std::endl;
        return 0;
    } else {
        std::cout << "\n==================================" << std::endl;
        std::cout << "    TEST FAILED!" << std::endl;
        std::cout << "    Errors: " << errors << std::endl;
        std::cout << "==================================" << std::endl;
        return 1;
    }
}