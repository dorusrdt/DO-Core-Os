# 📊 Tâches des Applications - Optimisation Mémoire

Ce document décrit les tâches utilisées par les applications et les optimisations de mémoire effectuées.

## 🔍 Tâches Actuelles

### Applications ESP32

Chaque application crée **une seule tâche FreeRTOS** via le système de gestion d'applications :

1. **ESP32_master** : 1 tâche
2. **ESP32_sensor** : 1 tâche
3. **ESP32_com** : 1 tâche

**Total** : **3 tâches** pour les applications utilisateur

## 📏 Tailles de Stack

### Avant Optimisation

- **Taille par défaut** : `8192 bytes` (8 KB) par application
- **Total pour 3 apps** : `24 KB` de stack mémoire

### Après Optimisation

- **Taille par défaut** : `4096 bytes` (4 KB) par application
- **Total pour 3 apps** : `12 KB` de stack mémoire
- **Économie** : `12 KB` (50% de réduction)

## 🔧 Configuration

**Fichier** : `src/kernel/app/app_manager.h`

```cpp
#define APP_STACK_SIZE_DEFAULT 4096  // Taille par défaut de la pile pour les applications
```

Cette taille est utilisée pour **toutes les applications** lors de leur enregistrement.

## 📋 Détails des Tâches

### Structure de la Tâche

Chaque application utilise la fonction wrapper `app_task_wrapper()` qui :

1. Initialise l'application (`init` callback)
2. Démarre l'application (`start` callback)
3. Exécute la boucle principale (`loop` callback) avec un délai de 10ms
4. Arrête l'application (`stop` callback) quand nécessaire

**Code** : `src/kernel/app/app_manager.cpp` (lignes 15-66)

### Création de la Tâche

**Fonction** : `app_start()` (ligne 233)

```cpp
BaseType_t result = xTaskCreate(
    app_task_wrapper,        // Fonction de la tâche
    app->info.name,          // Nom de la tâche
    app->info.stack_size,    // Taille de la stack (4096 bytes)
    app,                     // Paramètres
    app->info.priority,      // Priorité (5)
    &app->task_handle        // Handle de la tâche
);
```

## 💾 Utilisation Mémoire

### Stack par Application

| Application | Stack Size | Mémoire Utilisée |
|------------|-----------|------------------|
| ESP32_master | 4096 bytes | 4 KB |
| ESP32_sensor | 4096 bytes | 4 KB |
| ESP32_com | 4096 bytes | 4 KB |
| **Total** | **12288 bytes** | **12 KB** |

### Autres Mémoires Utilisées

En plus de la stack, chaque application utilise :
- **Structures de données** : Variables globales, buffers, etc.
- **Heap** : Allocations dynamiques (JSON, WebSocket, etc.)
- **Code** : Flash memory (non-RAM)

## ⚠️ Considérations

### Taille Minimale Recommandée

- **4096 bytes (4 KB)** est une taille raisonnable pour :
  - Parsing JSON (ArduinoJson)
  - WebSocket communication
  - Buffers de données
  - Variables locales

### Surveillance

Si vous rencontrez des **stack overflows**, vous pouvez :
1. Augmenter `APP_STACK_SIZE_DEFAULT` à `5120` ou `6144` bytes
2. Utiliser `uxTaskGetStackHighWaterMark()` pour surveiller l'utilisation
3. Réduire les buffers locaux dans les fonctions

### Vérification de l'Utilisation

Pour vérifier l'utilisation réelle de la stack :

```cpp
UBaseType_t highWaterMark = uxTaskGetStackHighWaterMark(NULL);
Serial.printf("Stack remaining: %d bytes\n", highWaterMark);
```

## 📊 Comparaison Avant/Après

| Métrique | Avant | Après | Économie |
|----------|-------|-------|----------|
| Stack par app | 8 KB | 4 KB | 4 KB |
| Total (3 apps) | 24 KB | 12 KB | **12 KB** |
| Réduction | - | - | **50%** |

## ✅ Optimisations Effectuées

1. ✅ Réduction de `APP_STACK_SIZE_DEFAULT` de 8192 à 4096 bytes
2. ✅ Taille toujours suffisante pour les opérations courantes
3. ✅ Économie de 12 KB de RAM pour 3 applications

## 🔄 Tâches du Système (Non-App)

Les applications n'utilisent que leurs propres tâches. Les autres tâches du système (WiFi, NTP, etc.) sont gérées par le kernel et ne sont pas affectées par cette optimisation.

