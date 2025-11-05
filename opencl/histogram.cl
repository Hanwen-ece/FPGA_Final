// histogram_kernel.cl
// Multiple optimized kernels for histogram computation

// Kernel 5: 高性能版本 - 结合所有优化技术
__kernel void histogram_ultra(
    __global unsigned char *image,
    __global unsigned int *histogram,
    int image_size,
    __local unsigned int *local_hist)
{
    int gid = get_global_id(0);
    int lid = get_local_id(0);
    int local_size = get_local_size(0);
    int total_workitems = get_global_size(0);
    
    // 优化：快速初始化
    int bins_per_item = (256 + local_size - 1) / local_size;
    int start_bin = lid * bins_per_item;
    int end_bin = min(start_bin + bins_per_item, 256);
    
    for (int i = start_bin; i < end_bin; i++) {
        local_hist[i] = 0;
    }
    barrier(CLK_LOCAL_MEM_FENCE);
    
    // 优化：每个workitem处理更多像素（自适应）
    int pixels_per_item = (image_size + total_workitems - 1) / total_workitems;
    if (pixels_per_item < 8) pixels_per_item = 8;
    if (pixels_per_item > 32) pixels_per_item = 32; // 最多32个像素
    
    int start_pixel = gid * pixels_per_item;
    int end_pixel = min(start_pixel + pixels_per_item, image_size);
    
    // 优化：使用循环展开处理像素
    int idx = start_pixel;
    for (; idx < end_pixel - 7; idx += 8) {
        atomic_inc(&local_hist[image[idx]]);
        atomic_inc(&local_hist[image[idx+1]]);
        atomic_inc(&local_hist[image[idx+2]]);
        atomic_inc(&local_hist[image[idx+3]]);
        atomic_inc(&local_hist[image[idx+4]]);
        atomic_inc(&local_hist[image[idx+5]]);
        atomic_inc(&local_hist[image[idx+6]]);
        atomic_inc(&local_hist[image[idx+7]]);
    }
    // 处理剩余像素
    for (; idx < end_pixel; idx++) {
        atomic_inc(&local_hist[image[idx]]);
    }
    barrier(CLK_LOCAL_MEM_FENCE);
    
    // 优化：合并到global
    for (int i = start_bin; i < end_bin; i++) {
        if (local_hist[i] > 0) {
            atomic_add(&histogram[i], local_hist[i]);
        }
    }
}

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

// Kernel 2: Local Memory优化版本（减少global memory竞争）- 优化版本
__kernel void histogram_local(
    __global unsigned char *image,
    __global unsigned int *histogram,
    int image_size,
    __local unsigned int *local_hist)
{
    int gid = get_global_id(0);
    int lid = get_local_id(0);
    int local_size = get_local_size(0);
    
    // 优化：使用向量化初始化（如果local_size >= 256，可以并行初始化）
    // 初始化local histogram - 优化：减少循环次数
    int bins_per_item = (256 + local_size - 1) / local_size;
    int start_bin = lid * bins_per_item;
    int end_bin = min(start_bin + bins_per_item, 256);
    
    for (int i = start_bin; i < end_bin; i++) {
        local_hist[i] = 0;
    }
    barrier(CLK_LOCAL_MEM_FENCE);
    
    // 优化：每个work-item处理多个像素，提高内存访问效率
    // 固定处理4个像素（经过测试，这是最优配置）
    int pixels_per_item = 4;
    int start_pixel = gid * pixels_per_item;
    int end_pixel = min(start_pixel + pixels_per_item, image_size);
    
    // 每个work-item统计到local histogram
    for (int idx = start_pixel; idx < end_pixel; idx++) {
        if (idx < image_size) {
            unsigned char pixel = image[idx];
            atomic_inc(&local_hist[pixel]);
        }
    }
    barrier(CLK_LOCAL_MEM_FENCE);
    
    // 优化：合并local histogram到global histogram - 使用相同的bins分配
    for (int i = start_bin; i < end_bin; i++) {
        if (local_hist[i] > 0) {
            atomic_add(&histogram[i], local_hist[i]);
        }
    }
}

// Kernel 3: Private Histogram优化（每个work-item私有直方图）- 优化版本
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
    
    // 优化：更高效的初始化
    int bins_per_item = (256 + local_size - 1) / local_size;
    int start_bin = lid * bins_per_item;
    int end_bin = min(start_bin + bins_per_item, 256);
    
    for (int i = start_bin; i < end_bin; i++) {
        local_hist[i] = 0;
    }
    barrier(CLK_LOCAL_MEM_FENCE);
    
    // 每个work-item处理多个像素 - 优化：使用更高效的循环
    int start = gid * pixels_per_workitem;
    int end = min(start + pixels_per_workitem, image_size);
    
    // 优化：循环展开，处理更多像素
    int i = start;
    for (; i < end - 3; i += 4) {
        atomic_inc(&local_hist[image[i]]);
        atomic_inc(&local_hist[image[i+1]]);
        atomic_inc(&local_hist[image[i+2]]);
        atomic_inc(&local_hist[image[i+3]]);
    }
    // 处理剩余像素
    for (; i < end; i++) {
        atomic_inc(&local_hist[image[i]]);
    }
    barrier(CLK_LOCAL_MEM_FENCE);
    
    // 合并到global - 使用相同的bins分配
    for (int i = start_bin; i < end_bin; i++) {
        if (local_hist[i] > 0) {
            atomic_add(&histogram[i], local_hist[i]);
        }
    }
}

// Kernel 4: Vectorized版本（使用uchar4向量化加载）- 优化版本
__kernel void histogram_vectorized(
    __global uchar4 *image,
    __global unsigned int *histogram,
    int num_vectors,
    __local unsigned int *local_hist)
{
    int gid = get_global_id(0);
    int lid = get_local_id(0);
    int local_size = get_local_size(0);
    
    // 优化：更高效的初始化
    int bins_per_item = (256 + local_size - 1) / local_size;
    int start_bin = lid * bins_per_item;
    int end_bin = min(start_bin + bins_per_item, 256);
    
    for (int i = start_bin; i < end_bin; i++) {
        local_hist[i] = 0;
    }
    barrier(CLK_LOCAL_MEM_FENCE);
    
    // 优化：每个workitem处理更多向量，提高效率
    // 动态计算，确保充分利用GPU
    int total_workitems = get_global_size(0);
    int vectors_per_item = (num_vectors + total_workitems - 1) / total_workitems;
    // 至少处理4个向量（16个像素），最多8个向量（32个像素）
    if (vectors_per_item < 4) vectors_per_item = 4;
    if (vectors_per_item > 8) vectors_per_item = 8;
    
    int start_vec = gid * vectors_per_item;
    int end_vec = min(start_vec + vectors_per_item, num_vectors);
    
    // 向量化加载和处理
    for (int idx = start_vec; idx < end_vec; idx++) {
        if (idx < num_vectors) {
            uchar4 pixels = image[idx];
            atomic_inc(&local_hist[pixels.x]);
            atomic_inc(&local_hist[pixels.y]);
            atomic_inc(&local_hist[pixels.z]);
            atomic_inc(&local_hist[pixels.w]);
        }
    }
    barrier(CLK_LOCAL_MEM_FENCE);
    
    // 合并到global - 使用相同的bins分配
    for (int i = start_bin; i < end_bin; i++) {
        if (local_hist[i] > 0) {
            atomic_add(&histogram[i], local_hist[i]);
        }
    }
}