#ifndef EXAMPLE_APP_H
#define EXAMPLE_APP_H

#include "../../kernel/app/app_manager.h"

// Structure de configuration de l'application exemple
typedef struct {
    // Ajoutez vos paramètres de configuration ici
    int param1;
    bool param2;
} ExampleAppConfig_t;

// Déclaration des fonctions de l'application
SysError_t example_app_init(void);
void example_app_start(void);
void example_app_loop(void);
void example_app_stop(void);

// Fonction d'enregistrement de l'application
SysError_t register_example_app(const ExampleAppConfig_t* config);

#endif // EXAMPLE_APP_H
