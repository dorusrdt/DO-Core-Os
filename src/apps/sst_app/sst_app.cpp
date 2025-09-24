#include "sst_app.h"
#include "sst_data.h"
#include "sst_buttons.h"
#include "../../kernel/core/log_system_optimized.h"
#include "../../kernel/hal/time_sync_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// DMD includes pour utiliser l'objet global
#include "../lib/DMD32-main/DMD32.h"
#include "../lib/DMD32-main/fonts/SystemFont5x7.h"

// Déclarations externes pour les fonctions DMD globales de main.cpp
extern DMD dmd;

// Énumération des indicateurs SST pour l'affichage en rotation
typedef enum {
    SST_INDICATOR_JSA,    // Jours Sans Accident
    SST_INDICATOR_TAC,    // Total Accidents
    SST_INDICATOR_AAC,    // Accidents Avec Arrêt
    SST_INDICATOR_DAC,    // Date Accident
    SST_INDICATOR_TFA,    // Taux Fréquence
    SST_INDICATOR_REC,    // Record
    SST_INDICATOR_COUNT   // Nombre total d'indicateurs
} SSTIndicatorType_t;

// Constantes pour l'affichage en rotation basé sur les défilements complets
#define SST_DISPLAY_MAX_CHARS 8        // Maximum 8 caractères pour la valeur
#define SST_SCROLL_SPEED_MS 100        // 100ms entre chaque étape de défilement (10 FPS)
#define SST_SCROLL_START_DELAY_MS 2000 // 2 secondes avant de commencer le défilement
#define SST_SCROLL_CYCLES_PER_INDICATOR 2  // Nombre de défilements complets par indicateur

// Noms complets des indicateurs pour le défilement
static const char* SST_INDICATOR_NAMES[SST_INDICATOR_COUNT] = {
    "Jours Sans Accident",    // SST_INDICATOR_JSA
    "Total Accidents",        // SST_INDICATOR_TAC
    "Accidents Avec Arret",   // SST_INDICATOR_AAC
    "Dernier Accident",       // SST_INDICATOR_DAC
    "Taux Frequence",         // SST_INDICATOR_TFA
    "Record Jours"            // SST_INDICATOR_REC
};

// Variables globales de l'application SST
static bool sst_app_initialized = false;
static bool sst_app_running = false;
static uint32_t sst_loop_counter = 0;


// Afficher les jours sans accident sur l'écran DMD (SEULEMENT SI LA VALEUR CHANGE)
void sst_dmd_display_days_without_accident(void) {
    // Les jours sans accident sont maintenant gérés directement dans sst_loop()

    // Variable statique pour mémoriser la dernière valeur affichée
    static uint32_t last_displayed_days = 0xFFFFFFFF; // Valeur impossible pour forcer le premier affichage

    // Ne rafraîchir l'écran que si la valeur a changé
    if (sst_data.jours_sans_accident != last_displayed_days) {
        SERIAL_PRINTF_MINIMAL("SST Display: Updating display - Days: %u -> %u\n",
                             last_displayed_days, sst_data.jours_sans_accident);

        // Effacer l'écran
        dmd.clearScreen(true);

        // Sélectionner la police System5x7 (5x7 pixels)
        dmd.selectFont(System5x7);

        // Convertir le nombre de jours en string
        char days_str[16];
        snprintf(days_str, sizeof(days_str), "%u", sst_data.jours_sans_accident);

        // Calculer la position pour centrer le nombre en haut (y=0)
        int text_width = strlen(days_str) * 6; // System5x7 = 6 pixels de largeur
        int x_pos = (32 - text_width) / 2;
        if (x_pos < 0) x_pos = 0;

        // Afficher le nombre de jours en haut
        dmd.drawString(x_pos, 0, days_str, strlen(days_str), GRAPHICS_NORMAL);

        // Afficher "JOURS" en bas (y=9 pour laisser de l'espace)
        const char* jours_text = "JOURS";
        int jours_width = strlen(jours_text) * 6; // System5x7 = 6 pixels de largeur
        int jours_x_pos = (32 - jours_width) / 2;
        if (jours_x_pos < 0) jours_x_pos = 0;

        // Afficher "JOURS" en bas
        dmd.drawString(jours_x_pos, 9, jours_text, strlen(jours_text), GRAPHICS_NORMAL);

        // Mémoriser la nouvelle valeur
        last_displayed_days = sst_data.jours_sans_accident;
    }
}

// Afficher du texte sur l'écran DMD
void sst_dmd_display_text(const char* text) {
    if (!text) {
        return;
    }

    // Effacer l'écran
    dmd.clearScreen(true);

    // Sélectionner la police System5x7
    dmd.selectFont(System5x7);

    // Afficher le texte
    dmd.drawString(0, 0, text, strlen(text), GRAPHICS_NORMAL);
}

// Effacer l'écran DMD
void sst_dmd_clear_screen(void) {
    dmd.clearScreen(true);
}

// Fonction pour formater la valeur d'un indicateur SST
void sst_format_indicator_value(SSTIndicatorType_t indicator, char* value_buffer, size_t buffer_size, const char** label) {
    if (!value_buffer || !label || buffer_size == 0) {
        return;
    }

    // Valeurs par défaut
    *label = "???";
    value_buffer[0] = '\0';

    switch (indicator) {
        case SST_INDICATOR_JSA:
            *label = "JSA";
            snprintf(value_buffer, buffer_size, "%u", sst_data.jours_sans_accident);
            break;

        case SST_INDICATOR_TAC:
            *label = "TAC";
            snprintf(value_buffer, buffer_size, "%u", sst_data.total_accidents);
            break;

        case SST_INDICATOR_AAC:
            *label = "AAC";
            snprintf(value_buffer, buffer_size, "%u", sst_data.accidents_avec_arret);
            break;

        case SST_INDICATOR_DAC:
            *label = "DAC";
            if (sst_data.date_dernier_accident > 0) {
                struct tm* timeinfo = localtime(&sst_data.date_dernier_accident);
                if (timeinfo) {
                    strftime(value_buffer, buffer_size, "%d/%m", timeinfo);
                } else {
                    strcpy(value_buffer, "--/--");
                }
            } else {
                strcpy(value_buffer, "--/--");
            }
            break;

        case SST_INDICATOR_TFA:
            *label = "TFA";
            if (sst_data.taux_frequence >= 0.0f) {
                // Afficher avec 2 décimales, mais tronquer si trop long
                char temp[16];
                snprintf(temp, sizeof(temp), "%.2f", sst_data.taux_frequence);
                // Si plus de 5 caractères, afficher seulement la partie entière
                if (strlen(temp) > 5) {
                    snprintf(value_buffer, buffer_size, "%.0f", sst_data.taux_frequence);
                } else {
                    strcpy(value_buffer, temp);
                }
            } else {
                strcpy(value_buffer, "0.0");
            }
            break;

        case SST_INDICATOR_REC:
            *label = "REC";
            snprintf(value_buffer, buffer_size, "%u", sst_data.record_jours_sans_accident);
            break;

        default:
            *label = "ERR";
            strcpy(value_buffer, "ERROR");
            break;
    }

    // Tronquer si trop long (sécurité)
    if (strlen(value_buffer) >= buffer_size - 1) {
        value_buffer[buffer_size - 2] = '\0'; // Garder de la place pour le null terminator
    }
}

// Variables pour la gestion du défilement basé sur les cycles complets
static bool scroll_active = false;
static uint32_t last_scroll_time = 0;
static uint32_t indicator_display_start = 0;
static SSTIndicatorType_t current_scrolling_indicator = SST_INDICATOR_JSA;
static int scroll_position = 32; // Position X de départ (à droite)
static int scroll_cycle_count = 0; // Nombre de cycles complets effectués
static int current_text_width = 0; // Largeur du texte actuel (cache)

// Fonction pour initialiser le défilement d'un indicateur
void sst_start_indicator_scroll(SSTIndicatorType_t indicator) {
    const char* indicator_name = SST_INDICATOR_NAMES[indicator];
    current_text_width = strlen(indicator_name) * 6; // Calculer la largeur une fois

    SERIAL_PRINTF_MINIMAL("SST Display: Starting scroll for '%s' (width: %dpx)\n", indicator_name, current_text_width);

    // Effacer seulement la ligne 9 (où se fait le défilement)
    dmd.drawFilledBox(0, 9, 31, 9, GRAPHICS_NORMAL); // Effacement optimisé

    // Initialiser le défilement
    current_scrolling_indicator = indicator;
    scroll_position = 32; // Commencer à droite de l'écran
    scroll_active = true;
    last_scroll_time = millis();
}

// Nouvelle fonction d'affichage en rotation basé sur les défilements complets
void sst_dmd_display_rotating_indicators(void) {
    // Variables statiques pour la gestion de la rotation
    static SSTIndicatorType_t current_indicator = SST_INDICATOR_JSA;
    static char last_value[SST_DISPLAY_MAX_CHARS] = "";

    uint32_t current_time = millis();

    // Vérifier si on doit changer d'indicateur (après 2 défilements complets)
    if (scroll_cycle_count >= SST_SCROLL_CYCLES_PER_INDICATOR) {
        // Passer à l'indicateur suivant
        current_indicator = (SSTIndicatorType_t)((current_indicator + 1) % SST_INDICATOR_COUNT);
        scroll_cycle_count = 0; // Reset le compteur
        scroll_active = false; // Reset le défilement

        SERIAL_PRINTF_MINIMAL("SST Display: Changing to indicator %d after %d complete scrolls\n",
                             current_indicator, SST_SCROLL_CYCLES_PER_INDICATOR);
    }

    // Formater la valeur de l'indicateur actuel
    char value_buffer[SST_DISPLAY_MAX_CHARS];
    const char* label;
    sst_format_indicator_value(current_indicator, value_buffer, sizeof(value_buffer), &label);

    // Afficher la valeur en haut (statique et centrée) - seulement si elle change
    if (strcmp(value_buffer, last_value) != 0) {
        SERIAL_PRINTF_MINIMAL("SST Display: Updating static value - %s: %s\n", label, value_buffer);

        // Effacer seulement la partie haute (lignes 0-7) pour la valeur
        for(int y = 0; y < 8; y++) {
            for(int x = 0; x < 32; x++) {
                dmd.writePixel(x, y, GRAPHICS_NORMAL, 0);
            }
        }

        // Sélectionner la police System5x7
        dmd.selectFont(System5x7);

        // Calculer la position pour centrer la valeur en haut (y=0)
        int value_width = strlen(value_buffer) * 6;
        int value_x_pos = (32 - value_width) / 2;
        if (value_x_pos < 0) value_x_pos = 0;

        // Afficher la valeur en haut
        dmd.drawString(value_x_pos, 0, value_buffer, strlen(value_buffer), GRAPHICS_NORMAL);

        // Mémoriser la nouvelle valeur
        strcpy(last_value, value_buffer);
    }

    // Gérer le défilement du nom de l'indicateur
    // Démarrer le défilement après un délai initial (2 secondes)
    if (!scroll_active && (current_time - indicator_display_start) >= SST_SCROLL_START_DELAY_MS) {
        sst_start_indicator_scroll(current_indicator);
    }

    // Animer le défilement si actif (défilement continu)
    if (scroll_active) {
        if (current_time - last_scroll_time >= SST_SCROLL_SPEED_MS) {
            // Effacer la ligne 9 avant de redessiner
            dmd.drawFilledBox(0, 9, 31, 9, GRAPHICS_NORMAL);

            // Dessiner le texte à la position actuelle
            const char* indicator_name = SST_INDICATOR_NAMES[current_scrolling_indicator];
            dmd.selectFont(System5x7);
            dmd.drawString(scroll_position, 9, indicator_name, strlen(indicator_name), GRAPHICS_NORMAL);

            // Déplacer la position vers la gauche
            scroll_position--;

            // Si le texte est complètement sorti à gauche, le ramener à droite et compter un cycle
            if (scroll_position < -current_text_width) {
                scroll_position = 32; // Ramener à droite
                scroll_cycle_count++; // Incrémenter le compteur de cycles

                SERIAL_PRINTF_MINIMAL("SST Display: Scroll cycle %d/%d completed\n",
                                     scroll_cycle_count, SST_SCROLL_CYCLES_PER_INDICATOR);
            }

            last_scroll_time = current_time;
        }
    }
}

// Callback d'initialisation de l'application SST
SysError_t sst_app_init(void) {
    SERIAL_PRINTLN_MINIMAL("SST App: Initializing...");

    // Initialisation des variables
    sst_app_initialized = true;
    sst_app_running = false;
    sst_loop_counter = 0;

    // Initialiser les données SST
    SysError_t result = sst_data_init();
    if (result != SYS_OK) {
        SERIAL_PRINTLN_MINIMAL("SST App: Failed to initialize SST data");
        return result;
    }

    // Initialiser les boutons SST
    result = sst_buttons_init();
    if (result != SYS_OK) {
        SERIAL_PRINTLN_MINIMAL("SST App: Failed to initialize buttons");
        kernel_log(LOG_LEVEL_ERROR, "SST App buttons initialization failed");
        // Continuer sans les boutons
    }

    // Note: DMD est maintenant initialisé dans main.cpp pour l'animation de boot

    SERIAL_PRINTLN_MINIMAL("SST App: Initialized successfully");
    kernel_log(LOG_LEVEL_INFO, "SST App initialized");

    return SYS_OK;
}

// Callback de démarrage de l'application SST
void sst_app_start(void) {
    SERIAL_PRINTLN_MINIMAL("SST App: Starting...");

    sst_app_running = true;
    sst_loop_counter = 0;

    // Afficher le message de démarrage
    SERIAL_PRINTLN_MINIMAL("=== SST Application ===");
    SERIAL_PRINTLN_MINIMAL("SST App with P10 Display started!");
    SERIAL_PRINTLN_MINIMAL("======================");

    // Afficher le message de démarrage sur l'écran
    sst_dmd_display_text("SST START");
    delay(2000);

    // Initialiser les variables de défilement
    scroll_active = false;
    scroll_position = 32;
    scroll_cycle_count = 0;
    indicator_display_start = millis();

    // Démarrer l'affichage en rotation des indicateurs
    sst_dmd_display_rotating_indicators();

    kernel_log(LOG_LEVEL_INFO, "SST App started with P10 display");
}

// Callback d'arrêt de l'application SST
void sst_app_stop(void) {
    SERIAL_PRINTLN_MINIMAL("SST App: Stopping...");

    sst_app_running = false;

    // Désinitialiser les boutons SST
    sst_buttons_deinit();

    // Effacer l'écran
    sst_dmd_clear_screen();
    delay(2000);

    // Note: La task DMD est maintenant gérée dans main.cpp

    SERIAL_PRINTLN_MINIMAL("SST App: Stopped");
    kernel_log(LOG_LEVEL_INFO, "SST App stopped");
}

// Callback de mise en pause de l'application SST
void sst_app_pause(void) {
    SERIAL_PRINTLN_MINIMAL("SST App: Paused");
    kernel_log(LOG_LEVEL_INFO, "SST App paused");
}

// Callback de reprise de l'application SST
void sst_app_resume(void) {
    SERIAL_PRINTLN_MINIMAL("SST App: Resumed");
    kernel_log(LOG_LEVEL_INFO, "SST App resumed");
}

// Callback de boucle principale de l'application SST
void sst_app_loop(void) {
    if (!sst_app_running) {
        return;
    }

    // ============================================================================
    // LOGIQUE D'INCRÉMENTATION SIMPLE (DIRECTEMENT DANS LOOP)
    // ============================================================================

    // Variables statiques pour la logique d'incrémentation
    static uint32_t last_increment_check = 0;
    static bool increment_done_today = false;
    static uint32_t last_check_day = 0;

    uint32_t current_time = millis();

    // Vérifier l'incrémentation toutes les 30 secondes
    if (current_time - last_increment_check >= 30000) { // 30 secondes
        last_increment_check = current_time;

        SERIAL_PRINTLN_MINIMAL("SST Loop: Checking increment logic...");

        // Obtenir l'heure actuelle (NTP > RTC > System Clock)
        time_t current_time_source = time_sync_get_current_time();
        SERIAL_PRINTF_MINIMAL("SST Loop: Current time source: %lu\n", current_time_source);

        if (current_time_source > 0) {
            // Log de la source de temps utilisée
            TimeSource_t source = time_sync_get_current_source();
            const char* source_name = (source == TIME_SOURCE_NTP) ? "NTP" :
                                     (source == TIME_SOURCE_RTC) ? "RTC" :
                                     (source == TIME_SOURCE_SYSTEM) ? "SYSTEM" : "UNKNOWN";

            struct tm* timeinfo = localtime(&current_time_source);
            uint32_t current_hour = timeinfo->tm_hour;
            uint32_t current_minute = timeinfo->tm_min;
            uint32_t current_day = timeinfo->tm_yday; // Jour de l'année (0-365)

            SERIAL_PRINTF_MINIMAL("SST Loop: Current time: %02u:%02u (day %u) - Source: %s\n",
                                 current_hour, current_minute, current_day, source_name);
            SERIAL_PRINTF_MINIMAL("SST Loop: heure_incrementation: %u, increment_done_today: %s\n",
                                 sst_data.heure_incrementation, increment_done_today ? "true" : "false");

            // Vérifier si on a changé de jour
            if (current_day != last_check_day) {
                increment_done_today = false;
                last_check_day = current_day;
                SERIAL_PRINTF_MINIMAL("SST Loop: New day detected (day %u) - Time source: %s\n", current_day, source_name);
            }

            // Vérifier si l'heure d'incrémentation est arrivée (fenêtre de ±1 minute)
            if (!increment_done_today && sst_data.heure_incrementation > 0) {
                uint32_t target_hour = sst_data.heure_incrementation / 100;    // Heures (ex: 8)
                uint32_t target_minute = sst_data.heure_incrementation % 100;  // Minutes (ex: 0)

                SERIAL_PRINTF_MINIMAL("SST Loop: Target time: %02u:%02u\n", target_hour, target_minute);

                // Fenêtre de ±1 minutes autour de l'heure cible
                int32_t time_diff = (current_hour * 60 + current_minute) - (target_hour * 60 + target_minute);

                SERIAL_PRINTF_MINIMAL("SST Loop: Time difference: %d minutes\n", time_diff);

                if (abs(time_diff) <= 1) { // Dans la fenêtre de ±5 minutes
                    SERIAL_PRINTF_MINIMAL("SST Loop: Increment window reached! Current: %02u:%02u, Target: %02u:%02u\n",
                                         current_hour, current_minute, target_hour, target_minute);

                    // Vérifier s'il y a eu des accidents dans la période métier précédente
                    bool accident_dans_periode_metier = sst_check_accidents_in_metier_period();

                    if (!accident_dans_periode_metier) {
                        // Incrémenter les jours sans accident (jour métier)
                        sst_data.jours_sans_accident++;

                        // Incrémenter les heures travaillées (utiliser la valeur configurée)
                        sst_data.heures_travaillees += sst_data.heures_travaillees_par_jour;

                        SERIAL_PRINTF_MINIMAL("SST Loop: Jour métier incrémenté! Jours sans accident: %u, Heures ajoutées: %.2f\n",
                                             sst_data.jours_sans_accident, sst_data.heures_travaillees_par_jour);
                    } else {
                        SERIAL_PRINTLN_MINIMAL("SST Loop: Accident détecté dans la période métier - Pas d'incrémentation");
                    }

                    // Mettre à jour le record si nécessaire
                    if (sst_data.jours_sans_accident > sst_data.record_jours_sans_accident) {
                        sst_data.record_jours_sans_accident = sst_data.jours_sans_accident;
                    }

                    // Recalculer le taux de fréquence
                    sst_calculate_taux_frequence();

                    // Marquer comme fait pour aujourd'hui
                    increment_done_today = true;

                    // Sauvegarder
                    sst_data_save();

                    SERIAL_PRINTF_MINIMAL("SST Loop: Daily increment done! Days: %u, Hours: %.2f\n",
                                         sst_data.jours_sans_accident, sst_data.heures_travaillees);
                } else {
                    SERIAL_PRINTF_MINIMAL("SST Loop: Not in increment window (diff: %d min, need <= 5)\n", abs(time_diff));
                }
            } else {
                if (increment_done_today) {
                    SERIAL_PRINTLN_MINIMAL("SST Loop: Increment already done today");
                } else {
                    SERIAL_PRINTF_MINIMAL("SST Loop: heure_incrementation is 0 (value: %u)\n", sst_data.heure_incrementation);
                }
            }
        } else {
            SERIAL_PRINTLN_MINIMAL("SST Loop: No time source available (NTP/RTC/System)");
        }
    }

    // Afficher les indicateurs SST en rotation (toutes les 5 secondes)
    sst_dmd_display_rotating_indicators();

    // Gérer les boutons SST
    sst_buttons_loop();

    // Petite pause pour éviter de surcharger le CPU
    vTaskDelay(pdMS_TO_TICKS(10));
}

// Structure des callbacks pour l'application SST
static const AppCallbacks_t sst_app_callbacks = {
    .init = sst_app_init,
    .start = sst_app_start,
    .stop = sst_app_stop,
    .pause = sst_app_pause,
    .resume = sst_app_resume,
    .loop = sst_app_loop
};

// Fonction d'enregistrement de l'application SST
SysError_t sst_app_register(uint8_t* app_id) {
    if (!app_id) {
        return SYS_INVALID_PARAM;
    }

    SERIAL_PRINTLN_MINIMAL("SST App: Registering...");

    // Enregistrer l'application avec l'app manager
    SysError_t result = app_register(SST_APP_NAME, SST_APP_DESCRIPTION,
                                   APP_TYPE_USER, &sst_app_callbacks, app_id);

    if (result == SYS_OK) {
        SERIAL_PRINTF_MINIMAL("SST App: Registered successfully with ID: %d\n", *app_id);
        kernel_log(LOG_LEVEL_INFO, "SST App registered with ID: %d", *app_id);
    } else {
        SERIAL_PRINTLN_MINIMAL("SST App: Registration failed");
        kernel_log(LOG_LEVEL_ERROR, "SST App registration failed");
    }

    return result;
}
