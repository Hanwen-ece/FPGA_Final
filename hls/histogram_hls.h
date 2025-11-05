#ifndef HISTOGRAM_HLS_H
#define HISTOGRAM_HLS_H

#include "ap_int.h"
#include "ap_axi_sdata.h"
#include "hls_stream.h"

#define HISTOGRAM_BINS 256

// 使用 AXI Stream 接口的版本
void compute_histogram_axi_stream(
    hls::stream<ap_axiu<32, 0, 0, 0> >& image_stream,
    hls::stream<ap_axiu<32, 0, 0, 0> >& histogram_stream,
    int image_size
);

// 使用 AXI Master 接口的版本（直接内存访问）
void compute_histogram_axi_master(
    unsigned char* image_data,
    unsigned int* histogram_data,
    int image_size
);

#endif // HISTOGRAM_HLS_H
