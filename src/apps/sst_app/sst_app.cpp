#include "sst_app.h"
#include "sst_data.h"
#include "../../kernel/core/log_system_optimized.h"
#include "../../kernel/hal/time_sync_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// DMD includes
#include "../../../lib/DMD32-main/DMD32.h"
#include "../../../lib/DMD32-main/fonts/SystemFont5x7.h"

// Configuration DMD
#define DISPLAYS_ACROSS 1
#define DISPLAYS_DOWN 1
#define DMD_REFRESH_RATE 300

// Variables globales de l'application SST
static bool sst_app_initialized = false;
static bool sst_app_running = false;
static uint32_t sst_loop_counter = 0;

// Configuration DMD (exactement comme l'exemple)
#define DISPLAYS_ACROSS 1
#define DISPLAYS_DOWN 1
DMD dmd(DISPLAYS_ACROSS, DISPLAYS_DOWN);

// Task dédiée pour le DMD (au lieu d'ISR)
TaskHandle_t dmd_task_handle = NULL;
static bool dmd_task_running = false;

// Task de rafraîchissement DMD (au lieu d'ISR)
void dmd_refresh_task(void* pvParameters) {
    SERIAL_PRINTLN_MINIMAL("DMD Task: Started");
    
    while (dmd_task_running) {
        // Appeler scanDisplayBySPI depuis la task (pas d'ISR)
        dmd.scanDisplayBySPI();
        
        // Délai pour contrôler la fréquence de rafraîchissement
        vTaskDelay(pdMS_TO_TICKS(2)); // 1ms = ~1000 FPS max
    }
    
    SERIAL_PRINTLN_MINIMAL("DMD Task: Stopped");
    vTaskDelete(NULL);
}

// Initialisation de l'écran DMD (avec task au lieu d'ISR)
void sst_dmd_init(void) {
    SERIAL_PRINTLN_MINIMAL("SST App: Initializing DMD display with task...");
    
    // clear/init the DMD pixels held in RAM
    dmd.clearScreen(true);
    
    // Créer la task de rafraîchissement DMD
    dmd_task_running = true;
    BaseType_t result = xTaskCreatePinnedToCore(
        dmd_refresh_task,           // Fonction de la task
        "dmd_refresh_task",         // Nom de la task
        2048,                       // Taille de la pile
        NULL,                       // Paramètres
        6,                          // Priorité (haute pour le rafraîchissement)
        &dmd_task_handle,           // Handle de la task
        1                           // Core 1 (même que les autres tasks)
    );
    
    if (result != pdPASS) {
        SERIAL_PRINTLN_MINIMAL("SST App: Failed to create DMD task");
        kernel_log(LOG_LEVEL_ERROR, "SST App: Failed to create DMD task");
        return;
    }
    
    SERIAL_PRINTLN_MINIMAL("SST App: DMD display initialized with task");
    kernel_log(LOG_LEVEL_INFO, "SST App: DMD display initialized with task");
}

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
    
    // Initialiser l'écran DMD
    sst_dmd_init();
    
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
    sst_dmd_display_days_without_accident();
    
    kernel_log(LOG_LEVEL_INFO, "SST App started with P10 display");
}

// Callback d'arrêt de l'application SST
void sst_app_stop(void) {
    SERIAL_PRINTLN_MINIMAL("SST App: Stopping...");
    
    sst_app_running = false;

    // Effacer l'écran
    sst_dmd_clear_screen();
    delay(2000);
    
    // Arrêter la task DMD
    if (dmd_task_handle) {
        dmd_task_running = false;
        vTaskDelete(dmd_task_handle);
        dmd_task_handle = NULL;
    }
    
    
    
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
            SERIAL_PRINTF_MINIMAL("SST Loop: heures_par_jour: %u, increment_done_today: %s\n", 
                                 sst_data.heures_par_jour, increment_done_today ? "true" : "false");
            
            // Vérifier si on a changé de jour
            if (current_day != last_check_day) {
                increment_done_today = false;
                last_check_day = current_day;
                SERIAL_PRINTF_MINIMAL("SST Loop: New day detected (day %u) - Time source: %s\n", current_day, source_name);
            }
            
            // Vérifier si l'heure d'incrémentation est arrivée (fenêtre de ±5 minutes)
            if (!increment_done_today && sst_data.heures_par_jour > 0) {
                uint32_t target_hour = sst_data.heures_par_jour / 100;    // Heures (ex: 8)
                uint32_t target_minute = sst_data.heures_par_jour % 100;  // Minutes (ex: 30)
                
                SERIAL_PRINTF_MINIMAL("SST Loop: Target time: %02u:%02u\n", target_hour, target_minute);
                
                // Fenêtre de ±1 minutes autour de l'heure cible
                int32_t time_diff = (current_hour * 60 + current_minute) - (target_hour * 60 + target_minute);
                
                SERIAL_PRINTF_MINIMAL("SST Loop: Time difference: %d minutes\n", time_diff);
                
                if (abs(time_diff) <= 1) { // Dans la fenêtre de ±5 minutes
                    SERIAL_PRINTF_MINIMAL("SST Loop: Increment window reached! Current: %02u:%02u, Target: %02u:%02u\n", 
                                         current_hour, current_minute, target_hour, target_minute);
                    
                    // Incrémenter les jours sans accident
                    sst_data.jours_sans_accident++;
                    
                    // Incrémenter les heures travaillées
                    sst_data.heures_travaillees += sst_data.heures_par_jour;
                    
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
                    
                    SERIAL_PRINTF_MINIMAL("SST Loop: Daily increment done! Days: %u, Hours: %u\n", 
                                         sst_data.jours_sans_accident, sst_data.heures_travaillees);
                } else {
                    SERIAL_PRINTF_MINIMAL("SST Loop: Not in increment window (diff: %d min, need <= 5)\n", abs(time_diff));
                }
            } else {
                if (increment_done_today) {
                    SERIAL_PRINTLN_MINIMAL("SST Loop: Increment already done today");
                } else {
                    SERIAL_PRINTF_MINIMAL("SST Loop: heures_par_jour is 0 (value: %u)\n", sst_data.heures_par_jour);
                }
            }
        } else {
            SERIAL_PRINTLN_MINIMAL("SST Loop: No time source available (NTP/RTC/System)");
        }
    }
    
    // Afficher les jours sans accident sur l'écran DMD (toutes les 1 seconde)
    static uint32_t last_display_time = 0;
    if (current_time - last_display_time >= 1000) { // 1 seconde
        // Afficher les jours sans accident sur l'écran DMD
        sst_dmd_display_days_without_accident();
        last_display_time = current_time;
    }
    
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
