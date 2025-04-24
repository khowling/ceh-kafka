
#include <glib.h>
#include <librdkafka/rdkafka.h>
#include <stdlib.h> // For getenv()
#include <curl/curl.h> // For HTTP requests

#include "man_id.h"

/* Wrapper to set config values and error out if needed.
 */
void set_config(rd_kafka_conf_t *conf, char *key, const char *value) {
    char errstr[512];
    rd_kafka_conf_res_t res;

    res = rd_kafka_conf_set(conf, key, value, errstr, sizeof(errstr));
    if (res != RD_KAFKA_CONF_OK) {
        g_error("Unable to set config: %s", errstr);
        exit(1);
    }
}

#define ARR_SIZE(arr) ( sizeof((arr)) / sizeof((arr[0])) )

// Structure to store response data from HTTP requests
struct MemoryStruct {
    char *memory;
    size_t size;
};


// Callback for CURL to handle received data
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    
    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if(!ptr) {
        fprintf(stderr, "Not enough memory (realloc returned NULL)\n");
        return 0;
    }
    
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    
    return realsize;
}

// Function to get token from Azure Instance Metadata Service
char* get_token_from_imds(const char* resource, const char* client_id) {
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk;
    char* token = NULL;
    
    chunk.memory = malloc(1);
    chunk.size = 0;
    
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    
    if(curl) {
       
        char url[512];
        snprintf(url, sizeof(url), 
            "http://169.254.169.254/metadata/identity/oauth2/token?api-version=2018-02-01&resource=%s",  resource);
        
        if (client_id && client_id[0]) {
            char tmp[512];
            snprintf(tmp, sizeof(tmp), "%s&client_id=%s", url, client_id);
            snprintf(url, sizeof(url), tmp);
        }
        
        fprintf(stderr, "Attempting to get token from: %s\n", url);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        
        // Add required headers for IMDS
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Metadata: true");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        
        res = curl_easy_perform(curl);
        
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {

            //fprintf(stderr, "IMDS response: %s\n", chunk.memory);

            // Parse JSON response to extract token
            char *token_start = strstr(chunk.memory, "\"access_token\":\"");
            if (token_start) {
                token_start += 16; // Length of "access_token":"
                char *token_end = strchr(token_start, '\"');
                if (token_end) {
                    size_t token_len = token_end - token_start;
                    token = malloc(token_len + 1);
                    if (token) {
                        memcpy(token, token_start, token_len);
                        token[token_len] = '\0';
                        fprintf(stderr, "Extracted token (length: %zu): %.10s...\n", 
                                token_len, token); // Only print first 10 chars for security
                    } else {
                        fprintf(stderr, "Failed to allocate memory for token\n");
                    }
                } else {
                    fprintf(stderr, "Failed to find end of access_token in response\n");
                }
            } else {
                fprintf(stderr, "Failed to find access_token in response\n");
            }
        }
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    
    curl_global_cleanup();
    free(chunk.memory);
    
    return token;
}

void oauth_cb(rd_kafka_t *rk, const char *oauthbearer_config, void *opaque) {

    man_id_t *ctx = (man_id_t *)opaque;

    if (!ctx->eh_name) {
        rd_kafka_oauthbearer_set_token_failure(rk, "EH_NAME not set");
        return;// -1;
    }

    char resource[256];
    snprintf(resource, sizeof(resource), "https://%s.servicebus.windows.net/", ctx->eh_name);

    char *token = get_token_from_imds(resource, ctx->client_id);
    if (!token) {
        rd_kafka_oauthbearer_set_token_failure(rk, "Failed to retrieve token from IMDS");
        return;// -1;
    }

    // debug got token
    //fprintf(stderr, "Got token: %s\n", token);

    // Set the token in librdkafka
    char err_str[512];
    rd_kafka_resp_err_t error = rd_kafka_oauthbearer_set_token(
        rk,
        token,              // token value
        2743681274753,                 // KH: This needs to be extracted from the token
        "principal",        // principal (NULL = use default)
        NULL, 0,            // no extensions
        err_str, sizeof(err_str));          

    
    free(token);

    if (error != RD_KAFKA_RESP_ERR_NO_ERROR ) {
        fprintf(stderr, "rd_kafka_oauthbearer_set_token error: %s\n", err_str);
        //const char *err_str = rd_kafka_error_string(error);
        rd_kafka_oauthbearer_set_token_failure(rk, err_str);
        //rd_kafka_error_destroy(error);
        return;// -1;
    }

    //return 0;
}