# Template d'Application pour DO-Core-OS

Ce dossier contient un template d'application qui sert de base pour créer de nouvelles applications pour DO-Core-OS. Cette documentation est destinée aux développeurs et aux agents IA qui souhaitent étendre les fonctionnalités du système.

## Structure du Template

- `example_app.h` : En-tête définissant l'interface de l'application
  - Définition des structures de configuration
  - Déclaration des fonctions publiques
  - Constantes et types spécifiques à l'application

- `example_app.cpp` : Implémentation de l'application
  - Implémentation des callbacks (init, start, loop, stop)
  - Gestion de l'état de l'application
  - Logique métier

## Guide Étape par Étape pour Créer une Nouvelle Application

1. **Création du dossier et des fichiers**
   ```bash
   cp -r example_app/ my_new_app/
   cd my_new_app/
   mv example_app.h my_new_app.h
   mv example_app.cpp my_new_app.cpp
   ```

2. **Modification des fichiers**
   - Remplacez tous les "example_app" par "my_new_app"
   - Mettez à jour les gardes d'en-tête (#ifndef, #define)
   - Personnalisez la structure de configuration selon vos besoins

3. **Implémentation des Callbacks**
   ```cpp
   // Initialisation : appelé une seule fois au chargement
   SysError_t my_new_app_init(void) {
       // Initialisez vos ressources ici
       return SYS_OK;
   }

   // Démarrage : appelé quand l'application démarre
   void my_new_app_start(void) {
       // Code de démarrage
   }

   // Boucle principale : appelée périodiquement
   void my_new_app_loop(void) {
       // Logique principale
       vTaskDelay(pdMS_TO_TICKS(1000)); // Délai important !
   }

   // Arrêt : appelé quand l'application s'arrête
   void my_new_app_stop(void) {
       // Nettoyage des ressources
   }
   ```

## Intégration avec le Système

1. **Dans main.cpp**
   ```cpp
   #include "apps/my_new_app/my_new_app.h"

   void setup() {
       // ... autres initialisations ...
       
       // Configuration de l'application
       MyNewAppConfig_t config = {
           .param1 = 42,
           .param2 = true
       };
       
       // Enregistrement de l'application
       register_my_new_app(&config);
   }
   ```

2. **ID d'Application**
   - L'ID est attribué automatiquement lors de l'enregistrement (1-255)
   - L'ID est unique pendant toute la durée de vie du système
   - Stockez l'ID si vous devez interagir avec l'application plus tard

## Commandes du Noyau pour Gérer l'Application

Les commandes suivantes sont disponibles dans le shell du système :

```
app_status               - Affiche l'état de toutes les applications
app_list                - Liste les applications enregistrées
app_start <id>          - Démarre l'application avec l'ID spécifié
app_stop <id>           - Arrête l'application
app_restart <id>        - Redémarre l'application
app_pause <id>          - Met en pause l'application
app_resume <id>         - Reprend l'exécution de l'application
app_info <id>           - Affiche les détails de l'application

app_start_all           - Démarre toutes les applications
app_stop_all            - Arrête toutes les applications
app_pause_all           - Met en pause toutes les applications
app_resume_all          - Reprend toutes les applications
```

## États Possibles d'une Application

- `APP_STATE_UNLOADED` : Application non chargée
- `APP_STATE_LOADING` : En cours de chargement
- `APP_STATE_RUNNING` : En cours d'exécution
- `APP_STATE_PAUSED` : En pause
- `APP_STATE_STOPPED` : Arrêtée
- `APP_STATE_ERROR` : Erreur survenue

## Bonnes Pratiques

1. **Gestion de la Mémoire**
   - Libérez toutes les ressources allouées
   - Utilisez les allocations statiques quand possible
   - Vérifiez les fuites mémoire avec `memory` dans le shell

2. **Performance**
   - Incluez toujours un délai dans la boucle principale
   - Évitez les opérations bloquantes
   - Utilisez `tasks` pour surveiller l'utilisation CPU

3. **Robustesse**
   - Validez tous les paramètres d'entrée
   - Gérez les cas d'erreur
   - Implémentez un mécanisme de reprise sur erreur

4. **Debug**
   - Utilisez kernel_log() pour les messages de debug
   - Niveaux disponibles : ERROR, WARN, INFO, DEBUG
   - Activez l'écho des logs avec `log_echo on`

## Exemple d'Utilisation via le Shell

```bash
# Vérifier l'état du système
status

# Lister les applications
app_list

# Démarrer votre application (si ID = 1)
app_start 1

# Vérifier son état
app_info 1

# Surveiller les logs
log_echo on
```

Pour plus d'informations sur les APIs du système, consultez la documentation dans le dossier `doc/` à la racine du projet.
