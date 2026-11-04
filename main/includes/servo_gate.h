#ifndef SERVO_GATE_H
#define SERVO_GATE_H

#include "esp_err.h"

/*
 * PLATEGUARD SG90 SERVO GATE
 *
 * Initializes the servo and starts a background task.
 *
 * The gate starts CLOSED at 0 degrees.
 *
 * Each accepted detection request causes the gate to:
 *   1. Open to 90 degrees.
 *   2. Remain open for 5 seconds.
 *   3. Close to 0 degrees.
 *   4. Wait through a 2-second cooldown.
 *
 * The gate operates regardless of plate authorization.
 */

// Initialize the SG90 and its control task.
esp_err_t servo_gate_init(void);

// Request one gate-opening cycle.
// Returns immediately without waiting for the servo.
// Returns ESP_ERR_TIMEOUT if the request queue is full.
esp_err_t servo_gate_trigger(void);

#endif