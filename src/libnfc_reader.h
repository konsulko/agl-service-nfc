#pragma once

#include <json-c/json.h>

#define EXIT_LIBNFC_SUCCESS				0
#define EXIT_LIBNFC_NOT_INITIALIZED		1
#define EXIT_LIBNFC_NO_DEVICE_FOUND		2

int libnfc_init();
int libnfc_list_devices(struct json_object* result);
int libnfc_list_devices_capabilities(struct json_object* result, struct json_object* devices);
int libnfc_start_polling(struct json_object* result, struct json_object* devices);
