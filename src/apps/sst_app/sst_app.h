#ifndef SST_APP_H
#define SST_APP_H

#include "../../kernel/core/kernel.h"
#include "../../kernel/app/app_manager.h"
#include "sst_data.h"
#include "DMD32.h"
#include "../../../lib/DMD32-main/fonts/SystemFont5x7.h"

// Configuration de l'application SST
#define SST_APP_NAME "SST"
#define SST_APP_DESCRIPTION "SST Application - Hello World"

// Fonctions publiques de l'application SST
SysError_t sst_app_init(void);
void sst_app_start(void);
void sst_app_stop(void);
void sst_app_pause(void);
void sst_app_resume(void);
void sst_app_loop(void);

// Fonction d'enregistrement de l'application
SysError_t sst_app_register(uint8_t* app_id);

// Fonctions DMD
void sst_dmd_init(void);
void sst_dmd_display_days_without_accident(void);
void sst_dmd_display_text(const char* text);
void sst_dmd_clear_screen(void);

#endif // SST_APP_H
