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
            // 添加一些随机性，让每张图像略有不同
            img->data[i * width + j] = (i * 13 + j * 7) % 256;
        }
    }
    return img;
}

// CPU版本的直方图计算
void compute_histogram_cpu(unsigned char *image, int size, unsigned int *histogram)
{
    memset(histogram, 0, HISTOGRAM_BINS * sizeof(unsigned int));

    for (int i = 0; i < size; i++)
    {
        histogram[image[i]]++;
    }
}

// 保存直方图到文件
void save_histogram_txt(unsigned int *histogram, const char *filename)
{
    FILE *fp = fopen(filename, "w");
    if (!fp)
    {
        printf("Error: Cannot open file %s\n", filename);
        return;
    }

    fprintf(fp, "# Histogram Data (Bin, Count)\n");
    for (int i = 0; i < HISTOGRAM_BINS; i++)
    {
        fprintf(fp, "%d %u\n", i, histogram[i]);
    }
    fclose(fp);
}

// 计时函数
double get_time_ms()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

// 打印进度条
void print_progress(int current, int total, double elapsed_time)
{
    int barWidth = 50;
    float progress = (float)current / total;

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
           current, total, progress * 100.0, elapsed_time / 1000.0);
    fflush(stdout);
}

int main(int argc, char **argv)
{
    int width = 3840;      // 4K width
    int height = 2160;     // 4K height
    int iterations = 1000; // 默认1000次迭代

    // 可以通过命令行调整
    if (argc >= 3)
    {
        width = atoi(argv[1]);
        height = atoi(argv[2]);
    }
    if (argc >= 4)
    {
        iterations = atoi(argv[3]);
    }

    printf("=== CPU Histogram Computation (Long Run) ===\n");
    printf("Image size: %dx%d (%.2f MP)\n", width, height, (width * height) / 1e6);
    printf("Iterations: %d\n", iterations);

    long long total_pixels = (long long)width * height;
    double total_data = (total_pixels * iterations) / (1024.0 * 1024.0 * 1024.0);
    printf("Total data to process: %.2f GB\n", total_data);
    printf("\n");

    // 创建测试图像
    printf("Generating test image...\n");
    Image *img = create_test_image(width, height);
    int image_size = width * height;

    // 分配直方图内存
    unsigned int *histogram = (unsigned int *)malloc(HISTOGRAM_BINS * sizeof(unsigned int));

    // 预热
    printf("Warming up...\n");
    compute_histogram_cpu(img->data, image_size, histogram);
    printf("Starting benchmark...\n\n");

    // 主循环 - 运行多次
    double start_time = get_time_ms();
    double last_update = start_time;

    for (int iter = 0; iter < iterations; iter++)
    {
        compute_histogram_cpu(img->data, image_size, histogram);

        // 每100ms更新一次进度
        double current_time = get_time_ms();
        if (current_time - last_update > 100 || iter == iterations - 1)
        {
            print_progress(iter + 1, iterations, current_time - start_time);
            last_update = current_time;
        }
    }

    double end_time = get_time_ms();
    double total_time = end_time - start_time;

    // 修复：使用 long long 避免整数溢出
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

    // 保存最后一次结果
    save_histogram_txt(histogram, "output/histogram_cpu.txt");
    printf("\nHistogram saved to output/histogram_cpu.txt\n");

    // 打印部分统计信息
    printf("\nSample histogram values:\n");
    for (int i = 0; i < 10; i++)
    {
        printf("Bin %3d: %u\n", i, histogram[i]);
    }

    // 验证结果
    unsigned long long sum = 0;
    for (int i = 0; i < HISTOGRAM_BINS; i++)
    {
        sum += histogram[i];
    }
    printf("\nVerification: Total pixel count = %llu (expected: %d)\n", sum, image_size);
    if (sum == image_size)
    {
        printf("✓ Result is CORRECT!\n");
    }

    // 清理
    free(img->data);
    free(img);
    free(histogram);

    printf("\n=== Summary ===\n");
    printf("If GPU/FPGA can achieve 50x speedup:\n");
    printf("  Expected GPU time: %.2f seconds\n", total_time / 1000.0 / 50.0);
    printf("If GPU/FPGA can achieve 100x speedup:\n");
    printf("  Expected GPU time: %.2f seconds\n", total_time / 1000.0 / 100.0);

    return 0;
}