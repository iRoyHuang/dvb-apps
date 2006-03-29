/*
	CA-ZAP utility

	Copyright (C) 2004, 2005 Manu Abraham (manu@kromtek.com)
	Copyright (C) 2006 Andrew de Quincey (adq_dvb@lidskialf.net)

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU Lesser General Public License as
	published by the Free Software Foundation; either version 2.1 of
	the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
*/

#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <dvben50221/en50221_session.h>
#include <dvben50221/en50221_transport.h>
#include <dvben50221/en50221_app_utils.h>
#include <dvben50221/en50221_app_ai.h>
#include <dvben50221/en50221_app_ca.h>
#include <dvben50221/en50221_app_mmi.h>
#include <dvben50221/en50221_app_rm.h>
#include "ca_zap.h"
#include "ca_zap_llci.h"


// resource IDs we send to the CAM.
uint32_t resource_ids[] = { EN50221_APP_RM_RESOURCEID,
  		            EN50221_APP_CA_RESOURCEID,
			    EN50221_APP_AI_RESOURCEID, };
int resource_ids_count = sizeof(resource_ids)/4;

// resource id function table
struct resource {
	struct en50221_app_public_resource_id resid;
	en50221_sl_resource_callback callback;
	void *arg;
};
struct resource resources[20];
int resources_count = 0;

// instances of private resources
en50221_app_rm rm_resource;
en50221_app_ai ai_resource;

// session numbers of connections to resources
int ai_session_number;

// misc stack related stuff
en50221_transport_layer tl;
en50221_session_layer sl;
struct en50221_app_send_functions sendfuncs;
int slot_id = -1;
int lasterror = 0;

// function declarations
static int llci_lookup_callback(void *arg, uint8_t slot_id, uint32_t resource_id, en50221_sl_resource_callback *callback_out, void **arg_out);
static int llci_session_callback(void *arg, int reason, uint8_t slot_id, uint16_t session_number, uint32_t resource_id);
static int llci_rm_enq_callback(void *arg, uint8_t slot_id, uint16_t session_number);
static int llci_rm_reply_callback(void *arg, uint8_t slot_id, uint16_t session_number, uint32_t resource_id_count, uint32_t *resource_ids);
static int llci_rm_changed_callback(void *arg, uint8_t slot_id, uint16_t session_number);
static int llci_ai_callback(void *arg, uint8_t slot_id, uint16_t session_number,
			    uint8_t application_type, uint16_t application_manufacturer,
			    uint16_t manufacturer_code, uint8_t menu_string_length,
			    uint8_t *menu_string);



int llci_init()
{
	// create transport layer
	tl = en50221_tl_create(1, 32);
	if (tl == NULL) {
		fprintf(stderr, "Failed to create transport layer\n");
		return -1;
	}

	// create session layer
	sl = en50221_sl_create(tl, 256);
	if (sl == NULL) {
		fprintf(stderr, "Failed to create session layer\n");
		return -1;
	}

	// create the sendfuncs
	sendfuncs.arg        = sl;
	sendfuncs.send_data  = en50221_sl_send_data;
	sendfuncs.send_datav = en50221_sl_send_datav;

	// create the resource manager resource
	rm_resource = en50221_app_rm_create(&sendfuncs);
	en50221_app_decode_public_resource_id(&resources[resources_count].resid, EN50221_APP_RM_RESOURCEID);
	resources[resources_count].callback = en50221_app_rm_message;
	resources[resources_count].arg = rm_resource;
	en50221_app_rm_register_enq_callback(rm_resource, llci_rm_enq_callback, NULL);
	en50221_app_rm_register_reply_callback(rm_resource, llci_rm_reply_callback, NULL);
	en50221_app_rm_register_changed_callback(rm_resource, llci_rm_changed_callback, NULL);
	resources_count++;

	// create the application information resource
	ai_resource = en50221_app_ai_create(&sendfuncs);
	en50221_app_decode_public_resource_id(&resources[resources_count].resid, EN50221_APP_AI_RESOURCEID);
	resources[resources_count].callback = en50221_app_ai_message;
	resources[resources_count].arg = ai_resource;
	en50221_app_ai_register_callback(ai_resource, llci_ai_callback, NULL);
	resources_count++;

	// create the CA resource
	ca_resource = en50221_app_ca_create(&sendfuncs);
	en50221_app_decode_public_resource_id(&resources[resources_count].resid, EN50221_APP_CA_RESOURCEID);
	resources[resources_count].callback = en50221_app_ca_message;
	resources[resources_count].arg = ca_resource;
	resources_count++;

	// register callbacks
	en50221_sl_register_lookup_callback(sl, llci_lookup_callback, sl);
	en50221_sl_register_session_callback(sl, llci_session_callback, sl);

	return 0;
}

int llci_cam_added(int cafd)
{
	if (slot_id != -1) {
		llci_cam_removed();
	}

	// register the slot
	if ((slot_id = en50221_tl_register_slot(tl, cafd, 1000, 100)) < 0) {
		return -1;
	}

	// create a new connection on the slot
	if (en50221_tl_new_tc(tl, slot_id)) {
		llci_cam_removed();
		return -1;
	}

	// ok!
	return 0;
}

void llci_cam_removed()
{
	if (slot_id != -1) {
		en50221_tl_destroy_slot(tl, slot_id);
		lasterror = 0;
		slot_id = -1;
	}
}

void llci_poll()
{
	// poll the stack
	int error;
	if ((error = en50221_tl_poll(tl)) != 0) {
		if (error != lasterror) {
			fprintf(stderr, "Error reported by stack:%i\n", en50221_tl_get_error(tl));
		}
		lasterror = error;
	}
}

void llci_shutdown()
{
	// destroy slot
	llci_cam_removed();

	// destroy session layer
	en50221_sl_destroy(sl);

	// destroy transport layer
	en50221_tl_destroy(tl);
}

static int llci_lookup_callback(void *arg, uint8_t slot_id, uint32_t resource_id, en50221_sl_resource_callback *callback_out, void **arg_out)
{
	struct en50221_app_public_resource_id resid;
	(void) arg;
	(void) slot_id;

	// decode the resource id
	if (!en50221_app_decode_public_resource_id(&resid, resource_id)) {
		return -1;
	}

	// try and find an instance of the resource
	int i;
	for(i=0; i<resources_count; i++) {
		if ((resid.resource_class == resources[i].resid.resource_class) &&
		    (resid.resource_type == resources[i].resid.resource_type)) {
			*callback_out = resources[i].callback;
			*arg_out = resources[i].arg;
			return 0;
		}
	}

	return -1;
}

static int llci_session_callback(void *arg, int reason, uint8_t slot_id, uint16_t session_number, uint32_t resource_id)
{
	(void) arg;
	(void) slot_id;

	switch(reason) {
	case S_SCALLBACK_REASON_CAMCONNECTED:
		if (resource_id == EN50221_APP_RM_RESOURCEID) {
			en50221_app_rm_enq(rm_resource, session_number);
		} else if (resource_id == EN50221_APP_AI_RESOURCEID) {
			en50221_app_ai_enquiry(ai_resource, session_number);
			ai_session_number = session_number;
		} else if (resource_id == EN50221_APP_CA_RESOURCEID) {
			en50221_app_ca_info_enq(ca_resource, session_number);
			ca_session_number = session_number;
		}

		break;
	}
	return 0;
}

static int llci_rm_enq_callback(void *arg, uint8_t slot_id, uint16_t session_number)
{
	(void) arg;
	(void) slot_id;

	if (en50221_app_rm_reply(rm_resource, session_number, resource_ids_count, resource_ids)) {
		printf("Failed to send reply to ENQ\n");
	}
	return 0;
}

static int llci_rm_reply_callback(void *arg, uint8_t slot_id, uint16_t session_number, uint32_t resource_id_count, uint32_t *resource_ids)
{
	(void) arg;
	(void) slot_id;
	(void) resource_id_count;
	(void) resource_ids;

	if (en50221_app_rm_changed(rm_resource, session_number)) {
		printf("Failed to send REPLY\n");
	}
	return 0;
}

static int llci_rm_changed_callback(void *arg, uint8_t slot_id, uint16_t session_number)
{
	(void) arg;
	(void) slot_id;

	if (en50221_app_rm_enq(rm_resource, session_number)) {
		printf("Failed to send ENQ\n");
	}
	return 0;
}

static int llci_ai_callback(void *arg, uint8_t slot_id, uint16_t session_number,
			    uint8_t application_type, uint16_t application_manufacturer,
			    uint16_t manufacturer_code, uint8_t menu_string_length,
			    uint8_t *menu_string)
{
	(void) arg;
	(void) slot_id;
	(void) session_number;

	printf("Application type: %02x\n", application_type);
	printf("Application manufacturer: %04x\n", application_manufacturer);
	printf("Manufacturer code: %04x\n", manufacturer_code);
	printf("Menu string: %.*s\n", menu_string_length, menu_string);

	return 0;
}