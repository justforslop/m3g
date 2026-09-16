/* Optional cJSON backend — C callbacks; C++ JsonIo installed via io_adapters_cxx.cpp. */
#include "cjson/cJSON.h"

#include <stddef.h>

void *m3g_cjson_create_object(void *user) {
    (void)user;
    return cJSON_CreateObject();
}
void *m3g_cjson_create_array(void *user) {
    (void)user;
    return cJSON_CreateArray();
}
void *m3g_cjson_create_string(char const *s, void *user) {
    (void)user;
    return cJSON_CreateString(s);
}
void *m3g_cjson_create_number(double v, void *user) {
    (void)user;
    return cJSON_CreateNumber(v);
}
void *m3g_cjson_create_bool(int v, void *user) {
    (void)user;
    return cJSON_CreateBool(v);
}
void m3g_cjson_add_item_to_object(void *object, char const *key, void *item, void *user) {
    (void)user;
    cJSON_AddItemToObject((cJSON *)object, key, (cJSON *)item);
}
void m3g_cjson_add_item_to_array(void *array, void *item, void *user) {
    (void)user;
    cJSON_AddItemToArray((cJSON *)array, (cJSON *)item);
}
int m3g_cjson_get_array_size(void const *array, void *user) {
    (void)user;
    return cJSON_GetArraySize((cJSON const *)array);
}
void m3g_cjson_delete_node(void *node, void *user) {
    (void)user;
    cJSON_Delete((cJSON *)node);
}
char *m3g_cjson_print_unformatted(void *node, void *user) {
    (void)user;
    return cJSON_PrintUnformatted((cJSON *)node);
}
char *m3g_cjson_print_formatted(void *node, void *user) {
    (void)user;
    return cJSON_Print((cJSON *)node);
}
void m3g_cjson_free_print(char *printed, void *user) {
    (void)user;
    cJSON_free(printed);
}
