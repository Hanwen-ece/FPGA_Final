
from pynq import Overlay, allocate
import numpy as np
import time

# 配置常量
HISTOGRAM_BINS = 256
IMAGE_WIDTH = 32
IMAGE_HEIGHT = 32
IMAGE_SIZE = IMAGE_WIDTH * IMAGE_HEIGHT  # 1024 pixels

def create_test_image(width, height):
    """生成测试图像（和HLS testbench相同的模式）"""
    image_size = width * height
    image_data = np.zeros(image_size, dtype=np.uint8)
    
    for i in range(height):
        for j in range(width):
            image_data[i * width + j] = (i * 13 + j * 7) % 256
    
    return image_data

def compute_histogram_cpu(image_data):
    """CPU参考实现"""
    histogram = np.zeros(HISTOGRAM_BINS, dtype=np.uint32)
    for pixel in image_data:
        histogram[pixel] += 1
    return histogram

def pack_uint8_to_uint32(image_data):
    """将uint8打包成uint32"""
    pixels_per_word = 4
    image_size = len(image_data)
    num_words = (image_size + pixels_per_word - 1) // pixels_per_word
    
    # Padding到4的倍数
    padded_size = num_words * pixels_per_word
    if len(image_data) < padded_size:
        padding = padded_size - len(image_data)
        image_data = np.append(image_data, np.zeros(padding, dtype=np.uint8))
    
    # 转换为uint32
    packed_data = image_data.view(np.uint32)
    return packed_data, num_words

def main(iterations=1000):
    """
    主函数
    
    参数:
        iterations: 迭代次数（默认1000次）
    """
    print("\n" + "="*70)
    print("     Histogram Computation - Performance Test")
    print("="*70)
    print(f"Image size: {IMAGE_WIDTH}x{IMAGE_HEIGHT} = {IMAGE_SIZE} pixels")
    print(f"Iterations: {iterations}")
    
    # --- 加载overlay ---
    print("\nLoading overlay...")
    bitstream_path = '/home/ubuntu/finalProject/hyx.bit'
    
    try:
        overlay = Overlay(bitstream_path)
        print("✓ Overlay loaded!")
    except Exception as e:
        print(f"✗ Error loading overlay: {e}")
        print(f"  Please ensure {bitstream_path} exists")
        return False
    
    # --- 获取DMA ---
    try:
        dma = overlay.axi_dma_0
        print("✓ DMA initialized!")
    except AttributeError:
        print("✗ Error: DMA not found!")
        print("  Available components:", [x for x in dir(overlay) if not x.startswith('_')])
        return False
    
    # --- 生成测试图像 ---
    print("\n" + "-"*70)
    print("Generating test image...")
    image_data = create_test_image(IMAGE_WIDTH, IMAGE_HEIGHT)
    print(f"✓ Generated {IMAGE_SIZE} pixels")
    
    # --- CPU参考计算（只做一次）---
    print("\nComputing CPU reference...")
    start_time = time.time()
    cpu_histogram = compute_histogram_cpu(image_data)
    cpu_time = time.time() - start_time
    print(f"✓ CPU time: {cpu_time:.6f} seconds")
    
    # --- 数据打包 ---
    print("\nPacking data (uint8 -> uint32)...")
    packed_data, num_words = pack_uint8_to_uint32(image_data)
    transfer_size = num_words * 4
    print(f"✓ Original: {IMAGE_SIZE} pixels (uint8)")
    print(f"✓ Packed: {num_words} words (uint32)")
    print(f"✓ Transfer size: {transfer_size} bytes")
    
    # --- 分配DMA缓冲区 ---
    print("\nAllocating DMA buffers...")
    tx_buffer = allocate(shape=(num_words,), dtype=np.uint32)
    rx_buffer = allocate(shape=(HISTOGRAM_BINS,), dtype=np.uint32)
    print(f"✓ TX buffer: {tx_buffer.nbytes} bytes")
    print(f"✓ RX buffer: {rx_buffer.nbytes} bytes")
    
    # --- 准备输入数据 ---
    print("\nPreparing input data...")
    tx_buffer[:] = packed_data[:]
    print("✓ Data copied to buffers")
    
    # --- 迭代测试 ---
    print("\n" + "="*70)
    print(f"Starting FPGA acceleration ({iterations} iterations)...")
    print("="*70)
    
    hw_times = []
    success_count = 0
    error_count = 0
    
    # 进度显示间隔
    progress_interval = max(1, iterations // 10)  # 每10%显示一次
    
    for i in range(iterations):
        # 清零输出缓冲区
        rx_buffer[:] = 0
        
        # DMA传输
        start_time = time.time()
        
        try:
            dma.sendchannel.transfer(tx_buffer)
            dma.recvchannel.transfer(rx_buffer)
            
            dma.sendchannel.wait()
            dma.recvchannel.wait()
            
            hw_time = time.time() - start_time
            hw_times.append(hw_time)
            success_count += 1
            
        except Exception as e:
            error_count += 1
            print(f"\n✗ Error at iteration {i}: {e}")
            if error_count > 10:
                print("Too many errors, stopping...")
                break
            continue
        
        # 显示进度
        if (i + 1) % progress_interval == 0 or i == iterations - 1:
            progress = (i + 1) / iterations * 100
            avg_time = np.mean(hw_times) if hw_times else 0
            print(f"Progress: {i+1:4d}/{iterations} ({progress:5.1f}%) | "
                  f"Avg time: {avg_time*1000:6.3f} ms | "
                  f"Throughput: {IMAGE_SIZE/avg_time/1e6:5.2f} MP/s", end='\r')
    
    print()  # 换行
    
    if success_count == 0:
        print("\n✗ All iterations failed!")
        tx_buffer.freebuffer()
        rx_buffer.freebuffer()
        return False
    
    # --- 统计结果 ---
    hw_times = np.array(hw_times)
    avg_hw_time = np.mean(hw_times)
    min_hw_time = np.min(hw_times)
    max_hw_time = np.max(hw_times)
    std_hw_time = np.std(hw_times)
    
    print("\n" + "="*70)
    print("Performance Statistics:")
    print("="*70)
    print(f"Successful iterations: {success_count}/{iterations}")
    print(f"Failed iterations:     {error_count}")
    print(f"\nFPGA Time Statistics:")
    print(f"  Average:   {avg_hw_time:.6f} s ({avg_hw_time * 1000:.3f} ms)")
    print(f"  Minimum:   {min_hw_time:.6f} s ({min_hw_time * 1000:.3f} ms)")
    print(f"  Maximum:   {max_hw_time:.6f} s ({max_hw_time * 1000:.3f} ms)")
    print(f"  Std Dev:   {std_hw_time:.6f} s ({std_hw_time * 1000:.3f} ms)")
    print(f"\nThroughput:")
    print(f"  Average:   {IMAGE_SIZE / avg_hw_time / 1e6:.2f} MPixels/s")
    print(f"  Peak:      {IMAGE_SIZE / min_hw_time / 1e6:.2f} MPixels/s")
    
    # --- 验证最后一次结果 ---
    print("\n" + "="*70)
    print("Verifying Last Iteration Result...")
    print("="*70)
    
    errors = 0
    max_errors_show = 10
    
    for i in range(HISTOGRAM_BINS):
        hw_value = rx_buffer[i]
        cpu_value = cpu_histogram[i]
        
        if hw_value != cpu_value:
            if errors < max_errors_show:
                diff = int(hw_value) - int(cpu_value)
                print(f"  ✗ Bin {i:3d}: FPGA={hw_value:5d}, CPU={cpu_value:5d}, diff={diff:+d}")
            errors += 1
    
    # --- 显示部分结果 ---
    print("\n" + "-"*70)
    print("Sample Results (first 10 bins):")
    print("-"*70)
    print(f"{'Bin':>5} {'FPGA':>10} {'CPU':>10} {'Status':>10}")
    print("-"*70)
    
    for i in range(10):
        match = "✓ Match" if rx_buffer[i] == cpu_histogram[i] else "✗ Mismatch"
        print(f"{i:5d} {rx_buffer[i]:10d} {cpu_histogram[i]:10d} {match:>10}")
    
    # --- 显示统计信息 ---
    print("\n" + "-"*70)
    print("Correctness Check:")
    print("-"*70)
    total_cpu = np.sum(cpu_histogram)
    total_fpga = np.sum(rx_buffer)
    print(f"Total pixels (CPU):  {total_cpu}")
    print(f"Total pixels (FPGA): {total_fpga}")
    print(f"Expected:            {IMAGE_SIZE}")
    
    match = "✓" if total_cpu == total_fpga == IMAGE_SIZE else "✗"
    print(f"Total count match:   {match}")
    
    # --- 最终结果 ---
    print("\n" + "="*70)
    if errors == 0:
        print("              ✓✓✓ TEST PASSED! ✓✓✓")
        print("        Histogram computation is correct!")
    else:
        print(f"              ✗✗✗ TEST FAILED! ✗✗✗")
        print(f"     Found {errors} mismatches out of {HISTOGRAM_BINS} bins")
        print(f"              ({errors/HISTOGRAM_BINS*100:.1f}% error rate)")
    print("="*70)
    
    # --- 性能对比 ---
    print("\nPerformance Comparison:")
    print("-"*70)
    print(f"CPU time (1 iteration):  {cpu_time:.6f} s ({cpu_time * 1000:.3f} ms)")
    print(f"FPGA time (average):     {avg_hw_time:.6f} s ({avg_hw_time * 1000:.3f} ms)")
    
    if avg_hw_time > 0 and cpu_time > 0:
        speedup = cpu_time / avg_hw_time
        print(f"Speedup:                 {speedup:.2f}x")
        
        if speedup < 1:
            print("\nNote: FPGA slower than CPU due to:")
            print("  - DMA transfer overhead dominates for small images")
            print("  - CPU benefits from cache for repeated operations")
            print("  - Try larger images (e.g., 256x256 or 512x512) for better speedup")
        else:
            print("\n✓ FPGA is faster than CPU!")
    
    # --- 总运行时间 ---
    total_time = np.sum(hw_times)
    print(f"\nTotal FPGA time ({success_count} iterations): {total_time:.3f} seconds")
    print(f"Average time per iteration: {avg_hw_time * 1000:.3f} ms")
    
    # --- 清理 ---
    tx_buffer.freebuffer()
    rx_buffer.freebuffer()
    
    return errors == 0

if __name__ == "__main__":
    # ===== 配置参数 =====
    # 可以修改这个数字来改变迭代次数
    ITERATIONS = 10000 # 运行1000次
    
    
    success = main(iterations=ITERATIONS)
    
    if success:
        print("\n" + "="*70)
        print("           Program completed successfully!")
        print("="*70 + "\n")
        exit(0)
    else:
        print("\n" + "="*70)
        print("               Program failed!")
        print("="*70 + "\n")
        exit(1)
