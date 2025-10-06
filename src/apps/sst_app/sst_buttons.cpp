#include "sst_buttons.h"
#include "sst_data.h"
#include "sst_sync.h"
#include "../../kernel/core/minimal_config.h"

// Variables globales
static bool sst_buttons_initialized = false;
static bool sst_buttons_enabled = true;

// Protection contre les actions multiples
static unsigned long last_increment_time = 0;
static unsigned long last_decrement_time = 0;
static unsigned long last_reset_time = 0;
static unsigned long last_accident_time = 0;
#define BUTTON_ACTION_COOLDOWN_MS 2000  // 2 secondes entre les actions

// Initialisation des boutons SST
SysError_t sst_buttons_init(void) {
    if (sst_buttons_initialized) {
        return SYS_ALREADY_INITIALIZED;
    }

    SERIAL_PRINTLN_MINIMAL("SST Buttons: Initializing...");

    // Initialiser le gestionnaire de boutons
    SysError_t result = button_manager_init();
    if (result != SYS_OK) {
        SERIAL_PRINTLN_MINIMAL("SST Buttons: Failed to initialize button manager");
        return result;
    }

    // Enregistrer le bouton d'incrémentation
    result = button_register(SST_BUTTON_INCREMENT_PIN, BUTTON_TYPE_INCREMENT, sst_button_increment_callback);
    if (result != SYS_OK) {
        SERIAL_PRINTLN_MINIMAL("SST Buttons: Failed to register increment button");
        return result;
    }

    // Enregistrer le bouton de décrémentation
    result = button_register(SST_BUTTON_DECREMENT_PIN, BUTTON_TYPE_DECREMENT, sst_button_decrement_callback);
    if (result != SYS_OK) {
        SERIAL_PRINTLN_MINIMAL("SST Buttons: Failed to register decrement button");
        return result;
    }

    sst_buttons_initialized = true;
    sst_buttons_enabled = true;

    SERIAL_PRINTLN_MINIMAL("SST Buttons: Initialized successfully");
    SERIAL_PRINTF_MINIMAL("SST Buttons: Increment button on pin %d\n", SST_BUTTON_INCREMENT_PIN);
    SERIAL_PRINTF_MINIMAL("SST Buttons: Decrement button on pin %d\n", SST_BUTTON_DECREMENT_PIN);
    kernel_log(LOG_LEVEL_INFO, "SST Buttons initialized - Increment: pin %d, Decrement: pin %d",
               SST_BUTTON_INCREMENT_PIN, SST_BUTTON_DECREMENT_PIN);

    return SYS_OK;
}

// Boucle principale des boutons SST
void sst_buttons_loop(void) {
    if (!sst_buttons_initialized || !sst_buttons_enabled) {
        return;
    }

    // Appeler la boucle du gestionnaire de boutons
    button_manager_loop();
}

// Désinitialisation des boutons SST
void sst_buttons_deinit(void) {
    if (!sst_buttons_initialized) {
        return;
    }

    // Désenregistrer les boutons
    button_unregister(BUTTON_TYPE_INCREMENT);
    button_unregister(BUTTON_TYPE_DECREMENT);

    // Désinitialiser le gestionnaire de boutons
    button_manager_deinit();

    sst_buttons_initialized = false;
    sst_buttons_enabled = false;

    SERIAL_PRINTLN_MINIMAL("SST Buttons: Deinitialized");
    kernel_log(LOG_LEVEL_INFO, "SST Buttons deinitialized");
}

// Callback du bouton d'incrémentation
void sst_button_increment_callback(ButtonType_t type, ButtonEvent_t event) {
    if (!sst_buttons_enabled) {
        return;
    }

    unsigned long current_time = millis();

    switch (event) {
        case BUTTON_EVENT_CLICK:
            // Clic court : incrémenter (avec protection)
            if (current_time - last_increment_time >= BUTTON_ACTION_COOLDOWN_MS) {
                SERIAL_PRINTLN_MINIMAL("SST Buttons: Increment button clicked");
                sst_increment_days_manual();
                // Informer le backend (événement device)
                sst_post_event(SST_EVENT_INCREMENT, nullptr);
                last_increment_time = current_time;
            } else {
                SERIAL_PRINTLN_MINIMAL("SST Buttons: Increment blocked - too soon");
            }
            break;

        case BUTTON_EVENT_LONG_PRESSED:
            // Pression longue : réinitialiser (avec protection)
            if (current_time - last_reset_time >= BUTTON_ACTION_COOLDOWN_MS) {
                SERIAL_PRINTLN_MINIMAL("SST Buttons: Increment button long pressed - RESET");
                sst_reset_days_manual();
                // Informer le backend (événement device)
                sst_post_event(SST_EVENT_RESET, nullptr);
                last_reset_time = current_time;
            } else {
                SERIAL_PRINTLN_MINIMAL("SST Buttons: Reset blocked - too soon");
            }
            break;

        case BUTTON_EVENT_PRESSED:
            SERIAL_PRINTLN_MINIMAL("SST Buttons: Increment button pressed");
            break;

        case BUTTON_EVENT_RELEASED:
            SERIAL_PRINTLN_MINIMAL("SST Buttons: Increment button released");
            break;

        default:
            break;
    }
}

// Callback du bouton de décrémentation
void sst_button_decrement_callback(ButtonType_t type, ButtonEvent_t event) {
    if (!sst_buttons_enabled) {
        return;
    }

    unsigned long current_time = millis();

    switch (event) {
        case BUTTON_EVENT_CLICK:
            // Clic court : décrémenter (avec protection)
            if (current_time - last_decrement_time >= BUTTON_ACTION_COOLDOWN_MS) {
                SERIAL_PRINTLN_MINIMAL("SST Buttons: Decrement button clicked");
                sst_decrement_days_manual();
                // Informer le backend (événement device)
                sst_post_event(SST_EVENT_DECREMENT, nullptr);
                last_decrement_time = current_time;
            } else {
                SERIAL_PRINTLN_MINIMAL("SST Buttons: Decrement blocked - too soon");
            }
            break;

        case BUTTON_EVENT_LONG_PRESSED:
            // Pression longue : ajouter un accident avec arrêt (avec protection)
            if (current_time - last_accident_time >= BUTTON_ACTION_COOLDOWN_MS) {
                SERIAL_PRINTLN_MINIMAL("SST Buttons: Decrement button long pressed - ACCIDENT WITH STOP");
                sst_add_accident(true, "Accident avec arret - Bouton");
                // Informer le backend (événement device)
                sst_post_event(SST_EVENT_ACCIDENT, "Accident avec arret - Bouton");
                last_accident_time = current_time;
            } else {
                SERIAL_PRINTLN_MINIMAL("SST Buttons: Accident blocked - too soon");
            }
            break;

        case BUTTON_EVENT_PRESSED:
            SERIAL_PRINTLN_MINIMAL("SST Buttons: Decrement button pressed");
            break;

        case BUTTON_EVENT_RELEASED:
            SERIAL_PRINTLN_MINIMAL("SST Buttons: Decrement button released");
            break;

        default:
            break;
    }
}

// Vérifier si les boutons sont activés
bool sst_buttons_is_enabled(void) {
    return sst_buttons_enabled;
}

