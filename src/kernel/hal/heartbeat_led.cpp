#include "heartbeat_led.h"
#include "../../apps/irrig_common/irrig_types.h"

// ===== VARIABLES GLOBALES =====

static HeartbeatState_t current_state = HEARTBEAT_OFF;
static bool heartbeat_initialized = false;
static bool heartbeat_running = false;

// ===== PATTERNS DE CLIGNOTEMENT =====

typedef struct {
    uint16_t on_time_ms;     // Durée LED ON (ms)
    uint16_t off_time_ms;    // Durée LED OFF (ms)
    uint8_t pulse_count;     // Nombre de pulses par cycle (0 = continu)
    uint16_t cycle_delay_ms; // Délai entre cycles (si pulse_count > 0)
} HeartbeatPattern_t;

// Patterns pour chaque état
static const HeartbeatPattern_t patterns[] = {
    [HEARTBEAT_OFF]        = {0,    0,    0, 0},      // OFF permanent
    [HEARTBEAT_BOOTING]    = {25,   25,   0, 0},      // Très rapide continu (boot)
    [HEARTBEAT_READY]      = {1000, 1000, 0, 0},      // Lent continu
    [HEARTBEAT_RUNNING]    = {200,  200,  2, 1000},   // Double pulse
    [HEARTBEAT_WIFI_ERROR] = {100,  100,  0, 0},      // Rapide continu (erreur WiFi)
    [HEARTBEAT_ERROR]      = {0,    0,    0, 0}       // ON permanent
};

// ===== FONCTIONS PRIVÉES =====

static void set_led(bool state) {
    digitalWrite(HEARTBEAT_LED_PIN, state ? HIGH : LOW);
}

// ===== FONCTIONS PUBLIQUES =====

SysError_t heartbeat_init(void) {
    if (heartbeat_initialized) {
        kernel_log(LOG_LEVEL_WARN, "Heartbeat already initialized");
        return SYS_OK;
    }

    // Configurer GPIO en sortie
    pinMode(HEARTBEAT_LED_PIN, OUTPUT);
    set_led(false);

    current_state = HEARTBEAT_OFF;
    heartbeat_initialized = true;

    kernel_log(LOG_LEVEL_INFO, "Heartbeat LED initialized on GPIO %d", HEARTBEAT_LED_PIN);
    return SYS_OK;
}

void heartbeat_set_state(HeartbeatState_t state) {
    if (!heartbeat_initialized) {
        kernel_log(LOG_LEVEL_ERROR, "Heartbeat not initialized");
        return;
    }

    if (state == current_state) {
        return; // Pas de changement
    }

    HeartbeatState_t old_state = current_state;
    current_state = state;

    // Log changement d'état
    const char* state_names[] = {
        "OFF", "BOOTING", "READY", "RUNNING", "WIFI_ERROR", "ERROR"
    };

    kernel_log(LOG_LEVEL_INFO, "Heartbeat: %s → %s",
               state_names[old_state], state_names[state]);

    // Gestion états spéciaux
    if (state == HEARTBEAT_OFF) {
        set_led(false);
    } else if (state == HEARTBEAT_ERROR) {
        set_led(true);
    }
}

HeartbeatState_t heartbeat_get_state(void) {
    return current_state;
}

void heartbeat_stop(void) {
    heartbeat_running = false;
    current_state = HEARTBEAT_OFF;
    set_led(false);
    kernel_log(LOG_LEVEL_INFO, "Heartbeat stopped");
}

void heartbeat_task(void* params) {
    (void)params;

    if (!heartbeat_initialized) {
        kernel_log(LOG_LEVEL_ERROR, "Heartbeat task started but not initialized!");
        vTaskDelete(NULL);
        return;
    }

    heartbeat_running = true;
    kernel_log(LOG_LEVEL_INFO, "Heartbeat task started");

    uint8_t pulse_counter = 0;
    bool led_state = false;

    while (heartbeat_running) {
        HeartbeatState_t state = current_state;

        // États spéciaux (pas de clignotement)
        if (state == HEARTBEAT_OFF || state == HEARTBEAT_ERROR) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        const HeartbeatPattern_t* pattern = &patterns[state];

        // Pattern avec pulses
        if (pattern->pulse_count > 0) {
            // Générer les pulses
            for (uint8_t i = 0; i < pattern->pulse_count; i++) {
                if (current_state != state) break; // État changé

                set_led(true);
                vTaskDelay(pdMS_TO_TICKS(pattern->on_time_ms));

                set_led(false);
                vTaskDelay(pdMS_TO_TICKS(pattern->off_time_ms));
            }

            // Délai entre cycles
            if (current_state == state && pattern->cycle_delay_ms > 0) {
                vTaskDelay(pdMS_TO_TICKS(pattern->cycle_delay_ms));
            }
        }
        // Pattern continu
        else {
            led_state = !led_state;
            set_led(led_state);

            uint16_t delay_ms = led_state ? pattern->on_time_ms : pattern->off_time_ms;
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
    }

    kernel_log(LOG_LEVEL_INFO, "Heartbeat task ended");
    vTaskDelete(NULL);
}
