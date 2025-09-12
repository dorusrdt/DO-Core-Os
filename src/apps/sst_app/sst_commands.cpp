#include "sst_commands.h"
#include "sst_data.h"
#include <string.h>
#include <stdlib.h>
#include "../../kernel/hal/time_sync_manager.h"

// Commande: sst_accident_avec_arret [description]
SysError_t cmd_sst_accident_avec_arret(int argc, char* argv[]) {
    if (argc < 1) {
        Serial.println("Usage: sst_accident_avec_arret [description]");
        return SYS_INVALID_PARAM;
    }
    
    const char* description = (argc > 1) ? argv[1] : "Accident avec arret";
    
    // Utiliser la fonction directe (AVEC PERSISTANCE)
    SysError_t result = sst_add_accident(true, description);
    if (result == SYS_OK) {
        Serial.println("Accident avec arret ajoute avec succes");
    } else {
        Serial.println("Erreur lors de l'ajout de l'accident");
    }
    
    return result;
}

// Commande: sst_accident_sans_arret [description]
SysError_t cmd_sst_accident_sans_arret(int argc, char* argv[]) {
    if (argc < 1) {
        Serial.println("Usage: sst_accident_sans_arret [description]");
        return SYS_INVALID_PARAM;
    }
    
    const char* description = (argc > 1) ? argv[1] : "Accident sans arret";
    
    // Utiliser la fonction directe (AVEC PERSISTANCE)
    SysError_t result = sst_add_accident(false, description);
    if (result == SYS_OK) {
        Serial.println("Accident sans arret ajoute avec succes");
    } else {
        Serial.println("Erreur lors de l'ajout de l'accident");
    }
    
    return result;
}

// Commande: sst_config_heures HH MM
SysError_t cmd_sst_config_heures(int argc, char* argv[]) {
    if (argc != 3) {
        Serial.println("Usage: sst_config_heures HH MM");
        Serial.println("Exemple: sst_config_heures 8 30");
        return SYS_INVALID_PARAM;
    }
    
    uint32_t heure = strtoul(argv[1], NULL, 10);
    uint32_t minute = strtoul(argv[2], NULL, 10);
    
    if (heure > 23) {
        Serial.println("Erreur: L'heure doit etre entre 0 et 23");
        return SYS_INVALID_PARAM;
    }
    
    if (minute > 59) {
        Serial.println("Erreur: La minute doit etre entre 0 et 59");
        return SYS_INVALID_PARAM;
    }
    
    // Calculer le timestamp pour aujourd'hui à l'heure spécifiée
    time_t current_time = time_sync_get_current_time();
    if (current_time == 0) {
        Serial.println("Erreur: Aucune source de temps disponible (NTP/RTC/System)");
        return SYS_ERROR;
    }
    struct tm* tm_info = localtime(&current_time);
    
    // Définir l'heure et la minute
    tm_info->tm_hour = heure;
    tm_info->tm_min = minute;
    tm_info->tm_sec = 0;
    
    time_t increment_time = mktime(tm_info);
    
    // Si l'heure est déjà passée aujourd'hui, programmer pour demain
    if (increment_time <= current_time) {
        increment_time += 86400; // +1 jour
    }
    
    sst_data.derniere_incrementation = increment_time;
    
    // Stocker l'heure d'incrémentation (format HHMM)
    sst_data.heure_incrementation = heure * 100 + minute;
    
    // Sauvegarder la configuration
    SysError_t result = sst_data_save();
    if (result == SYS_OK) {
        Serial.printf("Incrementation programmee: %02u:%02u\n", heure, minute);
        Serial.printf("Prochaine incrementation: %s", ctime(&increment_time));
    } else {
        Serial.println("Erreur lors de la sauvegarde");
    }
    
    return result;
}

// Commande: sst_config_heures_travail HOURS
SysError_t cmd_sst_config_heures_travail(int argc, char* argv[]) {
    if (argc != 2) {
        Serial.println("Usage: sst_config_heures_travail HOURS");
        Serial.println("Exemple: sst_config_heures_travail 8.5");
        return SYS_INVALID_PARAM;
    }
    
    float heures = atof(argv[1]);
    
    if (heures <= 0 || heures > 24) {
        Serial.println("Erreur: Les heures doivent etre entre 0 et 24");
        return SYS_INVALID_PARAM;
    }
    
    sst_data.heures_travaillees_par_jour = heures;
    
    // Sauvegarder la configuration
    SysError_t result = sst_data_save();
    if (result == SYS_OK) {
        Serial.printf("Heures travaillees par jour configurees: %.2f heures\n", heures);
    } else {
        Serial.println("Erreur lors de la sauvegarde");
    }
    
    return result;
}

// Commande: sst_liste_accidents
SysError_t cmd_sst_liste_accidents(int argc, char* argv[]) {
    char buffer[2048];
    SysError_t result = sst_get_accidents_list(buffer, sizeof(buffer));
    
    if (result == SYS_OK) {
        Serial.println(buffer);
    } else {
        Serial.println("Erreur lors de la recuperation de la liste des accidents");
    }
    
    return result;
}

// Commande: sst_statistiques
SysError_t cmd_sst_statistiques(int argc, char* argv[]) {
    char buffer[1024];
    SysError_t result = sst_get_statistics(buffer, sizeof(buffer));
    
    if (result == SYS_OK) {
        Serial.println(buffer);
    } else {
        Serial.println("Erreur lors de la recuperation des statistiques");
    }
    
    return result;
}

// Commande: sst_reset (SANS CONFIRMATION)
SysError_t cmd_sst_reset(int argc, char* argv[]) {
    Serial.println("Reinitialisation des donnees SST...");
    
    // Utiliser la fonction directe (AVEC PERSISTANCE)
    SysError_t result = sst_data_reset();
    if (result == SYS_OK) {
        Serial.println("Donnees SST reinitialisees avec succes");
    } else {
        Serial.println("Erreur lors de la reinitialisation");
    }
    
    return result;
}

// Commande: sst_status
SysError_t cmd_sst_status(int argc, char* argv[]) {
    Serial.println("=== STATUS SST ===");
    Serial.printf("Jours sans accident: %u\n", sst_data.jours_sans_accident);
    Serial.printf("Total accidents: %u\n", sst_data.total_accidents);
    Serial.printf("  - Avec arret: %u\n", sst_data.accidents_avec_arret);
    Serial.printf("  - Sans arret: %u\n", sst_data.accidents_sans_arret);
    Serial.printf("Heures travaillees: %.2f\n", sst_data.heures_travaillees);
    Serial.printf("Taux de frequence: %.2f\n", sst_data.taux_frequence);
    Serial.printf("Heure d'incrementation: %u\n", sst_data.heure_incrementation);
    Serial.printf("Heures travaillees par jour: %.2f\n", sst_data.heures_travaillees_par_jour);
    
    return SYS_OK;
}
