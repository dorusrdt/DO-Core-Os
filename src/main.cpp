#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
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
#include "kernel/network/ota_manager.h"
#include "kernel/app/app_manager.h"
#include "kernel/core/minimal_config.h"
#include "kernel/hal/rtc_manager.h"
#include "kernel/hal/time_sync_manager.h"
#include "kernel/hal/heartbeat_led.h"
#include <time.h>

// Apps ESP-NOW Master/Slave
#include "apps/espnow_master/espnow_master.h"
#include "apps/espnow_slave/espnow_slave.h"

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
    const int max_attempts = 30;
    const int attempt_delay = 500;

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

// Tâche principale du système
void system_main_task(void* parameter) {
    SERIAL_PRINTLN_MINIMAL("Main task start");

    int main_counter = 0;
    while (system_running) {
        if (main_counter % 300 == 0) {
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

    const uint32_t sync_interval_ms = 900000; // 15 minutes
    const uint32_t check_interval_ms = 1000;  // Vérifier toutes les secondes
    uint32_t elapsed_ms = 0;

    while (system_running) {
        // Vérifier si une synchronisation immédiate est demandée
        bool immediate_requested = time_sync_is_immediate_requested();
        bool should_sync = (elapsed_ms >= sync_interval_ms) || immediate_requested;

        if (should_sync) {
            if (immediate_requested) {
                kernel_log(LOG_LEVEL_INFO, "Executing immediate time sync");
            }

            // Synchronisation automatique toutes les 15 minutes (ou immédiate)
            SysError_t result = time_sync_automatic();
            if (result == SYS_OK) {
                TimeSource_t source = time_sync_get_current_source();
                kernel_log(LOG_LEVEL_INFO, "Time sync OK - Source: %s",
                          (source == TIME_SOURCE_NTP) ? "NTP" :
                          (source == TIME_SOURCE_RTC) ? "RTC" : "SYSTEM");
            } else {
                kernel_log(LOG_LEVEL_WARN, "Time sync failed");
            }

            // Réinitialiser le compteur
            elapsed_ms = 0;
        }

        // Attendre 1 seconde et incrémenter le compteur
        vTaskDelay(pdMS_TO_TICKS(check_interval_ms));
        elapsed_ms += check_interval_ms;
    }

    kernel_log(LOG_LEVEL_INFO, "Time sync task stop");
    vTaskDelete(NULL);
}

// Tâche de surveillance WiFi
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

        // Détecter les changements de statut
        if (is_connected != was_connected) {
            if (is_connected) {
                kernel_log(LOG_LEVEL_INFO, "WiFi CON - IP: %s, RSSI: %d",
                          WiFi.localIP().toString().c_str(), current_rssi);

                // Mettre à jour heartbeat
                heartbeat_set_state(HEARTBEAT_READY);

                // Déclencher une synchronisation immédiate du temps
                kernel_log(LOG_LEVEL_INFO, "WiFi reconnected - triggering immediate time sync");
                time_sync_request_immediate();
            } else {
                kernel_log(LOG_LEVEL_WARN, "WiFi DIS");

                // Mettre à jour heartbeat
                heartbeat_set_state(HEARTBEAT_WIFI_ERROR);

                if (load_wifi_credentials()) {
                    kernel_log(LOG_LEVEL_INFO, "Reconnect with creds");
                    WiFi.begin(get_stored_ssid(), get_stored_password());
                } else {
                    kernel_log(LOG_LEVEL_INFO, "No creds - manual needed");
                }
            }
            was_connected = is_connected;
            reconnect_attempts = 0;
        }

        // Log périodique du statut (toutes les 30 minutes)
        if (supervision_counter % 1800 == 0) {
            if (is_connected) {
                kernel_log(LOG_LEVEL_INFO, "WiFi - Status: CON, RSSI: %d, IP: %s",
                          current_rssi, WiFi.localIP().toString().c_str());
            } else {
                kernel_log(LOG_LEVEL_WARN, "WiFi - Status: DIS");
            }
        }

        // Vérifier la qualité du signal
        if (is_connected && current_rssi < -80) {
            weak_signal_counter++;

            if (weak_signal_counter % 900 == 0) {
                kernel_log(LOG_LEVEL_WARN, "WiFi - Weak signal: %d dBm", current_rssi);
            }
        } else {
            weak_signal_counter = 0;
        }

        // Détecter les changements significatifs de RSSI
        if (is_connected && abs(current_rssi - last_rssi) > 20) {
            static int rssi_log_counter = 0;
            rssi_log_counter++;

            if (rssi_log_counter % 300 == 0) {
                kernel_log(LOG_LEVEL_INFO, "RSSI change: %d -> %d", last_rssi, current_rssi);
                rssi_log_counter = 0;
            }
            last_rssi = current_rssi;
        }

        // Tentative de reconnexion si déconnecté
        if (!is_connected && load_wifi_credentials()) {
            reconnect_attempts++;

            if (reconnect_attempts % 60 == 0) {
                kernel_log(LOG_LEVEL_INFO, "Reconnect %d", reconnect_attempts / 60);
                WiFi.disconnect();
                delay(1000);
                WiFi.begin(get_stored_ssid(), get_stored_password());
            }
        } else if (is_connected) {
            reconnect_attempts = 0;
        }

        supervision_counter++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    kernel_log(LOG_LEVEL_INFO, "WiFi task stop");
    vTaskDelete(NULL);
}

// Fonction d'affichage du logo système style neofetch
void display_system_logo() {
    Serial.println();
    Serial.println("        ██████╗ ██ ██████╗      OS: D'O-CORE v" DO_CORE_VERSION);
    Serial.println("        ██╔══██╗ ██╔═══██╗      Host: ESP32 DevKit");
    Serial.println("        ██║  ██║ ██║   ██║      Kernel: ESP-IDF");

    // Calculer l'uptime
    uint32_t uptime_seconds = system_monitor_get_uptime();
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
    String system_health = system_monitor_is_system_healthy() ? "HEALTHY" : "UNHEALTHY";
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
    Serial.printf("       v%s\n", DO_CORE_VERSION);
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    SERIAL_PRINTLN_MINIMAL("=== D'O-Core Init ===");
    SERIAL_PRINTF_MINIMAL("Ver: %s\n", DO_CORE_VERSION);

    // Afficher la mémoire initiale
    SERIAL_PRINTF_MINIMAL("Heap: %lu\n", esp_get_free_heap_size());

    // Initialiser NVS pour la persistance des données
    SERIAL_PRINTLN_MINIMAL("NVS init...");
    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
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

    // Initialiser le heartbeat LED
    SERIAL_PRINTLN_MINIMAL("Heartbeat init...");
    heartbeat_init();
    heartbeat_set_state(HEARTBEAT_BOOTING);

    // Créer la tâche heartbeat immédiatement
    uint8_t heartbeat_task_id;
    result = task_create_pinned_to_core("Heartbeat", heartbeat_task, NULL,
                                       PRIORITY_LOW, STACK_SIZE_SMALL, 0, &heartbeat_task_id);
    if (result == SYS_OK) {
        SERIAL_PRINTLN_MINIMAL("Heartbeat task started");
    } else {
        SERIAL_PRINTLN_MINIMAL("Heartbeat task fail");
    }

    // Initialiser le système de logs
    SERIAL_PRINTLN_MINIMAL("Log system init...");
    log_system_init();
    kernel_log(LOG_LEVEL_INFO, "=== D'O-Core OS Starting ===");
    kernel_log(LOG_LEVEL_INFO, "Version: %s", DO_CORE_VERSION);
    kernel_log(LOG_LEVEL_INFO, "Build: %s %s", __DATE__, __TIME__);

    // Initialiser le module OTA
    SERIAL_PRINTLN_MINIMAL("OTA Manager init...");
    kernel_log(LOG_LEVEL_INFO, "OTA Manager init");
    SysError_t ota_result = ota_manager_module_init();
    if (ota_result != SYS_OK) {
        SERIAL_PRINTLN_MINIMAL("OTA Manager fail");
        kernel_log(LOG_LEVEL_ERROR, "OTA Manager init failed");
    } else {
        SERIAL_PRINTLN_MINIMAL("OTA Manager OK");
        kernel_log(LOG_LEVEL_INFO, "OTA Manager initialized successfully");
    }

    // Initialiser le gestionnaire RTC DS3231
    SERIAL_PRINTLN_MINIMAL("RTC init...");
    SysError_t rtc_result = rtc_manager_init();
    if (rtc_result != SYS_OK) {
        SERIAL_PRINTLN_MINIMAL("RTC fail");
        kernel_log(LOG_LEVEL_ERROR, "RTC Manager initialization failed");
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

    // Initialiser le gestionnaire d'applications
    SysError_t app_result = app_manager_init();
    if (app_result != SYS_OK) {
        SERIAL_PRINTLN_MINIMAL(MSG_APP_FAIL);
        return;
    }

    // Enregistrer les applications Master et Slave (même firmware)
    {
        SysError_t r1 = espnow_master_register_app();
        SysError_t r2 = espnow_slave_register_app();
        if (r1 == SYS_OK && r2 == SYS_OK) {
            kernel_log(LOG_LEVEL_INFO, "Registered apps: espnow_master(id=%d), espnow_slave(id=%d)", ESPNOW_MASTER_APP_ID, ESPNOW_SLAVE_APP_ID);
        } else {
            kernel_log(LOG_LEVEL_ERROR, "Failed to register espnow apps (master=%d, slave=%d)", r1, r2);
        }
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
            heartbeat_set_state(HEARTBEAT_READY);
        } else {
            SERIAL_PRINTLN_MINIMAL(MSG_WIFI_FAIL);
            SERIAL_PRINTLN_MINIMAL("Use 'wifi_save' to update");
            kernel_log(LOG_LEVEL_ERROR, MSG_WIFI_FAIL);
            heartbeat_set_state(HEARTBEAT_WIFI_ERROR);
        }
    } else {
        SERIAL_PRINTLN_MINIMAL("No saved creds");
        kernel_log(LOG_LEVEL_INFO, "No saved creds");
        SERIAL_PRINTLN_MINIMAL("Use 'wifi_save' to save");
        heartbeat_set_state(HEARTBEAT_WIFI_ERROR);
    }

    SERIAL_PRINTF_MINIMAL("Heap after WiFi: %lu\n", esp_get_free_heap_size());

    // Initialiser l'interface
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
        TimeSource_t source = time_sync_get_current_source();
        const char* source_name = (source == TIME_SOURCE_NTP) ? "NTP" :
                                  (source == TIME_SOURCE_RTC) ? "RTC" : "SYSTEM";

        SERIAL_PRINTF_MINIMAL("Time sync OK - Source: %s\n", source_name);
        kernel_log(LOG_LEVEL_INFO, "Initial time synchronization successful - Source: %s", source_name);

        // Afficher l'heure actuelle
        kernel_log(LOG_LEVEL_INFO, "Current time: %s", time_sync_format_current_time().c_str());

        // Vérifier les conditions temporelles (si NTP disponible)
        if (source == TIME_SOURCE_NTP) {
            if (ntp_is_business_hours()) {
                kernel_log(LOG_LEVEL_INFO, "Business mode");
            } else if (ntp_is_night_time()) {
                kernel_log(LOG_LEVEL_INFO, "Night mode");
            }
        }
    } else {
        SERIAL_PRINTLN_MINIMAL("Initial sync failed");
        kernel_log(LOG_LEVEL_WARN, "Initial time synchronization failed - No valid time source");
    }

    // Activer le système
    system_initialized = true;
    system_running = true;

    // Créer les tâches essentielles
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

    // Afficher les apps enregistrées
    Serial.println();
    Serial.println("📋 Registered Apps:");
    Serial.println("  ID 10: espnow_master");
    Serial.println("  ID 11: espnow_slave");
    Serial.println();
    Serial.println("🎯 Available Commands:");
    Serial.println("  help                - Show all commands");
    Serial.println("  app_list            - List registered apps");
    Serial.println("  wifi_save <ssid> <pwd> - Save WiFi credentials");
    Serial.println();

    heartbeat_set_state(HEARTBEAT_READY);

    // Démarrer le shell
    interface_start();
}

void loop() {
    // Boucle principale du système
    app_manager_loop();

    // Délai pour éviter de surcharger le CPU
    delay(10);
}
