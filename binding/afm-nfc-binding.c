/*
 * Copyright (C) 2018 Konsulko Group
 * Author: Matt Ranostay <matt.ranostay@konsulko.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * TODO: add support for NFC p2p transactions
 */

#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <glib.h>
#include <glib-object.h>
#include <json-c/json.h>
#include <neardal/neardal.h>
#define AFB_BINDING_VERSION 3
#include <afb/afb-binding.h>

#include "afm-nfc-common.h"

static afb_event_t presence_event;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void neard_cb_record_found(const char *tag_name, void *ptr)
{
	nfc_binding_data *data = ptr;
	neardal_record *record;
	int ret = neardal_get_record_properties(tag_name, &record);

	if (ret == NEARDAL_SUCCESS) {
		GVariantIter iter;
		char *s = NULL;
		GVariant *v, *value = (neardal_record_to_g_variant(record));
		json_object *jresp = json_object_new_object();
		json_object *jdict = json_object_new_object();

		g_variant_iter_init(&iter, value);
		json_object_object_add(jresp, "status", json_object_new_string("detected"));

		while (g_variant_iter_loop(&iter, "{sv}", &s, &v)) {
			gchar *str;

			if (g_strcmp0("Name", s) == 0)
				continue;

			str = g_variant_print(v, 0);
			str[strlen(str) - 1] = '\0';

			json_object_object_add(jdict, s, json_object_new_string(str + 1));

			g_free(str);
		}

		json_object_object_add(jresp, "record", jdict);

		neardal_free_record(record);

		pthread_mutex_lock(&mutex);
		data->jresp = jresp;
		json_object_get(jresp);
		pthread_mutex_unlock(&mutex);

		afb_event_push(presence_event, jresp);
	}
}

static void neard_cb_tag_removed(const char *tag_name, void *ptr)
{
	nfc_binding_data *data = ptr;
	json_object *jresp = json_object_new_object();

	pthread_mutex_lock(&mutex);
	if (data->jresp) {
		json_object_put(data->jresp);
		data->jresp = NULL;
	}
	pthread_mutex_unlock(&mutex);

	json_object_object_add(jresp, "status", json_object_new_string("removed"));

	afb_event_push(presence_event, jresp);

	g_main_loop_quit(data->loop);
}

static void *neard_loop_thread(void *ptr)
{
	nfc_binding_data *data = ptr;
	int ret;

	data->loop = g_main_loop_new(NULL, FALSE);

	neardal_set_cb_tag_lost(neard_cb_tag_removed, ptr);
	neardal_set_cb_record_found(neard_cb_record_found, ptr);

	while (1) {
		ret = neardal_start_poll(data->adapter);

		if (ret != NEARDAL_SUCCESS)
			break;

		g_main_loop_run(data->loop);
	}

	g_free(data->adapter);

	return NULL;
}

static int init(afb_api_t api)
{
	pthread_t thread_id;
	nfc_binding_data *data = NULL;
	char **adapters = NULL;
	int num_adapters, ret;

	presence_event = afb_daemon_make_event("presence");

	ret = neardal_get_adapters(&adapters, &num_adapters);

	if (ret == NEARDAL_SUCCESS) {
		ret = neardal_set_adapter_property(adapters[0], NEARD_ADP_PROP_POWERED, GINT_TO_POINTER(1));

		if (ret == NEARDAL_SUCCESS) {
			data = malloc(sizeof(nfc_binding_data));

			if (data == NULL)
				return -ENOMEM;

			afb_api_set_userdata(api, data);

			data->adapter = g_strdup(adapters[0]);
			ret = pthread_create(&thread_id, NULL, neard_loop_thread, data);
		}
	}

	neardal_free_array(&adapters);

	return 0;
}

static void subscribe(afb_req_t request)
{
	const char *value = afb_req_value(request, "value");
	afb_api_t api = afb_req_get_api(request);
	nfc_binding_data *data = afb_api_get_userdata(api);

	if (value && !strcasecmp(value, "presence")) {
		afb_req_subscribe(request, presence_event);
		afb_req_success(request, NULL, NULL);

		// send initial tag if exists
		pthread_mutex_lock(&mutex);
		if (data && data->jresp) {
			json_object_get(data->jresp);
			afb_event_push(presence_event, data->jresp);
		}
		pthread_mutex_unlock(&mutex);

		return;
	}

	afb_req_fail(request, "failed", "Invalid event");
}

static void unsubscribe(afb_req_t request)
{
	const char *value = afb_req_value(request, "value");

	if (value && !strcasecmp(value, "presence")) {
		afb_req_unsubscribe(request, presence_event);
		afb_req_success(request, NULL, NULL);
		return;
	}

	afb_req_fail(request, "failed", "Invalid event");
}

static const struct afb_verb_v3 binding_verbs[] = {
	{ .verb = "subscribe",   .callback = subscribe,    .info = "Subscribe to NFC events" },
	{ .verb = "unsubscribe", .callback = unsubscribe,  .info = "Unsubscribe to NFC events" },
	{ }
};

/*
 * binder API description
 */
const struct afb_binding_v3 afbBindingV3 = {
	.api		= "nfc",
	.specification	= "NFC service API",
	.verbs		= binding_verbs,
	.init		= init,
};
