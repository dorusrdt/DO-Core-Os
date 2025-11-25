╔════════════════════════════════════════════════════════════════════════════╗
║              📖 GUIDE COMPLET - UTILISATION create_test_zones.sh            ║
╚════════════════════════════════════════════════════════════════════════════╝

📍 LOCATION DU SCRIPT
═════════════════════════════════════════════════════════════════════════════
/home/dorus/Documents/GitHub/DO-Core-Os/test_server/create_test_zones.sh


🎯 OBJECTIF DU SCRIPT
═════════════════════════════════════════════════════════════════════════════

Automatise la création de 4 zones de test avec toutes les configurations:
  • Potager Nord (Tomates)
  • Jardin Sud (Laitue)
  • Serre (Carottes)
  • Verger (Arbres Fruitiers)

Chaque zone inclut:
  ✅ physicalZoneNumber (1-4) - numéro du relais
  ✅ irrigationSchedule - horaires avec jours de la semaine
  ✅ sensors - capteurs d'humidité
  ✅ Configuration complète pour l'ESP32


═════════════════════════════════════════════════════════════════════════════
⚙️  PRÉ-REQUIS AVANT D'UTILISER
═════════════════════════════════════════════════════════════════════════════

1. ✅ Le serveur DOIT être démarré
   $ python3 irrigation_server.py
   ou
   $ ./start_irrigation_server.sh

2. ✅ Le serveur DOIT écouter sur: http://localhost:3000
   (Vérifiez dans start_irrigation_server.sh ou irrigation_server.py)

3. ✅ Le device DOIT être enregistré AVANT de créer les zones
   $ curl -X POST http://localhost:3000/api/devices/register \
     -H "Content-Type: application/json" \
     -d '{
       "type": "device",
       "deviceId": "ESP32_IRRIGATION_11100454456464674",
       "capacity": {"zones": 4, "sensors": 12},
       "timestamp": "2025-11-25T00:00:00"
     }'


═════════════════════════════════════════════════════════════════════════════
🚀 UTILISATION DU SCRIPT
═════════════════════════════════════════════════════════════════════════════

OPTION 1: Utilisation simple (RECOMMANDÉE)
───────────────────────────────────────────────────────────────────────────

Exécutez simplement sans arguments:

$ ./create_test_zones.sh

Cela créera les 4 zones avec toutes les configurations.

Output attendu:
  ========================================
  🌱 Creating Test Zones (v2.0)
  ========================================

  Server: http://localhost:3000
  Device: ESP32_IRRIGATION_11100454456464674

  1️⃣  Checking device registration...
  ✅ Device found!

  2️⃣  Creating Zone: Potager Nord (Tomates)...
  {
    "status": "success",
    "zone": {...}
  }

  3️⃣  Creating Zone: Jardin Sud (Laitue)...
  [...]

  ✅ Test Zones Created!


OPTION 2: Utilisation avec parametres (si le script le supporte)
───────────────────────────────────────────────────────────────────────────

Vérifiez le début du script pour voir les options disponibles:

$ cat create_test_zones.sh | head -20

Si le script accepte des arguments:
  $ ./create_test_zones.sh 1-zone      # Crée 1 zone simple
  $ ./create_test_zones.sh 2-zones     # Crée 2 zones
  $ ./create_test_zones.sh 4-zones     # Crée 4 zones (complet)


═════════════════════════════════════════════════════════════════════════════
📋 ÉTAPES D'UTILISATION COMPLÈTES
═════════════════════════════════════════════════════════════════════════════

ÉTAPE 1: Naviguer jusqu'au dossier
────────────────────────────────────────────────────────────────────────────
$ cd /home/dorus/Documents/GitHub/DO-Core-Os/test_server


ÉTAPE 2: Vérifier que le script est exécutable
────────────────────────────────────────────────────────────────────────────
$ ls -la create_test_zones.sh
-rwxrwxr-x  ... create_test_zones.sh

Si pas d'exécution (pas de x):
$ chmod +x create_test_zones.sh


ÉTAPE 3: Démarrer le serveur (si pas déjà fait)
────────────────────────────────────────────────────────────────────────────
Terminal 1:
$ python3 irrigation_server.py
ou
$ ./start_irrigation_server.sh

Vérifier que le serveur démarre:
  🚀 ESP32 Irrigation Server v2.0.1
  🌐 Server: http://0.0.0.0:3000
  📝 Docs: http://0.0.0.0:3000/docs


ÉTAPE 4: Enregistrer le device (Terminal 2 - nouveau terminal)
────────────────────────────────────────────────────────────────────────────
$ curl -X POST http://localhost:3000/api/devices/register \
  -H "Content-Type: application/json" \
  -d '{
    "type": "device",
    "deviceId": "ESP32_IRRIGATION_11100454456464674",
    "capacity": {"zones": 4, "sensors": 12},
    "timestamp": "2025-11-25T00:00:00",
    "latitude": 48.8566,
    "longitude": 2.3522
  }'

Réponse attendue:
{
  "status": "registered",
  "deviceId": "ESP32_IRRIGATION_11100454456464674",
  "message": "Device registered successfully"
}


ÉTAPE 5: Exécuter le script de création de zones
────────────────────────────────────────────────────────────────────────────
$ ./create_test_zones.sh

Le script va:
  1. Vérifier que le device existe
  2. Créer Zone 1 (Potager Nord)
  3. Créer Zone 2 (Jardin Sud)
  4. Créer Zone 3 (Serre)
  5. Créer Zone 4 (Verger)
  6. Afficher les résultats JSON
  7. Récupérer et afficher la config finale


ÉTAPE 6: Vérifier que les zones ont été créées
────────────────────────────────────────────────────────────────────────────
$ curl http://localhost:3000/api/devices/ESP32_IRRIGATION_11100454456464674/zones | jq

Réponse: Liste des 4 zones avec tous les champs


ÉTAPE 7: Récupérer la configuration pour l'ESP32
────────────────────────────────────────────────────────────────────────────
$ curl http://localhost:3000/api/devices/ESP32_IRRIGATION_11100454456464674/config | jq

Réponse: Configuration complète avec toutes les zones


═════════════════════════════════════════════════════════════════════════════
📊 DONNÉES CRÉÉES PAR LE SCRIPT
═════════════════════════════════════════════════════════════════════════════

Zone 1: Potager Nord (Tomates)
├─ relais physique: 1
├─ eau par jour: 2000 ml
├─ seuil humidité: 80%
├─ capteurs: s_01, s_02, s_03
├─ Horaire 1: Lun/Mer/Ven 08:00 → 15 min
└─ Horaire 2: Dim/Mar/Jeu/Sam 18:00 → 20 min

Zone 2: Jardin Sud (Laitue)
├─ relais physique: 2
├─ eau par jour: 3000 ml
├─ seuil humidité: 70%
├─ capteurs: s_04, s_05, s_06
├─ Horaire 1: Lun-Ven 06:00 → 20 min
└─ Horaire 2: Sam/Dim 20:00 → 15 min

Zone 3: Serre (Carottes)
├─ relais physique: 3
├─ eau par jour: 1500 ml
├─ seuil humidité: 75%
├─ capteurs: s_07, s_08, s_09
└─ Horaire 1: Lun/Mer/Ven 10:00 → 10 min

Zone 4: Verger (Arbres Fruitiers)
├─ relais physique: 4
├─ eau par jour: 1000 ml
├─ seuil humidité: 60%
├─ capteurs: s_10, s_11, s_12
└─ Horaire 1: Tous les jours 07:00 → 25 min


═════════════════════════════════════════════════════════════════════════════
🔍 VÉRIFICATION APRÈS EXÉCUTION
═════════════════════════════════════════════════════════════════════════════

✅ Les zones ont été créées avec succès si:

1. Le script affiche "✅ Test Zones Created!"

2. curl http://localhost:3000/api/devices/{id}/zones retourne 4 zones

3. Chaque zone contient:
   {
     "zoneId": "zone_...",
     "physicalZoneNumber": 1-4,
     "waterPerDay": xxx,
     "humidityThreshold": xx,
     "sensors": [...],
     "irrigationSchedule": [...]
   }

4. Config récupérable:
   curl http://localhost:3000/api/devices/{id}/config
   └─ Retourne "type": "config" avec zones


═════════════════════════════════════════════════════════════════════════════
⚠️  DÉPANNAGE
═════════════════════════════════════════════════════════════════════════════

ERREUR: "Device not registered"
───────────────────────────────────────────────────────────────────────────
CAUSE: Le device n'a pas été enregistré avant de créer les zones
SOLUTION:
  1. D'abord: $ curl -X POST http://localhost:3000/api/devices/register ...
  2. Ensuite: $ ./create_test_zones.sh


ERREUR: "curl: (7) Failed to connect to localhost port 3000"
───────────────────────────────────────────────────────────────────────────
CAUSE: Le serveur n'est pas en cours d'exécution
SOLUTION:
  1. Terminal 1: $ python3 irrigation_server.py
  2. Terminal 2: $ ./create_test_zones.sh


ERREUR: "command not found: ./create_test_zones.sh"
───────────────────────────────────────────────────────────────────────────
CAUSE: Le script n'est pas exécutable
SOLUTION:
  $ chmod +x create_test_zones.sh
  $ ./create_test_zones.sh


ERREUR: JSON parsing error ou validation error
───────────────────────────────────────────────────────────────────────────
CAUSE: Le format JSON dans le script peut être invalide
SOLUTION:
  1. Vérifier la syntaxe: $ bash -n create_test_zones.sh
  2. Vérifier les logs du serveur
  3. Tester avec curl manuel:
     $ curl -X POST http://localhost:3000/api/devices/{id}/zones \
       -H "Content-Type: application/json" \
       -d '{"zoneId": "test", "physicalZoneNumber": 1, ...}'


═════════════════════════════════════════════════════════════════════════════
💡 CONSEILS D'UTILISATION
═════════════════════════════════════════════════════════════════════════════

1. Utilisez deux terminaux:
   Terminal 1: Serveur (python3 irrigation_server.py)
   Terminal 2: Scripts et tests (curl, create_test_zones.sh)

2. Toujours vérifier que le serveur est en cours d'exécution:
   $ curl http://localhost:3000/health

3. Pour déboguer, utilisez -v avec curl:
   $ curl -v http://localhost:3000/api/devices/.../zones

4. Visualisez la réponse JSON:
   $ curl http://localhost:3000/api/devices/.../zones | jq '.'

5. Le script est idempotent (peut être exécuté plusieurs fois)
   └─ Les zones existantes seront mises à jour si les IDs correspondent


═════════════════════════════════════════════════════════════════════════════
📚 FICHIERS CONNEXES
═════════════════════════════════════════════════════════════════════════════

- irrigation_server.py: Serveur principal
- start_irrigation_server.sh: Script pour démarrer le serveur
- quick_validation_test.sh: Tests automatisés
- ZONE_CONFIG_COMPARISON.txt: Documentation des champs
- README_ZONE_CONFIG_UPDATE.md: Guide global

═════════════════════════════════════════════════════════════════════════════

Questions fréquentes:
Q: Puis-je exécuter le script plusieurs fois?
R: Oui, il est idempotent. Les zones seront mises à jour.

Q: Puis-je créer d'autres zones?
R: Oui, editez le script et ajoutez d'autres appels curl POST /zones

Q: Est-ce que le script crée les capteurs?
R: Non, il les liste seulement. Les capteurs sont virtuels.

Q: Puis-je customiser les zones?
R: Oui, modifiez les valeurs dans les appels curl du script.

═════════════════════════════════════════════════════════════════════════════
