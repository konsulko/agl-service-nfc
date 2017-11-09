#include "nfc-binding.h"

#if USE_LIBNFC == 1
#include "libnfc_reader.h"
#endif

struct afb_event on_nfc_target_add_event;
struct afb_event on_nfc_target_remove_event;

/// @brief Binding's initialization.
/// @return Exit code, zero on success, non-zero otherwise.
int init()
{
	on_nfc_target_add_event = afb_daemon_make_event("on-nfc-target-add");
	on_nfc_target_remove_event = afb_daemon_make_event("on-nfc-target-remove");
	if (!afb_event_is_valid(on_nfc_target_add_event) || !afb_event_is_valid(on_nfc_target_remove_event))
	{
		AFB_ERROR("Failed to create a valid event!");
		return 1;
	}
	
#if USE_LIBNFC == 1
	if (libnfc_init())
	{
		AFB_ERROR("Failed start libnfc reader!");
		return 2;
	}
#endif
	
	return 0;
}

/// @brief Get a list of devices.
/// @param[in] req The query.
void verb_subscribe(struct afb_req req)
{	
	if (!afb_req_subscribe(req, on_nfc_target_remove_event))
	{
		if (!afb_req_subscribe(req, on_nfc_target_add_event))
		{
			afb_req_success(req, NULL, "Subscription success!");
			return;
		}
		else afb_req_unsubscribe(req, on_nfc_target_remove_event);
	}
	
	afb_req_fail(req, NULL, "Subscription failure!");
}

/// @brief Get a list of devices.
/// @param[in] req The query.
void verb_unsubscribe(struct afb_req req)
{
	if (!afb_req_unsubscribe(req, on_nfc_target_add_event))
	{
		if (!afb_req_unsubscribe(req, on_nfc_target_remove_event))
		{
			afb_req_success(req, NULL, "Unsubscription success!");
			return;
		}
	}
	
	afb_req_fail(req, NULL, "Unsubscription failure!");
}

/// @brief Get a list of devices.
/// @param[in] req The query.
void verb_list_devices(struct afb_req req)
{
	struct json_object* result;
	
	result = json_object_new_array();

#if USE_LIBNFC == 1
	if (libnfc_list_devices(result))
	{
		afb_req_fail(req, "Failed to get devices list from libnfc!", NULL);
		return;
	}
#endif
	
	afb_req_success(req, result, NULL);
}

/// @brief Get a list of devices capabilities.
/// @param[in] req The query.
void verb_list_devices_capabilities(struct afb_req req)
{
	struct json_object* result;
	struct json_object* arg;
	
	arg = afb_req_json(req);
	
	result = json_object_new_array();

#if USE_LIBNFC == 1
	if (libnfc_list_devices_capabilities(result, arg))
	{
		afb_req_fail(req, "Failed to get devices list from libnfc!", NULL);
		return;
	}
#endif
	
	afb_req_success(req, result, NULL);
}

/// @brief Start polling.
/// @param[in] req The query.
void verb_start(struct afb_req req)
{
	struct json_object* result;
	struct json_object* arg;
	
	arg = afb_req_json(req);
	
	result = json_object_new_array();

#if USE_LIBNFC == 1
	if (libnfc_start_polling(result, arg))
	{
		afb_req_fail(req, "Failed to get devices list from libnfc!", NULL);
		return;
	}
#endif
	
	afb_req_success(req, result, NULL);
}

/// @brief Stop polling.
/// @param[in] req The query.
void verb_stop(struct afb_req req)
{
	afb_req_fail(req, "Not implemented yet!", NULL);
}
