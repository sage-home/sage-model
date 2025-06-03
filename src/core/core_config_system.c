#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include "core_config_system.h"
#include "core_module_system.h"
#include "core_pipeline_system.h"
#include "core_logging.h"

/**
 * @file core_config_system.c
 * @brief Implementation of the configuration system
 * 
 * This file implements a lightweight JSON parser and configuration system.
 */

/* Global configuration system instance */
struct config_system *global_config = NULL;

/* Forward declarations for internal functions */
static struct config_object *json_parse_object(const char **json);
static struct config_value json_parse_value(const char **json);
static void json_skip_whitespace(const char **json);
static char *json_parse_string(const char **json);
static void config_value_free(struct config_value *value);
static void config_object_free(struct config_object *obj);
static const struct config_value *config_get_value_internal(const struct config_object *obj, const char *path);
static int config_set_value_internal(struct config_object *obj, const char *path, struct config_value value);
static void json_serialize_value(const struct config_value *value, FILE *f, int indent_level, bool pretty);
static void json_serialize_object(const struct config_object *obj, FILE *f, int indent_level, bool pretty);

/**
 * Initialize the configuration system
 */
int config_system_initialize(void) {
    if (global_config != NULL) {
        LOG_WARNING("Configuration system already initialized");
        return 0;
    }
    
    global_config = calloc(1, sizeof(struct config_system));
    if (global_config == NULL) {
        LOG_ERROR("Failed to allocate memory for configuration system");
        return -1;
    }
    
    /* Create default configuration */
    global_config->root = config_generate_default();
    if (global_config->root == NULL) {
        LOG_ERROR("Failed to generate default configuration");
        free(global_config);
        global_config = NULL;
        return -1;
    }
    
    global_config->initialized = true;
    global_config->num_overrides = 0;
    
    LOG_INFO("Configuration system initialized");
    return 0;
}

/**
 * Clean up the configuration system
 */
int config_system_cleanup(void) {
    if (global_config == NULL) {
        return 0;
    }
    
    if (global_config->root != NULL) {
        config_object_free(global_config->root);
    }
    
    if (global_config->filename != NULL) {
        free(global_config->filename);
    }
    
    free(global_config);
    global_config = NULL;
    
    LOG_INFO("Configuration system cleaned up");
    return 0;
}

/**
 * Skip whitespace characters in JSON
 */
static void json_skip_whitespace(const char **json) {
    const char *p = *json;
    
    while (*p && (isspace((unsigned char)*p) || *p == '\n' || *p == '\r' || *p == '\t')) {
        p++;
    }
    
    *json = p;
}

/**
 * Parse a JSON string
 */
static char *json_parse_string(const char **json) {
    const char *p = *json;
    
    /* Expect opening quote */
    if (*p != '"') {
        LOG_ERROR("Expected '\"' at position %zu", (size_t)(p - *json));
        return NULL;
    }
    
    p++; /* Skip opening quote */
    
    /* Find closing quote, handling escapes */
    const char *start = p;
    int len = 0;
    bool escaped = false;
    
    while (*p) {
        if (escaped) {
            escaped = false;
        } else if (*p == '\\') {
            escaped = true;
        } else if (*p == '"') {
            break;
        }
        
        p++;
        len++;
    }
    
    if (*p != '"') {
        LOG_ERROR("Unterminated string at position %zu", (size_t)(start - *json));
        return NULL;
    }
    
    /* Allocate and copy the string */
    char *result = malloc(len + 1);
    if (result == NULL) {
        LOG_ERROR("Failed to allocate memory for string");
        return NULL;
    }
    
    /* Copy string, handling escape sequences */
    p = start;
    char *out = result;
    
    for (int i = 0; i < len; i++) {
        if (*p == '\\') {
            p++;
            
            switch (*p) {
                case '"': *out++ = '"'; break;
                case '\\': *out++ = '\\'; break;
                case '/': *out++ = '/'; break;
                case 'b': *out++ = '\b'; break;
                case 'f': *out++ = '\f'; break;
                case 'n': *out++ = '\n'; break;
                case 'r': *out++ = '\r'; break;
                case 't': *out++ = '\t'; break;
                default: *out++ = *p; break;
            }
        } else {
            *out++ = *p;
        }
        
        p++;
    }
    
    *out = '\0';
    
    /* Update the JSON pointer to after the closing quote */
    *json = p + 1;
    
    return result;
}

/**
 * Parse a JSON object
 */
static struct config_object *json_parse_object(const char **json) {
    const char *p = *json;
    
    /* Expect opening brace */
    if (*p != '{') {
        LOG_ERROR("Expected '{' at position %zu", (size_t)(p - *json));
        return NULL;
    }
    
    p++; /* Skip opening brace */
    
    /* Create new object */
    struct config_object *obj = calloc(1, sizeof(struct config_object));
    if (obj == NULL) {
        LOG_ERROR("Failed to allocate memory for object");
        return NULL;
    }
    
    obj->capacity = 8; /* Initial capacity */
    obj->entries = calloc(obj->capacity, sizeof(*obj->entries));
    if (obj->entries == NULL) {
        LOG_ERROR("Failed to allocate memory for object entries");
        free(obj);
        return NULL;
    }
    
    /* Skip whitespace */
    json_skip_whitespace(&p);
    
    /* Handle empty object */
    if (*p == '}') {
        p++; /* Skip closing brace */
        *json = p;
        return obj;
    }
    
    /* Check for unexpected end of input before parsing content */
    if (*p == '\0') {
        LOG_ERROR("Unexpected end of input, expected object content or '}' at position %zu", (size_t)(p - *json));
        config_object_free(obj);
        return NULL;
    }
    
    /* Parse key-value pairs */
    while (*p) {
        /* Skip whitespace */
        json_skip_whitespace(&p);
        
        /* Parse key */
        char *key = json_parse_string(&p);
        if (key == NULL) {
            LOG_ERROR("Failed to parse object key");
            config_object_free(obj);
            return NULL;
        }
        
        /* Skip whitespace */
        json_skip_whitespace(&p);
        
        /* Expect colon */
        if (*p != ':') {
            LOG_ERROR("Expected ':' after key at position %zu", (size_t)(p - *json));
            free(key);
            config_object_free(obj);
            return NULL;
        }
        
        p++; /* Skip colon */
        
        /* Parse value */
        const char *value_start = p;
        struct config_value value = json_parse_value(&p);
        if (value.type == CONFIG_VALUE_NULL && p == value_start) {
            LOG_ERROR("Failed to parse value for key '%s'", key);
            free(key);
            config_object_free(obj);
            return NULL;
        }
        
        /* Add key-value pair to object */
        if (obj->count >= obj->capacity) {
            /* Expand capacity */
            int old_capacity = obj->capacity;
            obj->capacity *= 2;
            obj->entries = realloc(obj->entries, obj->capacity * sizeof(*obj->entries));
            if (obj->entries == NULL) {
                LOG_ERROR("Failed to reallocate memory for object entries");
                free(key);
                config_value_free(&value);
                config_object_free(obj);
                return NULL;
            }
            
            /* Initialize newly allocated entries to zero */
            memset(&obj->entries[old_capacity], 0, 
                   (obj->capacity - old_capacity) * sizeof(*obj->entries));
        }
        
        obj->entries[obj->count].key = key;
        obj->entries[obj->count].value = value;
        obj->count++;
        
        /* Skip whitespace */
        json_skip_whitespace(&p);
        
        /* Check for comma, closing brace, or end of input */
        if (*p == ',') {
            p++; /* Skip comma */
        } else if (*p == '}') {
            p++; /* Skip closing brace */
            break;
        } else if (*p == '\0') {
            /* We've reached the end of input without a closing brace */
            LOG_ERROR("Unexpected end of input, expected ',' or '}' at position %zu", (size_t)(p - *json));
            config_object_free(obj);
            return NULL;
        } else {
            LOG_ERROR("Expected ',' or '}' at position %zu", (size_t)(p - *json));
            config_object_free(obj);
            return NULL;
        }
    }
    
    *json = p;
    return obj;
}

/**
 * Parse a JSON array
 */
static struct config_value json_parse_array(const char **json) {
    const char *p = *json;
    
    /* Expect opening bracket */
    if (*p != '[') {
        LOG_ERROR("Expected '[' at position %zu", (size_t)(p - *json));
        return (struct config_value){.type = CONFIG_VALUE_NULL};
    }
    
    p++; /* Skip opening bracket */
    
    /* Create array value */
    struct config_value array = {
        .type = CONFIG_VALUE_ARRAY,
        .u.array = {
            .items = NULL,
            .count = 0,
            .capacity = 0
        }
    };
    
    /* Initial capacity */
    array.u.array.capacity = 8;
    array.u.array.items = calloc(array.u.array.capacity, sizeof(struct config_value *));
    if (array.u.array.items == NULL) {
        LOG_ERROR("Failed to allocate memory for array");
        return (struct config_value){.type = CONFIG_VALUE_NULL};
    }
    
    /* Skip whitespace */
    json_skip_whitespace(&p);
    
    /* Handle empty array */
    if (*p == ']') {
        p++; /* Skip closing bracket */
        *json = p;
        return array;
    }
    
    /* Parse array elements */
    while (*p) {
        /* Parse value */
        struct config_value value = json_parse_value(&p);
        if (value.type == CONFIG_VALUE_NULL && *p == '\0') {
            LOG_ERROR("Failed to parse array element");
            config_value_free(&array);
            return (struct config_value){.type = CONFIG_VALUE_NULL};
        }
        
        /* Add value to array */
        if (array.u.array.count >= array.u.array.capacity) {
            /* Expand capacity */
            array.u.array.capacity *= 2;
            array.u.array.items = realloc(array.u.array.items, 
                                        array.u.array.capacity * sizeof(struct config_value *));
            if (array.u.array.items == NULL) {
                LOG_ERROR("Failed to reallocate memory for array");
                config_value_free(&value);
                config_value_free(&array);
                return (struct config_value){.type = CONFIG_VALUE_NULL};
            }
        }
        
        /* Allocate memory for the array element */
        array.u.array.items[array.u.array.count] = malloc(sizeof(struct config_value));
        if (array.u.array.items[array.u.array.count] == NULL) {
            LOG_ERROR("Failed to allocate memory for array element");
            config_value_free(&value);
            config_value_free(&array);
            return (struct config_value){.type = CONFIG_VALUE_NULL};
        }
        
        /* Copy the value */
        *array.u.array.items[array.u.array.count] = value;
        array.u.array.count++;
        
        /* Skip whitespace */
        json_skip_whitespace(&p);
        
        /* Check for comma, closing bracket, or end of input */
        if (*p == ',') {
            p++; /* Skip comma */
            /* After comma, skip whitespace and ensure we don't have immediate closing bracket */
            json_skip_whitespace(&p);
            if (*p == ']') {
                LOG_ERROR("Trailing comma in array at position %zu", (size_t)(p - *json));
                config_value_free(&array);
                return (struct config_value){.type = CONFIG_VALUE_NULL};
            }
        } else if (*p == ']') {
            p++; /* Skip closing bracket */
            break;
        } else if (*p == '\0') {
            /* We've reached the end of input without a closing bracket */
            LOG_ERROR("Unexpected end of input, expected ',' or ']' at position %zu", (size_t)(p - *json));
            config_value_free(&array);
            return (struct config_value){.type = CONFIG_VALUE_NULL};
        } else {
            LOG_ERROR("Expected ',' or ']' at position %zu", (size_t)(p - *json));
            config_value_free(&array);
            return (struct config_value){.type = CONFIG_VALUE_NULL};
        }
        
        /* Skip whitespace */
        json_skip_whitespace(&p);
    }
    
    *json = p;
    return array;
}

/**
 * Parse a JSON number
 */
static struct config_value json_parse_number(const char **json) {
    const char *p = *json;
    bool is_float = false;
    bool is_negative = false;
    
    /* Check for negative sign */
    if (*p == '-') {
        is_negative = true;
        p++;
    }
    
    /* Parse digits */
    if (!isdigit(*p)) {
        LOG_ERROR("Expected digit at position %zu", (size_t)(p - *json));
        return (struct config_value){.type = CONFIG_VALUE_NULL};
    }
    
    /* Parse integer part */
    int64_t int_value = 0;
    while (isdigit(*p)) {
        int_value = int_value * 10 + (*p - '0');
        p++;
    }
    
    /* Apply negative sign */
    if (is_negative) {
        int_value = -int_value;
    }
    
    /* Check for decimal point */
    double float_value = (double)int_value;
    if (*p == '.') {
        is_float = true;
        p++;
        
        /* Parse fractional part */
        double fraction = 0.0;
        double scale = 0.1;
        
        while (isdigit(*p)) {
            fraction += (*p - '0') * scale;
            scale *= 0.1;
            p++;
        }
        
        /* Apply fraction */
        float_value += is_negative ? -fraction : fraction;
    }
    
    /* Check for exponent */
    if (*p == 'e' || *p == 'E') {
        is_float = true;
        p++;
        
        /* Check for exponent sign */
        bool exp_negative = false;
        if (*p == '+') {
            p++;
        } else if (*p == '-') {
            exp_negative = true;
            p++;
        }
        
        /* Parse exponent */
        int exponent = 0;
        if (!isdigit(*p)) {
            LOG_ERROR("Expected digit in exponent at position %zu", (size_t)(p - *json));
            return (struct config_value){.type = CONFIG_VALUE_NULL};
        }
        
        while (isdigit(*p)) {
            exponent = exponent * 10 + (*p - '0');
            p++;
        }
        
        /* Apply exponent */
        double scale = 1.0;
        for (int i = 0; i < exponent; i++) {
            scale *= 10.0;
        }
        
        if (exp_negative) {
            float_value /= scale;
        } else {
            float_value *= scale;
        }
    }
    
    /* Return value */
    *json = p;
    
    if (is_float) {
        return (struct config_value){
            .type = CONFIG_VALUE_DOUBLE,
            .u.floating = float_value
        };
    } else {
        return (struct config_value){
            .type = CONFIG_VALUE_INTEGER,
            .u.integer = int_value
        };
    }
}

/**
 * Parse a JSON value
 */
static struct config_value json_parse_value(const char **json) {
    const char *p = *json;
    
    /* Skip whitespace */
    json_skip_whitespace(&p);
    
    /* Check value type */
    if (*p == '{') {
        struct config_object *obj = json_parse_object(&p);
        if (obj == NULL) {
            return (struct config_value){.type = CONFIG_VALUE_NULL};
        }
        
        *json = p;
        return (struct config_value){
            .type = CONFIG_VALUE_OBJECT,
            .u.object = obj
        };
    } else if (*p == '[') {
        struct config_value array = json_parse_array(&p);
        *json = p;
        return array;
    } else if (*p == '"') {
        char *str = json_parse_string(&p);
        if (str == NULL) {
            return (struct config_value){.type = CONFIG_VALUE_NULL};
        }
        
        *json = p;
        return (struct config_value){
            .type = CONFIG_VALUE_STRING,
            .u.string = str
        };
    } else if (*p == 't') {
        /* Parse "true" */
        if (strncmp(p, "true", 4) == 0) {
            *json = p + 4;
            return (struct config_value){
                .type = CONFIG_VALUE_BOOLEAN,
                .u.boolean = true
            };
        }
    } else if (*p == 'f') {
        /* Parse "false" */
        if (strncmp(p, "false", 5) == 0) {
            *json = p + 5;
            return (struct config_value){
                .type = CONFIG_VALUE_BOOLEAN,
                .u.boolean = false
            };
        }
    } else if (*p == 'n') {
        /* Parse "null" */
        if (strncmp(p, "null", 4) == 0) {
            *json = p + 4;
            return (struct config_value){.type = CONFIG_VALUE_NULL};
        }
    } else if (*p == '-' || isdigit(*p)) {
        /* Parse number */
        struct config_value result = json_parse_number(&p);
        *json = p;
        return result;
    }
    
    LOG_ERROR("Invalid JSON value at position %zu", (size_t)(p - *json));
    return (struct config_value){.type = CONFIG_VALUE_NULL};
}

/**
 * Free a config_value structure
 */
static void config_value_free(struct config_value *value) {
    if (value == NULL) {
        return;
    }
    
    switch (value->type) {
        case CONFIG_VALUE_STRING:
            free(value->u.string);
            break;
        case CONFIG_VALUE_OBJECT:
            config_object_free(value->u.object);
            break;
        case CONFIG_VALUE_ARRAY:
            for (int i = 0; i < value->u.array.count; i++) {
                if (value->u.array.items[i] != NULL) {
                    config_value_free(value->u.array.items[i]);
                    free(value->u.array.items[i]);
                }
            }
            free(value->u.array.items);
            break;
        default:
            /* Nothing to free for other types */
            break;
    }
    
    /* Reset the value to avoid double-free */
    value->type = CONFIG_VALUE_NULL;
}

/**
 * Free a config_object structure
 */
static void config_object_free(struct config_object *obj) {
    if (obj == NULL) {
        return;
    }
    
    for (int i = 0; i < obj->count; i++) {
        free(obj->entries[i].key);
        config_value_free(&obj->entries[i].value);
    }
    
    free(obj->entries);
    free(obj);
}

/**
 * Load configuration from a file
 */
int config_load_file(const char *filename) {
    if (global_config == NULL) {
        LOG_ERROR("Configuration system not initialized");
        return -1;
    }
    
    FILE *f = fopen(filename, "r");
    if (f == NULL) {
        LOG_ERROR("Failed to open configuration file: %s", filename);
        return -1;
    }
    
    /* Determine file size */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size <= 0 || size > MAX_CONFIG_FILE_SIZE) {
        LOG_ERROR("Invalid configuration file size: %lld bytes", size);
        fclose(f);
        return -1;
    }
    
    /* Read the entire file */
    char *buffer = malloc(size + 1);
    if (buffer == NULL) {
        LOG_ERROR("Failed to allocate memory for configuration file");
        fclose(f);
        return -1;
    }
    
    size_t read_size = fread(buffer, 1, size, f);
    fclose(f);
    
    if (read_size != (size_t)size) {
        LOG_ERROR("Failed to read configuration file: %s", filename);
        free(buffer);
        return -1;
    }
    
    buffer[size] = '\0'; /* Null-terminate the buffer */
    
    /* Parse the JSON */
    const char *json = buffer;
    struct config_object *new_config = json_parse_object(&json);
    
    free(buffer);
    
    if (new_config == NULL) {
        LOG_ERROR("Failed to parse configuration file: %s", filename);
        return -1;
    }
    
    /* Check for trailing garbage after a successful parse */
    json_skip_whitespace(&json);
    if (*json != '\0') {
        LOG_ERROR("Unexpected trailing characters after JSON object in %s: '%s'", filename, json);
        config_object_free(new_config);
        return -1;
    }
    
    /* Replace the current configuration */
    if (global_config->root != NULL) {
        config_object_free(global_config->root);
    }
    
    global_config->root = new_config;
    
    /* Store the filename */
    if (global_config->filename != NULL) {
        free(global_config->filename);
    }
    
    global_config->filename = strdup(filename);
    
    LOG_INFO("Loaded configuration from %s", filename);
    
    /* Apply any pending overrides */
    if (global_config->num_overrides > 0) {
        config_apply_overrides();
    }
    
    return 0;
}

/**
 * Save configuration to a file
 */
int config_save_file(const char *filename, bool pretty) {
    if (global_config == NULL || global_config->root == NULL) {
        LOG_ERROR("No configuration to save");
        return -1;
    }
    
    FILE *f = fopen(filename, "w");
    if (f == NULL) {
        LOG_ERROR("Failed to open configuration file for writing: %s", filename);
        return -1;
    }
    
    /* Serialize the configuration */
    json_serialize_object(global_config->root, f, 0, pretty);
    
    fclose(f);
    
    LOG_INFO("Saved configuration to %s", filename);
    
    return 0;
}

/**
 * Serialize a JSON value to a file
 */
static void json_serialize_value(const struct config_value *value, FILE *f, int indent_level, bool pretty) {
    if (value == NULL) {
        fprintf(f, "null");
        return;
    }
    
    switch (value->type) {
        case CONFIG_VALUE_NULL:
            fprintf(f, "null");
            break;
        case CONFIG_VALUE_BOOLEAN:
            fprintf(f, value->u.boolean ? "true" : "false");
            break;
        case CONFIG_VALUE_INTEGER:
            fprintf(f, "%"PRId64, value->u.integer);
            break;
        case CONFIG_VALUE_DOUBLE:
            fprintf(f, "%.16g", value->u.floating);
            break;
        case CONFIG_VALUE_STRING:
            fprintf(f, "\"");
            
            /* Escape special characters */
            const char *p = value->u.string;
            while (*p) {
                switch (*p) {
                    case '"': fprintf(f, "\\\""); break;
                    case '\\': fprintf(f, "\\\\"); break;
                    case '\b': fprintf(f, "\\b"); break;
                    case '\f': fprintf(f, "\\f"); break;
                    case '\n': fprintf(f, "\\n"); break;
                    case '\r': fprintf(f, "\\r"); break;
                    case '\t': fprintf(f, "\\t"); break;
                    default:
                        if ((unsigned char)*p < 32) {
                            fprintf(f, "\\u%04x", (unsigned char)*p);
                        } else {
                            fputc(*p, f);
                        }
                        break;
                }
                p++;
            }
            
            fprintf(f, "\"");
            break;
        case CONFIG_VALUE_OBJECT:
            json_serialize_object(value->u.object, f, indent_level, pretty);
            break;
        case CONFIG_VALUE_ARRAY:
            fprintf(f, "[");
            
            if (pretty && value->u.array.count > 0) {
                fprintf(f, "\n");
            }
            
            for (int i = 0; i < value->u.array.count; i++) {
                if (pretty) {
                    for (int j = 0; j < indent_level + 2; j++) {
                        fprintf(f, "  ");
                    }
                }
                
                json_serialize_value(value->u.array.items[i], f, indent_level + 2, pretty);
                
                if (i < value->u.array.count - 1) {
                    fprintf(f, ",");
                }
                
                if (pretty) {
                    fprintf(f, "\n");
                }
            }
            
            if (pretty && value->u.array.count > 0) {
                for (int j = 0; j < indent_level; j++) {
                    fprintf(f, "  ");
                }
            }
            
            fprintf(f, "]");
            break;
    }
}

/**
 * Serialize a JSON object to a file
 */
static void json_serialize_object(const struct config_object *obj, FILE *f, int indent_level, bool pretty) {
    if (obj == NULL) {
        fprintf(f, "null");
        return;
    }
    
    fprintf(f, "{");
    
    if (pretty && obj->count > 0) {
        fprintf(f, "\n");
    }
    
    for (int i = 0; i < obj->count; i++) {
        if (pretty) {
            for (int j = 0; j < indent_level + 1; j++) {
                fprintf(f, "  ");
            }
        }
        
        fprintf(f, "\"%s\":", obj->entries[i].key);
        
        if (pretty) {
            fprintf(f, " ");
        }
        
        json_serialize_value(&obj->entries[i].value, f, indent_level + 1, pretty);
        
        if (i < obj->count - 1) {
            fprintf(f, ",");
        }
        
        if (pretty) {
            fprintf(f, "\n");
        }
    }
    
    if (pretty && obj->count > 0) {
        for (int j = 0; j < indent_level; j++) {
            fprintf(f, "  ");
        }
    }
    
    fprintf(f, "}");
}

/**
 * Get a configuration value
 */
static const struct config_value *config_get_value_internal(const struct config_object *obj, const char *path) {
    if (obj == NULL || path == NULL) {
        return NULL;
    }
    
    /* Make a copy of the path so we can modify it */
    char *path_copy = strdup(path);
    if (path_copy == NULL) {
        LOG_ERROR("Failed to allocate memory for path copy");
        return NULL;
    }
    
    /* Parse the path */
    const struct config_object *current_obj = obj;
    const struct config_value *result = NULL;
    
    char *token = strtok(path_copy, ".");
    char *next_token = NULL;
    
    while (token != NULL) {
        next_token = strtok(NULL, ".");
        
        /* Find the key in the current object */
        bool found = false;
        
        for (int i = 0; i < current_obj->count; i++) {
            if (strcmp(current_obj->entries[i].key, token) == 0) {
                if (next_token == NULL) {
                    /* This is the final token, return the value */
                    result = &current_obj->entries[i].value;
                    found = true;
                    break;
                } else if (current_obj->entries[i].value.type == CONFIG_VALUE_OBJECT) {
                    /* Continue traversing the path */
                    current_obj = current_obj->entries[i].value.u.object;
                    found = true;
                    break;
                } else {
                    /* Path expects an object but found something else */
                    LOG_DEBUG("Path '%s' expects an object at '%s' but found %d",
                             path, token, current_obj->entries[i].value.type);
                    free(path_copy);
                    return NULL;
                }
            }
        }
        
        if (!found) {
            /* Key not found */
            free(path_copy);
            return NULL;
        }
        
        token = next_token;
    }
    
    free(path_copy);
    return result;
}

/**
 * Get a configuration value
 */
const struct config_value *config_get_value(const char *path) {
    if (global_config == NULL || global_config->root == NULL) {
        LOG_ERROR("Configuration system not initialized");
        return NULL;
    }
    
    return config_get_value_internal(global_config->root, path);
}

/**
 * Get a boolean value from the configuration
 */
bool config_get_boolean(const char *path, bool default_value) {
    const struct config_value *value = config_get_value(path);
    
    if (value == NULL) {
        return default_value;
    }
    
    if (value->type == CONFIG_VALUE_BOOLEAN) {
        return value->u.boolean;
    } else if (value->type == CONFIG_VALUE_INTEGER) {
        return value->u.integer != 0;
    } else if (value->type == CONFIG_VALUE_DOUBLE) {
        return value->u.floating != 0.0;
    } else if (value->type == CONFIG_VALUE_STRING) {
        if (strcmp(value->u.string, "true") == 0 || 
            strcmp(value->u.string, "yes") == 0 || 
            strcmp(value->u.string, "1") == 0) {
            return true;
        } else if (strcmp(value->u.string, "false") == 0 || 
                  strcmp(value->u.string, "no") == 0 || 
                  strcmp(value->u.string, "0") == 0) {
            return false;
        }
    }
    
    LOG_WARNING("Invalid boolean value for path '%s', using default", path);
    return default_value;
}

/**
 * Get an integer value from the configuration
 */
int config_get_integer(const char *path, int default_value) {
    const struct config_value *value = config_get_value(path);
    
    if (value == NULL) {
        return default_value;
    }
    
    if (value->type == CONFIG_VALUE_INTEGER) {
        return (int)value->u.integer;
    } else if (value->type == CONFIG_VALUE_DOUBLE) {
        return (int)value->u.floating;
    } else if (value->type == CONFIG_VALUE_BOOLEAN) {
        return value->u.boolean ? 1 : 0;
    } else if (value->type == CONFIG_VALUE_STRING) {
        return atoi(value->u.string);
    }
    
    LOG_WARNING("Invalid integer value for path '%s', using default", path);
    return default_value;
}

/**
 * Get a double value from the configuration
 */
double config_get_double(const char *path, double default_value) {
    const struct config_value *value = config_get_value(path);
    
    if (value == NULL) {
        return default_value;
    }
    
    if (value->type == CONFIG_VALUE_DOUBLE) {
        return value->u.floating;
    } else if (value->type == CONFIG_VALUE_INTEGER) {
        return (double)value->u.integer;
    } else if (value->type == CONFIG_VALUE_BOOLEAN) {
        return value->u.boolean ? 1.0 : 0.0;
    } else if (value->type == CONFIG_VALUE_STRING) {
        return atof(value->u.string);
    }
    
    LOG_WARNING("Invalid double value for path '%s', using default", path);
    return default_value;
}

/**
 * Get a string value from the configuration
 */
const char *config_get_string(const char *path, const char *default_value) {
    const struct config_value *value = config_get_value(path);
    
    if (value == NULL) {
        return default_value;
    }
    
    if (value->type == CONFIG_VALUE_STRING) {
        return value->u.string;
    }
    
    LOG_WARNING("Invalid string value for path '%s', using default", path);
    return default_value;
}

/**
 * Get an array size
 */
int config_get_array_size(const char *path) {
    const struct config_value *value = config_get_value(path);
    
    if (value == NULL) {
        return -1;
    }
    
    if (value->type != CONFIG_VALUE_ARRAY) {
        LOG_WARNING("Path '%s' does not point to an array", path);
        return -1;
    }
    
    return value->u.array.count;
}

/**
 * Get an array element
 */
const struct config_value *config_get_array_element(const char *path, int index) {
    const struct config_value *value = config_get_value(path);
    
    if (value == NULL) {
        return NULL;
    }
    
    if (value->type != CONFIG_VALUE_ARRAY) {
        LOG_WARNING("Path '%s' does not point to an array", path);
        return NULL;
    }
    
    if (index < 0 || index >= value->u.array.count) {
        LOG_WARNING("Array index %d out of bounds for path '%s'", index, path);
        return NULL;
    }
    
    return value->u.array.items[index];
}

/**
 * Set a configuration value
 */
static int config_set_value_internal(struct config_object *obj, const char *path, struct config_value value) {
    if (obj == NULL || path == NULL) {
        return -1;
    }
    
    /* Make a copy of the path so we can modify it */
    char *path_copy = strdup(path);
    if (path_copy == NULL) {
        LOG_ERROR("Failed to allocate memory for path copy");
        config_value_free(&value);
        return -1;
    }
    
    /* Parse the path */
    struct config_object *current_obj = obj;
    
    char *token = strtok(path_copy, ".");
    char *next_token = NULL;
    
    while (token != NULL) {
        next_token = strtok(NULL, ".");
        
        if (next_token == NULL) {
            /* This is the final token, set the value */
            bool found = false;
            
            /* Check if the key already exists */
            for (int i = 0; i < current_obj->count; i++) {
                if (strcmp(current_obj->entries[i].key, token) == 0) {
                    /* Free the existing value */
                    config_value_free(&current_obj->entries[i].value);
                    
                    /* Set the new value */
                    current_obj->entries[i].value = value;
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                /* Add a new key-value pair */
                if (current_obj->count >= current_obj->capacity) {
                    /* Expand capacity */
                    int old_capacity = current_obj->capacity;
                    current_obj->capacity *= 2;
                    current_obj->entries = realloc(current_obj->entries, 
                                                 current_obj->capacity * sizeof(*current_obj->entries));
                    if (current_obj->entries == NULL) {
                        LOG_ERROR("Failed to reallocate memory for object entries");
                        free(path_copy);
                        config_value_free(&value);
                        return -1;
                    }
                    
                    /* Initialize newly allocated entries to zero */
                    memset(&current_obj->entries[old_capacity], 0, 
                           (current_obj->capacity - old_capacity) * sizeof(*current_obj->entries));
                }
                
                /* Add the new entry */
                current_obj->entries[current_obj->count].key = strdup(token);
                if (current_obj->entries[current_obj->count].key == NULL) {
                    LOG_ERROR("Failed to allocate memory for key");
                    free(path_copy);
                    config_value_free(&value);
                    return -1;
                }
                
                current_obj->entries[current_obj->count].value = value;
                current_obj->count++;
            }
            
            free(path_copy);
            return 0;
        } else {
            /* Find or create the intermediate object */
            bool found = false;
            
            for (int i = 0; i < current_obj->count; i++) {
                if (strcmp(current_obj->entries[i].key, token) == 0) {
                    if (current_obj->entries[i].value.type == CONFIG_VALUE_OBJECT) {
                        /* Continue traversing */
                        current_obj = current_obj->entries[i].value.u.object;
                        found = true;
                        break;
                    } else {
                        /* Replace with an object */
                        config_value_free(&current_obj->entries[i].value);
                        
                        /* Create a new object */
                        current_obj->entries[i].value.type = CONFIG_VALUE_OBJECT;
                        current_obj->entries[i].value.u.object = calloc(1, sizeof(struct config_object));
                        if (current_obj->entries[i].value.u.object == NULL) {
                            LOG_ERROR("Failed to allocate memory for object");
                            free(path_copy);
                            config_value_free(&value);
                            return -1;
                        }
                        
                        /* Initialize the new object */
                        current_obj->entries[i].value.u.object->capacity = 8;
                        current_obj->entries[i].value.u.object->entries = 
                            calloc(current_obj->entries[i].value.u.object->capacity, 
                                   sizeof(*current_obj->entries[i].value.u.object->entries));
                        if (current_obj->entries[i].value.u.object->entries == NULL) {
                            LOG_ERROR("Failed to allocate memory for object entries");
                            free(current_obj->entries[i].value.u.object);
                            current_obj->entries[i].value.u.object = NULL;
                            free(path_copy);
                            config_value_free(&value);
                            return -1;
                        }
                        
                        current_obj = current_obj->entries[i].value.u.object;
                        found = true;
                        break;
                    }
                }
            }
            
            if (!found) {
                /* Create a new object */
                if (current_obj->count >= current_obj->capacity) {
                    /* Expand capacity */
                    int old_capacity = current_obj->capacity;
                    current_obj->capacity *= 2;
                    current_obj->entries = realloc(current_obj->entries, 
                                                 current_obj->capacity * sizeof(*current_obj->entries));
                    if (current_obj->entries == NULL) {
                        LOG_ERROR("Failed to reallocate memory for object entries");
                        free(path_copy);
                        config_value_free(&value);
                        return -1;
                    }
                    
                    /* Initialize newly allocated entries to zero */
                    memset(&current_obj->entries[old_capacity], 0, 
                           (current_obj->capacity - old_capacity) * sizeof(*current_obj->entries));
                }
                
                /* Initialize the new entry */
                current_obj->entries[current_obj->count].key = strdup(token);
                if (current_obj->entries[current_obj->count].key == NULL) {
                    LOG_ERROR("Failed to allocate memory for key");
                    free(path_copy);
                    config_value_free(&value);
                    return -1;
                }
                
                current_obj->entries[current_obj->count].value.type = CONFIG_VALUE_OBJECT;
                current_obj->entries[current_obj->count].value.u.object = calloc(1, sizeof(struct config_object));
                if (current_obj->entries[current_obj->count].value.u.object == NULL) {
                    LOG_ERROR("Failed to allocate memory for object");
                    free(current_obj->entries[current_obj->count].key);
                    free(path_copy);
                    config_value_free(&value);
                    return -1;
                }
                
                /* Initialize the new object */
                current_obj->entries[current_obj->count].value.u.object->capacity = 8;
                current_obj->entries[current_obj->count].value.u.object->entries = 
                    calloc(current_obj->entries[current_obj->count].value.u.object->capacity, 
                           sizeof(*current_obj->entries[current_obj->count].value.u.object->entries));
                if (current_obj->entries[current_obj->count].value.u.object->entries == NULL) {
                    LOG_ERROR("Failed to allocate memory for object entries");
                    free(current_obj->entries[current_obj->count].value.u.object);
                    free(current_obj->entries[current_obj->count].key);
                    free(path_copy);
                    config_value_free(&value);
                    return -1;
                }
                
                /* Move to the newly created object */
                int parent_index = current_obj->count;
                current_obj->count++;  /* Increment parent object's count first */
                current_obj = current_obj->entries[parent_index].value.u.object;  /* Then point to child object */
            }
        }
        
        token = next_token;
    }
    
    free(path_copy);
    config_value_free(&value);
    return -1; /* Should not reach here */
}

/**
 * Set a boolean value in the configuration
 */
int config_set_boolean(const char *path, bool value) {
    if (global_config == NULL || global_config->root == NULL) {
        LOG_ERROR("Configuration system not initialized");
        return -1;
    }
    
    struct config_value config_value = {
        .type = CONFIG_VALUE_BOOLEAN,
        .u.boolean = value
    };
    
    return config_set_value_internal(global_config->root, path, config_value);
}

/**
 * Set an integer value in the configuration
 */
int config_set_integer(const char *path, int value) {
    if (global_config == NULL || global_config->root == NULL) {
        LOG_ERROR("Configuration system not initialized");
        return -1;
    }
    
    struct config_value config_value = {
        .type = CONFIG_VALUE_INTEGER,
        .u.integer = value
    };
    
    return config_set_value_internal(global_config->root, path, config_value);
}

/**
 * Set a double value in the configuration
 */
int config_set_double(const char *path, double value) {
    if (global_config == NULL || global_config->root == NULL) {
        LOG_ERROR("Configuration system not initialized");
        return -1;
    }
    
    struct config_value config_value = {
        .type = CONFIG_VALUE_DOUBLE,
        .u.floating = value
    };
    
    return config_set_value_internal(global_config->root, path, config_value);
}

/**
 * Set a string value in the configuration
 */
int config_set_string(const char *path, const char *value) {
    if (global_config == NULL || global_config->root == NULL) {
        LOG_ERROR("Configuration system not initialized");
        return -1;
    }
    
    if (value == NULL) {
        struct config_value config_value = {
            .type = CONFIG_VALUE_NULL
        };
        
        return config_set_value_internal(global_config->root, path, config_value);
    }
    
    struct config_value config_value = {
        .type = CONFIG_VALUE_STRING,
        .u.string = strdup(value)
    };
    
    if (config_value.u.string == NULL) {
        LOG_ERROR("Failed to allocate memory for string value");
        return -1;
    }
    
    return config_set_value_internal(global_config->root, path, config_value);
}

/**
 * Add a configuration value override
 */
int config_add_override(const char *path, const char *value) {
    if (global_config == NULL) {
        LOG_ERROR("Configuration system not initialized");
        return -1;
    }
    
    if (global_config->num_overrides >= MAX_CONFIG_OVERRIDE_ARGS) {
        LOG_ERROR("Maximum number of configuration overrides exceeded");
        return -1;
    }
    
    strncpy(global_config->overrides[global_config->num_overrides].path, path, MAX_CONFIG_PATH - 1);
    global_config->overrides[global_config->num_overrides].path[MAX_CONFIG_PATH - 1] = '\0';
    
    strncpy(global_config->overrides[global_config->num_overrides].value, value, MAX_CONFIG_VALUE - 1);
    global_config->overrides[global_config->num_overrides].value[MAX_CONFIG_VALUE - 1] = '\0';
    
    global_config->num_overrides++;
    
    return 0;
}

/**
 * Apply configuration overrides
 */
int config_apply_overrides(void) {
    if (global_config == NULL || global_config->root == NULL) {
        LOG_ERROR("Configuration system not initialized");
        return -1;
    }
    
    for (int i = 0; i < global_config->num_overrides; i++) {
        const char *path = global_config->overrides[i].path;
        const char *value = global_config->overrides[i].value;
        
        /* Try to determine the type of the value */
        if (strcmp(value, "true") == 0 || strcmp(value, "yes") == 0) {
            config_set_boolean(path, true);
        } else if (strcmp(value, "false") == 0 || strcmp(value, "no") == 0) {
            config_set_boolean(path, false);
        } else if (value[0] == '"' && value[strlen(value) - 1] == '"') {
            /* Remove the quotes */
            char *string_value = strdup(value + 1);
            if (string_value == NULL) {
                LOG_ERROR("Failed to allocate memory for string value");
                continue;
            }
            
            string_value[strlen(string_value) - 1] = '\0';
            config_set_string(path, string_value);
            free(string_value);
        } else {
            /* Try to parse as a number */
            char *endptr;
            long long_value = strtol(value, &endptr, 10);
            
            if (*endptr == '\0') {
                /* Integer */
                config_set_integer(path, (int)long_value);
            } else {
                /* Try double */
                double double_value = strtod(value, &endptr);
                
                if (*endptr == '\0') {
                    /* Double */
                    config_set_double(path, double_value);
                } else {
                    /* String */
                    config_set_string(path, value);
                }
            }
        }
    }
    
    return 0;
}

/**
 * Configure modules from the configuration
 */
int config_configure_modules(struct params *params) {
    if (global_config == NULL || global_config->root == NULL) {
        LOG_ERROR("Configuration system not initialized");
        return -1;
    }
    
    if (params == NULL) {
        LOG_ERROR("Invalid parameters");
        return -1;
    }
    
    LOG_INFO("Configuring module system");
    
    /* Configure individual modules */
    const struct config_value *modules = config_get_value("modules.instances");
    if (modules != NULL && modules->type == CONFIG_VALUE_ARRAY) {
        for (int i = 0; i < modules->u.array.count; i++) {
            const struct config_value *module = modules->u.array.items[i];
            
            if (module->type != CONFIG_VALUE_OBJECT) {
                continue;
            }
            
            /* Find the module name */
            const char *name = NULL;
            
            for (int j = 0; j < module->u.object->count; j++) {
                if (strcmp(module->u.object->entries[j].key, "name") == 0 &&
                    module->u.object->entries[j].value.type == CONFIG_VALUE_STRING) {
                    name = module->u.object->entries[j].value.u.string;
                    break;
                }
            }
            
            if (name == NULL) {
                LOG_WARNING("Module at index %d has no name, skipping", i);
                continue;
            }
            
            /* Find the module */
            int module_id = module_find_by_name(name);
            if (module_id < 0) {
                LOG_WARNING("Module '%s' not found, skipping", name);
                continue;
            }
            
            /* Configure the module */
            struct base_module *module_ptr;
            void *module_data;
            
            if (module_get(module_id, &module_ptr, &module_data) != 0) {
                LOG_WARNING("Failed to get module '%s', skipping", name);
                continue;
            }
            
            /* Apply configuration */
            for (int j = 0; j < module->u.object->count; j++) {
                const char *key = module->u.object->entries[j].key;
                const struct config_value *value = &module->u.object->entries[j].value;
                
                /* Skip the name */
                if (strcmp(key, "name") == 0) {
                    continue;
                }
                
                /* Convert the value to a string */
                char value_str[MAX_CONFIG_VALUE];
                
                switch (value->type) {
                    case CONFIG_VALUE_BOOLEAN:
                        snprintf(value_str, sizeof(value_str), "%s", value->u.boolean ? "true" : "false");
                        break;
                    case CONFIG_VALUE_INTEGER:
                        snprintf(value_str, sizeof(value_str), "%"PRId64, value->u.integer);
                        break;
                    case CONFIG_VALUE_DOUBLE:
                        snprintf(value_str, sizeof(value_str), "%.16g", value->u.floating);
                        break;
                    case CONFIG_VALUE_STRING:
                        strncpy(value_str, value->u.string, sizeof(value_str) - 1);
                        value_str[sizeof(value_str) - 1] = '\0';
                        break;
                    default:
                        LOG_WARNING("Unsupported value type %d for module '%s' parameter '%s'",
                                   value->type, name, key);
                        continue;
                }
                
                /* Configure the module */
                if (module_ptr->configure != NULL) {
                    int status = module_ptr->configure(module_data, key, value_str);
                    if (status != 0) {
                        LOG_WARNING("Failed to configure module '%s' parameter '%s'", name, key);
                    }
                } else {
                    LOG_WARNING("Module '%s' does not support configuration", name);
                }
            }
            
            /* Activate the module if specified */
            bool active = false;
            
            for (int j = 0; j < module->u.object->count; j++) {
                if (strcmp(module->u.object->entries[j].key, "active") == 0 &&
                    module->u.object->entries[j].value.type == CONFIG_VALUE_BOOLEAN) {
                    active = module->u.object->entries[j].value.u.boolean;
                    break;
                }
            }
            
            if (active) {
                module_set_active(module_id);
            }
        }
    }
    
    return 0;
}

/**
 * Configure pipeline from the configuration
 */
int config_configure_pipeline(void) {
    if (global_config == NULL || global_config->root == NULL) {
        LOG_ERROR("Configuration system not initialized");
        return -1;
    }
    
    /* Check if a custom pipeline is specified */
    const struct config_value *pipeline_steps = config_get_value("pipeline.steps");
    if (pipeline_steps == NULL || pipeline_steps->type != CONFIG_VALUE_ARRAY) {
        LOG_INFO("No custom pipeline defined in configuration");
        return 0;
    }
    
    /* Create a new pipeline */
    const char *pipeline_name = config_get_string("pipeline.name", "custom");
    struct module_pipeline *pipeline = pipeline_create(pipeline_name);
    if (pipeline == NULL) {
        LOG_ERROR("Failed to create pipeline '%s'", pipeline_name);
        return -1;
    }
    
    /* Add steps from the configuration */
    for (int i = 0; i < pipeline_steps->u.array.count; i++) {
        const struct config_value *step = pipeline_steps->u.array.items[i];
        
        if (step->type != CONFIG_VALUE_OBJECT) {
            LOG_WARNING("Pipeline step at index %d is not an object, skipping", i);
            continue;
        }
        
        /* Get step properties */
        const char *type_str = NULL;
        const char *module_name = NULL;
        const char *step_name = NULL;
        bool enabled = true;
        bool optional = false;
        
        for (int j = 0; j < step->u.object->count; j++) {
            const char *key = step->u.object->entries[j].key;
            const struct config_value *value = &step->u.object->entries[j].value;
            
            if (strcmp(key, "type") == 0 && value->type == CONFIG_VALUE_STRING) {
                type_str = value->u.string;
            } else if (strcmp(key, "module") == 0 && value->type == CONFIG_VALUE_STRING) {
                module_name = value->u.string;
            } else if (strcmp(key, "name") == 0 && value->type == CONFIG_VALUE_STRING) {
                step_name = value->u.string;
            } else if (strcmp(key, "enabled") == 0 && value->type == CONFIG_VALUE_BOOLEAN) {
                enabled = value->u.boolean;
            } else if (strcmp(key, "optional") == 0 && value->type == CONFIG_VALUE_BOOLEAN) {
                optional = value->u.boolean;
            }
        }
        
        if (type_str == NULL) {
            LOG_WARNING("Pipeline step at index %d has no type, skipping", i);
            continue;
        }
        
        enum module_type type = module_type_from_string(type_str);
        if (type == MODULE_TYPE_UNKNOWN) {
            LOG_WARNING("Unknown module type '%s' for pipeline step at index %d, skipping",
                       type_str, i);
            continue;
        }
        
        /* Add the step to the pipeline */
        if (pipeline_add_step(pipeline, type, module_name, step_name, enabled, optional) != 0) {
            LOG_WARNING("Failed to add step of type '%s' to pipeline", type_str);
        }
    }
    
    /* Validate the pipeline */
    if (!pipeline_validate(pipeline)) {
        LOG_ERROR("Pipeline validation failed");
        pipeline_destroy(pipeline);
        return -1;
    }
    
    /* Set as global pipeline if specified */
    bool use_as_global = config_get_boolean("pipeline.use_as_global", true);
    if (use_as_global) {
        if (pipeline_set_global(pipeline) != 0) {
            LOG_ERROR("Failed to set global pipeline");
            pipeline_destroy(pipeline);
            return -1;
        }
        
        LOG_INFO("Set global pipeline to '%s' with %d steps",
                pipeline->name, pipeline->num_steps);
    }
    
    return 0;
}

/**
 * Configure parameters from the configuration
 */
int config_configure_params(struct params *params) {
    if (global_config == NULL || global_config->root == NULL) {
        LOG_ERROR("Configuration system not initialized");
        return -1;
    }
    
    if (params == NULL) {
        LOG_ERROR("Invalid parameters");
        return -1;
    }
    
    /* Configure IO parameters */
    params->io.FirstFile = config_get_integer("simulation.first_file", params->io.FirstFile);
    params->io.LastFile = config_get_integer("simulation.last_file", params->io.LastFile);
    params->io.NumSimulationTreeFiles = config_get_integer("simulation.num_tree_files", params->io.NumSimulationTreeFiles);
    
    const char *snap_list_file = config_get_string("simulation.snap_list_file", NULL);
    if (snap_list_file != NULL) {
        strncpy(params->io.FileWithSnapList, snap_list_file, MAX_STRING_LEN - 1);
        params->io.FileWithSnapList[MAX_STRING_LEN - 1] = '\0';
    }
    
    const char *sim_dir = config_get_string("simulation.directory", NULL);
    if (sim_dir != NULL) {
        strncpy(params->io.SimulationDir, sim_dir, MAX_STRING_LEN - 1);
        params->io.SimulationDir[MAX_STRING_LEN - 1] = '\0';
    }
    
    const char *tree_name = config_get_string("simulation.tree_name", NULL);
    if (tree_name != NULL) {
        strncpy(params->io.TreeName, tree_name, MAX_STRING_LEN - 1);
        params->io.TreeName[MAX_STRING_LEN - 1] = '\0';
    }
    
    params->io.TreeType = config_get_integer("simulation.tree_type", params->io.TreeType);
    
    /* Configure cosmology parameters */
    params->cosmology.Omega = config_get_double("cosmology.omega_matter", params->cosmology.Omega);
    params->cosmology.OmegaLambda = config_get_double("cosmology.omega_lambda", params->cosmology.OmegaLambda);
    params->physics.BaryonFrac = config_get_double("cosmology.baryon_fraction", params->physics.BaryonFrac);
    params->cosmology.Hubble_h = config_get_double("cosmology.hubble_h", params->cosmology.Hubble_h);
    
    /* Configure output parameters */
    const char *output_dir = config_get_string("output.directory", NULL);
    if (output_dir != NULL) {
        strncpy(params->io.OutputDir, output_dir, MAX_STRING_LEN - 1);
        params->io.OutputDir[MAX_STRING_LEN - 1] = '\0';
    }
    
    const char *galaxy_name = config_get_string("output.prefix", NULL);
    if (galaxy_name != NULL) {
        strncpy(params->io.FileNameGalaxies, galaxy_name, MAX_STRING_LEN - 1);
        params->io.FileNameGalaxies[MAX_STRING_LEN - 1] = '\0';
    }
    
    params->io.NumSimulationTreeFiles = config_get_integer("output.num_files", params->io.NumSimulationTreeFiles);
    params->io.OutputFormat = config_get_integer("output.format", params->io.OutputFormat);
    
    /* Configure physics parameters */
    params->physics.RecycleFraction = config_get_double("physics.recycle_fraction", params->physics.RecycleFraction);
    params->physics.ReIncorporationFactor = config_get_double("physics.reincorporation_factor", params->physics.ReIncorporationFactor);
    params->physics.FeedbackReheatingEpsilon = config_get_double("physics.feedback_reheating_epsilon", params->physics.FeedbackReheatingEpsilon);
    params->physics.FeedbackEjectionEfficiency = config_get_double("physics.feedback_ejection_efficiency", params->physics.FeedbackEjectionEfficiency);
    params->physics.RadioModeEfficiency = config_get_double("physics.eject_cutoff_velocity", params->physics.RadioModeEfficiency);
    params->physics.SfrEfficiency = config_get_double("physics.sfr_efficiency", params->physics.SfrEfficiency);
    
    /* Configure more physics parameters */
    params->physics.AGNrecipeOn = config_get_integer("physics.agn_feedback_enabled", params->physics.AGNrecipeOn);
    params->physics.BlackHoleGrowthRate = config_get_double("physics.black_hole_growth_rate", params->physics.BlackHoleGrowthRate);
    params->physics.RadioModeEfficiency = config_get_double("physics.radio_mode_efficiency", params->physics.RadioModeEfficiency);
    params->physics.QuasarModeEfficiency = config_get_double("physics.quasar_mode_efficiency", params->physics.QuasarModeEfficiency);
    params->physics.ThreshMajorMerger = config_get_double("physics.thresh_major_merger", params->physics.ThreshMajorMerger);
    params->physics.ThresholdSatDisruption = config_get_double("physics.threshold_satellite_disruption", params->physics.ThresholdSatDisruption);
    params->physics.ReIncorporationFactor = config_get_double("physics.merger_time_multiplier", params->physics.ReIncorporationFactor);
    params->physics.DiskInstabilityOn = config_get_boolean("physics.disk_instability_enabled", params->physics.DiskInstabilityOn);
    
    
    return 0;
}

/**
 * Generate default configuration
 */
struct config_object *config_generate_default(void) {
    struct config_object *config = calloc(1, sizeof(struct config_object));
    if (config == NULL) {
        LOG_ERROR("Failed to allocate memory for default configuration");
        return NULL;
    }
    
    config->capacity = 16;
    config->entries = calloc(config->capacity, sizeof(*config->entries));
    if (config->entries == NULL) {
        LOG_ERROR("Failed to allocate memory for config entries");
        free(config);
        return NULL;
    }
    
    /* Add default sections */
    
    /* Modules section */
    {
        struct config_value value;
        value.type = CONFIG_VALUE_OBJECT;
        value.u.object = calloc(1, sizeof(struct config_object));
        if (value.u.object == NULL) {
            LOG_ERROR("Failed to allocate memory for modules section");
            config_object_free(config);
            return NULL;
        }
        
        value.u.object->capacity = 8;
        value.u.object->entries = calloc(value.u.object->capacity, sizeof(*value.u.object->entries));
        if (value.u.object->entries == NULL) {
            LOG_ERROR("Failed to allocate memory for modules entries");
            free(value.u.object);
            config_object_free(config);
            return NULL;
        }
        
        /* Add instances array (placeholder for module configuration) */
        value.u.object->entries[value.u.object->count].key = strdup("instances");
        value.u.object->entries[value.u.object->count].value.type = CONFIG_VALUE_ARRAY;
        value.u.object->entries[value.u.object->count].value.u.array.capacity = 0;
        value.u.object->entries[value.u.object->count].value.u.array.items = NULL;
        value.u.object->entries[value.u.object->count].value.u.array.count = 0;
        value.u.object->count++;
        
        /* Add modules section to root */
        config->entries[config->count].key = strdup("modules");
        config->entries[config->count].value = value;
        config->count++;
    }
    
    /* Pipeline section */
    {
        struct config_value value;
        value.type = CONFIG_VALUE_OBJECT;
        value.u.object = calloc(1, sizeof(struct config_object));
        if (value.u.object == NULL) {
            LOG_ERROR("Failed to allocate memory for pipeline section");
            config_object_free(config);
            return NULL;
        }
        
        value.u.object->capacity = 8;
        value.u.object->entries = calloc(value.u.object->capacity, sizeof(*value.u.object->entries));
        if (value.u.object->entries == NULL) {
            LOG_ERROR("Failed to allocate memory for pipeline entries");
            free(value.u.object);
            config_object_free(config);
            return NULL;
        }
        
        /* Add pipeline name */
        value.u.object->entries[value.u.object->count].key = strdup("name");
        value.u.object->entries[value.u.object->count].value.type = CONFIG_VALUE_STRING;
        value.u.object->entries[value.u.object->count].value.u.string = strdup("default");
        value.u.object->count++;
        
        /* Add use_as_global */
        value.u.object->entries[value.u.object->count].key = strdup("use_as_global");
        value.u.object->entries[value.u.object->count].value.type = CONFIG_VALUE_BOOLEAN;
        value.u.object->entries[value.u.object->count].value.u.boolean = true;
        value.u.object->count++;
        
        /* Add steps array */
        value.u.object->entries[value.u.object->count].key = strdup("steps");
        value.u.object->entries[value.u.object->count].value.type = CONFIG_VALUE_ARRAY;
        value.u.object->entries[value.u.object->count].value.u.array.capacity = 9;
        value.u.object->entries[value.u.object->count].value.u.array.items = 
            calloc(value.u.object->entries[value.u.object->count].value.u.array.capacity, 
                   sizeof(struct config_value *));
        value.u.object->entries[value.u.object->count].value.u.array.count = 0;
        
        /* Add default steps */
        const char *module_types[] = {
            "infall",
            "cooling",
            "star_formation",
            "feedback",
            "agn",
            "disk_instability",
            "mergers",
            "reincorporation",
            "misc"
        };
        
        for (int i = 0; i < 9; i++) {
            value.u.object->entries[value.u.object->count].value.u.array.items[i] = 
                calloc(1, sizeof(struct config_value));
            value.u.object->entries[value.u.object->count].value.u.array.items[i]->type = 
                CONFIG_VALUE_OBJECT;
            value.u.object->entries[value.u.object->count].value.u.array.items[i]->u.object = 
                calloc(1, sizeof(struct config_object));
            value.u.object->entries[value.u.object->count].value.u.array.items[i]->u.object->capacity = 4;
            value.u.object->entries[value.u.object->count].value.u.array.items[i]->u.object->entries = 
                calloc(value.u.object->entries[value.u.object->count].value.u.array.items[i]->u.object->capacity, 
                       sizeof(*value.u.object->entries[value.u.object->count].value.u.array.items[i]->u.object->entries));
            
            /* Add type */
            value.u.object->entries[value.u.object->count].value.u.array.items[i]->u.object->entries[0].key = 
                strdup("type");
            value.u.object->entries[value.u.object->count].value.u.array.items[i]->u.object->entries[0].value.type = 
                CONFIG_VALUE_STRING;
            value.u.object->entries[value.u.object->count].value.u.array.items[i]->u.object->entries[0].value.u.string = 
                strdup(module_types[i]);
            
            /* Add name */
            value.u.object->entries[value.u.object->count].value.u.array.items[i]->u.object->entries[1].key = 
                strdup("name");
            value.u.object->entries[value.u.object->count].value.u.array.items[i]->u.object->entries[1].value.type = 
                CONFIG_VALUE_STRING;
            value.u.object->entries[value.u.object->count].value.u.array.items[i]->u.object->entries[1].value.u.string = 
                strdup(module_types[i]);
            
            /* Add enabled */
            value.u.object->entries[value.u.object->count].value.u.array.items[i]->u.object->entries[2].key = 
                strdup("enabled");
            value.u.object->entries[value.u.object->count].value.u.array.items[i]->u.object->entries[2].value.type = 
                CONFIG_VALUE_BOOLEAN;
            value.u.object->entries[value.u.object->count].value.u.array.items[i]->u.object->entries[2].value.u.boolean = 
                true;
            
            /* Add optional */
            value.u.object->entries[value.u.object->count].value.u.array.items[i]->u.object->entries[3].key = 
                strdup("optional");
            value.u.object->entries[value.u.object->count].value.u.array.items[i]->u.object->entries[3].value.type = 
                CONFIG_VALUE_BOOLEAN;
            value.u.object->entries[value.u.object->count].value.u.array.items[i]->u.object->entries[3].value.u.boolean = 
                (i == 8); /* Only the misc step is optional */
            
            /* Update counts */
            value.u.object->entries[value.u.object->count].value.u.array.items[i]->u.object->count = 4;
            value.u.object->entries[value.u.object->count].value.u.array.count++;
        }
        
        value.u.object->count++;
        
        /* Add pipeline section to root */
        config->entries[config->count].key = strdup("pipeline");
        config->entries[config->count].value = value;
        config->count++;
    }
    
    /* Debug section */
    {
        struct config_value value;
        value.type = CONFIG_VALUE_OBJECT;
        value.u.object = calloc(1, sizeof(struct config_object));
        if (value.u.object == NULL) {
            LOG_ERROR("Failed to allocate memory for debug section");
            config_object_free(config);
            return NULL;
        }
        
        value.u.object->capacity = 8;
        value.u.object->entries = calloc(value.u.object->capacity, sizeof(*value.u.object->entries));
        if (value.u.object->entries == NULL) {
            LOG_ERROR("Failed to allocate memory for debug entries");
            free(value.u.object);
            config_object_free(config);
            return NULL;
        }
        
        /* Add debug enabled */
        value.u.object->entries[value.u.object->count].key = strdup("enabled");
        value.u.object->entries[value.u.object->count].value.type = CONFIG_VALUE_BOOLEAN;
        value.u.object->entries[value.u.object->count].value.u.boolean = false;
        value.u.object->count++;
        
        /* Add debug section to root */
        config->entries[config->count].key = strdup("debug");
        config->entries[config->count].value = value;
        config->count++;
    }
    
    return config;
}

/**
 * Create a configuration schema
 */
int config_create_schema(void) {
    /* Not implemented yet */
    return -1;
}
