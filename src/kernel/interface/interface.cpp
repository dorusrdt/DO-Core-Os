#include "interface.h"
#include <WiFi.h>
#include <Preferences.h>
#include "../core/task_manager.h"
#include "../core/memory_manager.h"
#include "../core/system_monitor.h"
#include "../network/wifi_manager.h"
#include "../network/ntp_manager.h"
#include "../network/http_client.h"
#include "../network/ota_manager.h"
#include "../app/app_manager.h"
#include "../hal/rtc_manager.h"
#include "../hal/time_sync_manager.h"
#include <string.h>


// Inclure les commandes HTTP
extern SysError_t cmd_http_config(int argc, char* argv[]);
extern SysError_t cmd_http_get(int argc, char* argv[]);
extern SysError_t cmd_http_post(int argc, char* argv[]);
extern SysError_t cmd_http_delete(int argc, char* argv[]);
extern SysError_t cmd_http_patch(int argc, char* argv[]);
extern SysError_t cmd_http_head(int argc, char* argv[]);
extern SysError_t cmd_http_options(int argc, char* argv[]);
extern SysError_t cmd_http_test(int argc, char* argv[]);
extern SysError_t cmd_http_stats(int argc, char* argv[]);
extern SysError_t cmd_http_debug(int argc, char* argv[]);

// Inclure les commandes OTA
extern SysError_t cmd_ota_start(int argc, char* argv[]);
extern SysError_t cmd_ota_stop(int argc, char* argv[]);
extern SysError_t cmd_ota_status(int argc, char* argv[]);
extern SysError_t cmd_ota_info(int argc, char* argv[]);
extern SysError_t cmd_ota_stats(int argc, char* argv[]);
extern SysError_t cmd_ota_url(int argc, char* argv[]);

// Commandes ESP-NOW supprimées - structure de base uniquement

// Commandes Irrigation : incluses via irrig_cli_commands.h

// Déclarations des fonctions de commandes
SysError_t cmd_network_test(int argc, char* argv[]);

// Variables globales
static Command_t commands[MAX_COMMANDS];
static uint8_t command_count = 0;
static bool shell_running = false;
static char input_buffer[MAX_COMMAND_LENGTH];

// Fonction pour ajouter une commande
static void add_command(const char* name, const char* description, SysError_t (*handler)(int argc, char* argv[])) {
    if (command_count < MAX_COMMANDS) {
        strcpy(commands[command_count].name, name);
        strcpy(commands[command_count].description, description);
        commands[command_count].handler = handler;
        command_count++;
    } else {
        Serial.printf("ERROR: Cannot add command '%s' - MAX_COMMANDS (%d) exceeded!\n", name, MAX_COMMANDS);
    }
}

// Fonction pour parser une ligne de commande
static SysError_t parse_command(const char* line, char* argv[], int* argc) {
    *argc = 0;
    char* token = strtok((char*)line, " \t\n\r");

    while (token != NULL && *argc < MAX_PARAMETERS) {
        argv[*argc] = token;
        (*argc)++;
        token = strtok(NULL, " \t\n\r");
    }

    return SYS_OK;
}

// Fonction pour exécuter une commande
static SysError_t execute_command(const char* command_line) {
    char* argv[MAX_PARAMETERS];
    int argc;

    if (parse_command(command_line, argv, &argc) != SYS_OK) {
        return SYS_ERROR;
    }

    if (argc == 0) {
        return SYS_OK; // Ligne vide
    }

    // Chercher la commande
    for (int i = 0; i < command_count; i++) {
        if (strcmp(commands[i].name, argv[0]) == 0) {
            return commands[i].handler(argc, argv);
        }
    }

    Serial.printf("Unknown command: %s\n", argv[0]);
    Serial.println("Type 'help' for available commands");
    return SYS_INVALID_PARAM;
}

// Tâche du shell
static void shell_task(void* parameter) {
    Serial.println("=== D'O-Core Shell Started ===");
    Serial.println("Type 'help' for available commands");
    Serial.println();

    int buffer_index = 0;
    bool need_prompt = true;

    while (shell_running) {
        // Afficher le prompt seulement si nécessaire
        if (need_prompt) {
            Serial.print("D'O-Core> ");
            need_prompt = false;
        }

        // Lire la ligne de commande
        buffer_index = 0;
        memset(input_buffer, 0, sizeof(input_buffer));

        while (buffer_index < MAX_COMMAND_LENGTH - 1) {
            if (Serial.available()) {
                char c = Serial.read();

                if (c == '\r' || c == '\n') {
                    // Ne pas afficher de nouvelle ligne ici
                    break;
                } else if (c == '\b' || c == 127) {
                    // Backspace
                    if (buffer_index > 0) {
                        buffer_index--;
                        Serial.print("\b \b");
                    }
                } else if (c >= 32 && c <= 126) {
                    // Caractère imprimable
                    input_buffer[buffer_index++] = c;
                    Serial.print(c);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        // Exécuter la commande
        if (buffer_index > 0) {
            Serial.println(); // Nouvelle ligne seulement après la commande
            execute_command(input_buffer);
            need_prompt = true; // Afficher le prompt au prochain tour
        }
    }

    vTaskDelete(NULL);
}

// Initialisation de l'interface
SysError_t interface_init(void) {
    Serial.println("Initializing Interface...");

    // Ajouter les commandes
    add_command("help", "Show available commands", cmd_help);
    add_command("status", "Show system status", cmd_status);
    add_command("tasks", "Show task information", cmd_tasks);
    add_command("memory", "Show memory information", cmd_memory);
    add_command("dmesg", "Show system logs", cmd_dmesg);
    add_command("logs", "Show filtered logs", cmd_logs);
    add_command("log_echo", "Enable/disable log echo", cmd_log_echo);
    add_command("test_log", "Test log system", cmd_test_log);
    add_command("debug_log", "Debug log system", cmd_debug_log);
    add_command("validate_log", "Validate log system integrity", cmd_validate_log);
    add_command("neofetch", "Display system information and logo", cmd_neofetch);
    add_command("clear", "Clear screen", cmd_clear);
    add_command("uptime", "Show system uptime", cmd_uptime);
    add_command("version", "Show system version", cmd_version);

    // Commandes réseau
    add_command("wifi", "WiFi configuration and status", cmd_wifi);
    add_command("wifi_enable", "Enable WiFi with minimal configuration", cmd_wifi_enable);
    add_command("network", "Network overview and status", cmd_network);
    add_command("wifi_scan", "Scan for available WiFi networks", cmd_wifi_scan);
    add_command("wifi_reconnect", "Force WiFi reconnection", cmd_wifi_reconnect);
    add_command("wifi_stats", "Show detailed WiFi statistics", cmd_wifi_stats);
    add_command("wifi_save", "Save WiFi credentials for auto-connect", cmd_wifi_save);
    add_command("wifi_clear", "Clear saved WiFi credentials", cmd_wifi_clear);
    add_command("wifi_auto", "Connect using saved credentials", cmd_wifi_auto);

    // Commandes NTP minimalistes
    add_command("ntp_status", "Show NTP synchronization status", cmd_ntp_status);
    add_command("ntp_sync", "Force manual NTP synchronization", cmd_ntp_sync);
    add_command("ntp_test", "Test NTP connectivity and diagnose issues", cmd_ntp_test);
    add_command("ntp_timezone", "Configure timezone settings", cmd_ntp_timezone);
    add_command("ntp_utilities", "Show time utilities and business hours", cmd_ntp_utilities);

    // Network functionality removed

    // Commandes Application Manager
    add_command("app_status", "Show application manager status", cmd_app_status);
    add_command("app_list", "List all registered applications", cmd_app_list);
    add_command("app_start", "Start an application", cmd_app_start);
    add_command("app_stop", "Stop an application", cmd_app_stop);
    add_command("app_restart", "Restart an application", cmd_app_restart);
    add_command("app_pause", "Pause an application", cmd_app_pause);
    add_command("app_resume", "Resume an application", cmd_app_resume);
    add_command("app_info", "Show detailed application information", cmd_app_info);
    add_command("app_start_all", "Start all registered applications", cmd_app_start_all);
    add_command("app_stop_all", "Stop all running applications", cmd_app_stop_all);
    add_command("app_pause_all", "Pause all running applications", cmd_app_pause_all);
    add_command("app_resume_all", "Resume all paused applications", cmd_app_resume_all);


    // Commandes HTTP Client
    add_command("http_config", "Configure HTTP client settings", cmd_http_config);
    add_command("http_get", "Send HTTP GET request", cmd_http_get);
    add_command("http_post", "Send HTTP POST request", cmd_http_post);
    add_command("http_put", "Send HTTP PUT request", cmd_http_put);
    add_command("http_delete", "Send HTTP DELETE request", cmd_http_delete);
    add_command("http_patch", "Send HTTP PATCH request", cmd_http_patch);
    add_command("http_head", "Send HTTP HEAD request", cmd_http_head);
    add_command("http_options", "Send HTTP OPTIONS request", cmd_http_options);
    add_command("http_test", "Run HTTP client tests", cmd_http_test);
    add_command("http_stats", "Show HTTP client statistics", cmd_http_stats);
    add_command("http_debug", "Debug HTTP client", cmd_http_debug);

    add_command("network_test", "Test network connectivity and DNS resolution", cmd_network_test);

    // Commandes RTC et Time Sync simples
    add_command("rtc_status", "Show RTC status and time", cmd_rtc_status);
    add_command("rtc_recovery", "Attempt RTC recovery", cmd_rtc_recovery);
    add_command("rtc_temp", "Show RTC temperature", cmd_rtc_temp);
    add_command("rtc_battery", "Show RTC battery status", cmd_rtc_battery);
    add_command("time_status", "Show current time and source", cmd_time_status);
    add_command("time_source", "Show current time source", cmd_time_source);
    add_command("time_sync", "Force time synchronization", cmd_time_sync);
    add_command("time_sources", "Show detailed time sources info", cmd_time_sources);

    // Commandes ESP-NOW supprimées - structure de base uniquement

    // Commandes OTA
    add_command("ota_start", "Start OTA web server", cmd_ota_start);
    add_command("ota_stop", "Stop OTA web server", cmd_ota_stop);
    add_command("ota_status", "Show OTA server status", cmd_ota_status);
    add_command("ota_info", "Show firmware version info", cmd_ota_info);
    add_command("ota_stats", "Show OTA statistics", cmd_ota_stats);
    add_command("ota_url", "Show OTA update URL", cmd_ota_url);

    Serial.printf("Interface initialized with %d/%d commands\n", command_count, MAX_COMMANDS);

    if (command_count >= MAX_COMMANDS) {
        Serial.println("WARNING: Command limit reached! Some commands may not be available.");
    }

    return SYS_OK;
}

// Démarrer le shell
void interface_start(void) {
    shell_running = true;

    // Créer la tâche du shell avec stack spécifique pour les commandes WiFi
    BaseType_t result = xTaskCreate(shell_task, "Shell", STACK_SIZE_SHELL, NULL, PRIORITY_NORMAL, NULL);

    if (result == pdPASS) {
        Serial.println("Shell task created successfully");
    } else {
        Serial.println("ERROR: Failed to create shell task!");
    }
}

// Commandes système
SysError_t cmd_help(int argc, char* argv[]) {
    Serial.println("Available commands:");
    Serial.println("==================");

    for (int i = 0; i < command_count; i++) {
        Serial.printf("  %-16s - %s\n", commands[i].name, commands[i].description);
    }

    Serial.println();
    return SYS_OK;
}

SysError_t cmd_status(int argc, char* argv[]) {
    Serial.println("=== System Status ===");
    Serial.printf("Version: %s\n", DO_CORE_VERSION);
    Serial.printf("Uptime: %lu seconds\n", system_monitor_get_uptime());
    Serial.printf("Active Tasks: %d\n", task_get_count());
    Serial.printf("Free Heap: %lu bytes\n", esp_get_free_heap_size());
    Serial.printf("System Health: %s\n", system_monitor_is_system_healthy() ? "HEALTHY" : "UNHEALTHY");
    Serial.println("====================");

    return SYS_OK;
}

SysError_t cmd_tasks(int argc, char* argv[]) {
    Serial.println("=== Task Information ===");
    task_manager_print_stats();
    task_manager_print_all_tasks();
    Serial.println("=======================");

    return SYS_OK;
}

SysError_t cmd_memory(int argc, char* argv[]) {
    Serial.println("=== Memory Information ===");
    memory_print_stats();
    memory_print_pool_stats();
    Serial.println("=========================");

    return SYS_OK;
}

SysError_t cmd_dmesg(int argc, char* argv[]) {
    Serial.println("=== D'O-Core System Logs (dmesg) ===");

    // Validation des paramètres
    uint32_t max_display = 5; // Valeur par défaut sécurisée

    if (argc == 2) {
        int requested = atoi(argv[1]);
        if (requested > 0 && requested <= 20) { // Limite de sécurité
            max_display = (uint32_t)requested;
        } else {
            Serial.println("ERROR: Invalid count. Use 1-20 messages max.");
            Serial.println("Usage: dmesg [count]");
            return SYS_INVALID_PARAM;
        }
    } else if (argc > 2) {
        Serial.println("ERROR: Too many parameters");
        Serial.println("Usage: dmesg [count]");
        return SYS_INVALID_PARAM;
    }

    uint32_t total_msgs = log_system_get_total_messages();
    uint32_t buffer_count = log_system_get_count();

    Serial.printf("Total messages: %lu\n", total_msgs);
    Serial.printf("Buffer count: %lu\n", buffer_count);
    Serial.printf("Log echo: %s\n", log_system_is_echo_enabled() ? "ON" : "OFF");

    if (buffer_count == 0) {
        Serial.println("No D'O-Core logs in buffer yet.");
        Serial.println("Use 'test_log' to add test messages.");
        Serial.println("Use 'log_echo on' to see logs in real-time.");
    } else {
        // Allocation dynamique sécurisée
        LogMessage_t* messages = (LogMessage_t*)malloc(max_display * sizeof(LogMessage_t));
        if (!messages) {
            Serial.println("ERROR: Memory allocation failed for log display");
            Serial.println("Try with fewer messages: dmesg 3");
            return SYS_ERROR;
        }

        // Obtenir les derniers messages
        uint32_t to_display = (buffer_count > max_display) ? max_display : buffer_count;

        // Obtenir tous les messages disponibles pour pouvoir sélectionner les derniers
        LogMessage_t* all_messages = (LogMessage_t*)malloc(buffer_count * sizeof(LogMessage_t));
        if (!all_messages) {
            Serial.println("ERROR: Memory allocation failed for log display");
            free(messages);
            return SYS_ERROR;
        }

        // Afficher l'information sur les messages restants en haut
        if (buffer_count > max_display) {
            Serial.printf("... and %lu more messages (use 'dmesg %lu' for more)\n",
                         buffer_count - max_display, max_display + 5);
        }

        // Afficher les logs de manière sécurisée
        Serial.printf("=== Recent Log Messages (showing %lu) ===\n", max_display);

        uint32_t actual_count;
        SysError_t result = log_system_get_messages(all_messages, buffer_count, &actual_count);

        if (result == SYS_OK && actual_count > 0) {
            // Calculer l'index de départ pour les derniers messages
            uint32_t start_index = (actual_count > to_display) ? (actual_count - to_display) : 0;

            // Copier les derniers messages dans le buffer d'affichage
            for (uint32_t i = 0; i < to_display && (start_index + i) < actual_count; i++) {
                messages[i] = all_messages[start_index + i];
            }

            // Afficher les messages (du plus ancien au plus récent parmi les sélectionnés)
            for (uint32_t i = 0; i < to_display; i++) {
                const LogMessage_t* msg = &messages[i];
                if (msg != NULL) { // Protection supplémentaire
                    const char* level_str = log_level_to_string(msg->level);
                    Serial.printf("[%lu] %s: %s: %s\n",
                                 msg->timestamp, level_str, msg->source, msg->message);
                }
            }
        } else {
            Serial.printf("ERROR: Failed to retrieve log messages (error: %d)\n", result);
        }

        // Libération de la mémoire
        free(all_messages);
        free(messages);

    }

    Serial.println("================================");
    return SYS_OK;
}

SysError_t cmd_logs(int argc, char* argv[]) {
    if (argc == 1) {
        // Afficher le statut général
        Serial.println("=== Log System Status ===");
        Serial.printf("Buffer count: %lu\n", log_system_get_count());
        Serial.printf("Total messages: %lu\n", log_system_get_total_messages());
        Serial.printf("Log echo: %s\n", log_system_is_echo_enabled() ? "ON" : "OFF");
        Serial.println("Usage: logs [info|error|warn|debug|source|daycounter]");
        Serial.println("=========================");
        return SYS_OK;
    } else if (argc == 2) {
        const char* param = argv[1];

        // Afficher les logs filtrés de manière sécurisée
        Serial.printf("=== Filtered Logs: %s ===\n", param);

        uint32_t buffer_count = log_system_get_count();

        if (buffer_count == 0) {
            Serial.println("No logs available in buffer");
            Serial.println("=========================");
            return SYS_OK;
        }

        // Allocation dynamique sécurisée pour tous les messages
        LogMessage_t* all_messages = (LogMessage_t*)malloc(buffer_count * sizeof(LogMessage_t));
        if (!all_messages) {
            Serial.println("ERROR: Memory allocation failed for log filtering");
            return SYS_ERROR;
        }

        uint32_t actual_count;
        SysError_t result = log_system_get_messages(all_messages, buffer_count, &actual_count);

        if (result == SYS_OK && actual_count > 0) {
            uint32_t filtered_count = 0;

            // Filtrer selon le paramètre
            for (uint32_t i = 0; i < actual_count; i++) {
                const LogMessage_t* msg = &all_messages[i];
                bool show_message = false;

                // Filtrer selon le paramètre
                if (strcmp(param, "info") == 0 && msg->level == LOG_LEVEL_INFO) {
                    show_message = true;
                } else if (strcmp(param, "error") == 0 && msg->level == LOG_LEVEL_ERROR) {
                    show_message = true;
                } else if (strcmp(param, "warn") == 0 && msg->level == LOG_LEVEL_WARN) {
                    show_message = true;
                } else if (strcmp(param, "debug") == 0 && msg->level == LOG_LEVEL_DEBUG) {
                    show_message = true;
                } else if (strcmp(param, "critical") == 0 && msg->level == LOG_LEVEL_CRITICAL) {
                    show_message = true;
                } else if (strcmp(param, "daycounter") == 0 && strstr(msg->message, "DayCounter") != NULL) {
                    show_message = true;
                } else if (strcmp(msg->source, param) == 0) {
                    show_message = true;
                }

                if (show_message) {
                    const char* level_str = log_level_to_string(msg->level);
                    Serial.printf("[%lu] %s: %s: %s\n",
                                 msg->timestamp, level_str, msg->source, msg->message);
                    filtered_count++;
                }
            }

            if (filtered_count == 0) {
                Serial.printf("No logs found matching '%s'\n", param);
            } else {
                Serial.printf("Found %lu matching logs\n", filtered_count);
            }
        } else {
            Serial.printf("ERROR: Failed to retrieve log messages (error: %d)\n", result);
        }

        // Libération de la mémoire
        free(all_messages);

        Serial.println("=========================");
        return SYS_OK;
    }

    Serial.println("Usage: logs [info|error|warn|debug|critical|source|daycounter]");
    return SYS_INVALID_PARAM;
}

SysError_t cmd_log_echo(int argc, char* argv[]) {
    if (argc == 1) {
        // Afficher l'état actuel
        bool echo_enabled = log_system_is_echo_enabled();
        Serial.printf("Log echo is currently %s\n", echo_enabled ? "ENABLED" : "DISABLED");
        return SYS_OK;
    } else if (argc == 2) {
        const char* param = argv[1];

        if (strcmp(param, "on") == 0 || strcmp(param, "1") == 0 || strcmp(param, "true") == 0) {
            log_system_enable_echo(true);
            Serial.println("Log echo ENABLED - logs will be displayed in real-time");
        } else if (strcmp(param, "off") == 0 || strcmp(param, "0") == 0 || strcmp(param, "false") == 0) {
            log_system_enable_echo(false);
            Serial.println("Log echo DISABLED - logs will only be stored in buffer");
        } else {
            Serial.println("Usage: log_echo [on|off]");
            Serial.println("  on  - Enable real-time log display");
            Serial.println("  off - Disable real-time log display (default)");
            return SYS_INVALID_PARAM;
        }

        return SYS_OK;
    } else {
        Serial.println("Usage: log_echo [on|off]");
        return SYS_INVALID_PARAM;
    }
}

SysError_t cmd_clear(int argc, char* argv[]) {
    for (int i = 0; i < 50; i++) {
        Serial.println();
    }
    return SYS_OK;
}

SysError_t cmd_uptime(int argc, char* argv[]) {
    Serial.printf("System uptime: %lu seconds\n", system_monitor_get_uptime());
    return SYS_OK;
}

SysError_t cmd_version(int argc, char* argv[]) {
    Serial.printf("D'O-Core Version: %s\n", DO_CORE_VERSION);
    Serial.printf("Name: %s\n", DO_CORE_NAME);
    Serial.printf("Description: %s\n", DO_CORE_DESCRIPTION);
    return SYS_OK;
}

SysError_t cmd_test_log(int argc, char* argv[]) {
    Serial.println("Testing log system...");

    // Tester l'ajout de messages
    kernel_log(LOG_LEVEL_INFO, "Test info message from shell");
    kernel_log(LOG_LEVEL_WARN, "Test warning message from shell");
    kernel_log(LOG_LEVEL_ERROR, "Test error message from shell");

    Serial.printf("Added test messages. Buffer count: %lu\n", log_system_get_count());
    Serial.printf("Total messages: %lu\n", log_system_get_total_messages());

    // Version simplifiée - pas d'accès direct au buffer
    Serial.println("=== Test Logs ===");
    Serial.println("Log messages have been added to buffer.");
    Serial.println("Use 'log_echo on' to see logs in real-time.");
    Serial.println("Use 'dmesg' to see buffer status.");

    Serial.println("=== End Test ===");
    return SYS_OK;
}

SysError_t cmd_debug_log(int argc, char* argv[]) {
    Serial.println("=== Log System Debug ===");

    // Test 1: Vérifier l'état du système
    Serial.println("1. Testing log system state...");
    uint32_t count = log_system_get_count();
    uint32_t total = log_system_get_total_messages();
    bool echo = log_system_is_echo_enabled();

    Serial.printf("   Buffer count: %lu\n", count);
    Serial.printf("   Total messages: %lu\n", total);
    Serial.printf("   Echo enabled: %s\n", echo ? "YES" : "NO");

    // Test 2: Ajouter un message de test
    Serial.println("2. Adding test message...");
    kernel_log(LOG_LEVEL_INFO, "Debug test message");

    // Test 3: Vérifier après ajout
    Serial.println("3. Checking after add...");
    uint32_t new_count = log_system_get_count();
    uint32_t new_total = log_system_get_total_messages();

    Serial.printf("   New buffer count: %lu\n", new_count);
    Serial.printf("   New total messages: %lu\n", new_total);

    if (new_count > count) {
        Serial.println("   ✓ Message added successfully");
    } else {
        Serial.println("   ✗ Message not added");
    }

    Serial.println("=== Debug Complete ===");
    return SYS_OK;
}

SysError_t cmd_validate_log(int argc, char* argv[]) {
    Serial.println("=== Log System Validation ===");

    // Test 1: Validation du système
    Serial.println("1. Validating log system integrity...");
    SysError_t result = log_system_validate();

    if (result == SYS_OK) {
        Serial.println("   ✓ Log system integrity: OK");
    } else {
        Serial.printf("   ✗ Log system integrity: FAILED (error: %d)\n", result);
    }

    // Test 2: Test d'ajout de message
    Serial.println("2. Testing message addition...");
    SysError_t add_result = kernel_log(LOG_LEVEL_INFO, "Validation test message");

    if (add_result == SYS_OK) {
        Serial.println("   ✓ Message addition: OK");
    } else {
        Serial.printf("   ✗ Message addition: FAILED (error: %d)\n", add_result);
    }

    // Test 3: Test de récupération
    Serial.println("3. Testing message retrieval...");
    uint32_t count = log_system_get_count();
    uint32_t total = log_system_get_total_messages();

    Serial.printf("   Buffer count: %lu\n", count);
    Serial.printf("   Total messages: %lu\n", total);

    if (count > 0) {
        Serial.println("   ✓ Message retrieval: OK");
    } else {
        Serial.println("   ✗ Message retrieval: FAILED");
    }

    // Test 4: Test de stress (ajout multiple)
    Serial.println("4. Stress test (adding multiple messages)...");
    int success_count = 0;
    for (int i = 0; i < 5; i++) {
        if (kernel_log(LOG_LEVEL_DEBUG, "Stress test message %d", i) == SYS_OK) {
            success_count++;
        }
    }

    Serial.printf("   Successfully added %d/5 messages\n", success_count);

    if (success_count == 5) {
        Serial.println("   ✓ Stress test: OK");
    } else {
        Serial.printf("   ✗ Stress test: FAILED (%d/5)\n", success_count);
    }

    Serial.println("=== Validation Complete ===");
    return SYS_OK;
}

// Commandes réseau
SysError_t cmd_wifi(int argc, char* argv[]) {
    if (argc == 1) {
        // Afficher le statut WiFi
        Serial.println("=== WiFi Status ===");
        WifiStatus_t status = wifi_manager_get_status();

        Serial.printf("Status: ");
        switch (status) {
            case WIFI_STATUS_DISCONNECTED:
                Serial.println("DISCONNECTED");
                break;
            case WIFI_STATUS_CONNECTING:
                Serial.println("CONNECTING");
                break;
            case WIFI_STATUS_CONNECTED:
                Serial.println("CONNECTED");
                // Vérifications de sécurité avant d'accéder à WiFi
                if (WiFi.status() == WL_CONNECTED) {
                    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
                    Serial.printf("Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
                    Serial.printf("Subnet Mask: %s\n", WiFi.subnetMask().toString().c_str());
                    Serial.printf("DNS: %s\n", WiFi.dnsIP().toString().c_str());
                    Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
                    Serial.printf("SSID: %s\n", WiFi.SSID().c_str());
                    Serial.printf("BSSID: %s\n", WiFi.BSSIDstr().c_str());
                    Serial.printf("Channel: %d\n", WiFi.channel());
                } else {
                    Serial.println("WiFi object not available");
                }
                break;
            case WIFI_STATUS_FAILED:
                Serial.println("FAILED");
                break;
            default:
                Serial.println("UNKNOWN");
                break;
        }

        Serial.println("Usage: wifi [connect|disconnect|scan|config]");
        Serial.println("==================");
        return SYS_OK;
    }

    if (argc >= 2) {
        const char* action = argv[1];

        if (strcmp(action, "connect") == 0) {
            if (argc >= 4) {
                // wifi connect <ssid> <password>
                Serial.printf("Connecting to WiFi network: %s\n", argv[2]);

                // Utiliser la fonction connect_to_wifi qui gère la sauvegarde automatique
                extern SysError_t connect_to_wifi(const char* ssid, const char* password);
                SysError_t result = connect_to_wifi(argv[2], argv[3]);

                if (result == SYS_OK) {
                    Serial.println("✅ WiFi connection successful!");
                    Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
                    Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
                    Serial.println("Credentials automatically saved for next boot");

                    // Synchronisation NTP automatique après connexion WiFi
                    Serial.println("🕐 Synchronizing time with NTP...");
                    extern NtpStatus_t ntp_sync(void);
                    extern bool ntp_is_synced(void);
                    extern time_t ntp_get_time(void);
                    extern String ntp_format_time(time_t timestamp, const char* format);

                    NtpStatus_t ntp_result = NTP_STATUS_FAILED;
                    const int max_retries = 3;
                    const int timeout_ms = 30000; // 30 secondes

                    for (int retry = 1; retry <= max_retries; retry++) {
                        Serial.printf("NTP sync attempt %d/%d...\n", retry, max_retries);

                        unsigned long start_time = millis();
                        ntp_result = ntp_sync();

                        // Attendre la synchronisation avec timeout
                        while (!ntp_is_synced() && (millis() - start_time) < timeout_ms) {
                            delay(500);
                            Serial.print(".");
                        }
                        Serial.println();

                        if (ntp_is_synced()) {
                            time_t current_time = ntp_get_time();
                            Serial.printf("✅ NTP sync successful! Time: %s\n",
                                        ntp_format_time(current_time, "%Y-%m-%d %H:%M:%S").c_str());
                            break;
                        } else {
                            Serial.printf("❌ NTP sync attempt %d failed\n", retry);
                            if (retry < max_retries) {
                                Serial.println("Retrying in 2 seconds...");
                                delay(2000);
                            }
                        }
                    }

                    if (!ntp_is_synced()) {
                        Serial.println("⚠️ NTP sync failed after all retries");
                        Serial.println("System will continue with RTC time");
                    }
                } else {
                    Serial.println("❌ WiFi connection failed");
                    Serial.println("Check SSID and password");
                }
            } else {
                Serial.println("Usage: wifi connect <ssid> <password>");
                return SYS_INVALID_PARAM;
            }
        } else if (strcmp(action, "disconnect") == 0) {
            Serial.println("Disconnecting from WiFi...");
            wifi_manager_disconnect();
            Serial.println("WiFi disconnected");
        } else if (strcmp(action, "scan") == 0) {
            Serial.println("=== WiFi Scan ===");
            Serial.println("Scanning for available networks...");

            // Vérification de sécurité avant le scan
            wifi_mode_t current_mode = WiFi.getMode();
            if (current_mode == WIFI_MODE_NULL) {
                Serial.println("ERROR: WiFi scan not available in NULL mode");
                Serial.println("Use 'wifi config' to see current mode");
                Serial.println("Use 'wifi connect <ssid> <password>' to enable STA mode");
                Serial.println("==================");
                return SYS_ERROR;
            }

            // Vérifier si le WiFi est prêt pour le scan
            if (current_mode != WIFI_MODE_STA && current_mode != WIFI_MODE_APSTA) {
                Serial.println("ERROR: WiFi scan requires STA or APSTA mode");
                Serial.printf("Current mode: %s\n",
                    (current_mode == WIFI_MODE_AP) ? "AP" : "NULL");
                Serial.println("==================");
                return SYS_ERROR;
            }

            int n = WiFi.scanNetworks();
            if (n == 0) {
                Serial.println("No networks found");
            } else {
                Serial.printf("%d networks found:\n", n);
                Serial.println("SSID\t\t\tRSSI\tEncryption");
                Serial.println("----------------------------------------");

                for (int i = 0; i < n; ++i) {
                    Serial.printf("%-20s\t%d\t%s\n",
                        WiFi.SSID(i).c_str(),
                        WiFi.RSSI(i),
                        (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "Secured");
                }
            }

            Serial.println("==================");
        } else if (strcmp(action, "config") == 0) {
            Serial.println("=== WiFi Configuration ===");
            Serial.println("Current configuration:");
            // Vérification de sécurité
            wifi_mode_t mode = WiFi.getMode();
            Serial.printf("Mode: %s\n",
                (mode == WIFI_MODE_STA) ? "Station" :
                (mode == WIFI_MODE_AP) ? "Access Point" :
                (mode == WIFI_MODE_APSTA) ? "AP+Station" : "NULL");
            Serial.println("========================");
        } else {
            Serial.println("Usage: wifi [connect|disconnect|scan|config]");
            return SYS_INVALID_PARAM;
        }

        return SYS_OK;
    }

    Serial.println("Usage: wifi [connect|disconnect|scan|config]");
    return SYS_INVALID_PARAM;
}

SysError_t cmd_network(int argc, char* argv[]) {
    Serial.println("=== Network Overview ===");

    // Statut WiFi
    Serial.println("WiFi Status:");
    WifiStatus_t wifi_status = wifi_manager_get_status();
    Serial.printf("  Status: %s\n",
        (wifi_status == WIFI_STATUS_CONNECTED) ? "CONNECTED" :
        (wifi_status == WIFI_STATUS_DISCONNECTED) ? "DISCONNECTED" :
        (wifi_status == WIFI_STATUS_CONNECTING) ? "CONNECTING" : "FAILED");

    if (wifi_status == WIFI_STATUS_CONNECTED) {
        // Vérifications de sécurité avant d'accéder à WiFi
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("  IP Address: %s\n", WiFi.localIP().toString().c_str());
            Serial.printf("  Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
            Serial.printf("  Subnet: %s\n", WiFi.subnetMask().toString().c_str());
            Serial.printf("  DNS: %s\n", WiFi.dnsIP().toString().c_str());
            Serial.printf("  RSSI: %d dBm\n", WiFi.RSSI());
            Serial.printf("  SSID: %s\n", WiFi.SSID().c_str());
            Serial.printf("  Channel: %d\n", WiFi.channel());
        } else {
            Serial.println("  WiFi object not available");
        }
    }

    // Test de connectivité
    Serial.println("\nConnectivity Test:");
    if (wifi_status == WIFI_STATUS_CONNECTED) {
        Serial.println("  Internet: Available (WiFi connected)");
        Serial.println("  Local Network: Available");
    } else {
        Serial.println("  Internet: Unavailable (WiFi disconnected)");
        Serial.println("  Local Network: Unavailable");
    }

    // Services réseau
    Serial.println("\nNetwork Services:");
    Serial.println("  NTP: Available (minimal implementation)");
    // Network functionality removed

    Serial.println("=====================");
    return SYS_OK;
}

// Commande pour activer le WiFi avec configuration minimaliste
SysError_t cmd_wifi_enable(int argc, char* argv[]) {
    Serial.println("=== WiFi Enable (Minimalist) ===");

    // Vérifier la mémoire disponible
    uint32_t free_heap_before = esp_get_free_heap_size();
    Serial.printf("Free heap before enabling WiFi: %lu bytes\n", free_heap_before);

    if (free_heap_before < 20000) { // 20KB minimum
        Serial.println("ERROR: Insufficient memory to enable WiFi");
        Serial.println("Need at least 20KB free heap");
        return SYS_ERROR;
    }

    if (argc >= 3) {
        // wifi_enable <ssid> <password>
        Serial.printf("Enabling WiFi with SSID: %s\n", argv[1]);

        // APPROCHE PROPRE: Réinitialiser le WiFi Manager
        Serial.println("Reinitializing WiFi Manager...");

        // Configuration WiFi en mode STA
        WifiConfig_t sta_config = {
            .ssid = String(argv[1]),
            .password = String(argv[2]),
            .mode = WIFI_MODE_STA
        };

        // Réinitialiser complètement le WiFi Manager
        wifi_manager_init(&sta_config);
        Serial.println("WiFi Manager reinitialized in STA mode");

        // Tenter la connexion via le WiFi Manager
        Serial.println("Attempting to connect via WiFi Manager...");
        WifiStatus_t result = wifi_manager_connect();

        // Attendre et vérifier le statut
        int attempts = 0;
        const int max_attempts = 40;  // 20 secondes

        while (wifi_manager_get_status() != WIFI_STATUS_CONNECTED && attempts < max_attempts) {
            delay(500);
            Serial.print(".");
            attempts++;

            // Afficher le statut périodiquement
            if (attempts % 10 == 0) {
                WifiStatus_t current_status = wifi_manager_get_status();
                Serial.printf("\nStatus: %d", current_status);
            }
        }

        Serial.println();  // Nouvelle ligne après les points

        // Vérifier le résultat final
        WifiStatus_t final_status = wifi_manager_get_status();

        if (final_status == WIFI_STATUS_CONNECTED) {
            Serial.println("WiFi connected successfully!");
            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
                Serial.printf("Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
                Serial.printf("Subnet: %s\n", WiFi.subnetMask().toString().c_str());
                Serial.printf("DNS: %s\n", WiFi.dnsIP().toString().c_str());
                Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
                Serial.printf("SSID: %s\n", WiFi.SSID().c_str());
                Serial.printf("Channel: %d\n", WiFi.channel());
            }
        } else {
            Serial.printf("WiFi connection failed after %d attempts\n", max_attempts);
            Serial.printf("Final WiFi Manager status: %d\n", final_status);
            Serial.printf("Final WiFi.status(): %d\n", WiFi.status());

            // Afficher les codes d'erreur WiFi
            switch (WiFi.status()) {
                case WL_NO_SSID_AVAIL:
                    Serial.println("Error: SSID not found");
                    break;
                case WL_CONNECT_FAILED:
                    Serial.println("Error: Connection failed (wrong password?)");
                    break;
                case WL_DISCONNECTED:
                    Serial.println("Error: Disconnected");
                    break;
                default:
                    Serial.printf("Error: Unknown status %d\n", WiFi.status());
                    break;
            }
        }

        // Vérifier la mémoire après activation
        uint32_t free_heap_after = esp_get_free_heap_size();
        Serial.printf("Free heap after enabling WiFi: %lu bytes\n", free_heap_after);
        Serial.printf("Memory used: %lu bytes\n", free_heap_before - free_heap_after);

    } else {
        Serial.println("Usage: wifi_enable <ssid> <password>");
        Serial.println("Example: wifi_enable MyNetwork mypassword123");
        return SYS_INVALID_PARAM;
    }

    Serial.println("========================");
    return SYS_OK;
}

// Nouvelles commandes WiFi avancées
SysError_t cmd_wifi_scan(int argc, char* argv[]) {
    Serial.println("=== WiFi Network Scan ===");
    Serial.println("Scanning for available networks...");

    // Vérifier que le WiFi est en mode STA
    if (WiFi.getMode() != WIFI_MODE_STA) {
        Serial.println("ERROR: WiFi scan requires STA mode");
        Serial.println("Use 'wifi_enable <ssid> <password>' to enable STA mode");
        Serial.println("========================");
        return SYS_ERROR;
    }

    int n = WiFi.scanNetworks();
    if (n == 0) {
        Serial.println("No networks found");
    } else {
        Serial.printf("%d networks found:\n", n);
        Serial.println("SSID\t\t\t\tRSSI\tEncryption\tChannel");
        Serial.println("--------------------------------------------------------");

        for (int i = 0; i < n; ++i) {
            String ssid = WiFi.SSID(i);
            int rssi = WiFi.RSSI(i);
            int channel = WiFi.channel(i);
            String encryption = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "Secured";

            Serial.printf("%-20s\t%d\t%s\t\t%d\n",
                         ssid.c_str(), rssi, encryption.c_str(), channel);
        }
    }

    Serial.println("========================");
    return SYS_OK;
}

SysError_t cmd_wifi_reconnect(int argc, char* argv[]) {
    Serial.println("=== WiFi Reconnection ===");

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi is currently connected");
        Serial.printf("Current IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("Current RSSI: %d dBm\n", WiFi.RSSI());
    }

    Serial.println("Forcing WiFi reconnection...");
    WiFi.disconnect(true);
    delay(1000);

    // Utiliser les credentials stockés
    extern bool load_wifi_credentials();
    extern const char* get_stored_ssid();
    extern const char* get_stored_password();
    extern bool connect_to_wifi(const char* ssid, const char* password);

    if (load_wifi_credentials()) {
        Serial.printf("Reconnecting to: %s\n", get_stored_ssid());
        if (connect_to_wifi(get_stored_ssid(), get_stored_password())) {
            Serial.println("✅ Reconnection successful!");
        } else {
            Serial.println("❌ Reconnection failed");
        }
    } else {
        Serial.println("❌ No saved credentials found");
        Serial.println("Use 'wifi_save <ssid> <password>' to save credentials first");
    }

    Serial.println("========================");
    return SYS_OK;
}

SysError_t cmd_wifi_stats(int argc, char* argv[]) {
    Serial.println("=== WiFi Statistics ===");

    // Informations de base
    Serial.printf("WiFi Status: %s\n",
                 (WiFi.status() == WL_CONNECTED) ? "CONNECTED" : "DISCONNECTED");

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
        Serial.printf("Subnet Mask: %s\n", WiFi.subnetMask().toString().c_str());
        Serial.printf("DNS Server: %s\n", WiFi.dnsIP().toString().c_str());
        Serial.printf("SSID: %s\n", WiFi.SSID().c_str());
        Serial.printf("BSSID: %s\n", WiFi.BSSIDstr().c_str());
        Serial.printf("Channel: %d\n", WiFi.channel());
        Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());

        // Qualité du signal
        int rssi = WiFi.RSSI();
        String signal_quality;
        if (rssi >= -50) signal_quality = "Excellent";
        else if (rssi >= -60) signal_quality = "Good";
        else if (rssi >= -70) signal_quality = "Fair";
        else if (rssi >= -80) signal_quality = "Poor";
        else signal_quality = "Very Poor";

        Serial.printf("Signal Quality: %s\n", signal_quality.c_str());
    }

    // Informations système
    Serial.printf("WiFi Mode: %s\n",
                 (WiFi.getMode() == WIFI_MODE_STA) ? "Station" :
                 (WiFi.getMode() == WIFI_MODE_AP) ? "Access Point" :
                 (WiFi.getMode() == WIFI_MODE_APSTA) ? "AP+Station" : "NULL");

    Serial.printf("Free Heap: %lu bytes\n", esp_get_free_heap_size());

    Serial.println("========================");
    return SYS_OK;
}

// Nouvelles commandes de gestion des credentials WiFi
SysError_t cmd_wifi_save(int argc, char* argv[]) {
    Serial.println("=== Save WiFi Credentials ===");

    if (argc != 3) {
        Serial.println("Usage: wifi_save <ssid> <password>");
        Serial.println("Example: wifi_save MyNetwork mypassword123");
        Serial.println("==========================");
        return SYS_INVALID_PARAM;
    }

    // Vérifier la longueur des paramètres
    if (strlen(argv[1]) > 31) {
        Serial.println("ERROR: SSID too long (max 31 characters)");
        Serial.println("==========================");
        return SYS_INVALID_PARAM;
    }

    if (strlen(argv[2]) > 63) {
        Serial.println("ERROR: Password too long (max 63 characters)");
        Serial.println("==========================");
        return SYS_INVALID_PARAM;
    }

    // Sauvegarder les credentials
    extern void save_wifi_credentials(const char* ssid, const char* password);
    save_wifi_credentials(argv[1], argv[2]);

    Serial.println("✅ WiFi credentials saved successfully!");
    Serial.println("Use 'wifi_auto' to connect automatically");
    Serial.println("==========================");
    return SYS_OK;
}

SysError_t cmd_wifi_clear(int argc, char* argv[]) {
    Serial.println("=== Clear WiFi Credentials ===");

    extern void clear_wifi_credentials();
    extern bool load_wifi_credentials();

    if (load_wifi_credentials()) {
        clear_wifi_credentials();
        Serial.println("✅ WiFi credentials cleared successfully!");
    } else {
        Serial.println("ℹ️  No saved credentials to clear");
    }

    Serial.println("============================");
    return SYS_OK;
}

SysError_t cmd_wifi_auto(int argc, char* argv[]) {
    Serial.println("=== Auto WiFi Connection ===");

    extern bool load_wifi_credentials();
    extern const char* get_stored_ssid();
    extern const char* get_stored_password();
    extern bool connect_to_wifi(const char* ssid, const char* password);

    if (!load_wifi_credentials()) {
        Serial.println("❌ No saved credentials found");
        Serial.println("Use 'wifi_save <ssid> <password>' to save credentials first");
        Serial.println("============================");
        return SYS_ERROR;
    }

    Serial.printf("Attempting to connect to: %s\n", get_stored_ssid());

    if (connect_to_wifi(get_stored_ssid(), get_stored_password())) {
        Serial.println("✅ Auto-connection successful!");
    } else {
        Serial.println("❌ Auto-connection failed");
        Serial.println("Check your credentials with 'wifi_save'");
    }

    Serial.println("============================");
    return SYS_OK;
}

SysError_t cmd_network_test(int argc, char* argv[]) {
    Serial.println("=== Network Test ===");

    // Test 1: Vérifier la connexion WiFi
    Serial.println("1. Testing WiFi connection...");
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("   ✓ WiFi is connected.");
        Serial.printf("   IP Address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("   RSSI: %d dBm\n", WiFi.RSSI());
    } else {
        Serial.println("   ✗ WiFi is not connected.");
        Serial.println("   Please ensure 'wifi_save' and 'wifi_auto' are configured.");
        Serial.println("============================");
        return SYS_ERROR;
    }

    // Test 2: Tester la résolution DNS
    Serial.println("2. Testing DNS resolution...");
    Serial.printf("   Attempting to resolve 'www.google.com'...");
    IPAddress google_ip;
    if (WiFi.hostByName("www.google.com", google_ip)) {
        Serial.printf("   ✓ DNS resolution successful. IP: %s\n", google_ip.toString().c_str());
    } else {
        Serial.println("   ✗ DNS resolution failed. Check WiFi connection or network.");
    }

    Serial.println("============================");
    return SYS_OK;
}

SysError_t cmd_neofetch(int argc, char* argv[]) {
    // Appeler la fonction d'affichage du logo système
    extern void display_system_logo();
    display_system_logo();
    return SYS_OK;
}

// Commandes NTP minimalistes
SysError_t cmd_ntp_status(int argc, char* argv[]) {
    Serial.println("=== NTP Status ===");

    NtpStatus_t status = ntp_get_status();
    Serial.printf("Status: ");
    switch (status) {
        case NTP_STATUS_UNINITIALIZED:
            Serial.println("UNINITIALIZED");
            break;
        case NTP_STATUS_DISCONNECTED:
            Serial.println("DISCONNECTED");
            break;
        case NTP_STATUS_SYNCING:
            Serial.println("SYNCING");
            break;
        case NTP_STATUS_SYNCED:
            Serial.println("SYNCED");
            break;
        case NTP_STATUS_FAILED:
            Serial.println("FAILED");
            break;
        case NTP_STATUS_TIMEOUT:
            Serial.println("TIMEOUT");
            break;
        default:
            Serial.println("UNKNOWN");
            break;
    }

    if (status == NTP_STATUS_SYNCED) {
        time_t current_time = ntp_get_time();
        Serial.printf("Current time: %s\n", ntp_format_time(current_time).c_str());

        NtpSyncInfo_t sync_info;
        ntp_get_sync_info(&sync_info);
        Serial.printf("Last sync: %s\n", ntp_format_time(sync_info.last_sync_time).c_str());
        Serial.printf("Last sync server: %s\n", sync_info.last_sync_server);
        Serial.printf("Successful syncs: %d\n", sync_info.successful_syncs);
        Serial.printf("Failed syncs: %d\n", sync_info.failed_syncs);
    }

    Serial.println("==================");
    return SYS_OK;
}

SysError_t cmd_ntp_sync(int argc, char* argv[]) {
    Serial.println("=== Manual NTP Sync ===");

    // Vérifier la connexion WiFi
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("ERROR: WiFi not connected");
        Serial.println("Connect to WiFi first using 'wifi_save' and 'wifi_auto'");
        Serial.println("=====================");
        return SYS_ERROR;
    }

    NtpStatus_t status = ntp_sync();
    Serial.printf("Sync result: ");
    switch (status) {
        case NTP_STATUS_SYNCED:
            Serial.println("SUCCESS");
            break;
        case NTP_STATUS_FAILED:
            Serial.println("FAILED");
            break;
        case NTP_STATUS_TIMEOUT:
            Serial.println("TIMEOUT");
            break;
        case NTP_STATUS_DISCONNECTED:
            Serial.println("DISCONNECTED");
            break;
        default:
            Serial.println("UNKNOWN");
            break;
    }

    if (status == NTP_STATUS_SYNCED) {
        time_t current_time = ntp_get_time();
        Serial.printf("Current time: %s\n", ntp_format_time(current_time).c_str());
    }

    Serial.println("=====================");
    return SYS_OK;
}

SysError_t cmd_ntp_test(int argc, char* argv[]) {
    Serial.println("=== NTP Diagnostic Test ===");

    // Test 1: Vérifier l'état du système
    Serial.println("1. Testing NTP system state...");
    NtpStatus_t status = ntp_get_status();
    Serial.printf("   NTP Status: %s\n",
        (status == NTP_STATUS_UNINITIALIZED) ? "UNINITIALIZED" :
        (status == NTP_STATUS_DISCONNECTED) ? "DISCONNECTED" :
        (status == NTP_STATUS_SYNCING) ? "SYNCING" :
        (status == NTP_STATUS_SYNCED) ? "SYNCED" : "FAILED");

    // Test 2: Vérifier la connexion WiFi
    Serial.println("2. Testing WiFi connection...");
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("   ✓ WiFi is connected.");
        Serial.printf("   IP Address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("   Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
        Serial.printf("   DNS: %s\n", WiFi.dnsIP().toString().c_str());
        Serial.printf("   RSSI: %d dBm\n", WiFi.RSSI());
    } else {
        Serial.println("   ✗ WiFi is not connected.");
        Serial.println("   Please connect to WiFi first using 'wifi_save' and 'wifi_auto'");
        Serial.println("============================");
        return SYS_ERROR;
    }

    // Test 3: Test de connectivité Internet
    Serial.println("3. Testing Internet connectivity...");
    WiFiClient client;
    if (client.connect("8.8.8.8", 53)) {
        Serial.println("   ✓ Internet connectivity OK");
        client.stop();
    } else {
        Serial.println("   ✗ Internet connectivity failed");
    }

    // Test 4: Test de résolution DNS
    Serial.println("4. Testing DNS resolution...");
    IPAddress ntp_ip;
    if (WiFi.hostByName("pool.ntp.org", ntp_ip)) {
        Serial.printf("   ✓ DNS resolution OK: pool.ntp.org -> %s\n", ntp_ip.toString().c_str());
    } else {
        Serial.println("   ✗ DNS resolution failed for pool.ntp.org");
    }

    // Test 5: Tester la synchronisation NTP
    Serial.println("5. Testing NTP synchronization...");
    NtpStatus_t sync_status = ntp_sync();
    Serial.printf("   NTP sync result: %s\n",
        (sync_status == NTP_STATUS_SYNCED) ? "SUCCESS" :
        (sync_status == NTP_STATUS_FAILED) ? "FAILED" :
        (sync_status == NTP_STATUS_TIMEOUT) ? "TIMEOUT" : "UNKNOWN");

    // Test 6: Vérifier le temps actuel
    Serial.println("6. Checking current time...");
    time_t current_time = ntp_get_time();
    if (current_time != 0) {
        Serial.printf("   ✓ Current time: %s\n", ntp_format_time(current_time).c_str());
    } else {
        Serial.println("   ✗ Could not retrieve current time.");
    }

    // Test 7: Vérifier l'âge de la synchronisation
    NtpSyncInfo_t sync_info;
    ntp_get_sync_info(&sync_info);
    Serial.printf("   Successful syncs: %d\n", sync_info.successful_syncs);
    Serial.printf("   Failed syncs: %d\n", sync_info.failed_syncs);
    if (strlen(sync_info.last_sync_server) > 0) {
        Serial.printf("   Last sync server: %s\n", sync_info.last_sync_server);
    }

    Serial.println("============================");
    return SYS_OK;
}

SysError_t cmd_ntp_timezone(int argc, char* argv[]) {
    Serial.println("=== NTP Timezone Configuration ===");

    if (argc == 1) {
        // Afficher la configuration actuelle
        Serial.printf("Current timezone: %s\n", ntp_get_timezone_string().c_str());
        Serial.printf("GMT offset: %d seconds\n", NTP_GMT_OFFSET_SEC);
        Serial.printf("Daylight offset: %d seconds\n", NTP_DAYLIGHT_OFFSET_SEC);
        Serial.println();
        Serial.println("Usage: ntp_timezone <gmt_offset_hours> [daylight_offset_hours]");
        Serial.println("Examples:");
        Serial.println("  ntp_timezone 0        # UTC");
        Serial.println("  ntp_timezone 1        # UTC+1 (Central Europe)");
        Serial.println("  ntp_timezone -5       # UTC-5 (Eastern US)");
        Serial.println("  ntp_timezone 1 1      # UTC+1 with 1h daylight saving");
        Serial.println("================================");
        return SYS_OK;
    }

    if (argc >= 2) {
        int gmt_offset_hours = atoi(argv[1]);
        int daylight_offset_hours = (argc >= 3) ? atoi(argv[2]) : 0;

        // Validation
        if (gmt_offset_hours < -12 || gmt_offset_hours > 14) {
            Serial.println("ERROR: GMT offset must be between -12 and +14 hours");
            Serial.println("================================");
            return SYS_INVALID_PARAM;
        }

        if (daylight_offset_hours < 0 || daylight_offset_hours > 2) {
            Serial.println("ERROR: Daylight offset must be between 0 and 2 hours");
            Serial.println("================================");
            return SYS_INVALID_PARAM;
        }

        // Conversion en secondes
        int gmt_offset_sec = gmt_offset_hours * 3600;
        int daylight_offset_sec = daylight_offset_hours * 3600;

        Serial.printf("Setting timezone to UTC%+d", gmt_offset_hours);
        if (daylight_offset_hours > 0) {
            Serial.printf(" (DST: +%d)", daylight_offset_hours);
        }
        Serial.println();

        if (ntp_set_timezone(gmt_offset_sec, daylight_offset_sec)) {
            Serial.println("✅ Timezone updated successfully!");
            Serial.printf("New timezone: %s\n", ntp_get_timezone_string().c_str());

            // Re-synchroniser pour appliquer le nouveau fuseau
            Serial.println("Re-syncing NTP with new timezone...");
            NtpStatus_t sync_status = ntp_sync();
            if (sync_status == NTP_STATUS_SYNCED) {
                time_t current_time = ntp_get_time();
                Serial.printf("Current time: %s\n", ntp_format_time(current_time).c_str());
            }
    } else {
            Serial.println("❌ Failed to update timezone");
        }

        Serial.println("================================");
        return SYS_OK;
    }

    Serial.println("Usage: ntp_timezone <gmt_offset_hours> [daylight_offset_hours]");
    Serial.println("================================");
    return SYS_INVALID_PARAM;
}

SysError_t cmd_ntp_utilities(int argc, char* argv[]) {
    Serial.println("=== NTP Time Utilities ===");

    if (!ntp_is_synced()) {
        Serial.println("❌ NTP not synchronized");
        Serial.println("Use 'ntp_sync' to synchronize first");
        Serial.println("=========================");
        return SYS_ERROR;
    }

    time_t current_time = ntp_get_time();
    struct tm* timeinfo = localtime(&current_time);

    // Informations de base
    Serial.printf("Current time: %s\n", ntp_format_time(current_time).c_str());
    Serial.printf("Time: %02d:%02d:%02d\n", ntp_get_hour(), ntp_get_minute(), ntp_get_second());
    Serial.printf("Date: %04d-%02d-%02d\n", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday);
    Serial.printf("Day of week: %s\n",
        (timeinfo->tm_wday == 0) ? "Sunday" :
        (timeinfo->tm_wday == 1) ? "Monday" :
        (timeinfo->tm_wday == 2) ? "Tuesday" :
        (timeinfo->tm_wday == 3) ? "Wednesday" :
        (timeinfo->tm_wday == 4) ? "Thursday" :
        (timeinfo->tm_wday == 5) ? "Friday" : "Saturday");

    // Informations de synchronisation
    Serial.printf("Uptime since sync: %lu seconds\n", ntp_get_uptime_since_sync());
    Serial.printf("Timezone: %s\n", ntp_get_timezone_string().c_str());

    // Conditions temporelles
    Serial.println("\nTime Conditions:");
    Serial.printf("  Business hours: %s\n", ntp_is_business_hours() ? "YES" : "NO");
    Serial.printf("  Night time: %s\n", ntp_is_night_time() ? "YES" : "NO");

    // Suggestions d'utilisation
    Serial.println("\nUsage Examples:");
    Serial.println("  if (ntp_is_business_hours()) { /* Reduce power consumption */ }");
    Serial.println("  if (ntp_is_night_time()) { /* Enable night mode */ }");
    Serial.println("  time_t event_time = ntp_get_time(); /* Log event timestamp */");

    Serial.println("=========================");
    return SYS_OK;
}

// Network functionality removed

// HTTP server functionality - To be implemented

// Commandes Application Manager
SysError_t cmd_app_status(int argc, char* argv[]) {
    Serial.println("=== Application Manager Status ===");
    app_manager_print_status();
    Serial.println("==================================");
    return SYS_OK;
}

SysError_t cmd_app_list(int argc, char* argv[]) {
    Serial.println("=== Registered Applications ===");
    app_manager_print_all_apps();
    Serial.println("===============================");
    return SYS_OK;
}

SysError_t cmd_app_start(int argc, char* argv[]) {
    if (argc < 2) {
        Serial.println("Usage: app_start <app_id>");
        Serial.println("Example: app_start 1");
        return SYS_INVALID_PARAM;
    }

    uint8_t app_id = atoi(argv[1]);

    if (!app_exists(app_id)) {
        Serial.printf("ERROR: Application %d does not exist\n", app_id);
        return SYS_INVALID_PARAM;
    }

    Serial.printf("Starting application %d...\n", app_id);
    SysError_t result = app_start(app_id);

    if (result == SYS_OK) {
        Serial.println("✅ Application started successfully");
    } else {
        Serial.printf("❌ Failed to start application: %d\n", result);
    }

    return result;
}

SysError_t cmd_app_stop(int argc, char* argv[]) {
    if (argc < 2) {
        Serial.println("Usage: app_stop <app_id>");
        Serial.println("Example: app_stop 1");
        return SYS_INVALID_PARAM;
    }

    uint8_t app_id = atoi(argv[1]);

    if (!app_exists(app_id)) {
        Serial.printf("ERROR: Application %d does not exist\n", app_id);
        return SYS_INVALID_PARAM;
    }

    Serial.printf("Stopping application %d...\n", app_id);
    SysError_t result = app_stop(app_id);

    if (result == SYS_OK) {
        Serial.println("✅ Application stopped successfully");
    } else {
        Serial.printf("❌ Failed to stop application: %d\n", result);
    }

    return result;
}

SysError_t cmd_app_restart(int argc, char* argv[]) {
    if (argc < 2) {
        Serial.println("Usage: app_restart <app_id>");
        Serial.println("Example: app_restart 1");
        return SYS_INVALID_PARAM;
    }

    uint8_t app_id = atoi(argv[1]);

    if (!app_exists(app_id)) {
        Serial.printf("ERROR: Application %d does not exist\n", app_id);
        return SYS_INVALID_PARAM;
    }

    Serial.printf("Restarting application %d...\n", app_id);
    SysError_t result = app_restart(app_id);

    if (result == SYS_OK) {
        Serial.println("✅ Application restarted successfully");
    } else {
        Serial.printf("❌ Failed to restart application: %d\n", result);
    }

    return result;
}

SysError_t cmd_app_pause(int argc, char* argv[]) {
    if (argc < 2) {
        Serial.println("Usage: app_pause <app_id>");
        Serial.println("Example: app_pause 1");
        return SYS_INVALID_PARAM;
    }

    uint8_t app_id = atoi(argv[1]);

    if (!app_exists(app_id)) {
        Serial.printf("ERROR: Application %d does not exist\n", app_id);
        return SYS_INVALID_PARAM;
    }

    Serial.printf("Pausing application %d...\n", app_id);
    SysError_t result = app_pause(app_id);

    if (result == SYS_OK) {
        Serial.println("✅ Application paused successfully");
    } else {
        Serial.printf("❌ Failed to pause application: %d\n", result);
    }

    return result;
}

SysError_t cmd_app_resume(int argc, char* argv[]) {
    if (argc < 2) {
        Serial.println("Usage: app_resume <app_id>");
        Serial.println("Example: app_resume 1");
        return SYS_INVALID_PARAM;
    }

    uint8_t app_id = atoi(argv[1]);

    if (!app_exists(app_id)) {
        Serial.printf("ERROR: Application %d does not exist\n", app_id);
        return SYS_INVALID_PARAM;
    }

    Serial.printf("Resuming application %d...\n", app_id);
    SysError_t result = app_resume(app_id);

    if (result == SYS_OK) {
        Serial.println("✅ Application resumed successfully");
    } else {
        Serial.printf("❌ Failed to resume application: %d\n", result);
    }

    return result;
}

SysError_t cmd_app_info(int argc, char* argv[]) {
    if (argc < 2) {
        Serial.println("Usage: app_info <app_id>");
        Serial.println("Example: app_info 1");
        return SYS_INVALID_PARAM;
    }

    uint8_t app_id = atoi(argv[1]);

    if (!app_exists(app_id)) {
        Serial.printf("ERROR: Application %d does not exist\n", app_id);
        return SYS_INVALID_PARAM;
    }

    AppInfo_t info;
    SysError_t result = app_get_info(app_id, &info);

    if (result == SYS_OK) {
        Serial.println("=== Application Information ===");
        Serial.printf("ID: %d\n", info.app_id);
        Serial.printf("Name: %s\n", info.name);
        Serial.printf("Description: %s\n", info.description);
        Serial.printf("Type: %s\n",
            (info.type == APP_TYPE_SYSTEM) ? "SYSTEM" :
            (info.type == APP_TYPE_USER) ? "USER" : "BACKGROUND");
        Serial.printf("State: %s\n",
            (info.state == APP_STATE_UNLOADED) ? "UNLOADED" :
            (info.state == APP_STATE_LOADING) ? "LOADING" :
            (info.state == APP_STATE_RUNNING) ? "RUNNING" :
            (info.state == APP_STATE_PAUSED) ? "PAUSED" :
            (info.state == APP_STATE_STOPPED) ? "STOPPED" : "ERROR");
        Serial.printf("Stack Size: %lu bytes\n", info.stack_size);
        Serial.printf("Priority: %lu\n", info.priority);
        Serial.printf("Memory Used: %lu bytes\n", info.memory_used);
        Serial.printf("CPU Time: %lu ticks\n", info.cpu_time);
        Serial.printf("Start Time: %lu\n", info.start_time);
        Serial.printf("Last Activity: %lu\n", info.last_activity);
        Serial.printf("Auto Start: %s\n", info.auto_start ? "YES" : "NO");
        Serial.printf("Persistent: %s\n", info.persistent ? "YES" : "NO");
        Serial.println("===============================");
    } else {
        Serial.printf("❌ Failed to get application info: %d\n", result);
    }

    return result;
}

// Nouvelles commandes de gestion globale des applications
SysError_t cmd_app_start_all(int argc, char* argv[]) {
    Serial.println("=== Start All Applications ===");

    uint8_t app_count = app_get_count();
    if (app_count == 0) {
        Serial.println("No applications registered");
        Serial.println("===========================");
        return SYS_OK;
    }

    Serial.printf("Starting %d applications...\n", app_count);
    SysError_t result = app_manager_start_all();

    if (result == SYS_OK) {
        Serial.println("✅ All applications started successfully");
    } else {
        Serial.printf("❌ Failed to start all applications: %d\n", result);
    }

    Serial.println("===========================");
    return result;
}

SysError_t cmd_app_stop_all(int argc, char* argv[]) {
    Serial.println("=== Stop All Applications ===");

    uint8_t app_count = app_get_count();
    if (app_count == 0) {
        Serial.println("No applications registered");
        Serial.println("==========================");
        return SYS_OK;
    }

    Serial.printf("Stopping %d applications...\n", app_count);
    SysError_t result = app_manager_stop_all();

    if (result == SYS_OK) {
        Serial.println("✅ All applications stopped successfully");
    } else {
        Serial.printf("❌ Failed to stop all applications: %d\n", result);
    }

    Serial.println("==========================");
    return result;
}

SysError_t cmd_app_pause_all(int argc, char* argv[]) {
    Serial.println("=== Pause All Applications ===");

    uint8_t app_count = app_get_count();
    if (app_count == 0) {
        Serial.println("No applications registered");
        Serial.println("===========================");
        return SYS_OK;
    }

    Serial.printf("Pausing %d applications...\n", app_count);
    SysError_t result = app_manager_pause_all();

    if (result == SYS_OK) {
        Serial.println("✅ All applications paused successfully");
    } else {
        Serial.printf("❌ Failed to pause all applications: %d\n", result);
    }

    Serial.println("===========================");
    return result;
}

SysError_t cmd_app_resume_all(int argc, char* argv[]) {
    Serial.println("=== Resume All Applications ===");

    uint8_t app_count = app_get_count();
    if (app_count == 0) {
        Serial.println("No applications registered");
        Serial.println("============================");
        return SYS_OK;
    }

    Serial.printf("Resuming %d applications...\n", app_count);
    SysError_t result = app_manager_resume_all();

    if (result == SYS_OK) {
        Serial.println("✅ All applications resumed successfully");
    } else {
        Serial.printf("❌ Failed to resume all applications: %d\n", result);
    }

    Serial.println("============================");
    return result;
}

// Commandes RTC et Time Sync simples
SysError_t cmd_rtc_status(int argc, char* argv[]) {
    Serial.println("=== RTC Status ===");

    if (!rtc_is_initialized()) {
        Serial.println("RTC: NOT INITIALIZED");
        Serial.println("===================");
        return SYS_ERROR;
    }

    RtcStatus_t status = rtc_get_status();
    Serial.printf("RTC Status: ");
    switch (status) {
        case RTC_STATUS_OK:
            Serial.println("OK");
            break;
        case RTC_STATUS_ERROR:
            Serial.println("ERROR");
            break;
        case RTC_STATUS_NOT_FOUND:
            Serial.println("NOT FOUND");
            break;
        default:
            Serial.println("UNKNOWN");
            break;
    }

    if (status == RTC_STATUS_OK) {
        time_t rtc_time = rtc_get_time();
        if (rtc_time > 0) {
            struct tm* timeinfo = localtime(&rtc_time);
            Serial.printf("RTC Time: %04d-%02d-%02d %02d:%02d:%02d\n",
                         timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
                         timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        }

        float temp = rtc_get_temperature();
        if (temp > -999.0f) {
            Serial.printf("Temperature: %.1f°C\n", temp);
        }

        bool battery_ok = rtc_is_battery_ok();
        Serial.printf("Battery: %s\n", battery_ok ? "OK" : "LOW");
    }

    Serial.println("===================");
    return SYS_OK;
}

SysError_t cmd_rtc_recovery(int argc, char* argv[]) {
    Serial.println("=== RTC Recovery ===");

    if (!rtc_is_initialized()) {
        Serial.println("ERROR: RTC not initialized");
        Serial.println("========================");
        return SYS_ERROR;
    }

    Serial.println("Attempting RTC recovery...");
    SysError_t result = rtc_recovery_attempt();

    if (result == SYS_OK) {
        Serial.println("✅ RTC recovery successful!");

        // Afficher le nouveau statut
        RtcStatus_t status = rtc_get_status();
        Serial.printf("New RTC Status: %s\n", rtc_get_status_string().c_str());

        if (status == RTC_STATUS_OK) {
            time_t rtc_time = rtc_get_time();
            if (rtc_time > 0) {
                struct tm* timeinfo = localtime(&rtc_time);
                Serial.printf("RTC Time: %04d-%02d-%02d %02d:%02d:%02d\n",
                             timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
                             timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
            }
        }
    } else {
        Serial.println("❌ RTC recovery failed");
        Serial.printf("Current RTC Status: %s\n", rtc_get_status_string().c_str());
    }

    Serial.println("========================");
    return result;
}

SysError_t cmd_time_status(int argc, char* argv[]) {
    Serial.println("=== Time Status ===");

    if (!time_sync_is_initialized()) {
        Serial.println("Time Sync: NOT INITIALIZED");
        Serial.println("========================");
        return SYS_ERROR;
    }

    TimeSource_t source = time_sync_get_current_source();
    Serial.printf("Time Source: ");
    switch (source) {
        case TIME_SOURCE_NTP:
            Serial.println("NTP");
            break;
        case TIME_SOURCE_RTC:
            Serial.println("RTC");
            break;
        case TIME_SOURCE_SYSTEM:
            Serial.println("SYSTEM");
            break;
        default:
            Serial.println("UNKNOWN");
            break;
    }

    time_t current_time = time_sync_get_current_time();
    if (current_time > 0) {
        Serial.printf("Current Time: %s\n", time_sync_format_current_time().c_str());
    }

    Serial.println("========================");
    return SYS_OK;
}

SysError_t cmd_time_sync(int argc, char* argv[]) {
    Serial.println("=== Time Synchronization ===");

    if (!time_sync_is_initialized()) {
        Serial.println("ERROR: Time sync not initialized");
        Serial.println("=============================");
        return SYS_ERROR;
    }

    Serial.println("Forcing time synchronization...");
    SysError_t result = time_sync_automatic();

    if (result == SYS_OK) {
        TimeSource_t source = time_sync_get_current_source();
        Serial.printf("✅ Sync successful - Source: ");
        switch (source) {
            case TIME_SOURCE_NTP:
                Serial.println("NTP");
                break;
            case TIME_SOURCE_RTC:
                Serial.println("RTC");
                break;
            case TIME_SOURCE_SYSTEM:
                Serial.println("SYSTEM");
                break;
            default:
                Serial.println("UNKNOWN");
                break;
        }

        time_t current_time = time_sync_get_current_time();
        if (current_time > 0) {
            Serial.printf("Current Time: %s\n", time_sync_format_current_time().c_str());
        }
    } else {
        Serial.println("❌ Sync failed");
    }

    Serial.println("=============================");
    return result;
}

SysError_t cmd_time_sources(int argc, char* argv[]) {
    Serial.println("=== Time Sources Info ===");

    if (!time_sync_is_initialized()) {
        Serial.println("ERROR: Time sync not initialized");
        Serial.println("=============================");
        return SYS_ERROR;
    }

    Serial.println(time_sync_get_source_info().c_str());
    Serial.println("=============================");
    return SYS_OK;
}

SysError_t cmd_rtc_temp(int argc, char* argv[]) {
    Serial.println("=== RTC Temperature ===");

    if (!rtc_is_initialized()) {
        Serial.println("ERROR: RTC not initialized");
        Serial.println("========================");
        return SYS_ERROR;
    }

    float temp = rtc_get_temperature();
    if (temp > -999.0f) {
        Serial.printf("RTC Temperature: %.1f°C\n", temp);
    } else {
        Serial.println("ERROR: Cannot read RTC temperature");
    }

    Serial.println("========================");
    return SYS_OK;
}

SysError_t cmd_rtc_battery(int argc, char* argv[]) {
    Serial.println("=== RTC Battery Status ===");

    if (!rtc_is_initialized()) {
        Serial.println("ERROR: RTC not initialized");
        Serial.println("=========================");
        return SYS_ERROR;
    }

    bool battery_ok = rtc_is_battery_ok();
    Serial.printf("RTC Battery: %s\n", battery_ok ? "OK" : "LOW");

    RtcStatus_t status = rtc_get_status();
    if (status == RTC_STATUS_BATTERY_LOW) {
        Serial.println("WARNING: RTC battery is low - time may be lost on power failure");
    }

    Serial.println("=========================");
    return SYS_OK;
}

SysError_t cmd_time_source(int argc, char* argv[]) {
    Serial.println("=== Current Time Source ===");

    if (!time_sync_is_initialized()) {
        Serial.println("ERROR: Time sync not initialized");
        Serial.println("==============================");
        return SYS_ERROR;
    }

    TimeSource_t source = time_sync_get_current_source();
    Serial.printf("Current Time Source: ");
    switch (source) {
        case TIME_SOURCE_NTP:
            Serial.println("NTP (Network Time Protocol)");
            break;
        case TIME_SOURCE_RTC:
            Serial.println("RTC (Real-Time Clock)");
            break;
        case TIME_SOURCE_SYSTEM:
            Serial.println("SYSTEM (Internal Clock)");
            break;
        case TIME_SOURCE_UNKNOWN:
            Serial.println("UNKNOWN (No valid source)");
            break;
        default:
            Serial.println("UNKNOWN");
            break;
    }

    TimeSyncStatus_t sync_status = time_sync_get_status();
    Serial.printf("Sync Status: ");
    switch (sync_status) {
        case TIME_SYNC_STATUS_OK:
            Serial.println("OK");
            break;
        case TIME_SYNC_STATUS_DEGRADED_MODE:
            Serial.println("DEGRADED (Using fallback source)");
            break;
        case TIME_SYNC_STATUS_ERROR:
            Serial.println("ERROR");
            break;
        case TIME_SYNC_STATUS_NO_SOURCE:
            Serial.println("NO SOURCE");
            break;
        default:
            Serial.println("UNKNOWN");
            break;
    }

    Serial.println("==============================");
    return SYS_OK;
}

// Commandes ESP-NOW supprimées - structure de base uniquement

