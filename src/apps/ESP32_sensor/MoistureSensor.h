#ifndef MOISTURE_SENSOR_H
#define MOISTURE_SENSOR_H

#include <Arduino.h>
#include <math.h>

/**
 * @class MoistureSensor
 * @brief Classe pour lire et filtrer les données d'un capteur d'humidité du sol
 *
 * Cette classe implémente :
 * - Filtre médian pour supprimer les pics de bruit
 * - Moyenne glissante pour lisser les variations
 * - Conversion ADC → Voltage via polynôme ESP32
 * - Calibration avec limites V_MIN/V_MAX
 * - Conversion Voltage → Humidité %
 */
class MoistureSensor {
public:
    /**
     * @brief Constructeur
     * @param pin Numéro de la broche GPIO pour le capteur
     * @param vMin Tension minimale (sol très humide) - défaut: 1.50V
     * @param vMax Tension maximale (sol très sec) - défaut: 3.15V
     */
    MoistureSensor(uint8_t pin, double vMin = 1.50, double vMax = 3.15);

    /**
     * @brief Initialise le capteur
     * @note Configure la résolution ADC et l'atténuation
     */
    void begin();

    /**
     * @brief Lit la valeur d'humidité filtrée
     * @return Humidité en pourcentage (0-100%)
     * @note Cette méthode est bloquante (~2-3ms par lecture)
     */
    double readHumidity();

    /**
     * @brief Lit la tension filtrée
     * @return Tension en volts
     * @note Cette méthode est bloquante (~2-3ms par lecture)
     */
    double readVoltage();

    /**
     * @brief Lit la valeur ADC brute filtrée
     * @return Valeur ADC (0-4095)
     * @note Cette méthode est bloquante (~2-3ms par lecture)
     */
    int readRaw();

    /**
     * @brief Configure les limites de calibration
     * @param vMin Tension minimale (sol très humide)
     * @param vMax Tension maximale (sol très sec)
     */
    void setCalibration(double vMin, double vMax);

    /**
     * @brief Configure les paramètres de filtrage
     * @param medianSamples Nombre d'échantillons pour le filtre médian (défaut: 7)
     * @param averageSamples Nombre d'échantillons pour la moyenne (défaut: 200)
     */
    void setFilterParams(int medianSamples = 7, int averageSamples = 200);

    /**
     * @brief Obtient la dernière valeur d'humidité lue (sans nouvelle lecture)
     * @return Dernière humidité en pourcentage
     */
    double getLastHumidity() const { return lastHumidity; }

    /**
     * @brief Obtient la dernière tension lue
     * @return Dernière tension en volts
     */
    double getLastVoltage() const { return lastVoltage; }

    /**
     * @brief Obtient la dernière valeur ADC brute lue
     * @return Dernière valeur ADC (0-4095)
     */
    int getLastRaw() const { return lastRaw; }

private:
    uint8_t pin;
    double vMin;
    double vMax;
    int medianSamples;
    int averageSamples;

    // Dernières valeurs lues (cache)
    double lastHumidity;
    double lastVoltage;
    int lastRaw;

    /**
     * @brief Filtre médian pour supprimer les pics
     * @param vals Tableau de valeurs
     * @param n Nombre d'échantillons
     * @return Valeur médiane
     */
    int medianFilter(int *vals, int n);

    /**
     * @brief Conversion ADC → Voltage via polynôme ESP32
     * @param raw Valeur ADC brute (0-4095)
     * @return Tension en volts
     */
    double polyVoltage(int raw);

    /**
     * @brief Conversion Voltage → Humidité %
     * @param voltage Tension en volts
     * @return Humidité en pourcentage (0-100%)
     */
    double voltageToHumidity(double voltage);

    /**
     * @brief Lit la tension filtrée (méthode interne)
     * @return Tension en volts
     */
    double readFilteredVoltage();
};

#endif // MOISTURE_SENSOR_H

