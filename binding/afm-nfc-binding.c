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
#include <nfc/nfc.h>
#include <nfc/nfc-types.h>

#define AFB_BINDING_VERSION 3
#include <afb/afb-binding.h>

#include "afm-nfc-common.h"

#define WAIT_FOR_REMOVE(dev) { while (0 == nfc_initiator_target_is_present(dev, NULL)) {} }

static afb_event_t presence_event;
static char *current_uid = NULL;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static const nfc_modulation modulations[] = {
	{ .nmt = NMT_ISO14443A, .nbr = NBR_106 },
};

static char *get_tag_uid(nfc_target *nt)
{
	if (nt->nm.nmt == NMT_ISO14443A)
		return to_hex_string((unsigned char *) &nt->nti.nai.abtUid, nt->nti.nai.szUidLen);

	return NULL;
}

static void send_detect_event(char *current_id, nfc_binding_data *data)
{
	json_object *jresp;

	if (current_id == NULL)
		return;

	jresp = json_object_new_object();

	json_object_object_add(jresp, "status", json_object_new_string("detected"));
	json_object_object_add(jresp, "uid", json_object_new_string(current_uid));

	if (data->jresp) {
		json_object_put(data->jresp);
		data->jresp = NULL;
	}

	json_object_get(jresp);
	data->jresp = jresp;

	afb_event_push(presence_event, jresp);
}

static void *nfc_loop_thread(void *ptr)
{
	nfc_binding_data *data = ptr;

	while (1) {
		nfc_target nt;
		json_object *jresp;
		int res = nfc_initiator_poll_target(data->dev, modulations, ARRAY_SIZE(modulations), 0xff, 2, &nt);

		if (res < 0)
			break;

		pthread_mutex_lock(&mutex);

		current_uid = get_tag_uid(&nt);
		send_detect_event(current_uid, data);

		pthread_mutex_unlock(&mutex);

		WAIT_FOR_REMOVE(data->dev);

		pthread_mutex_lock(&mutex);

		jresp = json_object_new_object();
		json_object_object_add(jresp, "status", json_object_new_string("removed"));
		json_object_object_add(jresp, "uid", json_object_new_string(current_uid));

		if (data->jresp) {
			json_object_put(data->jresp);
			data->jresp = NULL;
		}

		afb_event_push(presence_event, jresp);

		pthread_mutex_unlock(&mutex);
	}

	nfc_close(data->dev);
	nfc_exit(data->ctx);
	free(data);

	return NULL;
}


static nfc_binding_data *get_libnfc_instance()
{
	nfc_context *ctx = NULL;
	nfc_device *dev = NULL;
	nfc_binding_data *data;

	nfc_init(&ctx);

	dev = nfc_open(ctx, NULL);

	if (dev == NULL) {
		AFB_ERROR("Cannot get context for libnfc");
		nfc_exit(ctx);
		return NULL;
	}

	if (nfc_initiator_init(dev) < 0) {
		AFB_ERROR("Cannot get initiator mode from libnfc");
		nfc_close(dev);
		nfc_exit(ctx);
		return NULL;
	}

	data = malloc(sizeof(nfc_binding_data));

	if (data) {
		data->ctx = ctx;
		data->dev = dev;
	}

	return data;
}

static int init(afb_api_t api)
{
	pthread_t thread_id;
	nfc_binding_data *data = get_libnfc_instance();

	presence_event = afb_daemon_make_event("presence");

	if (data) {
		afb_api_set_userdata(api, data);

		return pthread_create(&thread_id, NULL, nfc_loop_thread, data);
	}

	return -ENODEV;
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
