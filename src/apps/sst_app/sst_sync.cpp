#include "sst_sync.h"
#include "sst_app.h"
#include "../../kernel/core/minimal_config.h"
#include "../../kernel/core/log_system_optimized.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>

extern SSTData_t sst_data;
// Forward declaration from sst_app.cpp
const DeviceInfo_t* sst_device_get_info(void);

time_t last_sync_time = 0;
bool   sync_enabled   = true;
static char last_etag[32] = {0};

SysError_t sst_sync_init(void) {
    SERIAL_PRINTLN_MINIMAL("SST Sync: Initializing...");
    last_sync_time = 0;
    memset(last_etag, 0, sizeof(last_etag));
    sync_enabled = true;
    SERIAL_PRINTLN_MINIMAL("SST Sync: Initialized");
    return SYS_OK;
}

void sst_sync_deinit(void) {
    sync_enabled = false;
}

SysError_t sst_sync_with_backend(void) {
    if (!sync_enabled || !sst_device_is_registered()) {
        SERIAL_PRINTLN_MINIMAL("SST Sync: Sync disabled or device not registered");
        return SYS_ERROR;
    }

    const DeviceInfo_t* info = sst_device_get_info();
    String url = String("http://") + BACKEND_HOST + ":" + String(BACKEND_PORT) + "/devices/" + info->device_id + "/indicators";

    SERIAL_PRINTF_MINIMAL("SST Sync: Fetching indicators from %s\n", url.c_str());

    HTTPClient http;
    http.begin(url);
    http.addHeader("Authorization", String("Bearer ") + info->api_token);
    if (last_etag[0] != '\0') {
        http.addHeader("If-None-Match", last_etag);
        SERIAL_PRINTF_MINIMAL("SST Sync: Using ETag: %s\n", last_etag);
    }

    int code = http.GET();
    SERIAL_PRINTF_MINIMAL("SST Sync: HTTP response code: %d\n", code);

    if (code == 304) {
        SERIAL_PRINTLN_MINIMAL("SST Sync: No changes (304 Not Modified)");
        http.end();
        return SYS_OK;
    }
    if (code != 200) {
        SERIAL_PRINTF_MINIMAL("SST Sync: HTTP error %d\n", code);
        http.end();
        return SYS_ERROR;
    }

    String etag = http.header("ETag");
    if (etag.length() > 0) {
        strncpy(last_etag, etag.c_str(), sizeof(last_etag)-1);
    }

    String body = http.getString();
    http.end();

    SERIAL_PRINTF_MINIMAL("SST Sync: Received JSON: %s\n", body.c_str());

    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, body) != DeserializationError::Ok) {
        SERIAL_PRINTLN_MINIMAL("SST Sync: JSON parse failed");
        return SYS_ERROR;
    }

    BackendSnapshot_t snap;
    snap.jours_sans_accident   = doc["jours_sans_accident"] | sst_data.jours_sans_accident;
    snap.total_accidents       = doc["total_accidents"] | sst_data.total_accidents;
    snap.accidents_avec_arret  = doc["accidents_avec_arret"] | sst_data.accidents_avec_arret;
    snap.accidents_sans_arret  = doc["accidents_sans_arret"] | sst_data.accidents_sans_arret;
    snap.date_dernier_accident = doc["date_dernier_accident"] | sst_data.date_dernier_accident;
    snap.taux_frequence        = doc["taux_frequence"] | sst_data.taux_frequence;
    snap.record_jours          = doc["record_jours"] | sst_data.record_jours_sans_accident;
    snap.updated_at            = doc["updated_at"] | 0;
    snap.etag[0]               = '\0';

    SERIAL_PRINTF_MINIMAL("SST Sync: Backend snapshot - JSA: %u, Total: %u, AvecArret: %u, Taux: %.2f\n",
                         snap.jours_sans_accident, snap.total_accidents, snap.accidents_avec_arret, snap.taux_frequence);

    SysError_t res = sst_apply_backend_snapshot(&snap);
    if (res == SYS_OK) {
        last_sync_time = time(nullptr);
        SERIAL_PRINTLN_MINIMAL("SST Sync: Indicators updated successfully");
    }
    return res;
}

SysError_t sst_post_event(SSTEventType_t event_type, const char* description) {
    if (!sync_enabled || !sst_device_is_registered()) {
        return SYS_ERROR;
    }
    const DeviceInfo_t* info = sst_device_get_info();
    String url = String("http://") + BACKEND_HOST + ":" + String(BACKEND_PORT) + "/devices/" + info->device_id + "/events";

    DynamicJsonDocument doc(512);
    switch(event_type) {
        case SST_EVENT_INCREMENT: doc["type"] = "INCREMENT_JOUR"; break;
        case SST_EVENT_DECREMENT: doc["type"] = "DECREMENT_JOUR"; break;
        case SST_EVENT_RESET:     doc["type"] = "RESET_JOURS"; break;
        case SST_EVENT_ACCIDENT:  doc["type"] = "ACCIDENT_AVEC_ARRET"; break;
    }
    if (description) doc["description"] = description;
    doc["timestamp"] = (int)time(nullptr);
    doc["source"] = "device";

    String payload; serializeJson(doc, payload);
    HTTPClient http; http.begin(url);
    http.addHeader("Authorization", String("Bearer ") + info->api_token);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(payload);
    http.end();
    return (code == 200) ? SYS_OK : SYS_ERROR;
}

SysError_t sst_apply_backend_snapshot(const BackendSnapshot_t* s) {
    if (!s) return SYS_INVALID_PARAM;
    bool changed = false;

    SERIAL_PRINTF_MINIMAL("SST Sync: Comparing local vs backend - JSA: %u->%u, Total: %u->%u, AvecArret: %u->%u\n",
                         sst_data.jours_sans_accident, s->jours_sans_accident,
                         sst_data.total_accidents, s->total_accidents,
                         sst_data.accidents_avec_arret, s->accidents_avec_arret);

    if (sst_data.jours_sans_accident != s->jours_sans_accident) {
        SERIAL_PRINTF_MINIMAL("SST Sync: Updating jours_sans_accident: %u -> %u\n", sst_data.jours_sans_accident, s->jours_sans_accident);
        sst_data.jours_sans_accident = s->jours_sans_accident;
        changed = true;
    }
    if (sst_data.total_accidents != s->total_accidents) {
        SERIAL_PRINTF_MINIMAL("SST Sync: Updating total_accidents: %u -> %u\n", sst_data.total_accidents, s->total_accidents);
        sst_data.total_accidents = s->total_accidents;
        changed = true;
    }
    if (sst_data.accidents_avec_arret != s->accidents_avec_arret) {
        SERIAL_PRINTF_MINIMAL("SST Sync: Updating accidents_avec_arret: %u -> %u\n", sst_data.accidents_avec_arret, s->accidents_avec_arret);
        sst_data.accidents_avec_arret = s->accidents_avec_arret;
        changed = true;
    }
    if (sst_data.accidents_sans_arret != s->accidents_sans_arret) {
        SERIAL_PRINTF_MINIMAL("SST Sync: Updating accidents_sans_arret: %u -> %u\n", sst_data.accidents_sans_arret, s->accidents_sans_arret);
        sst_data.accidents_sans_arret = s->accidents_sans_arret;
        changed = true;
    }
    if (sst_data.record_jours_sans_accident != s->record_jours) {
        SERIAL_PRINTF_MINIMAL("SST Sync: Updating record_jours: %u -> %u\n", sst_data.record_jours_sans_accident, s->record_jours);
        sst_data.record_jours_sans_accident = s->record_jours;
        changed = true;
    }
    if (sst_data.date_dernier_accident != s->date_dernier_accident) {
        SERIAL_PRINTF_MINIMAL("SST Sync: Updating date_dernier_accident: %u -> %u\n", sst_data.date_dernier_accident, s->date_dernier_accident);
        sst_data.date_dernier_accident = s->date_dernier_accident;
        changed = true;
    }
    if (fabsf(sst_data.taux_frequence - s->taux_frequence) > 0.0001f) {
        SERIAL_PRINTF_MINIMAL("SST Sync: Updating taux_frequence: %.2f -> %.2f\n", sst_data.taux_frequence, s->taux_frequence);
        sst_data.taux_frequence = s->taux_frequence;
        changed = true;
    }
    if (changed) {
        SERIAL_PRINTLN_MINIMAL("SST Sync: Local indicators updated - saving to NVS");
        return sst_data_save();
    } else {
        SERIAL_PRINTLN_MINIMAL("SST Sync: No changes detected");
    }
    return SYS_OK;
}


