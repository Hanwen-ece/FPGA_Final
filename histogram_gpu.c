#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

#define HISTOGRAM_BINS 256
#define MAX_SOURCE_SIZE (0x100000)

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
            img->data[i * width + j] = (i * 13 + j * 7) % 256;
        }
    }
    return img;
}

// 计时函数
double get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

// 读取kernel文件
char* read_kernel_source(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Failed to load kernel file: %s\n", filename);
        exit(1);
    }
    char *source_str = (char*)malloc(MAX_SOURCE_SIZE);
    size_t source_size = fread(source_str, 1, MAX_SOURCE_SIZE, fp);
    source_str[source_size] = '\0';
    fclose(fp);
    return source_str;
}

// 检查OpenCL错误
void check_error(cl_int err, const char *operation) {
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Error during operation '%s': %d\n", operation, err);
        exit(1);
    }
}

// 打印进度条
void print_progress(int current, int total, double elapsed_time) {
    int barWidth = 50;
    float progress = (float)current / total;
    
    printf("\r[");
    int pos = barWidth * progress;
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) printf("=");
        else if (i == pos) printf(">");
        else printf(" ");
    }
    printf("] %d/%d (%.1f%%) - %.2fs elapsed", 
           current, total, progress * 100.0, elapsed_time / 1000.0);
    fflush(stdout);
}

// 验证结果
int verify_histogram(unsigned int *histogram, int expected_total) {
    unsigned long long sum = 0;
    for (int i = 0; i < HISTOGRAM_BINS; i++) {
        sum += histogram[i];
    }
    return (sum == expected_total);
}

int main(int argc, char **argv) {
    int width = 3840;
    int height = 2160;
    int iterations = 1000;
    int kernel_choice = 2;  // 默认使用local memory版本
    
    if (argc >= 3) {
        width = atoi(argv[1]);
        height = atoi(argv[2]);
    }
    if (argc >= 4) {
        iterations = atoi(argv[3]);
    }
    if (argc >= 5) {
        kernel_choice = atoi(argv[4]);
    }
    
    printf("=== OpenCL GPU Histogram Computation ===\n");
    printf("Image size: %dx%d (%.2f MP)\n", width, height, (width * height) / 1e6);
    printf("Iterations: %d\n", iterations);
    
    long long total_pixels = (long long)width * height;
    double total_data = (total_pixels * iterations) / (1024.0 * 1024.0 * 1024.0);
    printf("Total data to process: %.2f GB\n\n", total_data);
    
    // 创建测试图像
    printf("Generating test image...\n");
    Image *img = create_test_image(width, height);
    int image_size = width * height;
    
    // OpenCL初始化
    printf("Initializing OpenCL...\n");
    cl_platform_id platform_id = NULL;
    cl_device_id device_id = NULL;
    cl_uint ret_num_devices;
    cl_uint ret_num_platforms;
    cl_int ret;
    
    ret = clGetPlatformIDs(1, &platform_id, &ret_num_platforms);
    check_error(ret, "clGetPlatformIDs");
    
    ret = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_GPU, 1, &device_id, &ret_num_devices);
    if (ret != CL_SUCCESS) {
        printf("No GPU found, trying CPU...\n");
        ret = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_CPU, 1, &device_id, &ret_num_devices);
    }
    check_error(ret, "clGetDeviceIDs");
    
    // 打印设备信息
    char device_name[128];
    char device_vendor[128];
    cl_uint compute_units;
    size_t max_work_group_size;
    cl_ulong global_mem_size;
    cl_ulong local_mem_size;
    
    clGetDeviceInfo(device_id, CL_DEVICE_NAME, sizeof(device_name), device_name, NULL);
    clGetDeviceInfo(device_id, CL_DEVICE_VENDOR, sizeof(device_vendor), device_vendor, NULL);
    clGetDeviceInfo(device_id, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(compute_units), &compute_units, NULL);
    clGetDeviceInfo(device_id, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(max_work_group_size), &max_work_group_size, NULL);
    clGetDeviceInfo(device_id, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(global_mem_size), &global_mem_size, NULL);
    clGetDeviceInfo(device_id, CL_DEVICE_LOCAL_MEM_SIZE, sizeof(local_mem_size), &local_mem_size, NULL);
    
    printf("\n=== Device Information ===\n");
    printf("Device: %s\n", device_name);
    printf("Vendor: %s\n", device_vendor);
    printf("Compute Units: %u\n", compute_units);
    printf("Max Work Group Size: %zu\n", max_work_group_size);
    printf("Global Memory: %.2f GB\n", global_mem_size / (1024.0 * 1024.0 * 1024.0));
    printf("Local Memory: %.2f KB\n\n", local_mem_size / 1024.0);
    
    // 创建context和command queue
    cl_context context = clCreateContext(NULL, 1, &device_id, NULL, NULL, &ret);
    check_error(ret, "clCreateContext");
    
    cl_command_queue command_queue = clCreateCommandQueue(context, device_id, 0, &ret);
    check_error(ret, "clCreateCommandQueue");
    
    // 读取并编译kernel
    printf("Loading and compiling kernels...\n");
    char *kernel_source = read_kernel_source("histogram_kernel.cl");
    size_t source_size = strlen(kernel_source);
    
    cl_program program = clCreateProgramWithSource(context, 1, (const char **)&kernel_source, 
                                                   (const size_t *)&source_size, &ret);
    check_error(ret, "clCreateProgramWithSource");
    
    ret = clBuildProgram(program, 1, &device_id, NULL, NULL, NULL);
    if (ret != CL_SUCCESS) {
        size_t len;
        char buffer[4096];
        clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
        fprintf(stderr, "Build error:\n%s\n", buffer);
        exit(1);
    }
    
    // 选择kernel
    const char *kernel_names[] = {
        "histogram_naive",
        "histogram_local",
        "histogram_private",
        "histogram_vectorized"
    };
    
    const char *kernel_descriptions[] = {
        "Naive (simple atomic)",
        "Local Memory (optimized)",
        "Private Histogram",
        "Vectorized (uchar4)"
    };
    
    if (kernel_choice < 1 || kernel_choice > 4) {
        kernel_choice = 2;  // 默认local memory
    }
    
    printf("Using kernel: %s\n", kernel_descriptions[kernel_choice - 1]);
    
    cl_kernel kernel = clCreateKernel(program, kernel_names[kernel_choice - 1], &ret);
    check_error(ret, "clCreateKernel");
    
    // 创建缓冲区
    cl_mem image_buffer = clCreateBuffer(context, CL_MEM_READ_ONLY, 
                                         image_size * sizeof(unsigned char), NULL, &ret);
    check_error(ret, "clCreateBuffer image");
    
    cl_mem histogram_buffer = clCreateBuffer(context, CL_MEM_READ_WRITE, 
                                             HISTOGRAM_BINS * sizeof(unsigned int), NULL, &ret);
    check_error(ret, "clCreateBuffer histogram");
    
    // 写入图像数据
    ret = clEnqueueWriteBuffer(command_queue, image_buffer, CL_TRUE, 0, 
                               image_size * sizeof(unsigned char), img->data, 0, NULL, NULL);
    check_error(ret, "clEnqueueWriteBuffer");
    
    // 设置kernel参数和工作组大小
    size_t local_size = 256;
    size_t global_size = ((image_size + local_size - 1) / local_size) * local_size;
    
    ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&image_buffer);
    ret |= clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *)&histogram_buffer);
    ret |= clSetKernelArg(kernel, 2, sizeof(int), (void *)&image_size);
    
    // 如果使用local memory的kernel，设置local memory参数
    if (kernel_choice >= 2) {
        ret |= clSetKernelArg(kernel, 3, HISTOGRAM_BINS * sizeof(unsigned int), NULL);
    }
    
    check_error(ret, "clSetKernelArg");
    
    printf("\nWork configuration:\n");
    printf("  Global work size: %zu\n", global_size);
    printf("  Local work size: %zu\n", local_size);
    printf("  Work groups: %zu\n\n", global_size / local_size);
    
    // 预热
    printf("Warming up...\n");
    unsigned int zeros[HISTOGRAM_BINS] = {0};
    ret = clEnqueueWriteBuffer(command_queue, histogram_buffer, CL_TRUE, 0, 
                               HISTOGRAM_BINS * sizeof(unsigned int), zeros, 0, NULL, NULL);
    ret = clEnqueueNDRangeKernel(command_queue, kernel, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
    clFinish(command_queue);
    
    printf("Starting benchmark...\n\n");
    
    // 主循环
    double start_time = get_time_ms();
    double last_update = start_time;
    
    for (int iter = 0; iter < iterations; iter++) {
        // 重置histogram
        ret = clEnqueueWriteBuffer(command_queue, histogram_buffer, CL_FALSE, 0, 
                                   HISTOGRAM_BINS * sizeof(unsigned int), zeros, 0, NULL, NULL);
        
        // 执行kernel
        ret = clEnqueueNDRangeKernel(command_queue, kernel, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
        
        // 每100次迭代或最后一次才同步
        if ((iter + 1) % 100 == 0 || iter == iterations - 1) {
            clFinish(command_queue);
            
            double current_time = get_time_ms();
            if (current_time - last_update > 100 || iter == iterations - 1) {
                print_progress(iter + 1, iterations, current_time - start_time);
                last_update = current_time;
            }
        }
    }
    
    clFinish(command_queue);
    double end_time = get_time_ms();
    double total_time = end_time - start_time;
    
    // 读取结果
    unsigned int *histogram = (unsigned int*)malloc(HISTOGRAM_BINS * sizeof(unsigned int));
    ret = clEnqueueReadBuffer(command_queue, histogram_buffer, CL_TRUE, 0, 
                              HISTOGRAM_BINS * sizeof(unsigned int), histogram, 0, NULL, NULL);
    check_error(ret, "clEnqueueReadBuffer");
    
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
    
    // 验证结果
    printf("\nSample histogram values:\n");
    for (int i = 0; i < 10; i++) {
        printf("Bin %3d: %u\n", i, histogram[i]);
    }
    
    if (verify_histogram(histogram, image_size)) {
        printf("\n✓ Result is CORRECT!\n");
    } else {
        printf("\n✗ Result is INCORRECT!\n");
    }
    
    // 清理
    clReleaseMemObject(image_buffer);
    clReleaseMemObject(histogram_buffer);
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(command_queue);
    clReleaseContext(context);
    
    free(kernel_source);
    free(histogram);
    free(img->data);
    free(img);
    
    printf("\n=== Summary ===\n");
    printf("Kernel used: %s\n", kernel_descriptions[kernel_choice - 1]);
    printf("GPU processing complete!\n");
    
    return 0;
}