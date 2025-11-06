void myip_v1_0_HLS(
    hls::stream<ap_axiu<32, 0, 0, 0> >& image_stream,
    hls::stream<ap_axiu<32, 0, 0, 0> >& histogram_stream,
    int image_size
) {
    #pragma HLS INTERFACE axis port=image_stream
    #pragma HLS INTERFACE axis port=histogram_stream
    #pragma HLS INTERFACE s_axilite port=image_size bundle=control
    #pragma HLS INTERFACE s_axilite port=return bundle=control
    
    static unsigned int histogram_local[HISTOGRAM_BINS];
    #pragma HLS ARRAY_PARTITION variable=histogram_local complete
    
    // 4个独立的累加器
    static unsigned int hist_acc0[HISTOGRAM_BINS];
    static unsigned int hist_acc1[HISTOGRAM_BINS];
    static unsigned int hist_acc2[HISTOGRAM_BINS];
    static unsigned int hist_acc3[HISTOGRAM_BINS];
    #pragma HLS ARRAY_PARTITION variable=hist_acc0 complete
    #pragma HLS ARRAY_PARTITION variable=hist_acc1 complete
    #pragma HLS ARRAY_PARTITION variable=hist_acc2 complete
    #pragma HLS ARRAY_PARTITION variable=hist_acc3 complete
    
    // 初始化
    INIT_LOOP: for (int i = 0; i < HISTOGRAM_BINS; i++) {
        #pragma HLS UNROLL
        hist_acc0[i] = 0;
        hist_acc1[i] = 0;
        hist_acc2[i] = 0;
        hist_acc3[i] = 0;
    }
    
    int pixels_per_transfer = 4;
    int transfers = (image_size + pixels_per_transfer - 1) / pixels_per_transfer;
    
    // 使用独立累加器避免冲突
    PROCESS_LOOP: for (int i = 0; i < transfers; i++) {
        #pragma HLS PIPELINE II=1
        
        ap_axiu<32, 0, 0, 0> data = image_stream.read();
        ap_uint<32> pixel_data = data.data;
        
        ap_uint<8> pixel0 = pixel_data.range(7, 0);
        ap_uint<8> pixel1 = pixel_data.range(15, 8);
        ap_uint<8> pixel2 = pixel_data.range(23, 16);
        ap_uint<8> pixel3 = pixel_data.range(31, 24);
        
        int base_idx = i * pixels_per_transfer;
        
        if (base_idx < image_size) hist_acc0[pixel0]++;
        if (base_idx + 1 < image_size) hist_acc1[pixel1]++;
        if (base_idx + 2 < image_size) hist_acc2[pixel2]++;
        if (base_idx + 3 < image_size) hist_acc3[pixel3]++;
    }
    
    // 合并累加器
    MERGE_LOOP: for (int i = 0; i < HISTOGRAM_BINS; i++) {
        #pragma HLS UNROLL
        histogram_local[i] = hist_acc0[i] + hist_acc1[i] + hist_acc2[i] + hist_acc3[i];
    }
    
    // 输出
    OUTPUT_LOOP: for (int i = 0; i < HISTOGRAM_BINS; i++) {
        #pragma HLS PIPELINE II=1
        ap_axiu<32, 0, 0, 0> output_data;
        output_data.data = histogram_local[i];
        output_data.last = (i == HISTOGRAM_BINS - 1);
        histogram_stream.write(output_data);
    }
}