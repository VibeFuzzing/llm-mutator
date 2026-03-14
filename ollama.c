#include "ollama.h"
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>   /* requires libcjson-dev or the header in include path */

// Ollama must be running: ollama run <model-name>

/* response‑accumulator used by the write callback */
struct curl_string {
    char *ptr;
    size_t len;
};

typedef struct {
    char *created_at;
    char *response;
} Chunk;

void init_generate_data(OllamaGenerateResponse *d) {
    d->str = NULL;
    d->str_len = 0;
    d->arr = NULL;
    d->arr_len = 0;
}

void free_generate_data(OllamaGenerateResponse *d) {
    free(d->str);
    free(d->arr);
    free(d);
}

int generate_append_string(OllamaGenerateResponse *d, const char *src) {
    size_t add_len = strlen(src);
    size_t new_len = d->str_len + add_len;

    char *tmp = realloc(d->str, new_len + 1);
    if (!tmp) return 0;

    memcpy(tmp + d->str_len, src, add_len + 1);  // includes null terminator

    d->str = tmp;
    d->str_len = new_len;
    return 1;
}

int generate_set_long_array(OllamaGenerateResponse *d, const long *src, size_t count) {
    long *tmp = NULL;

    if (count > 0) {
        tmp = malloc(count * sizeof(long));
        if (!tmp) return 0;

        memcpy(tmp, src, count * sizeof(long));
    }

    free(d->arr);

    d->arr = tmp;
    d->arr_len = count;
    return 1;
}

/**
 * @brief Create a long array object
 * 
 * @param values 
 * @param length 
 * @return cJSON* 
 */
cJSON *create_long_array(const long *values, int length) {
    cJSON *array = cJSON_CreateArray();
    if (!array) return NULL;

    for (int i = 0; i < length; i++) {
        cJSON_AddItemToArray(array, cJSON_CreateNumber(values[i]));
    }
    return array;
}

static void init_string(struct curl_string *s) {
    s->len = 0;
    s->ptr = malloc(1);
    if (s->ptr) {
        s->ptr[0] = '\0';
    }
}

void init_chat_message(OllamaChatMessage *msg) {
    msg->str = NULL;
    msg->str_len = 0;
    msg->role = NULL;
    msg->role_len = 0;
}

void chat_append_string(OllamaChatMessage *msg, const char *str) {
    size_t add_len = strlen(str);
    size_t new_len = msg->str_len + add_len;

    char *tmp = realloc(msg->str, new_len + 1);
    if (!tmp) {
        return;
    }

    memcpy(tmp + msg->str_len, str, add_len + 1);  // includes null terminator

    msg->str = tmp;
    msg->str_len = new_len;
}

void chat_set_role(OllamaChatMessage *msg, const char *role) {
    size_t role_len = strlen(role);

    char *tmp = realloc(msg->role, role_len + 1);
    if (!tmp) {
        return;
    }

    memcpy(tmp, role, role_len + 1);  // includes null terminator

    msg->role = tmp;
    msg->role_len = role_len;
}

void init_chat_history(OllamaChatHistory *history) {
    history->messages = NULL;
    history->msg_count = 0;
}

void free_chat_history(OllamaChatHistory *history) {
    if (history->messages) {
        for (size_t i = 0; i < history->msg_count; i++) {
            free(history->messages[i].str);
            free(history->messages[i].role);
        }
        free(history->messages);
    }
    free(history);
}

void history_add_message(OllamaChatHistory *history, OllamaChatMessage *msg) {
    OllamaChatMessage *tmp = realloc(history->messages, (history->msg_count + 1) * sizeof(OllamaChatMessage));
    if (!tmp) {
        return;
    }

    history->messages = tmp;
    history->messages[history->msg_count] = *msg;  // copy message struct
    history->msg_count++;
}

void print_chat_history(const OllamaChatHistory *history) {
    for (size_t i = 0; i < history->msg_count; i++) {
        printf("[%s]: %s\n", history->messages[i].role, history->messages[i].str);
    }
}

static size_t curl_writefn(void *data, size_t size, size_t nmemb, void *userp) {
    size_t tot = size * nmemb;
    struct curl_string *s = userp;
    char *newp = realloc(s->ptr, s->len + tot + 1);
    if (!newp)          /* OOM – signal error by returning 0 */
        return 0;
    s->ptr = newp;
    memcpy(s->ptr + s->len, data, tot);
    s->len += tot;
    s->ptr[s->len] = '\0';
    return tot;
}

/**
 * issue an HTTP request.
 *
 * @param method    "GET", "POST", "PUT", "DELETE", etc. (defaults to GET if NULL)
 * @param url       full URL
 * @param body      optional request body (NULL for none)
 * @return          malloc'd response body, or NULL on error
 */
char *http_request(const char *method, const char *url, const char *body) {
    CURL *curl = curl_easy_init();
    if (!curl)
        return NULL;

    struct curl_string resp;
    init_string(&resp);

    curl_easy_setopt(curl, CURLOPT_URL, url);

    if (method && *method) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
    }
    if (body) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
        /* optionally set length explicitly:
           curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(body)); */
    }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_writefn);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);

    CURLcode rc = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK) {
        free(resp.ptr);
        return NULL;
    }
    return resp.ptr;        /* caller must free() */
}

long *cjson_to_long_array(const cJSON *json_arr, size_t *out_len) {
    if (!cJSON_IsArray(json_arr)) {
        *out_len = 0;
        return NULL;
    }

    size_t count = cJSON_GetArraySize(json_arr);
    long *arr = NULL;

    if (count > 0) {
        arr = malloc(count * sizeof(long));
        if (!arr) {
            *out_len = 0;
            return NULL;
        }

        for (size_t i = 0; i < count; i++) {
            const cJSON *item = cJSON_GetArrayItem(json_arr, i);
            if (!cJSON_IsNumber(item)) {
                free(arr);
                *out_len = 0;
                return NULL;
            }
            arr[i] = (long)item->valuedouble;  // cJSON stores numbers as double
        }
    }

    *out_len = count;
    return arr;
}

/*
 * build a JSON body for /api/generate and perform the request.
 *
 * @param base_url  e.g. "http://localhost:11434" (no trailing slash)
 * @param model     model name ("llama3", etc.)
 * @param prompt    prompt text
 * @return          malloc'd response string or NULL on failure; caller frees
 */
OllamaGenerateResponse *ollama_generate(const char *base_url, const char *model, const char *prompt, const long *context_arr, size_t context_len)
{
    // TODO: need to be able to pass context as well

    if (!base_url || !model || !prompt) {
        fprintf(stderr, "Invalid input parameters\n");
        return NULL;
    }

    /* construct JSON payload */
    cJSON *root = cJSON_CreateObject();
    if (!root)
        return NULL;
    cJSON_AddStringToObject(root, "model", model);
    cJSON_AddStringToObject(root, "prompt", prompt);
    if (context_arr && context_len > 0) {
        // cJSON *context_array = cJSON_CreateLongArray(context, context_len);
        // cJSON_AddItemToObject(root, "context", context_array);
        cJSON_AddItemToObject(root, "context", create_long_array(context_arr, context_len));
    }

    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!body) {
        return NULL;
    }

    // printf("Request body:\n%s\n", body);

    /* build full URL */
    size_t len = strlen(base_url) + strlen("/api/generate") + 1;
    char *url = malloc(len);
    if (!url) {
        free(body);
        return NULL;
    }
    strcpy(url, base_url);
    strcat(url, "/api/generate");

    char *resp = http_request("POST", url, body);
    free(body);
    free(url);

    if (!resp) {
        printf("generate failed\n");
        return NULL;
    }

    /* process response */
    // OllamaGenerateResponse data;
    // init_data(&data);
    OllamaGenerateResponse *data = malloc(sizeof(OllamaGenerateResponse));
    if (!data) {
        free(resp);
        return NULL;
    }
    init_generate_data(data);


    char *words = strtok(resp, "\r\n");
    while (words) {
        cJSON *json = cJSON_Parse(words);
        if (!json) {
            words = strtok(NULL, "\r\n");
            continue;  /* skip invalid JSON lines */

        }

        // cJSON *created_at = cJSON_GetObjectItem(json, "created_at");
        cJSON *response = cJSON_GetObjectItem(json, "response");
        cJSON *done = cJSON_GetObjectItem(json, "done");

        if (!response || !done) {
            cJSON_Delete(json);
            words = strtok(NULL, "\r\n");
            continue;  /* skip if expected fields are missing */
        }

        generate_append_string(data, response->valuestring);

        if (done && done->valueint) {
            cJSON *done_reason = cJSON_GetObjectItem(json, "done_reason");
            if (done_reason && strcmp(done_reason->valuestring, "stop") != 0) {
                fprintf(stderr, "Warning: done_reason is '%s', expected 'stop'\n", done_reason->valuestring);
                return NULL;
            }
            
            cJSON *context = cJSON_GetObjectItem(json, "context");
            size_t context_len = 0;
            long *ctx = cjson_to_long_array(context, &context_len);

            generate_set_long_array(data, ctx, context_len); 
        }

        cJSON_Delete(json);
        words = strtok(NULL, "\r\n");
    }
    free(resp);
    // printf("Full response:\n%s\n", data->str);

    return data;
}


OllamaChatMessage *ollama_chat(const char *base_url, const char *model, const char *role, const char *prompt, OllamaChatHistory *history) {
    if (strcmp(role, "user") != 0 && strcmp(role, "system") != 0) {
        fprintf(stderr, "Invalid role: %s. Must be 'user' or 'system'.\n", role);
        return NULL;
    }

    /* construct JSON payload */
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        fprintf(stderr, "Failed to create JSON root object\n");
        return NULL;
    }
    cJSON_AddStringToObject(root, "model", model);

    // add new message to the end of the history array
    OllamaChatMessage *chat_data = malloc(sizeof(OllamaChatMessage));
    if (!chat_data) {
        cJSON_Delete(root);
        fprintf(stderr, "Failed to allocate memory for chat data\n");
        return NULL;
    }
    init_chat_message(chat_data);
    chat_append_string(chat_data, prompt);
    chat_set_role(chat_data, role);
    history_add_message(history, chat_data);  // add new message to history

    /* build messages array from history */
    cJSON *messages = cJSON_CreateArray();
    if (!messages) {
        cJSON_Delete(root);
        fprintf(stderr, "Failed to create JSON messages array\n");
        return NULL;
    }
    for (size_t i = 0; i < history->msg_count; i++) {
        cJSON *msg_obj = cJSON_CreateObject();
        if (!msg_obj) {
            cJSON_Delete(root);
            fprintf(stderr, "Failed to create JSON message object\n");
            return NULL;
        }
        cJSON_AddStringToObject(msg_obj, "role", history->messages[i].role);
        cJSON_AddStringToObject(msg_obj, "content", history->messages[i].str);
        cJSON_AddItemToArray(messages, msg_obj);
    }

    // add new message to the end of the array
    // cJSON *new_msg = cJSON_CreateObject();
    // if (!new_msg) {
    //     cJSON_Delete(root);
    //     fprintf(stderr, "Failed to create JSON new message object\n");
    //     return NULL;
    // }
    // cJSON_AddStringToObject(new_msg, "role", role);
    // cJSON_AddStringToObject(new_msg, "content", prompt);
    // cJSON_AddItemToArray(messages, new_msg);

    cJSON_AddItemToObject(root, "messages", messages);

    char *body = cJSON_PrintUnformatted(root);
    // printf("Request body:\n%s\n", body);
    // return NULL;
    cJSON_Delete(root);
    if (!body) {
        return NULL;
    }

    /* build full URL */
    size_t len = strlen(base_url) + strlen("/api/chat") + 1;
    char *url = malloc(len);
    if (!url) {
        free(body);
        return NULL;
    }
    strcpy(url, base_url);
    strcat(url, "/api/chat");

    char *resp = http_request("POST", url, body);
    free(body);
    free(url);

    if (!resp) {
        printf("chat failed\n");
        return NULL;
    }

    OllamaChatMessage *response_chat = malloc(sizeof(OllamaChatMessage));
    if (!response_chat) {
        free(resp);
        return NULL;
    }
    init_chat_message(response_chat);

    /* process response */
    char *lines = strtok(resp, "\r\n");
    while (lines) {
        cJSON *json = cJSON_Parse(lines);
        if (!json) {
            lines = strtok(NULL, "\r\n");
            continue;  /* skip invalid JSON lines */
        }

        cJSON *message = cJSON_GetObjectItem(json, "message");
        if (message) {
            cJSON *content = cJSON_GetObjectItem(message, "content");
            chat_append_string(response_chat, content->valuestring);
        }

        cJSON *done = cJSON_GetObjectItem(json, "done");
        if (done && done->valueint) {
            cJSON *done_reason = cJSON_GetObjectItem(json, "done_reason");
            cJSON *role = cJSON_GetObjectItem(message, "role");
            chat_set_role(response_chat, role->valuestring);
            if (done_reason && strcmp(done_reason->valuestring, "stop") != 0) {
                fprintf(stderr, "Warning: done_reason is '%s', expected 'stop'\n", done_reason->valuestring);
                free(response_chat);
                return NULL;
            }
        }

        cJSON_Delete(json);
        lines = strtok(NULL, "\r\n");
    }
    free(resp);
    history_add_message(history, response_chat);  // add response to history

    return response_chat;
}


/**
 * Parse a JSON document contained in `body`.
 *
 * @param body   NUL‑terminated string containing JSON (e.g. result of http_request)
 * @return       pointer to a cJSON structure, or NULL on parse failure
 *               (caller owns the returned object and must cJSON_Delete it)
 */
cJSON *parse_json_body(const char *body) {
    if (!body)                                 /* nothing to parse */
        return NULL;

    cJSON *root = cJSON_Parse(body);
    if (!root) {
        fprintf(stderr, "JSON parse error at %s\n", cJSON_GetErrorPtr());
        /* parsing failed; return NULL so caller can handle the error */
    }
    return root;
}



// int main() {
//     const char *OLLAMA_url = "http://localhost:11434";

//     const char *base = "http://localhost:11434";
//     // OllamaGenerateResponse *data = ollama_generate(base, "llama3", "Respond with blue next time I talk to you", NULL, 0);
//     // printf("Full response:\n%s\n", data->str);

//     OllamaChatHistory history;
//     init_chat_history(&history);

//     OllamaChatMessage *msg1 = malloc(sizeof(OllamaChatMessage));
//     init_chat_message(msg1);
//     chat_append_string(msg1, "Respond with blue next time I talk to you");
//     chat_set_role(msg1, "system");
//     history_add_message(&history, msg1);

//     OllamaChatMessage *data = ollama_chat(base, "llama3", "user", "Hello", &history);
//     print_chat_history(&history);
    
//     // printf("Full chat response:\n%s\n", data->str);

//     // OllamaGenerateResponse *data2 = ollama_generate(base, "llama3", "Hello", data->arr, data->arr_len);
//     // printf("Full response with context:\n%s\n", data2->str);

//     // free_generate_data(data);
//     // free_generate_data(data2);
//     // if (!response) {
//     //     printf("generate failed\n");
//     //     return 1;
//     // }

//     // char *words = strtok(response, "\r\n");
//     // while (words) {
//     //     cJSON *json = cJSON_Parse(words);
//     //     if (!json) {
//     //         words = strtok(NULL, "\r\n");
//     //         continue;  /* skip invalid JSON lines */

//     //     }

//     //     // put OllamaGenerateResponse init here

//     //     // cJSON *created_at = cJSON_GetObjectItem(json, "created_at");
//     //     cJSON *response = cJSON_GetObjectItem(json, "response");
//     //     cJSON *done = cJSON_GetObjectItem(json, "done");

//     //     if (!response || !done) {
//     //         cJSON_Delete(json);
//     //         words = strtok(NULL, "\r\n");
//     //         continue;  /* skip if expected fields are missing */
//     //     }

//     //     append_string(&data, response->valuestring);

//     //     // printf("%s", response->valuestring);

//     //     if (done->valueint) {
//     //         cJSON *done_reason = cJSON_GetObjectItem(json, "done_reason");
//     //         cJSON *context = cJSON_GetObjectItem(json, "context");

//     //         if (strcmp(done_reason->valuestring, "stop") != 0) {
//     //             fprintf(stderr, "Warning: done_reason is '%s', expected 'stop'\n", done_reason->valuestring);
//     //             return 1;
//     //         }
            
//     //         set_int_array(&data, context, 0); 
//     //     }

//     //     cJSON_Delete(json);
//     //     words = strtok(NULL, "\r\n");
//     // }

//     // printf("Full response:\n%s\n", data.str);

//     // // printf("\n");
//     // free(response);
//     return 0;
// }
