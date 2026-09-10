/* Optional cJSON backend for m3g::JsonIo. */
#include "m3g.hpp"

#include "cjson/cJSON.h"

namespace m3g {
namespace {

void *cjson_create_object(void * /*user*/) { return cJSON_CreateObject(); }
void *cjson_create_array(void * /*user*/) { return cJSON_CreateArray(); }
void *cjson_create_string(char const *s, void * /*user*/) { return cJSON_CreateString(s); }
void *cjson_create_number(double v, void * /*user*/) { return cJSON_CreateNumber(v); }
void *cjson_create_bool(int v, void * /*user*/) { return cJSON_CreateBool(v); }

void cjson_add_item_to_object(void *object, char const *key, void *item, void * /*user*/) {
    cJSON_AddItemToObject(static_cast<cJSON *>(object), key, static_cast<cJSON *>(item));
}

void cjson_add_item_to_array(void *array, void *item, void * /*user*/) {
    cJSON_AddItemToArray(static_cast<cJSON *>(array), static_cast<cJSON *>(item));
}

int cjson_get_array_size(void const *array, void * /*user*/) {
    return cJSON_GetArraySize(static_cast<cJSON const *>(array));
}

void cjson_delete_node(void *node, void * /*user*/) { cJSON_Delete(static_cast<cJSON *>(node)); }

char *cjson_print_unformatted(void *node, void * /*user*/) {
    return cJSON_PrintUnformatted(static_cast<cJSON *>(node));
}

char *cjson_print_formatted(void *node, void * /*user*/) { return cJSON_Print(static_cast<cJSON *>(node)); }

void cjson_free_print(char *printed, void * /*user*/) { cJSON_free(printed); }

} // namespace

void install_cjson_json_io(void) {
    JsonIo io{};
    io.create_object = cjson_create_object;
    io.create_array = cjson_create_array;
    io.create_string = cjson_create_string;
    io.create_number = cjson_create_number;
    io.create_bool = cjson_create_bool;
    io.add_item_to_object = cjson_add_item_to_object;
    io.add_item_to_array = cjson_add_item_to_array;
    io.get_array_size = cjson_get_array_size;
    io.delete_node = cjson_delete_node;
    io.print_unformatted = cjson_print_unformatted;
    io.print_formatted = cjson_print_formatted;
    io.free_print = cjson_free_print;
    io.user = nullptr;
    set_json_io(&io);
}

namespace {
struct CjsonJsonIoAutoInstall {
    CjsonJsonIoAutoInstall() { install_cjson_json_io(); }
};
static CjsonJsonIoAutoInstall g_m3g_cjson_json_io_auto_install;
} // namespace

} // namespace m3g
