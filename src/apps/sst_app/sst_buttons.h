#ifndef SST_BUTTONS_H
#define SST_BUTTONS_H

#include "../../kernel/core/kernel.h"
#include "../../kernel/hal/button_manager.h"

// Configuration des boutons SST
#define SST_BUTTON_INCREMENT_PIN 4    // Pin pour le bouton d'incrémentation
#define SST_BUTTON_DECREMENT_PIN 12    // Pin pour le bouton de décrémentation

// Fonctions principales
SysError_t sst_buttons_init(void);
void sst_buttons_loop(void);
void sst_buttons_deinit(void);

// Callbacks des boutons
void sst_button_increment_callback(ButtonType_t type, ButtonEvent_t event);
void sst_button_decrement_callback(ButtonType_t type, ButtonEvent_t event);

// Fonction utilitaire
bool sst_buttons_is_enabled(void);

#endif // SST_BUTTONS_H
