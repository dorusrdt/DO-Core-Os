#ifndef DO_CORE_MEMORY_MANAGER_H
#define DO_CORE_MEMORY_MANAGER_H

#include "kernel.h"

// Constantes du gestionnaire de mémoire
#define MEMORY_POOL_COUNT 4  // Réduit de 8 à 4
#define MEMORY_BLOCK_SIZES {32, 64, 128, 256}  // Réduit de 8 tailles à 4
#define MEMORY_POOL_BLOCKS 16  // Réduit de 32 à 16
#define MEMORY_ALIGNMENT 4
#define MEMORY_GUARD_SIZE 4
#define MEMORY_MAGIC_NUMBER 0xDEADBEEF

// Types de mémoire
typedef enum {
    MEMORY_TYPE_HEAP,      // Mémoire heap standard
    MEMORY_TYPE_POOL,      // Mémoire pool pré-allouée
    MEMORY_TYPE_DMA,       // Mémoire DMA
    MEMORY_TYPE_IRAM,      // Mémoire IRAM
    MEMORY_TYPE_PSRAM      // Mémoire PSRAM
} MemoryType_t;

// États d'un bloc mémoire
typedef enum {
    MEMORY_BLOCK_FREE,
    MEMORY_BLOCK_USED,
    MEMORY_BLOCK_RESERVED,
    MEMORY_BLOCK_CORRUPTED
} MemoryBlockState_t;

// Structure d'un bloc mémoire
typedef struct {
    uint32_t magic_start;
    uint32_t size;
    uint32_t allocated_time;
    uint8_t pool_id;
    MemoryType_t type;
    MemoryBlockState_t state;
    uint32_t magic_end;
} MemoryBlockHeader_t;

// Structure d'un pool mémoire
typedef struct {
    uint8_t pool_id;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t free_blocks;
    uint32_t used_blocks;
    void* memory_start;
    void* free_list;
    bool initialized;
} MemoryPool_t;

// Statistiques mémoire
typedef struct {
    uint32_t total_heap;
    uint32_t free_heap;
    uint32_t min_free_heap;
    uint32_t largest_free_block;
    uint32_t total_allocated;
    uint32_t total_freed;
    uint32_t allocation_count;
    uint32_t fragmentation_percent;
    uint32_t pool_usage[MEMORY_POOL_COUNT];
} MemoryStats_t;

// Informations détaillées sur l'allocation
typedef struct {
    void* address;
    uint32_t size;
    uint32_t allocated_time;
    uint32_t allocation_id;
    MemoryType_t type;
    uint8_t pool_id;
    const char* file;
    uint32_t line;
} AllocationInfo_t;

// Callback pour les événements mémoire
typedef void (*MemoryEventCallback_t)(MemoryType_t type, uint32_t size, bool is_allocation);

// Initialisation et configuration
SysError_t memory_manager_init(void);
void memory_manager_deinit(void);
SysError_t memory_manager_set_config(const char* key, const char* value);

// Allocation et désallocation de base
void* memory_alloc(size_t size);
void* memory_calloc(size_t count, size_t size);
void* memory_realloc(void* ptr, size_t size);
void memory_free(void* ptr);

// Allocation typée
void* memory_alloc_typed(size_t size, MemoryType_t type);
void* memory_calloc_typed(size_t count, size_t size, MemoryType_t type);
void memory_free_typed(void* ptr, MemoryType_t type);

// Gestion des pools mémoire
SysError_t memory_pool_init(uint8_t pool_id, uint32_t block_size, uint32_t block_count);
SysError_t memory_pool_deinit(uint8_t pool_id);
void* memory_pool_alloc(uint8_t pool_id);
SysError_t memory_pool_free(uint8_t pool_id, void* ptr);
SysError_t memory_pool_get_stats(uint8_t pool_id, MemoryPool_t* stats);

// Allocation avec debug
#ifdef DEBUG_MEMORY
void* memory_alloc_debug(size_t size, const char* file, uint32_t line);
void* memory_calloc_debug(size_t count, size_t size, const char* file, uint32_t line);
void memory_free_debug(void* ptr, const char* file, uint32_t line);
#define MEMORY_ALLOC(size) memory_alloc_debug(size, __FILE__, __LINE__)
#define MEMORY_CALLOC(count, size) memory_calloc_debug(count, size, __FILE__, __LINE__)
#define MEMORY_FREE(ptr) memory_free_debug(ptr, __FILE__, __LINE__)
#else
#define MEMORY_ALLOC(size) memory_alloc(size)
#define MEMORY_CALLOC(count, size) memory_calloc(count, size)
#define MEMORY_FREE(ptr) memory_free(ptr)
#endif

// Gestion de la fragmentation
SysError_t memory_defrag(void);
uint32_t memory_get_fragmentation_percent(void);
SysError_t memory_compact_pool(uint8_t pool_id);

// Statistiques et monitoring
SysError_t memory_get_stats(MemoryStats_t* stats);
void memory_print_stats(void);
void memory_print_pool_stats(void);
void memory_print_allocation_map(void);

// Validation et intégrité
SysError_t memory_validate_block(void* ptr);
SysError_t memory_validate_all_blocks(void);
SysError_t memory_check_integrity(void);
bool memory_is_valid_pointer(void* ptr);

// Gestion des fuites mémoire
#ifdef DEBUG_MEMORY
SysError_t memory_get_allocation_info(void* ptr, AllocationInfo_t* info);
SysError_t memory_get_all_allocations(AllocationInfo_t* allocations, uint32_t max_count, uint32_t* actual_count);
void memory_print_all_allocations(void);
SysError_t memory_detect_leaks(void);
#endif

// Utilitaires
uint32_t memory_get_block_size(void* ptr);
uint32_t memory_get_total_allocated(void);
uint32_t memory_get_total_freed(void);
uint32_t memory_get_allocation_count(void);
MemoryType_t memory_get_type(void* ptr);

// Gestion des événements
SysError_t memory_register_event_callback(MemoryEventCallback_t callback);
SysError_t memory_unregister_event_callback(MemoryEventCallback_t callback);

// Gestion des limites
SysError_t memory_set_limit(uint32_t limit);
uint32_t memory_get_limit(void);
bool memory_is_limit_reached(void);

// Gestion des alertes
SysError_t memory_set_low_memory_threshold(uint32_t threshold);
uint32_t memory_get_low_memory_threshold(void);
bool memory_is_low_memory(void);

// Callbacks système
void memory_manager_on_allocation(void* ptr, uint32_t size);
void memory_manager_on_deallocation(void* ptr);
void memory_manager_on_low_memory(void);

// Debug et diagnostic
void memory_manager_dump_state(void);
SysError_t memory_debug_info(void* ptr, char* buffer, size_t buffer_size);
void memory_print_heap_info(void);

// Macros utilitaires
#define MEMORY_ALIGN(size) (((size) + (MEMORY_ALIGNMENT - 1)) & ~(MEMORY_ALIGNMENT - 1))
#define MEMORY_IS_ALIGNED(ptr) (((uintptr_t)(ptr) & (MEMORY_ALIGNMENT - 1)) == 0)

// Vérifications de sécurité
#define MEMORY_CHECK_PTR(ptr) \
    do { \
        if ((ptr) != NULL && !memory_is_valid_pointer(ptr)) { \
            kernel_log(LOG_LEVEL_ERROR, "Invalid memory pointer: %p", (ptr)); \
            return SYS_INVALID_PARAM; \
        } \
    } while(0)

#endif // DO_CORE_MEMORY_MANAGER_H 