#ifndef WIFI_PERSISTENCE_H
#define WIFI_PERSISTENCE_H

#include "../core/kernel.h"
#include <ArduinoJson.h>

// Configuration du fichier WiFi
#define WIFI_CONFIG_FILE "/wifi_config.json"
#define WIFI_CONFIG_VERSION "1.0"

// Structure des credentials WiFi
typedef struct {
    char ssid[32];
    char password[64];
    bool valid;
    uint32_t last_saved;
} WifiCredentials_t;

// Structure de configuration WiFi (renommée pour éviter le conflit)
typedef struct {
    bool auto_connect;
    uint32_t reconnect_attempts;
    uint32_t connection_timeout;
} WifiPersistenceConfig_t;

// Structure complète des données WiFi
typedef struct {
    char version[8];
    WifiCredentials_t credentials;
    WifiPersistenceConfig_t config;
} WifiData_t;

// Fonctions de gestion de la persistance WiFi
SysError_t wifi_persistence_init(void);
SysError_t wifi_persistence_save_credentials(const char* ssid, const char* password);
SysError_t wifi_persistence_load_credentials(void);
SysError_t wifi_persistence_clear_credentials(void);
SysError_t wifi_persistence_save_config(const WifiPersistenceConfig_t* config);
SysError_t wifi_persistence_load_config(WifiPersistenceConfig_t* config);

// Fonctions d'accès aux données
const char* wifi_persistence_get_ssid(void);
const char* wifi_persistence_get_password(void);
bool wifi_persistence_has_credentials(void);
WifiCredentials_t* wifi_persistence_get_credentials(void);
WifiPersistenceConfig_t* wifi_persistence_get_config(void);

// Fonctions utilitaires
SysError_t wifi_persistence_reset_to_defaults(void);
bool wifi_persistence_validate_credentials(const char* ssid, const char* password);

// Variables globales (pour compatibilité avec l'API existante)
extern WifiCredentials_t g_wifi_credentials;
extern WifiPersistenceConfig_t g_wifi_config;

#endif // WIFI_PERSISTENCE_H
