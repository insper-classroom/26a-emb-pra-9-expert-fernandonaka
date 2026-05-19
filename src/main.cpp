#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"

#include "FreeRTOS.h"
#include "task.h"

#include "mpu6050.h"

#include "edge-impulse-sdk/classifier/ei_classifier_types.h"
#include "model-parameters/model_metadata.h"

using namespace ei;

extern "C" EI_IMPULSE_ERROR run_classifier(signal_t *signal, ei_impulse_result_t *result, bool debug);

#define MPU_ADDRESS  0x68
#define I2C_SDA_GPIO 4
#define I2C_SCL_GPIO 5

#define LED_R_GPIO 16  // idle
#define LED_G_GPIO 15  // wave
#define LED_B_GPIO 14  // updown

static float inference_buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];

static void led_init() {
    gpio_init(LED_R_GPIO); gpio_set_dir(LED_R_GPIO, GPIO_OUT);
    gpio_init(LED_G_GPIO); gpio_set_dir(LED_G_GPIO, GPIO_OUT);
    gpio_init(LED_B_GPIO); gpio_set_dir(LED_B_GPIO, GPIO_OUT);
}

static void led_set(bool r, bool g, bool b) {
    gpio_put(LED_R_GPIO, r);
    gpio_put(LED_G_GPIO, g);
    gpio_put(LED_B_GPIO, b);
}

static void mpu6050_init() {
    i2c_init(i2c_default, 400 * 1000);
    gpio_set_function(I2C_SDA_GPIO, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_GPIO, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_GPIO);
    gpio_pull_up(I2C_SCL_GPIO);
    uint8_t buf[] = { 0x6B, 0x00 };
    i2c_write_blocking(i2c_default, MPU_ADDRESS, buf, 2, false);
}

static void mpu6050_read_raw(int16_t accel[3], int16_t gyro[3], int16_t *temp) {
    uint8_t buffer[14];
    uint8_t val = 0x3B;
    i2c_write_blocking(i2c_default, MPU_ADDRESS, &val, 1, true);
    i2c_read_blocking(i2c_default, MPU_ADDRESS, buffer, 14, false);
    for (int i = 0; i < 3; i++)
        accel[i] = (int16_t)(buffer[i * 2] << 8 | buffer[(i * 2) + 1]);
    *temp = (int16_t)(buffer[6] << 8 | buffer[7]);
    for (int i = 0; i < 3; i++)
        gyro[i] = (int16_t)(buffer[8 + i * 2] << 8 | buffer[8 + (i * 2) + 1]);
}

static void gesture_recognize_task(void *p) {
    mpu6050_init();
    led_init();
    int16_t accelerometer[3], gyro[3], temp;

    while (true) {
        for (size_t ix = 0; ix < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; ix += 3) {
            mpu6050_read_raw(accelerometer, gyro, &temp);
            inference_buffer[ix + 0] = (float)accelerometer[0];
            inference_buffer[ix + 1] = (float)accelerometer[1];
            inference_buffer[ix + 2] = (float)accelerometer[2];
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        signal_t signal;
        signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
        signal.get_data = [](size_t offset, size_t length, float *out_ptr) -> int {
            memcpy(out_ptr, inference_buffer + offset, length * sizeof(float));
            return 0;
        };

        ei_impulse_result_t result = { 0 };
        EI_IMPULSE_ERROR err = run_classifier(&signal, &result, false);
        if (err != EI_IMPULSE_OK) {
            printf("ERR: Failed to run classifier (%d)\n", err);
            break;
        }

        printf("Predictions (DSP: %d ms., Classification: %d ms.):\n",
            result.timing.dsp, result.timing.classification);

        size_t best_ix = 0;
        for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
            printf("    %s: %.5f\n", result.classification[ix].label, result.classification[ix].value);
            if (result.classification[ix].value > result.classification[best_ix].value)
                best_ix = ix;
        }

        const char *label = result.classification[best_ix].label;
        if (strcmp(label, "idle") == 0)
            led_set(true, false, false);
        else if (strcmp(label, "wave") == 0)
            led_set(false, true, false);
        else if (strcmp(label, "updown") == 0)
            led_set(false, false, true);
        else
            led_set(false, false, false);
    }
}

int main() {
    stdio_init_all();
    xTaskCreate(gesture_recognize_task, "gesture_task", 8192, NULL, 1, NULL);
    vTaskStartScheduler();
    while (true);
}
