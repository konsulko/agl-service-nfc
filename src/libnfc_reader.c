#include "nfc-binding.h"

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

extern struct afb_event on_nfc_read_event;

#define MAX_NFC_DEVICE_COUNT	8
#define MAX_NFC_MODULATIONS		8
#define MAX_NFC_BAUDRATES 		8
#define POLL_NUMBER				0xff
#define POLL_PERIOD				0x05

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
} libnfc_context;

static libnfc_context libnfc = {
	.context			= NULL,
	.devices			= NULL,
	.devices_count		= 0
};

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

void* libnfc_reader_main(void* arg)
{
	libnfc_device* device;
	nfc_target nt;
	int polled_target_count;
	
	// Read datas
	const char* mt;
	char* field1;
	char* field2;
	char* field3;
	char* field4;
	struct json_object* result;
	
	device = (libnfc_device*)arg;
	
	while(device->device)
	{
		polled_target_count = nfc_initiator_poll_target(device->device, device->modulations, device->modulations_count, POLL_NUMBER, POLL_PERIOD, &nt);
		switch(polled_target_count)
		{
			case 0:
				AFB_INFO("libnfc: polling done with no result.");
				break;
				
			case 1:
				mt = str_nfc_modulation_type(nt.nm.nmt);
				AFB_NOTICE("libnfc: polling done with one result of type %s.", mt);
				switch(nt.nm.nmt)
				{
					case NMT_ISO14443A:
						field1 = to_hex_string(nt.nti.nai.abtAtqa, 2);
						field2 = to_hex_string(&nt.nti.nai.btSak, 1);
						field3 = to_hex_string(nt.nti.nai.abtUid, nt.nti.nai.szUidLen);
						field4 = to_hex_string(nt.nti.nai.abtAts, nt.nti.nai.szAtsLen);
						
						result = json_object_new_object();
						json_object_object_add(result, "Type", json_object_new_string(mt));
						if (field1) json_object_object_add(result, "ATQA", json_object_new_string(field1));
						if (field2) json_object_object_add(result, "SAK", json_object_new_string(field2));
						if (field3) json_object_object_add(result, "UID", json_object_new_string(field3));
						if (field4) json_object_object_add(result, "ATS", json_object_new_string(field4));
						
						break;
					case NMT_ISO14443B:
						field1 = to_hex_string(nt.nti.nbi.abtPupi, 4);
						field2 = to_hex_string(nt.nti.nbi.abtApplicationData, 4);
						field3 = to_hex_string(nt.nti.nbi.abtProtocolInfo, 3);
						field4 = to_hex_string(&nt.nti.nbi.ui8CardIdentifier, 1);
						
						result = json_object_new_object();
						json_object_object_add(result, "Type", json_object_new_string(mt));
						if (field1) json_object_object_add(result, "PUPI", json_object_new_string(field1));
						if (field2) json_object_object_add(result, "Application Data", json_object_new_string(field2));
						if (field3)json_object_object_add(result, "Protocol Info", json_object_new_string(field3));
						if (field4)json_object_object_add(result, "Card Id", json_object_new_string(field4));
						
						break;
					default:
						AFB_WARNING("libnfc: unsupported modulation type: %s.", mt);
						break;
				}
				
				if (result)
				{
					AFB_NOTICE("libnfc: push tag read event=%s", json_object_to_json_string(result));
					afb_event_push(on_nfc_read_event, result);
				}
				if (field1) free(field1);
				if (field2) free(field2);
				if (field3) free(field3);
				if (field4) free(field4);
				
				break;
				
			default:
				if (polled_target_count < 0)
					libnfc_polling_error(polled_target_count);
				else
					AFB_WARNING("libnfc: polling done with unsupported result count: %d.", polled_target_count);
				break;
		}
	}

	return NULL;
}

/// @brief Start the libnfc context.
/// @return An exit code, @c EXIT_LIBNFC_SUCCESS (zero) on success.
int libnfc_init()
{
	nfc_device* dev;
	nfc_connstring connstrings[MAX_NFC_DEVICE_COUNT];
	const nfc_modulation_type* modulations;
	const nfc_baud_rate* baudrates;
	size_t ref_device_count;
	size_t device_idx;
	size_t modulation_idx;
	
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
