// OpenCL Kernel for Histogram Computation
// 使用原子操作实现直方图统计
// HISTOGRAM_BINS 通过编译选项 -D 定义

__kernel void compute_histogram(__global const unsigned char* image,
                                 __global unsigned int* histogram,
                                 const int image_size) {
    int id = get_global_id(0);
    
    // 每个work item处理一个像素
    if (id < image_size) {
        unsigned char pixel_value = image[id];
        // 使用原子操作更新直方图
        atomic_inc(&histogram[pixel_value]);
    }
}

// 优化版本：使用局部内存减少全局内存访问
__kernel void compute_histogram_local(__global const unsigned char* image,
                                      __global unsigned int* histogram,
                                      const int image_size) {
    int local_id = get_local_id(0);
    int global_id = get_global_id(0);
    
    // 使用局部内存存储每个workgroup的局部直方图
    __local unsigned int local_histogram[HISTOGRAM_BINS];
    
    // 初始化局部直方图
    if (local_id < HISTOGRAM_BINS) {
        local_histogram[local_id] = 0;
    }
    barrier(CLK_LOCAL_MEM_FENCE);
    
    // 每个work item处理多个像素以减少全局内存访问
    int pixels_per_workitem = (image_size + get_global_size(0) - 1) / get_global_size(0);
    int start_idx = global_id * pixels_per_workitem;
    int end_idx = start_idx + pixels_per_workitem;
    if (end_idx > image_size) {
        end_idx = image_size;
    }
    
    // 计算局部直方图
    for (int i = start_idx; i < end_idx; i++) {
        unsigned char pixel_value = image[i];
        atomic_inc(&local_histogram[pixel_value]);
    }
    barrier(CLK_LOCAL_MEM_FENCE);
    
    // 将局部直方图合并到全局直方图
    if (local_id < HISTOGRAM_BINS) {
        atomic_add(&histogram[local_id], local_histogram[local_id]);
    }
}
