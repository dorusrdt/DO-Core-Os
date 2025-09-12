#include "sst_data.h"
#include "../../kernel/hal/rtc_manager.h"
#include "../../kernel/core/minimal_config.h"
#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

// Variable globale des données SST
SSTData_t sst_data;

// Instance Preferences globale (comme pour WiFi - STATIC)
static Preferences sst_prefs;

// Queue et task pour la sauvegarde sécurisée
QueueHandle_t sst_save_queue = NULL;
TaskHandle_t sst_save_task_handle = NULL;

// Protection contre les sauvegardes trop fréquentes
static unsigned long last_save_time = 0;
#define MIN_SAVE_INTERVAL_MS 2000  // Minimum 2 secondes entre les sauvegardes

// Initialisation des données SST (AVEC PERSISTANCE)
SysError_t sst_data_init(void) {
    SERIAL_PRINTLN_MINIMAL("SST Data: Initializing with persistence...");
    
    // Charger les données depuis NVS (méthode WiFi)
    return sst_data_load();
}

// Charger les données depuis SPIFFS (JSON) - Code utilisateur
SysError_t sst_data_load(void) {
    SERIAL_PRINTLN_MINIMAL("SST Data: Loading from SPIFFS JSON...");
    
    if (!SPIFFS.begin(true)) {
        SERIAL_PRINTLN_MINIMAL("SST Data: Erreur SPIFFS");
        return SYS_ERROR;
    }

    File file = SPIFFS.open("/sst_data.json", FILE_READ);
    if (!file) {
        SERIAL_PRINTLN_MINIMAL("SST Data: Aucun fichier trouvé, using defaults");
        sst_data_reset_memory_only();
        return SYS_OK;
    }

    StaticJsonDocument<4096> doc;
    DeserializationError error = deserializeJson(doc, file);
    if (error) {
        SERIAL_PRINTF_MINIMAL("SST Data: Erreur lecture JSON: %s\n", error.c_str());
        file.close();
        sst_data_reset_memory_only();
        return SYS_ERROR;
    }

    // Récupération des champs
    sst_data.total_accidents = doc["total_accidents"] | 0;
    sst_data.accidents_avec_arret = doc["accidents_avec_arret"] | 0;
    sst_data.accidents_sans_arret = doc["accidents_sans_arret"] | 0;
    sst_data.date_dernier_accident = doc["date_dernier_accident"] | 0;
    sst_data.jours_sans_accident = doc["jours_sans_accident"] | 0;
    sst_data.record_jours_sans_accident = doc["record_jours_sans_accident"] | 0;
    sst_data.heures_travaillees = doc["heures_travaillees"] | 0.0f;
    sst_data.taux_frequence = doc["taux_frequence"] | 0.0;
    sst_data.heure_incrementation = doc["heure_incrementation"] | SST_DEFAULT_INCREMENT_HOUR;
    sst_data.heures_travaillees_par_jour = doc["heures_travaillees_par_jour"] | SST_DEFAULT_WORK_HOURS_PER_DAY;
    sst_data.derniere_incrementation = doc["derniere_incrementation"] | 0;

    // Liste des accidents
    JsonArray arr = doc["accidents"].as<JsonArray>();
    sst_data.nb_accidents = arr.size();
    for (uint32_t i = 0; i < sst_data.nb_accidents && i < SST_MAX_ACCIDENTS; i++) {
        JsonObject obj = arr[i];
        sst_data.accidents[i].id = obj["id"] | 0;
        sst_data.accidents[i].date = obj["date"] | 0;
        sst_data.accidents[i].avec_arret = obj["avec_arret"] | false;
        strncpy(sst_data.accidents[i].description, obj["description"] | "Accident",
                sizeof(sst_data.accidents[i].description) - 1);
        sst_data.accidents[i].description[sizeof(sst_data.accidents[i].description) - 1] = '\0';
    }

    file.close();
    
    // Les jours sans accident sont maintenant gérés directement dans sst_loop()
    
    SERIAL_PRINTLN_MINIMAL("SST Data: JSON data loaded successfully!");
    return SYS_OK;
}

// Sauvegarder les données dans SPIFFS (JSON) - Code utilisateur
SysError_t sst_data_save(void) {
    SERIAL_PRINTLN_MINIMAL("SST Data: Saving to SPIFFS JSON...");
    
    if (!SPIFFS.begin(true)) {
        SERIAL_PRINTLN_MINIMAL("SST Data: Erreur SPIFFS");
        return SYS_ERROR;
    }

    File file = SPIFFS.open("/sst_data.json", FILE_WRITE);
    if (!file) {
        SERIAL_PRINTLN_MINIMAL("SST Data: Erreur ouverture fichier");
        return SYS_ERROR;
    }

    StaticJsonDocument<4096> doc; // Taille à ajuster selon ta structure

    // Sauvegarde des champs principaux
    doc["total_accidents"] = sst_data.total_accidents;
    doc["accidents_avec_arret"] = sst_data.accidents_avec_arret;
    doc["accidents_sans_arret"] = sst_data.accidents_sans_arret;
    doc["date_dernier_accident"] = (uint32_t)sst_data.date_dernier_accident;
    doc["jours_sans_accident"] = sst_data.jours_sans_accident;
    doc["record_jours_sans_accident"] = sst_data.record_jours_sans_accident;
    doc["heures_travaillees"] = sst_data.heures_travaillees;
    doc["taux_frequence"] = sst_data.taux_frequence;
    doc["heure_incrementation"] = sst_data.heure_incrementation;
    doc["heures_travaillees_par_jour"] = sst_data.heures_travaillees_par_jour;
    doc["derniere_incrementation"] = (uint32_t)sst_data.derniere_incrementation;

    // Liste des accidents
    JsonArray arr = doc.createNestedArray("accidents");
    for (uint32_t i = 0; i < sst_data.nb_accidents; i++) {
        JsonObject obj = arr.createNestedObject();
        obj["id"] = sst_data.accidents[i].id;
        obj["date"] = (uint32_t)sst_data.accidents[i].date;
        obj["avec_arret"] = sst_data.accidents[i].avec_arret;
        obj["description"] = sst_data.accidents[i].description;
    }
    doc["nb_accidents"] = sst_data.nb_accidents;

    if (serializeJson(doc, file) == 0) {
        SERIAL_PRINTLN_MINIMAL("SST Data: Erreur écriture JSON");
        file.close();
        return SYS_ERROR;
    }

    file.close();
    SERIAL_PRINTLN_MINIMAL("SST Data: JSON saved successfully!");
    return SYS_OK;
}

// Réinitialiser les données SST (SPIFFS + JSON)
SysError_t sst_data_reset(void) {
    SERIAL_PRINTLN_MINIMAL("SST Data: Resetting to defaults with SPIFFS");
    
    // 1️⃣ Réinitialiser en mémoire
    sst_data_reset_memory_only();
    
    // 2️⃣ Supprimer le fichier JSON existant
    if (SPIFFS.begin(true)) {
        if (SPIFFS.exists("/sst_data.json")) {
            SPIFFS.remove("/sst_data.json");
            SERIAL_PRINTLN_MINIMAL("SST Data: Old JSON file removed");
        }
    }
    
    // 3️⃣ Sauvegarder les valeurs par défaut avec le nouveau format JSON
    return sst_data_save();
}

// Réinitialiser les données SST en mémoire seulement
SysError_t sst_data_reset_memory_only(void) {
    SERIAL_PRINTLN_MINIMAL("SST Data: Resetting to defaults (memory only)");
    
    // Initialiser avec des valeurs par défaut
    memset(&sst_data, 0, sizeof(SSTData_t));
    
    sst_data.heure_incrementation = SST_DEFAULT_INCREMENT_HOUR;
    sst_data.heures_travaillees_par_jour = SST_DEFAULT_WORK_HOURS_PER_DAY;
    sst_data.derniere_incrementation = 0;
    sst_data.date_dernier_accident = 0;
    sst_data.jours_sans_accident = 0;
    sst_data.record_jours_sans_accident = 0;
    sst_data.taux_frequence = 0.0;
    
    SERIAL_PRINTLN_MINIMAL("SST Data: Reset to defaults (memory only)");
    return SYS_OK;
}


// Calculer le taux de fréquence
SysError_t sst_calculate_taux_frequence(void) {
    if (sst_data.heures_travaillees == 0.0f) {
        sst_data.taux_frequence = 0.0;
    } else {
        // TF = (accidents_avec_arret × 1_000_000) / heures_travaillees
        sst_data.taux_frequence = (float)(sst_data.accidents_avec_arret * 1000000) / sst_data.heures_travaillees;
    }
    
    return SYS_OK;
}


// Ajouter un accident (AVEC PERSISTANCE)
SysError_t sst_add_accident(bool avec_arret, const char* description) {
    SERIAL_PRINTLN_MINIMAL("SST Data: Adding accident with persistence");
    
    // Ajouter l'accident en mémoire
    SysError_t result = sst_add_accident_memory_only(avec_arret, description);
    if (result != SYS_OK) {
        return result;
    }
    
    // Sauvegarder (avec protection contre les sauvegardes trop fréquentes)
    SysError_t save_result = sst_data_save();
    if (save_result != SYS_OK) {
        SERIAL_PRINTLN_MINIMAL("SST Data: Warning - Failed to save accident data");
        // Ne pas retourner d'erreur car l'accident a été ajouté en mémoire
    }
    
    return SYS_OK;
}

// Obtenir la liste des accidents
SysError_t sst_get_accidents_list(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return SYS_INVALID_PARAM;
    }
    
    buffer[0] = '\0';
    
    if (sst_data.nb_accidents == 0) {
        strcpy(buffer, "Aucun accident enregistre");
        return SYS_OK;
    }
    
    char temp[256];
    strcpy(buffer, "Liste des accidents:\n");
    
    for (uint32_t i = 0; i < sst_data.nb_accidents; i++) {
        Accident_t* accident = &sst_data.accidents[i];
        
        // Formater la date
        struct tm* timeinfo = localtime(&accident->date);
        char date_str[32];
        strftime(date_str, sizeof(date_str), "%d/%m/%Y %H:%M", timeinfo);
        
        snprintf(temp, sizeof(temp), 
                "%u. [%s] %s - %s\n",
                accident->id,
                accident->avec_arret ? "AVEC ARRET" : "SANS ARRET",
                date_str,
                accident->description);
        
        // Vérifier si on a assez de place
        if (strlen(buffer) + strlen(temp) >= buffer_size - 1) {
            strcat(buffer, "... (tronque)");
            break;
        }
        
        strcat(buffer, temp);
    }
    
    return SYS_OK;
}

// Obtenir les statistiques
SysError_t sst_get_statistics(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return SYS_INVALID_PARAM;
    }
    
    // Recalculer les valeurs à jour
    sst_calculate_taux_frequence();
    
    snprintf(buffer, buffer_size,
            "=== STATISTIQUES SST ===\n"
            "Jours sans accident: %u\n"
            "Record jours sans accident: %u\n"
            "Total accidents: %u\n"
            "  - Avec arret: %u\n"
            "  - Sans arret: %u\n"
            "Heures travaillees: %.2f\n"
            "Taux de frequence: %.2f\n"
            "Heure d'incrementation: %u\n"
            "Heures travaillees par jour: %.2f\n",
            sst_data.jours_sans_accident,
            sst_data.record_jours_sans_accident,
            sst_data.total_accidents,
            sst_data.accidents_avec_arret,
            sst_data.accidents_sans_arret,
            sst_data.heures_travaillees,
            sst_data.taux_frequence,
            sst_data.heure_incrementation,
            sst_data.heures_travaillees_par_jour);
    
    return SYS_OK;
}

// ============================================================================
// FONCTIONS DE SAUVEGARDE SÉCURISÉE (ISR-SAFE)
// ============================================================================

// Task de sauvegarde sécurisée
void sst_save_task(void* pvParameters) {
    SSTSaveMessage_t msg;
    
    SERIAL_PRINTLN_MINIMAL("SST Save Task: Started");
    
    for (;;) {
        // Attendre un message de la queue
        if (xQueueReceive(sst_save_queue, &msg, portMAX_DELAY) == pdTRUE) {
            switch (msg.type) {
                case SST_SAVE_MSG_DATA:
                    // Sauvegarder les données actuelles
                    sst_data_save();
                    SERIAL_PRINTLN_MINIMAL("SST Save Task: Data saved");
                    break;
                    
                case SST_SAVE_MSG_RESET:
                    // Réinitialiser les données
                    sst_data_reset();
                    SERIAL_PRINTLN_MINIMAL("SST Save Task: Data reset");
                    break;
                    
                default:
                    SERIAL_PRINTLN_MINIMAL("SST Save Task: Unknown message type");
                    break;
            }
        }
    }
}

// Initialiser le task de sauvegarde
SysError_t sst_save_task_init(void) {
    // Créer la queue
    sst_save_queue = xQueueCreate(SST_SAVE_QUEUE_SIZE, sizeof(SSTSaveMessage_t));
    if (sst_save_queue == NULL) {
        SERIAL_PRINTLN_MINIMAL("SST Save Task: Failed to create queue");
        return SYS_ERROR;
    }
    
    // Créer le task
    BaseType_t result = xTaskCreatePinnedToCore(
        sst_save_task,           // Fonction du task
        "sst_save_task",         // Nom du task
        4096,                    // Taille de la pile
        NULL,                    // Paramètres
        5,                       // Priorité (moyenne)
        &sst_save_task_handle,   // Handle du task
        1                        // Core 1 (même que les autres tasks)
    );
    
    if (result != pdPASS) {
        SERIAL_PRINTLN_MINIMAL("SST Save Task: Failed to create task");
        vQueueDelete(sst_save_queue);
        sst_save_queue = NULL;
        return SYS_ERROR;
    }
    
    SERIAL_PRINTLN_MINIMAL("SST Save Task: Initialized successfully");
    return SYS_OK;
}

// Déinitialiser le task de sauvegarde
void sst_save_task_deinit(void) {
    if (sst_save_task_handle != NULL) {
        vTaskDelete(sst_save_task_handle);
        sst_save_task_handle = NULL;
    }
    
    if (sst_save_queue != NULL) {
        vQueueDelete(sst_save_queue);
        sst_save_queue = NULL;
    }
    
    SERIAL_PRINTLN_MINIMAL("SST Save Task: Deinitialized");
}

// Demander une sauvegarde (ISR-safe)
SysError_t sst_request_save(void) {
    if (sst_save_queue == NULL) {
        return SYS_ERROR;
    }
    
    SSTSaveMessage_t msg;
    msg.type = SST_SAVE_MSG_DATA;
    
    BaseType_t result = xQueueSend(sst_save_queue, &msg, pdMS_TO_TICKS(1000));
    if (result != pdTRUE) {
        SERIAL_PRINTLN_MINIMAL("SST Save Task: Failed to send save request");
        return SYS_ERROR;
    }
    
    return SYS_OK;
}

// Demander l'ajout d'un accident (ISR-safe)
SysError_t sst_request_add_accident(bool avec_arret, const char* description) {
    if (sst_save_queue == NULL) {
        return SYS_ERROR;
    }
    
    // D'abord ajouter l'accident en mémoire (sans sauvegarder)
    SysError_t result = sst_add_accident_memory_only(avec_arret, description);
    if (result != SYS_OK) {
        return result;
    }
    
    // Puis demander la sauvegarde
    return sst_request_save();
}

// Demander une réinitialisation (ISR-safe)
SysError_t sst_request_reset(void) {
    if (sst_save_queue == NULL) {
        return SYS_ERROR;
    }
    
    SSTSaveMessage_t msg;
    msg.type = SST_SAVE_MSG_RESET;
    
    BaseType_t result = xQueueSend(sst_save_queue, &msg, pdMS_TO_TICKS(1000));
    if (result != pdTRUE) {
        SERIAL_PRINTLN_MINIMAL("SST Save Task: Failed to send reset request");
        return SYS_ERROR;
    }
    
    return SYS_OK;
}

// Ajouter un accident en mémoire seulement (sans sauvegarder)
SysError_t sst_add_accident_memory_only(bool avec_arret, const char* description) {
    if (sst_data.nb_accidents >= SST_MAX_ACCIDENTS) {
        SERIAL_PRINTLN_MINIMAL("SST Data: Maximum accidents reached");
        return SYS_ERROR;
    }
    
    // Obtenir l'heure actuelle avec protection
    time_t current_time = rtc_get_time();
    if (current_time == 0) {
        // Si RTC n'est pas disponible, utiliser l'heure système
        current_time = time(nullptr);
        if (current_time == 0) {
            // Si aucune heure n'est disponible, utiliser l'heure de compilation
            current_time = 1640995200; // 1er janvier 2022 par défaut
            SERIAL_PRINTLN_MINIMAL("SST Data: Using default time (no RTC/system time)");
        }
    }
    
    // Créer le nouvel accident
    Accident_t* accident = &sst_data.accidents[sst_data.nb_accidents];
    accident->id = sst_data.nb_accidents + 1;
    accident->date = current_time;
    accident->avec_arret = avec_arret;
    
    if (description) {
        strncpy(accident->description, description, sizeof(accident->description) - 1);
        accident->description[sizeof(accident->description) - 1] = '\0';
    } else {
        strcpy(accident->description, "Accident");
    }
    
    // Mettre à jour les compteurs
    sst_data.total_accidents++;
    sst_data.date_dernier_accident = current_time;
    sst_data.jours_sans_accident = 0; // Réinitialisé à 0
    
    if (avec_arret) {
        sst_data.accidents_avec_arret++;
        // Recalculer le taux de fréquence
        sst_calculate_taux_frequence();
    } else {
        sst_data.accidents_sans_arret++;
    }
    
    sst_data.nb_accidents++;
    
    SERIAL_PRINTLN_MINIMAL("SST Data: Accident added to memory");
    return SYS_OK;
}

// Vérifier s'il y a eu des accidents dans la période métier précédente
bool sst_check_accidents_in_metier_period(void) {
    // Obtenir l'heure actuelle
    time_t current_time = rtc_get_time();
    if (current_time == 0) {
        current_time = time(nullptr);
    }
    if (current_time == 0) {
        SERIAL_PRINTLN_MINIMAL("SST Data: No time source available for accident check");
        return false; // En cas de doute, on considère qu'il n'y a pas d'accident
    }
    
    // Calculer le début de la période métier précédente
    // Période métier = de l'heure d'incrémentation d'hier à l'heure d'incrémentation d'aujourd'hui
    struct tm* tm_info = localtime(&current_time);
    
    // Extraire l'heure d'incrémentation configurée
    uint32_t target_hour = sst_data.heure_incrementation / 100;
    uint32_t target_minute = sst_data.heure_incrementation % 100;
    
    // Définir l'heure d'incrémentation d'aujourd'hui
    tm_info->tm_hour = target_hour;
    tm_info->tm_min = target_minute;
    tm_info->tm_sec = 0;
    time_t today_increment_time = mktime(tm_info);
    
    // Si l'heure d'incrémentation est déjà passée aujourd'hui, 
    // la période métier a commencé hier à la même heure
    time_t period_start;
    if (today_increment_time <= current_time) {
        // Période métier = d'hier à l'heure d'incrémentation jusqu'à aujourd'hui à l'heure d'incrémentation
        period_start = today_increment_time - 86400; // -24h
    } else {
        // Période métier = d'avant-hier à l'heure d'incrémentation jusqu'à hier à l'heure d'incrémentation
        period_start = today_increment_time - (2 * 86400); // -48h
    }
    
    SERIAL_PRINTF_MINIMAL("SST Data: Checking accidents in metier period from %s to %s\n", 
                         ctime(&period_start), ctime(&today_increment_time));
    
    // Vérifier tous les accidents dans cette période
    for (uint32_t i = 0; i < sst_data.nb_accidents; i++) {
        Accident_t* accident = &sst_data.accidents[i];
        
        if (accident->date >= period_start && accident->date < today_increment_time) {
            SERIAL_PRINTF_MINIMAL("SST Data: Accident found in metier period: ID %u, Date: %s\n", 
                                 accident->id, ctime(&accident->date));
            return true; // Accident trouvé dans la période métier
        }
    }
    
    SERIAL_PRINTLN_MINIMAL("SST Data: No accidents found in metier period");
    return false; // Aucun accident dans la période métier
}
