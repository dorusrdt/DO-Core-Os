# Application SST - Écran P10 32x16

## Description

L'application SST est une application pour le mini OS D'O-Core qui interfacer un écran LED P10 32x16 pixels. Elle affiche un compteur qui s'incrémente en temps réel sur l'écran.

## Fonctionnalités

- ✅ Affichage d'un compteur en temps réel sur l'écran P10
- ✅ Support des polices Arial_Black_16 et System5x7
- ✅ Rafraîchissement automatique de l'écran via timer hardware
- ✅ Gestion propre des ressources (initialisation/arrêt)
- ✅ Intégration complète avec l'app manager du kernel

## Configuration Matérielle

### Connexions ESP32 - Écran P10

L'écran P10 doit être connecté selon la configuration DMD32 :

| Pin ESP32 | Pin P10 | Description |
|-----------|---------|-------------|
| D22       | OE      | Output Enable (active low) |
| D19       | A       | Adresse ligne A |
| D21       | B       | Adresse ligne B |
| D18       | CLK     | Horloge SPI (SCK) |
| D2        | SCLK    | Horloge latch |
| D23       | R_DATA  | Données SPI (MOSI) |
| 3.3V      | VCC     | Alimentation |
| GND       | GND     | Masse |

### Caractéristiques de l'écran

- **Résolution** : 32x16 pixels
- **Type** : LED Matrix P10
- **Communication** : SPI (VSPI)
- **Fréquence de rafraîchissement** : 300µs (configurable)

## Utilisation

### 1. Démarrage de l'application

```bash
# Voir les applications disponibles
app_list

# Lancer l'application SST (remplacer X par l'ID de l'app)
app_start X
```

### 2. Comportement

Une fois lancée, l'application SST :

1. **Initialisation** : Configure l'écran P10 et le timer de rafraîchissement
2. **Démarrage** : Affiche "SST START" pendant 2 secondes
3. **Comptage** : Affiche le compteur qui s'incrémente toutes les ~1 seconde
4. **Arrêt** : Efface l'écran et libère les ressources

### 3. Arrêt de l'application

```bash
# Arrêter l'application SST
app_stop X
```

## API de l'Application

### Fonctions Principales

- `sst_dmd_init()` : Initialise l'écran DMD et le timer
- `sst_dmd_display_counter(uint32_t counter)` : Affiche un compteur
- `sst_dmd_display_text(const char* text)` : Affiche du texte
- `sst_dmd_clear_screen()` : Efface l'écran
- `sst_dmd_refresh()` : Rafraîchit l'écran (appelée par le timer)

### Polices Disponibles

- **Arial_Black_16** : Police grande pour les compteurs
- **System5x7** : Police petite pour le texte

## Configuration

### Paramètres Modifiables

Dans `sst_app.h` :

```cpp
#define DISPLAYS_ACROSS 1        // Nombre de panneaux en largeur
#define DISPLAYS_DOWN 1          // Nombre de panneaux en hauteur  
#define DMD_REFRESH_RATE 300     // Fréquence de rafraîchissement (µs)
```

### Pins Configurables

Dans `DMD32.h` (bibliothèque) :

```cpp
#define PIN_DMD_nOE    22        // Output Enable
#define PIN_DMD_A      19        // Adresse A
#define PIN_DMD_B      21        // Adresse B
#define PIN_DMD_CLK    18        // Horloge SPI
#define PIN_DMD_SCLK   2         // Horloge latch
#define PIN_DMD_R_DATA 23        // Données SPI
```

## Dépannage

### Problèmes Courants

1. **Écran ne s'allume pas** :
   - Vérifier les connexions
   - Vérifier l'alimentation 3.3V
   - Vérifier que l'application est bien lancée

2. **Affichage déformé** :
   - Vérifier la fréquence de rafraîchissement
   - Vérifier les connexions CLK et SCLK

3. **Application ne démarre pas** :
   - Vérifier les logs système
   - Vérifier que l'app manager est initialisé

### Logs de Debug

L'application génère des logs détaillés :

```
SST App: Initializing DMD display...
SST App: DMD display initialized
SST App: Starting...
SST App with P10 Display started!
SST App: Running... Loop count: 100
```

## Extensions Possibles

### Fonctionnalités Avancées

- **Scrolling text** : Utiliser `drawMarquee()` et `stepMarquee()`
- **Animations** : Utiliser `drawTestPattern()` pour des effets
- **Graphiques** : Utiliser `drawLine()`, `drawCircle()`, `drawBox()`
- **Multi-panneaux** : Modifier `DISPLAYS_ACROSS` et `DISPLAYS_DOWN`

### Exemple d'Extension

```cpp
// Afficher un texte défilant
dmd_display->drawMarquee("Hello World!", 12, 32, 0);
while(!dmd_display->stepMarquee(-1, 0)) {
    delay(30);
}
```

## Ressources

- **Bibliothèque DMD32** : `/lib/DMD32-main/`
- **Exemples** : `/lib/DMD32-main/examples/`
- **Polices** : `/lib/DMD32-main/fonts/`
- **Documentation** : Voir les commentaires dans le code source
