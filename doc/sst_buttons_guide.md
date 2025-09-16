# Guide des Boutons SST - D'O-Core OS

## 🎯 **Vue d'ensemble**

Le système de boutons SST permet de contrôler manuellement le compteur de jours sans accident directement via des boutons physiques, avec des protections contre les actions multiples.

## 🔌 **Configuration Matérielle**

### Connexions des Boutons
```
ESP32 Pin 4 ←→ Bouton 1 ←→ GND
ESP32 Pin 5 ←→ Bouton 2 ←→ GND
```

**Note** : Les boutons utilisent la résistance de pull-up interne de l'ESP32.

## 🎮 **Fonctionnalités des Boutons**

### Bouton 1 (Pin 4) - Incrémentation
- **Clic court** : +1 jour sans accident
- **Pression longue (2s)** : Reset du compteur à 0

### Bouton 2 (Pin 5) - Décrémentation & Accidents
- **Clic court** : -1 jour sans accident
- **Pression longue (2s)** : Ajouter un accident avec arrêt

## 🛡️ **Protections Intégrées**

### Protection Anti-Spam
- **Délai minimum** : 2 secondes entre chaque action
- **Actions protégées** :
  - Incrémentation
  - Décrémentation
  - Reset
  - Ajout d'accident

### Messages de Protection
```
SST Buttons: Increment blocked - too soon
SST Buttons: Decrement blocked - too soon
SST Buttons: Reset blocked - too soon
SST Buttons: Accident blocked - too soon
```

## 💾 **Persistance des Données**

### Sauvegarde Automatique
Toutes les actions manuelles sont **automatiquement sauvegardées** :
- ✅ Incrémentation manuelle → Sauvegarde immédiate
- ✅ Décrémentation manuelle → Sauvegarde immédiate
- ✅ Reset manuel → Sauvegarde immédiate
- ✅ Accident avec arrêt → Sauvegarde immédiate

### Données Sauvegardées
- Compteur de jours sans accident
- Heures travaillées totales
- Record historique
- Taux de fréquence (recalculé automatiquement)
- Timestamp de dernière incrémentation

## 🔧 **Commandes CLI**

### Vérifier le Statut
```bash
sst_buttons_status
```

**Sortie :**
```
=== SST Buttons Status ===
Buttons enabled: YES
Increment button: Pin 4
Decrement button: Pin 5

Button functions:
  - Increment button: Click = +1 day, Long press = Reset
  - Decrement button: Click = -1 day, Long press = Accident avec arret
Protection: 2 secondes entre chaque action
=========================
```

## 📊 **Impact sur les Statistiques**

### Incrémentation Manuelle
- **Jours sans accident** : +1
- **Heures travaillées** : +heures_travaillees_par_jour
- **Record** : Mis à jour si nouveau record
- **Taux de fréquence** : Recalculé

### Décrémentation Manuelle
- **Jours sans accident** : -1 (minimum 0)
- **Heures travaillées** : -heures_travaillees_par_jour
- **Taux de fréquence** : Recalculé

### Reset Manuel
- **Jours sans accident** : 0
- **Taux de fréquence** : Recalculé

### Accident avec Arrêt
- **Jours sans accident** : 0
- **Total accidents** : +1
- **Accidents avec arrêt** : +1
- **Taux de fréquence** : Recalculé
- **Date dernier accident** : Timestamp actuel

## 🚀 **Utilisation Pratique**

### Scénario 1 : Ajout d'un Jour de Travail
1. Appuyer brièvement sur le bouton 1 (Pin 4)
2. Le compteur s'incrémente de 1
3. Les heures travaillées s'ajoutent automatiquement
4. Sauvegarde immédiate

### Scénario 2 : Correction d'Erreur
1. Appuyer brièvement sur le bouton 2 (Pin 5)
2. Le compteur se décrémente de 1
3. Les heures travaillées se soustraient
4. Sauvegarde immédiate

### Scénario 3 : Reset Complet
1. Maintenir le bouton 1 (Pin 4) pendant 2 secondes
2. Le compteur se remet à 0
3. Sauvegarde immédiate

### Scénario 4 : Déclaration d'Accident
1. Maintenir le bouton 2 (Pin 5) pendant 2 secondes
2. Un accident avec arrêt est ajouté
3. Le compteur se remet à 0
4. Sauvegarde immédiate

## ⚠️ **Points d'Attention**

### Limitations
- **Délai de protection** : 2 secondes entre les actions
- **Décrémentation** : Impossible si déjà à 0
- **Boutons désactivés** : Si l'app SST n'est pas démarrée

### Logs de Debug
Toutes les actions sont loggées :
```
SST Buttons: Increment button clicked
SST Data: Manual increment successful - Days: 5, Hours: 40.0
SST Buttons: Decrement button long pressed - ACCIDENT WITH STOP
SST Data: Accident added to memory
```

## 🔄 **Intégration avec le Système**

### Démarrage Automatique
- Les boutons sont initialisés avec l'app SST
- Actifs dès le démarrage de l'application
- Désactivés à l'arrêt de l'application

### Synchronisation
- Les actions manuelles sont synchronisées avec l'incrémentation automatique
- Le système respecte la logique métier existante
- Compatible avec la détection d'accidents dans la période métier

## 🎉 **Avantages**

1. **Contrôle Immédiat** : Actions instantanées sans CLI
2. **Protection Anti-Spam** : Évite les actions accidentelles multiples
3. **Persistance Garantie** : Toutes les modifications sont sauvegardées
4. **Intégration Parfaite** : Compatible avec le système existant
5. **Interface Simple** : 2 boutons pour toutes les fonctions essentielles
