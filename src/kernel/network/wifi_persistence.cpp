#include "wifi_persistence.h"
#include "../core/log_system_optimized.h"
#include "../core/minimal_config.h"
#include <FS.h>
#include <SPIFFS.h>
#include <time.h>

// Variables globales pour compatibilité avec l'API existante
WifiCredentials_t g_wifi_credentials = {"", "", false, 0};
WifiPersistenceConfig_t g_wifi_config = {true, 3, 10000}; // auto_connect=true, 3 tentatives, 10s timeout

// Structure complète des données WiFi
static WifiData_t wifi_data = {
    WIFI_CONFIG_VERSION,
    {"", "", false, 0},
    {true, 3, 10000}
};

// Initialisation du système de persistance WiFi
SysError_t wifi_persistence_init(void) {
    SERIAL_PRINTLN_MINIMAL("WiFi Persistence: Initializing SPIFFS...");
    
    // Initialiser SPIFFS avec timeout
    if (!SPIFFS.begin(true)) {
        SERIAL_PRINTLN_MINIMAL("WiFi Persistence: SPIFFS initialization failed");
        kernel_log(LOG_LEVEL_ERROR, "WiFi Persistence: SPIFFS init failed");
        return SYS_ERROR;
    }
    
    // SPIFFS est maintenant initialisé et prêt
    
    // Charger les données existantes
    SysError_t result = wifi_persistence_load_credentials();
    if (result != SYS_OK) {
        if (result == SYS_NO_DATA) {
            SERIAL_PRINTLN_MINIMAL("WiFi Persistence: No existing data, using defaults");
        } else {
            SERIAL_PRINTLN_MINIMAL("WiFi Persistence: Error loading data, using defaults");
        }
        wifi_persistence_reset_to_defaults();
    }
    
    // Charger la configuration
    wifi_persistence_load_config(&g_wifi_config);
    
    SERIAL_PRINTLN_MINIMAL("WiFi Persistence: Initialized successfully");
    kernel_log(LOG_LEVEL_INFO, "WiFi Persistence: Initialized with SPIFFS");
    
    return SYS_OK;
}

// Sauvegarder les credentials WiFi
SysError_t wifi_persistence_save_credentials(const char* ssid, const char* password) {
    // Validation des paramètres
    if (!wifi_persistence_validate_credentials(ssid, password)) {
        return SYS_INVALID_PARAM;
    }
    
    SERIAL_PRINTLN_MINIMAL("WiFi Persistence: Saving credentials to SPIFFS...");
    
    // Mettre à jour les données en mémoire
    strncpy(wifi_data.credentials.ssid, ssid, sizeof(wifi_data.credentials.ssid) - 1);
    wifi_data.credentials.ssid[sizeof(wifi_data.credentials.ssid) - 1] = '\0';
    
    strncpy(wifi_data.credentials.password, password, sizeof(wifi_data.credentials.password) - 1);
    wifi_data.credentials.password[sizeof(wifi_data.credentials.password) - 1] = '\0';
    
    wifi_data.credentials.valid = true;
    wifi_data.credentials.last_saved = time(nullptr);
    
    // Mettre à jour les variables globales pour compatibilité
    g_wifi_credentials = wifi_data.credentials;
    
    // Sauvegarder dans SPIFFS
    if (!SPIFFS.begin(true)) {
        SERIAL_PRINTLN_MINIMAL("WiFi Persistence: SPIFFS not available");
        return SYS_ERROR;
    }
    
    File file = SPIFFS.open(WIFI_CONFIG_FILE, FILE_WRITE);
    if (!file) {
        SERIAL_PRINTLN_MINIMAL("WiFi Persistence: Failed to open file for writing");
        return SYS_ERROR;
    }
    
    // Créer le document JSON
    StaticJsonDocument<1024> doc;
    doc["version"] = wifi_data.version;
    
    // Section credentials
    JsonObject credentials = doc.createNestedObject("credentials");
    credentials["ssid"] = wifi_data.credentials.ssid;
    credentials["password"] = wifi_data.credentials.password;
    credentials["valid"] = wifi_data.credentials.valid;
    credentials["last_saved"] = wifi_data.credentials.last_saved;
    
    // Section config
    JsonObject config = doc.createNestedObject("config");
    config["auto_connect"] = wifi_data.config.auto_connect;
    config["reconnect_attempts"] = wifi_data.config.reconnect_attempts;
    config["connection_timeout"] = wifi_data.config.connection_timeout;
    
    // Écrire le JSON
    if (serializeJson(doc, file) == 0) {
        SERIAL_PRINTLN_MINIMAL("WiFi Persistence: Failed to write JSON");
        file.close();
        return SYS_ERROR;
    }
    
    file.close();
    
    SERIAL_PRINTF_MINIMAL("WiFi Persistence: Credentials saved successfully - SSID: %s\n", ssid);
    kernel_log(LOG_LEVEL_INFO, "WiFi Persistence: Credentials saved - SSID: %s", ssid);
    
    return SYS_OK;
}

// Charger les credentials WiFi
SysError_t wifi_persistence_load_credentials(void) {
    SERIAL_PRINTLN_MINIMAL("WiFi Persistence: Loading credentials from SPIFFS...");
    
    if (!SPIFFS.begin(true)) {
        SERIAL_PRINTLN_MINIMAL("WiFi Persistence: SPIFFS not available");
        return SYS_ERROR;
    }
    
    File file = SPIFFS.open(WIFI_CONFIG_FILE, FILE_READ);
    if (!file) {
        SERIAL_PRINTLN_MINIMAL("WiFi Persistence: No config file found");
        return SYS_NO_DATA;
    }
    
    // Vérifier si le fichier est vide
    if (file.size() == 0) {
        SERIAL_PRINTLN_MINIMAL("WiFi Persistence: Config file is empty");
        file.close();
        return SYS_NO_DATA;
    }
    
    // Parser le JSON
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        SERIAL_PRINTF_MINIMAL("WiFi Persistence: JSON parse error: %s\n", error.c_str());
        return SYS_ERROR;
    }
    
    // Vérifier la version
    const char* version = doc["version"] | "unknown";
    if (strcmp(version, WIFI_CONFIG_VERSION) != 0) {
        SERIAL_PRINTF_MINIMAL("WiFi Persistence: Version mismatch - found: %s, expected: %s\n", 
                             version, WIFI_CONFIG_VERSION);
        // Continuer quand même, mais log un avertissement
    }
    
    // Charger les credentials
    JsonObject credentials = doc["credentials"];
    if (credentials.isNull()) {
        SERIAL_PRINTLN_MINIMAL("WiFi Persistence: No credentials section found");
        return SYS_NO_DATA;
    }
    
    const char* ssid = credentials["ssid"] | "";
    const char* password = credentials["password"] | "";
    bool valid = credentials["valid"] | false;
    uint32_t last_saved = credentials["last_saved"] | 0;
    
    if (strlen(ssid) > 0 && strlen(password) > 0 && valid) {
        strncpy(wifi_data.credentials.ssid, ssid, sizeof(wifi_data.credentials.ssid) - 1);
        wifi_data.credentials.ssid[sizeof(wifi_data.credentials.ssid) - 1] = '\0';
        
        strncpy(wifi_data.credentials.password, password, sizeof(wifi_data.credentials.password) - 1);
        wifi_data.credentials.password[sizeof(wifi_data.credentials.password) - 1] = '\0';
        
        wifi_data.credentials.valid = true;
        wifi_data.credentials.last_saved = last_saved;
        
        // Mettre à jour les variables globales
        g_wifi_credentials = wifi_data.credentials;
        
        SERIAL_PRINTF_MINIMAL("WiFi Persistence: Credentials loaded - SSID: %s\n", ssid);
        kernel_log(LOG_LEVEL_INFO, "WiFi Persistence: Credentials loaded - SSID: %s", ssid);
        
        return SYS_OK;
    } else {
        SERIAL_PRINTLN_MINIMAL("WiFi Persistence: Invalid credentials in file");
        return SYS_NO_DATA;
    }
}

// Effacer les credentials WiFi
SysError_t wifi_persistence_clear_credentials(void) {
    SERIAL_PRINTLN_MINIMAL("WiFi Persistence: Clearing credentials...");
    
    // Effacer de la mémoire
    memset(&wifi_data.credentials, 0, sizeof(wifi_data.credentials));
    wifi_data.credentials.valid = false;
    g_wifi_credentials = wifi_data.credentials;
    
    // Supprimer le fichier
    if (SPIFFS.begin(true)) {
        if (SPIFFS.exists(WIFI_CONFIG_FILE)) {
            SPIFFS.remove(WIFI_CONFIG_FILE);
            SERIAL_PRINTLN_MINIMAL("WiFi Persistence: Config file removed");
        }
    }
    
    SERIAL_PRINTLN_MINIMAL("WiFi Persistence: Credentials cleared successfully");
    kernel_log(LOG_LEVEL_INFO, "WiFi Persistence: Credentials cleared");
    
    return SYS_OK;
}

// Sauvegarder la configuration WiFi
SysError_t wifi_persistence_save_config(const WifiPersistenceConfig_t* config) {
    if (!config) {
        return SYS_INVALID_PARAM;
    }
    
    // Mettre à jour la configuration
    wifi_data.config = *config;
    g_wifi_config = *config;
    
    // Sauvegarder le fichier complet
    return wifi_persistence_save_credentials(
        wifi_data.credentials.ssid, 
        wifi_data.credentials.password
    );
}

// Charger la configuration WiFi
SysError_t wifi_persistence_load_config(WifiPersistenceConfig_t* config) {
    if (!config) {
        return SYS_INVALID_PARAM;
    }
    
    // Charger depuis les données en mémoire
    *config = wifi_data.config;
    
    return SYS_OK;
}

// Obtenir le SSID stocké
const char* wifi_persistence_get_ssid(void) {
    return wifi_data.credentials.valid ? wifi_data.credentials.ssid : nullptr;
}

// Obtenir le mot de passe stocké
const char* wifi_persistence_get_password(void) {
    return wifi_data.credentials.valid ? wifi_data.credentials.password : nullptr;
}

// Vérifier si des credentials sont disponibles
bool wifi_persistence_has_credentials(void) {
    return wifi_data.credentials.valid;
}

// Obtenir les credentials complets
WifiCredentials_t* wifi_persistence_get_credentials(void) {
    return wifi_data.credentials.valid ? &wifi_data.credentials : nullptr;
}

// Obtenir la configuration
WifiPersistenceConfig_t* wifi_persistence_get_config(void) {
    return &wifi_data.config;
}

// Réinitialiser aux valeurs par défaut
SysError_t wifi_persistence_reset_to_defaults(void) {
    SERIAL_PRINTLN_MINIMAL("WiFi Persistence: Resetting to defaults...");
    
    // Réinitialiser les credentials
    memset(&wifi_data.credentials, 0, sizeof(wifi_data.credentials));
    wifi_data.credentials.valid = false;
    
    // Réinitialiser la configuration
    wifi_data.config.auto_connect = true;
    wifi_data.config.reconnect_attempts = 3;
    wifi_data.config.connection_timeout = 10000;
    
    // Mettre à jour les variables globales
    g_wifi_credentials = wifi_data.credentials;
    g_wifi_config = wifi_data.config;
    
    SERIAL_PRINTLN_MINIMAL("WiFi Persistence: Reset to defaults completed");
    return SYS_OK;
}

// Valider les credentials
bool wifi_persistence_validate_credentials(const char* ssid, const char* password) {
    if (!ssid || !password) {
        SERIAL_PRINTLN_MINIMAL("WiFi Persistence: Invalid credentials - null pointer");
        return false;
    }
    
    if (strlen(ssid) == 0 || strlen(ssid) > 31) {
        SERIAL_PRINTF_MINIMAL("WiFi Persistence: Invalid SSID length: %d (must be 1-31)\n", strlen(ssid));
        return false;
    }
    
    if (strlen(password) < 8 || strlen(password) > 63) {
        SERIAL_PRINTF_MINIMAL("WiFi Persistence: Invalid password length: %d (must be 8-63)\n", strlen(password));
        return false;
    }
    
    return true;
}
