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

#define AFB_BINDING_VERSION 2
#include <afb/afb-binding.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define WAIT_FOR_REMOVE(dev) { while (0 == nfc_initiator_target_is_present(dev, NULL)) {} }

static struct afb_event presence_event;
static char *current_uid = NULL;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static const nfc_modulation modulations[] = {
	{ .nmt = NMT_ISO14443A, .nbr = NBR_106 },
};

static char *to_hex_string(unsigned char *data, size_t size)
{
	char *buffer = malloc((2 * size) + 1);
	char *tmp = buffer;
	int i;

	if (buffer == NULL)
		return buffer;

	for (i = 0; i < size; i++) {
		tmp += sprintf(tmp, "%.2x", data[i]);
	}

	return buffer;
}

static char *get_tag_uid(nfc_target *nt)
{
	if (nt->nm.nmt == NMT_ISO14443A)
		return to_hex_string((unsigned char *) &nt->nti.nai.abtUid, nt->nti.nai.szUidLen);

	return NULL;
}

static void send_detect_event(char *current_id)
{
	json_object *jresp;

	if (current_id == NULL)
		return;

	jresp = json_object_new_object();

	json_object_object_add(jresp, "status", json_object_new_string("detected"));
	json_object_object_add(jresp, "uid", json_object_new_string(current_uid));

	afb_event_push(presence_event, jresp);
}

static void *nfc_loop_thread(void *ptr)
{
	nfc_context *ctx = NULL;
	nfc_device *dev = NULL;

	nfc_init(&ctx);

	dev = nfc_open(ctx, NULL);

	if (dev == NULL) {
		AFB_ERROR("Cannot get context for libnfc");
		nfc_exit(ctx);
		exit(EXIT_FAILURE);
	}

	if (nfc_initiator_init(dev) < 0) {
		AFB_ERROR("Cannot get initiator mode from libnfc");
		nfc_close(dev);
		nfc_exit(ctx);
		exit(EXIT_FAILURE);
	}

	while (1) {
		nfc_target nt;
		json_object *jresp;
		int res = nfc_initiator_poll_target(dev, modulations, ARRAY_SIZE(modulations), 0xff, 2, &nt);

		if (res < 0)
			break;

		pthread_mutex_lock(&mutex);

		current_uid = get_tag_uid(&nt);
		send_detect_event(current_uid);

		pthread_mutex_unlock(&mutex);

		WAIT_FOR_REMOVE(dev);

		pthread_mutex_lock(&mutex);

		jresp = json_object_new_object();
		json_object_object_add(jresp, "status", json_object_new_string("removed"));
		json_object_object_add(jresp, "uid", json_object_new_string(current_uid));
		afb_event_push(presence_event, jresp);

		free(current_uid);
		current_uid = NULL;

		pthread_mutex_unlock(&mutex);
	}

	nfc_close(dev);
	nfc_exit(ctx);

	return NULL;
}

static int init()
{
	pthread_t thread_id;

	presence_event = afb_daemon_make_event("presence");

	return pthread_create(&thread_id, NULL, nfc_loop_thread, NULL);
}

static void subscribe(struct afb_req request)
{
	const char *value = afb_req_value(request, "value");

	if (value && !strcasecmp(value, "presence")) {
		afb_req_subscribe(request, presence_event);
		afb_req_success(request, NULL, NULL);

		// send initial tag if exists
		pthread_mutex_lock(&mutex);
		send_detect_event(current_uid);
		pthread_mutex_unlock(&mutex);

		return;
	}

	afb_req_fail(request, "failed", "Invalid event");
}

static void unsubscribe(struct afb_req request)
{
	const char *value = afb_req_value(request, "value");

	if (value && !strcasecmp(value, "presence")) {
		afb_req_unsubscribe(request, presence_event);
		afb_req_success(request, NULL, NULL);
		return;
	}

	afb_req_fail(request, "failed", "Invalid event");
}

static const struct afb_verb_v2 binding_verbs[] = {
	{ .verb = "subscribe",   .callback = subscribe,    .info = "Subscribe to NFC events" },
	{ .verb = "unsubscribe", .callback = unsubscribe,  .info = "Unsubscribe to NFC events" },
	{ }
};

/*
 * binder API description
 */
const struct afb_binding_v2 afbBindingV2 = {
	.api		= "nfc",
	.specification	= "NFC service API",
	.verbs		= binding_verbs,
	.init		= init,
};
