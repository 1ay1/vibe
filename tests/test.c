/*
 * VIBE Parser Test Suite
 * Comprehensive tests for the VIBE configuration format parser
 */

#include "../vibe.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* Test counter */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Color output */
#define COLOR_GREEN "\033[0;32m"
#define COLOR_RED "\033[0;31m"
#define COLOR_YELLOW "\033[0;33m"
#define COLOR_BLUE "\033[0;34m"
#define COLOR_RESET "\033[0m"

#define TEST(name) \
    do { \
        tests_run++; \
        printf("  [%d] Testing: %s... ", tests_run, name); \
        fflush(stdout); \
    } while(0)

#define PASS() \
    do { \
        tests_passed++; \
        printf(COLOR_GREEN "PASS" COLOR_RESET "\n"); \
    } while(0)

#define FAIL(msg) \
    do { \
        tests_failed++; \
        printf(COLOR_RED "FAIL" COLOR_RESET " - %s\n", msg); \
    } while(0)

#define ASSERT(condition, msg) \
    do { \
        if (!(condition)) { \
            FAIL(msg); \
            return; \
        } \
    } while(0)

/* Test: Basic integer parsing */
void test_basic_integer() {
    TEST("Basic integer");
    
    VibeParser* parser = vibe_parser_new();
    VibeValue* config = vibe_parse_string(parser, "count 42");
    
    ASSERT(config != NULL, "Failed to parse");
    ASSERT(config->type == VIBE_TYPE_OBJECT, "Root should be object");
    
    int64_t count = vibe_get_int(config, "count");
    ASSERT(count == 42, "Expected count=42");
    
    vibe_value_free(config);
    vibe_parser_free(parser);
    PASS();
}

/* Test: Negative integers */
void test_negative_integer() {
    TEST("Negative integer");
    
    VibeParser* parser = vibe_parser_new();
    VibeValue* config = vibe_parse_string(parser, "temp -17");
    
    ASSERT(config != NULL, "Failed to parse");
    
    int64_t temp = vibe_get_int(config, "temp");
    ASSERT(temp == -17, "Expected temp=-17");
    
    vibe_value_free(config);
    vibe_parser_free(parser);
    PASS();
}

/* Test: Float parsing */
void test_float() {
    TEST("Float values");
    
    VibeParser* parser = vibe_parser_new();
    VibeValue* config = vibe_parse_string(parser, 
        "pi 3.14159\n"
        "negative -2.5\n"
        "zero 0.0"
    );
    
    ASSERT(config != NULL, "Failed to parse");
    
    double pi = vibe_get_float(config, "pi");
    ASSERT(pi > 3.14 && pi < 3.15, "Expected pi≈3.14159");
    
    double neg = vibe_get_float(config, "negative");
    ASSERT(neg == -2.5, "Expected negative=-2.5");
    
    vibe_value_free(config);
    vibe_parser_free(parser);
    PASS();
}

/* Test: Boolean values */
void test_boolean() {
    TEST("Boolean values");
    
    VibeParser* parser = vibe_parser_new();
    VibeValue* config = vibe_parse_string(parser, 
        "enabled true\n"
        "disabled false"
    );
    
    ASSERT(config != NULL, "Failed to parse");
    
    bool enabled = vibe_get_bool(config, "enabled");
    bool disabled = vibe_get_bool(config, "disabled");
    
    ASSERT(enabled == true, "Expected enabled=true");
    ASSERT(disabled == false, "Expected disabled=false");
    
    vibe_value_free(config);
    vibe_parser_free(parser);
    PASS();
}

/* Test: String parsing */
void test_strings() {
    TEST("String values");
    
    VibeParser* parser = vibe_parser_new();
    VibeValue* config = vibe_parse_string(parser, 
        "unquoted localhost\n"
        "quoted \"Hello World\"\n"
        "path /usr/local/bin"
    );
    
    ASSERT(config != NULL, "Failed to parse");
    
    const char* unquoted = vibe_get_string(config, "unquoted");
    const char* quoted = vibe_get_string(config, "quoted");
    const char* path = vibe_get_string(config, "path");
    
    ASSERT(strcmp(unquoted, "localhost") == 0, "Expected unquoted=localhost");
    ASSERT(strcmp(quoted, "Hello World") == 0, "Expected quoted='Hello World'");
    ASSERT(strcmp(path, "/usr/local/bin") == 0, "Expected path=/usr/local/bin");
    
    vibe_value_free(config);
    vibe_parser_free(parser);
    PASS();
}

/* Test: String escaping */
void test_string_escape() {
    TEST("String escape sequences");
    
    VibeParser* parser = vibe_parser_new();
    VibeValue* config = vibe_parse_string(parser, 
        "quote \"She said \\\"Hello\\\"\"\n"
        "newline \"Line1\\nLine2\"\n"
        "tab \"Col1\\tCol2\"\n"
        "backslash \"Path\\\\File\""
    );
    
    ASSERT(config != NULL, "Failed to parse");
    
    const char* quote = vibe_get_string(config, "quote");
    const char* newline = vibe_get_string(config, "newline");
    const char* tab = vibe_get_string(config, "tab");
    const char* backslash = vibe_get_string(config, "backslash");
    
    ASSERT(strcmp(quote, "She said \"Hello\"") == 0, "Quote escape failed");
    ASSERT(strcmp(newline, "Line1\nLine2") == 0, "Newline escape failed");
    ASSERT(strcmp(tab, "Col1\tCol2") == 0, "Tab escape failed");
    ASSERT(strcmp(backslash, "Path\\File") == 0, "Backslash escape failed");
    
    vibe_value_free(config);
    vibe_parser_free(parser);
    PASS();
}

/* Test: Simple arrays */
void test_arrays() {
    TEST("Array parsing");
    
    VibeParser* parser = vibe_parser_new();
    VibeValue* config = vibe_parse_string(parser, 
        "numbers [1 2 3 4 5]\n"
        "strings [apple orange banana]\n"
        "mixed [42 \"hello\" true 3.14]"
    );
    
    ASSERT(config != NULL, "Failed to parse");
    
    VibeArray* numbers = vibe_get_array(config, "numbers");
    ASSERT(numbers != NULL, "Numbers array not found");
    ASSERT(numbers->count == 5, "Expected 5 numbers");
    ASSERT(numbers->values[0]->as_integer == 1, "First number should be 1");
    ASSERT(numbers->values[4]->as_integer == 5, "Last number should be 5");
    
    VibeArray* strings = vibe_get_array(config, "strings");
    ASSERT(strings != NULL, "Strings array not found");
    ASSERT(strings->count == 3, "Expected 3 strings");
    ASSERT(strcmp(strings->values[0]->as_string, "apple") == 0, "First string=apple");
    
    VibeArray* mixed = vibe_get_array(config, "mixed");
    ASSERT(mixed != NULL, "Mixed array not found");
    ASSERT(mixed->count == 4, "Expected 4 mixed values");
    ASSERT(mixed->values[0]->type == VIBE_TYPE_INTEGER, "First should be integer");
    ASSERT(mixed->values[1]->type == VIBE_TYPE_STRING, "Second should be string");
    ASSERT(mixed->values[2]->type == VIBE_TYPE_BOOLEAN, "Third should be boolean");
    ASSERT(mixed->values[3]->type == VIBE_TYPE_FLOAT, "Fourth should be float");
    
    vibe_value_free(config);
    vibe_parser_free(parser);
    PASS();
}

/* Test: Multiline arrays */
void test_multiline_arrays() {
    TEST("Multiline arrays");
    
    VibeParser* parser = vibe_parser_new();
    VibeValue* config = vibe_parse_string(parser, 
        "servers [\n"
        "  server1.com\n"
        "  server2.com\n"
        "  server3.com\n"
        "]"
    );
    
    ASSERT(config != NULL, "Failed to parse");
    
    VibeArray* servers = vibe_get_array(config, "servers");
    ASSERT(servers != NULL, "Servers array not found");
    ASSERT(servers->count == 3, "Expected 3 servers");
    
    vibe_value_free(config);
    vibe_parser_free(parser);
    PASS();
}

/* Test: Empty arrays */
void test_empty_arrays() {
    TEST("Empty arrays");
    
    VibeParser* parser = vibe_parser_new();
    VibeValue* config = vibe_parse_string(parser, "empty []");
    
    ASSERT(config != NULL, "Failed to parse");
    
    VibeArray* empty = vibe_get_array(config, "empty");
    ASSERT(empty != NULL, "Empty array not found");
    ASSERT(empty->count == 0, "Expected count=0");
    
    vibe_value_free(config);
    vibe_parser_free(parser);
    PASS();
}

/* Test: Nested objects */
void test_nested_objects() {
    TEST("Nested objects");
    
    VibeParser* parser = vibe_parser_new();
    VibeValue* config = vibe_parse_string(parser, 
        "server {\n"
        "  host localhost\n"
        "  port 8080\n"
        "  ssl {\n"
        "    enabled true\n"
        "    cert /etc/ssl/cert.pem\n"
        "  }\n"
        "}"
    );
    
    ASSERT(config != NULL, "Failed to parse");
    
    const char* host = vibe_get_string(config, "server.host");
    int64_t port = vibe_get_int(config, "server.port");
    bool ssl = vibe_get_bool(config, "server.ssl.enabled");
    const char* cert = vibe_get_string(config, "server.ssl.cert");
    
    ASSERT(strcmp(host, "localhost") == 0, "host=localhost");
    ASSERT(port == 8080, "port=8080");
    ASSERT(ssl == true, "ssl.enabled=true");
    ASSERT(strcmp(cert, "/etc/ssl/cert.pem") == 0, "cert path correct");
    
    vibe_value_free(config);
    vibe_parser_free(parser);
    PASS();
}

/* Test: Deep nesting */
void test_deep_nesting() {
    TEST("Deep nesting (5 levels)");
    
    VibeParser* parser = vibe_parser_new();
    VibeValue* config = vibe_parse_string(parser, 
        "level1 {\n"
        "  level2 {\n"
        "    level3 {\n"
        "      level4 {\n"
        "        level5 {\n"
        "          deep_value 42\n"
        "        }\n"
        "      }\n"
        "    }\n"
        "  }\n"
        "}"
    );
    
    ASSERT(config != NULL, "Failed to parse");
    
    int64_t val = vibe_get_int(config, "level1.level2.level3.level4.level5.deep_value");
    ASSERT(val == 42, "Deep value should be 42");
    
    vibe_value_free(config);
    vibe_parser_free(parser);
    PASS();
}

/* Test: Empty objects */
void test_empty_objects() {
    TEST("Empty objects");
    
    VibeParser* parser = vibe_parser_new();
    VibeValue* config = vibe_parse_string(parser, "empty {}");
    
    ASSERT(config != NULL, "Failed to parse");
    
    VibeObject* empty = vibe_get_object(config, "empty");
    ASSERT(empty != NULL, "Empty object not found");
    ASSERT(empty->count == 0, "Expected count=0");
    
    vibe_value_free(config);
    vibe_parser_free(parser);
    PASS();
}

/* Test: Comments */
void test_comments() {
    TEST("Comments");
    
    VibeParser* parser = vibe_parser_new();
    VibeValue* config = vibe_parse_string(parser, 
        "# This is a comment\n"
        "port 8080  # Inline comment\n"
        "# Another comment\n"
        "host localhost"
    );
    
    ASSERT(config != NULL, "Failed to parse");
    
    int64_t port = vibe_get_int(config, "port");
    const char* host = vibe_get_string(config, "host");
    
    ASSERT(port == 8080, "port=8080");
    ASSERT(strcmp(host, "localhost") == 0, "host=localhost");
    
    vibe_value_free(config);
    vibe_parser_free(parser);
    PASS();
}

/* Test: Complex real-world config */
void test_complex_config() {
    TEST("Complex real-world config");
    
    VibeParser* parser = vibe_parser_new();
    VibeValue* config = vibe_parse_string(parser, 
        "# Application config\n"
        "app {\n"
        "  name \"Production API\"\n"
        "  version 2.1.0\n"
        "  debug false\n"
        "  \n"
        "  features {\n"
        "    auth true\n"
        "    analytics true\n"
        "    beta false\n"
        "  }\n"
        "}\n"
        "\n"
        "database {\n"
        "  host db.internal\n"
        "  port 5432\n"
        "  replicas [\n"
        "    db1.internal\n"
        "    db2.internal\n"
        "    db3.internal\n"
        "  ]\n"
        "  \n"
        "  pool {\n"
        "    min 10\n"
        "    max 100\n"
        "    timeout 30.5\n"
        "  }\n"
        "}\n"
        "\n"
        "ports [8080 8081 8082]\n"
    );
    
    ASSERT(config != NULL, "Failed to parse");
    
    // Test app config
    const char* name = vibe_get_string(config, "app.name");
    bool debug = vibe_get_bool(config, "app.debug");
    bool auth = vibe_get_bool(config, "app.features.auth");
    
    ASSERT(strcmp(name, "Production API") == 0, "app.name");
    ASSERT(debug == false, "app.debug");
    ASSERT(auth == true, "app.features.auth");
    
    // Test database config
    const char* db_host = vibe_get_string(config, "database.host");
    int64_t db_port = vibe_get_int(config, "database.port");
    int64_t pool_min = vibe_get_int(config, "database.pool.min");
    double timeout = vibe_get_float(config, "database.pool.timeout");
    
    ASSERT(strcmp(db_host, "db.internal") == 0, "database.host");
    ASSERT(db_port == 5432, "database.port");
    ASSERT(pool_min == 10, "database.pool.min");
    ASSERT(timeout == 30.5, "database.pool.timeout");
    
    // Test arrays
    VibeArray* replicas = vibe_get_array(config, "database.replicas");
    VibeArray* ports = vibe_get_array(config, "ports");
    
    ASSERT(replicas != NULL && replicas->count == 3, "replicas count");
    ASSERT(ports != NULL && ports->count == 3, "ports count");
    
    vibe_value_free(config);
    vibe_parser_free(parser);
    PASS();
}

/* Test: Error - Unclosed brace */
void test_error_unclosed_brace() {
    TEST("Error detection: unclosed brace");
    
    VibeParser* parser = vibe_parser_new();
    VibeValue* config = vibe_parse_string(parser, 
        "server {\n"
        "  port 8080\n"
        // Missing closing brace
    );
    
    // An unclosed object must be rejected, not silently accepted (First Law of
    // conformance: a parser MUST NOT silently accept malformed input).
    ASSERT(config == NULL, "Unclosed object should be rejected");
    
    VibeError error = vibe_get_last_error(parser);
    ASSERT(error.has_error == true, "Should report an error");
    
    vibe_value_free(config);
    vibe_parser_free(parser);
    PASS();
}

/* Test: Error - Unclosed quote */
void test_error_unclosed_quote() {
    TEST("Error detection: unclosed quote");
    
    VibeParser* parser = vibe_parser_new();
    VibeValue* config = vibe_parse_string(parser, 
        "name \"Unclosed string\n"
        "port 8080"
    );
    
    ASSERT(config == NULL, "Should fail to parse");
    
    VibeError error = vibe_get_last_error(parser);
    ASSERT(error.has_error == true, "Should have error");
    
    vibe_parser_free(parser);
    PASS();
}

/* Test: Unicode in quoted strings */
void test_unicode_strings() {
    TEST("Unicode in quoted strings");
    
    VibeParser* parser = vibe_parser_new();
    VibeValue* config = vibe_parse_string(parser, 
        "chinese \"你好世界\"\n"
        "emoji \"Hello 🌊 World\"\n"
        "russian \"Привет\""
    );
    
    ASSERT(config != NULL, "Failed to parse unicode");
    
    const char* chinese = vibe_get_string(config, "chinese");
    const char* emoji = vibe_get_string(config, "emoji");
    const char* russian = vibe_get_string(config, "russian");
    
    ASSERT(chinese != NULL, "Chinese string should exist");
    ASSERT(emoji != NULL, "Emoji string should exist");
    ASSERT(russian != NULL, "Russian string should exist");
    
    vibe_value_free(config);
    vibe_parser_free(parser);
    PASS();
}

/* Test: Paths and URLs */
void test_paths_urls() {
    TEST("Paths and URLs as unquoted strings");
    
    VibeParser* parser = vibe_parser_new();
    VibeValue* config = vibe_parse_string(parser, 
        "path1 /usr/local/bin\n"
        "path2 /etc/ssl/cert.pem\n"
        "path3 ./relative/path\n"
        "path4 ../parent/dir\n"
        "url http://example.com:8080\n"
        "ip \"192.168.1.1\""  // IP addresses need quotes (dots are parsed separately)
    );
    
    ASSERT(config != NULL, "Failed to parse");
    
    const char* path1 = vibe_get_string(config, "path1");
    ASSERT(path1 != NULL, "path1 not null");
    ASSERT(strcmp(path1, "/usr/local/bin") == 0, "path1 correct");
    
    const char* url = vibe_get_string(config, "url");
    ASSERT(url != NULL, "url not null");
    ASSERT(strcmp(url, "http://example.com:8080") == 0, "url correct");
    
    const char* ip = vibe_get_string(config, "ip");
    ASSERT(ip != NULL, "ip not null");
    ASSERT(strcmp(ip, "192.168.1.1") == 0, "ip correct");
    
    vibe_value_free(config);
    vibe_parser_free(parser);
    PASS();
}

/* Test: No reserved words */
void test_no_reserved_words() {
    TEST("No reserved words");
    
    VibeParser* parser = vibe_parser_new();
    VibeValue* config = vibe_parse_string(parser, 
        "null \"not null\"\n"
        "if conditional\n"
        "for loop\n"
        "class object\n"
        "return success\n"
        "while continuing"
    );
    
    ASSERT(config != NULL, "Failed to parse");
    
    // These are all valid key names (no reserved words in VIBE!)
    const char* null_val = vibe_get_string(config, "null");
    const char* if_val = vibe_get_string(config, "if");
    const char* for_val = vibe_get_string(config, "for");
    const char* class_val = vibe_get_string(config, "class");
    
    ASSERT(strcmp(null_val, "not null") == 0, "null is a valid key");
    ASSERT(strcmp(if_val, "conditional") == 0, "if is a valid key");
    ASSERT(strcmp(for_val, "loop") == 0, "for is a valid key");
    ASSERT(strcmp(class_val, "object") == 0, "class is a valid key");
    
    vibe_value_free(config);
    vibe_parser_free(parser);
    PASS();
}

/* Test: simple.vibe example file */
void test_simple_example() {
    TEST("simple.vibe example");
    
    VibeParser* parser = vibe_parser_new();
    ASSERT(parser != NULL, "Failed to create parser");
    
    VibeValue* config = vibe_parse_file(parser, "examples/simple.vibe");
    ASSERT(config != NULL, "Failed to parse simple.vibe");
    ASSERT(config->type == VIBE_TYPE_OBJECT, "Root should be object");
    
    /* Test basic values */
    const char* name = vibe_get_string(config, "name");
    ASSERT(name && strcmp(name, "My Application") == 0, "Wrong application name");
    
    double version = vibe_get_float(config, "version");
    ASSERT(version == 1.0, "Wrong version");
    
    bool debug = vibe_get_bool(config, "debug");
    ASSERT(debug == true, "Wrong debug flag");
    
    int64_t port = vibe_get_int(config, "port");
    ASSERT(port == 8080, "Wrong port");
    
    /* Test database object */
    const char* db_host = vibe_get_string(config, "database.host");
    ASSERT(db_host && strcmp(db_host, "localhost") == 0, "Wrong database host");
    
    int64_t db_port = vibe_get_int(config, "database.port");
    ASSERT(db_port == 5432, "Wrong database port");
    
    /* Test arrays */
    VibeArray* allowed_ips = vibe_get_array(config, "allowed_ips");
    ASSERT(allowed_ips != NULL, "Failed to get allowed_ips array");
    ASSERT(allowed_ips->count == 3, "Wrong allowed_ips count");
    
    VibeArray* features = vibe_get_array(config, "features");
    ASSERT(features != NULL, "Failed to get features array");
    ASSERT(features->count == 4, "Wrong features count");
    
    /* Test nested objects */
    bool ssl_enabled = vibe_get_bool(config, "server.ssl.enabled");
    ASSERT(ssl_enabled == false, "Wrong SSL enabled flag");
    
    const char* log_level = vibe_get_string(config, "logging.level");
    ASSERT(log_level && strcmp(log_level, "info") == 0, "Wrong log level");
    
    vibe_value_free(config);
    vibe_parser_free(parser);
    PASS();
}

/* Test: config.vibe example file */
void test_config_example() {
    TEST("config.vibe example");
    
    VibeParser* parser = vibe_parser_new();
    ASSERT(parser != NULL, "Failed to create parser");
    
    VibeValue* config = vibe_parse_file(parser, "examples/config.vibe");
    ASSERT(config != NULL, "Failed to parse config.vibe");
    ASSERT(config->type == VIBE_TYPE_OBJECT, "Root should be object");
    
    /* Test application section */
    const char* app_name = vibe_get_string(config, "application.name");
    ASSERT(app_name && strcmp(app_name, "Enterprise Web Application") == 0, "Wrong app name");
    
    const char* app_version = vibe_get_string(config, "application.version");
    ASSERT(app_version && strcmp(app_version, "2.3.1") == 0, "Wrong app version");
    
    int64_t build_number = vibe_get_int(config, "application.build_number");
    ASSERT(build_number == 1247, "Wrong build number");
    
    /* Test feature flags */
    bool new_ui = vibe_get_bool(config, "application.features.new_ui");
    ASSERT(new_ui == true, "Wrong new_ui flag");
    
    bool beta_api = vibe_get_bool(config, "application.features.beta_api");
    ASSERT(beta_api == false, "Wrong beta_api flag");
    
    /* Test database configuration */
    const char* primary_host = vibe_get_string(config, "database.primary.host");
    ASSERT(primary_host && strcmp(primary_host, "db-primary.example.com") == 0, "Wrong primary DB host");
    
    int64_t primary_port = vibe_get_int(config, "database.primary.port");
    ASSERT(primary_port == 5432, "Wrong primary DB port");
    
    /* Test connection pool */
    int64_t min_conn = vibe_get_int(config, "database.primary.pool.min_connections");
    ASSERT(min_conn == 5, "Wrong min connections");
    
    int64_t max_conn = vibe_get_int(config, "database.primary.pool.max_connections");
    ASSERT(max_conn == 20, "Wrong max connections");
    
    /* Test SSL configuration */
    bool ssl_enabled = vibe_get_bool(config, "database.primary.ssl.enabled");
    ASSERT(ssl_enabled == true, "Wrong DB SSL enabled flag");
    
    const char* ssl_mode = vibe_get_string(config, "database.primary.ssl.mode");
    ASSERT(ssl_mode && strcmp(ssl_mode, "require") == 0, "Wrong SSL mode");
    
    /* Test read replicas (now named objects) */
    VibeObject* replicas = vibe_get_object(config, "database.replicas");
    ASSERT(replicas != NULL, "Failed to get replicas object");
    ASSERT(replicas->count == 3, "Should have 3 replicas");
    
    /* Test server configuration */
    const char* bind_addr = vibe_get_string(config, "server.bind_address");
    ASSERT(bind_addr && strcmp(bind_addr, "0.0.0.0") == 0, "Wrong bind address");
    
    int64_t server_port = vibe_get_int(config, "server.port");
    ASSERT(server_port == 8080, "Wrong server port");
    
    /* Test API configuration */
    const char* api_version = vibe_get_string(config, "api.version");
    ASSERT(api_version && strcmp(api_version, "v2") == 0, "Wrong API version");
    
    bool rate_limit_enabled = vibe_get_bool(config, "api.rate_limit.enabled");
    ASSERT(rate_limit_enabled == true, "Wrong rate limit enabled flag");
    
    /* Test monitoring */
    bool health_enabled = vibe_get_bool(config, "monitoring.health.enabled");
    ASSERT(health_enabled == true, "Wrong health enabled flag");
    
    const char* health_path = vibe_get_string(config, "monitoring.health.path");
    ASSERT(health_path && strcmp(health_path, "/health") == 0, "Wrong health path");
    
    vibe_value_free(config);
    vibe_parser_free(parser);
    PASS();
}

/* Test: web_server.vibe example file */
void test_web_server_example() {
    TEST("web_server.vibe example");
    
    VibeParser* parser = vibe_parser_new();
    ASSERT(parser != NULL, "Failed to create parser");
    
    VibeValue* config = vibe_parse_file(parser, "examples/web_server.vibe");
    ASSERT(config != NULL, "Failed to parse web_server.vibe");
    ASSERT(config->type == VIBE_TYPE_OBJECT, "Root should be object");
    
    /* Test server configuration */
    const char* server_name = vibe_get_string(config, "server.name");
    ASSERT(server_name && strcmp(server_name, "Production Web Server") == 0, "Wrong server name");
    
    const char* bind_addr = vibe_get_string(config, "server.bind_address");
    ASSERT(bind_addr && strcmp(bind_addr, "0.0.0.0") == 0, "Wrong bind address");
    
    int64_t port = vibe_get_int(config, "server.port");
    ASSERT(port == 8080, "Wrong server port");
    
    int64_t worker_processes = vibe_get_int(config, "server.worker_processes");
    ASSERT(worker_processes == 4, "Wrong worker processes");
    
    /* Test SSL configuration */
    bool ssl_enabled = vibe_get_bool(config, "server.ssl.enabled");
    ASSERT(ssl_enabled == true, "Wrong SSL enabled flag");
    
    int64_t ssl_port = vibe_get_int(config, "server.ssl.port");
    ASSERT(ssl_port == 8443, "Wrong SSL port");
    
    VibeArray* protocols = vibe_get_array(config, "server.ssl.protocols");
    ASSERT(protocols != NULL, "Failed to get protocols array");
    ASSERT(protocols->count == 2, "Wrong protocols count");
    
    /* Test vhosts (now named objects) */
    VibeObject* vhosts = vibe_get_object(config, "vhosts");
    ASSERT(vhosts != NULL, "Failed to get vhosts object");
    ASSERT(vhosts->count == 2, "Should have 2 vhosts");
    
    /* Test logging configuration */
    const char* log_level = vibe_get_string(config, "logging.level");
    ASSERT(log_level && strcmp(log_level, "info") == 0, "Wrong log level");
    
    const char* log_format = vibe_get_string(config, "logging.format");
    ASSERT(log_format && strcmp(log_format, "json") == 0, "Wrong log format");
    
    /* Test rate limiting */
    bool rate_limit_enabled = vibe_get_bool(config, "rate_limit.enabled");
    ASSERT(rate_limit_enabled == true, "Wrong rate limit enabled flag");
    
    VibeObject* rate_rules = vibe_get_object(config, "rate_limit.rules");
    ASSERT(rate_rules != NULL, "Failed to get rate limit rules");
    ASSERT(rate_rules->count == 2, "Should have 2 rate limit rules");
    
    /* Test CORS */
    bool cors_enabled = vibe_get_bool(config, "cors.enabled");
    ASSERT(cors_enabled == true, "Wrong CORS enabled flag");
    
    VibeArray* allowed_origins = vibe_get_array(config, "cors.allowed_origins");
    ASSERT(allowed_origins != NULL, "Failed to get allowed origins");
    ASSERT(allowed_origins->count == 2, "Wrong allowed origins count");
    
    /* Test compression */
    bool compression_enabled = vibe_get_bool(config, "compression.enabled");
    ASSERT(compression_enabled == true, "Wrong compression enabled flag");
    
    int64_t compression_level = vibe_get_int(config, "compression.level");
    ASSERT(compression_level == 6, "Wrong compression level");
    
    vibe_value_free(config);
    vibe_parser_free(parser);
    PASS();
}

/* Test: database.vibe example file */
void test_database_example() {
    TEST("database.vibe example");
    
    VibeParser* parser = vibe_parser_new();
    ASSERT(parser != NULL, "Failed to create parser");
    
    VibeValue* config = vibe_parse_file(parser, "examples/database.vibe");
    ASSERT(config != NULL, "Failed to parse database.vibe");
    ASSERT(config->type == VIBE_TYPE_OBJECT, "Root should be object");
    
    /* Test basic database info */
    const char* db_name = vibe_get_string(config, "primary.database");
    ASSERT(db_name != NULL, "Database name should exist");
    
    /* Test if we can access some configuration without knowing exact structure */
    VibeValue* primary = vibe_get(config, "primary");
    ASSERT(primary != NULL, "Should have primary object");
    ASSERT(primary->type == VIBE_TYPE_OBJECT, "Primary should be object type");
    
    vibe_value_free(config);
    vibe_parser_free(parser);
    PASS();
}

/* Test: All example files can be parsed without errors */
void test_all_examples_parse() {
    TEST("all examples parse successfully");
    
    const char* example_files[] = {
        "examples/simple.vibe",
        "examples/config.vibe", 
        "examples/web_server.vibe",
        "examples/database.vibe"
    };
    
    int num_files = sizeof(example_files) / sizeof(example_files[0]);
    
    for (int i = 0; i < num_files; i++) {
        VibeParser* parser = vibe_parser_new();
        ASSERT(parser != NULL, "Failed to create parser");
        
        VibeValue* config = vibe_parse_file(parser, example_files[i]);
        if (!config) {
            VibeError error = vibe_get_last_error(parser);
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), 
                    "Failed to parse %s: %s", 
                    example_files[i], 
                    error.message ? error.message : "unknown error");
            FAIL(error_msg);
            vibe_parser_free(parser);
            return;
        }
        
        ASSERT(config->type == VIBE_TYPE_OBJECT, "Root should be object");
        
        vibe_value_free(config);
        vibe_parser_free(parser);
    }
    
    PASS();
}

/* Test: Cross-example consistency */
void test_examples_consistency() {
    TEST("examples consistency checks");
    
    /* Parse simple.vibe and config.vibe */
    VibeParser* parser1 = vibe_parser_new();
    VibeParser* parser2 = vibe_parser_new();
    
    VibeValue* simple = vibe_parse_file(parser1, "examples/simple.vibe");
    VibeValue* config = vibe_parse_file(parser2, "examples/config.vibe");
    
    ASSERT(simple != NULL, "Failed to parse simple.vibe");
    ASSERT(config != NULL, "Failed to parse config.vibe");
    
    /* Both should have application names (even if different) */
    const char* simple_name = vibe_get_string(simple, "name");
    const char* config_name = vibe_get_string(config, "application.name");
    
    ASSERT(simple_name != NULL, "Simple should have name");
    ASSERT(config_name != NULL, "Config should have application.name");
    
    /* Both should have version information */
    VibeValue* simple_version = vibe_get(simple, "version");
    VibeValue* config_version = vibe_get(config, "application.version");
    
    ASSERT(simple_version != NULL, "Simple should have version");
    ASSERT(config_version != NULL, "Config should have application.version");
    
    /* Both should have some kind of database configuration */
    VibeValue* simple_db = vibe_get(simple, "database");
    VibeValue* config_db = vibe_get(config, "database");
    
    ASSERT(simple_db != NULL, "Simple should have database config");
    ASSERT(config_db != NULL, "Config should have database config");
    
    vibe_value_free(simple);
    vibe_value_free(config);
    vibe_parser_free(parser1);
    vibe_parser_free(parser2);
    PASS();
}

/* Run all tests */
/* ============================================================================
 * Security & robustness — adversarial inputs, limits, resource exhaustion.
 * These exercise the hardened allocation paths and every fail-closed guard.
 * ============================================================================ */

void test_sec_deep_nesting_rejected() {
    TEST("Security: nesting past max_depth fails closed");
    /* Build many opening braces "k{k{k{...". Must be refused, not crash. */
    static char src[10008];
    size_t p = 0;
    for (size_t i = 0; i < 5000; i++) { src[p++] = 'k'; src[p++] = '{'; }
    src[p] = '\0';
    VibeError err; memset(&err, 0, sizeof(err));
    VibeValue* v = vibe_parse(src, p, &err);
    ASSERT(v == NULL, "deep nesting must be rejected");
    ASSERT(err.code == VIBE_ERROR_LIMIT_EXCEEDED, "expected limit-exceeded");
    vibe_error_free(&err);
    PASS();
}

void test_sec_nested_array_rejected() {
    TEST("Security: First Law — array inside array rejected");
    VibeError err; memset(&err, 0, sizeof(err));
    VibeValue* v = vibe_parse("a [ [ 1 ] ]", 11, &err);
    ASSERT(v == NULL, "nested array must be rejected");
    ASSERT(err.code == VIBE_ERROR_NESTED_CONTAINER, "expected nested-container");
    vibe_error_free(&err);
    PASS();
}

void test_sec_embedded_nul_rejected() {
    TEST("Security: embedded NUL byte rejected");
    const char buf[] = { 'k', ' ', '1', '\0', 'x', ' ', '2', '\n' };
    VibeError err; memset(&err, 0, sizeof(err));
    VibeValue* v = vibe_parse(buf, sizeof(buf), &err);
    ASSERT(v == NULL, "embedded NUL must be rejected");
    ASSERT(err.code == VIBE_ERROR_ILLEGAL_CHARACTER, "expected illegal-character");
    vibe_error_free(&err);
    PASS();
}

void test_sec_bad_utf8_rejected() {
    TEST("Security: ill-formed UTF-8 rejected");
    /* Lone continuation byte, overlong, surrogate, truncated, > U+10FFFF. */
    const char* bad[] = {
        "k \"\x80\"\n",             /* lone continuation */
        "k \"\xC0\xAF\"\n",         /* overlong '/' */
        "k \"\xED\xA0\x80\"\n",     /* surrogate U+D800 */
        "k \"\xE2\x82\"\n",         /* truncated 3-byte */
        "k \"\xF5\x80\x80\x80\"\n", /* > U+10FFFF */
    };
    for (size_t i = 0; i < sizeof(bad)/sizeof(bad[0]); i++) {
        VibeError err; memset(&err, 0, sizeof(err));
        VibeValue* v = vibe_parse(bad[i], strlen(bad[i]), &err);
        ASSERT(v == NULL, "ill-formed UTF-8 must be rejected");
        ASSERT(err.code == VIBE_ERROR_ENCODING || err.code == VIBE_ERROR_ILLEGAL_CHARACTER,
               "expected an encoding/illegal error");
        vibe_error_free(&err);
    }
    PASS();
}

void test_sec_integer_out_of_range() {
    TEST("Security: out-of-range integer fails, no silent wrap");
    VibeError err; memset(&err, 0, sizeof(err));
    VibeValue* v = vibe_parse("k 99999999999999999999999999\n", 29, &err);
    ASSERT(v == NULL, "overflowing integer must be rejected");
    vibe_error_free(&err);
    /* But INT64_MAX itself must parse. */
    VibeValue* ok = vibe_parse("k 9223372036854775807\n", 22, NULL);
    ASSERT(ok != NULL, "INT64_MAX must parse");
    ASSERT(vibe_get_int(ok, "k") == 9223372036854775807LL, "value preserved");
    vibe_value_free(ok);
    PASS();
}

void test_sec_custom_limits() {
    TEST("Security: custom limits enforced (keys, array, string, key length)");
    VibeParser* p = vibe_parser_new();
    ASSERT(p, "parser");
    VibeLimits lim = vibe_default_limits();
    lim.max_object_keys = 2;
    lim.max_array_elements = 3;
    lim.max_key_length = 4;
    vibe_parser_set_limits(p, &lim);

    VibeValue* v = vibe_parse_string(p, "a 1\nb 2\nc 3\n");
    ASSERT(v == NULL && vibe_get_last_error(p).code == VIBE_ERROR_LIMIT_EXCEEDED,
           "too many keys must be rejected");

    v = vibe_parse_string(p, "a [ 1 2 3 4 ]\n");
    ASSERT(v == NULL && vibe_get_last_error(p).code == VIBE_ERROR_LIMIT_EXCEEDED,
           "too many array elements must be rejected");

    v = vibe_parse_string(p, "toolongkey 1\n");
    ASSERT(v == NULL && vibe_get_last_error(p).code == VIBE_ERROR_LIMIT_EXCEEDED,
           "over-long key must be rejected");

    vibe_parser_free(p);
    PASS();
}

void test_sec_first_law_api_guard() {
    TEST("Security: API refuses container elements in arrays");
    VibeValue* arr = vibe_value_new_array();
    ASSERT(arr, "array");
    /* push now returns bool: false on First-Law rejection, value still freed. */
    ASSERT(!vibe_array_push(arr->as_array, vibe_value_new_object()), "object push refused");
    ASSERT(!vibe_array_push(arr->as_array, vibe_value_new_array()), "array push refused");
    ASSERT(vibe_array_size(arr->as_array) == 0, "containers must not enter array");
    ASSERT(vibe_array_push(arr->as_array, vibe_value_new_integer(42)), "scalar push ok");
    ASSERT(vibe_array_push_int(arr->as_array, 7), "scalar push_int ok");
    ASSERT(vibe_array_size(arr->as_array) == 2, "scalars accepted");
    vibe_value_free(arr);
    PASS();
}

void test_sec_parse_file_rejects_directory() {
    TEST("Security: vibe_parse_file rejects a directory");
    VibeParser* p = vibe_parser_new();
    ASSERT(p, "parser");
    /* "." is a directory; on POSIX fopen() would open it and a naive reader
     * would treat it as an empty valid document. It must fail instead. */
    VibeValue* v = vibe_parse_file(p, ".");
    ASSERT(v == NULL, "directory must not parse as a document");
    vibe_parser_free(p);
    PASS();
}

void test_sec_set_push_return_contract() {
    TEST("Security: set/push return true and take ownership");
    VibeValue* obj = vibe_value_new_object();
    ASSERT(obj, "obj");
    /* set returns true; replacing a key frees the old value (no leak under ASan). */
    ASSERT(vibe_object_set(obj->as_object, "k", vibe_value_new_integer(1)), "set 1");
    ASSERT(vibe_object_set(obj->as_object, "k", vibe_value_new_integer(2)), "set 2 (replace)");
    ASSERT(vibe_object_size(obj->as_object) == 1, "replace keeps one key");
    ASSERT(vibe_get_int(vibe_object_get(obj->as_object, "k"), NULL) == 2, "last-wins");
    /* NULL value: set consumes ownership (nothing to free) and returns false. */
    ASSERT(!vibe_object_set(obj->as_object, "x", NULL), "set NULL value false");
    ASSERT(!vibe_object_set(NULL, "x", vibe_value_new_integer(9)), "set NULL obj false (frees value)");
    VibeValue* arr = vibe_value_new_array();
    ASSERT(!vibe_array_push(NULL, vibe_value_new_integer(9)), "push NULL arr false (frees value)");
    ASSERT(vibe_array_push(arr->as_array, vibe_value_new_string("ok")), "push true");
    vibe_value_free(arr);
    vibe_value_free(obj);
    PASS();
}

void test_sec_null_safety() {
    TEST("Security: NULL arguments never crash");
    ASSERT(vibe_parse(NULL, 0, NULL) == NULL, "parse NULL");
    ASSERT(vibe_get(NULL, "a") == NULL, "get NULL root");
    ASSERT(vibe_get(NULL, NULL) == NULL, "get NULL both");
    ASSERT(vibe_emit(NULL) == NULL, "emit NULL");
    ASSERT(vibe_value_clone(NULL) == NULL, "clone NULL");
    ASSERT(vibe_object_get(NULL, "a") == NULL, "object_get NULL");
    ASSERT(vibe_array_get(NULL, 0) == NULL, "array_get NULL");
    ASSERT(vibe_object_size(NULL) == 0, "object_size NULL");
    ASSERT(vibe_array_size(NULL) == 0, "array_size NULL");
    ASSERT(!vibe_object_remove(NULL, "a"), "object_remove NULL");
    ASSERT(!vibe_array_remove(NULL, 0), "array_remove NULL");
    ASSERT(vibe_value_equals(NULL, NULL), "both NULL are equal (same pointer)");
    {
        VibeValue* one = vibe_value_new_integer(1);
        ASSERT(!vibe_value_equals(NULL, one), "equals lhs NULL");
        ASSERT(!vibe_value_equals(one, NULL), "equals rhs NULL");
        vibe_value_free(one);
    }
    vibe_value_free(NULL); /* must be a no-op */
    vibe_free(NULL);
    PASS();
}

void test_sec_roundtrip_idempotent() {
    TEST("Security: emit is idempotent (parse->emit->parse->emit stable)");
    const char* src =
        "name test\n"
        "port 8080\n"
        "ratio 1.5\n"
        "weird \"has space and \\\"quote\\\"\"\n"
        "tags [ a b c ]\n"
        "nested {\n  deep {\n    x 1\n  }\n}\n";
    VibeValue* v1 = vibe_parse(src, strlen(src), NULL);
    ASSERT(v1, "parse 1");
    char* e1 = vibe_emit(v1);
    ASSERT(e1, "emit 1");
    VibeValue* v2 = vibe_parse(e1, strlen(e1), NULL);
    ASSERT(v2, "parse 2");
    char* e2 = vibe_emit(v2);
    ASSERT(e2, "emit 2");
    ASSERT(strcmp(e1, e2) == 0, "emit must be idempotent");
    ASSERT(vibe_value_equals(v1, v2), "trees must be equal");
    vibe_free(e1); vibe_free(e2);
    vibe_value_free(v1); vibe_value_free(v2);
    PASS();
}

void test_sec_hash_index_stress() {
    TEST("Security: large flat object (hash index + reindex) is correct");
    VibeValue* obj = vibe_value_new_object();
    ASSERT(obj, "obj");
    char key[32];
    const size_t N = 20000;
    for (size_t i = 0; i < N; i++) {
        snprintf(key, sizeof(key), "k%zu", i);
        ASSERT(vibe_object_set_int(obj->as_object, key, (int64_t)i), "set");
    }
    ASSERT(vibe_object_size(obj->as_object) == N, "count");
    /* Random-ish probes must all resolve through the hash index. */
    for (size_t i = 0; i < N; i += 137) {
        snprintf(key, sizeof(key), "k%zu", i);
        VibeValue* got = vibe_object_get(obj->as_object, key);
        ASSERT(got && vibe_get_int(got, NULL) == (int64_t)i, "lookup");
    }
    /* Remove half, ensure the rest still resolve (index rebuild path). */
    for (size_t i = 0; i < N; i += 2) {
        snprintf(key, sizeof(key), "k%zu", i);
        ASSERT(vibe_object_remove(obj->as_object, key), "remove");
    }
    ASSERT(vibe_object_size(obj->as_object) == N / 2, "count after remove");
    snprintf(key, sizeof(key), "k%zu", (size_t)1);
    ASSERT(vibe_object_get(obj->as_object, key), "odd key survives");
    snprintf(key, sizeof(key), "k%zu", (size_t)0);
    ASSERT(!vibe_object_get(obj->as_object, key), "even key removed");
    vibe_value_free(obj);
    PASS();
}

void test_sec_fuzz_no_crash() {
    TEST("Security: pseudo-random fuzz never crashes the parser");
    /* Deterministic LCG so failures reproduce. Every input must either parse
     * or fail cleanly — never crash, leak (ASan/UBSan catch that), or hang. */
    uint32_t seed = 0x5eed1234u;
    const char* alphabet = "{}[]\"\\ \n\t#abc123.-_:/=\xC3\xA9\x80";
    size_t alen = strlen(alphabet);
    char buf[256];
    for (int iter = 0; iter < 20000; iter++) {
        seed = seed * 1103515245u + 12345u;
        size_t len = (seed >> 8) % sizeof(buf);
        for (size_t i = 0; i < len; i++) {
            seed = seed * 1103515245u + 12345u;
            buf[i] = alphabet[(seed >> 8) % alen];
        }
        VibeError err; memset(&err, 0, sizeof(err));
        VibeValue* v = vibe_parse(buf, len, &err);
        if (v) {
            char* e = vibe_emit(v);   /* emit must also survive any valid tree */
            if (e) vibe_free(e);
            vibe_value_free(v);
        } else {
            vibe_error_free(&err);
        }
    }
    PASS();
}

int main() {
    printf("\n");
    printf(COLOR_BLUE "╔══════════════════════════════════════════════════════════╗\n" COLOR_RESET);
    printf(COLOR_BLUE "║         VIBE Parser - Comprehensive Test Suite          ║\n" COLOR_RESET);
    printf(COLOR_BLUE "╚══════════════════════════════════════════════════════════╝\n" COLOR_RESET);
    printf("\n");
    
    printf("Running tests...\n\n");
    
    /* Basic type tests */
    printf(COLOR_YELLOW "Basic Types:\n" COLOR_RESET);
    test_basic_integer();
    test_negative_integer();
    test_float();
    test_boolean();
    test_strings();
    test_string_escape();
    
    /* Array tests */
    printf("\n" COLOR_YELLOW "Arrays:\n" COLOR_RESET);
    test_arrays();
    test_multiline_arrays();
    test_empty_arrays();
    
    /* Object tests */
    printf("\n" COLOR_YELLOW "Objects:\n" COLOR_RESET);
    test_nested_objects();
    test_deep_nesting();
    test_empty_objects();
    
    /* Feature tests */
    printf("\n" COLOR_YELLOW "Features:\n" COLOR_RESET);
    test_comments();
    test_unicode_strings();
    test_paths_urls();
    test_no_reserved_words();
    
    /* Complex tests */
    printf("\n" COLOR_YELLOW "Complex Scenarios:\n" COLOR_RESET);
    test_complex_config();
    
    /* Example file tests */
    printf("\n" COLOR_YELLOW "Example Files:\n" COLOR_RESET);
    test_simple_example();
    test_config_example();
    test_web_server_example();
    test_database_example();
    test_all_examples_parse();
    test_examples_consistency();
    
    /* Error tests */
    printf("\n" COLOR_YELLOW "Error Handling:\n" COLOR_RESET);
    test_error_unclosed_brace();
    test_error_unclosed_quote();

    /* Security & robustness */
    printf("\n" COLOR_YELLOW "Security & Robustness:\n" COLOR_RESET);
    test_sec_deep_nesting_rejected();
    test_sec_nested_array_rejected();
    test_sec_embedded_nul_rejected();
    test_sec_bad_utf8_rejected();
    test_sec_integer_out_of_range();
    test_sec_custom_limits();
    test_sec_first_law_api_guard();
    test_sec_parse_file_rejects_directory();
    test_sec_set_push_return_contract();
    test_sec_null_safety();
    test_sec_roundtrip_idempotent();
    test_sec_hash_index_stress();
    test_sec_fuzz_no_crash();
    
    /* Summary */
    printf("\n");
    printf(COLOR_BLUE "════════════════════════════════════════════════════════════\n" COLOR_RESET);
    printf("Test Summary:\n");
    printf("  Total:  %d\n", tests_run);
    printf("  " COLOR_GREEN "Passed: %d" COLOR_RESET "\n", tests_passed);
    if (tests_failed > 0) {
        printf("  " COLOR_RED "Failed: %d" COLOR_RESET "\n", tests_failed);
    } else {
        printf("  Failed: 0\n");
    }
    printf(COLOR_BLUE "════════════════════════════════════════════════════════════\n" COLOR_RESET);
    printf("\n");
    
    if (tests_failed == 0) {
        printf(COLOR_GREEN "✓ All tests passed!" COLOR_RESET " 🌊\n\n");
        return 0;
    } else {
        printf(COLOR_RED "✗ Some tests failed!" COLOR_RESET "\n\n");
        return 1;
    }
}
