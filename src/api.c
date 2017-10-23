#include "nfc-binding.h"

/*
static const struct afb_auth nfc_auths[] = {
};
*/

static const struct afb_verb_v2 nfc_verbs[] = {
	{
		.verb = "subscribe",
		.callback = verb_subscribe,
		.auth = NULL,
		.info = NULL,
		.session = AFB_SESSION_NONE_V2
	},
	{
		.verb = "unsubscribe",
		.callback = verb_unsubscribe,
		.auth = NULL,
		.info = NULL,
		.session = AFB_SESSION_NONE_V2
	},
	{
		.verb = "list-devices",
		.callback = verb_list_devices,
		.auth = NULL,
		.info = NULL,
		.session = AFB_SESSION_NONE_V2
	},
	{
		.verb = "list-devices-capabilities",
		.callback = verb_list_devices_capabilities,
		.auth = NULL,
		.info = NULL,
		.session = AFB_SESSION_NONE_V2
	},
	{
		.verb = "start-polling",
		.callback = verb_start_polling,
		.auth = NULL,
		.info = NULL,
		.session = AFB_SESSION_NONE_V2
	},	
	{
		.verb = "stop-polling",
		.callback = verb_stop_polling,
		.auth = NULL,
		.info = NULL,
		.session = AFB_SESSION_NONE_V2
	},
	{ .verb = NULL }
};

const struct afb_binding afbBindingV2 = {
	.api = "nfc",
	.specification = NULL,
	.info = NULL,
	.verbs = nfc_verbs,
	.preinit = NULL,
	.init = init,
	.onevent = NULL,
	.noconcurrency = 0
};
