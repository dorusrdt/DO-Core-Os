#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <Preferences.h>
#include "kernel/core/kernel.h"
#include "kernel/core/task_manager.h"
#include "kernel/core/memory_manager.h"
#include "kernel/core/log_system_optimized.h"
#include "kernel/core/system_monitor.h"
#include "kernel/interface/interface.h"
#include "kernel/network/wifi_manager.h"
#include "kernel/network/ntp_manager.h"
#include "kernel/network/http_client.h"
#include "kernel/app/app_manager.h"
#include "kernel/core/minimal_config.h"
#include "apps/sst_app/sst_app.h"
#include "kernel/hal/rtc_manager.h"
#include "kernel/hal/time_sync_manager.h"
#include <time.h>

// Variables globales du système
static bool system_initialized = false;
volatile bool system_running = false;

// Système de stockage persistant des credentials WiFi
static Preferences wifi_prefs;

// Configuration WiFi stockée en mémoire
struct WifiCredentials {
    char ssid[32];
    char password[64];
    bool valid;
};

static WifiCredentials stored_credentials = {"", "", false};

// Fonctions de gestion des credentials WiFi avec persistance
SysError_t save_wifi_credentials(const char* ssid, const char* password) {
    // Validation des paramètres
    if (!ssid || !password) {
        kernel_log(LOG_LEVEL_ERROR, "Invalid credentials: null pointer");
        return SYS_INVALID_PARAM;
    }
    
    if (strlen(ssid) == 0 || strlen(ssid) > 31) {
        kernel_log(LOG_LEVEL_ERROR, "Invalid SSID length: %d (must be 1-31)", strlen(ssid));
        return SYS_INVALID_PARAM;
    }
    
    if (strlen(password) < 8 || strlen(password) > 63) {
        kernel_log(LOG_LEVEL_ERROR, "Invalid password length: %d (must be 8-63)", strlen(password));
        return SYS_INVALID_PARAM;
    }
    
    // Sauvegarder en mémoire RAM
    strncpy(stored_credentials.ssid, ssid, sizeof(stored_credentials.ssid) - 1);
    strncpy(stored_credentials.password, password, sizeof(stored_credentials.password) - 1);
    stored_credentials.valid = true;
    
    // Sauvegarder en mémoire flash (persistant)
    wifi_prefs.begin("wifi", false);
    bool flash_ok = wifi_prefs.putString("ssid", ssid) > 0 &&
                   wifi_prefs.putString("password", password) > 0 &&
                   wifi_prefs.putBool("valid", true);
    wifi_prefs.end();
    
    if (flash_ok) {
        kernel_log(LOG_LEVEL_INFO, "WiFi credentials saved to flash: SSID=%s", stored_credentials.ssid);
        return SYS_OK;
    } else {
        kernel_log(LOG_LEVEL_ERROR, "Failed to save credentials to flash");
        return SYS_ERROR;
    }
}

bool load_wifi_credentials() {
    // Charger depuis la mémoire flash
    wifi_prefs.begin("wifi", true);
    bool valid = wifi_prefs.getBool("valid", false);
    
    if (valid) {
        String ssid = wifi_prefs.getString("ssid", "");
        String password = wifi_prefs.getString("password", "");
        
        if (ssid.length() > 0 && password.length() > 0) {
            strncpy(stored_credentials.ssid, ssid.c_str(), sizeof(stored_credentials.ssid) - 1);
            strncpy(stored_credentials.password, password.c_str(), sizeof(stored_credentials.password) - 1);
            stored_credentials.valid = true;
            kernel_log(LOG_LEVEL_INFO, "WiFi credentials loaded from flash: SSID=%s", stored_credentials.ssid);
        } else {
            valid = false;
        }
    }
    
    wifi_prefs.end();
    return valid;
}

const char* get_stored_ssid() {
    return stored_credentials.valid ? stored_credentials.ssid : nullptr;
}

const char* get_stored_password() {
    return stored_credentials.valid ? stored_credentials.password : nullptr;
}

void clear_wifi_credentials() {
    // Effacer de la mémoire RAM
    memset(&stored_credentials, 0, sizeof(stored_credentials));
    stored_credentials.valid = false;
    
    // Effacer de la mémoire flash
    wifi_prefs.begin("wifi", false);
    wifi_prefs.clear();
    wifi_prefs.end();
    
    kernel_log(LOG_LEVEL_INFO, "WiFi credentials cleared from flash");
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
                if (load_wifi_credentials()) {
                    kernel_log(LOG_LEVEL_INFO, "Reconnect with creds");
                    WiFi.begin(get_stored_ssid(), get_stored_password());
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
        
        // Tentative de reconnexion si déconnecté
        if (!is_connected && load_wifi_credentials()) {
            reconnect_attempts++;
            
            if (reconnect_attempts % 60 == 0) { // Toutes les 60 secondes au lieu de 30
                kernel_log(LOG_LEVEL_INFO, "Reconnect %d", reconnect_attempts / 60);
                WiFi.disconnect();
                delay(1000);
                WiFi.begin(get_stored_ssid(), get_stored_password());
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
    Serial.printf("       v%s \"IRRIG Distro\"\n", DO_CORE_VERSION);
    Serial.println();
}

void setup() {
    Serial.begin(9600);
    delay(1000);
    
    SERIAL_PRINTLN_MINIMAL("=== D'O-Core Init ===");
    SERIAL_PRINTF_MINIMAL("Ver: %s\n", DO_CORE_VERSION);
    
    // Afficher la mémoire initiale
    SERIAL_PRINTF_MINIMAL("Heap: %lu\n", esp_get_free_heap_size());
    
    // Initialiser NVS pour la persistance des données
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
    SERIAL_PRINTLN_MINIMAL("Init components...");
    
    // Gestionnaire de tâches
    SysError_t result = task_manager_init();
    if (result != SYS_OK) {
        SERIAL_PRINTLN_MINIMAL(MSG_TASK_FAIL);
        return;
    }

    // Gestionnaire de mémoire
    SysError_t mem_result = memory_manager_init();
    if (mem_result != SYS_OK) {
        SERIAL_PRINTLN_MINIMAL(MSG_MEM_FAIL);
        return;
    }

    // Initialiser le système de logs
    SysError_t log_result = log_system_init();
    if (log_result != SYS_OK) {
        SERIAL_PRINTLN_MINIMAL("Log fail");
        return;
    }

    // Initialiser le système de monitoring
    SysError_t monitor_result = system_monitor_init();
    if (monitor_result != SYS_OK) {
        SERIAL_PRINTLN_MINIMAL("Monitor fail");
        return;
    }
    
    // Démarrer le monitoring
    system_monitor_start();

    // Initialiser le gestionnaire de WiFi
    wifi_manager_init(nullptr);
    SERIAL_PRINTLN_MINIMAL("WiFi init");

    // Initialiser le gestionnaire NTP
    SysError_t ntp_result = ntp_init();
    if (ntp_result != SYS_OK) {
        SERIAL_PRINTLN_MINIMAL(MSG_NTP_FAIL);
        return;
    }

    // Initialiser le module HTTP Client
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
    SysError_t app_result = app_manager_init();
    if (app_result != SYS_OK) {
        SERIAL_PRINTLN_MINIMAL(MSG_APP_FAIL);
        return;
    }

    // Enregistrer l'application SST
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
    SERIAL_PRINTLN_MINIMAL("Check saved WiFi...");
    kernel_log(LOG_LEVEL_INFO, "Check WiFi creds");
    if (load_wifi_credentials()) {
        SERIAL_PRINTF_MINIMAL("Found: %s\n", get_stored_ssid());
        kernel_log(LOG_LEVEL_INFO, "Found creds: %s", get_stored_ssid());
        SERIAL_PRINTLN_MINIMAL("Auto connect...");
        
        if (connect_to_wifi(get_stored_ssid(), get_stored_password()) == SYS_OK) {
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