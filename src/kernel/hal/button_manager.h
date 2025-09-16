#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include "../core/kernel.h"

// Configuration des boutons
#define BUTTON_DEBOUNCE_TIME_MS 50
#define BUTTON_LONG_PRESS_TIME_MS 2000
#define BUTTON_MAX_BUTTONS 4

// Types de boutons
typedef enum {
    BUTTON_TYPE_INCREMENT = 0,    // Bouton d'incrémentation
    BUTTON_TYPE_DECREMENT = 1,    // Bouton de décrémentation
    BUTTON_TYPE_CUSTOM_1 = 2,     // Bouton personnalisé 1
    BUTTON_TYPE_CUSTOM_2 = 3      // Bouton personnalisé 2
} ButtonType_t;

// États des boutons
typedef enum {
    BUTTON_STATE_RELEASED = 0,
    BUTTON_STATE_PRESSED = 1,
    BUTTON_STATE_LONG_PRESSED = 2
} ButtonState_t;

// Événements de boutons
typedef enum {
    BUTTON_EVENT_PRESSED = 0,
    BUTTON_EVENT_RELEASED = 1,
    BUTTON_EVENT_LONG_PRESSED = 2,
    BUTTON_EVENT_CLICK = 3
} ButtonEvent_t;

// Structure d'un bouton
typedef struct {
    uint8_t pin;
    ButtonType_t type;
    ButtonState_t state;
    ButtonState_t last_state;
    unsigned long last_press_time;
    unsigned long press_start_time;
    bool long_press_detected;
    bool enabled;
} Button_t;

// Callback pour les événements de boutons
typedef void (*ButtonCallback_t)(ButtonType_t type, ButtonEvent_t event);

// Fonctions principales
SysError_t button_manager_init(void);
void button_manager_loop(void);
void button_manager_deinit(void);

// Gestion des boutons
SysError_t button_register(uint8_t pin, ButtonType_t type, ButtonCallback_t callback);
SysError_t button_unregister(ButtonType_t type);
SysError_t button_set_enabled(ButtonType_t type, bool enabled);

// Fonctions utilitaires
ButtonState_t button_get_state(ButtonType_t type);
bool button_is_pressed(ButtonType_t type);
bool button_is_long_pressed(ButtonType_t type);

// Variables globales
extern Button_t buttons[BUTTON_MAX_BUTTONS];
extern ButtonCallback_t button_callbacks[BUTTON_MAX_BUTTONS];

#endif // BUTTON_MANAGER_H
