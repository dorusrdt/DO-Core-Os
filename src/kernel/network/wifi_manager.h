#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <esp_wifi_types.h> // Use ESP32 SDK wifi_mode_t

#ifdef __cplusplus
#include <WiFi.h>
#endif

// Enumération des états de connexion
typedef enum {
    WIFI_STATUS_DISCONNECTED = 0,
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_FAILED
} WifiStatus_t;

// Structure de configuration WiFi
typedef struct {
    String ssid;
    String password;
    wifi_mode_t mode; // Use ESP32 SDK type
} WifiConfig_t;

#ifdef __cplusplus
extern "C" {
#endif

// Initialisation du WiFi Manager
void wifi_manager_init(const WifiConfig_t* config);

// Connexion au WiFi
WifiStatus_t wifi_manager_connect();

// Déconnexion du WiFi
void wifi_manager_disconnect();

// Obtenir le statut courant
WifiStatus_t wifi_manager_get_status();

// Changer de mode (AP/STA)
void wifi_manager_set_mode(wifi_mode_t mode); // Use ESP32 SDK type

// Gestion des callbacks d'événements
void wifi_manager_set_event_callback(void (*callback)(WifiStatus_t status));

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H 