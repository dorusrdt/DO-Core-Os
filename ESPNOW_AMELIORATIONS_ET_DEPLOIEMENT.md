# 🚀 ESP-NOW : Améliorations et Guide de Déploiement

## 📋 Table des matières
1. [Améliorations proposées](#améliorations-proposées)
2. [Guide de déploiement Master](#guide-de-déploiement-master)
3. [Guide de déploiement Slave](#guide-de-déploiement-slave)
4. [Scénarios de déploiement](#scénarios-de-déploiement)
5. [Commandes CLI utiles](#commandes-cli-utiles)

---

## 🔧 Améliorations proposées

### 1. **Scan intelligent avec priorité au canal connu**

**Problème actuel :**
- Le slave scanne toujours depuis le canal 1, même s'il connaît déjà le canal du master
- Perte de temps si le master est sur le canal 13

**Solution :**
```cpp
// Dans discovery_task() du slave
if (!master_info.master_found) {
    // 1. Essayer d'abord le canal sauvegardé en NVS
    if (master_info.channel > 0 && master_info.channel <= 13) {
        esp_wifi_set_channel(master_info.channel, WIFI_SECOND_CHAN_NONE);
        vTaskDelay(pdMS_TO_TICKS(500));  // Attendre 2 beacons
        if (master_info.master_found) {
            // Trouvé ! Pas besoin de scanner les autres canaux
            continue;
        }
    }

    // 2. Si pas trouvé, scan complet
    for (uint8_t ch = 1; ch <= 13 && !master_info.master_found; ++ch) {
        esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
```

**Bénéfice :** Découverte 10x plus rapide si le master est connu (500ms vs 2.6s)

---

### 2. **Timeout adaptatif selon la qualité du signal**

**Problème actuel :**
- Timeout fixe de 5s pour le slave, 10s pour le master
- Trop court si le signal est faible, trop long si le signal est fort

**Solution :**
```cpp
// Calculer timeout basé sur RSSI
static TickType_t calculate_timeout(int8_t rssi) {
    if (rssi > -50) {
        return pdMS_TO_TICKS(3000);  // Signal excellent : 3s
    } else if (rssi > -70) {
        return pdMS_TO_TICKS(5000);  // Signal bon : 5s
    } else if (rssi > -85) {
        return pdMS_TO_TICKS(10000); // Signal faible : 10s
    } else {
        return pdMS_TO_TICKS(15000); // Signal très faible : 15s
    }
}
```

**Bénéfice :** Moins de fausses déconnexions, meilleure robustesse

---

### 3. **Mécanisme ACK pour confirmation de réception**

**Problème actuel :**
- Le slave envoie des données mais ne sait pas si le master les a reçues
- Pas de retry en cas d'échec

**Solution :**
```cpp
// Dans espnow_master.cpp - on_data_recv()
static void on_data_recv(const uint8_t* mac_addr, const uint8_t* data, int len) {
    // ... validation existante ...

    // Envoyer ACK
    AckPacket_t ack{};
    ack.frame_type = FRAME_ACK;
    ack.version = 1;
    ack.slave_id = pkt.slave_id;
    ack.seq = pkt.seq;

    esp_now_send(mac_addr, (uint8_t*)&ack, sizeof(ack));
}

// Dans espnow_slave.cpp - send_task()
static uint32_t pending_seq = 0;
static TickType_t ack_timeout = 0;

// Envoyer data
esp_now_send(master_info.mac, (uint8_t*)&pkt, sizeof(pkt));
pending_seq = pkt.seq;
ack_timeout = xTaskGetTickCount() + pdMS_TO_TICKS(1000);

// Dans on_data_recv() du slave
if (ack->frame_type == FRAME_ACK && ack->seq == pending_seq) {
    // ACK reçu ! Données bien arrivées
    pending_seq = 0;
} else if (xTaskGetTickCount() > ack_timeout) {
    // Timeout ACK - retry
    consecutive_send_failures++;
}
```

**Bénéfice :** Fiabilité accrue, détection des pertes de paquets

---

### 4. **File d'attente HTTP avec retry pour l'envoi au serveur**

**Problème actuel :**
- Le master reçoit les données mais ne les envoie pas au serveur
- Pas de gestion d'erreur si le serveur est down

**Solution :**
```cpp
// Queue pour stocker les paquets à envoyer
#define HTTP_QUEUE_SIZE 50
static QueueHandle_t http_queue = nullptr;

// Dans on_data_recv() du master
static void on_data_recv(const uint8_t* mac_addr, const uint8_t* data, int len) {
    // ... validation existante ...

    // Ajouter à la queue HTTP
    DataPacket_t* pkt_copy = (DataPacket_t*)pvPortMalloc(sizeof(DataPacket_t));
    if (pkt_copy) {
        memcpy(pkt_copy, &pkt, sizeof(DataPacket_t));
        if (xQueueSend(http_queue, &pkt_copy, 0) != pdTRUE) {
            // Queue pleine - libérer la mémoire
            vPortFree(pkt_copy);
            kernel_log(LOG_LEVEL_WARN, "HTTP queue full - packet dropped");
        }
    }
}

// Tâche HTTP dédiée
static void http_send_task(void* pv) {
    http_queue = xQueueCreate(HTTP_QUEUE_SIZE, sizeof(DataPacket_t*));

    for (;;) {
        DataPacket_t* pkt = nullptr;
        if (xQueueReceive(http_queue, &pkt, pdMS_TO_TICKS(1000)) == pdTRUE) {
            // Construire JSON
            char json[512];
            snprintf(json, sizeof(json),
                "{\"seq\":%lu,\"timestamp\":%lu,\"humidity\":[%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u]}",
                pkt->seq, pkt->timestamp,
                pkt->humidity[0], pkt->humidity[1], pkt->humidity[2], pkt->humidity[3],
                pkt->humidity[4], pkt->humidity[5], pkt->humidity[6], pkt->humidity[7],
                pkt->humidity[8], pkt->humidity[9], pkt->humidity[10], pkt->humidity[11]);

            // Envoyer HTTP POST avec retry
            bool success = false;
            for (int retry = 0; retry < 3 && !success; retry++) {
                HttpResponse_t response = http_client.post("/api/data", json, "Content-Type: application/json");
                if (response.status_code == 200) {
                    success = true;
                    kernel_log(LOG_LEVEL_INFO, "HTTP: Data sent - seq=%lu", pkt->seq);
                } else {
                    vTaskDelay(pdMS_TO_TICKS(1000 * (retry + 1)));  // Backoff exponentiel
                }
            }

            if (!success) {
                kernel_log(LOG_LEVEL_WARN, "HTTP: Failed to send seq=%lu after 3 retries", pkt->seq);
                // Optionnel : sauvegarder en NVS pour envoi ultérieur
            }

            vPortFree(pkt);
        }
    }
}
```

**Bénéfice :** Pas de perte de données, retry automatique, robustesse

---

### 5. **Validation de séquence pour détecter les pertes**

**Problème actuel :**
- Pas de détection de paquets perdus ou dupliqués
- `last_seq` stocké mais non utilisé

**Solution :**
```cpp
// Dans SlaveInfo_t
typedef struct {
    uint8_t mac[6];
    uint16_t slave_id;
    uint32_t last_seq;
    uint32_t expected_seq;      // ← NOUVEAU
    uint32_t lost_packets;      // ← NOUVEAU
    uint32_t duplicate_packets; // ← NOUVEAU
    TickType_t last_seen_ticks;
    bool is_active;
} SlaveInfo_t;

// Dans on_data_recv() du master
if (pkt.seq == slaves[i].expected_seq) {
    // Séquence correcte
    slaves[i].expected_seq = pkt.seq + 1;
} else if (pkt.seq < slaves[i].expected_seq) {
    // Paquet en retard ou dupliqué
    slaves[i].duplicate_packets++;
    kernel_log(LOG_LEVEL_WARN, "Duplicate packet: seq=%lu (expected=%lu)",
              pkt.seq, slaves[i].expected_seq);
} else {
    // Paquets perdus
    uint32_t lost = pkt.seq - slaves[i].expected_seq;
    slaves[i].lost_packets += lost;
    slaves[i].expected_seq = pkt.seq + 1;
    kernel_log(LOG_LEVEL_WARN, "Lost %lu packets: expected=%lu, got=%lu",
              lost, slaves[i].expected_seq - lost - 1, pkt.seq);
}
```

**Bénéfice :** Monitoring de la qualité de la connexion, statistiques

---

### 6. **Gestion dynamique du canal WiFi (ESP-NOW suit le WiFi)**

**⚠️ Important :** ESP-NOW utilise **toujours le même canal** que l'interface WiFi active (STA ou AP). Si le WiFi change de canal, ESP-NOW change aussi automatiquement.

**Problème actuel :**
- Le master annonce son canal dans le beacon, mais si le WiFi change de canal, les slaves peuvent perdre le master
- Le slave doit détecter les changements de canal du master

**Solution :**
```cpp
// Dans beacon_task() du master - DÉJÀ CORRECT dans votre code
static void beacon_task(void* pv) {
    for (;;) {
        // Lire le canal WiFi actuel (ESP-NOW utilisera ce canal)
        uint8_t primary = 0;
        wifi_second_chan_t secondary;
        if (WiFi.status() == WL_CONNECTED) {
            esp_wifi_get_channel(&primary, &secondary);
        }
        uint8_t tx_channel = (primary >= 1 && primary <= 13) ? primary : 6;

        // Annoncer le canal actuel dans le beacon
        beacon.channel = tx_channel;

        // ... reste du code ...
    }
}

// Dans discovery_task() du slave - AMÉLIORATION
static void discovery_task(void* pv) {
    uint8_t last_known_channel = 0;

    for (;;) {
        if (master_info.master_found) {
            // Vérifier si le canal a changé
            if (master_info.channel != last_known_channel) {
                kernel_log(LOG_LEVEL_INFO, "Master channel changed: %d → %d",
                          last_known_channel, master_info.channel);

                // S'aligner immédiatement sur le nouveau canal
                esp_wifi_set_channel(master_info.channel, WIFI_SECOND_CHAN_NONE);
                last_known_channel = master_info.channel;
            }

            // Vérifier timeout (avec marge si canal change)
            TickType_t now = xTaskGetTickCount();
            TickType_t timeout = pdMS_TO_TICKS(5000);

            // Si le canal vient de changer, donner plus de temps
            if ((now - master_info.last_beacon_ticks) > timeout) {
                master_info.master_found = false;
                kernel_log(LOG_LEVEL_WARN, "Master lost - no beacon for 5s");
            }
        }
        // ... reste du code ...
    }
}
```

**Bénéfice :**
- Gestion correcte des changements de canal WiFi
- Le slave suit automatiquement les changements de canal du master
- Pas de perte de connexion lors des changements de canal WiFi

---

### ⚠️ **Note importante sur le canal ESP-NOW**

**Comportement ESP32 :** ESP-NOW utilise **automatiquement le même canal** que l'interface WiFi active :
- Si WiFi STA connecté → ESP-NOW utilise le canal du réseau WiFi
- Si WiFi AP actif → ESP-NOW utilise le canal de l'AP
- Si WiFi déconnecté → ESP-NOW utilise le canal par défaut (généralement 6)

**Conséquences :**
1. **Ne pas coder le canal en dur** - Il change automatiquement avec le WiFi
2. **Le master doit annoncer son canal** - Dans chaque beacon (déjà fait)
3. **Le slave doit suivre les changements** - S'aligner quand le canal change
4. **Gestion des reconnexions** - Si le WiFi change de canal, les slaves doivent se réaligner

**Votre code actuel gère déjà cela correctement** dans `beacon_task()` :
```cpp
// Lit le canal WiFi actuel
if (WiFi.status() == WL_CONNECTED) {
    esp_wifi_get_channel(&primary, &secondary);
}
uint8_t tx_channel = (primary >= 1 && primary <= 13) ? primary : 6;
beacon.channel = tx_channel;  // Annonce le canal actuel
```

---

### 7. **Heartbeat bidirectionnel**

**Problème actuel :**
- Pas de heartbeat si le slave n'a pas de données à envoyer
- Détection de panne lente

**Solution :**
```cpp
// Dans send_task() du slave
// Envoyer heartbeat même sans données
if (data_seq % 10 == 0) {  // Toutes les 20s (10 * 2s)
    // Envoyer packet heartbeat vide
    DataPacket_t heartbeat{};
    heartbeat.frame_type = FRAME_DATA;
    heartbeat.version = 1;
    heartbeat.slave_id = device_config.device_id;
    heartbeat.master_id = device_config.master_id;
    heartbeat.seq = data_seq++;
    heartbeat.timestamp = (uint32_t)millis();
    // humidity reste à 0 (heartbeat)

    esp_now_send(master_info.mac, (uint8_t*)&heartbeat, sizeof(heartbeat));
}
```

**Bénéfice :** Détection de panne plus rapide

---

## 📦 Guide de Déploiement Master

### Étape 1 : Préparation du firmware

Le firmware est **identique** pour master et slave. La différence vient de la **configuration**.

### Étape 2 : Configuration initiale du Master

#### Option A : Via CLI (recommandé)

1. **Compiler et uploader le firmware** sur l'ESP32 master
2. **Se connecter via Serial Monitor** (115200 baud)
3. **Configurer le WiFi** (si nécessaire) :
   ```
   wifi_save MonWiFi MonMotDePasse
   ```
4. **Vérifier la configuration actuelle** :
   ```
   espnow config show
   ```
   Devrait afficher :
   ```
   Device ID:      0xXXXX (basé sur MAC)
   Master ID:      1
   Device Index:   0 (MASTER)
   ```

5. **Si besoin, configurer le master_id** :
   ```
   espnow config set-master-id 1
   ```
   (1 est la valeur par défaut, mais vous pouvez utiliser 1-255)

6. **Vérifier que device_index = 0** :
   ```
   espnow config set-device-index 0
   ```

#### Option B : Configuration par défaut

Si c'est la première utilisation, la configuration par défaut est :
- `master_id = 1`
- `device_index = 0` (MASTER)
- `device_id` = généré automatiquement depuis MAC

### Étape 3 : Démarrer l'application Master

```
app_start 10
```

Ou via le nom :
```
app_start espnow_master
```

### Étape 4 : Vérifier le fonctionnement

Les logs devraient afficher :
```
Master: App starting
Master: Config loaded - device_id=0xXXXX, master_id=1
Master: ESP-NOW initialized successfully
Master: Beacon task started
Master: Beacon #1 sent on channel 6 (master_id=1)
Master: Beacon #2 sent on channel 6 (master_id=1)
...
```

### Étape 5 : Configuration du serveur HTTP (optionnel)

Si vous voulez que le master envoie les données au serveur :

```
http config set-url http://192.168.1.100:8000
http config enable
```

---

## 📦 Guide de Déploiement Slave

### Étape 1 : Préparation du firmware

**Même firmware** que le master. Pas besoin de recompiler.

### Étape 2 : Configuration initiale du Slave

#### Option A : Via CLI (recommandé)

1. **Compiler et uploader le firmware** sur l'ESP32 slave
2. **Se connecter via Serial Monitor** (115200 baud)
3. **Configurer le WiFi** (optionnel, pour CLI seulement) :
   ```
   wifi_save MonWiFi MonMotDePasse
   ```

4. **Configurer le master_id** (doit correspondre au master) :
   ```
   espnow config set-master-id 1
   ```
   ⚠️ **IMPORTANT** : Le `master_id` doit être **identique** à celui du master !

5. **Configurer le device_index** (1, 2, 3, ... pour chaque slave) :
   ```
   espnow config set-device-index 1
   ```
   - Slave 1 → `device_index = 1`
   - Slave 2 → `device_index = 2`
   - Slave 3 → `device_index = 3`
   - etc.

6. **Vérifier la configuration** :
   ```
   espnow config show
   ```
   Devrait afficher :
   ```
   Device ID:      0xYYYY (différent du master)
   Master ID:      1 (identique au master)
   Device Index:   1 (SLAVE)
   ```

#### Option B : Configuration par défaut (première utilisation)

Par défaut :
- `master_id = 1`
- `device_index = 0` (MASTER) ← **À CHANGER !**

Vous **devez** changer `device_index` :
```
espnow config set-device-index 1
```

### Étape 3 : Démarrer l'application Slave

```
app_start 11
```

Ou via le nom :
```
app_start espnow_slave
```

### Étape 4 : Vérifier la découverte

Les logs devraient afficher :
```
Slave: App starting
Slave: Config loaded - device_id=0xYYYY, master_id=1, device_index=1
Slave: ESP-NOW initialized successfully
Slave: Discovery task started
Slave: Scanning channels for master (master_id=1)
Slave: Scanning channel 1
Slave: Scanning channel 2
...
Slave: Valid beacon from master 0xXXXX on channel 6
Slave: Master discovered - device_id=0xXXXX (AA:BB:CC:DD:EE:FF)
Slave: Master info saved to NVS
Slave: Aligned to master channel 6
Slave: Send task started
Slave: Data sent - seq=1, h0=123, h11=456
...
```

### Étape 5 : Vérifier la connexion (côté Master)

Sur le master, vous devriez voir :
```
Master: Data from slave 0xYYYY - seq=1, h0=123, h11=456
Master: New slave discovered - 0xYYYY (AA:BB:CC:DD:EE:FF)
Master: 1 slave(s) connected
```

---

## 🎯 Scénarios de Déploiement

### Scénario 1 : Un Master + Un Slave

```
MASTER (ESP32 #1)
├─ device_index = 0
├─ master_id = 1
└─ Commande: app_start 10

SLAVE (ESP32 #2)
├─ device_index = 1
├─ master_id = 1 (identique au master)
└─ Commande: app_start 11
```

### Scénario 2 : Un Master + Plusieurs Slaves

```
MASTER (ESP32 #1)
├─ device_index = 0
├─ master_id = 1
└─ Commande: app_start 10

SLAVE 1 (ESP32 #2)
├─ device_index = 1
├─ master_id = 1
└─ Commande: app_start 11

SLAVE 2 (ESP32 #3)
├─ device_index = 2
├─ master_id = 1
└─ Commande: app_start 11

SLAVE 3 (ESP32 #4)
├─ device_index = 3
├─ master_id = 1
└─ Commande: app_start 11
```

### Scénario 3 : Plusieurs Réseaux Indépendants

```
RÉSEAU 1 (Lab)
├─ MASTER (ESP32 #1)
│  ├─ device_index = 0
│  ├─ master_id = 1
│  └─ Commande: app_start 10
│
└─ SLAVE (ESP32 #2)
   ├─ device_index = 1
   ├─ master_id = 1
   └─ Commande: app_start 11

RÉSEAU 2 (Garage)
├─ MASTER (ESP32 #3)
│  ├─ device_index = 0
│  ├─ master_id = 2  ← DIFFÉRENT
│  └─ Commande: app_start 10
│
└─ SLAVE (ESP32 #4)
   ├─ device_index = 1
   ├─ master_id = 2  ← DIFFÉRENT
   └─ Commande: app_start 11
```

Les deux réseaux fonctionnent **indépendamment** grâce au filtrage par `master_id`.

---

## 🛠️ Commandes CLI utiles

### Configuration ESP-NOW

```bash
# Afficher la configuration actuelle
espnow config show

# Changer le master_id (1-255)
espnow config set-master-id <id>

# Changer le device_index (0=master, 1+=slave)
espnow config set-device-index <index>

# Réinitialiser la configuration
espnow config reset
```

### Gestion des applications

```bash
# Lister les applications disponibles
app_list

# Démarrer le master
app_start 10
# ou
app_start espnow_master

# Démarrer le slave
app_start 11
# ou
app_start espnow_slave

# Arrêter une application
app_stop 10

# Redémarrer une application
app_restart 10

# Voir l'état des applications
app_status
```

### Configuration HTTP (pour envoi au serveur)

```bash
# Configurer l'URL du serveur
http config set-url http://192.168.1.100:8000

# Activer le client HTTP
http config enable

# Tester la connexion
http test

# Voir les statistiques
http stats
```

### WiFi

```bash
# Sauvegarder les credentials WiFi
wifi_save <ssid> <password>

# Voir le statut WiFi
wifi_status
```

---

## 📝 Checklist de Déploiement

### Pour le Master

- [ ] Firmware compilé et uploadé
- [ ] WiFi configuré (si nécessaire)
- [ ] `master_id` configuré (défaut: 1)
- [ ] `device_index = 0` (vérifier)
- [ ] Application master démarrée (`app_start 10`)
- [ ] Beacons visibles dans les logs
- [ ] Serveur HTTP configuré (si nécessaire)

### Pour chaque Slave

- [ ] Firmware compilé et uploadé
- [ ] `master_id` configuré (identique au master)
- [ ] `device_index` configuré (1, 2, 3, ... unique par slave)
- [ ] Application slave démarrée (`app_start 11`)
- [ ] Découverte du master visible dans les logs
- [ ] Envoi de données visible dans les logs
- [ ] Réception confirmée côté master

---

## 🔍 Dépannage

### Le slave ne trouve pas le master

1. **Vérifier le master_id** :
   ```
   # Sur le master
   espnow config show

   # Sur le slave
   espnow config show
   ```
   Les deux doivent avoir le **même master_id**.

2. **Vérifier que le master est démarré** :
   ```
   # Sur le master
   app_status
   ```
   L'app 10 doit être `RUNNING`.

3. **Vérifier les beacons** :
   Les logs du master doivent afficher :
   ```
   Master: Beacon #X sent on channel 6 (master_id=1)
   ```

4. **Vérifier le scan** :
   Les logs du slave doivent afficher :
   ```
   Slave: Scanning channels for master (master_id=1)
   ```

### Le master ne reçoit pas les données du slave

1. **Vérifier que le slave a trouvé le master** :
   Les logs du slave doivent afficher :
   ```
   Slave: Master discovered - device_id=0xXXXX
   ```

2. **Vérifier l'envoi** :
   Les logs du slave doivent afficher :
   ```
   Slave: Data sent - seq=X, h0=XXX, h11=XXX
   ```

3. **Vérifier la réception** :
   Les logs du master doivent afficher :
   ```
   Master: Data from slave 0xYYYY - seq=X
   ```

### Le master et le slave ont le même device_id

**Impossible !** Le `device_id` est généré depuis la MAC address, qui est unique par ESP32.

Si vous voyez le même `device_id`, c'est que vous regardez le même device.

---

## 🎓 Résumé

1. **Même firmware** pour master et slave
2. **Configuration différente** :
   - Master : `device_index = 0`
   - Slave : `device_index = 1, 2, 3, ...`
3. **Même master_id** pour tous les devices d'un réseau
4. **Démarrage** : `app_start 10` (master) ou `app_start 11` (slave)
5. **Auto-détection** : Le slave trouve automatiquement le master via scan des canaux

---

## 📚 Prochaines étapes

1. Implémenter les améliorations proposées
2. Tester avec plusieurs slaves
3. Configurer le serveur HTTP pour recevoir les données
4. Monitorer les statistiques (perte de paquets, latence, etc.)

