#ifndef SST_SYNC_H
#define SST_SYNC_H

#include <time.h>
#include "../../kernel/core/kernel.h"
#include "sst_data.h"

#ifndef BACKEND_HOST
#define BACKEND_HOST "192.168.1.3"
#endif

#ifndef BACKEND_PORT
#define BACKEND_PORT 5000
#endif

typedef struct {
    uint32_t jours_sans_accident;
    uint32_t total_accidents;
    uint32_t accidents_avec_arret;
    uint32_t accidents_sans_arret;
    time_t   date_dernier_accident;
    float    taux_frequence;
    uint32_t record_jours;
    time_t   updated_at;
    char     etag[32];
} BackendSnapshot_t;

typedef enum {
    SST_EVENT_INCREMENT,
    SST_EVENT_DECREMENT,
    SST_EVENT_RESET,
    SST_EVENT_ACCIDENT
} SSTEventType_t;

SysError_t sst_sync_init(void);
void       sst_sync_deinit(void);
SysError_t sst_sync_with_backend(void);
SysError_t sst_post_event(SSTEventType_t event_type, const char* description);
SysError_t sst_apply_backend_snapshot(const BackendSnapshot_t* snapshot);

extern time_t last_sync_time;
extern bool   sync_enabled;

#endif // SST_SYNC_H


