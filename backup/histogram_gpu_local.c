#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

#define HISTOGRAM_BINS 256
#define MAX_SOURCE_SIZE (0x100000)

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

// 读取OpenCL kernel文件
char* read_kernel_file(const char* filename) {
    FILE *fp = fopen(filename, "r");
    const char* actual_file = filename;
    
    // 如果找不到，尝试在 opencl 文件夹中查找
    if (!fp) {
        char alt_path[256];
        snprintf(alt_path, sizeof(alt_path), "opencl/%s", filename);
        fp = fopen(alt_path, "r");
        if (fp) {
            actual_file = alt_path;
        }
    }
    
    if (!fp) {
        fprintf(stderr, "Error: Cannot open kernel file %s (also tried opencl/%s)\n", filename, filename);
        return NULL;
    }
    
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);
    
    char *source = (char*)malloc(size + 1);
    fread(source, 1, size, fp);
    source[size] = '\0';
    fclose(fp);
    
    return source;
}

// 计时函数（跨平台）
double get_time_ms() {
#ifdef _WIN32
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart * 1000.0 / frequency.QuadPart;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
#endif
}

// 保存直方图到文件
void save_histogram_txt(unsigned int *histogram, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("Error: Cannot open file %s\n", filename);
        return;
    }
    
    fprintf(fp, "# Histogram Data (Bin, Count)\n");
    for (int i = 0; i < HISTOGRAM_BINS; i++) {
        fprintf(fp, "%d %u\n", i, histogram[i]);
    }
    fclose(fp);
    printf("Histogram saved to %s\n", filename);
}

// OpenCL直方图计算
int compute_histogram_gpu(unsigned char *image, int size, unsigned int *histogram, 
                          cl_device_id device_id, cl_context context, cl_command_queue queue,
                          cl_program program, cl_kernel kernel, int use_local_memory) {
    cl_int err;
    
    // 创建buffer
    cl_mem image_buffer = clCreateBuffer(context, CL_MEM_READ_ONLY, size * sizeof(unsigned char), NULL, &err);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Error creating image buffer: %d\n", err);
        return -1;
    }
    
    cl_mem histogram_buffer = clCreateBuffer(context, CL_MEM_READ_WRITE, HISTOGRAM_BINS * sizeof(unsigned int), NULL, &err);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Error creating histogram buffer: %d\n", err);
        clReleaseMemObject(image_buffer);
        return -1;
    }
    
    // 初始化直方图
    unsigned int zero = 0;
    err = clEnqueueFillBuffer(queue, histogram_buffer, &zero, sizeof(unsigned int), 0, 
                               HISTOGRAM_BINS * sizeof(unsigned int), 0, NULL, NULL);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Error filling histogram buffer: %d\n", err);
        clReleaseMemObject(image_buffer);
        clReleaseMemObject(histogram_buffer);
        return -1;
    }
    
    // 写入图像数据
    err = clEnqueueWriteBuffer(queue, image_buffer, CL_TRUE, 0, size * sizeof(unsigned char), 
                                image, 0, NULL, NULL);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Error writing image buffer: %d\n", err);
        clReleaseMemObject(image_buffer);
        clReleaseMemObject(histogram_buffer);
        return -1;
    }
    
    // 设置kernel参数
    err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &image_buffer);
    err |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &histogram_buffer);
    err |= clSetKernelArg(kernel, 2, sizeof(int), &size);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Error setting kernel arguments: %d\n", err);
        clReleaseMemObject(image_buffer);
        clReleaseMemObject(histogram_buffer);
        return -1;
    }
    
    // 设置workgroup大小
    size_t local_work_size = 256; // 可以根据设备调整
    size_t global_work_size = ((size + local_work_size - 1) / local_work_size) * local_work_size;
    
    // 执行kernel
    err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_work_size, &local_work_size, 0, NULL, NULL);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Error enqueueing kernel: %d\n", err);
        clReleaseMemObject(image_buffer);
        clReleaseMemObject(histogram_buffer);
        return -1;
    }
    
    // 读取结果
    err = clEnqueueReadBuffer(queue, histogram_buffer, CL_TRUE, 0, 
                               HISTOGRAM_BINS * sizeof(unsigned int), histogram, 0, NULL, NULL);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Error reading histogram buffer: %d\n", err);
        clReleaseMemObject(image_buffer);
        clReleaseMemObject(histogram_buffer);
        return -1;
    }
    
    // 清理
    clReleaseMemObject(image_buffer);
    clReleaseMemObject(histogram_buffer);
    
    return 0;
}

int main(int argc, char **argv) {
    int width = 1920;
    int height = 1080;
    int use_local = 0; // 0 = 简单版本, 1 = 局部内存优化版本
    
    if (argc >= 3) {
        width = atoi(argv[1]);
        height = atoi(argv[2]);
    }
    if (argc >= 4) {
        use_local = atoi(argv[3]);
    }
    
    printf("=== OpenCL GPU Histogram Computation ===\n");
    printf("Image size: %dx%d\n", width, height);
    
    // 创建测试图像
    Image *img = create_test_image(width, height);
    int image_size = width * height;
    
    // 分配直方图内存
    unsigned int *histogram = (unsigned int*)malloc(HISTOGRAM_BINS * sizeof(unsigned int));
    
    // OpenCL初始化
    cl_platform_id platform_id = NULL;
    cl_device_id device_id = NULL;
    cl_uint num_devices, num_platforms;
    cl_int err;
    
    // 获取平台
    err = clGetPlatformIDs(1, &platform_id, &num_platforms);
    if (err != CL_SUCCESS || num_platforms == 0) {
        fprintf(stderr, "Error: No OpenCL platforms found\n");
        free(img->data);
        free(img);
        free(histogram);
        return -1;
    }
    
    // 获取设备（优先使用GPU）
    err = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_GPU, 1, &device_id, &num_devices);
    if (err != CL_SUCCESS || num_devices == 0) {
        // 如果没有GPU，尝试使用CPU
        printf("No GPU found, using CPU...\n");
        err = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_CPU, 1, &device_id, &num_devices);
        if (err != CL_SUCCESS || num_devices == 0) {
            fprintf(stderr, "Error: No OpenCL devices found\n");
            free(img->data);
            free(img);
            free(histogram);
            return -1;
        }
    }
    
    // 显示设备信息
    char device_name[256];
    clGetDeviceInfo(device_id, CL_DEVICE_NAME, sizeof(device_name), device_name, NULL);
    printf("Using device: %s\n", device_name);
    
    // 创建上下文和命令队列
    cl_context context = clCreateContext(NULL, 1, &device_id, NULL, NULL, &err);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Error creating context: %d\n", err);
        free(img->data);
        free(img);
        free(histogram);
        return -1;
    }
    
    cl_command_queue queue = clCreateCommandQueue(context, device_id, CL_QUEUE_PROFILING_ENABLE, &err);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Error creating command queue: %d\n", err);
        clReleaseContext(context);
        free(img->data);
        free(img);
        free(histogram);
        return -1;
    }
    
    // 读取kernel文件（在opencl文件夹中）
    char *kernel_source = read_kernel_file("opencl/histogram.cl");
    if (!kernel_source) {
        clReleaseCommandQueue(queue);
        clReleaseContext(context);
        free(img->data);
        free(img);
        free(histogram);
        return -1;
    }
    
    // 创建并编译program
    cl_program program = clCreateProgramWithSource(context, 1, (const char**)&kernel_source, NULL, &err);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Error creating program: %d\n", err);
        free(kernel_source);
        clReleaseCommandQueue(queue);
        clReleaseContext(context);
        free(img->data);
        free(img);
        free(histogram);
        return -1;
    }
    
    // 添加HISTOGRAM_BINS宏定义
    char build_options[256];
    sprintf(build_options, "-D HISTOGRAM_BINS=%d", HISTOGRAM_BINS);
    
    err = clBuildProgram(program, 1, &device_id, build_options, NULL, NULL);
    if (err != CL_SUCCESS) {
        size_t log_size;
        clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        char *log = (char*)malloc(log_size);
        clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
        fprintf(stderr, "Error building program:\n%s\n", log);
        free(log);
        free(kernel_source);
        clReleaseProgram(program);
        clReleaseCommandQueue(queue);
        clReleaseContext(context);
        free(img->data);
        free(img);
        free(histogram);
        return -1;
    }
    
    // 创建kernel
    const char *kernel_name = use_local ? "compute_histogram_local" : "compute_histogram";
    cl_kernel kernel = clCreateKernel(program, kernel_name, &err);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Error creating kernel '%s': %d\n", kernel_name, err);
        free(kernel_source);
        clReleaseProgram(program);
        clReleaseCommandQueue(queue);
        clReleaseContext(context);
        free(img->data);
        free(img);
        free(histogram);
        return -1;
    }
    
    printf("Kernel: %s\n", kernel_name);
    
    // 预热
    compute_histogram_gpu(img->data, image_size, histogram, device_id, context, queue, 
                          program, kernel, use_local);
    
    // 多次运行取平均
    int iterations = 10;
    double total_time = 0.0;
    
    for (int iter = 0; iter < iterations; iter++) {
        double start = get_time_ms();
        compute_histogram_gpu(img->data, image_size, histogram, device_id, context, queue, 
                              program, kernel, use_local);
        double end = get_time_ms();
        total_time += (end - start);
    }
    
    double avg_time = total_time / iterations;
    printf("Average execution time: %.3f ms\n", avg_time);
    printf("Throughput: %.2f MPixels/s\n", (image_size / 1e6) / (avg_time / 1000.0));
    
    // 保存结果
    save_histogram_txt(histogram, "histogram_gpu.txt");
    
    // 打印部分统计信息
    printf("\nSample histogram values:\n");
    for (int i = 0; i < 10; i++) {
        printf("Bin %d: %u\n", i, histogram[i]);
    }
    
    // 清理
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);
    free(kernel_source);
    free(img->data);
    free(img);
    free(histogram);
    
    return 0;
}
