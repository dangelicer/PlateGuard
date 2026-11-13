/*
============================================================
                      PLATEGUARD
                  SG90 SERVO CONTROL
============================================================

PURPOSE:

Controls an SG90 servo motor using the ESP32's
built-in LEDC PWM peripheral.

The servo represents a parking gate.

The ESP32 receives vehicle detections through Daniel's
existing HTTP server and detection pipeline.

Every detection requests a gate-opening cycle,
regardless of authorization status.

OPERATION:

1. Gate begins at 0 degrees (CLOSED).
2. Detection pipeline calls servo_gate_trigger().
3. A request is added to the servo queue.
4. Servo rotates to 90 degrees (OPEN).
5. Gate remains open for 5 seconds.
6. Servo returns to 0 degrees (CLOSED).
7. System waits through a 2-second cooldown.
8. Next queued request is processed.

HARDWARE:

SG90 Brown Wire  -> GND
SG90 Red Wire    -> External regulated 5V
SG90 Orange Wire -> ESP32 GPIO 18

IMPORTANT:

The ESP32 ground and external servo supply ground
MUST be connected together.

This code assumes a standard ESP32 with GPIO 18
available.

============================================================
*/

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"

#include "includes/servo_gate.h"


// ==========================================================
// SERVO CONFIGURATION
// ==========================================================

// ESP32 GPIO connected to the orange servo signal wire.
#define SERVO_GPIO 18

// SG90 servos typically use a 50 Hz PWM signal.
#define SERVO_FREQUENCY 50

// PWM resolution: 14 bits.
#define SERVO_DUTY_RESOLUTION LEDC_TIMER_14_BIT

// Number of duty-count levels.
#define SERVO_DUTY_STEPS 16384

// LEDC timer and channel used by the servo.
#define SERVO_TIMER LEDC_TIMER_0
#define SERVO_CHANNEL LEDC_CHANNEL_0

// Standard ESP32 LEDC speed mode.
#define SERVO_SPEED_MODE LEDC_LOW_SPEED_MODE


// ==========================================================
// SERVO ANGLE CONFIGURATION
// ==========================================================

// Closed gate position.
#define GATE_CLOSED_ANGLE 0

// Open gate position.
#define GATE_OPEN_ANGLE 90

/*
 * Servo pulse-width calibration.
 *
 * These are conservative starting values.
 *
 * 1000 microseconds = approximately 0 degrees
 * 2000 microseconds = approximately 180 degrees
 *
 * Therefore:
 *
 * 0 degrees  -> 1000 microseconds
 * 90 degrees -> 1500 microseconds
 *
 * Actual SG90 servo positions vary.
 *
 * Adjust these values after testing the physical gate.
 *
 * Do not force the servo beyond its mechanical limits.
 */

#define SERVO_MIN_PULSE_US 1000
#define SERVO_MAX_PULSE_US 2000


// ==========================================================
// TIMING CONFIGURATION
// ==========================================================

// Gate stays open for 5 seconds.
#define GATE_OPEN_TIME_MS 5000

// Cooldown after closing.
#define GATE_COOLDOWN_MS 2000

// Small delay to allow servo movement.
#define SERVO_MOVE_TIME_MS 500

// Maximum number of pending gate requests.
#define GATE_QUEUE_LENGTH 8


// ==========================================================
// SYSTEM VARIABLES
// ==========================================================

// Logging identifier for the serial monitor.
static const char *TAG = "SERVO_GATE";

// FreeRTOS queue storing gate-opening requests.
static QueueHandle_t gate_queue = NULL;


// ==========================================================
// FUNCTION: SET SERVO ANGLE
// ==========================================================

/*
 * Converts an angle into a PWM duty cycle.
 *
 * angle = 0   -> closed
 * angle = 90  -> open
 *
 * The LEDC peripheral continuously generates PWM.
 */

static esp_err_t servo_set_angle(int angle)
{
    // Prevent invalid angles.
    if (angle < 0) {
        angle = 0;
    }

    if (angle > 180) {
        angle = 180;
    }

    // Convert angle into pulse width.
    uint32_t pulse_us =
        SERVO_MIN_PULSE_US +
        ((SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US)
         * angle / 180);

    // Convert pulse width into PWM duty count.
    //
    // At 50 Hz, one PWM period is 20,000 microseconds.

    uint32_t duty =
        (uint32_t)(((uint64_t)pulse_us *
                    SERVO_DUTY_STEPS *
                    SERVO_FREQUENCY) / 1000000ULL);

    // Set PWM duty cycle.
    esp_err_t err = ledc_set_duty(
        SERVO_SPEED_MODE,
        SERVO_CHANNEL,
        duty
    );

    if (err != ESP_OK) {
        return err;
    }

    // Apply the new duty cycle.
    return ledc_update_duty(
        SERVO_SPEED_MODE,
        SERVO_CHANNEL
    );
}


// ==========================================================
// FUNCTION: OPEN GATE
// ==========================================================

static void servo_gate_open(void)
{
    ESP_LOGI(TAG, "Opening gate to 90 degrees");

    ESP_ERROR_CHECK(
        servo_set_angle(GATE_OPEN_ANGLE)
    );
}


// ==========================================================
// FUNCTION: CLOSE GATE
// ==========================================================

static void servo_gate_close(void)
{
    ESP_LOGI(TAG, "Closing gate to 0 degrees");

    ESP_ERROR_CHECK(
        servo_set_angle(GATE_CLOSED_ANGLE)
    );
}


// ==========================================================
// FUNCTION: SERVO CONTROL TASK
// ==========================================================

/*
 * This FreeRTOS task runs independently from
 * Daniel's detection pipeline.
 *
 * It waits for gate requests.
 *
 * When a request arrives:
 *
 * 1. Open the gate.
 * 2. Wait 5 seconds.
 * 3. Close the gate.
 * 4. Wait 2 seconds.
 *
 * Additional detections are stored in the queue.
 *
 * This prevents the detection task from being blocked
 * while the servo is operating.
 */

static void servo_gate_task(void *arg)
{
    uint8_t request;

    while (1) {

        // Wait until a detection requests gate movement.
        if (xQueueReceive(
                gate_queue,
                &request,
                portMAX_DELAY) == pdTRUE)
        {
            ESP_LOGI(TAG, "New gate request received");

            // Open gate.
            servo_gate_open();

            // Wait for gate to physically move.
            vTaskDelay(
                pdMS_TO_TICKS(SERVO_MOVE_TIME_MS)
            );

            // Keep gate open for 5 seconds.
            vTaskDelay(
                pdMS_TO_TICKS(GATE_OPEN_TIME_MS)
            );

            // Close gate.
            servo_gate_close();

            // Allow servo to finish closing.
            vTaskDelay(
                pdMS_TO_TICKS(SERVO_MOVE_TIME_MS)
            );

            // Cooldown before next detection.
            vTaskDelay(
                pdMS_TO_TICKS(GATE_COOLDOWN_MS)
            );

            ESP_LOGI(TAG, "Gate cycle complete");
        }
    }
}


// ==========================================================
// FUNCTION: INITIALIZE SERVO
// ==========================================================

esp_err_t servo_gate_init(void)
{
    ESP_LOGI(TAG, "Initializing SG90 servo");

    // Configure the LEDC PWM timer.
    ledc_timer_config_t timer_config = {
        .speed_mode = SERVO_SPEED_MODE,
        .duty_resolution = SERVO_DUTY_RESOLUTION,
        .timer_num = SERVO_TIMER,
        .freq_hz = SERVO_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK
    };

    esp_err_t err = ledc_timer_config(&timer_config);

    if (err != ESP_OK) {
        return err;
    }

    // Configure the PWM output channel.
    ledc_channel_config_t channel_config = {
        .gpio_num = SERVO_GPIO,
        .speed_mode = SERVO_SPEED_MODE,
        .channel = SERVO_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = SERVO_TIMER,
        .duty = 0,
        .hpoint = 0
    };

    err = ledc_channel_config(&channel_config);

    if (err != ESP_OK) {
        return err;
    }

    // Set the initial position to CLOSED.
    err = servo_set_angle(GATE_CLOSED_ANGLE);

    if (err != ESP_OK) {
        return err;
    }

    // Create queue for incoming detection requests.
    gate_queue = xQueueCreate(
        GATE_QUEUE_LENGTH,
        sizeof(uint8_t)
    );

    if (gate_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    // Start independent servo control task.
    if (xTaskCreate(
            servo_gate_task,
            "servo_gate",
            4096,
            NULL,
            5,
            NULL) != pdPASS)
    {
        vQueueDelete(gate_queue);
        gate_queue = NULL;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Servo initialized successfully");
    ESP_LOGI(TAG, "Gate starting CLOSED");

    return ESP_OK;
}


// ==========================================================
// FUNCTION: TRIGGER GATE
// ==========================================================

/*
 * Called by Daniel's detection pipeline.
 *
 * Every accepted request adds one gate-opening cycle.
 *
 * This function does not wait for the servo.
 *
 * If the queue is full, the request is rejected.
 *
 * Authorization status is not checked here.
 */

esp_err_t servo_gate_trigger(void)
{
    if (gate_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t request = 1;

    // Add request without blocking.
    if (xQueueSend(
            gate_queue,
            &request,
            0) != pdTRUE)
    {
        ESP_LOGW(TAG, "Gate queue full");

        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGI(TAG, "Gate request queued");

    return ESP_OK;
}