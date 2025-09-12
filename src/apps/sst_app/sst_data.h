#ifndef SST_DATA_H
#define SST_DATA_H

#include "../../kernel/core/kernel.h"
#include <time.h>
#include <Preferences.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

// Constantes SST
#define SST_NVS_NAMESPACE "sst_data"
#define SST_MAX_ACCIDENTS 100
#define SST_DEFAULT_HOURS_PER_DAY 8

// Queue pour la sauvegarde sécurisée
#define SST_SAVE_QUEUE_SIZE 5

// Types de messages pour la queue de sauvegarde
typedef enum {
    SST_SAVE_MSG_DATA,        // Sauvegarder les données SST
    SST_SAVE_MSG_RESET        // Réinitialiser les données
} SSTSaveMsgType_t;

// Structure de message pour la queue
typedef struct {
    SSTSaveMsgType_t type;
    bool avec_arret;          // Pour les accidents
    char description[64];     // Description de l'accident
} SSTSaveMessage_t;

// Structure pour un accident
typedef struct {
    uint32_t id;
    time_t date;
    bool avec_arret;
    char description[64];
} Accident_t;

// Structure principale des données SST
typedef struct {
    // Compteurs d'accidents
    uint32_t total_accidents;
    uint32_t accidents_avec_arret;
    uint32_t accidents_sans_arret;
    
    // Dates
    time_t date_dernier_accident;
    uint32_t jours_sans_accident;
    uint32_t record_jours_sans_accident;
    
    // Calculs
    uint32_t heures_travaillees;
    float taux_frequence;
    
    // Configuration
    uint32_t heures_par_jour;
    time_t derniere_incrementation;
    
    // Liste des accidents
    Accident_t accidents[SST_MAX_ACCIDENTS];
    uint32_t nb_accidents;
} SSTData_t;

// Fonctions de gestion des données SST
SysError_t sst_data_init(void);
SysError_t sst_data_load(void);
SysError_t sst_data_save(void);
SysError_t sst_data_reset(void);
SysError_t sst_data_reset_memory_only(void);

// Fonctions de calcul
SysError_t sst_calculate_taux_frequence(void);

// Fonctions d'accidents
SysError_t sst_add_accident(bool avec_arret, const char* description);
SysError_t sst_get_accidents_list(char* buffer, size_t buffer_size);
SysError_t sst_get_statistics(char* buffer, size_t buffer_size);

// Fonctions de sauvegarde sécurisée
SysError_t sst_save_task_init(void);
void sst_save_task_deinit(void);
SysError_t sst_request_save(void);
SysError_t sst_request_add_accident(bool avec_arret, const char* description);
SysError_t sst_request_reset(void);
SysError_t sst_add_accident_memory_only(bool avec_arret, const char* description);

// Variables globales
extern SSTData_t sst_data;

// Queue et task pour la sauvegarde sécurisée
extern QueueHandle_t sst_save_queue;
extern TaskHandle_t sst_save_task_handle;

#endif // SST_DATA_H
