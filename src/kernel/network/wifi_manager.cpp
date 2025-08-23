#include "wifi_manager.h"
#include <WiFi.h>

static WifiConfig_t g_wifi_config;
static WifiStatus_t g_wifi_status = WIFI_STATUS_DISCONNECTED;
static void (*g_event_callback)(WifiStatus_t status) = nullptr;

// Gestionnaire d'événements WiFi
void WiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case SYSTEM_EVENT_STA_CONNECTED:
            g_wifi_status = WIFI_STATUS_CONNECTED;
            if (g_event_callback) g_event_callback(g_wifi_status);
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            g_wifi_status = WIFI_STATUS_DISCONNECTED;
            if (g_event_callback) g_event_callback(g_wifi_status);
            break;
        default:
            break;
    }
}

void wifi_manager_init(const WifiConfig_t* config) {
    Serial.println("WiFi Manager: Starting initialization...");
    
    if (config) {
        g_wifi_config = *config;
        Serial.printf("WiFi Manager: Config loaded - SSID: %s, Mode: %d\n", 
                     g_wifi_config.ssid.c_str(), g_wifi_config.mode);
    } else {
        Serial.println("WiFi Manager: Using default config");
        // Configuration par défaut sécurisée
        g_wifi_config.ssid = "DO-Core-Default";
        g_wifi_config.password = "12345678";
        g_wifi_config.mode = WIFI_MODE_AP;
    }
    
    // Vérifications de sécurité
    if (g_wifi_config.ssid.length() == 0) {
        Serial.println("WiFi Manager: WARNING - Empty SSID, using default");
        g_wifi_config.ssid = "DO-Core-Default";
    }
    
    if (g_wifi_config.password.length() < 8) {
        Serial.println("WiFi Manager: WARNING - Password too short, using default");
        g_wifi_config.password = "12345678";
    }
    
    Serial.println("WiFi Manager: Disconnecting any existing connections...");
    WiFi.disconnect(true);
    
    Serial.println("WiFi Manager: Setting up event handler...");
    WiFi.onEvent(WiFiEvent);
    
    // SUPPRIMÉ: Restriction de sécurité qui forçait le mode NULL
    // Maintenant on permet tous les modes WiFi
    
    Serial.println("WiFi Manager: Setting WiFi mode...");
    wifi_manager_set_mode(g_wifi_config.mode);
    
    Serial.println("WiFi Manager: Initialization complete");
}

WifiStatus_t wifi_manager_connect() {
    if (g_wifi_config.mode == WIFI_MODE_STA) {
        WiFi.begin(g_wifi_config.ssid.c_str(), g_wifi_config.password.c_str());
        g_wifi_status = WIFI_STATUS_CONNECTING;
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
            delay(500);
        }
        if (WiFi.status() == WL_CONNECTED) {
            g_wifi_status = WIFI_STATUS_CONNECTED;
        } else {
            g_wifi_status = WIFI_STATUS_FAILED;
        }
    }
    if (g_event_callback) g_event_callback(g_wifi_status);
    return g_wifi_status;
}

void wifi_manager_disconnect() {
    WiFi.disconnect();
    g_wifi_status = WIFI_STATUS_DISCONNECTED;
    if (g_event_callback) g_event_callback(g_wifi_status);
}

WifiStatus_t wifi_manager_get_status() {
    return g_wifi_status;
}

void wifi_manager_set_mode(wifi_mode_t mode) {
    g_wifi_config.mode = mode;
    Serial.printf("WiFi Manager: Setting mode to %d\n", mode);
    
    switch (mode) {
        case WIFI_MODE_STA:
            Serial.println("WiFi Manager: Setting STA mode");
            WiFi.mode(WIFI_MODE_STA);
            g_wifi_status = WIFI_STATUS_DISCONNECTED;
            if (g_event_callback) g_event_callback(g_wifi_status);
            break;
        case WIFI_MODE_AP:
            Serial.println("WiFi Manager: Setting AP mode");
            WiFi.mode(WIFI_MODE_AP);
            // Vérifications de sécurité avant de créer l'AP
            if (g_wifi_config.ssid.length() > 0 && g_wifi_config.password.length() >= 8) {
                Serial.printf("WiFi Manager: Creating AP with SSID: %s\n", g_wifi_config.ssid.c_str());
                WiFi.softAP(g_wifi_config.ssid.c_str(), g_wifi_config.password.c_str());
                g_wifi_status = WIFI_STATUS_CONNECTED;
                if (g_event_callback) g_event_callback(g_wifi_status);
            } else {
                Serial.println("WiFi Manager: ERROR - Invalid AP credentials");
                g_wifi_status = WIFI_STATUS_FAILED;
            }
            break;
        case WIFI_MODE_APSTA:
            Serial.println("WiFi Manager: Setting APSTA mode");
            WiFi.mode(WIFI_MODE_APSTA);
            g_wifi_status = WIFI_STATUS_DISCONNECTED;
            if (g_event_callback) g_event_callback(g_wifi_status);
            break;
        case WIFI_MODE_NULL:
        default:
            Serial.println("WiFi Manager: Setting NULL mode");
            WiFi.mode(WIFI_MODE_NULL);
            g_wifi_status = WIFI_STATUS_DISCONNECTED;
            if (g_event_callback) g_event_callback(g_wifi_status);
            break;
    }
}

void wifi_manager_set_event_callback(void (*callback)(WifiStatus_t status)) {
    g_event_callback = callback;
}
