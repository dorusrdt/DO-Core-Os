# Analyse de la Gestion du Temps - DO-Core OS

## Vue d'ensemble

Le système DO-Core OS implémente une **gestion hiérarchique du temps** avec trois sources principales, organisées par ordre de priorité :

1. **NTP (Network Time Protocol)** - Priorité 1 (si WiFi disponible)
2. **RTC DS3231 (Real-Time Clock)** - Priorité 2 (si données valides)
3. **Horloge système ESP32** - Priorité 3 (mode dégradé)

---

## Architecture des Composants

### 1. Time Sync Manager (`time_sync_manager.cpp/h`)
**Rôle** : Coordinateur central de la synchronisation temporelle

#### Fichiers
- `src/kernel/hal/time_sync_manager.h`
- `src/kernel/hal/time_sync_manager.cpp`

#### Sources de temps (enum)
```c
typedef enum {
    TIME_SOURCE_NTP = 0,      // Serveur NTP via WiFi
    TIME_SOURCE_RTC,          // Horloge RTC DS3231
    TIME_SOURCE_SYSTEM,       // Horloge interne ESP32
    TIME_SOURCE_UNKNOWN       // Aucune source valide
} TimeSource_t;
```

#### Statuts de synchronisation
```c
typedef enum {
    TIME_SYNC_STATUS_UNINITIALIZED = 0,
    TIME_SYNC_STATUS_OK,              // Synchronisation réussie
    TIME_SYNC_STATUS_ERROR,           // Erreur de synchronisation
    TIME_SYNC_STATUS_NO_SOURCE,       // Aucune source disponible
    TIME_SYNC_STATUS_DEGRADED_MODE    // Mode dégradé (horloge système)
} TimeSyncStatus_t;
```

#### Variables globales
```c
static TimeSyncStatus_t g_sync_status = TIME_SYNC_STATUS_UNINITIALIZED;
static bool g_sync_initialized = false;
static TimeSource_t g_current_source = TIME_SOURCE_UNKNOWN;
static uint32_t g_sync_failures = 0;
static uint32_t g_last_sync_attempt = 0;
```

#### Fonction principale : `time_sync_automatic()`

**Algorithme de sélection (lignes 35-116)** :

```
1. Vérifier NTP (priorité 1)
   ├─ Si ntp_is_synced() == true
   ├─ ET ntp_get_time() > 1577836800 (après 2020)
   │  ├─ Source = TIME_SOURCE_NTP
   │  ├─ Synchroniser RTC avec NTP (si disponible)
   │  └─ Retour SYS_OK
   
2. Vérifier RTC (priorité 2)
   ├─ Si rtc_is_initialized() == true
   ├─ ET rtc_status == RTC_STATUS_OK ou RTC_STATUS_BATTERY_LOW
   ├─ ET rtc_get_time() > 1577836800
   │  ├─ Source = TIME_SOURCE_RTC
   │  ├─ Synchroniser l'horloge système avec RTC (settimeofday)
   │  └─ Retour SYS_OK (ou DEGRADED si batterie faible)
   
3. Vérifier horloge système (priorité 3)
   ├─ Si time(nullptr) > 1577836800
   │  ├─ Source = TIME_SOURCE_SYSTEM
   │  ├─ Mode = TIME_SYNC_STATUS_DEGRADED_MODE
   │  └─ Retour SYS_OK
   
4. Aucune source valide
   └─ Source = TIME_SOURCE_UNKNOWN
   └─ Status = TIME_SYNC_STATUS_NO_SOURCE
   └─ Retour SYS_ERROR
```

**Validation temporelle** : Toutes les sources doivent fournir un timestamp > `1577836800` (1er janvier 2020 00:00:00 UTC)

---

### 2. NTP Manager (`ntp_manager.cpp/h`)
**Rôle** : Gestion de la synchronisation via serveur NTP

#### Fichiers
- `src/kernel/network/ntp_manager.h`
- `src/kernel/network/ntp_manager.cpp`

#### Configuration
```c
#define NTP_SERVER "pool.ntp.org"
#define NTP_GMT_OFFSET_SEC 0          // UTC+0 (Maroc - WET)
#define NTP_DAYLIGHT_OFFSET_SEC 3600  // +1h en été (WEST)
```

#### États NTP
```c
typedef enum {
    NTP_STATUS_UNINITIALIZED = 0,
    NTP_STATUS_DISCONNECTED,    // WiFi déconnecté
    NTP_STATUS_SYNCING,         // Synchronisation en cours
    NTP_STATUS_SYNCED,          // Synchronisé avec succès
    NTP_STATUS_FAILED,          // Échec de synchronisation
    NTP_STATUS_TIMEOUT          // Timeout (10 secondes)
} NtpStatus_t;
```

#### Informations de synchronisation
```c
typedef struct {
    time_t last_sync_time;           // Timestamp de la dernière sync
    char last_sync_server[32];       // Serveur NTP utilisé
    uint8_t successful_syncs;        // Compteur de succès
    uint8_t failed_syncs;            // Compteur d'échecs
} NtpSyncInfo_t;
```

#### Fonctionnement

**Initialisation (`ntp_init()`)** :
- Utilise `configTime()` de l'API Arduino ESP32
- Configure le serveur NTP et les offsets de timezone
- État initial : `NTP_STATUS_DISCONNECTED`

**Synchronisation (`ntp_sync()`)** :
1. Vérifier que WiFi est connecté (`WiFi.status() == WL_CONNECTED`)
2. Tenter `getLocalTime(&timeinfo)` avec timeout de 10 secondes
3. Convertir en timestamp Unix avec `mktime()`
4. Mettre à jour `g_ntp_info` et passer à `NTP_STATUS_SYNCED`

**Vérification (`ntp_is_synced()`)** :
- Statut = `NTP_STATUS_SYNCED`
- ET `ntp_get_time() > 1577836800`

#### Fonctions utilitaires
- `ntp_is_business_hours()` : Détecte heures de bureau (8h-18h, lun-ven)
- `ntp_is_night_time()` : Détecte la nuit (22h-6h)
- `ntp_get_hour/minute/second()` : Extraction des composants temporels

---

### 3. RTC Manager (`rtc_manager.cpp/h`)
**Rôle** : Gestion de l'horloge matérielle DS3231

#### Fichiers
- `src/kernel/hal/rtc_manager.h`
- `src/kernel/hal/rtc_manager.cpp`

#### Configuration matérielle
```c
#define RTC_SDA_PIN 25              // Pin I2C Data
#define RTC_SCL_PIN 26              // Pin I2C Clock
#define RTC_I2C_ADDRESS 0x68        // Adresse I2C du DS3231
```

#### États RTC
```c
typedef enum {
    RTC_STATUS_UNINITIALIZED = 0,
    RTC_STATUS_OK,              // Fonctionnel
    RTC_STATUS_ERROR,           // Erreur générique
    RTC_STATUS_NOT_FOUND,       // Module non détecté sur I2C
    RTC_STATUS_BATTERY_LOW,     // Pile faible (lostPower)
    RTC_STATUS_COMM_ERROR       // Erreur de communication I2C
} RtcStatus_t;
```

#### Bibliothèque utilisée
- **RTClib** (Adafruit) version 2.1.4
- Classe `RTC_DS3231`

#### Fonctionnement

**Initialisation (`rtc_manager_init()`)** :
1. Initialiser I2C avec `Wire.begin(RTC_SDA_PIN, RTC_SCL_PIN)`
2. Configurer vitesse I2C à 100kHz
3. Tenter `rtc.begin()` avec 3 tentatives (retry)
4. Vérifier `rtc.lostPower()` pour détecter batterie faible
5. Si batterie faible : tenter récupération avec heure de compilation

**Lecture de l'heure (`rtc_get_time()`)** :
1. Vérifier statut RTC (OK ou BATTERY_LOW)
2. Appeler `rtc.now()` pour obtenir `DateTime`
3. Convertir en timestamp Unix avec `now.unixtime()`
4. Valider : timestamp > 1577836800 (après 2020)
5. En cas d'erreur : tenter `rtc_recovery_attempt()`

**Écriture de l'heure (`rtc_set_time()`)** :
- Utilisé par Time Sync Manager pour synchroniser RTC avec NTP
- Crée un objet `DateTime(timestamp)` et appelle `rtc.adjust(dt)`

**Récupération (`rtc_recovery_attempt()`)** :
- Limité à 5 tentatives
- Délai minimum de 30 secondes entre tentatives
- Réinitialise le bus I2C
- Tente `rtc.begin()` à nouveau

#### Fonctionnalités supplémentaires
- `rtc_get_temperature()` : Lecture du capteur de température intégré DS3231
- `rtc_is_battery_ok()` : Vérification de l'état de la pile

---

## Intégration dans le Système

### Initialisation (main.cpp - setup())

**Ordre d'initialisation** :
```c
1. NVS Flash (ligne 421)
2. Task Manager (ligne 434)
3. Memory Manager (ligne 441)
4. Log System (ligne 448)
5. System Monitor (ligne 455)
6. WiFi Manager (ligne 465)
7. NTP Manager (ligne 469)          ← Initialisation NTP
8. HTTP Client (ligne 478)
9. OTA Manager (ligne 489)
10. RTC Manager (ligne 501)         ← Initialisation RTC
11. Time Sync Manager (ligne 513)   ← Initialisation coordinateur
12. App Manager (ligne 524)
```

**Synchronisation initiale** (lignes 612-626) :
```c
SysError_t initial_sync_result = time_sync_automatic();
if (initial_sync_result == SYS_OK) {
    time_t current_time = time_sync_get_current_time();
    kernel_log(LOG_LEVEL_INFO, "Current time: %s", 
               time_sync_format_current_time().c_str());
}
```

### Tâche de synchronisation périodique

**Tâche FreeRTOS** : `time_sync_task()` (lignes 234-270)

```c
void time_sync_task(void* parameter) {
    const uint32_t sync_interval_ms = 900000; // 15 minutes
    const uint32_t check_interval_ms = 1000;  // Vérifier toutes les secondes
    uint32_t elapsed_ms = 0;

    while (system_running) {
        // Vérifier si une synchronisation immédiate est demandée
        bool immediate_requested = time_sync_is_immediate_requested();
        bool should_sync = (elapsed_ms >= sync_interval_ms) || immediate_requested;
        
        if (should_sync) {
            if (immediate_requested) {
                kernel_log(LOG_LEVEL_INFO, "Executing immediate time sync");
            }
            
            // Synchronisation automatique
            SysError_t result = time_sync_automatic();
            
            if (result == SYS_OK) {
                TimeSource_t source = time_sync_get_current_source();
                kernel_log(LOG_LEVEL_INFO, "Time sync OK - Source: %s",
                          (source == TIME_SOURCE_NTP) ? "NTP" :
                          (source == TIME_SOURCE_RTC) ? "RTC" : "SYSTEM");
            }
            
            elapsed_ms = 0;
        }
        
        // Vérifier toutes les secondes
        vTaskDelay(pdMS_TO_TICKS(check_interval_ms));
        elapsed_ms += check_interval_ms;
    }
}
```

**Caractéristiques** :
- Priorité : `PRIORITY_LOW`
- Stack : `STACK_SIZE_SMALL`
- Core : 0 (pinned)
- Période : **15 minutes** (900 secondes) ⚡ **AMÉLIORÉ**
- Vérification : **Toutes les secondes** (pour synchronisation immédiate) ⚡ **NOUVEAU**

### Synchronisation NTP initiale (main.cpp)

**Première synchronisation WiFi** (lignes 581-603) :
```c
NtpStatus_t sync_status = ntp_sync();
if (sync_status == NTP_STATUS_SYNCED) {
    time_t current_time = ntp_get_time();
    kernel_log(LOG_LEVEL_INFO, "Start: %s", ntp_format_time(current_time).c_str());
    
    // Vérifier les conditions temporelles
    if (ntp_is_business_hours()) {
        kernel_log(LOG_LEVEL_INFO, "Business mode");
    } else if (ntp_is_night_time()) {
        kernel_log(LOG_LEVEL_INFO, "Night mode");
    }
}
```

---

## Commandes Interface CLI

Le système expose plusieurs commandes pour interagir avec la gestion du temps :

### Commandes disponibles (interface.cpp)

1. **`time_status`** : Affiche l'heure actuelle et la source
   - Fonction : `cmd_time_status()`
   - Affiche : source active, heure formatée

2. **`time_source`** : Affiche la source de temps actuelle
   - Fonction : `cmd_time_source()`
   - Affiche : source + statut de synchronisation

3. **`time_sync`** : Force une synchronisation manuelle
   - Fonction : `cmd_time_sync()`
   - Appelle : `time_sync_automatic()`

4. **`time_sources`** : Informations détaillées sur toutes les sources
   - Fonction : `cmd_time_sources()`
   - Appelle : `time_sync_get_source_info()`
   - Affiche : statut NTP, RTC, System, échecs, etc.

5. **`rtc_status`** : Statut du module RTC
6. **`rtc_time`** : Heure du RTC
7. **`rtc_battery`** : État de la batterie RTC

---

## Flux de Données

### Scénario 1 : Démarrage avec WiFi

```
[BOOT]
  ↓
[Init NTP Manager] → configTime(pool.ntp.org)
  ↓
[Init RTC Manager] → Wire.begin() → rtc.begin()
  ↓
[Init Time Sync Manager]
  ↓
[WiFi Connect] → WiFi.begin(ssid, password)
  ↓
[NTP Sync] → ntp_sync() → getLocalTime() → SUCCESS
  ↓
[Initial Time Sync] → time_sync_automatic()
  ├─ NTP disponible → TIME_SOURCE_NTP
  ├─ Synchroniser RTC ← NTP
  └─ Status: TIME_SYNC_STATUS_OK
  ↓
[Tâche périodique] → Toutes les 1h → time_sync_automatic()
```

### Scénario 2 : Démarrage sans WiFi (RTC valide)

```
[BOOT]
  ↓
[Init RTC Manager] → rtc.begin() → RTC_STATUS_OK
  ↓
[Init Time Sync Manager]
  ↓
[WiFi NOT Connected]
  ↓
[Initial Time Sync] → time_sync_automatic()
  ├─ NTP non disponible
  ├─ RTC disponible → rtc_get_time() → valide
  ├─ Source = TIME_SOURCE_RTC
  ├─ settimeofday() ← RTC
  └─ Status: TIME_SYNC_STATUS_OK
  ↓
[Tâche périodique] → Toutes les 1h
  ├─ Si WiFi reconnecté → NTP prend le relais
  └─ Sinon → continue avec RTC
```

### Scénario 3 : Mode dégradé (pas de WiFi, RTC défaillant)

```
[BOOT]
  ↓
[Init RTC Manager] → rtc.begin() → RTC_STATUS_NOT_FOUND
  ↓
[WiFi NOT Connected]
  ↓
[Initial Time Sync] → time_sync_automatic()
  ├─ NTP non disponible
  ├─ RTC non disponible
  ├─ Horloge système > 2020 → valide (depuis dernier boot)
  ├─ Source = TIME_SOURCE_SYSTEM
  └─ Status: TIME_SYNC_STATUS_DEGRADED_MODE
  ↓
[Tâche périodique] → Toutes les 1h
  └─ Tente de récupérer NTP ou RTC
```

### Scénario 4 : Synchronisation RTC par NTP

```
[NTP Sync réussie]
  ↓
time_sync_automatic() → ntp_is_synced() == true
  ↓
ntp_time = ntp_get_time() → 1729123456 (exemple)
  ↓
rtc_is_initialized() && rtc_status != NOT_FOUND
  ↓
rtc_set_time(ntp_time) → rtc.adjust(DateTime(ntp_time))
  ↓
[RTC synchronisé avec NTP]
  └─ Garantit l'heure même après déconnexion WiFi
```

---

## Gestion des Erreurs et Récupération

### Mécanismes de tolérance aux pannes

#### 1. Compteur d'échecs
```c
static uint32_t g_sync_failures = 0;
```
- Incrémenté à chaque échec de synchronisation
- Réinitialisé à 0 en cas de succès
- Utilisé pour le monitoring et le diagnostic

#### 2. Récupération RTC
- **Tentatives limitées** : Maximum 5 tentatives
- **Délai anti-spam** : 30 secondes minimum entre tentatives
- **Réinitialisation I2C** : `Wire.begin()` à nouveau
- **Fallback** : Si échec, passe à la source suivante

#### 3. Mode dégradé
- Utilise l'horloge système ESP32
- Statut : `TIME_SYNC_STATUS_DEGRADED_MODE`
- Logs de warning pour alerter l'utilisateur
- Continue de fonctionner avec précision réduite

#### 4. Validation temporelle stricte
- Tous les timestamps doivent être > `1577836800` (2020)
- Évite les dates aberrantes (1970, etc.)
- Protection contre les horloges non initialisées

---

## Avantages de l'Architecture

### 1. **Hiérarchie claire**
- NTP (précision maximale) → RTC (persistance) → System (fallback)
- Sélection automatique de la meilleure source disponible

### 2. **Résilience**
- Continue de fonctionner même sans WiFi
- Récupération automatique après pannes temporaires
- Mode dégradé plutôt qu'échec total

### 3. **Synchronisation bidirectionnelle**
- NTP → RTC : Mise à jour du RTC avec l'heure précise
- RTC → System : Restauration de l'heure après reboot

### 4. **Monitoring complet**
- Logs détaillés de chaque synchronisation
- Compteurs de succès/échecs
- Commandes CLI pour diagnostic

### 5. **Optimisation énergétique**
- Synchronisation périodique (1h) au lieu de continue
- Utilise RTC (très faible consommation) quand WiFi indisponible
- Évite les requêtes NTP inutiles

---

## Améliorations Implémentées ✅

### 1. **Synchronisation immédiate lors de la reconnexion WiFi** ✅
- **Avant** : Délai max 1 heure après reconnexion WiFi
- **Après** : Synchronisation en < 1 seconde
- **Implémentation** : Flag `g_immediate_sync_requested` + fonction `time_sync_request_immediate()`
- **Déclenchement** : Automatique dans `wifi_supervision_task()` lors de la reconnexion

### 2. **Intervalle de synchronisation réduit** ✅
- **Avant** : 1 heure (3600 secondes)
- **Après** : 15 minutes (900 secondes)
- **Avantage** : Dérive réduite de 4x, détection plus rapide des changements

**Voir le document `AMELIORATIONS_GESTION_TEMPS.md` pour les détails complets.**

---

## Points d'Amélioration Potentiels (Futurs)

### 1. **Persistance de l'heure système**
- Sauvegarder l'heure dans NVS avant reboot
- Restaurer au démarrage si RTC absent

### 2. **Détection de dérive RTC**
- Comparer RTC vs NTP périodiquement
- Alerter si dérive > seuil (ex: 5 secondes)

### 3. **Timezone dynamique**
- Actuellement : hardcodé (UTC+0/+1)
- Suggestion : Configuration via commande CLI ou API

### 4. **Serveurs NTP multiples**
- Actuellement : `pool.ntp.org` uniquement
- Suggestion : Liste de serveurs avec fallback

### 5. **Métriques de qualité**
- Enregistrer la précision de chaque source
- Statistiques de dérive sur 24h/7j

### 6. **NTP adaptive polling**
- Adapter l'intervalle selon la dérive observée
- Plus fréquent si dérive importante, moins si stable

---

## Dépendances Externes

### Bibliothèques
1. **RTClib** (Adafruit) v2.1.4
   - Gestion du DS3231
   - `platformio.ini` : `adafruit/RTClib@^2.1.4`

2. **Arduino ESP32 Core**
   - `configTime()` pour NTP
   - `getLocalTime()` pour récupération
   - `settimeofday()` pour synchronisation système

3. **Wire** (I2C)
   - Communication avec RTC DS3231
   - Pins configurables (25/26)

### Matériel requis
- **ESP32 DevKit** (ou compatible)
- **DS3231 RTC Module** (optionnel mais recommandé)
  - Connexion I2C sur pins 25 (SDA) et 26 (SCL)
  - Pile CR2032 pour sauvegarde

---

## Résumé Technique

| Aspect | Détail |
|--------|--------|
| **Sources de temps** | NTP → RTC → System Clock |
|--------|--------|
| **Priorité** | NTP (si WiFi) > RTC (si valide) > System |
| **Validation** | Timestamp > 1er janvier 2020 |
| **Synchronisation** | Automatique toutes les **15 minutes** ⚡ |
| **Sync immédiate** | < 1 seconde après reconnexion WiFi ⚡ |
| **Tolérance aux pannes** | Mode dégradé avec fallback |
| **RTC** | DS3231 sur I2C (pins 25/26) |
| **NTP** | pool.ntp.org, timeout 10s |
| **Timezone** | UTC+0 (WET) / UTC+1 (WEST) |
| **Tâche FreeRTOS** | Priority LOW, Stack SMALL, Core 0 |
| **Commandes CLI** | 7 commandes (time_*, rtc_*) |

---

## Conclusion

Le système de gestion du temps de DO-Core OS est **robuste, hiérarchique et résilient**. Il combine la précision du NTP (quand disponible), la persistance du RTC (pour les reboots), et un fallback sur l'horloge système. L'architecture permet de maintenir une heure fiable dans tous les scénarios d'utilisation, avec une récupération automatique après pannes temporaires.

La synchronisation bidirectionnelle (NTP → RTC et RTC → System) garantit que l'heure est toujours disponible, même après une coupure de courant ou une déconnexion WiFi prolongée.
