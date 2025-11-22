/**
 * @file app_autostart_config.h
 * @brief Application Auto-Start Configuration (Kconfig-style)
 *
 * This file allows you to configure which applications should automatically
 * start after system boot. Simply uncomment (remove //) the applications
 * you want to auto-start.
 *
 * Usage:
 *   - Comment (add //) to disable auto-start
 *   - Uncomment (remove //) to enable auto-start
 *
 * Example:
 *   #define CONFIG_APP_AUTOSTART_ESP32_MASTER    1  // Enabled
 *   // #define CONFIG_APP_AUTOSTART_ESP32_SENSOR 1  // Disabled
 */

#ifndef APP_AUTOSTART_CONFIG_H
#define APP_AUTOSTART_CONFIG_H

// ============================================================================
// Application Auto-Start Configurationt
// ============================================================================
// Set to 1 to enable auto-start, 0 or comment to disable

// ESP32 Master Application (ID: 10)
// Controls irrigation system, communicates with server, manages zones
#define CONFIG_APP_AUTOSTART_ESP32_MASTER    0

// ESP32 Sensor Application (ID: 11)
// Reads 12 moisture sensors and sends data to master
#define CONFIG_APP_AUTOSTART_ESP32_SENSOR   0

// ESP32 Communication Application (ID: 12)
// Controls relay zones and pump based on master commands
#define CONFIG_APP_AUTOSTART_ESP32_COM      0

// ============================================================================
// Helper Macros
// ============================================================================

// Check if ESP32 Master should auto-start
#if defined(CONFIG_APP_AUTOSTART_ESP32_MASTER) && CONFIG_APP_AUTOSTART_ESP32_MASTER == 1
    #define APP_AUTOSTART_ESP32_MASTER_ENABLED   1
#else
    #define APP_AUTOSTART_ESP32_MASTER_ENABLED   0
#endif

// Check if ESP32 Sensor should auto-start
#if defined(CONFIG_APP_AUTOSTART_ESP32_SENSOR) && CONFIG_APP_AUTOSTART_ESP32_SENSOR == 1
    #define APP_AUTOSTART_ESP32_SENSOR_ENABLED   1
#else
    #define APP_AUTOSTART_ESP32_SENSOR_ENABLED   0
#endif

// Check if ESP32 Com should auto-start
#if defined(CONFIG_APP_AUTOSTART_ESP32_COM) && CONFIG_APP_AUTOSTART_ESP32_COM == 1
    #define APP_AUTOSTART_ESP32_COM_ENABLED     1
#else
    #define APP_AUTOSTART_ESP32_COM_ENABLED     0
#endif

#endif // APP_AUTOSTART_CONFIG_H

