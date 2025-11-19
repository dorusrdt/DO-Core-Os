#ifndef IRRIG_CLI_COMMANDS_H
#define IRRIG_CLI_COMMANDS_H

#include "../../kernel/core/kernel.h"

// ===== COMMANDES CLI POUR CONFIGURATION IRRIGATION =====

// Commande pour configurer l'URL du serveur FastAPI (Master → Serveur)
// Usage: irrig_config_server <url>
// Exemple: irrig_config_server http://192.168.1.100:8000
SysError_t cmd_irrig_config_server(int argc, char* argv[]);

// Note: Commandes irrig_config_master/slave1/slave2 supprimées
// Les MAC addresses sont maintenant codées en dur dans main.cpp

// Commande pour afficher la configuration actuelle
// Usage: irrig_config_show
SysError_t cmd_irrig_config_show(int argc, char* argv[]);

// Commande pour sauvegarder la configuration en NVS (Preferences)
// Usage: irrig_config_save
SysError_t cmd_irrig_config_save(int argc, char* argv[]);

// Commande pour charger la configuration depuis NVS
// Usage: irrig_config_load
SysError_t cmd_irrig_config_load(int argc, char* argv[]);

// Commande pour réinitialiser la configuration par défaut
// Usage: irrig_config_reset
SysError_t cmd_irrig_config_reset(int argc, char* argv[]);

// ===== COMMANDES DE GESTION DU RÔLE =====

// Commande pour définir le rôle du device
// Usage: irrig_set_role <master|slave1|slave2>
// Exemple: irrig_set_role master
SysError_t cmd_irrig_set_role(int argc, char* argv[]);

// Commande pour afficher le rôle actuel
// Usage: irrig_get_role
SysError_t cmd_irrig_get_role(int argc, char* argv[]);

// Commande pour activer l'app correspondant au rôle
// Usage: irrig_activate_role
SysError_t cmd_irrig_activate_role(int argc, char* argv[]);

// ===== FONCTIONS D'ACCÈS =====

// Obtenir l'URL du serveur FastAPI configurée
const char* irrig_cli_get_server_url(void);

// Vérifier si le serveur est configuré
bool irrig_cli_is_server_configured(void);

// Enregistrer les IDs des apps (appelé depuis main.cpp)
void irrig_cli_set_app_ids(uint8_t master_id, uint8_t slave1_id, uint8_t slave2_id);

// Obtenir le rôle actuel du device
typedef enum {
    DEVICE_ROLE_NONE = 0,
    DEVICE_ROLE_MASTER = 1,
    DEVICE_ROLE_SLAVE1 = 2,
    DEVICE_ROLE_SLAVE2 = 3
} DeviceRole_t;

DeviceRole_t irrig_cli_get_device_role(void);

// Activer automatiquement le rôle configuré (appelé au boot)
bool irrig_cli_auto_activate_role(void);

#endif // IRRIG_CLI_COMMANDS_H
