#include "http_client.h"
#include "../interface/interface.h"
#include <string.h>
#include <stdlib.h>

// Commande de configuration HTTP
SysError_t cmd_http_config(int argc, char* argv[]) {
    if (argc < 2) {
        Serial.println("=== HTTP Client Configuration ===");
        Serial.println("Usage:");
        Serial.println("  http_config show                    - Show current configuration");
        Serial.println("  http_config enable                  - Enable HTTP client");
        Serial.println("  http_config disable                 - Disable HTTP client");
        Serial.println("  http_config set server <url>        - Set server URL");
        Serial.println("  http_config set timeout <ms>        - Set timeout in milliseconds");
        Serial.println("  http_config set retry <count>       - Set retry count");
        Serial.println("  http_config set api_key <key>       - Set API key");
        Serial.println("  http_config reset                   - Reset to defaults");
        Serial.println("");
        Serial.println("Examples:");
        Serial.println("  http_config set server \"http://192.168.1.100:3000\"");
        Serial.println("  http_config set timeout 15000");
        Serial.println("  http_config set retry 5");
        Serial.println("================================");
        return SYS_OK;
    }
    
    if (strcmp(argv[1], "show") == 0) {
        http_client.print_debug_info();
        return SYS_OK;
    }

    if (strcmp(argv[1], "enable") == 0) {
        HttpClientError_t result = http_client.enable(true);
        if (result == HTTP_CLIENT_OK) {
            Serial.println("✅ HTTP client enabled");
        } else {
            Serial.printf("❌ Failed to enable HTTP client: %s\n",
                         http_client_error_to_string(result));
        }
        return result == HTTP_CLIENT_OK ? SYS_OK : SYS_ERROR;
    }

    if (strcmp(argv[1], "disable") == 0) {
        HttpClientError_t result = http_client.enable(false);
        if (result == HTTP_CLIENT_OK) {
            Serial.println("✅ HTTP client disabled");
        } else {
            Serial.printf("❌ Failed to disable HTTP client: %s\n",
                         http_client_error_to_string(result));
        }
        return result == HTTP_CLIENT_OK ? SYS_OK : SYS_ERROR;
    }
    
    if (strcmp(argv[1], "reset") == 0) {
        HttpClientError_t result = http_client.reset_config();
        if (result == HTTP_CLIENT_OK) {
            Serial.println("✅ HTTP configuration reset to defaults");
        } else {
            Serial.printf("❌ Failed to reset configuration: %s\n", 
                         http_client_error_to_string(result));
        }
        return result == HTTP_CLIENT_OK ? SYS_OK : SYS_ERROR;
    }
    
    if (strcmp(argv[1], "set") == 0) {
        if (argc < 4) {
            Serial.println("❌ Usage: http_config set <parameter> <value>");
            return SYS_INVALID_PARAM;
        }
        
        const char* param = argv[2];
        const char* value = argv[3];
        
        HttpClientError_t result = HTTP_CLIENT_ERROR_PARAM;
        
        if (strcmp(param, "server") == 0) {
            result = http_client.set_server_url(value);
            if (result == HTTP_CLIENT_OK) {
                Serial.printf("✅ Server URL set to: %s\n", value);
            }
        }
        else if (strcmp(param, "timeout") == 0) {
            uint32_t timeout_ms = atoi(value);
            if (timeout_ms > 0) {
                result = http_client.set_timeout(timeout_ms);
                if (result == HTTP_CLIENT_OK) {
                    Serial.printf("✅ Timeout set to: %u ms\n", timeout_ms);
                }
            } else {
                Serial.println("❌ Invalid timeout value (must be > 0)");
                return SYS_INVALID_PARAM;
            }
        }
        else if (strcmp(param, "retry") == 0) {
            uint8_t retry_count = atoi(value);
            if (retry_count <= 10) {
                result = http_client.set_retry_count(retry_count);
                if (result == HTTP_CLIENT_OK) {
                    Serial.printf("✅ Retry count set to: %d\n", retry_count);
                }
            } else {
                Serial.println("❌ Invalid retry count (must be <= 10)");
                return SYS_INVALID_PARAM;
            }
        }
        else if (strcmp(param, "api_key") == 0) {
            result = http_client.set_api_key(value);
            if (result == HTTP_CLIENT_OK) {
                Serial.println("✅ API key set");
            }
        }
        else {
            Serial.printf("❌ Unknown parameter: %s\n", param);
            return SYS_INVALID_PARAM;
        }
        
        if (result != HTTP_CLIENT_OK) {
            Serial.printf("❌ Failed to set %s: %s\n", param, 
                         http_client_error_to_string(result));
            return SYS_ERROR;
        }
        
        return SYS_OK;
    }
    
    Serial.printf("❌ Unknown command: %s\n", argv[1]);
    return SYS_INVALID_PARAM;
}

// Commande GET HTTP
SysError_t cmd_http_get(int argc, char* argv[]) {
    if (argc < 2) {
        Serial.println("Usage: http_get <endpoint> [headers]");
        Serial.println("Examples:");
        Serial.println("  http_get /api/status");
        Serial.println("  http_get /api/data \"Accept: application/json\"");
        return SYS_INVALID_PARAM;
    }
    
    const char* endpoint = argv[1];
    const char* headers = (argc >= 3) ? argv[2] : nullptr;
    
    Serial.printf("🌐 HTTP GET %s\n", endpoint);
    
    HttpResponse_t response = http_client.get(endpoint, headers);
    
    http_print_response_summary(&response);
    
    return response.success ? SYS_OK : SYS_ERROR;
}

// Commande POST HTTP
SysError_t cmd_http_post(int argc, char* argv[]) {
    if (argc < 3) {
        Serial.println("Usage: http_post <endpoint> <data> [headers]");
        Serial.println("Examples:");
        Serial.println("  http_post /api/data '{\"key\": \"value\"}'");
        Serial.println("  http_post /api/users '{\"name\": \"John\"}' \"Content-Type: application/json\"");
        return SYS_INVALID_PARAM;
    }
    
    const char* endpoint = argv[1];
    const char* data = argv[2];
    const char* headers = (argc >= 4) ? argv[3] : nullptr;
    
    Serial.printf("🌐 HTTP POST %s\n", endpoint);
    Serial.printf("📤 Data: %s\n", data);
    
    HttpResponse_t response = http_client.post(endpoint, data, headers);
    
    http_print_response_summary(&response);
    
    return response.success ? SYS_OK : SYS_ERROR;
}

// Commande PUT HTTP
SysError_t cmd_http_put(int argc, char* argv[]) {
    if (argc < 3) {
        Serial.println("Usage: http_put <endpoint> <data> [headers]");
        Serial.println("Examples:");
        Serial.println("  http_put /api/users/123 '{\"name\": \"Jane\"}'");
        Serial.println("  http_put /api/config '{\"setting\": \"value\"}' \"Content-Type: application/json\"");
        return SYS_INVALID_PARAM;
    }
    
    const char* endpoint = argv[1];
    const char* data = argv[2];
    const char* headers = (argc >= 4) ? argv[3] : nullptr;
    
    Serial.printf("🌐 HTTP PUT %s\n", endpoint);
    Serial.printf("📤 Data: %s\n", data);
    
    HttpResponse_t response = http_client.put(endpoint, data, headers);
    
    http_print_response_summary(&response);
    
    return response.success ? SYS_OK : SYS_ERROR;
}

// Commande DELETE HTTP
SysError_t cmd_http_delete(int argc, char* argv[]) {
    if (argc < 2) {
        Serial.println("Usage: http_delete <endpoint> [headers]");
        Serial.println("Examples:");
        Serial.println("  http_delete /api/users/123");
        Serial.println("  http_delete /api/data/456 \"Authorization: Bearer token123\"");
        return SYS_INVALID_PARAM;
    }
    
    const char* endpoint = argv[1];
    const char* headers = (argc >= 3) ? argv[2] : nullptr;
    
    Serial.printf("🌐 HTTP DELETE %s\n", endpoint);
    
    HttpResponse_t response = http_client.delete_request(endpoint, headers);
    
    http_print_response_summary(&response);
    
    return response.success ? SYS_OK : SYS_ERROR;
}

// Commande PATCH HTTP
SysError_t cmd_http_patch(int argc, char* argv[]) {
    if (argc < 3) {
        Serial.println("Usage: http_patch <endpoint> <data> [headers]");
        Serial.println("Examples:");
        Serial.println("  http_patch /api/users/123 '{\"email\": \"new@example.com\"}'");
        Serial.println("  http_patch /api/settings '{\"theme\": \"dark\"}' \"Content-Type: application/json\"");
        return SYS_INVALID_PARAM;
    }
    
    const char* endpoint = argv[1];
    const char* data = argv[2];
    const char* headers = (argc >= 4) ? argv[3] : nullptr;
    
    Serial.printf("🌐 HTTP PATCH %s\n", endpoint);
    Serial.printf("📤 Data: %s\n", data);
    
    HttpResponse_t response = http_client.patch(endpoint, data, headers);
    
    http_print_response_summary(&response);
    
    return response.success ? SYS_OK : SYS_ERROR;
}

// Commande HEAD HTTP
SysError_t cmd_http_head(int argc, char* argv[]) {
    if (argc < 2) {
        Serial.println("Usage: http_head <endpoint> [headers]");
        Serial.println("Examples:");
        Serial.println("  http_head /api/status");
        Serial.println("  http_head /api/file.txt \"Accept: text/plain\"");
        return SYS_INVALID_PARAM;
    }
    
    const char* endpoint = argv[1];
    const char* headers = (argc >= 3) ? argv[2] : nullptr;
    
    Serial.printf("🌐 HTTP HEAD %s\n", endpoint);
    
    HttpResponse_t response = http_client.head(endpoint, headers);
    
    http_print_response_summary(&response);
    
    return response.success ? SYS_OK : SYS_ERROR;
}

// Commande OPTIONS HTTP
SysError_t cmd_http_options(int argc, char* argv[]) {
    if (argc < 2) {
        Serial.println("Usage: http_options <endpoint> [headers]");
        Serial.println("Examples:");
        Serial.println("  http_options /api/users");
        Serial.println("  http_options /api/data \"Origin: http://example.com\"");
        return SYS_INVALID_PARAM;
    }
    
    const char* endpoint = argv[1];
    const char* headers = (argc >= 3) ? argv[2] : nullptr;
    
    Serial.printf("🌐 HTTP OPTIONS %s\n", endpoint);
    
    HttpResponse_t response = http_client.options(endpoint, headers);
    
    http_print_response_summary(&response);
    
    return response.success ? SYS_OK : SYS_ERROR;
}

// Commandes de test HTTP
SysError_t cmd_http_test(int argc, char* argv[]) {
    if (argc < 2) {
        Serial.println("=== HTTP Client Tests ===");
        Serial.println("Usage:");
        Serial.println("  http_test connectivity              - Test basic connectivity");
        Serial.println("  http_test endpoint <path>           - Test specific endpoint");
        Serial.println("  http_test performance <path> <n>    - Performance test (n iterations)");
        Serial.println("  http_test all                       - Run all tests");
        Serial.println("");
        Serial.println("Examples:");
        Serial.println("  http_test connectivity");
        Serial.println("  http_test endpoint /api/health");
        Serial.println("  http_test performance /api/ping 10");
        Serial.println("========================");
        return SYS_OK;
    }
    
    if (strcmp(argv[1], "connectivity") == 0) {
        Serial.println("🔍 Testing HTTP connectivity...");
        
        HttpClientError_t result = http_client.test_connectivity();
        
        if (result == HTTP_CLIENT_OK) {
            Serial.println("✅ HTTP connectivity test PASSED");
        } else {
            Serial.printf("❌ HTTP connectivity test FAILED: %s\n", 
                         http_client_error_to_string(result));
        }
        
        return result == HTTP_CLIENT_OK ? SYS_OK : SYS_ERROR;
    }
    
    if (strcmp(argv[1], "endpoint") == 0) {
        if (argc < 3) {
            Serial.println("❌ Usage: http_test endpoint <path>");
            return SYS_INVALID_PARAM;
        }
        
        const char* endpoint = argv[2];
        Serial.printf("🔍 Testing endpoint %s...\n", endpoint);
        
        HttpClientError_t result = http_client.test_endpoint(endpoint);
        
        if (result == HTTP_CLIENT_OK) {
            Serial.printf("✅ Endpoint %s test PASSED\n", endpoint);
        } else {
            Serial.printf("❌ Endpoint %s test FAILED: %s\n", endpoint, 
                         http_client_error_to_string(result));
        }
        
        return result == HTTP_CLIENT_OK ? SYS_OK : SYS_ERROR;
    }
    
    if (strcmp(argv[1], "performance") == 0) {
        if (argc < 4) {
            Serial.println("❌ Usage: http_test performance <path> <iterations>");
            return SYS_INVALID_PARAM;
        }
        
        const char* endpoint = argv[2];
        uint8_t iterations = atoi(argv[3]);
        
        if (iterations == 0 || iterations > 100) {
            Serial.println("❌ Invalid iterations count (must be 1-100)");
            return SYS_INVALID_PARAM;
        }
        
        Serial.printf("🔍 Performance test: %s (%d iterations)...\n", endpoint, iterations);
        
        HttpClientError_t result = http_client.test_performance(endpoint, iterations);
        
        if (result == HTTP_CLIENT_OK) {
            Serial.printf("✅ Performance test PASSED\n");
        } else {
            Serial.printf("❌ Performance test FAILED: %s\n", 
                         http_client_error_to_string(result));
        }
        
        return result == HTTP_CLIENT_OK ? SYS_OK : SYS_ERROR;
    }
    
    if (strcmp(argv[1], "all") == 0) {
        Serial.println("🔍 Running all HTTP tests...");
        
        bool all_passed = true;
        
        // Test 1: Connectivity
        Serial.println("\n1. Testing connectivity...");
        HttpClientError_t result = http_client.test_connectivity();
        if (result == HTTP_CLIENT_OK) {
            Serial.println("   ✅ PASSED");
        } else {
            Serial.printf("   ❌ FAILED: %s\n", http_client_error_to_string(result));
            all_passed = false;
        }
        
        // Test 2: Basic endpoint
        Serial.println("\n2. Testing basic endpoint...");
        result = http_client.test_endpoint("/");
        if (result == HTTP_CLIENT_OK) {
            Serial.println("   ✅ PASSED");
        } else {
            Serial.printf("   ❌ FAILED: %s\n", http_client_error_to_string(result));
            all_passed = false;
        }
        
        // Test 3: Performance
        Serial.println("\n3. Testing performance...");
        result = http_client.test_performance("/", 5);
        if (result == HTTP_CLIENT_OK) {
            Serial.println("   ✅ PASSED");
        } else {
            Serial.printf("   ❌ FAILED: %s\n", http_client_error_to_string(result));
            all_passed = false;
        }
        
        Serial.println("\n=== Test Summary ===");
        if (all_passed) {
            Serial.println("✅ All tests PASSED");
        } else {
            Serial.println("❌ Some tests FAILED");
        }
        Serial.println("====================");
        
        return all_passed ? SYS_OK : SYS_ERROR;
    }
    
    Serial.printf("❌ Unknown test: %s\n", argv[1]);
    return SYS_INVALID_PARAM;
}

// Commande de statistiques HTTP
SysError_t cmd_http_stats(int argc, char* argv[]) {
    if (argc >= 2 && strcmp(argv[1], "reset") == 0) {
        HttpClientError_t result = http_client.reset_stats();
        if (result == HTTP_CLIENT_OK) {
            Serial.println("✅ HTTP statistics reset");
        } else {
            Serial.printf("❌ Failed to reset statistics: %s\n", 
                         http_client_error_to_string(result));
        }
        return result == HTTP_CLIENT_OK ? SYS_OK : SYS_ERROR;
    }
    
    http_client.print_stats();
    return SYS_OK;
}

// Commande de debug HTTP
SysError_t cmd_http_debug(int argc, char* argv[]) {
    Serial.println("=== HTTP Client Debug Information ===");
    
    // Configuration
    http_client.print_debug_info();
    
    Serial.println();
    
    // Statistiques
    http_client.print_stats();
    
    Serial.println();
    
    // État du système
    Serial.printf("WiFi Status: %s\n", 
                  (WiFi.status() == WL_CONNECTED) ? "CONNECTED" : "DISCONNECTED");
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("WiFi IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("WiFi RSSI: %d dBm\n", WiFi.RSSI());
    }
    
    Serial.printf("Free Heap: %u bytes\n", esp_get_free_heap_size());
    Serial.printf("System Uptime: %lu seconds\n", millis() / 1000);
    
    Serial.println("=====================================");
    
    return SYS_OK;
}
