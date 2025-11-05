#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#define HISTOGRAM_BINS 256

// 简化版图像加载（可以替换为stb_image）
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
    img->channels = 1; // 灰度图
    img->data = (unsigned char*)malloc(width * height);
    
    // 生成渐变图案用于测试
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            img->data[i * width + j] = (i + j) % 256;
        }
    }
    return img;
}

// CPU版本的直方图计算（ARM Cortex-A53优化）
void compute_histogram_cpu(unsigned char *image, int size, unsigned int *histogram) {
    // 初始化直方图
    memset(histogram, 0, HISTOGRAM_BINS * sizeof(unsigned int));
    
    // 统计每个像素值
    // 使用简单的循环，ARM编译器会自动优化
    for (int i = 0; i < size; i++) {
        histogram[image[i]]++;
    }
}

// 保存直方图到文件
void save_histogram_txt(unsigned int *histogram, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("Error: Cannot open file %s\n", filename);
        return;
    }
    
    fprintf(fp, "# Histogram Data (Bin, Count)\n");
    fprintf(fp, "# Platform: Kria (Zynq Ultrascale+) PS - ARM Cortex-A53\n");
    for (int i = 0; i < HISTOGRAM_BINS; i++) {
        fprintf(fp, "%d %u\n", i, histogram[i]);
    }
    fclose(fp);
    printf("Histogram saved to %s\n", filename);
}

// 计时函数（Linux/ARM平台）
double get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

int main(int argc, char **argv) {
    int width = 1920;
    int height = 1080;
    
    if (argc >= 3) {
        width = atoi(argv[1]);
        height = atoi(argv[2]);
    }
    
    printf("=== Kria PS (Cortex-A53) CPU Histogram Computation ===\n");
    printf("Platform: ARM Cortex-A53 (Zynq Ultrascale+)\n");
    printf("Image size: %dx%d\n", width, height);
    
    // 创建测试图像
    Image *img = create_test_image(width, height);
    int image_size = width * height;
    
    // 分配直方图内存
    unsigned int *histogram = (unsigned int*)malloc(HISTOGRAM_BINS * sizeof(unsigned int));
    
    // 预热
    compute_histogram_cpu(img->data, image_size, histogram);
    
    // 多次运行取平均
    int iterations = 10;
    double total_time = 0.0;
    
    for (int iter = 0; iter < iterations; iter++) {
        double start = get_time_ms();
        compute_histogram_cpu(img->data, image_size, histogram);
        double end = get_time_ms();
        total_time += (end - start);
    }
    
    double avg_time = total_time / iterations;
    printf("Average execution time: %.3f ms\n", avg_time);
    printf("Throughput: %.2f MPixels/s\n", (image_size / 1e6) / (avg_time / 1000.0));
    
    // 保存结果
    save_histogram_txt(histogram, "histogram_kria_ps.txt");
    
    // 打印部分统计信息
    printf("\nSample histogram values:\n");
    for (int i = 0; i < 10; i++) {
        printf("Bin %d: %u\n", i, histogram[i]);
    }
    
    // 清理
    free(img->data);
    free(img);
    free(histogram);
    
    return 0;
}


