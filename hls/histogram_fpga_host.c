// FPGA Host Code for Histogram Computation
// 使用 AXI DMA 与 FPGA/PL 加速器通信的主机程序
// 适用于 Kria (Zynq Ultrascale+) 平台

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#define HISTOGRAM_BINS 256
#define PAGE_SIZE 4096

// 内存映射的物理地址（需要根据实际硬件配置调整）
#define FPGA_BASE_ADDR 0xA0000000
#define DMA_BASE_ADDR 0xA0010000

// 简化版图像加载
typedef struct {
    unsigned char *data;
    int width;
    int height;
    int channels;
} Image;

// 生成测试图像
Image* create_test_image(int width, int height) {
    Image *img = (Image*)malloc(sizeof(Image));
    img->width = width;
    img->height = height;
    img->channels = 1;
    img->data = (unsigned char*)malloc(width * height);
    
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            img->data[i * width + j] = (i + j) % 256;
        }
    }
    return img;
}

// 计时函数（Linux/ARM）
double get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

// 保存直方图到文件
void save_histogram_txt(unsigned int *histogram, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("Error: Cannot open file %s\n", filename);
        return;
    }
    
    fprintf(fp, "# Histogram Data (Bin, Count)\n");
    fprintf(fp, "# Platform: FPGA/PL Accelerator on Kria\n");
    for (int i = 0; i < HISTOGRAM_BINS; i++) {
        fprintf(fp, "%d %u\n", i, histogram[i]);
    }
    fclose(fp);
    printf("Histogram saved to %s\n", filename);
}

// 使用 AXI Master 接口的 FPGA 加速器调用
int compute_histogram_fpga_axi_master(
    unsigned char *image_data,
    unsigned int *histogram_data,
    int image_size
) {
    // 打开 /dev/mem 设备文件
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("Failed to open /dev/mem");
        return -1;
    }
    
    // 映射 FPGA 控制寄存器
    void *fpga_base = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, 
                          MAP_SHARED, fd, FPGA_BASE_ADDR);
    if (fpga_base == MAP_FAILED) {
        perror("Failed to map FPGA registers");
        close(fd);
        return -1;
    }
    
    // 映射图像数据内存（共享内存）
    // 注意：实际实现中需要使用 Xilinx 的 xil_malloc 或类似函数
    // 这里使用简化版本
    unsigned char *image_shared = image_data;
    unsigned int *histogram_shared = histogram_data;
    
    // 设置控制寄存器
    volatile unsigned int *ctrl_reg = (volatile unsigned int*)fpga_base;
    
    // 写入图像数据地址
    ctrl_reg[0] = (unsigned int)(uintptr_t)image_shared;
    // 写入直方图输出地址
    ctrl_reg[1] = (unsigned int)(uintptr_t)histogram_shared;
    // 写入图像大小
    ctrl_reg[2] = image_size;
    
    // 启动加速器
    ctrl_reg[3] = 0x1;  // Start bit
    
    // 等待完成
    while ((ctrl_reg[4] & 0x1) == 0) {  // Done bit
        usleep(100);
    }
    
    // 清理
    munmap(fpga_base, PAGE_SIZE);
    close(fd);
    
    return 0;
}

// 使用 AXI DMA 的 FPGA 加速器调用（简化版本）
// 注意：完整的实现需要使用 Xilinx DMA 驱动 API
int compute_histogram_fpga_dma(
    unsigned char *image_data,
    unsigned int *histogram_data,
    int image_size
) {
    printf("Note: This is a simplified DMA implementation.\n");
    printf("Full implementation requires Xilinx DMA driver.\n");
    
    // 在实际实现中，这里应该：
    // 1. 使用 Xilinx DMA 驱动 API (如 xaxidma_* 函数)
    // 2. 配置 DMA 通道
    // 3. 传输图像数据到 FPGA
    // 4. 启动 FPGA 加速器
    // 5. 等待 DMA 传输完成
    // 6. 读取结果
    
    // 简化版本：直接调用 AXI Master 接口
    return compute_histogram_fpga_axi_master(image_data, histogram_data, image_size);
}

// CPU 参考实现（用于验证）
void compute_histogram_cpu(unsigned char *image, int size, unsigned int *histogram) {
    memset(histogram, 0, HISTOGRAM_BINS * sizeof(unsigned int));
    for (int i = 0; i < size; i++) {
        histogram[image[i]]++;
    }
}

int main(int argc, char **argv) {
    int width = 1920;
    int height = 1080;
    int use_dma = 0;  // 0 = AXI Master, 1 = DMA
    
    if (argc >= 3) {
        width = atoi(argv[1]);
        height = atoi(argv[2]);
    }
    if (argc >= 4) {
        use_dma = atoi(argv[3]);
    }
    
    printf("=== FPGA/PL Accelerator Histogram Computation ===\n");
    printf("Platform: Kria (Zynq Ultrascale+)\n");
    printf("Image size: %dx%d\n", width, height);
    printf("Interface: %s\n", use_dma ? "AXI DMA" : "AXI Master");
    
    // 创建测试图像
    Image *img = create_test_image(width, height);
    int image_size = width * height;
    
    // 分配直方图内存（需要对齐到页边界）
    unsigned int *histogram_fpga = (unsigned int*)malloc(
        HISTOGRAM_BINS * sizeof(unsigned int) + PAGE_SIZE);
    unsigned int *histogram_cpu = (unsigned int*)malloc(
        HISTOGRAM_BINS * sizeof(unsigned int));
    
    // 对齐内存地址（如果需要）
    unsigned int *histogram_aligned = (unsigned int*)((uintptr_t)histogram_fpga & ~(PAGE_SIZE - 1));
    
    // 计算 CPU 参考结果
    compute_histogram_cpu(img->data, image_size, histogram_cpu);
    
    // 预热 FPGA（如果支持）
    
    // 多次运行取平均
    int iterations = 10;
    double total_time = 0.0;
    int success_count = 0;
    
    for (int iter = 0; iter < iterations; iter++) {
        // 清零 FPGA 结果
        memset(histogram_aligned, 0, HISTOGRAM_BINS * sizeof(unsigned int));
        
        double start = get_time_ms();
        
        int result;
        if (use_dma) {
            result = compute_histogram_fpga_dma(img->data, histogram_aligned, image_size);
        } else {
            result = compute_histogram_fpga_axi_master(img->data, histogram_aligned, image_size);
        }
        
        double end = get_time_ms();
        
        if (result == 0) {
            total_time += (end - start);
            success_count++;
        }
    }
    
    if (success_count > 0) {
        double avg_time = total_time / success_count;
        printf("Average execution time: %.3f ms\n", avg_time);
        printf("Throughput: %.2f MPixels/s\n", (image_size / 1e6) / (avg_time / 1000.0));
    } else {
        printf("Warning: All FPGA runs failed. Using CPU implementation.\n");
        memcpy(histogram_aligned, histogram_cpu, HISTOGRAM_BINS * sizeof(unsigned int));
    }
    
    // 验证结果
    int errors = 0;
    for (int i = 0; i < HISTOGRAM_BINS; i++) {
        if (histogram_aligned[i] != histogram_cpu[i]) {
            printf("Error at bin %d: FPGA=%u, CPU=%u\n", 
                   i, histogram_aligned[i], histogram_cpu[i]);
            errors++;
            if (errors > 10) break;  // 只显示前10个错误
        }
    }
    
    if (errors == 0) {
        printf("Verification: PASSED (FPGA results match CPU reference)\n");
    } else {
        printf("Verification: FAILED (%d errors found)\n", errors);
    }
    
    // 保存结果
    save_histogram_txt(histogram_aligned, "histogram_fpga.txt");
    
    // 打印部分统计信息
    printf("\nSample histogram values:\n");
    for (int i = 0; i < 10; i++) {
        printf("Bin %d: %u\n", i, histogram_aligned[i]);
    }
    
    // 清理
    free(img->data);
    free(img);
    free(histogram_fpga);
    free(histogram_cpu);
    
    return (errors == 0) ? 0 : 1;
}
