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

#define STR(X) STR1(X)
#define STR1(X) #X

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static afb_event_t presence_event;

static void __attribute__((unused)) dbg_dump_tag_records(neardal_tag *tag)
{
	char **recs = tag->records;
	int i;

	if (!recs) {
		AFB_API_DEBUG(afbBindingV3root, "tag empty!");
		return;
	}

	for (i = 0; recs[i] != NULL; i++)
		AFB_API_DEBUG(afbBindingV3root, "tag record[n]: %s", recs[i]);

	return;
}


static void __attribute__((unused)) dbg_dump_record_content(neardal_record record)
{
#define DBG_RECORD(__x) if (record.__x)  \
	AFB_API_DEBUG(afbBindingV3root, "record %s=%s", STR(__x), (record.__x))

	DBG_RECORD(authentication);
	DBG_RECORD(representation);
	DBG_RECORD(passphrase);
	DBG_RECORD(encryption);
	DBG_RECORD(encoding);
	DBG_RECORD(language);
	DBG_RECORD(action);
	DBG_RECORD(mime);
	DBG_RECORD(type);
	DBG_RECORD(ssid);
	DBG_RECORD(uri);
}

static void record_found(const char *tag_name, void *ptr)
{
	json_object *jresp, *jdict, *jtemp;
	nfc_binding_data *data = ptr;
	neardal_record *record;
	GVariant *value, *v = NULL;
	GVariantIter iter;
	char *s = NULL;
	int ret;

	ret = neardal_get_record_properties(tag_name, &record);
	if (ret != NEARDAL_SUCCESS) {
		AFB_API_ERROR(afbBindingV3root,
			      "read record properties for %s tag, err:0x%x (%s)",
			      tag_name, ret, neardal_error_get_text(ret));
		return;
	}

	value = neardal_record_to_g_variant(record);
	jresp = json_object_new_object();
	jdict = json_object_new_object();

	g_variant_iter_init(&iter, value);
	json_object_object_add(jresp,
			       "status", json_object_new_string("detected"));

	while (g_variant_iter_loop(&iter, "{sv}", &s, &v)) {
		gchar *str = g_variant_print(v, 0);

		str[strlen(str) - 1] = '\0';
		AFB_API_DEBUG(afbBindingV3root,
			      "%s tag, record %s= %s", tag_name, s, str);

		json_object_object_add(jdict, s,
                                       json_object_new_string(str + 1));
		g_free(str);
	}

	neardal_free_record(record);

	/*
	 * get record dictionary and look for 'Representation' field which should
	 * contain the uid value the identity agent needs.
	 *
	 */
	if (json_object_object_get_ex(jdict, "Representation", &jtemp)) {
		const char *uid = json_object_get_string(jtemp);

		pthread_mutex_lock(&mutex);
		data->jresp = jresp;
		json_object_object_add(jresp, "uid", json_object_new_string(uid));
		pthread_mutex_unlock(&mutex);

		afb_event_push(presence_event, jresp);
		AFB_API_DEBUG(afbBindingV3root,
			      "sent presence event, record content %s, tag %s",
			      uid, tag_name);
	}

	return;
}

static void tag_found(const char *tag_name, void *ptr)
{
	neardal_tag *tag;
	int ret;

	ret = neardal_get_tag_properties(tag_name, &tag);
	if (ret != NEARDAL_SUCCESS) {
		AFB_API_ERROR(afbBindingV3root,
			      "read tag %s properties, err:0x%x (%s)",
			      tag_name, ret, neardal_error_get_text(ret));
		return;
	}

	dbg_dump_tag_records(tag);
	neardal_free_tag(tag);

	return;
}

static void tag_removed(const char *tag_name, void *ptr)
{
	nfc_binding_data *data = ptr;
	json_object *jresp;

	pthread_mutex_lock(&mutex);
	if (data->jresp) {
		json_object_put(data->jresp);
		data->jresp = NULL;
	}
	pthread_mutex_unlock(&mutex);

	jresp = json_object_new_object();
	json_object_object_add(jresp, "status",
			       json_object_new_string("removed"));
	afb_event_push(presence_event, jresp);

	AFB_API_DEBUG(afbBindingV3root, "%s tag removed, quit loop", tag_name);
	g_main_loop_quit(data->loop);
}

static void *poll_adapter(void *ptr)
{
	nfc_binding_data *data = ptr;
	int ret;

	data->loop = g_main_loop_new(NULL, FALSE);

	neardal_set_cb_tag_found(tag_found, ptr);
	neardal_set_cb_tag_lost(tag_removed, ptr);
	neardal_set_cb_record_found(record_found, ptr);

	ret = neardal_set_adapter_property(data->adapter,
					   NEARD_ADP_PROP_POWERED,
					   GINT_TO_POINTER(1));
	if (ret != NEARDAL_SUCCESS) {
		AFB_API_DEBUG(afbBindingV3root,
			      "failed to power %s adapter on: ret=0x%x (%s)",
			      data->adapter, ret, neardal_error_get_text(ret));

		goto out;
	}

	for (;;) {
		ret = neardal_start_poll(data->adapter);

		if ((ret != NEARDAL_SUCCESS) &&
		    (ret != NEARDAL_ERROR_POLLING_ALREADY_ACTIVE))
			break;

		g_main_loop_run(data->loop);
	}

	AFB_API_DEBUG(afbBindingV3root, "exiting polling loop");

out:
	g_free(data->adapter);
	free(data);

	return NULL;
}

static int get_adapter(nfc_binding_data *data, unsigned int id)
{
	char **adapters = NULL;
	int num_adapters, ret;

	ret = neardal_get_adapters(&adapters, &num_adapters);
	if (ret != NEARDAL_SUCCESS) {
		AFB_API_DEBUG(afbBindingV3root,
			      "failed to find adapters ret=0x%x (%s)",
			      ret, neardal_error_get_text(ret));
		return -EIO;
	}

	if (id > num_adapters - 1) {
		AFB_API_DEBUG(afbBindingV3root,
			      "adapter out of range (%d - %d)",
			      id, num_adapters);
		ret = -EINVAL;
		goto out;
	}

	data->adapter = g_strdup(adapters[id]);

out:
	neardal_free_array(&adapters);
	return ret;
}

static int init(afb_api_t api)
{
	nfc_binding_data *data;
	pthread_t thread_id;
	int ret;

	data = malloc(sizeof(nfc_binding_data));
	if (!data)
		return -ENOMEM;

	presence_event = afb_api_make_event(api, "presence");
	if (!afb_event_is_valid(presence_event)) {
		AFB_API_ERROR(api, "Failed to create event");
		free(data);
		return -EINVAL;
	}

	/* get the first adapter */
	ret = get_adapter(data, 0);
	if (ret != NEARDAL_SUCCESS) {
		free(data);
		return 0;  /* ignore error if no adapter */
	}

	AFB_API_DEBUG(api, " %s adapter found", data->adapter);

	afb_api_set_userdata(api, data);
	ret = pthread_create(&thread_id, NULL, poll_adapter, data);
	if (ret) {
		AFB_API_ERROR(api, "polling pthread creation failed");
		g_free(data->adapter);
		free(data);
	}

	return 0;
}

static void subscribe(afb_req_t request)
{
        if (afb_req_subscribe(request, presence_event) < 0) {
                AFB_REQ_ERROR(request, "subscribe to presence_event failed");
                afb_req_reply(request, NULL, "failed", "Invalid event");

		return;
        }

	afb_req_reply(request, NULL, NULL, NULL);
}

static void unsubscribe(afb_req_t request)
{
        if (afb_req_unsubscribe(request, presence_event) < 0) {
                AFB_REQ_ERROR(request, "unsubscribe to presence_event failed");
                afb_req_reply(request, NULL, "failed", "Invalid event");

		return;
	}

	afb_req_reply(request, NULL, NULL, NULL);
}

static const afb_verb_t binding_verbs[] = {
        { .verb = "subscribe",   .callback = subscribe, 
	  .info = "Subscribe to NFC events" },
        { .verb = "unsubscribe", .callback = unsubscribe, 
	  .info = "Unsubscribe to NFC events" },
        { .verb = NULL }
};

/*
 * binder API description
 */
const afb_binding_t afbBindingExport = {
        .api            = "nfc",
        .specification  = "NFC service API",
        .info           = "AGL nfc service",
        .verbs          = binding_verbs,
        .preinit        = NULL,
        .init           = init,
        .onevent        = NULL,
        .noconcurrency  = 0
};
