# 📡 ESP-NOW : Gestion du Canal WiFi

## ⚠️ Comportement Important

**ESP-NOW utilise TOUJOURS le même canal que l'interface WiFi active.**

### Comment ça fonctionne

```
┌─────────────────────────────────────────┐
│ Interface WiFi (STA ou AP)              │
│ Canal = X (1-13)                        │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│ ESP-NOW                                  │
│ Canal = X (identique au WiFi)           │
│ ← AUTOMATIQUE, pas de configuration    │
└─────────────────────────────────────────┘
```

### Scénarios

#### 1. WiFi STA connecté
```
WiFi.connect("MonWiFi") → Canal 6
ESP-NOW → Canal 6 automatiquement
```

#### 2. WiFi change de canal
```
WiFi reconnecte → Canal 11
ESP-NOW → Canal 11 automatiquement
```

#### 3. WiFi déconnecté
```
WiFi.disconnect()
ESP-NOW → Canal par défaut (généralement 6)
```

#### 4. WiFi AP actif
```
WiFi.softAP("MonAP") → Canal 1
ESP-NOW → Canal 1 automatiquement
```

---

## 🎯 Implications pour votre Code

### ✅ Ce qui est CORRECT dans votre code actuel

```cpp
// Dans beacon_task() du master
uint8_t primary = 0;
wifi_second_chan_t secondary;
if (WiFi.status() == WL_CONNECTED) {
    esp_wifi_get_channel(&primary, &secondary);  // ← Lit le canal WiFi
}
uint8_t tx_channel = (primary >= 1 && primary <= 13) ? primary : 6;
beacon.channel = tx_channel;  // ← Annonce le canal actuel
```

**Pourquoi c'est correct :**
- Le master lit le canal WiFi actuel
- Il l'annonce dans le beacon
- ESP-NOW utilise automatiquement ce canal
- Les slaves peuvent s'aligner

### ❌ Ce qu'il NE FAUT PAS faire

```cpp
// ❌ MAUVAIS : Canal codé en dur
#define ESPNOW_FIXED_CHANNEL 6
esp_wifi_set_channel(ESPNOW_FIXED_CHANNEL, WIFI_SECOND_CHAN_NONE);
```

**Pourquoi c'est mauvais :**
- Force un canal spécifique
- Peut entrer en conflit avec le WiFi
- Si le WiFi change, ESP-NOW ne suivra pas
- Les slaves perdront le master

---

## 🔄 Gestion des Changements de Canal

### Problème

Si le WiFi du master change de canal :
1. ESP-NOW change automatiquement de canal
2. Le master envoie un nouveau beacon avec le nouveau canal
3. Les slaves doivent détecter le changement et s'aligner

### Solution actuelle (dans votre code)

```cpp
// Slave : discovery_task()
if (master_info.master_found) {
    // Vérifier si le canal a changé
    uint8_t current_ch = 0;
    wifi_second_chan_t secondary;
    esp_wifi_get_channel(&current_ch, &secondary);

    if (current_ch != master_info.channel) {
        // Canal différent → s'aligner
        esp_wifi_set_channel(master_info.channel, WIFI_SECOND_CHAN_NONE);
        kernel_log(LOG_LEVEL_DEBUG, "Aligned to master channel %d", master_info.channel);
    }

    // Vérifier timeout (avec marge si canal change)
    if ((now - master_info.last_beacon_ticks) > pdMS_TO_TICKS(5000)) {
        master_info.master_found = false;
    }
}
```

**Cela fonctionne car :**
- Le slave lit le canal du master depuis le beacon
- Il s'aligne automatiquement
- Le timeout permet de détecter les pertes

---

## 🚨 Cas Problématiques

### Cas 1 : Master connecté à WiFi, puis WiFi change de canal

```
État initial :
- Master WiFi : Canal 6
- ESP-NOW : Canal 6
- Slave : Canal 6 ✓

WiFi change de canal :
- Master WiFi : Canal 11
- ESP-NOW : Canal 11 (automatique)
- Master envoie beacon avec channel=11
- Slave reçoit beacon → détecte changement
- Slave s'aligne sur canal 11 ✓
```

**Résultat :** Fonctionne correctement grâce aux beacons

### Cas 2 : Master déconnecte WiFi, puis reconnecte sur autre canal

```
État initial :
- Master WiFi : Déconnecté
- ESP-NOW : Canal 6 (défaut)
- Slave : Canal 6 ✓

Master reconnecte WiFi :
- Master WiFi : Canal 3
- ESP-NOW : Canal 3 (automatique)
- Master envoie beacon avec channel=3
- Slave reçoit beacon → détecte changement
- Slave s'aligne sur canal 3 ✓
```

**Résultat :** Fonctionne correctement

### Cas 3 : Slave perd temporairement le signal

```
État :
- Master : Canal 6
- Slave : Canal 6
- Signal perdu pendant 3s

Slave :
- Continue d'écouter sur canal 6
- Timeout après 5s sans beacon
- Passe en mode scan
- Scan tous les canaux
- Retrouve le master sur canal 6 ✓
```

**Résultat :** Le scan permet de retrouver le master

---

## 💡 Recommandations

### 1. Ne pas forcer le canal

```cpp
// ✅ BON : Lire le canal actuel
esp_wifi_get_channel(&primary, &secondary);
beacon.channel = primary;

// ❌ MAUVAIS : Forcer un canal
esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);
```

### 2. Toujours annoncer le canal dans le beacon

```cpp
// Le master doit toujours inclure le canal actuel
beacon.channel = tx_channel;  // Canal WiFi actuel
```

### 3. Le slave doit suivre les changements

```cpp
// Détecter les changements de canal
if (beacon.channel != master_info.channel) {
    // Canal a changé → s'aligner
    esp_wifi_set_channel(beacon.channel, WIFI_SECOND_CHAN_NONE);
    master_info.channel = beacon.channel;
}
```

### 4. Gérer les timeouts avec marge

```cpp
// Donner plus de temps si le canal vient de changer
TickType_t timeout = pdMS_TO_TICKS(5000);
if (canal_just_changed) {
    timeout = pdMS_TO_TICKS(8000);  // Plus de marge
}
```

---

## 📊 Résumé

| Aspect | Comportement |
|--------|--------------|
| **Canal ESP-NOW** | Suit automatiquement le WiFi |
| **Configuration** | Pas nécessaire, automatique |
| **Annonce** | Le master annonce son canal dans le beacon |
| **Alignement** | Le slave s'aligne automatiquement |
| **Changements** | Gérés via les beacons |

---

## ✅ Votre Code est Correct

Votre implémentation actuelle gère correctement le canal :
- ✅ Le master lit le canal WiFi actuel
- ✅ Le master annonce le canal dans le beacon
- ✅ Le slave s'aligne sur le canal du master
- ✅ Pas de canal codé en dur

**Aucune modification nécessaire** pour la gestion du canal !

