#ifndef LOADING_DOTS_ANIMATION_H
#define LOADING_DOTS_ANIMATION_H

#include "../lib/DMD32-main/DMD32.h"

class LoadingDotsAnimation {
private:
    DMD* dmd;
    const int DOT_SPACING = 5;
    const int NUM_DOTS = 5;
    const int PULSE_STEPS = 8;
    int step;
    int pulseStep;

public:
    static const int DELAY = 80;  // Délai plus rapide pour une animation fluide

    LoadingDotsAnimation(DMD* display) : dmd(display), step(0), pulseStep(0) {}

    void reset() {
        step = 0;
        pulseStep = 0;
    }

    void update() {
        // Effacer seulement la section basse (lignes 8-15)
        for(int y = 8; y < 16; y++) {
            for(int x = 0; x < 32; x++) {
                dmd->writePixel(x, y, GRAPHICS_NORMAL, 0);
            }
        }

        // Position de départ des points (centrée dans la section basse)
        int startX = (32 - (NUM_DOTS * DOT_SPACING)) / 2;
        int y = 12;  // Centre de la section basse

        // Dessine tous les points avec effets modernes
        for(int i = 0; i < NUM_DOTS; i++) {
            int x = startX + (i * DOT_SPACING);

            if(i == step % NUM_DOTS) {
                // Point actif avec effet de pulsation
                drawPulsingDot(x, y);
            } else if(i == (step - 1) % NUM_DOTS) {
                // Point précédent avec effet de fondu
                drawFadingDot(x, y);
            } else {
                // Points inactifs
                dmd->writePixel(x, y, GRAPHICS_NORMAL, 1);
            }
        }

        // Mise à jour des étapes
        pulseStep = (pulseStep + 1) % PULSE_STEPS;
        if(pulseStep == 0) {
            step++;
        }
    }

private:
    void drawPulsingDot(int x, int y) {
        // Calcul de la taille basée sur l'étape de pulsation
        float pulseRatio = sin((pulseStep * PI) / PULSE_STEPS);
        int size = 1 + (int)(pulseRatio * 1.5); // Taille de 1 à 2.5

        // Dessine le point avec taille variable
        for(int dx = -size; dx <= size; dx++) {
            for(int dy = -size; dy <= size; dy++) {
                if(dx*dx + dy*dy <= size*size) {
                    int px = x + dx;
                    int py = y + dy;
                    // Vérifier les limites de la section basse
                    if(px >= 0 && px < 32 && py >= 8 && py < 16) {
                        dmd->writePixel(px, py, GRAPHICS_NORMAL, 1);
                    }
                }
            }
        }
    }

    void drawFadingDot(int x, int y) {
        // Point en train de s'effacer
        float fadeRatio = 1.0 - ((float)pulseStep / PULSE_STEPS);
        if(fadeRatio > 0.3) { // Garde visible jusqu'à 30%
            dmd->writePixel(x, y, GRAPHICS_NORMAL, 1);
        }
    }
};

#endif