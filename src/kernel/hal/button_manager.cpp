#include "button_manager.h"
#include "../core/minimal_config.h"
#include <Arduino.h>

// Variables globales
Button_t buttons[BUTTON_MAX_BUTTONS];
ButtonCallback_t button_callbacks[BUTTON_MAX_BUTTONS];
bool button_manager_initialized = false;

// Initialisation du gestionnaire de boutons
SysError_t button_manager_init(void) {
    if (button_manager_initialized) {
        return SYS_ALREADY_INITIALIZED;
    }
    
    SERIAL_PRINTLN_MINIMAL("Button Manager: Initializing...");
    
    // Initialiser les structures de boutons
    for (int i = 0; i < BUTTON_MAX_BUTTONS; i++) {
        buttons[i].pin = 0;
        buttons[i].type = (ButtonType_t)i;
        buttons[i].state = BUTTON_STATE_RELEASED;
        buttons[i].last_state = BUTTON_STATE_RELEASED;
        buttons[i].last_press_time = 0;
        buttons[i].press_start_time = 0;
        buttons[i].long_press_detected = false;
        buttons[i].enabled = false;
        button_callbacks[i] = NULL;
    }
    
    button_manager_initialized = true;
    SERIAL_PRINTLN_MINIMAL("Button Manager: Initialized successfully");
    kernel_log(LOG_LEVEL_INFO, "Button Manager initialized");
    
    return SYS_OK;
}

// Boucle principale du gestionnaire de boutons
void button_manager_loop(void) {
    if (!button_manager_initialized) {
        return;
    }
    
    unsigned long current_time = millis();
    
    for (int i = 0; i < BUTTON_MAX_BUTTONS; i++) {
        if (!buttons[i].enabled || buttons[i].pin == 0) {
            continue;
        }
        
        // Lire l'état du bouton (inversé car pull-up interne)
        bool pin_state = !digitalRead(buttons[i].pin);
        ButtonState_t new_state = pin_state ? BUTTON_STATE_PRESSED : BUTTON_STATE_RELEASED;
        
        // Détection de changement d'état avec debounce
        if (new_state != buttons[i].last_state) {
            if (current_time - buttons[i].last_press_time >= BUTTON_DEBOUNCE_TIME_MS) {
                buttons[i].last_state = new_state;
                buttons[i].last_press_time = current_time;
                
                if (new_state == BUTTON_STATE_PRESSED) {
                    // Bouton pressé
                    buttons[i].state = BUTTON_STATE_PRESSED;
                    buttons[i].press_start_time = current_time;
                    buttons[i].long_press_detected = false;
                    
                    // Appeler le callback
                    if (button_callbacks[i] != NULL) {
                        button_callbacks[i](buttons[i].type, BUTTON_EVENT_PRESSED);
                    }
                    
                    SERIAL_PRINTF_MINIMAL("Button %d: Pressed\n", buttons[i].type);
                } else {
                    // Bouton relâché
                    buttons[i].state = BUTTON_STATE_RELEASED;
                    
                    // Vérifier si c'était un clic court ou long
                    if (!buttons[i].long_press_detected) {
                        // Clic court
                        if (button_callbacks[i] != NULL) {
                            button_callbacks[i](buttons[i].type, BUTTON_EVENT_CLICK);
                        }
                        SERIAL_PRINTF_MINIMAL("Button %d: Click\n", buttons[i].type);
                    }
                    
                    // Appeler le callback de relâchement
                    if (button_callbacks[i] != NULL) {
                        button_callbacks[i](buttons[i].type, BUTTON_EVENT_RELEASED);
                    }
                    
                    SERIAL_PRINTF_MINIMAL("Button %d: Released\n", buttons[i].type);
                }
            }
        }
        
        // Détection de pression longue
        if (buttons[i].state == BUTTON_STATE_PRESSED && !buttons[i].long_press_detected) {
            if (current_time - buttons[i].press_start_time >= BUTTON_LONG_PRESS_TIME_MS) {
                buttons[i].long_press_detected = true;
                buttons[i].state = BUTTON_STATE_LONG_PRESSED;
                
                // Appeler le callback de pression longue
                if (button_callbacks[i] != NULL) {
                    button_callbacks[i](buttons[i].type, BUTTON_EVENT_LONG_PRESSED);
                }
                
                SERIAL_PRINTF_MINIMAL("Button %d: Long Press\n", buttons[i].type);
            }
        }
    }
}

// Désinitialisation du gestionnaire de boutons
void button_manager_deinit(void) {
    if (!button_manager_initialized) {
        return;
    }
    
    // Désactiver tous les boutons
    for (int i = 0; i < BUTTON_MAX_BUTTONS; i++) {
        if (buttons[i].pin != 0) {
            pinMode(buttons[i].pin, INPUT);
            buttons[i].enabled = false;
        }
    }
    
    button_manager_initialized = false;
    SERIAL_PRINTLN_MINIMAL("Button Manager: Deinitialized");
    kernel_log(LOG_LEVEL_INFO, "Button Manager deinitialized");
}

// Enregistrer un bouton
SysError_t button_register(uint8_t pin, ButtonType_t type, ButtonCallback_t callback) {
    if (!button_manager_initialized) {
        return SYS_NOT_INITIALIZED;
    }
    
    if (type >= BUTTON_MAX_BUTTONS) {
        return SYS_INVALID_PARAM;
    }
    
    // Configurer le pin en entrée avec pull-up interne
    pinMode(pin, INPUT_PULLUP);
    
    // Enregistrer le bouton
    buttons[type].pin = pin;
    buttons[type].type = type;
    buttons[type].state = BUTTON_STATE_RELEASED;
    buttons[type].last_state = BUTTON_STATE_RELEASED;
    buttons[type].last_press_time = 0;
    buttons[type].press_start_time = 0;
    buttons[type].long_press_detected = false;
    buttons[type].enabled = true;
    button_callbacks[type] = callback;
    
    SERIAL_PRINTF_MINIMAL("Button %d: Registered on pin %d\n", type, pin);
    kernel_log(LOG_LEVEL_INFO, "Button %d registered on pin %d", type, pin);
    
    return SYS_OK;
}

// Désenregistrer un bouton
SysError_t button_unregister(ButtonType_t type) {
    if (!button_manager_initialized) {
        return SYS_NOT_INITIALIZED;
    }
    
    if (type >= BUTTON_MAX_BUTTONS) {
        return SYS_INVALID_PARAM;
    }
    
    if (buttons[type].pin != 0) {
        pinMode(buttons[type].pin, INPUT);
        buttons[type].pin = 0;
        buttons[type].enabled = false;
        button_callbacks[type] = NULL;
        
        SERIAL_PRINTF_MINIMAL("Button %d: Unregistered\n", type);
        kernel_log(LOG_LEVEL_INFO, "Button %d unregistered", type);
    }
    
    return SYS_OK;
}

// Activer/désactiver un bouton
SysError_t button_set_enabled(ButtonType_t type, bool enabled) {
    if (!button_manager_initialized) {
        return SYS_NOT_INITIALIZED;
    }
    
    if (type >= BUTTON_MAX_BUTTONS) {
        return SYS_INVALID_PARAM;
    }
    
    buttons[type].enabled = enabled;
    
    SERIAL_PRINTF_MINIMAL("Button %d: %s\n", type, enabled ? "Enabled" : "Disabled");
    kernel_log(LOG_LEVEL_INFO, "Button %d %s", type, enabled ? "enabled" : "disabled");
    
    return SYS_OK;
}

// Obtenir l'état d'un bouton
ButtonState_t button_get_state(ButtonType_t type) {
    if (!button_manager_initialized || type >= BUTTON_MAX_BUTTONS) {
        return BUTTON_STATE_RELEASED;
    }
    
    return buttons[type].state;
}

// Vérifier si un bouton est pressé
bool button_is_pressed(ButtonType_t type) {
    return button_get_state(type) == BUTTON_STATE_PRESSED;
}

// Vérifier si un bouton est en pression longue
bool button_is_long_pressed(ButtonType_t type) {
    return button_get_state(type) == BUTTON_STATE_LONG_PRESSED;
}
