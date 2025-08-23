#ifndef MINIMAL_CONFIG_H
#define MINIMAL_CONFIG_H

// Configuration pour le mode minimal
#ifdef MINIMAL_MODE

// Macros pour les logs conditionnels
#define SERIAL_LOG_MINIMAL(msg) Serial.println(msg)
#define SERIAL_LOG_MINIMAL_FMT(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)

// Macros pour les chaînes courtes
#define STR_SHORT(msg) msg
#define STR_ERR "ERR"
#define STR_OK "OK"
#define STR_WARN "WARN"
#define STR_INFO "INFO"
#define STR_DEBUG "DBG"

// Macros pour les messages d'erreur courts
#define MSG_INIT_FAIL "Init fail"
#define MSG_MEM_FAIL "Mem fail"
#define MSG_WIFI_FAIL "WiFi fail"
#define MSG_NTP_FAIL "NTP fail"
#define MSG_TASK_FAIL "Task fail"
#define MSG_APP_FAIL "App fail"

// Macros pour les noms de composants courts
#define COMP_SYS "SYS"
#define COMP_WIFI "WIFI"
#define COMP_NTP "NTP"
#define COMP_TASK "TASK"
#define COMP_MEM "MEM"
#define COMP_LOG "LOG"

#else

// Mode normal - logs complets
#define SERIAL_LOG_MINIMAL(msg) Serial.println(msg)
#define SERIAL_LOG_MINIMAL_FMT(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)

// Chaînes complètes
#define STR_SHORT(msg) msg
#define STR_ERR "ERROR"
#define STR_OK "OK"
#define STR_WARN "WARNING"
#define STR_INFO "INFO"
#define STR_DEBUG "DEBUG"

// Messages d'erreur complets
#define MSG_INIT_FAIL "Initialization failed"
#define MSG_MEM_FAIL "Memory allocation failed"
#define MSG_WIFI_FAIL "WiFi connection failed"
#define MSG_NTP_FAIL "NTP synchronization failed"
#define MSG_TASK_FAIL "Task creation failed"
#define MSG_APP_FAIL "Application manager initialization failed"

// Noms de composants complets
#define COMP_SYS "SYSTEM"
#define COMP_WIFI "WIFI"
#define COMP_NTP "NTP"
#define COMP_TASK "TASK"
#define COMP_MEM "MEMORY"
#define COMP_LOG "LOG"

#endif

// Macros pour les logs conditionnels basés sur SERIAL_LOGS_MINIMAL
#ifdef SERIAL_LOGS_MINIMAL

#define SERIAL_PRINT_MINIMAL(msg) Serial.print(msg)
#define SERIAL_PRINTLN_MINIMAL(msg) Serial.println(msg)
#define SERIAL_PRINTF_MINIMAL(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)

#else

#define SERIAL_PRINT_MINIMAL(msg) Serial.print(msg)
#define SERIAL_PRINTLN_MINIMAL(msg) Serial.println(msg)
#define SERIAL_PRINTF_MINIMAL(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)

#endif

// Macros pour l'optimisation des chaînes
#ifdef STRING_OPTIMIZATION

// Raccourcir les chaînes de caractères
#define STR_TASK_MANAGER "TM"
#define STR_MEMORY_MANAGER "MM"
#define STR_LOG_SYSTEM "LS"
#define STR_SYSTEM_MONITOR "SM"
#define STR_WIFI_MANAGER "WM"
#define STR_NTP_MANAGER "NM"
#define STR_INTERFACE "IF"

#define STR_RUNNING "RUN"
#define STR_SUSPENDED "SUS"
#define STR_DELETED "DEL"
#define STR_CONNECTED "CON"
#define STR_DISCONNECTED "DIS"
#define STR_SYNCED "SYNC"
#define STR_FAILED "FAIL"

#else

// Chaînes complètes
#define STR_TASK_MANAGER "TaskManager"
#define STR_MEMORY_MANAGER "MemoryManager"
#define STR_LOG_SYSTEM "LogSystem"
#define STR_SYSTEM_MONITOR "SystemMonitor"
#define STR_WIFI_MANAGER "WiFiManager"
#define STR_NTP_MANAGER "NTPManager"
#define STR_INTERFACE "Interface"

#define STR_RUNNING "RUNNING"
#define STR_SUSPENDED "SUSPENDED"
#define STR_DELETED "DELETED"
#define STR_CONNECTED "CONNECTED"
#define STR_DISCONNECTED "DISCONNECTED"
#define STR_SYNCED "SYNCED"
#define STR_FAILED "FAILED"

#endif

#endif // MINIMAL_CONFIG_H 