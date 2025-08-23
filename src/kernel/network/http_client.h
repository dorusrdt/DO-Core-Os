#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include "../core/kernel.h"
#include <WiFiClient.h>
#include <HTTPClient.h>

// Configuration du client HTTP
#define HTTP_CLIENT_MAX_URL_LENGTH 128
#define HTTP_CLIENT_MAX_HEADERS_LENGTH 256
#define HTTP_CLIENT_MAX_BODY_LENGTH 1024
#define HTTP_CLIENT_MAX_RESPONSE_LENGTH 2048
#define HTTP_CLIENT_DEFAULT_TIMEOUT 10000  // 10 secondes
#define HTTP_CLIENT_DEFAULT_RETRY_COUNT 3
#define HTTP_CLIENT_MAX_RETRY_DELAY 5000   // 5 secondes

// Méthodes HTTP supportées
typedef enum {
    HTTP_METHOD_GET = 0,
    HTTP_METHOD_POST,
    HTTP_METHOD_PUT,
    HTTP_METHOD_DELETE,
    HTTP_METHOD_PATCH,
    HTTP_METHOD_HEAD,
    HTTP_METHOD_OPTIONS,
    HTTP_METHOD_TRACE,
    HTTP_METHOD_CONNECT
} HttpMethod_t;

// Codes d'erreur HTTP client
typedef enum {
    HTTP_CLIENT_OK = 0,
    HTTP_CLIENT_ERROR_INIT,
    HTTP_CLIENT_ERROR_WIFI,
    HTTP_CLIENT_ERROR_URL,
    HTTP_CLIENT_ERROR_CONNECTION,
    HTTP_CLIENT_ERROR_TIMEOUT,
    HTTP_CLIENT_ERROR_RESPONSE,
    HTTP_CLIENT_ERROR_MEMORY,
    HTTP_CLIENT_ERROR_PARAM
} HttpClientError_t;

// Configuration du client HTTP
typedef struct {
    char server_url[HTTP_CLIENT_MAX_URL_LENGTH];
    uint16_t port;
    uint32_t timeout_ms;
    uint8_t retry_count;
    uint32_t retry_delay_ms;
    bool use_ssl;
    char api_key[64];
    char user_agent[64];
    bool enabled;
    uint32_t version;
    uint32_t checksum;
} HttpClientConfig_t;

// Structure de requête HTTP
typedef struct {
    HttpMethod_t method;
    char endpoint[64];
    char headers[HTTP_CLIENT_MAX_HEADERS_LENGTH];
    char body[HTTP_CLIENT_MAX_BODY_LENGTH];
    uint32_t timeout_ms;
    bool follow_redirects;
    uint8_t priority;
} HttpRequest_t;

// Structure de réponse HTTP
typedef struct {
    uint16_t status_code;
    char body[HTTP_CLIENT_MAX_RESPONSE_LENGTH];
    char headers[HTTP_CLIENT_MAX_HEADERS_LENGTH];
    uint32_t response_time_ms;
    uint32_t content_length;
    bool success;
    HttpClientError_t error_code;
    char error_message[128];
} HttpResponse_t;

// Statistiques du client HTTP
typedef struct {
    uint32_t total_requests;
    uint32_t successful_requests;
    uint32_t failed_requests;
    uint32_t timeout_errors;
    uint32_t connection_errors;
    uint32_t average_response_time;
    uint32_t last_successful_request;
    uint32_t last_failed_request;
    uint32_t total_bytes_sent;
    uint32_t total_bytes_received;
} HttpStats_t;

// Classe principale HTTP Client
class HttpClientManager {
private:
    HttpClientConfig_t config;
    HttpStats_t stats;
    bool initialized;
    SemaphoreHandle_t mutex;
    
    // Fonctions privées
    uint32_t calculate_checksum(const void* data, size_t size);
    bool validate_checksum(const void* data, size_t size, uint32_t expected_checksum);
    const char* method_to_string(HttpMethod_t method);
    const char* error_to_string(HttpClientError_t error);
    HttpClientError_t send_request_internal(HttpRequest_t* request, HttpResponse_t* response);
    bool add_default_headers(HTTPClient& client, const char* custom_headers);
    void update_stats(bool success, uint32_t response_time, uint32_t bytes_sent, uint32_t bytes_received);

public:
    // Constructeur/Destructeur
    HttpClientManager();
    ~HttpClientManager();
    
    // Initialisation et gestion
    HttpClientError_t init(void);
    HttpClientError_t deinit(void);
    bool is_initialized(void) const;
    
    // Configuration
    HttpClientError_t set_config(const HttpClientConfig_t* new_config);
    HttpClientError_t get_config(HttpClientConfig_t* config_out);
    HttpClientError_t reset_config(void);
    HttpClientError_t set_server_url(const char* url);
    HttpClientError_t set_timeout(uint32_t timeout_ms);
    HttpClientError_t set_retry_count(uint8_t count);
    HttpClientError_t set_api_key(const char* api_key);
    HttpClientError_t enable(bool enable = true);
    
    // Requêtes HTTP principales
    HttpResponse_t get(const char* endpoint, const char* headers = nullptr);
    HttpResponse_t post(const char* endpoint, const char* data, const char* headers = nullptr);
    HttpResponse_t put(const char* endpoint, const char* data, const char* headers = nullptr);
    HttpResponse_t delete_request(const char* endpoint, const char* headers = nullptr);
    HttpResponse_t patch(const char* endpoint, const char* data, const char* headers = nullptr);
    HttpResponse_t head(const char* endpoint, const char* headers = nullptr);
    HttpResponse_t options(const char* endpoint, const char* headers = nullptr);
    
    // Requête générique
    HttpResponse_t request(HttpRequest_t* request);
    
    // Tests et diagnostic
    HttpClientError_t test_connectivity(void);
    HttpClientError_t test_endpoint(const char* endpoint);
    HttpClientError_t test_performance(const char* endpoint, uint8_t iterations);
    
    // Statistiques
    HttpClientError_t get_stats(HttpStats_t* stats_out);
    HttpClientError_t reset_stats(void);
    void print_stats(void);
    
    // Validation et diagnostic
    HttpClientError_t validate_config(void);
    void print_debug_info(void);
    
    // Persistance
    HttpClientError_t save_config_to_nvs(void);
    HttpClientError_t load_config_from_nvs(void);
};

// Instance globale
extern HttpClientManager http_client;

// Fonctions d'interface console
SysError_t cmd_http_config(int argc, char* argv[]);
SysError_t cmd_http_get(int argc, char* argv[]);
SysError_t cmd_http_post(int argc, char* argv[]);
SysError_t cmd_http_put(int argc, char* argv[]);
SysError_t cmd_http_delete(int argc, char* argv[]);
SysError_t cmd_http_patch(int argc, char* argv[]);
SysError_t cmd_http_head(int argc, char* argv[]);
SysError_t cmd_http_options(int argc, char* argv[]);
SysError_t cmd_http_test(int argc, char* argv[]);
SysError_t cmd_http_stats(int argc, char* argv[]);
SysError_t cmd_http_debug(int argc, char* argv[]);

// Fonction d'initialisation du module HTTP Client
SysError_t http_client_module_init(void);
void http_client_module_deinit(void);

// Fonctions utilitaires
const char* http_method_to_string(HttpMethod_t method);
HttpMethod_t string_to_http_method(const char* str);
const char* http_client_error_to_string(HttpClientError_t error);
void http_print_response_summary(const HttpResponse_t* response);

#endif // HTTP_CLIENT_H
