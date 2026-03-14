#include <cjson/cJSON.h> /* requires libcjson-dev or the header in include path */
#include <stddef.h>

// -- GENERATE API --

/**
 * struct for holding API responses and context
 */
typedef struct {
  char *str;      // dynamically allocated string
  size_t str_len; // number of characters (not counting null terminator)

  long *arr;      // dynamically allocated int array
  size_t arr_len; // number of integers
} OllamaGenerateResponse;

void init_generate_data(OllamaGenerateResponse *d);

/**
 * TODO: document
 */
void free_generate_data(OllamaGenerateResponse *d);

/**
 * build a JSON body for /api/generate and perform the request.
 *
 * @param base_url  e.g. "http://localhost:11434" (no trailing slash)
 * @param model     model name ("llama3", etc.)
 * @param prompt    prompt text
 * @return          malloc'd response string or NULL on failure; caller frees
 */
OllamaGenerateResponse *ollama_generate(const char *base_url, const char *model,
                                        const char *prompt,
                                        const long *context_arr,
                                        size_t context_len);

// -- CHAT API --

/**
 * TODO: document
 */
typedef struct {
  char *str;      // dynamically allocated string
  size_t str_len; // number of characters (not counting null terminator)

  char *role;
  size_t role_len;
} OllamaChatMessage;

/**
 * TODO: document
 */
typedef struct {
  OllamaChatMessage *messages; // dynamically allocated array of messages
  size_t msg_count;            // number of messages in the array
} OllamaChatHistory;

/**
 * TODO: document
 */
void init_chat_history(OllamaChatHistory *history);

/**
 * TODO: document
 */
void free_chat_history(OllamaChatHistory *history);

/**
 * TODO: document
 */
void init_chat_message(OllamaChatMessage *msg);

/**
 * TODO: document
 */
void chat_append_string(OllamaChatMessage *msg, const char *str);

/**
 * TODO: document
 */
void chat_set_role(OllamaChatMessage *msg, const char *role);

/**
 * TODO: document
 */
void history_add_message(OllamaChatHistory *history, OllamaChatMessage *msg);

/**
 * TODO: document
 */
void print_chat_history(const OllamaChatHistory *history);

/**
 * TODO: document
 */
OllamaChatMessage *ollama_chat(const char *base_url, const char *model,
                               const char *role, const char *prompt,
                               OllamaChatHistory *history);

/**
 * Parse a JSON document contained in `body`.
 *
 * @param body   NUL‑terminated string containing JSON (e.g. result of
 * http_request)
 * @return       pointer to a cJSON structure, or NULL on parse failure
 *               (caller owns the returned object and must cJSON_Delete it)
 */
cJSON *parse_json_body(const char *body);