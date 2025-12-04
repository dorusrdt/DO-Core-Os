#include <Arduino.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>

// Définition des broches
#define PIN_RST  15  // RST/CE
#define PIN_DAT  5   // IO/SDA
#define PIN_CLK  18  // SCL/SCLK

// Initialisation du bus et du RTC
ThreeWire myWire(PIN_DAT, PIN_CLK, PIN_RST);
RtcDS1302<ThreeWire> Rtc(myWire);

void printDateTime(const RtcDateTime& dt) {
    char datestring[20];
    snprintf_P(datestring,
               sizeof(datestring),
               PSTR("%04u-%02u-%02u %02u:%02u:%02u"),
               dt.Year(),
               dt.Month(),
               dt.Day(),
               dt.Hour(),
               dt.Minute(),
               dt.Second());
    Serial.println(datestring);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("=== Test DS1302 ===");

    Serial.println("Initialisation DS1302...");
    Rtc.Begin();

    Serial.print("RTC IsRunning: ");
    Serial.println(Rtc.GetIsRunning() ? "YES" : "NO");

    Serial.print("RTC WriteProtected: ");
    Serial.println(Rtc.GetIsWriteProtected() ? "YES" : "NO");

    Serial.print("RTC DateTimeValid: ");
    Serial.println(Rtc.IsDateTimeValid() ? "YES" : "NO");

    // Vérifier si le RTC est en cours d'exécution
    if (!Rtc.IsDateTimeValid()) {
        Serial.println("Le temps du RTC n'est pas valide!");

        // Régler la date et l'heure de compilation
        RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
        Serial.println("Programmation avec l'heure de compilation...");
        Rtc.SetDateTime(compiled);

        delay(100);

        if (Rtc.IsDateTimeValid()) {
            Serial.println("Programmation réussie!");
        } else {
            Serial.println("Échec de programmation!");
        }
    }

    if (Rtc.GetIsWriteProtected()) {
        Serial.println("RTC en protection écriture, désactivation...");
        Rtc.SetIsWriteProtected(false);
    }

    if (!Rtc.GetIsRunning()) {
        Serial.println("Démarrage du RTC...");
        Rtc.SetIsRunning(true);
    }

    // Lire et afficher l'heure
    if (Rtc.IsDateTimeValid()) {
        RtcDateTime now = Rtc.GetDateTime();
        Serial.print("Heure actuelle: ");
        printDateTime(now);
    } else {
        Serial.println("Impossible de lire l'heure!");
    }

    Serial.println("=== Test terminé ===");
}

void loop() {
    // Lire l'heure toutes les secondes
    if (Rtc.IsDateTimeValid()) {
        RtcDateTime now = Rtc.GetDateTime();
        printDateTime(now);
    } else {
        Serial.println("Erreur de lecture RTC");
    }

    delay(1000);
}