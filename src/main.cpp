#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include "kernel/core/kernel.h"
#include "kernel/core/task_manager.h"
#include "kernel/core/memory_manager.h"
#include "kernel/core/log_system_optimized.h"
#include "kernel/core/system_monitor.h"
#include "kernel/interface/interface.h"
#include "kernel/network/wifi_manager.h"
#include "kernel/network/wifi_persistence.h"
#include "kernel/network/ntp_manager.h"
#include "kernel/network/http_client.h"
#include "kernel/app/app_manager.h"
#include "kernel/core/minimal_config.h"
#include "apps/sst_app/sst_app.h"
#include "kernel/hal/rtc_manager.h"
#include "kernel/hal/time_sync_manager.h"
#include <time.h>

// DMD includes pour l'animation de boot
#include "../lib/DMD32-main/DMD32.h"
#include "../lib/DMD32-main/fonts/SystemFont5x7.h"
#include "../lib/DMD32-main/fonts/Arial_Black_16_ISO_8859_1.h"
#include "animations/LoadingDotsAnimation.h"

// Configuration DMD pour l'animation de boot
#define DISPLAYS_ACROSS 1
#define DISPLAYS_DOWN 1
#define DMD_REFRESH_RATE 300

// Variables globales DMD
DMD dmd(DISPLAYS_ACROSS, DISPLAYS_DOWN);
hw_timer_t * dmd_timer = NULL;
TaskHandle_t dmd_task_handle = NULL;
volatile bool dmd_task_running = false;
volatile bool dmd_refresh_needed = false;

// Pas d'animation de points - utilisation d'une barre de progression simple

// Variables globales du système
static bool system_initialized = false;
volatile bool system_running = false;

// Le système de persistance WiFi est maintenant géré par wifi_persistence.h/cpp

// Fonctions de gestion des credentials WiFi avec persistance (wrappers pour compatibilité)
SysError_t save_wifi_credentials(const char* ssid, const char* password) {
    return wifi_persistence_save_credentials(ssid, password);
}

bool load_wifi_credentials() {
    SysError_t result = wifi_persistence_load_credentials();
    return (result == SYS_OK) && wifi_persistence_has_credentials();
}

const char* get_stored_ssid() {
    return wifi_persistence_get_ssid();
}

const char* get_stored_password() {
    return wifi_persistence_get_password();
}

void clear_wifi_credentials() {
    wifi_persistence_clear_credentials();
}

// Fonction de connexion WiFi avec sauvegarde automatique
SysError_t connect_to_wifi(const char* ssid, const char* password) {
    // Validation des paramètres
    if (!ssid || !password) {
        kernel_log(LOG_LEVEL_ERROR, "Invalid connection parameters");
        return SYS_INVALID_PARAM;
    }
    
    kernel_log(LOG_LEVEL_INFO, "Connecting to WiFi: %s", ssid);
    
    // Vérifier que le WiFi est en mode STA
    if (WiFi.getMode() != WIFI_MODE_STA) {
        kernel_log(LOG_LEVEL_WARN, "Setting WiFi mode to STA");
        WiFi.mode(WIFI_STA);
        delay(100);
    }
    
    // Tenter la connexion
    WiFi.begin(ssid, password);
    
    // Attendre la connexion avec timeout configurable
    const int max_attempts = 30; // 15 secondes au lieu de 10
    const int attempt_delay = 500; // 500ms entre les tentatives
    
    int attempts = 0;
    wl_status_t last_status = WL_IDLE_STATUS;
    
    while (WiFi.status() != WL_CONNECTED && attempts < max_attempts) {
        wl_status_t current_status = WiFi.status();
        
        // Log les changements de statut
        if (current_status != last_status) {
            kernel_log(LOG_LEVEL_INFO, "WiFi status changed: %d -> %d", last_status, current_status);
            last_status = current_status;
        }
        
        delay(attempt_delay);
        Serial.print(".");
        attempts++;
    }
    
    Serial.println();
    
    // Analyser le résultat
    wl_status_t final_status = WiFi.status();
    
    if (final_status == WL_CONNECTED) {
        kernel_log(LOG_LEVEL_INFO, "WiFi connected successfully!");
        kernel_log(LOG_LEVEL_INFO, "IP: %s, Gateway: %s, RSSI: %d dBm", 
                  WiFi.localIP().toString().c_str(),
                  WiFi.gatewayIP().toString().c_str(),
                  WiFi.RSSI());
        
        // SAUVEGARDE AUTOMATIQUE des credentials après connexion réussie
        SysError_t save_result = save_wifi_credentials(ssid, password);
        if (save_result == SYS_OK) {
            kernel_log(LOG_LEVEL_INFO, "Credentials automatically saved for next boot");
        } else {
            kernel_log(LOG_LEVEL_WARN, "Failed to save credentials automatically");
        }
        
        return SYS_OK;
    } else {
        // Log détaillé de l'erreur
        const char* error_msg = "Unknown error";
        switch (final_status) {
            case WL_NO_SSID_AVAIL:
                error_msg = "SSID not found";
                break;
            case WL_CONNECT_FAILED:
                error_msg = "Connection failed (wrong password?)";
                break;
            case WL_DISCONNECTED:
                error_msg = "Disconnected";
                break;
            case WL_IDLE_STATUS:
                error_msg = "Idle status";
                break;
            case WL_SCAN_COMPLETED:
                error_msg = "Scan completed but not connected";
                break;
            default:
                error_msg = "Unknown status";
                break;
        }
        
        kernel_log(LOG_LEVEL_ERROR, "WiFi connection failed: %s (status: %d)", error_msg, final_status);
        return SYS_ERROR;
    }
}

// Tâche principale du système (simplifiée)
void system_main_task(void* parameter) {
    SERIAL_PRINTLN_MINIMAL("Main task start");
    
    int main_counter = 0;
    while (system_running) {
        if (main_counter % 300 == 0) { // Toutes les 5 minutes au lieu de 30 secondes
            kernel_log(LOG_LEVEL_INFO, "System - Run: %d, Heap: %lu", 
                         main_counter, esp_get_free_heap_size());
        }
        
        main_counter++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    SERIAL_PRINTLN_MINIMAL("Main task stop");
    vTaskDelete(NULL);
}

// Tâche de synchronisation automatique du temps
void time_sync_task(void* parameter) {
    kernel_log(LOG_LEVEL_INFO, "Time sync task start");
    
    while (system_running) {
        // Synchronisation automatique toutes les heures
        SysError_t result = time_sync_automatic();
        if (result == SYS_OK) {
            TimeSource_t source = time_sync_get_current_source();
            kernel_log(LOG_LEVEL_INFO, "Time sync OK - Source: %s", 
                      (source == TIME_SOURCE_NTP) ? "NTP" :
                      (source == TIME_SOURCE_RTC) ? "RTC" : "SYSTEM");
        } else {
            kernel_log(LOG_LEVEL_WARN, "Time sync failed");
        }
        
        // Attendre 1 heure (3600000 ms)
        vTaskDelay(pdMS_TO_TICKS(3600000));
    }
    
    kernel_log(LOG_LEVEL_INFO, "Time sync task stop");
    vTaskDelete(NULL);
}

// Tâche de surveillance WiFi (optimisée)
void wifi_supervision_task(void* parameter) {
    kernel_log(LOG_LEVEL_INFO, "WiFi task start");
    
    int supervision_counter = 0;
    bool was_connected = false;
    int reconnect_attempts = 0;
    int weak_signal_counter = 0;
    int last_rssi = 0;
    
    while (system_running) {
        bool is_connected = (WiFi.status() == WL_CONNECTED);
        int current_rssi = is_connected ? WiFi.RSSI() : 0;
        
        // Détecter les changements de statut (toujours loggé)
        if (is_connected != was_connected) {
            if (is_connected) {
                kernel_log(LOG_LEVEL_INFO, "WiFi CON - IP: %s, RSSI: %d", 
                          WiFi.localIP().toString().c_str(), current_rssi);
            } else {
                kernel_log(LOG_LEVEL_WARN, "WiFi DIS");
                if (wifi_persistence_has_credentials()) {
                    kernel_log(LOG_LEVEL_INFO, "Reconnect with creds");
                    WiFi.begin(wifi_persistence_get_ssid(), wifi_persistence_get_password());
                } else {
                    kernel_log(LOG_LEVEL_INFO, "No creds - manual needed");
                }
            }
            was_connected = is_connected;
            reconnect_attempts = 0; // Reset counter on status change
        }
        
        // Log périodique du statut (toutes les 30 minutes)
        if (supervision_counter % 1800 == 0) { // Toutes les 30 minutes au lieu de 10
            if (is_connected) {
                kernel_log(LOG_LEVEL_INFO, "WiFi - Status: CON, RSSI: %d, IP: %s", 
                          current_rssi, WiFi.localIP().toString().c_str());
            } else {
                kernel_log(LOG_LEVEL_WARN, "WiFi - Status: DIS");
            }
        }
        
        // Vérifier la qualité du signal (warning si faible, mais moins fréquent)
        if (is_connected && current_rssi < -80) {
            weak_signal_counter++;
            
            // Log seulement toutes les 15 minutes pour éviter le spam
            if (weak_signal_counter % 900 == 0) { // Toutes les 15 minutes au lieu de 5
                kernel_log(LOG_LEVEL_WARN, "WiFi - Weak signal: %d dBm", current_rssi);
            }
        } else {
            weak_signal_counter = 0; // Reset counter when signal is good
        }
        
        // Détecter les changements significatifs de RSSI (seulement si très important)
        if (is_connected && abs(current_rssi - last_rssi) > 20) { // Augmenté de 10 à 20 dBm
            // Log seulement toutes les 5 minutes pour éviter le spam
            static int rssi_log_counter = 0;
            rssi_log_counter++;
            
            if (rssi_log_counter % 300 == 0) { // Toutes les 5 minutes
                kernel_log(LOG_LEVEL_INFO, "RSSI change: %d -> %d", last_rssi, current_rssi);
                rssi_log_counter = 0; // Reset counter
            }
            last_rssi = current_rssi;
        }
        
        // Tentative de reconnexion si déconnecté (optimisé)
        if (!is_connected) {
            reconnect_attempts++;
            
            if (reconnect_attempts % 60 == 0) { // Toutes les 60 secondes au lieu de 30
                // Vérifier les credentials seulement quand nécessaire
                if (wifi_persistence_has_credentials()) {
                    kernel_log(LOG_LEVEL_INFO, "Reconnect %d", reconnect_attempts / 60);
                    WiFi.disconnect();
                    delay(1000);
                    WiFi.begin(wifi_persistence_get_ssid(), wifi_persistence_get_password());
                } else {
                    kernel_log(LOG_LEVEL_INFO, "No credentials for reconnect");
                }
            }
        } else if (is_connected) {
            reconnect_attempts = 0; // Reset counter when connected
        }
        
        supervision_counter++;
        vTaskDelay(pdMS_TO_TICKS(1000)); // 1 seconde
    }
    
    kernel_log(LOG_LEVEL_INFO, "WiFi task stop");
    vTaskDelete(NULL);
}

// ============================================================================
// GESTION DMD POUR L'ANIMATION DE BOOT
// ============================================================================

// ISR ultra-rapide (juste un flag - pas d'appel SPI direct)
void IRAM_ATTR dmd_trigger_scan() {
    dmd_refresh_needed = true;  // Juste un flag (1 byte)
}

// Tâche de rafraîchissement DMD (ISR + Flag optimisé)
void dmd_refresh_task(void* pvParameters) {
    SERIAL_PRINTLN_MINIMAL("DMD Task: Started (ISR + Flag mode)");

    while (dmd_task_running) {
        // Se réveiller seulement si rafraîchissement nécessaire
        if (dmd_refresh_needed) {
            dmd.scanDisplayBySPI();  // Appelé depuis tâche (sûr)
            dmd_refresh_needed = false;  // Reset le flag
        }
        
        // Attendre le prochain cycle (300µs = 0.3ms)
        vTaskDelay(pdMS_TO_TICKS(1)); // 1ms = 1000 FPS max
    }

    SERIAL_PRINTLN_MINIMAL("DMD Task: Stopped");
    vTaskDelete(NULL);
}

// Initialisation de l'écran DMD pour l'animation de boot
void dmd_boot_init(void) {
    SERIAL_PRINTLN_MINIMAL("Boot: Initializing DMD display...");

    // clear/init the DMD pixels held in RAM
    dmd.clearScreen(true);

    // Configuration du timer hardware (ISR + Flag optimisé)
    uint8_t cpuClock = ESP.getCpuFreqMHz();
    
    // Utiliser le timer 0 de l'ESP32
    dmd_timer = timerBegin(0, cpuClock, true);
    
    // Attacher la fonction ISR au timer
    timerAttachInterrupt(dmd_timer, &dmd_trigger_scan, true);
    
    // Configurer l'alarme pour appeler dmd_trigger_scan
    // 300µs = fréquence de rafraîchissement (comme dans les exemples)
    timerAlarmWrite(dmd_timer, 300, true);
    
    // Activer l'alarme
    timerAlarmEnable(dmd_timer);

    // Créer la tâche de rafraîchissement DMD (ISR + Flag)
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
        SERIAL_PRINTLN_MINIMAL("Boot: Failed to create DMD task");
        kernel_log(LOG_LEVEL_ERROR, "Boot: Failed to create DMD task");
        return;
    }

    SERIAL_PRINTLN_MINIMAL("Boot: DMD display initialized with ISR + Flag");
    kernel_log(LOG_LEVEL_INFO, "Boot: DMD display initialized with ISR + Flag");
}

// Arrêter la gestion DMD
void dmd_boot_stop(void) {
    // Arrêter la tâche
    if (dmd_task_handle) {
        dmd_task_running = false;
        vTaskDelete(dmd_task_handle);
        dmd_task_handle = NULL;
    }
    
    // Arrêter le timer ISR
    if (dmd_timer) {
        // Désactiver l'alarme
        timerAlarmDisable(dmd_timer);
        
        // Détacher l'interruption
        timerDetachInterrupt(dmd_timer);
        
        // Arrêter le timer
        timerEnd(dmd_timer);
        
        dmd_timer = NULL;
        SERIAL_PRINTLN_MINIMAL("DMD Timer: Stopped");
        kernel_log(LOG_LEVEL_INFO, "DMD Timer stopped");
    }
}

// ============================================================================
// ANIMATION DE BOOT MODERNE
// ============================================================================

// Afficher le logo S-T avec police Arial_Black_16_ISO_8859_1
void display_boot_logo() {
    dmd.clearScreen(true);
    dmd.selectFont(Arial_Black_16_ISO_8859_1);

    // Afficher "S-T" centré sur l'écran
    // La police Arial_Black_16 fait 16px de hauteur, écran 32x16
    // Centrer horizontalement et verticalement
    dmd.drawString(8, 0, "ST", 3, GRAPHICS_NORMAL);
}

// Afficher l'étape en cours : "STEP_NAME"
void display_boot_step(const char* step_name, bool is_error = false) {
    // Effacer seulement la section haute (lignes 0-7)
    for(int y = 0; y < 8; y++) {
        for(int x = 0; x < 32; x++) {
            dmd.writePixel(x, y, GRAPHICS_NORMAL, 0);
        }
    }

    dmd.selectFont(System5x7);

    // Afficher seulement le nom de l'étape (sans ">")
    if (is_error) {
        dmd.drawString(1,1, "ERROR", 5, GRAPHICS_NORMAL);
    } else if (step_name) {
        dmd.drawString(1,1, step_name, strlen(step_name), GRAPHICS_NORMAL);
    }
}


// Afficher une barre de progression simple centrée (section basse)
void display_progress_bar_centered(int percentage) {
    // Effacer seulement la section basse (lignes 8-15)
    for(int y = 8; y < 16; y++) {
        for(int x = 0; x < 32; x++) {
            dmd.writePixel(x, y, GRAPHICS_NORMAL, 0);
        }
    }

    // Barre de progression centrée (20 pixels de largeur, centrée)
    int bar_width = 20;
    int bar_start_x = (32 - bar_width) / 2;  // Centrer la barre
    int filled_width = (percentage * bar_width) / 100;

    // Position Y centrée dans la section basse
    int bar_y = 12;

    // Dessiner la barre (2 pixels de hauteur pour plus de visibilité)
    for (int x = 0; x < bar_width; x++) {
        for (int dy = 0; dy < 2; dy++) {  // 2 pixels de hauteur
            int pixel_x = bar_start_x + x;
            int pixel_y = bar_y + dy;

            if (pixel_x >= 0 && pixel_x < 32 && pixel_y >= 8 && pixel_y < 16) {
                if (x < filled_width) {
                    dmd.writePixel(pixel_x, pixel_y, GRAPHICS_NORMAL, 1); // Rempli
                } else {
                    dmd.writePixel(pixel_x, pixel_y, GRAPHICS_NORMAL, 0); // Vide
                }
            }
        }
    }
}

// Afficher une erreur d'initialisation
void display_boot_error(const char* failed_step) {
    display_boot_step("ERROR", true);
    display_progress_bar_centered(0); // Barre vide en cas d'erreur

    // Log de l'erreur
    SERIAL_PRINTF_MINIMAL("Boot ERROR: %s failed\n", failed_step);
    kernel_log(LOG_LEVEL_ERROR, "Boot initialization failed at: %s", failed_step);

    // Attendre un peu pour que l'utilisateur voie l'erreur
    vTaskDelay(pdMS_TO_TICKS(2000));
}

// Mettre à jour le progrès de l'animation
void update_boot_progress(int current_step, int total_steps, const char* step_name) {
    display_boot_step(step_name, false);
    int percentage = (current_step * 100) / total_steps;
    display_progress_bar_centered(percentage);
    vTaskDelay(pdMS_TO_TICKS(80)); // Délai pour l'effet visuel
}

// Fonction d'affichage du logo système style neofetch
void display_system_logo() {
    Serial.println();
    Serial.println("        ██████╗ ██ ██████╗      OS: D'O-CORE v" DO_CORE_VERSION " \"IRRIG Distro\"");
    Serial.println("        ██╔══██╗ ██╔═══██╗      Host: ESP32 DevKit");
    Serial.println("        ██║  ██║ ██║   ██║      Kernel: ESP-IDF");
    
    // Calculer l'uptime
    uint32_t uptime_seconds = system_monitor_get_uptime(); // Utiliser la fonction du système de monitoring
    uint32_t hours = uptime_seconds / 3600;
    uint32_t minutes = (uptime_seconds % 3600) / 60;
    uint32_t seconds = uptime_seconds % 60;
    Serial.printf("        ██║  ██║ ██║   ██║      Uptime: %luh %lum %lus\n", hours, minutes, seconds);
    
    // Informations système
    uint32_t free_heap = esp_get_free_heap_size();
    uint32_t total_heap = esp_get_minimum_free_heap_size() + free_heap;
    uint32_t used_heap = total_heap - free_heap;
    uint32_t cpu_freq = ESP.getCpuFreqMHz();
    
    Serial.printf("        ██████╔╝ ╚██████╔╝      Packages: %d tasks\n", task_get_count());
    Serial.println("        ╚═════╝  ╚═════╝        Shell: DORUS-CORE CLI");
    Serial.printf("                                CPU: %luMHz\n", cpu_freq);
    Serial.printf("                                Memory: %luMB / %luMB\n", used_heap/1024, total_heap/1024);
    
    // Santé du système
    String system_health = system_monitor_is_system_healthy() ? "HEALTHY" : "UNHEALTHY"; // Utiliser la fonction du système de monitoring
    Serial.printf("                                System Health: %s\n", system_health.c_str());
    
    // Statut WiFi
    String wifi_status = "DISCONNECTED";
    String ip_address = "N/A";
    String rssi_info = "";
    if (WiFi.status() == WL_CONNECTED) {
        wifi_status = "CONNECTED";
        ip_address = WiFi.localIP().toString();
        int rssi = WiFi.RSSI();
        rssi_info = String(rssi) + " dBm";
    }
    Serial.printf("                                WiFi: %s\n", wifi_status.c_str());
    Serial.printf("                                IP: %s\n", ip_address.c_str());
    if (rssi_info.length() > 0) {
        Serial.printf("                                Signal: %s\n", rssi_info.c_str());
    }
    
    // Informations sur les logs
    uint32_t log_count = log_system_get_count();
    uint32_t total_logs = log_system_get_total_messages();
    Serial.printf("                                Logs: %lu/%lu messages\n", log_count, total_logs);
    
    // Heure locale
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    Serial.printf("                                Local Time: %02d:%02d:%02d\n", 
                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    
    Serial.println();
    Serial.printf("       v%s \"TSITERA Distro\"\n", DO_CORE_VERSION);
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    SERIAL_PRINTLN_MINIMAL("=== D'O-Core Init ===");
    SERIAL_PRINTF_MINIMAL("Ver: %s\n", DO_CORE_VERSION);

    // Afficher la mémoire initiale
    SERIAL_PRINTF_MINIMAL("Heap: %lu\n", esp_get_free_heap_size());

    // Initialiser DMD pour l'animation de boot (TRÈS TÔT)
    dmd_boot_init();
    display_boot_logo();
    delay(2000); // Afficher le logo 2 secondes
    dmd.clearScreen(true); // Effacer pour commencer l'animation

    // Initialiser NVS pour la persistance des données
    update_boot_progress(1, 15, "NVS");
    SERIAL_PRINTLN_MINIMAL("NVS init...");
    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_ret);
    SERIAL_PRINTLN_MINIMAL("NVS OK");

    // Initialiser seulement les composants essentiels
    update_boot_progress(2, 15, "Core");
    SERIAL_PRINTLN_MINIMAL("Init components...");

    // Gestionnaire de tâches
    SysError_t result = task_manager_init();
    if (result != SYS_OK) {
        display_boot_error("Task Manager");
        SERIAL_PRINTLN_MINIMAL(MSG_TASK_FAIL);
        return;
    }

    // Gestionnaire de mémoire
    update_boot_progress(3, 15, "Mem");
    SysError_t mem_result = memory_manager_init();
    if (mem_result != SYS_OK) {
        SERIAL_PRINTLN_MINIMAL(MSG_MEM_FAIL);
        return;
    }

    // Initialiser le système de logs
    update_boot_progress(4, 15, "Logs");
    SysError_t log_result = log_system_init();
    if (log_result != SYS_OK) {
        SERIAL_PRINTLN_MINIMAL("Log fail");
        return;
    }

    // Initialiser le système de monitoring
    update_boot_progress(5, 15, "Shell");
    SysError_t monitor_result = system_monitor_init();
    if (monitor_result != SYS_OK) {
        SERIAL_PRINTLN_MINIMAL("Monitor fail");
        return;
    }

    // Démarrer le monitoring
    system_monitor_start();

    // Initialiser le système de persistance WiFi
    update_boot_progress(6, 15, "WiFi");
    SysError_t wifi_persist_result = wifi_persistence_init();
    if (wifi_persist_result != SYS_OK) {
        SERIAL_PRINTLN_MINIMAL("WiFi persistence init failed");
        kernel_log(LOG_LEVEL_ERROR, "WiFi persistence init failed");
    }
    
    // Initialiser le gestionnaire de WiFi
    wifi_manager_init(nullptr);
    SERIAL_PRINTLN_MINIMAL("WiFi init");

    // Initialiser le gestionnaire NTP
    update_boot_progress(7, 15, "NTP");
    SysError_t ntp_result = ntp_init();
    if (ntp_result != SYS_OK) {
        display_boot_error("NTP Manager");
        SERIAL_PRINTLN_MINIMAL(MSG_NTP_FAIL);
        return;
    }

    // Initialiser le module HTTP Client
    update_boot_progress(8, 15, "HTTP");
    SERIAL_PRINTLN_MINIMAL("HTTP Client init...");
    kernel_log(LOG_LEVEL_INFO, "HTTP Client init");
    SysError_t http_result = http_client_module_init();
    if (http_result != SYS_OK) {
        SERIAL_PRINTLN_MINIMAL("HTTP Client fail");
        kernel_log(LOG_LEVEL_ERROR, "HTTP Client init failed");
        return;
    }
    SERIAL_PRINTLN_MINIMAL("HTTP Client OK");

    // Initialiser le gestionnaire RTC DS3231
    update_boot_progress(9, 15, "RTC");
    SERIAL_PRINTLN_MINIMAL("RTC init...");
    SysError_t rtc_result = rtc_manager_init();
    if (rtc_result != SYS_OK) {
        SERIAL_PRINTLN_MINIMAL("RTC fail");
        kernel_log(LOG_LEVEL_ERROR, "RTC Manager initialization failed");
        // Continuer sans RTC (mode dégradé)
    } else {
        SERIAL_PRINTLN_MINIMAL("RTC OK");
        kernel_log(LOG_LEVEL_INFO, "RTC Manager initialized successfully");
    }

    // Initialiser le gestionnaire de synchronisation du temps
    update_boot_progress(10, 15, "Sync");
    SERIAL_PRINTLN_MINIMAL("Time sync init...");
    SysError_t time_sync_result = time_sync_init();
    if (time_sync_result != SYS_OK) {
        SERIAL_PRINTLN_MINIMAL("Time sync fail");
        kernel_log(LOG_LEVEL_ERROR, "Time Sync Manager initialization failed");
        return;
    }
    SERIAL_PRINTLN_MINIMAL("Time sync OK");

    // Network initialization complete

    // Initialiser le gestionnaire d'applications
    update_boot_progress(11, 15, "Apps");
    SysError_t app_result = app_manager_init();
    if (app_result != SYS_OK) {
        SERIAL_PRINTLN_MINIMAL(MSG_APP_FAIL);
        return;
    }

    // Enregistrer l'application SST
    update_boot_progress(12, 15, "SST");
    uint8_t sst_app_id;
    SysError_t sst_result = sst_app_register(&sst_app_id);
    if (sst_result == SYS_OK) {
        SERIAL_PRINTF_MINIMAL("SST App registered with ID: %d\n", sst_app_id);
        kernel_log(LOG_LEVEL_INFO, "SST App registered with ID: %d", sst_app_id);
    } else {
        SERIAL_PRINTLN_MINIMAL("SST App registration failed");
        kernel_log(LOG_LEVEL_ERROR, "SST App registration failed");
    }

    // Initialiser le WiFi (sans connexion automatique)
    SERIAL_PRINTLN_MINIMAL("WiFi init (no auto)");
    kernel_log(LOG_LEVEL_INFO, "WiFi init");
    WiFi.mode(WIFI_STA);
    SERIAL_PRINTLN_MINIMAL("WiFi STA ready");
    
    // Tenter la connexion automatique si des credentials sont sauvegardés
    update_boot_progress(13, 15, "WiFi ");
    SERIAL_PRINTLN_MINIMAL("Check saved WiFi...");
    kernel_log(LOG_LEVEL_INFO, "Check WiFi creds");
    if (wifi_persistence_has_credentials()) {
        SERIAL_PRINTF_MINIMAL("Found: %s\n", wifi_persistence_get_ssid());
        kernel_log(LOG_LEVEL_INFO, "Found creds: %s", wifi_persistence_get_ssid());
        SERIAL_PRINTLN_MINIMAL("Auto connect...");

        if (connect_to_wifi(wifi_persistence_get_ssid(), wifi_persistence_get_password()) == SYS_OK) {
            SERIAL_PRINTLN_MINIMAL("WiFi OK");
            kernel_log(LOG_LEVEL_INFO, "WiFi auto OK");
        } else {
            SERIAL_PRINTLN_MINIMAL(MSG_WIFI_FAIL);
            SERIAL_PRINTLN_MINIMAL("Use 'wifi_save' to update");
            kernel_log(LOG_LEVEL_ERROR, MSG_WIFI_FAIL);
        }
    } else {
        SERIAL_PRINTLN_MINIMAL("No saved creds");
        kernel_log(LOG_LEVEL_INFO, "No saved creds");
        SERIAL_PRINTLN_MINIMAL("Use 'wifi_save' to save");
    }

    SERIAL_PRINTF_MINIMAL("Heap after WiFi: %lu\n", esp_get_free_heap_size());

    // Synchronisation NTP initiale
    update_boot_progress(15, 15, "NTP");
    SERIAL_PRINTLN_MINIMAL("NTP sync...");
    kernel_log(LOG_LEVEL_INFO, "NTP sync");

    NtpStatus_t sync_status = ntp_sync();
    if (sync_status == NTP_STATUS_SYNCED) {
        SERIAL_PRINTLN_MINIMAL("NTP OK");
        kernel_log(LOG_LEVEL_INFO, "NTP OK");

        // Log avec timestamp précis
        time_t current_time = ntp_get_time();
        kernel_log(LOG_LEVEL_INFO, "Start: %s", ntp_format_time(current_time).c_str());

        // Vérifier les conditions temporelles
        if (ntp_is_business_hours()) {
            kernel_log(LOG_LEVEL_INFO, "Business mode");
        } else if (ntp_is_night_time()) {
            kernel_log(LOG_LEVEL_INFO, "Night mode");
        }
    } else {
        SERIAL_PRINTLN_MINIMAL(MSG_NTP_FAIL);
        kernel_log(LOG_LEVEL_WARN, MSG_NTP_FAIL);
    }
    
    // Initialiser l'interface (simplifiée)
    SysError_t interface_result = interface_init();
    if (interface_result != SYS_OK) {
        SERIAL_PRINTLN_MINIMAL("Interface fail");
        return;
    }

    // Synchronisation initiale du temps
    SERIAL_PRINTLN_MINIMAL("Initial time sync...");
    kernel_log(LOG_LEVEL_INFO, "Performing initial time synchronization");
    SysError_t initial_sync_result = time_sync_automatic();
    if (initial_sync_result == SYS_OK) {
        SERIAL_PRINTLN_MINIMAL("Initial sync OK");
        kernel_log(LOG_LEVEL_INFO, "Initial time synchronization successful");
        
        // Afficher l'heure actuelle
        time_t current_time = time_sync_get_current_time();
        kernel_log(LOG_LEVEL_INFO, "Current time: %s", time_sync_format_current_time().c_str());
    } else {
        SERIAL_PRINTLN_MINIMAL("Initial sync failed");
        kernel_log(LOG_LEVEL_WARN, "Initial time synchronization failed");
    }

    // Activer le système
    system_initialized = true;
    system_running = true;
    
    // Créer les tâches essentielles seulement
    uint8_t task_id;
    
    // Tâche principale
    result = task_create_pinned_to_core("SystemMain", system_main_task, NULL, 
                                       PRIORITY_NORMAL, STACK_SIZE_SMALL, 0, &task_id);
    if (result == SYS_OK) {
        SERIAL_PRINTF_MINIMAL("Main task: %d\n", task_id);
        kernel_log(LOG_LEVEL_INFO, "Main task: %d", task_id);
    } else {
        SERIAL_PRINTLN_MINIMAL(MSG_TASK_FAIL);
        kernel_log(LOG_LEVEL_ERROR, MSG_TASK_FAIL);
    }
    
    // Tâche WiFi
    result = task_create_pinned_to_core("WiFiSupervision", wifi_supervision_task, NULL, 
                                       PRIORITY_LOW, STACK_SIZE_SMALL, 0, &task_id);
    if (result == SYS_OK) {
        SERIAL_PRINTF_MINIMAL("WiFi task: %d\n", task_id);
        kernel_log(LOG_LEVEL_INFO, "WiFi task: %d", task_id);
    } else {
        SERIAL_PRINTLN_MINIMAL(MSG_TASK_FAIL);
        kernel_log(LOG_LEVEL_ERROR, MSG_TASK_FAIL);
    }
    
    // Tâche de synchronisation du temps
    result = task_create_pinned_to_core("TimeSync", time_sync_task, NULL, 
                                       PRIORITY_LOW, STACK_SIZE_SMALL, 0, &task_id);
    if (result == SYS_OK) {
        SERIAL_PRINTF_MINIMAL("Time sync task: %d\n", task_id);
        kernel_log(LOG_LEVEL_INFO, "Time sync task: %d", task_id);
    } else {
        SERIAL_PRINTLN_MINIMAL(MSG_TASK_FAIL);
        kernel_log(LOG_LEVEL_ERROR, MSG_TASK_FAIL);
    }
    
    // Animation de boot terminée
    display_progress_bar_centered(100); // Barre complète
    delay(1500); // Afficher la barre complète pendant 1.5 secondes

    // Nettoyer l'écran DMD pour l'application SST
    dmd.clearScreen(true);

    // Démarrer automatiquement l'application SST après le boot complet
    if (sst_result == SYS_OK) {
        SERIAL_PRINTLN_MINIMAL("Starting SST App automatically...");
        kernel_log(LOG_LEVEL_INFO, "Starting SST App automatically after boot");
        SysError_t start_result = app_start(sst_app_id);
        if (start_result == SYS_OK) {
            SERIAL_PRINTLN_MINIMAL("SST App started successfully");
            kernel_log(LOG_LEVEL_INFO, "SST App started automatically");
        } else {
            SERIAL_PRINTLN_MINIMAL("Failed to start SST App");
            kernel_log(LOG_LEVEL_ERROR, "Failed to start SST App automatically");
        }
    }

    SERIAL_PRINTLN_MINIMAL("=== D'O-Core Ready ===");
    SERIAL_PRINTF_MINIMAL("Tasks: %d\n", task_get_count());
    SERIAL_PRINTF_MINIMAL("Heap: %lu\n", esp_get_free_heap_size());

    kernel_log(LOG_LEVEL_INFO, "D'O-Core ready - Tasks: %d", task_get_count());
    kernel_log(LOG_LEVEL_INFO, "Final heap: %lu", esp_get_free_heap_size());

    // Afficher le logo système
    display_system_logo();

    // Démarrer le shell
    interface_start();
}

void loop() {
    // Boucle principale du système
    
    // Boucle Application Manager
    app_manager_loop();
    
    // Délai pour éviter de surcharger le CPU
    delay(10);
}