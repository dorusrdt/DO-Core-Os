#include "irrig_communication.h"
#include "../../kernel/core/log_system_optimized.h"
#include <esp_now.h>
#include <WiFi.h>

// ===== VARIABLES GLOBALES =====

static IrrigCommConfig_t g_comm_config;
static bool g_comm_initialized = false;
static uint32_t g_sequence_counter = 0;

// Callbacks
static SensorDataCallback_t g_sensor_callback = nullptr;
static IrrigationCommandCallback_t g_command_callback = nullptr;
static IrrigationStatusCallback_t g_status_callback = nullptr;

// Statistiques
static uint32_t g_sent_count = 0;
static uint32_t g_received_count = 0;
static uint32_t g_failed_count = 0;

// ===== CALLBACKS ESP-NOW =====

// Callback d'envoi (succès/échec)
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

    if (status == ESP_NOW_SEND_SUCCESS) {
        g_sent_count++;
        kernel_log(LOG_LEVEL_INFO, "✅ ESP-NOW: Data sent successfully to %s", mac_str);
    } else {
        g_failed_count++;
        kernel_log(LOG_LEVEL_WARN, "❌ ESP-NOW: Send failed to %s (status: %d)", mac_str, status);
        kernel_log(LOG_LEVEL_WARN, "   Possible causes: peer not found, wrong channel, or device offline");
    }
}

// Callback de réception
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int len) {
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

    if (len < sizeof(EspNowHeader_t)) {
        kernel_log(LOG_LEVEL_ERROR, "ESP-NOW: Invalid message length: %d from %s", len, mac_str);
        return;
    }

    EspNowHeader_t* header = (EspNowHeader_t*)data;
    g_received_count++;

    kernel_log(LOG_LEVEL_INFO, "📥 ESP-NOW: Received message from %s (type: %d, seq: %lu, len: %d)",
               mac_str, header->msg_type, header->sequence, len);

    // Traiter selon le type de message
    switch (header->msg_type) {
        case ESPNOW_MSG_SENSOR_DATA: {
            if (len < sizeof(EspNowHeader_t) + sizeof(SensorDataPacket_t)) {
                kernel_log(LOG_LEVEL_ERROR, "ESP-NOW: Invalid sensor data size");
                return;
            }
            SensorDataPacket_t* packet = (SensorDataPacket_t*)(data + sizeof(EspNowHeader_t));
            if (g_sensor_callback) {
                g_sensor_callback(packet);
            }
            break;
        }

        case ESPNOW_MSG_IRRIGATION_CMD: {
            if (len < sizeof(EspNowHeader_t) + sizeof(IrrigationCommandPacket_t)) {
                kernel_log(LOG_LEVEL_ERROR, "ESP-NOW: Invalid command size");
                return;
            }
            IrrigationCommandPacket_t* cmd = (IrrigationCommandPacket_t*)(data + sizeof(EspNowHeader_t));
            if (g_command_callback) {
                g_command_callback(cmd);
            }
            break;
        }

        case ESPNOW_MSG_IRRIGATION_STATUS: {
            if (len < sizeof(EspNowHeader_t) + sizeof(IrrigationStatusPacket_t)) {
                kernel_log(LOG_LEVEL_ERROR, "ESP-NOW: Invalid status size");
                return;
            }
            IrrigationStatusPacket_t* status = (IrrigationStatusPacket_t*)(data + sizeof(EspNowHeader_t));
            if (g_status_callback) {
                g_status_callback(status);
            }
            break;
        }

        default:
            kernel_log(LOG_LEVEL_WARN, "ESP-NOW: Unknown message type: %d", header->msg_type);
            break;
    }
}

// ===== INITIALISATION =====

SysError_t irrig_comm_init(IrrigCommConfig_t* config) {
    if (!config) {
        kernel_log(LOG_LEVEL_ERROR, "IrrigComm: Invalid config");
        return SYS_INVALID_PARAM;
    }

    memcpy(&g_comm_config, config, sizeof(IrrigCommConfig_t));

    // Initialiser WiFi en mode STA (requis pour ESP-NOW)
    // Ne pas déconnecter si déjà connecté, mais s'assurer que le mode est STA
    if (WiFi.getMode() != WIFI_STA) {
        WiFi.mode(WIFI_STA);
        delay(100);  // Laisser le temps au WiFi de changer de mode
    }

    // Obtenir MAC locale si non configurée
    if (g_comm_config.local_mac[0] == 0) {
        esp_read_mac(g_comm_config.local_mac, ESP_MAC_WIFI_STA);
    }

    // ===== CANAL WIFI FIXE POUR ESP-NOW =====
    // FORCER CANAL 1 sur tous les devices pour garantir la communication
    uint8_t wifi_channel = g_comm_config.wifi_channel;

    kernel_log(LOG_LEVEL_INFO, "=== ESP-NOW Channel Configuration ===");
    kernel_log(LOG_LEVEL_INFO, "WiFi Status: %s",
               WiFi.status() == WL_CONNECTED ? "CONNECTED" : "NOT CONNECTED");

    if (wifi_channel == 0) {
        // Forcer canal 1 pour tous les devices
        wifi_channel = 1;
        kernel_log(LOG_LEVEL_INFO, "Channel set to: 1 (FIXED for all devices)");
        if (WiFi.status() == WL_CONNECTED) {
            uint8_t wifi_ch = WiFi.channel();
            kernel_log(LOG_LEVEL_INFO, "WiFi Channel: %d (for server connection)", wifi_ch);
            kernel_log(LOG_LEVEL_INFO, "WiFi SSID: %s", WiFi.SSID().c_str());
            kernel_log(LOG_LEVEL_INFO, "Note: WiFi uses channel %d, ESP-NOW uses channel 1", wifi_ch);
        } else {
            kernel_log(LOG_LEVEL_INFO, "WiFi Channel: NOT CONNECTED");
        }
    } else {
        kernel_log(LOG_LEVEL_INFO, "Using configured channel: %d", wifi_channel);
    }

    kernel_log(LOG_LEVEL_INFO, "Final ESP-NOW Channel: %d (FIXED)", wifi_channel);
    kernel_log(LOG_LEVEL_INFO, "=====================================");

    g_comm_config.wifi_channel = wifi_channel;

    kernel_log(LOG_LEVEL_INFO, "IrrigComm: ESP-NOW mode initializing...");
    kernel_log(LOG_LEVEL_INFO, "  Local MAC: %02X:%02X:%02X:%02X:%02X:%02X",
               g_comm_config.local_mac[0], g_comm_config.local_mac[1],
               g_comm_config.local_mac[2], g_comm_config.local_mac[3],
               g_comm_config.local_mac[4], g_comm_config.local_mac[5]);
    kernel_log(LOG_LEVEL_INFO, "  Master MAC: %02X:%02X:%02X:%02X:%02X:%02X",
               g_comm_config.master_mac[0], g_comm_config.master_mac[1],
               g_comm_config.master_mac[2], g_comm_config.master_mac[3],
               g_comm_config.master_mac[4], g_comm_config.master_mac[5]);
    kernel_log(LOG_LEVEL_INFO, "  Slave1 MAC: %02X:%02X:%02X:%02X:%02X:%02X",
               g_comm_config.slave1_mac[0], g_comm_config.slave1_mac[1],
               g_comm_config.slave1_mac[2], g_comm_config.slave1_mac[3],
               g_comm_config.slave1_mac[4], g_comm_config.slave1_mac[5]);
    kernel_log(LOG_LEVEL_INFO, "  Slave2 MAC: %02X:%02X:%02X:%02X:%02X:%02X",
               g_comm_config.slave2_mac[0], g_comm_config.slave2_mac[1],
               g_comm_config.slave2_mac[2], g_comm_config.slave2_mac[3],
               g_comm_config.slave2_mac[4], g_comm_config.slave2_mac[5]);

    // Initialiser ESP-NOW
    // Si déjà initialisé, le déinitialiser d'abord
    esp_err_t init_result = esp_now_init();
    if (init_result == ESP_ERR_ESPNOW_NOT_INIT) {
        // Pas encore initialisé, c'est bon
    } else if (init_result == ESP_ERR_ESPNOW_INTERNAL) {
        // Déjà initialisé, déinitialiser d'abord
        esp_now_deinit();
        delay(100);
        init_result = esp_now_init();
    }

    if (init_result != ESP_OK) {
        kernel_log(LOG_LEVEL_ERROR, "IrrigComm: ESP-NOW init failed (error: %d)", init_result);
        return SYS_ERROR;
    }

    // Enregistrer callbacks
    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);

    kernel_log(LOG_LEVEL_INFO, "IrrigComm: ESP-NOW initialized, using channel %d", g_comm_config.wifi_channel);

    // Ajouter peers (tous les devices peuvent communiquer entre eux)
    esp_now_peer_info_t peer;
    peer.channel = g_comm_config.wifi_channel;  // Utiliser le canal déterminé
    peer.encrypt = false;
    peer.ifidx = WIFI_IF_STA;

    // Ajouter Master (si différent de local)
    char master_mac_str[18];
    snprintf(master_mac_str, sizeof(master_mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             g_comm_config.master_mac[0], g_comm_config.master_mac[1],
             g_comm_config.master_mac[2], g_comm_config.master_mac[3],
             g_comm_config.master_mac[4], g_comm_config.master_mac[5]);

    if (memcmp(g_comm_config.master_mac, g_comm_config.local_mac, 6) != 0) {
        memcpy(peer.peer_addr, g_comm_config.master_mac, 6);
        esp_err_t add_result = esp_now_add_peer(&peer);
        if (add_result != ESP_OK) {
            kernel_log(LOG_LEVEL_WARN, "IrrigComm: Failed to add Master peer %s (error: %d)", master_mac_str, add_result);
        } else {
            kernel_log(LOG_LEVEL_INFO, "✅ IrrigComm: Master peer %s added (channel %d)", master_mac_str, g_comm_config.wifi_channel);
        }
    } else {
        kernel_log(LOG_LEVEL_INFO, "IrrigComm: Master %s is local device", master_mac_str);
    }

    // Ajouter Slave1 (si différent de local)
    char slave1_mac_str[18];
    snprintf(slave1_mac_str, sizeof(slave1_mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             g_comm_config.slave1_mac[0], g_comm_config.slave1_mac[1],
             g_comm_config.slave1_mac[2], g_comm_config.slave1_mac[3],
             g_comm_config.slave1_mac[4], g_comm_config.slave1_mac[5]);

    if (memcmp(g_comm_config.slave1_mac, g_comm_config.local_mac, 6) != 0) {
        memcpy(peer.peer_addr, g_comm_config.slave1_mac, 6);
        esp_err_t add_result = esp_now_add_peer(&peer);
        if (add_result != ESP_OK) {
            kernel_log(LOG_LEVEL_WARN, "IrrigComm: Failed to add Slave1 peer %s (error: %d)", slave1_mac_str, add_result);
        } else {
            kernel_log(LOG_LEVEL_INFO, "✅ IrrigComm: Slave1 peer %s added (channel %d)", slave1_mac_str, g_comm_config.wifi_channel);
        }
    } else {
        kernel_log(LOG_LEVEL_INFO, "IrrigComm: Slave1 %s is local device", slave1_mac_str);
    }

    // Ajouter Slave2 (si différent de local)
    if (memcmp(g_comm_config.slave2_mac, g_comm_config.local_mac, 6) != 0) {
        memcpy(peer.peer_addr, g_comm_config.slave2_mac, 6);
        esp_err_t add_result = esp_now_add_peer(&peer);
        if (add_result != ESP_OK) {
            kernel_log(LOG_LEVEL_WARN, "IrrigComm: Failed to add Slave2 peer (error: %d)", add_result);
        } else {
            kernel_log(LOG_LEVEL_INFO, "IrrigComm: Slave2 peer added (channel %d)", g_comm_config.wifi_channel);
        }
    } else {
        kernel_log(LOG_LEVEL_INFO, "IrrigComm: Slave2 is local device");
    }

    g_comm_initialized = true;
    kernel_log(LOG_LEVEL_INFO, "IrrigComm: ESP-NOW initialized successfully");
    kernel_log(LOG_LEVEL_INFO, "  Retry: %d, Delay: %dms",
               g_comm_config.retry_count, g_comm_config.retry_delay_ms);

    return SYS_OK;
}

IrrigCommConfig_t* irrig_comm_get_config(void) {
    return &g_comm_config;
}

// ===== FONCTION UTILITAIRE : ENVOI MESSAGE =====

static bool send_espnow_message(const uint8_t* target_mac, EspNowMessageType_t msg_type, const void* payload, size_t payload_size) {
    if (!g_comm_initialized) {
        kernel_log(LOG_LEVEL_ERROR, "IrrigComm: Not initialized");
        return false;
    }

    // Créer buffer message
    size_t total_size = sizeof(EspNowHeader_t) + payload_size;
    uint8_t* buffer = (uint8_t*)malloc(total_size);
    if (!buffer) {
        kernel_log(LOG_LEVEL_ERROR, "IrrigComm: Memory allocation failed");
        return false;
    }

    // Remplir en-tête
    EspNowHeader_t* header = (EspNowHeader_t*)buffer;
    header->msg_type = msg_type;
    header->sequence = ++g_sequence_counter;

    // Copier payload
    memcpy(buffer + sizeof(EspNowHeader_t), payload, payload_size);

    // Vérifier que le peer existe, sinon l'ajouter
    esp_now_peer_info_t peer_info;
    esp_err_t get_peer_result = esp_now_get_peer(target_mac, &peer_info);

    char target_mac_str[18];
    snprintf(target_mac_str, sizeof(target_mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             target_mac[0], target_mac[1], target_mac[2], target_mac[3], target_mac[4], target_mac[5]);

    if (get_peer_result != ESP_OK) {
        kernel_log(LOG_LEVEL_WARN, "ESP-NOW: Peer %s not found, adding peer...", target_mac_str);

        // Ajouter le peer dynamiquement
        esp_now_peer_info_t new_peer;
        memcpy(new_peer.peer_addr, target_mac, 6);
        new_peer.channel = g_comm_config.wifi_channel;
        new_peer.encrypt = false;
        new_peer.ifidx = WIFI_IF_STA;

        esp_err_t add_result = esp_now_add_peer(&new_peer);
        if (add_result != ESP_OK) {
            kernel_log(LOG_LEVEL_ERROR, "ESP-NOW: Failed to add peer %s (error: %d)", target_mac_str, add_result);
            free(buffer);
            return false;
        }
        kernel_log(LOG_LEVEL_INFO, "ESP-NOW: Peer %s added successfully (channel %d)",
                   target_mac_str, g_comm_config.wifi_channel);
    } else {
        kernel_log(LOG_LEVEL_DEBUG, "ESP-NOW: Peer %s found (channel %d)",
                   target_mac_str, peer_info.channel);
    }

    // Envoyer avec retry
    kernel_log(LOG_LEVEL_INFO, "📤 ESP-NOW: Sending message to %s (type: %d, size: %d bytes, channel: %d)",
               target_mac_str, msg_type, total_size, g_comm_config.wifi_channel);

    bool success = false;
    for (uint8_t attempt = 0; attempt < g_comm_config.retry_count; attempt++) {
        if (attempt > 0) {
            kernel_log(LOG_LEVEL_INFO, "🔄 ESP-NOW: Retry attempt %d/%d",
                       attempt + 1, g_comm_config.retry_count);
            delay(g_comm_config.retry_delay_ms);
        }

        esp_err_t result = esp_now_send(target_mac, buffer, total_size);

        if (result == ESP_OK) {
            // Attendre un peu pour que le callback soit appelé
            delay(50);  // Augmenté de 10ms à 50ms pour laisser le temps au callback
            // Le callback OnDataSent indiquera si c'est un succès ou échec
            // On considère comme succès si le callback n'a pas signalé d'échec
            success = true;
            kernel_log(LOG_LEVEL_DEBUG, "ESP-NOW: Send call returned ESP_OK, waiting for callback...");
            break;
        } else {
            kernel_log(LOG_LEVEL_WARN, "ESP-NOW: Send call failed with error %d (attempt %d/%d)",
                       result, attempt + 1, g_comm_config.retry_count);
        }
    }

    free(buffer);

    if (!success) {
        kernel_log(LOG_LEVEL_ERROR, "ESP-NOW: Failed to send after %d attempts",
                   g_comm_config.retry_count);
        g_failed_count++;
    }

    return success;
}

// ===== SLAVE 1 → MASTER : PUBLIER DONNÉES CAPTEURS =====

bool irrig_comm_publish_sensor_data(SensorDataPacket_t* data) {
    if (!g_comm_initialized || !data) {
        kernel_log(LOG_LEVEL_ERROR, "IrrigComm: Not initialized or invalid data");
        return false;
    }

    char master_mac_str[18];
    snprintf(master_mac_str, sizeof(master_mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             g_comm_config.master_mac[0], g_comm_config.master_mac[1],
             g_comm_config.master_mac[2], g_comm_config.master_mac[3],
             g_comm_config.master_mac[4], g_comm_config.master_mac[5]);

    kernel_log(LOG_LEVEL_INFO, "📤 Slave1 → Master: Sending sensor data to %s", master_mac_str);

    return send_espnow_message(g_comm_config.master_mac,
                                ESPNOW_MSG_SENSOR_DATA,
                                data,
                                sizeof(SensorDataPacket_t));
}

// ===== MASTER → SLAVE 2 : ENVOYER COMMANDE IRRIGATION =====

bool irrig_comm_send_irrigation_command(IrrigationCommandPacket_t* cmd) {
    if (!g_comm_initialized || !cmd) {
        kernel_log(LOG_LEVEL_ERROR, "IrrigComm: Not initialized or invalid command");
        return false;
    }

    const char* cmd_str = "";
    switch (cmd->command) {
        case CMD_START_IRRIGATION: cmd_str = "START_IRRIGATION"; break;
        case CMD_STOP_IRRIGATION: cmd_str = "STOP_IRRIGATION"; break;
        case CMD_EMERGENCY_STOP: cmd_str = "EMERGENCY_STOP"; break;
        case CMD_TEST_RELAY: cmd_str = "TEST_RELAY"; break;
        case CMD_STATUS_REQUEST: cmd_str = "STATUS_REQUEST"; break;
        default: cmd_str = "UNKNOWN"; break;
    }

    kernel_log(LOG_LEVEL_INFO, "📤 Master → Slave2: Sending '%s' via ESP-NOW", cmd_str);
    kernel_log(LOG_LEVEL_DEBUG, "   Zone: %d, Duration: %ds",
               cmd->zone_id, cmd->duration_seconds);

    return send_espnow_message(g_comm_config.slave2_mac,
                                ESPNOW_MSG_IRRIGATION_CMD,
                                cmd,
                                sizeof(IrrigationCommandPacket_t));
}

// ===== SLAVE 2 → MASTER : PUBLIER STATUT IRRIGATION =====

bool irrig_comm_publish_irrigation_status(IrrigationStatusPacket_t* status) {
    if (!g_comm_initialized || !status) {
        kernel_log(LOG_LEVEL_ERROR, "IrrigComm: Not initialized or invalid status");
        return false;
    }

    kernel_log(LOG_LEVEL_DEBUG, "ESP-NOW: Sending irrigation status to Master");
    kernel_log(LOG_LEVEL_DEBUG, "   Zone: %d, Irrigating: %s, Remaining: %lus",
               status->zone_id,
               status->is_irrigating ? "YES" : "NO",
               status->remaining_seconds);

    return send_espnow_message(g_comm_config.master_mac,
                                ESPNOW_MSG_IRRIGATION_STATUS,
                                status,
                                sizeof(IrrigationStatusPacket_t));
}

// ===== GESTION CALLBACKS =====

void irrig_comm_set_sensor_callback(SensorDataCallback_t callback) {
    g_sensor_callback = callback;
}

void irrig_comm_set_command_callback(IrrigationCommandCallback_t callback) {
    g_command_callback = callback;
}

void irrig_comm_set_status_callback(IrrigationStatusCallback_t callback) {
    g_status_callback = callback;
}

// ===== UTILITAIRES =====

void irrig_comm_get_local_mac(uint8_t* mac) {
    if (mac && g_comm_initialized) {
        memcpy(mac, g_comm_config.local_mac, 6);
    } else if (mac) {
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
    }
}
