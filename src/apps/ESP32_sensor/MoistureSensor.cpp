#include "MoistureSensor.h"

// ==========================
//     CONSTRUCTEUR
// ==========================
MoistureSensor::MoistureSensor(uint8_t pin, double vMin, double vMax)
    : pin(pin), vMin(vMin), vMax(vMax),
      medianSamples(7), averageSamples(200),
      lastHumidity(50.0), lastVoltage(0.0), lastRaw(0) {
}

// ==========================
//       INITIALISATION
// ==========================
void MoistureSensor::begin() {
    // La configuration ADC est faite globalement dans ESP32_sensor_app_start()
    // Ici on peut ajouter des initialisations spécifiques si nécessaire
    lastHumidity = 50.0;
    lastVoltage = 0.0;
    lastRaw = 0;
}

// ==========================
//     FILTRE MEDIAN
// ==========================
int MoistureSensor::medianFilter(int *vals, int n) {
    int temp[n];
    memcpy(temp, vals, n * sizeof(int));

    // Tri par sélection (simple et efficace pour petits tableaux)
    for (int i = 0; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
            if (temp[j] < temp[i]) {
                int a = temp[i];
                temp[i] = temp[j];
                temp[j] = a;
            }
        }
    }

    return temp[n / 2];
}

// ==========================
//    POLYNÔME ADC → VOLTAGE
// ==========================
double MoistureSensor::polyVoltage(int raw) {
    // Protection contre les valeurs hors limites
    if (raw < 1 || raw > 4095) return 0;

    // Polynôme de calibration ESP32 (4ème degré)
    // Ce polynôme corrige la non-linéarité de l'ADC de l'ESP32
    return -0.000000000000016 * pow(raw, 4)
         + 0.000000000118171 * pow(raw, 3)
         - 0.000000301211691 * pow(raw, 2)
         + 0.001109019271794 * raw
         + 0.034143524634089;
}

// ==========================
//    VOLTAGE → HUMIDITÉ %
// ==========================
double MoistureSensor::voltageToHumidity(double voltage) {
    // Clamp de la tension entre V_MIN et V_MAX
    if (voltage < vMin) voltage = vMin;
    if (voltage > vMax) voltage = vMax;

    // Mapping linéaire : V_MIN → 100% (humide), V_MAX → 0% (sec)
    double percent = (voltage - vMin) * 100.0 / (vMax - vMin);

    // Inversion : sec = 0%, humide = 100%
    percent = 100.0 - percent;

    // Limites finales
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    return percent;
}

// ==========================
//       ADC FILTRÉ
// ==========================
double MoistureSensor::readFilteredVoltage() {
    int medianBuf[medianSamples];

    // --- 1) Filtre médian : supprimer les pics de bruit
    for (int i = 0; i < medianSamples; i++) {
        medianBuf[i] = analogRead(pin);
        delayMicroseconds(200); // Petit délai pour stabiliser l'ADC
    }
    int rawMedian = medianFilter(medianBuf, medianSamples);

    // --- 2) Moyenne glissante autour de la valeur médiane
    double sum = 0;
    for (int i = 0; i < averageSamples; i++) {
        sum += analogRead(pin);
        delayMicroseconds(10); // Délai minimal pour la stabilité
    }
    int rawAvg = sum / averageSamples;

    // --- 3) Mix pour stabilité : 70% moyenne, 30% médian
    // Cette combinaison donne une meilleure stabilité que l'un ou l'autre seul
    int rawFinal = (0.7 * rawAvg) + (0.3 * rawMedian);

    // --- 4) Conversion via le polynôme
    double voltage = polyVoltage(rawFinal);

    // --- 5) Clamp entre les limites réelles du capteur
    if (voltage < vMin) voltage = vMin;
    if (voltage > vMax) voltage = vMax;

    return voltage;
}

// ==========================
//    LECTURE HUMIDITÉ
// ==========================
double MoistureSensor::readHumidity() {
    // Calculer raw d'abord pour le cache
    int medianBuf[medianSamples];
    for (int i = 0; i < medianSamples; i++) {
        medianBuf[i] = analogRead(pin);
        delayMicroseconds(200);
    }
    int rawMedian = medianFilter(medianBuf, medianSamples);

    double sum = 0;
    for (int i = 0; i < averageSamples; i++) {
        sum += analogRead(pin);
        delayMicroseconds(10);
    }
    int rawAvg = sum / averageSamples;
    int rawFinal = (0.7 * rawAvg) + (0.3 * rawMedian);

    // Conversion
    double voltage = polyVoltage(rawFinal);
    if (voltage < vMin) voltage = vMin;
    if (voltage > vMax) voltage = vMax;

    double humidity = voltageToHumidity(voltage);

    // Mise à jour du cache
    lastRaw = rawFinal;
    lastVoltage = voltage;
    lastHumidity = humidity;

    return humidity;
}

// ==========================
//    LECTURE TENSION
// ==========================
double MoistureSensor::readVoltage() {
    double voltage = readFilteredVoltage();
    lastVoltage = voltage;
    return voltage;
}

// ==========================
//    LECTURE RAW
// ==========================
int MoistureSensor::readRaw() {
    int medianBuf[medianSamples];

    // Filtre médian
    for (int i = 0; i < medianSamples; i++) {
        medianBuf[i] = analogRead(pin);
        delayMicroseconds(200);
    }
    int rawMedian = medianFilter(medianBuf, medianSamples);

    // Moyenne
    double sum = 0;
    for (int i = 0; i < averageSamples; i++) {
        sum += analogRead(pin);
        delayMicroseconds(10);
    }
    int rawAvg = sum / averageSamples;

    // Mix
    int rawFinal = (0.7 * rawAvg) + (0.3 * rawMedian);

    lastRaw = rawFinal;
    return rawFinal;
}

// ==========================
//    CONFIGURATION
// ==========================
void MoistureSensor::setCalibration(double vMin, double vMax) {
    this->vMin = vMin;
    this->vMax = vMax;
}

void MoistureSensor::setFilterParams(int medianSamples, int averageSamples) {
    // Limites raisonnables pour éviter les problèmes de mémoire
    if (medianSamples > 0 && medianSamples <= 20) {
        this->medianSamples = medianSamples;
    }
    if (averageSamples > 0 && averageSamples <= 500) {
        this->averageSamples = averageSamples;
    }
}

