#ifndef HEARTBEAT_LED_H
#define HEARTBEAT_LED_H

#include "../core/kernel.h"
#include <Arduino.h>

// ===== ÉTATS DU HEARTBEAT =====

typedef enum {
    HEARTBEAT_OFF = 0,          // LED éteinte (système arrêté)
    HEARTBEAT_BOOTING,          // Clignotement rapide (100ms) - Démarrage
    HEARTBEAT_READY,            // Clignotement lent (1s) - Système prêt, pas d'app
    HEARTBEAT_RUNNING,          // Double pulse (200ms) - App active
    HEARTBEAT_WIFI_ERROR,       // Clignotement très rapide (50ms) - Pas de WiFi
    HEARTBEAT_ERROR             // LED fixe ON - Erreur système
} HeartbeatState_t;

// ===== FONCTIONS PUBLIQUES =====

/**
 * @brief Initialise le module heartbeat LED
 * @return SYS_OK si succès, SYS_ERROR sinon
 */
SysError_t heartbeat_init(void);

/**
 * @brief Définit l'état du heartbeat
 * @param state Nouvel état du heartbeat
 */
void heartbeat_set_state(HeartbeatState_t state);

/**
 * @brief Obtient l'état actuel du heartbeat
 * @return État actuel
 */
HeartbeatState_t heartbeat_get_state(void);

/**
 * @brief Tâche FreeRTOS pour gérer le heartbeat
 * @param params Paramètres de la tâche (non utilisé)
 */
void heartbeat_task(void* params);

/**
 * @brief Arrête le heartbeat (LED OFF)
 */
void heartbeat_stop(void);

#endif // HEARTBEAT_LED_H
