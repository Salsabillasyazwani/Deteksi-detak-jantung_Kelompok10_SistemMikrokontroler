#ifndef SPO2_ALGORITHM_H
#define SPO2_ALGORITHM_H

#include <stdint.h>

#define BUFFER_SIZE 100

// Fungsi untuk menghitung SpO2 dan detak jantung
void maxim_heart_rate_and_oxygen_saturation(
    uint32_t *ir_buffer,
    int32_t buffer_length,
    uint32_t *red_buffer,
    int32_t *spo2,
    int8_t *spo2_valid,
    int32_t *heart_rate,
    int8_t *hr_valid);

#endif