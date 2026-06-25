#include "spo2_algorithm.h"
#include <math.h>

#define FS 25

// Fungsi untuk menghitung SpO2 dan detak jantung
void maxim_heart_rate_and_oxygen_saturation(
    uint32_t *ir_buffer,
    int32_t buffer_length,
    uint32_t *red_buffer,
    int32_t *spo2,
    int8_t *spo2_valid,
    int32_t *heart_rate,
    int8_t *hr_valid)
{
  float ir_mean = 0, red_mean = 0;

  for (int i = 0; i < buffer_length; i++)
  {
    ir_mean += ir_buffer[i];
    red_mean += red_buffer[i];
  }

  ir_mean /= buffer_length;
  red_mean /= buffer_length;

  float ir_ac = 0, red_ac = 0;

  for (int i = 0; i < buffer_length; i++)
  {
    ir_ac += fabs(ir_buffer[i] - ir_mean);
    red_ac += fabs(red_buffer[i] - red_mean);
  }

  ir_ac /= buffer_length;
  red_ac /= buffer_length;

  if (ir_ac == 0 || red_mean == 0)
  {
    *spo2 = 0;
    *spo2_valid = 0;
    *heart_rate = 0;
    *hr_valid = 0;
    return;
  }

  float ratio = (red_ac / red_mean) / (ir_ac / ir_mean);

  float spo2_calc = 104 - 17 * ratio;

  if (spo2_calc > 100)
    spo2_calc = 100;
  if (spo2_calc < 0)
    spo2_calc = 0;

  *spo2 = (int32_t)spo2_calc;
  *spo2_valid = 1;

  // ===== HEART RATE =====
  int peak_count = 0;
  int last_peak = -1;
  int interval_sum = 0;

  for (int i = 1; i < buffer_length - 1; i++)
  {
    if (ir_buffer[i] > ir_buffer[i - 1] &&
        ir_buffer[i] > ir_buffer[i + 1] &&
        ir_buffer[i] > ir_mean)
    {
      if (last_peak != -1)
      {
        interval_sum += (i - last_peak);
        peak_count++;
      }
      last_peak = i;
    }
  }

  if (peak_count > 0)
  {
    float avg_interval = (float)interval_sum / peak_count;
    *heart_rate = (int32_t)(60.0 * FS / avg_interval);
    *hr_valid = 1;
  }
  else
  {
    *heart_rate = 0;
    *hr_valid = 0;
  }
}