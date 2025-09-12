#ifndef SST_COMMANDS_H
#define SST_COMMANDS_H

#include "../../kernel/core/kernel.h"

// Commandes SST pour le shell
SysError_t cmd_sst_accident_avec_arret(int argc, char* argv[]);
SysError_t cmd_sst_accident_sans_arret(int argc, char* argv[]);
SysError_t cmd_sst_config_heures(int argc, char* argv[]);
SysError_t cmd_sst_config_heures_travail(int argc, char* argv[]);
SysError_t cmd_sst_liste_accidents(int argc, char* argv[]);
SysError_t cmd_sst_statistiques(int argc, char* argv[]);
SysError_t cmd_sst_reset(int argc, char* argv[]);
SysError_t cmd_sst_status(int argc, char* argv[]);

#endif // SST_COMMANDS_H


