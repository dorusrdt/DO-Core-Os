#include "http_client.h"
#include "../core/log_system_optimized.h"
#include <Preferences.h>

// Instance globale
HttpClientManager http_client;

// Constructeur
HttpClientManager::HttpClientManager() {
    memset(&config, 0, sizeof(config));
    memset(&stats, 0, sizeof(stats));
    initialized = false;
    mutex = NULL;
    
    // Configuration par défaut
    strcpy(config.server_url, "http://192.168.1.100");
    config.port = 80;
    config.timeout_ms = HTTP_CLIENT_DEFAULT_TIMEOUT;
    config.retry_count = HTTP_CLIENT_DEFAULT_RETRY_COUNT;
    config.retry_delay_ms = 1000;
    config.use_ssl = false;
    strcpy(config.user_agent, "D'O-Core-HTTP-Client/1.0");
    config.enabled = false;
    config.version = 1;
}

// Destructeur
HttpClientManager::~HttpClientManager() {
    deinit();
}

// Initialisation
HttpClientError_t HttpClientManager::init(void) {
    if (initialized) {
        return HTTP_CLIENT_OK;
    }
    
    kernel_log(LOG_LEVEL_INFO, "HTTP Client init");
    
    // Créer le mutex
    mutex = xSemaphoreCreateMutex();
    if (!mutex) {
        kernel_log(LOG_LEVEL_ERROR, "HTTP Client: Failed to create mutex");
        return HTTP_CLIENT_ERROR_INIT;
    }
    
    // Charger la configuration depuis NVS
    load_config_from_nvs();
    
    initialized = true;
    kernel_log(LOG_LEVEL_INFO, "HTTP Client initialized");
    
    return HTTP_CLIENT_OK;
}

// Déinitialisation
HttpClientError_t HttpClientManager::deinit(void) {
    if (!initialized) {
        return HTTP_CLIENT_OK;
    }
    
    kernel_log(LOG_LEVEL_INFO, "HTTP Client deinit");
    
    if (mutex) {
        vSemaphoreDelete(mutex);
        mutex = NULL;
    }
    
    initialized = false;
    return HTTP_CLIENT_OK;
}

// Vérifier si initialisé
bool HttpClientManager::is_initialized(void) const {
    return initialized;
}

// Configuration
HttpClientError_t HttpClientManager::set_config(const HttpClientConfig_t* new_config) {
    if (!initialized || !new_config) {
        return HTTP_CLIENT_ERROR_PARAM;
    }
    
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return HTTP_CLIENT_ERROR_TIMEOUT;
    }
    
    memcpy(&config, new_config, sizeof(config));
    config.checksum = calculate_checksum(&config, sizeof(config) - sizeof(config.checksum));
    
    xSemaphoreGive(mutex);
    
    // Sauvegarder en NVS
    save_config_to_nvs();
    
    kernel_log(LOG_LEVEL_INFO, "HTTP Client config updated: %s", config.server_url);
    return HTTP_CLIENT_OK;
}

// Obtenir la configuration
HttpClientError_t HttpClientManager::get_config(HttpClientConfig_t* config_out) {
    if (!initialized || !config_out) {
        return HTTP_CLIENT_ERROR_PARAM;
    }
    
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return HTTP_CLIENT_ERROR_TIMEOUT;
    }
    
    memcpy(config_out, &config, sizeof(config));
    
    xSemaphoreGive(mutex);
    return HTTP_CLIENT_OK;
}

// Réinitialiser la configuration
HttpClientError_t HttpClientManager::reset_config(void) {
    HttpClientConfig_t default_config;
    memset(&default_config, 0, sizeof(default_config));
    
    strcpy(default_config.server_url, "http://192.168.1.100");
    default_config.port = 80;
    default_config.timeout_ms = HTTP_CLIENT_DEFAULT_TIMEOUT;
    default_config.retry_count = HTTP_CLIENT_DEFAULT_RETRY_COUNT;
    default_config.retry_delay_ms = 1000;
    default_config.use_ssl = false;
    strcpy(default_config.user_agent, "D'O-Core-HTTP-Client/1.0");
    default_config.enabled = false;
    default_config.version = 1;
    
    return set_config(&default_config);
}

// Définir l'URL du serveur
HttpClientError_t HttpClientManager::set_server_url(const char* url) {
    if (!initialized || !url) {
        return HTTP_CLIENT_ERROR_PARAM;
    }
    
    if (strlen(url) >= HTTP_CLIENT_MAX_URL_LENGTH) {
        return HTTP_CLIENT_ERROR_PARAM;
    }
    
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return HTTP_CLIENT_ERROR_TIMEOUT;
    }
    
    strcpy(config.server_url, url);
    config.checksum = calculate_checksum(&config, sizeof(config) - sizeof(config.checksum));
    
    xSemaphoreGive(mutex);
    
    save_config_to_nvs();
    kernel_log(LOG_LEVEL_INFO, "HTTP Client server URL: %s", config.server_url);
    return HTTP_CLIENT_OK;
}

// Définir le timeout
HttpClientError_t HttpClientManager::set_timeout(uint32_t timeout_ms) {
    if (!initialized) {
        return HTTP_CLIENT_ERROR_PARAM;
    }
    
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return HTTP_CLIENT_ERROR_TIMEOUT;
    }
    
    config.timeout_ms = timeout_ms;
    config.checksum = calculate_checksum(&config, sizeof(config) - sizeof(config.checksum));
    
    xSemaphoreGive(mutex);
    
    save_config_to_nvs();
    kernel_log(LOG_LEVEL_INFO, "HTTP Client timeout: %lu ms", config.timeout_ms);
    return HTTP_CLIENT_OK;
}

// Définir le nombre de tentatives
HttpClientError_t HttpClientManager::set_retry_count(uint8_t count) {
    if (!initialized) {
        return HTTP_CLIENT_ERROR_PARAM;
    }
    
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return HTTP_CLIENT_ERROR_TIMEOUT;
    }
    
    config.retry_count = count;
    config.checksum = calculate_checksum(&config, sizeof(config) - sizeof(config.checksum));
    
    xSemaphoreGive(mutex);
    
    save_config_to_nvs();
    kernel_log(LOG_LEVEL_INFO, "HTTP Client retry count: %d", config.retry_count);
    return HTTP_CLIENT_OK;
}

// Définir la clé API
HttpClientError_t HttpClientManager::set_api_key(const char* api_key) {
    if (!initialized || !api_key) {
        return HTTP_CLIENT_ERROR_PARAM;
    }
    
    if (strlen(api_key) >= 64) {
        return HTTP_CLIENT_ERROR_PARAM;
    }
    
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return HTTP_CLIENT_ERROR_TIMEOUT;
    }
    
    strcpy(config.api_key, api_key);
    config.checksum = calculate_checksum(&config, sizeof(config) - sizeof(config.checksum));
    
    xSemaphoreGive(mutex);
    
    save_config_to_nvs();
    kernel_log(LOG_LEVEL_INFO, "HTTP Client API key set");
    return HTTP_CLIENT_OK;
}

// Activer/Désactiver le client HTTP
HttpClientError_t HttpClientManager::enable(bool enable) {
    if (!initialized) {
        return HTTP_CLIENT_ERROR_INIT;
    }
    
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return HTTP_CLIENT_ERROR_TIMEOUT;
    }
    
    config.enabled = enable;
    config.checksum = calculate_checksum(&config, sizeof(config) - sizeof(config.checksum));
    
    xSemaphoreGive(mutex);
    
    save_config_to_nvs();
    kernel_log(LOG_LEVEL_INFO, "HTTP Client %s", enable ? "enabled" : "disabled");
    return HTTP_CLIENT_OK;
}

// Requête GET
HttpResponse_t HttpClientManager::get(const char* endpoint, const char* headers) {
    HttpRequest_t request;
    memset(&request, 0, sizeof(request));
    
    request.method = HTTP_METHOD_GET;
    if (endpoint) {
        strncpy(request.endpoint, endpoint, sizeof(request.endpoint) - 1);
    }
    if (headers) {
        strncpy(request.headers, headers, sizeof(request.headers) - 1);
    }
    request.timeout_ms = config.timeout_ms;
    request.follow_redirects = true;
    request.priority = 1;
    
    return this->request(&request);
}

// Requête POST
HttpResponse_t HttpClientManager::post(const char* endpoint, const char* data, const char* headers) {
    HttpRequest_t request;
    memset(&request, 0, sizeof(request));
    
    request.method = HTTP_METHOD_POST;
    if (endpoint) {
        strncpy(request.endpoint, endpoint, sizeof(request.endpoint) - 1);
    }
    if (data) {
        strncpy(request.body, data, sizeof(request.body) - 1);
    }
    if (headers) {
        strncpy(request.headers, headers, sizeof(request.headers) - 1);
    }
    request.timeout_ms = config.timeout_ms;
    request.follow_redirects = true;
    request.priority = 1;
    
    return this->request(&request);
}

// Requête PUT
HttpResponse_t HttpClientManager::put(const char* endpoint, const char* data, const char* headers) {
    HttpRequest_t request;
    memset(&request, 0, sizeof(request));
    
    request.method = HTTP_METHOD_PUT;
    if (endpoint) {
        strncpy(request.endpoint, endpoint, sizeof(request.endpoint) - 1);
    }
    if (data) {
        strncpy(request.body, data, sizeof(request.body) - 1);
    }
    if (headers) {
        strncpy(request.headers, headers, sizeof(request.headers) - 1);
    }
    request.timeout_ms = config.timeout_ms;
    request.follow_redirects = true;
    request.priority = 1;
    
    return this->request(&request);
}

// Requête DELETE
HttpResponse_t HttpClientManager::delete_request(const char* endpoint, const char* headers) {
    HttpRequest_t request;
    memset(&request, 0, sizeof(request));
    
    request.method = HTTP_METHOD_DELETE;
    if (endpoint) {
        strncpy(request.endpoint, endpoint, sizeof(request.endpoint) - 1);
    }
    if (headers) {
        strncpy(request.headers, headers, sizeof(request.headers) - 1);
    }
    request.timeout_ms = config.timeout_ms;
    request.follow_redirects = true;
    request.priority = 1;
    
    return this->request(&request);
}

// Requête PATCH
HttpResponse_t HttpClientManager::patch(const char* endpoint, const char* data, const char* headers) {
    HttpRequest_t request;
    memset(&request, 0, sizeof(request));
    
    request.method = HTTP_METHOD_PATCH;
    if (endpoint) {
        strncpy(request.endpoint, endpoint, sizeof(request.endpoint) - 1);
    }
    if (data) {
        strncpy(request.body, data, sizeof(request.body) - 1);
    }
    if (headers) {
        strncpy(request.headers, headers, sizeof(request.headers) - 1);
    }
    request.timeout_ms = config.timeout_ms;
    request.follow_redirects = true;
    request.priority = 1;
    
    return this->request(&request);
}

// Requête HEAD
HttpResponse_t HttpClientManager::head(const char* endpoint, const char* headers) {
    HttpRequest_t request;
    memset(&request, 0, sizeof(request));
    
    request.method = HTTP_METHOD_HEAD;
    if (endpoint) {
        strncpy(request.endpoint, endpoint, sizeof(request.endpoint) - 1);
    }
    if (headers) {
        strncpy(request.headers, headers, sizeof(request.headers) - 1);
    }
    request.timeout_ms = config.timeout_ms;
    request.follow_redirects = true;
    request.priority = 1;
    
    return this->request(&request);
}

// Requête OPTIONS
HttpResponse_t HttpClientManager::options(const char* endpoint, const char* headers) {
    HttpRequest_t request;
    memset(&request, 0, sizeof(request));
    
    request.method = HTTP_METHOD_OPTIONS;
    if (endpoint) {
        strncpy(request.endpoint, endpoint, sizeof(request.endpoint) - 1);
    }
    if (headers) {
        strncpy(request.headers, headers, sizeof(request.headers) - 1);
    }
    request.timeout_ms = config.timeout_ms;
    request.follow_redirects = true;
    request.priority = 1;
    
    return this->request(&request);
}

// Requête générique
HttpResponse_t HttpClientManager::request(HttpRequest_t* request) {
    HttpResponse_t response;
    memset(&response, 0, sizeof(response));
    
    if (!initialized || !request) {
        response.success = false;
        response.error_code = HTTP_CLIENT_ERROR_PARAM;
        strcpy(response.error_message, "Invalid parameters");
        return response;
    }
    
    // Vérifier la connectivité WiFi
    if (WiFi.status() != WL_CONNECTED) {
        response.success = false;
        response.error_code = HTTP_CLIENT_ERROR_WIFI;
        strcpy(response.error_message, "WiFi not connected");
        return response;
    }
    
    // Envoyer la requête avec retry
    uint8_t attempts = 0;
    HttpClientError_t result;
    
    while (attempts <= config.retry_count) {
        result = send_request_internal(request, &response);
        
        if (result == HTTP_CLIENT_OK) {
            break;
        }
        
        attempts++;
        if (attempts <= config.retry_count) {
            kernel_log(LOG_LEVEL_WARN, "HTTP request failed, retry %d/%d", attempts, config.retry_count);
            delay(config.retry_delay_ms);
        }
    }
    
    // Mettre à jour les statistiques
    update_stats(response.success, response.response_time_ms, 
                strlen(request->body), response.content_length);
    
    return response;
}

// Envoi interne de requête
HttpClientError_t HttpClientManager::send_request_internal(HttpRequest_t* request, HttpResponse_t* response) {
    HTTPClient client;
    uint32_t start_time = millis();
    
    // Construire l'URL complète
    char full_url[256];
    snprintf(full_url, sizeof(full_url), "%s%s", config.server_url, request->endpoint);
    
    kernel_log(LOG_LEVEL_INFO, "HTTP %s %s", method_to_string(request->method), full_url);
    
    // Configurer le client
    client.setTimeout(request->timeout_ms);
    client.setFollowRedirects(request->follow_redirects ? HTTPC_FORCE_FOLLOW_REDIRECTS : HTTPC_DISABLE_FOLLOW_REDIRECTS);
    
    // Ajouter les headers par défaut
    add_default_headers(client, request->headers);
    
    // Déterminer le type de contenu pour POST/PUT/PATCH
    if (request->method == HTTP_METHOD_POST || 
        request->method == HTTP_METHOD_PUT || 
        request->method == HTTP_METHOD_PATCH) {
        if (strlen(request->body) > 0) {
            client.addHeader("Content-Type", "application/json");
        }
    }
    
    // Envoyer la requête selon la méthode
    int http_code = 0;
    switch (request->method) {
        case HTTP_METHOD_GET:
            http_code = client.begin(full_url);
            if (http_code == HTTP_CODE_OK) {
                http_code = client.GET();
            }
            break;
            
        case HTTP_METHOD_POST:
            http_code = client.begin(full_url);
            if (http_code == HTTP_CODE_OK) {
                http_code = client.POST(request->body);
            }
            break;
            
        case HTTP_METHOD_PUT:
            http_code = client.begin(full_url);
            if (http_code == HTTP_CODE_OK) {
                http_code = client.PUT(request->body);
            }
            break;
            
        case HTTP_METHOD_DELETE:
            http_code = client.begin(full_url);
            if (http_code == HTTP_CODE_OK) {
                http_code = client.sendRequest("DELETE");
            }
            break;
            
        case HTTP_METHOD_PATCH:
            http_code = client.begin(full_url);
            if (http_code == HTTP_CODE_OK) {
                http_code = client.PATCH(request->body);
            }
            break;
            
        case HTTP_METHOD_HEAD:
            http_code = client.begin(full_url);
            if (http_code == HTTP_CODE_OK) {
                http_code = client.sendRequest("HEAD");
            }
            break;
            
        case HTTP_METHOD_OPTIONS:
            http_code = client.begin(full_url);
            if (http_code == HTTP_CODE_OK) {
                http_code = client.sendRequest("OPTIONS");
            }
            break;
            
        default:
            client.end();
            return HTTP_CLIENT_ERROR_PARAM;
    }
    
    // Traiter la réponse
    response->response_time_ms = millis() - start_time;
    response->status_code = http_code;
    
    if (http_code > 0) {
        response->success = true;
        response->error_code = HTTP_CLIENT_OK;
        
        // Récupérer le corps de la réponse
        String response_body = client.getString();
        strncpy(response->body, response_body.c_str(), sizeof(response->body) - 1);
        response->content_length = response_body.length();
        
        // Récupérer les headers de réponse
        String response_headers = client.header("Content-Type");
        strncpy(response->headers, response_headers.c_str(), sizeof(response->headers) - 1);
        
        kernel_log(LOG_LEVEL_INFO, "HTTP response: %d, size: %lu, time: %lu ms", 
                  http_code, response->content_length, response->response_time_ms);
    } else {
        response->success = false;
        response->error_code = HTTP_CLIENT_ERROR_CONNECTION;
        snprintf(response->error_message, sizeof(response->error_message), 
                "HTTP error: %d", http_code);
        
        kernel_log(LOG_LEVEL_ERROR, "HTTP request failed: %d", http_code);
    }
    
    client.end();
    return response->success ? HTTP_CLIENT_OK : HTTP_CLIENT_ERROR_CONNECTION;
}

// Ajouter les headers par défaut
bool HttpClientManager::add_default_headers(HTTPClient& client, const char* custom_headers) {
    // User-Agent
    client.addHeader("User-Agent", config.user_agent);
    
    // API Key si configurée
    if (strlen(config.api_key) > 0) {
        client.addHeader("X-API-Key", config.api_key);
    }
    
    // Headers personnalisés
    if (custom_headers && strlen(custom_headers) > 0) {
        // Parser les headers personnalisés (format: "Header1: Value1, Header2: Value2")
        char* headers_copy = strdup(custom_headers);
        char* token = strtok(headers_copy, ",");
        
        while (token != NULL) {
            char* colon = strchr(token, ':');
            if (colon) {
                *colon = '\0';
                char* header_name = token;
                char* header_value = colon + 1;
                
                // Supprimer les espaces
                while (*header_name == ' ') header_name++;
                while (*header_value == ' ') header_value++;
                
                client.addHeader(header_name, header_value);
            }
            token = strtok(NULL, ",");
        }
        
        free(headers_copy);
    }
    
    return true;
}

// Tests et diagnostic
HttpClientError_t HttpClientManager::test_connectivity(void) {
    if (!initialized) {
        return HTTP_CLIENT_ERROR_INIT;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        return HTTP_CLIENT_ERROR_WIFI;
    }
    
    // Test simple GET sur l'endpoint racine
    HttpResponse_t response = get("/");
    return response.success ? HTTP_CLIENT_OK : HTTP_CLIENT_ERROR_CONNECTION;
}

HttpClientError_t HttpClientManager::test_endpoint(const char* endpoint) {
    if (!initialized || !endpoint) {
        return HTTP_CLIENT_ERROR_PARAM;
    }
    
    HttpResponse_t response = get(endpoint);
    return response.success ? HTTP_CLIENT_OK : HTTP_CLIENT_ERROR_CONNECTION;
}

HttpClientError_t HttpClientManager::test_performance(const char* endpoint, uint8_t iterations) {
    if (!initialized || !endpoint || iterations == 0) {
        return HTTP_CLIENT_ERROR_PARAM;
    }
    
    uint32_t total_time = 0;
    uint8_t successful_requests = 0;
    
    for (uint8_t i = 0; i < iterations; i++) {
        HttpResponse_t response = get(endpoint);
        if (response.success) {
            total_time += response.response_time_ms;
            successful_requests++;
        }
        delay(100); // Petit délai entre les requêtes
    }
    
    if (successful_requests > 0) {
        uint32_t avg_time = total_time / successful_requests;
        kernel_log(LOG_LEVEL_INFO, "Performance test: %d/%d successful, avg time: %lu ms", 
                  successful_requests, iterations, avg_time);
    }
    
    return successful_requests > 0 ? HTTP_CLIENT_OK : HTTP_CLIENT_ERROR_CONNECTION;
}

// Statistiques
HttpClientError_t HttpClientManager::get_stats(HttpStats_t* stats_out) {
    if (!initialized || !stats_out) {
        return HTTP_CLIENT_ERROR_PARAM;
    }
    
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return HTTP_CLIENT_ERROR_TIMEOUT;
    }
    
    memcpy(stats_out, &stats, sizeof(stats));
    
    xSemaphoreGive(mutex);
    return HTTP_CLIENT_OK;
}

HttpClientError_t HttpClientManager::reset_stats(void) {
    if (!initialized) {
        return HTTP_CLIENT_ERROR_PARAM;
    }
    
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return HTTP_CLIENT_ERROR_TIMEOUT;
    }
    
    memset(&stats, 0, sizeof(stats));
    
    xSemaphoreGive(mutex);
    kernel_log(LOG_LEVEL_INFO, "HTTP Client stats reset");
    return HTTP_CLIENT_OK;
}

void HttpClientManager::print_stats(void) {
    if (!initialized) {
        Serial.println("HTTP Client not initialized");
        return;
    }
    
    HttpStats_t current_stats;
    if (get_stats(&current_stats) != HTTP_CLIENT_OK) {
        Serial.println("Failed to get HTTP stats");
        return;
    }
    
    Serial.println("=== HTTP Client Statistics ===");
    Serial.printf("Total requests: %lu\n", current_stats.total_requests);
    Serial.printf("Successful: %lu\n", current_stats.successful_requests);
    Serial.printf("Failed: %lu\n", current_stats.failed_requests);
    Serial.printf("Timeout errors: %lu\n", current_stats.timeout_errors);
    Serial.printf("Connection errors: %lu\n", current_stats.connection_errors);
    Serial.printf("Average response time: %lu ms\n", current_stats.average_response_time);
    Serial.printf("Total bytes sent: %lu\n", current_stats.total_bytes_sent);
    Serial.printf("Total bytes received: %lu\n", current_stats.total_bytes_received);
    Serial.println("=============================");
}

// Validation et diagnostic
HttpClientError_t HttpClientManager::validate_config(void) {
    if (!initialized) {
        return HTTP_CLIENT_ERROR_INIT;
    }
    
    if (strlen(config.server_url) == 0) {
        return HTTP_CLIENT_ERROR_PARAM;
    }
    
    if (config.timeout_ms == 0) {
        return HTTP_CLIENT_ERROR_PARAM;
    }
    
    return HTTP_CLIENT_OK;
}

void HttpClientManager::print_debug_info(void) {
    if (!initialized) {
        Serial.println("HTTP Client not initialized");
        return;
    }
    
    Serial.println("=== HTTP Client Debug Info ===");
    Serial.printf("Server URL: %s\n", config.server_url);
    Serial.printf("Port: %d\n", config.port);
    Serial.printf("Timeout: %lu ms\n", config.timeout_ms);
    Serial.printf("Retry count: %d\n", config.retry_count);
    Serial.printf("SSL: %s\n", config.use_ssl ? "Yes" : "No");
    Serial.printf("Enabled: %s\n", config.enabled ? "Yes" : "No");
    Serial.printf("User Agent: %s\n", config.user_agent);
    Serial.printf("API Key: %s\n", strlen(config.api_key) > 0 ? "Set" : "Not set");
    Serial.println("=============================");
}

// Persistance
HttpClientError_t HttpClientManager::save_config_to_nvs(void) {
    Preferences prefs;
    if (!prefs.begin("http_client", false)) {
        return HTTP_CLIENT_ERROR_MEMORY;
    }
    
    bool success = prefs.putBytes("config", &config, sizeof(config)) == sizeof(config);
    prefs.end();
    
    if (success) {
        kernel_log(LOG_LEVEL_INFO, "HTTP Client config saved to NVS");
        return HTTP_CLIENT_OK;
    } else {
        kernel_log(LOG_LEVEL_ERROR, "HTTP Client config save failed");
        return HTTP_CLIENT_ERROR_MEMORY;
    }
}

HttpClientError_t HttpClientManager::load_config_from_nvs(void) {
    Preferences prefs;
    if (!prefs.begin("http_client", true)) {
        return HTTP_CLIENT_ERROR_MEMORY;
    }
    
    size_t bytes_read = prefs.getBytes("config", &config, sizeof(config));
    prefs.end();
    
    if (bytes_read == sizeof(config)) {
        // Valider le checksum
        uint32_t calculated_checksum = calculate_checksum(&config, sizeof(config) - sizeof(config.checksum));
        if (calculated_checksum == config.checksum) {
            kernel_log(LOG_LEVEL_INFO, "HTTP Client config loaded from NVS");
            return HTTP_CLIENT_OK;
        } else {
            kernel_log(LOG_LEVEL_WARN, "HTTP Client config checksum invalid, using defaults");
            reset_config();
            return HTTP_CLIENT_OK;
        }
    } else {
        kernel_log(LOG_LEVEL_INFO, "HTTP Client config not found in NVS, using defaults");
        reset_config();
        return HTTP_CLIENT_OK;
    }
}

// Fonctions utilitaires privées
uint32_t HttpClientManager::calculate_checksum(const void* data, size_t size) {
    uint32_t checksum = 0;
    const uint8_t* bytes = (const uint8_t*)data;
    
    for (size_t i = 0; i < size; i++) {
        checksum += bytes[i];
    }
    
    return checksum;
}

bool HttpClientManager::validate_checksum(const void* data, size_t size, uint32_t expected_checksum) {
    return calculate_checksum(data, size) == expected_checksum;
}

const char* HttpClientManager::method_to_string(HttpMethod_t method) {
    switch (method) {
        case HTTP_METHOD_GET: return "GET";
        case HTTP_METHOD_POST: return "POST";
        case HTTP_METHOD_PUT: return "PUT";
        case HTTP_METHOD_DELETE: return "DELETE";
        case HTTP_METHOD_PATCH: return "PATCH";
        case HTTP_METHOD_HEAD: return "HEAD";
        case HTTP_METHOD_OPTIONS: return "OPTIONS";
        case HTTP_METHOD_TRACE: return "TRACE";
        case HTTP_METHOD_CONNECT: return "CONNECT";
        default: return "UNKNOWN";
    }
}

const char* HttpClientManager::error_to_string(HttpClientError_t error) {
    switch (error) {
        case HTTP_CLIENT_OK: return "OK";
        case HTTP_CLIENT_ERROR_INIT: return "Initialization error";
        case HTTP_CLIENT_ERROR_WIFI: return "WiFi error";
        case HTTP_CLIENT_ERROR_URL: return "URL error";
        case HTTP_CLIENT_ERROR_CONNECTION: return "Connection error";
        case HTTP_CLIENT_ERROR_TIMEOUT: return "Timeout error";
        case HTTP_CLIENT_ERROR_RESPONSE: return "Response error";
        case HTTP_CLIENT_ERROR_MEMORY: return "Memory error";
        case HTTP_CLIENT_ERROR_PARAM: return "Parameter error";
        default: return "Unknown error";
    }
}

void HttpClientManager::update_stats(bool success, uint32_t response_time, uint32_t bytes_sent, uint32_t bytes_received) {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return;
    }
    
    stats.total_requests++;
    
    if (success) {
        stats.successful_requests++;
        stats.last_successful_request = millis();
        
        // Calculer la moyenne du temps de réponse
        if (stats.average_response_time == 0) {
            stats.average_response_time = response_time;
        } else {
            stats.average_response_time = (stats.average_response_time + response_time) / 2;
        }
    } else {
        stats.failed_requests++;
        stats.last_failed_request = millis();
    }
    
    stats.total_bytes_sent += bytes_sent;
    stats.total_bytes_received += bytes_received;
    
    xSemaphoreGive(mutex);
}

// Fonctions utilitaires publiques
const char* http_method_to_string(HttpMethod_t method) {
    switch (method) {
        case HTTP_METHOD_GET: return "GET";
        case HTTP_METHOD_POST: return "POST";
        case HTTP_METHOD_PUT: return "PUT";
        case HTTP_METHOD_DELETE: return "DELETE";
        case HTTP_METHOD_PATCH: return "PATCH";
        case HTTP_METHOD_HEAD: return "HEAD";
        case HTTP_METHOD_OPTIONS: return "OPTIONS";
        case HTTP_METHOD_TRACE: return "TRACE";
        case HTTP_METHOD_CONNECT: return "CONNECT";
        default: return "UNKNOWN";
    }
}

HttpMethod_t string_to_http_method(const char* str) {
    if (!str) return HTTP_METHOD_GET;
    
    if (strcmp(str, "GET") == 0) return HTTP_METHOD_GET;
    if (strcmp(str, "POST") == 0) return HTTP_METHOD_POST;
    if (strcmp(str, "PUT") == 0) return HTTP_METHOD_PUT;
    if (strcmp(str, "DELETE") == 0) return HTTP_METHOD_DELETE;
    if (strcmp(str, "PATCH") == 0) return HTTP_METHOD_PATCH;
    if (strcmp(str, "HEAD") == 0) return HTTP_METHOD_HEAD;
    if (strcmp(str, "OPTIONS") == 0) return HTTP_METHOD_OPTIONS;
    if (strcmp(str, "TRACE") == 0) return HTTP_METHOD_TRACE;
    if (strcmp(str, "CONNECT") == 0) return HTTP_METHOD_CONNECT;
    
    return HTTP_METHOD_GET; // Par défaut
}

const char* http_client_error_to_string(HttpClientError_t error) {
    switch (error) {
        case HTTP_CLIENT_OK: return "OK";
        case HTTP_CLIENT_ERROR_INIT: return "Initialization error";
        case HTTP_CLIENT_ERROR_WIFI: return "WiFi error";
        case HTTP_CLIENT_ERROR_URL: return "URL error";
        case HTTP_CLIENT_ERROR_CONNECTION: return "Connection error";
        case HTTP_CLIENT_ERROR_TIMEOUT: return "Timeout error";
        case HTTP_CLIENT_ERROR_RESPONSE: return "Response error";
        case HTTP_CLIENT_ERROR_MEMORY: return "Memory error";
        case HTTP_CLIENT_ERROR_PARAM: return "Parameter error";
        default: return "Unknown error";
    }
}

void http_print_response_summary(const HttpResponse_t* response) {
    if (!response) {
        Serial.println("Invalid response");
        return;
    }
    
    Serial.printf("HTTP Response: %d - %s\n", 
                  response->status_code, 
                  response->success ? "SUCCESS" : "FAILED");
    
    if (!response->success) {
        Serial.printf("Error: %s\n", response->error_message);
    }
    
    Serial.printf("Response time: %lu ms\n", response->response_time_ms);
    Serial.printf("Content length: %lu bytes\n", response->content_length);
    
    if (strlen(response->body) > 0) {
        Serial.printf("Body: %s\n", response->body);
    }
}

// Fonction d'initialisation du module
SysError_t http_client_module_init(void) {
    kernel_log(LOG_LEVEL_INFO, "HTTP Client module init");
    
    HttpClientError_t result = http_client.init();
    if (result != HTTP_CLIENT_OK) {
        kernel_log(LOG_LEVEL_ERROR, "HTTP Client module init failed: %d", result);
        return SYS_ERROR;
    }
    
    kernel_log(LOG_LEVEL_INFO, "HTTP Client module initialized successfully");
    return SYS_OK;
}

void http_client_module_deinit(void) {
    kernel_log(LOG_LEVEL_INFO, "HTTP Client module deinit");
    http_client.deinit();
}
