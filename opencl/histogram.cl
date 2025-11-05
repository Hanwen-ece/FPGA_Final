// histogram_kernel.cl
// Multiple optimized kernels for histogram computation

// Kernel 1: Naive版本（最简单，使用atomic操作）
__kernel void histogram_naive(
    __global unsigned char *image,
    __global unsigned int *histogram,
    int image_size)
{
    int gid = get_global_id(0);
    
    if (gid < image_size) {
        unsigned char pixel = image[gid];
        atomic_inc(&histogram[pixel]);
    }
}

// Kernel 2: Local Memory优化版本（减少global memory竞争）
__kernel void histogram_local(
    __global unsigned char *image,
    __global unsigned int *histogram,
    int image_size,
    __local unsigned int *local_hist)
{
    int gid = get_global_id(0);
    int lid = get_local_id(0);
    int local_size = get_local_size(0);
    
    // 初始化local histogram
    for (int i = lid; i < 256; i += local_size) {
        local_hist[i] = 0;
    }
    barrier(CLK_LOCAL_MEM_FENCE);
    
    // 每个work-item统计到local histogram
    if (gid < image_size) {
        unsigned char pixel = image[gid];
        atomic_inc(&local_hist[pixel]);
    }
    barrier(CLK_LOCAL_MEM_FENCE);
    
    // 合并local histogram到global histogram
    for (int i = lid; i < 256; i += local_size) {
        if (local_hist[i] > 0) {
            atomic_add(&histogram[i], local_hist[i]);
        }
    }
}

// Kernel 3: Private Histogram优化（每个work-item私有直方图）
__kernel void histogram_private(
    __global unsigned char *image,
    __global unsigned int *histogram,
    int image_size,
    __local unsigned int *local_hist,
    int pixels_per_workitem)
{
    int gid = get_global_id(0);
    int lid = get_local_id(0);
    int local_size = get_local_size(0);
    
    // 初始化local histogram
    for (int i = lid; i < 256; i += local_size) {
        local_hist[i] = 0;
    }
    barrier(CLK_LOCAL_MEM_FENCE);
    
    // 每个work-item处理多个像素
    int start = gid * pixels_per_workitem;
    int end = min(start + pixels_per_workitem, image_size);
    
    for (int i = start; i < end; i++) {
        unsigned char pixel = image[i];
        atomic_inc(&local_hist[pixel]);
    }
    barrier(CLK_LOCAL_MEM_FENCE);
    
    // 合并到global
    for (int i = lid; i < 256; i += local_size) {
        if (local_hist[i] > 0) {
            atomic_add(&histogram[i], local_hist[i]);
        }
    }
}

// Kernel 4: Vectorized版本（使用uchar4向量化加载）
__kernel void histogram_vectorized(
    __global uchar4 *image,
    __global unsigned int *histogram,
    int num_vectors,
    __local unsigned int *local_hist)
{
    int gid = get_global_id(0);
    int lid = get_local_id(0);
    int local_size = get_local_size(0);
    
    // 初始化local histogram
    for (int i = lid; i < 256; i += local_size) {
        local_hist[i] = 0;
    }
    barrier(CLK_LOCAL_MEM_FENCE);
    
    // 向量化加载和处理
    if (gid < num_vectors) {
        uchar4 pixels = image[gid];
        atomic_inc(&local_hist[pixels.x]);
        atomic_inc(&local_hist[pixels.y]);
        atomic_inc(&local_hist[pixels.z]);
        atomic_inc(&local_hist[pixels.w]);
    }
    barrier(CLK_LOCAL_MEM_FENCE);
    
    // 合并到global
    for (int i = lid; i < 256; i += local_size) {
        if (local_hist[i] > 0) {
            atomic_add(&histogram[i], local_hist[i]);
        }
    }
}