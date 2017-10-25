#include "nfc-binding.h"

#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <pthread.h>

// FIXME: It compile without these lines, but KDevelop complains about pthread_t being undeclared.
#ifndef _BITS_PTHREADTYPES
typedef unsigned long int pthread_t;
#endif

#include <nfc/nfc.h>
#include "libnfc_reader.h"
#include "stringutils.h"

extern struct afb_event on_nfc_target_add_event;
extern struct afb_event on_nfc_target_remove_event;

#define MAX_NFC_DEVICE_COUNT	8
#define MAX_NFC_MODULATIONS		8
#define MAX_NFC_BAUDRATES 		8
#define POLL_NUMBER				0xA
#define POLL_PERIOD				0x7

typedef struct libnfc_device_tag
{
	pthread_t			poller;
	nfc_device*			device;
	nfc_connstring		name;
	
	nfc_modulation*		modulations;
	size_t				modulations_count;
} libnfc_device;

typedef struct libnfc_context_tag
{
	nfc_context*		context;
	libnfc_device*		devices;
	size_t				devices_count;
	struct json_object* last_target;
} libnfc_context;

static libnfc_context libnfc;

void libnfc_polling_error(int code)
{
	switch(code)
	{
	case NFC_EIO:
		AFB_ERROR("libnfc: polling failed with NFC_EIO (%d) code: Input / output error, device may not be usable anymore without re-open it!", code);
		break;
	case NFC_EINVARG:
		AFB_ERROR("libnfc: polling failed with NFC_EINVARG (%d) code: Invalid argument(s)!", code);
		break;
	case NFC_EDEVNOTSUPP:
		AFB_ERROR("libnfc: polling failed with NFC_EDEVNOTSUPP (%d) code: Operation not supported by device!", code);
		break;
	case NFC_ENOTSUCHDEV:
		AFB_ERROR("libnfc: polling failed with NFC_ENOTSUCHDEV (%d) code: No such device!", code);
		break;
	case NFC_EOVFLOW:
		AFB_ERROR("libnfc: polling failed with NFC_EOVFLOW (%d) code: Buffer overflow!", code);
		break;
	case NFC_ETIMEOUT:
		AFB_ERROR("libnfc: polling failed with NFC_ETIMEOUT (%d) code: Operation timed out!", code);
		break;
	case NFC_EOPABORTED:
		AFB_ERROR("libnfc: polling failed with NFC_EOPABORTED (%d) code: Operation aborted (by user)!", code);
		break;
	case NFC_ENOTIMPL:
		AFB_ERROR("libnfc: polling failed with NFC_ENOTIMPL (%d) code: Not (yet) implemented!", code);
		break;
	case NFC_ETGRELEASED:
		AFB_ERROR("libnfc: polling failed with NFC_ETGRELEASED (%d) code: Target released!", code);
		break;
	case NFC_ERFTRANS:
		AFB_ERROR("libnfc: polling failed with NFC_ERFTRANS (%d) code: Error while RF transmission!", code);
		break;
	case NFC_EMFCAUTHFAIL:
		AFB_ERROR("libnfc: polling failed with NFC_EMFCAUTHFAIL (%d) code: MIFARE Classic: authentication failed!", code);
		break;
	case NFC_ESOFT:
		AFB_ERROR("libnfc: polling failed with NFC_ESOFT (%d) code: Software error (allocation, file/pipe creation, etc.)!", code);
		break;
	case NFC_ECHIP:
		AFB_ERROR("libnfc: polling failed with NFC_ECHIP (%d) code: Device's internal chip error!", code);
		break;
	default:
		AFB_ERROR("libnfc: polling failed with unknown code: %d!", code);
		break;
	}
}

void add_nfc_field(struct json_object* parent, const char* field, const void* src, size_t sz)
{
	char* data;
	
	if (parent && field && src && sz)
	{
		data = to_hex_string(src, sz);
		if (data)
		{
			json_object_object_add(parent, field, json_object_new_string(data));
			free(data);
		}
	}
}

struct json_object* read_target(const nfc_target* target)
{
	struct json_object*	result;
	const char*			mt;
	
	if (!target)
	{
		AFB_WARNING("libnfc: No target to read!");
		return NULL;
	}
	
	result = json_object_new_object();
	mt = str_nfc_modulation_type(target->nm.nmt);
	json_object_object_add(result, "Type", json_object_new_string(mt));
	
	switch(target->nm.nmt)
	{
		case NMT_ISO14443A:
			add_nfc_field(result, "ATQA", target->nti.nai.abtAtqa, 2);
			add_nfc_field(result, "SAK", &target->nti.nai.btSak, 1);
			add_nfc_field(result, "UID", target->nti.nai.abtUid, target->nti.nai.szUidLen);
			add_nfc_field(result, "ATS", target->nti.nai.abtAts, target->nti.nai.szAtsLen);
			
			break;
		case NMT_ISO14443B:
			add_nfc_field(result, "PUPI", target->nti.nbi.abtPupi, 4);
			add_nfc_field(result, "Application Data", target->nti.nbi.abtApplicationData, 4);
			add_nfc_field(result, "Protocol Info", target->nti.nbi.abtProtocolInfo, 3);
			add_nfc_field(result, "Card Id", &target->nti.nbi.ui8CardIdentifier, 1);
			
			break;
		default:
			AFB_WARNING("libnfc: unsupported modulation type: %s.", mt);
			json_object_object_add(result, "error", json_object_new_string("unsupported tag type"));
			break;
	}
	return result;
}

void* libnfc_reader_main(void* arg)
{
	libnfc_device* device;
	nfc_target nt;
	int polled_target_count;
	nfc_modulation mods[MAX_NFC_MODULATIONS];
	struct json_object* result;
	size_t i, j;
	
	device = (libnfc_device*)arg;
	
	memset(mods, 0, sizeof(nfc_modulation) * MAX_NFC_MODULATIONS);
	for(i = 0, j = 0; i < device->modulations_count; ++i, ++j)
	{
		if (device->modulations[i].nmt != NMT_DEP)
			mods[j] = device->modulations[i];
		else --j;
	}
	
	while(device->device)
	{
		polled_target_count = nfc_initiator_poll_target
		(
			device->device,
			mods,
			j,
			POLL_NUMBER,
			POLL_PERIOD,
			&nt
		);
		
		switch(polled_target_count)
		{
		case 0:
			// No target detected
			AFB_INFO("libnfc: polling done with no result.");
			if (libnfc.last_target)
			{
				AFB_NOTICE("libnfc: tag removed = %s", json_object_to_json_string(libnfc.last_target));
				afb_event_push(on_nfc_target_remove_event, libnfc.last_target);
				libnfc.last_target = NULL;
			}
			break;
			
		case 1:
			AFB_INFO("libnfc: polling done with one result.");
			// One target detected
			result = read_target(&nt);
			
			if (libnfc.last_target)
			{
				if (strcmp(json_object_to_json_string(result), json_object_to_json_string(libnfc.last_target)))
				{
					AFB_NOTICE("libnfc: tag removed = %s", json_object_to_json_string(libnfc.last_target));
					afb_event_push(on_nfc_target_remove_event, libnfc.last_target);
					libnfc.last_target = NULL;
				}
			}
			
			if (!libnfc.last_target)
			{
				json_object_get(result);
				libnfc.last_target = result;
				
				AFB_NOTICE("libnfc: tag added = %s", json_object_to_json_string(result));
				afb_event_push(on_nfc_target_add_event, result);
			}
			break;
			
		default:
			if (polled_target_count < 0) libnfc_polling_error(polled_target_count);
			else AFB_WARNING("libnfc: polling done with unsupported result count: %d.", polled_target_count);
			
			// Consider target is removed
			if (libnfc.last_target)
			{
				AFB_NOTICE("libnfc: tag removed = %s", json_object_to_json_string(libnfc.last_target));
				afb_event_push(on_nfc_target_remove_event, libnfc.last_target);
				libnfc.last_target = NULL;
			}
			break;
		}
	}
	return NULL;
}

void sigterm_handler(int sig)
{
	size_t i;
	nfc_device* dev;
	if (sig == SIGTERM && libnfc.context && libnfc.devices_count)
	{
		for(i = 0; i < libnfc.devices_count; ++i)
		{
			if (libnfc.devices[i].device)
			{
				dev = libnfc.devices[i].device;
				libnfc.devices[i].device = NULL;
				nfc_close(dev);
			}
		}
		nfc_exit(libnfc.context);
		libnfc.context = NULL;
	}
}

/// @brief Start the libnfc context.
/// @return An exit code, @c EXIT_LIBNFC_SUCCESS (zero) on success.
int libnfc_init()
{
	nfc_device* dev;
	const nfc_modulation_type* modulations;
	const nfc_baud_rate* baudrates;
	size_t modulation_idx;
	nfc_connstring connstrings[MAX_NFC_DEVICE_COUNT];
	size_t ref_device_count;
	size_t device_idx;
	
	
	memset(&libnfc, 0, sizeof(libnfc_context));
	
	nfc_init(&libnfc.context);
	if (libnfc.context == NULL)
	{
		AFB_ERROR("[libnfc] Initialization failed (malloc)!");
		return EXIT_LIBNFC_NOT_INITIALIZED;
	}
	
	AFB_NOTICE("[libnfc] Using libnfc version: %s.", nfc_version());
	
	// Find and register devices
	ref_device_count = nfc_list_devices(libnfc.context, connstrings, MAX_NFC_DEVICE_COUNT);
	if (!ref_device_count)
	{
		AFB_ERROR("libnfc: No NFC device found!");
		return EXIT_LIBNFC_NO_DEVICE_FOUND;
	}
	libnfc.devices_count = ref_device_count;
	libnfc.devices = malloc(sizeof(libnfc_device) * libnfc.devices_count);
	memset(libnfc.devices, 0, sizeof(libnfc_device) * libnfc.devices_count);
	
	signal(SIGTERM, sigterm_handler);
	
	for(device_idx = 0; device_idx < ref_device_count; ++device_idx)
	{
		AFB_NOTICE("libnfc: NFC Device found: \"%s\".", connstrings[device_idx]);
		strcpy(libnfc.devices[device_idx].name, connstrings[device_idx]);

		// Find and register modulations
		dev = nfc_open(libnfc.context, connstrings[device_idx]);
		if (dev)
		{
			if (nfc_device_get_supported_modulation(dev, N_INITIATOR, &modulations))
			{
				AFB_ERROR("libnfc: Failed to get supported modulations from '%s'!", connstrings[device_idx]);
			}
			else
			{
				// Find and register modulations
				modulation_idx = 0;
				while(modulations[modulation_idx]) ++modulation_idx;
				libnfc.devices[device_idx].modulations_count = modulation_idx;
				if (modulation_idx)
				{
					libnfc.devices[device_idx].modulations = malloc(sizeof(nfc_modulation) * modulation_idx);
					memset(libnfc.devices[device_idx].modulations, 0, sizeof(nfc_modulation) * modulation_idx);
					
					modulation_idx = 0;
					while(modulations[modulation_idx])
					{
						libnfc.devices[device_idx].modulations[modulation_idx].nmt = modulations[modulation_idx];
						if (!nfc_device_get_supported_baud_rate(dev, modulations[modulation_idx], &baudrates))
						{
							 // Keep only the first speed which is supposed to be the fastest
							libnfc.devices[device_idx].modulations[modulation_idx].nbr = baudrates[0];
						}
						
						AFB_NOTICE("libnfc:    - Modulation '%s' supported at '%s'."
							, str_nfc_modulation_type(libnfc.devices[device_idx].modulations[modulation_idx].nmt)
							, str_nfc_baud_rate(libnfc.devices[device_idx].modulations[modulation_idx].nbr));
						++modulation_idx;
					}
				}
			}
			nfc_close(dev);
		}
	}
	
	return EXIT_LIBNFC_SUCCESS;
}

/// @brief List devices founds by libnfc.
/// @param[in] result A json object array into which found devices are added.
/// @return An exit code, @c EXIT_LIBNFC_SUCCESS (zero) on success.
int libnfc_list_devices(struct json_object* result)
{
	struct json_object* device;
	size_t i;
	
	for(i = 0; i < libnfc.devices_count; ++i)
	{
		device = json_object_new_object();
		json_object_object_add(device, "source", json_object_new_string("libnfc"));
		json_object_object_add(device, "name", json_object_new_string(libnfc.devices[i].name));
		json_object_array_add(result, device);
	}
	
	return EXIT_LIBNFC_SUCCESS;
}

int libnfc_list_devices_capabilities(struct json_object* result, struct json_object* devices)
{
	struct json_object* device;
	struct json_object* mods;
	struct json_object* mod;
	size_t i, j;
	
	for(i = 0; i < libnfc.devices_count; ++i)
	{
		device = json_object_new_object();
		json_object_object_add(device, "source", json_object_new_string("libnfc"));
		json_object_object_add(device, "name", json_object_new_string(libnfc.devices[i].name));
		mods = json_object_new_array();
		
		for(j = 0; j < libnfc.devices[i].modulations_count; ++j)
		{
			mod = json_object_new_object();
			json_object_object_add(mod, "modulation", json_object_new_string(str_nfc_modulation_type(libnfc.devices[i].modulations[j].nmt)));
			json_object_object_add(mod, "baudrate", json_object_new_string(str_nfc_baud_rate(libnfc.devices[i].modulations[j].nbr)));
			json_object_array_add(mods, mod);
		}
		
		json_object_object_add(device, "modulations", mods);
		json_object_array_add(result, device);
	}
	
	return EXIT_LIBNFC_SUCCESS;
}

int libnfc_start_polling(struct json_object* result, struct json_object* devices)
{
	struct json_object* device;
	size_t i;
	int r;
	
	for(i = 0; i < libnfc.devices_count; ++i)
	{
		device = json_object_new_object();
		json_object_object_add(device, "source", json_object_new_string("libnfc"));
		json_object_object_add(device, "name", json_object_new_string(libnfc.devices[i].name));
		if (libnfc.devices[i].device)
		{
			json_object_object_add(device, "status", json_object_new_string("already polling"));
			AFB_NOTICE("libnfc: Device '%s' is already polling.", libnfc.devices[i].name);
		}
		else
		{
			libnfc.devices[i].device = nfc_open(libnfc.context, libnfc.devices[i].name);
			if (libnfc.devices[i].device)
			{
				if (nfc_initiator_init(libnfc.devices[i].device) < 0)
				{
					nfc_close(libnfc.devices[i].device);
					libnfc.devices[i].device = NULL;
					json_object_object_add(device, "status", json_object_new_string("failed to set initiator mode"));
					AFB_ERROR("libnfc: nfc_initiator_init failedfor device '%s'!", libnfc.devices[i].name);
				}
				else
				{
					r = pthread_create(&libnfc.devices[i].poller, NULL, libnfc_reader_main, (void*)&libnfc.devices[i]);
					if (r)
					{
						nfc_close(libnfc.devices[i].device);
						libnfc.devices[i].device = NULL;
						json_object_object_add(device, "status", json_object_new_string("failed to create the polling thread"));
						AFB_ERROR("libnfc: pthread_create failed!");
					}
					else
					{
						json_object_object_add(device, "status", json_object_new_string("polling"));
						AFB_NOTICE("libnfc: Polling the device '%s'.", libnfc.devices[i].name);
					}
				}
			}
			else
			{
				json_object_object_add(device, "status", json_object_new_string("failed to open device"));
				AFB_ERROR("libnfc: Failed to open device '%s'!", libnfc.devices[i].name);
			}
		}
		json_object_array_add(result, device);
	}
	
	return EXIT_LIBNFC_SUCCESS;
}
