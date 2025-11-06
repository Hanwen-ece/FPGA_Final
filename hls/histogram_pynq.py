"""
PYNQ program for Histogram Computation using AXI DMA
参考 lab3 的 pythonDMA.py 实现
通过 DMA 连接 HLS IP 进行直方图计算
"""
from pynq import Overlay, allocate
import numpy as np
import time

# --- 配置常量 ---
HISTOGRAM_BINS = 256
DEFAULT_WIDTH = 3840
DEFAULT_HEIGHT = 2160

def create_test_image(width, height):
    """生成测试图像数据"""
    image_size = width * height
    image_data = np.zeros(image_size, dtype=np.uint8)
    
    # 生成渐变图案用于测试
    for i in range(height):
        for j in range(width):
            image_data[i * width + j] = (i * 13 + j * 7) % 256
    
    return image_data

def compute_histogram_cpu(image_data):
    """CPU版本的直方图计算（用于验证）"""
    histogram = np.zeros(HISTOGRAM_BINS, dtype=np.uint32)
    for pixel in image_data:
        histogram[pixel] += 1
    return histogram

def main(width=DEFAULT_WIDTH, height=DEFAULT_HEIGHT, bitstream_path=None, iterations=1000):
    """
    主函数：使用 PYNQ 通过 DMA 连接 FPGA IP 进行直方图计算
    
    参数:
        width: 图像宽度
        height: 图像高度
        bitstream_path: bitstream文件路径（.bit文件）
        iterations: 迭代次数（用于性能测试）
    """
    print("\n=== Histogram Computation using AXI DMA ===")
    print(f"Image size: {width}x{height} ({width * height / 1e6:.2f} MP)")
    print(f"Iterations: {iterations}")
    
    # --- 加载 overlay 和获取 DMA ---
    if bitstream_path is None:
        # 默认路径，需要根据实际情况修改
        bitstream_path = '/home/xilinx/jupyter_notebooks/histogram_hls.bit'
    
    print(f"\nLoading overlay from: {bitstream_path}")
    try:
        overlay = Overlay(bitstream_path)
        print("Overlay loaded successfully!")
    except Exception as e:
        print(f"Error loading overlay: {e}")
        print("Please ensure the bitstream file exists and path is correct.")
        return False
    
    # 检查可用的 DMA 实例
    # print("Available IPs:", overlay.ip_dict)
    # print("Available DMAs:", overlay.dma_dict)
    
    # 获取 DMA 实例（根据实际硬件配置修改名称）
    # 通常名称类似于 'axi_dma_0' 或 'dma'
    try:
        dma = overlay.axi_dma_0  # 根据实际硬件修改
        print("DMA initialized successfully!")
    except AttributeError:
        print("Error: Could not find DMA instance 'axi_dma_0'")
        print("Available attributes:", dir(overlay))
        return False
    
    # --- 生成测试图像 ---
    print("\nGenerating test image...")
    image_data = create_test_image(width, height)
    image_size = width * height
    print(f"Image generated: {image_size} pixels")
    
    # --- 计算 CPU 参考结果 ---
    print("\nCalculating CPU reference result...")
    start_time = time.time()
    cpu_histogram = compute_histogram_cpu(image_data)
    cpu_time = time.time() - start_time
    print(f"CPU computation time: {cpu_time:.6f} seconds")
    
    # --- 分配 DMA 缓冲区 ---
    print("\nAllocating DMA buffers...")
    # 输入缓冲区：图像数据（uint8）
    tx_buffer = allocate(shape=(image_size,), dtype=np.uint8)
    
    # 输出缓冲区：直方图结果（uint32，256个bins）
    rx_buffer = allocate(shape=(HISTOGRAM_BINS,), dtype=np.uint32)
    
    # --- 准备输入数据 ---
    print("\nPreparing input data for DMA...")
    tx_buffer[:] = image_data[:]
    print(f"Input data prepared: {image_size} bytes")
    
    # --- 性能测试：多次运行 ---
    print(f"\nStarting FPGA acceleration ({iterations} iterations)...")
    total_hw_time = 0.0
    success_count = 0
    
    for iter in range(iterations):
        # 清零输出缓冲区
        rx_buffer[:] = 0
        
        # 开始 DMA 传输
        start_time = time.time()
        
        try:
            # 启动发送和接收通道
            dma.sendchannel.transfer(tx_buffer)
            dma.recvchannel.transfer(rx_buffer)
            
            # 等待传输完成
            dma.sendchannel.wait()
            dma.recvchannel.wait()
            
            hw_time = time.time() - start_time
            total_hw_time += hw_time
            success_count += 1
            
        except Exception as e:
            print(f"Error during DMA transfer (iteration {iter}): {e}")
            continue
        
        # 显示进度
        if (iter + 1) % 100 == 0 or iter == iterations - 1:
            progress = (iter + 1) / iterations * 100
            print(f"Progress: {iter + 1}/{iterations} ({progress:.1f}%)", end='\r')
    
    print()  # 换行
    
    if success_count == 0:
        print("Error: All DMA transfers failed!")
        return False
    
    avg_hw_time = total_hw_time / success_count
    print(f"\nAverage FPGA time: {avg_hw_time:.6f} seconds ({avg_hw_time * 1000:.3f} ms)")
    print(f"Throughput: {image_size / avg_hw_time / 1e6:.2f} MPixels/s")
    
    # --- 验证结果 ---
    print("\n=== Verifying Results ===")
    errors = 0
    max_errors_to_show = 10
    
    for i in range(HISTOGRAM_BINS):
        hw_value = rx_buffer[i]
        cpu_value = cpu_histogram[i]
        
        if hw_value != cpu_value:
            if errors < max_errors_to_show:
                print(f"Mismatch at bin {i}: FPGA={hw_value}, CPU={cpu_value}")
            errors += 1
    
    # --- 显示结果 ---
    if errors == 0:
        print("\n" + "="*50)
        print("*       TEST PASSED!            *")
        print("* Histogram computation correct *")
        print("="*50)
    else:
        print(f"\n" + "="*50)
        print(f"*       TEST FAILED!            *")
        print(f"* Found {errors} mismatches           *")
        print("="*50)
    
    # --- 性能对比 ---
    print(f"\n=== Performance Comparison ===")
    print(f"CPU time (1 iteration): {cpu_time:.6f} seconds ({cpu_time * 1000:.3f} ms)")
    print(f"FPGA time (average): {avg_hw_time:.6f} seconds ({avg_hw_time * 1000:.3f} ms)")
    
    if avg_hw_time > 0:
        speedup = cpu_time / avg_hw_time
        print(f"Speedup: {speedup:.2f}x")
    
    # 显示前10个直方图值
    print("\nFirst 10 histogram values (FPGA):")
    for i in range(min(10, HISTOGRAM_BINS)):
        print(f"  Bin {i:3d}: {rx_buffer[i]:10u} (CPU: {cpu_histogram[i]:10u})")
    
    # --- 清理缓冲区 ---
    tx_buffer.freebuffer()
    rx_buffer.freebuffer()
    
    return errors == 0

if __name__ == "__main__":
    # 可以修改这些参数
    WIDTH = 3840
    HEIGHT = 2160
    ITERATIONS = 1000
    BITSTREAM_PATH = None  # 设置为 None 使用默认路径，或指定完整路径
    
    success = main(
        width=WIDTH,
        height=HEIGHT,
        bitstream_path=BITSTREAM_PATH,
        iterations=ITERATIONS
    )
    
    exit(0 if success else 1)


