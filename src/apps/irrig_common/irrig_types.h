#ifndef IRRIG_TYPES_H
#define IRRIG_TYPES_H

#include <Arduino.h>

// ===== CONSTANTES =====

#define MAX_ZONES 4
#define MAX_SENSORS 12
#define SENSORS_PER_ZONE 3  // Pour simulation (12 capteurs / 4 zones = 3)

// Pins capteurs ADC
#define MOISTURE_PIN_1  32
#define MOISTURE_PIN_2  33
#define MOISTURE_PIN_3  34
#define MOISTURE_PIN_4  35
#define MOISTURE_PIN_5  36
#define MOISTURE_PIN_6  39
#define MOISTURE_PIN_7  25
#define MOISTURE_PIN_8  26
#define MOISTURE_PIN_9  27
#define MOISTURE_PIN_10 14
#define MOISTURE_PIN_11 12
#define MOISTURE_PIN_12 13

// Pins relais
#define ZONE_1_RELAY_PIN 15  // Déplacé de GPIO 2 → 15 (GPIO 2 réservé poconur heartbeat)
#define ZONE_2_RELAY_PIN 4
#define ZONE_3_RELAY_PIN 18
#define ZONE_4_RELAY_PIN 19
#define PUMP_RELAY_PIN   5

// Pins indicateurs
#define STATUS_LED_PIN   18
#define BUZZER_PIN       19
#define HEARTBEAT_LED_PIN 2  // LED intégrée du dev board (heartbeat système)

// Seuils
#define MOISTURE_DRY_VALUE 4095
#define MOISTURE_WET_VALUE 0
#define MOISTURE_SAMPLES 5

// ===== TYPES COMMUNS =====

// Commandes irrigation
typedef enum {
    CMD_START_IRRIGATION = 0,
    CMD_STOP_IRRIGATION,
    CMD_EMERGENCY_STOP,
    CMD_TEST_RELAY,
    CMD_STATUS_REQUEST
} IrrigationCommand_t;

#endif // IRRIG_TYPES_H
