#include "memory_manager.h"
#include <string.h>
#include <esp_heap_caps.h>

// Variables globales du gestionnaire de mémoire
static MemoryPool_t memory_pools[MEMORY_POOL_COUNT];
static MemoryStats_t memory_stats;
static SemaphoreHandle_t memory_manager_mutex;
static QueueHandle_t memory_event_queue;
static TimerHandle_t memory_monitor_timer;

// Callbacks pour les événements mémoire
static MemoryEventCallback_t memory_event_callbacks[MAX_CALLBACKS];
static uint8_t callback_count = 0;

// Variables de configuration
static uint32_t memory_limit = 0;
static uint32_t low_memory_threshold = 10000; // 10KB par défaut
static uint32_t next_allocation_id = 1;

// Liste des allocations actives (pour détection de fuites)
static AllocationInfo_t* active_allocations = NULL;
static uint32_t active_allocation_count = 0;
static uint32_t max_active_allocations = 100;

// Fonction de monitoring mémoire
static void memory_manager_monitor(void) {
    if (memory_manager_mutex == NULL) {
        return;
    }
    
    if (xSemaphoreTake(memory_manager_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return;
    }
    
    // Mettre à jour les statistiques
    memory_stats.total_heap = esp_get_free_heap_size() + memory_stats.total_allocated;
    memory_stats.free_heap = esp_get_free_heap_size();
    memory_stats.min_free_heap = esp_get_minimum_free_heap_size();
    memory_stats.largest_free_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    
    // Calculer la fragmentation
    if (memory_stats.total_heap > 0) {
        memory_stats.fragmentation_percent = 
            ((memory_stats.total_heap - memory_stats.largest_free_block) * 100) / memory_stats.total_heap;
    }
    
    // Vérifier la mémoire faible
    if (memory_stats.free_heap < low_memory_threshold) {
        memory_manager_on_low_memory();
    }
    
    xSemaphoreGive(memory_manager_mutex);
}

// Initialisation du gestionnaire de mémoire
SysError_t memory_manager_init(void) {
    Serial.println("Initializing Memory Manager...");
    
    // Initialiser les structures
    memset(memory_pools, 0, sizeof(memory_pools));
    memset(&memory_stats, 0, sizeof(memory_stats));
    memset(memory_event_callbacks, 0, sizeof(memory_event_callbacks));
    
    // Créer les objets de synchronisation
    memory_manager_mutex = xSemaphoreCreateMutex();
    memory_event_queue = xQueueCreate(QUEUE_SIZE_DEFAULT, sizeof(uint32_t));
    
    if (memory_manager_mutex == NULL || memory_event_queue == NULL) {
        Serial.println("ERROR: Failed to create memory manager objects!");
        return SYS_ERROR;
    }
    
    // Initialiser les pools mémoire avec des tailles prédéfinies
    uint32_t block_sizes[] = MEMORY_BLOCK_SIZES;
    for (int i = 0; i < MEMORY_POOL_COUNT; i++) {
        memory_pool_init(i, block_sizes[i], MEMORY_POOL_BLOCKS);
    }
    
    // Créer le timer de monitoring
    memory_monitor_timer = xTimerCreate("MemoryMonitor", pdMS_TO_TICKS(10000), pdTRUE, NULL,
                                       [](TimerHandle_t timer) {
                                           memory_manager_monitor();
                                       });
    
    if (memory_monitor_timer == NULL) {
        Serial.println("ERROR: Failed to create memory monitor timer!");
        return SYS_ERROR;
    }
    
    // Démarrer le timer
    xTimerStart(memory_monitor_timer, 0);
    
    // Initialiser les statistiques
    memory_stats.total_heap = esp_get_free_heap_size();
    memory_stats.free_heap = esp_get_free_heap_size();
    memory_stats.min_free_heap = esp_get_minimum_free_heap_size();
    
    Serial.println("Memory Manager initialized successfully!");
    return SYS_OK;
}

void memory_manager_deinit(void) {
    if (memory_monitor_timer != NULL) {
        xTimerStop(memory_monitor_timer, 0);
        xTimerDelete(memory_monitor_timer, 0);
    }
    
    if (memory_event_queue != NULL) {
        vQueueDelete(memory_event_queue);
    }
    
    if (memory_manager_mutex != NULL) {
        vSemaphoreDelete(memory_manager_mutex);
    }
    
    // Libérer les pools mémoire
    for (int i = 0; i < MEMORY_POOL_COUNT; i++) {
        memory_pool_deinit(i);
    }
    
    Serial.println("Memory Manager deinitialized");
}

// Allocation de base
void* memory_alloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    
    // Vérifier la limite mémoire
    if (memory_limit > 0 && memory_stats.total_allocated + size > memory_limit) {
        Serial.printf("WARNING: Memory allocation limit reached (%lu bytes)\n", memory_limit);
        return NULL;
    }
    
    // Essayer d'abord les pools mémoire
    for (int i = 0; i < MEMORY_POOL_COUNT; i++) {
        if (memory_pools[i].initialized && size <= memory_pools[i].block_size) {
            void* ptr = memory_pool_alloc(i);
            if (ptr != NULL) {
                memory_manager_on_allocation(ptr, size);
                return ptr;
            }
        }
    }
    
    // Allocation heap standard
    void* ptr = malloc(size);
    if (ptr != NULL) {
        memory_manager_on_allocation(ptr, size);
    }
    
    return ptr;
}

void* memory_calloc(size_t count, size_t size) {
    if (count == 0 || size == 0) {
        return NULL;
    }
    
    size_t total_size = count * size;
    void* ptr = memory_alloc(total_size);
    if (ptr != NULL) {
        memset(ptr, 0, total_size);
    }
    
    return ptr;
}

void* memory_realloc(void* ptr, size_t size) {
    if (ptr == NULL) {
        return memory_alloc(size);
    }
    
    if (size == 0) {
        memory_free(ptr);
        return NULL;
    }
    
    // Pour simplifier, on fait une nouvelle allocation
    void* new_ptr = memory_alloc(size);
    if (new_ptr != NULL) {
        // Copier les données existantes
        uint32_t old_size = memory_get_block_size(ptr);
        size_t copy_size = (size < old_size) ? size : old_size;
        memcpy(new_ptr, ptr, copy_size);
        memory_free(ptr);
    }
    
    return new_ptr;
}

void memory_free(void* ptr) {
    if (ptr == NULL) {
        return;
    }
    
    // Essayer de libérer depuis les pools
    for (int i = 0; i < MEMORY_POOL_COUNT; i++) {
        if (memory_pools[i].initialized) {
            if (memory_pool_free(i, ptr) == SYS_OK) {
                memory_manager_on_deallocation(ptr);
                return;
            }
        }
    }
    
    // Libération heap standard
    free(ptr);
    memory_manager_on_deallocation(ptr);
}

// Gestion des pools mémoire
SysError_t memory_pool_init(uint8_t pool_id, uint32_t block_size, uint32_t block_count) {
    if (pool_id >= MEMORY_POOL_COUNT) {
        return SYS_INVALID_PARAM;
    }
    
    if (xSemaphoreTake(memory_manager_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return SYS_BUSY;
    }
    
    MemoryPool_t* pool = &memory_pools[pool_id];
    
    // Allouer la mémoire pour le pool
    size_t total_size = block_count * (block_size + sizeof(MemoryBlockHeader_t));
    pool->memory_start = malloc(total_size);
    
    if (pool->memory_start == NULL) {
        xSemaphoreGive(memory_manager_mutex);
        return SYS_NO_MEMORY;
    }
    
    // Initialiser le pool
    pool->pool_id = pool_id;
    pool->block_size = block_size;
    pool->total_blocks = block_count;
    pool->free_blocks = block_count;
    pool->used_blocks = 0;
    pool->initialized = true;
    
    // Initialiser la liste libre
    pool->free_list = pool->memory_start;
    
    Serial.printf("Memory pool %d initialized: %lu blocks of %lu bytes each\n", 
                 pool_id, block_count, block_size);
    
    xSemaphoreGive(memory_manager_mutex);
    return SYS_OK;
}

SysError_t memory_pool_deinit(uint8_t pool_id) {
    if (pool_id >= MEMORY_POOL_COUNT) {
        return SYS_INVALID_PARAM;
    }
    
    if (xSemaphoreTake(memory_manager_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return SYS_BUSY;
    }
    
    MemoryPool_t* pool = &memory_pools[pool_id];
    
    if (pool->initialized) {
        free(pool->memory_start);
        memset(pool, 0, sizeof(MemoryPool_t));
        Serial.printf("Memory pool %d deinitialized\n", pool_id);
    }
    
    xSemaphoreGive(memory_manager_mutex);
    return SYS_OK;
}

void* memory_pool_alloc(uint8_t pool_id) {
    if (pool_id >= MEMORY_POOL_COUNT) {
        return NULL;
    }
    
    if (xSemaphoreTake(memory_manager_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return NULL;
    }
    
    MemoryPool_t* pool = &memory_pools[pool_id];
    
    if (!pool->initialized || pool->free_blocks == 0) {
        xSemaphoreGive(memory_manager_mutex);
        return NULL;
    }
    
    // Allouer un bloc depuis la liste libre
    void* block = pool->free_list;
    pool->free_list = *(void**)block; // Suivant dans la liste libre
    pool->free_blocks--;
    pool->used_blocks++;
    
    xSemaphoreGive(memory_manager_mutex);
    return block;
}

SysError_t memory_pool_free(uint8_t pool_id, void* ptr) {
    if (pool_id >= MEMORY_POOL_COUNT || ptr == NULL) {
        return SYS_INVALID_PARAM;
    }
    
    if (xSemaphoreTake(memory_manager_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return SYS_BUSY;
    }
    
    MemoryPool_t* pool = &memory_pools[pool_id];
    
    if (!pool->initialized) {
        xSemaphoreGive(memory_manager_mutex);
        return SYS_ERROR;
    }
    
    // Vérifier que le pointeur appartient à ce pool
    if (ptr < pool->memory_start || 
        ptr >= (void*)((char*)pool->memory_start + pool->total_blocks * pool->block_size)) {
        xSemaphoreGive(memory_manager_mutex);
        return SYS_INVALID_PARAM;
    }
    
    // Remettre le bloc dans la liste libre
    *(void**)ptr = pool->free_list;
    pool->free_list = ptr;
    pool->free_blocks++;
    pool->used_blocks--;
    
    xSemaphoreGive(memory_manager_mutex);
    return SYS_OK;
}

// Statistiques et monitoring
SysError_t memory_get_stats(MemoryStats_t* stats) {
    if (stats == NULL) {
        return SYS_INVALID_PARAM;
    }
    
    if (xSemaphoreTake(memory_manager_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return SYS_BUSY;
    }
    
    memcpy(stats, &memory_stats, sizeof(MemoryStats_t));
    
    // Mettre à jour les statistiques des pools
    for (int i = 0; i < MEMORY_POOL_COUNT; i++) {
        if (memory_pools[i].initialized) {
            stats->pool_usage[i] = memory_pools[i].used_blocks;
        }
    }
    
    xSemaphoreGive(memory_manager_mutex);
    return SYS_OK;
}

void memory_print_stats(void) {
    MemoryStats_t stats;
    if (memory_get_stats(&stats) == SYS_OK) {
        Serial.println("=== Memory Manager Statistics ===");
        Serial.printf("Total heap: %lu bytes\n", stats.total_heap);
        Serial.printf("Free heap: %lu bytes\n", stats.free_heap);
        Serial.printf("Min free heap: %lu bytes\n", stats.min_free_heap);
        Serial.printf("Largest free block: %lu bytes\n", stats.largest_free_block);
        Serial.printf("Total allocated: %lu bytes\n", stats.total_allocated);
        Serial.printf("Total freed: %lu bytes\n", stats.total_freed);
        Serial.printf("Allocation count: %lu\n", stats.allocation_count);
        Serial.printf("Fragmentation: %lu%%\n", stats.fragmentation_percent);
        Serial.println("================================");
    }
}

void memory_print_pool_stats(void) {
    Serial.println("=== Memory Pool Statistics ===");
    
    for (int i = 0; i < MEMORY_POOL_COUNT; i++) {
        if (memory_pools[i].initialized) {
            MemoryPool_t* pool = &memory_pools[i];
            Serial.printf("Pool %d: %lu/%lu blocks used (%lu bytes each)\n",
                         i, pool->used_blocks, pool->total_blocks, pool->block_size);
        }
    }
    
    Serial.println("===============================");
}

// Callbacks système
void memory_manager_on_allocation(void* ptr, uint32_t size) {
    if (xSemaphoreTake(memory_manager_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        memory_stats.total_allocated += size;
        memory_stats.allocation_count++;
        xSemaphoreGive(memory_manager_mutex);
    }
    
    // Notifier les callbacks
    for (int i = 0; i < callback_count; i++) {
        if (memory_event_callbacks[i] != NULL) {
            memory_event_callbacks[i](MEMORY_TYPE_HEAP, size, true);
        }
    }
}

void memory_manager_on_deallocation(void* ptr) {
    // Notifier les callbacks
    for (int i = 0; i < callback_count; i++) {
        if (memory_event_callbacks[i] != NULL) {
            memory_event_callbacks[i](MEMORY_TYPE_HEAP, 0, false);
        }
    }
}

void memory_manager_on_low_memory(void) {
    Serial.println("WARNING: Low memory condition detected!");
    
    // Notifier les callbacks
    for (int i = 0; i < callback_count; i++) {
        if (memory_event_callbacks[i] != NULL) {
            memory_event_callbacks[i](MEMORY_TYPE_HEAP, 0, false);
        }
    }
}

// Utilitaires
uint32_t memory_get_block_size(void* ptr) {
    // Pour les pools, retourner la taille du bloc
    for (int i = 0; i < MEMORY_POOL_COUNT; i++) {
        if (memory_pools[i].initialized) {
            MemoryPool_t* pool = &memory_pools[i];
            if (ptr >= pool->memory_start && 
                ptr < (void*)((char*)pool->memory_start + pool->total_blocks * pool->block_size)) {
                return pool->block_size;
            }
        }
    }
    
    // Pour le heap standard, on ne peut pas déterminer la taille exacte
    return 0;
}

uint32_t memory_get_total_allocated(void) {
    return memory_stats.total_allocated;
}

uint32_t memory_get_allocation_count(void) {
    return memory_stats.allocation_count;
}

bool memory_is_low_memory(void) {
    return memory_stats.free_heap < low_memory_threshold;
}

// Configuration
SysError_t memory_set_low_memory_threshold(uint32_t threshold) {
    low_memory_threshold = threshold;
    return SYS_OK;
}

uint32_t memory_get_low_memory_threshold(void) {
    return low_memory_threshold;
}

// Gestion des callbacks
SysError_t memory_register_event_callback(MemoryEventCallback_t callback) {
    if (callback == NULL) {
        return SYS_INVALID_PARAM;
    }
    
    if (callback_count >= MAX_CALLBACKS) {
        return SYS_NO_MEMORY;
    }
    
    memory_event_callbacks[callback_count++] = callback;
    return SYS_OK;
}

SysError_t memory_unregister_event_callback(MemoryEventCallback_t callback) {
    for (int i = 0; i < callback_count; i++) {
        if (memory_event_callbacks[i] == callback) {
            // Déplacer les callbacks suivants
            for (int j = i; j < callback_count - 1; j++) {
                memory_event_callbacks[j] = memory_event_callbacks[j + 1];
            }
            callback_count--;
            return SYS_OK;
        }
    }
    
    return SYS_NOT_FOUND;
} 