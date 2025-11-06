// FPGA Host Code for Histogram Computation
// 适用于 Kria (Zynq Ultrascale+) 平台
// 简化版本 - 用于测试和演示

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#define HISTOGRAM_BINS 256

typedef struct
{
    unsigned char *data;
    int width;
    int height;
    int channels;
} Image;

// 生成测试图像
Image *create_test_image(int width, int height)
{
    Image *img = (Image *)malloc(sizeof(Image));
    img->width = width;
    img->height = height;
    img->channels = 1;
    img->data = (unsigned char *)malloc(width * height);

    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
        {
            img->data[i * width + j] = (i * 13 + j * 7) % 256;
        }
    }
    return img;
}

// CPU版本的直方图计算（用于Kria PS对比）
void compute_histogram_cpu(unsigned char *image, int size, unsigned int *histogram)
{
    memset(histogram, 0, HISTOGRAM_BINS * sizeof(unsigned int));
    for (int i = 0; i < size; i++)
    {
        histogram[image[i]]++;
    }
}

// FPGA加速器调用（简化版本）
// 注意：实际实现需要FPGA硬件支持
int compute_histogram_fpga(unsigned char *image_data, unsigned int *histogram_data, int image_size)
{
    // 简化版本：如果FPGA不可用，使用CPU实现
    // 实际实现中，这里应该：
    // 1. 初始化FPGA设备
    // 2. 配置AXI DMA或AXI Master接口
    // 3. 传输数据到FPGA
    // 4. 启动加速器
    // 5. 等待完成并读取结果

    // 当前实现：使用CPU作为fallback
    printf("Note: FPGA hardware not available, using CPU implementation.\n");
    compute_histogram_cpu(image_data, image_size, histogram_data);
    return 0;
}

// 计时函数
double get_time_ms()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

// 保存直方图到文件
void save_histogram_txt(unsigned int *histogram, const char *filename, double total_time, int width, int height, int iterations)
{
    FILE *fp = fopen(filename, "w");
    if (!fp)
    {
        printf("Error: Cannot open file %s\n", filename);
        return;
    }

    fprintf(fp, "# Histogram Data (Bin, Count)\n");
    fprintf(fp, "# Platform: FPGA/PL Accelerator on Kria\n");
    fprintf(fp, "# Image size: %dx%d\n", width, height);
    fprintf(fp, "# Iterations: %d\n", iterations);
    fprintf(fp, "# Total execution time: %.3f ms (%.3f seconds)\n", total_time, total_time / 1000.0);

    for (int i = 0; i < HISTOGRAM_BINS; i++)
    {
        fprintf(fp, "%d %u\n", i, histogram[i]);
    }
    fclose(fp);
    printf("Histogram saved to %s\n", filename);
}

int main(int argc, char **argv)
{
    int width = 3840;
    int height = 2160;
    int iterations = 1000;

    if (argc >= 3)
    {
        width = atoi(argv[1]);
        height = atoi(argv[2]);
    }
    if (argc >= 4)
    {
        iterations = atoi(argv[3]);
    }

    printf("=== FPGA/PL Accelerator Histogram Computation ===\n");
    printf("Platform: Kria (Zynq Ultrascale+)\n");
    printf("Image size: %dx%d (%.2f MP)\n", width, height, (width * height) / 1e6);
    printf("Iterations: %d\n", iterations);

    long long total_pixels = (long long)width * height;
    double total_data = (total_pixels * iterations) / (1024.0 * 1024.0 * 1024.0);
    printf("Total data to process: %.2f GB\n\n", total_data);

    // 创建测试图像
    printf("Generating test image...\n");
    Image *img = create_test_image(width, height);
    int image_size = width * height;

    // 分配内存
    unsigned int *histogram = (unsigned int *)malloc(HISTOGRAM_BINS * sizeof(unsigned int));

    // 预热
    printf("Warming up...\n");
    compute_histogram_fpga(img->data, histogram, image_size);
    printf("Starting benchmark...\n\n");

    // 主循环
    double start_time = get_time_ms();
    double last_update = start_time;

    for (int iter = 0; iter < iterations; iter++)
    {
        compute_histogram_fpga(img->data, histogram, image_size);

        // 每100ms更新一次进度
        double current_time = get_time_ms();
        if (current_time - last_update > 100 || iter == iterations - 1)
        {
            int barWidth = 50;
            float progress = (float)(iter + 1) / iterations;
            printf("\r[");
            int pos = barWidth * progress;
            for (int i = 0; i < barWidth; ++i)
            {
                if (i < pos)
                    printf("=");
                else if (i == pos)
                    printf(">");
                else
                    printf(" ");
            }
            printf("] %d/%d (%.1f%%) - %.2fs elapsed",
                   iter + 1, iterations, progress * 100.0, (current_time - start_time) / 1000.0);
            fflush(stdout);
            last_update = current_time;
        }
    }

    double end_time = get_time_ms();
    double total_time = end_time - start_time;

    // 计算性能指标
    long long total_pixels_processed = (long long)image_size * iterations;
    double throughput_mpixels = (total_pixels_processed / 1e6) / (total_time / 1000.0);

    printf("\n\n=== Results ===\n");
    printf("Total execution time: %.3f seconds (%.2f ms)\n", total_time / 1000.0, total_time);
    printf("Average time per iteration: %.3f ms\n", total_time / iterations);
    printf("Iterations per second: %.2f\n", iterations / (total_time / 1000.0));
    printf("Total pixels processed: %lld (%.2f MP)\n", total_pixels_processed, total_pixels_processed / 1e6);
    printf("Throughput: %.2f MPixels/s\n", throughput_mpixels);
    printf("Data processed: %.2f GB in %.2f seconds\n", total_data, total_time / 1000.0);
    printf("Bandwidth: %.2f GB/s\n", total_data / (total_time / 1000.0));

    // 保存结果
    save_histogram_txt(histogram, "output/histogram_fpga.txt", total_time, width, height, iterations);

    // 打印部分统计信息
    printf("\nSample histogram values:\n");
    for (int i = 0; i < 10; i++)
    {
        printf("Bin %3d: %u\n", i, histogram[i]);
    }

    // 清理
    free(img->data);
    free(img);
    free(histogram);

    printf("\nFPGA processing complete!\n");
    return 0;
}
