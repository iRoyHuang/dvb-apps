/*
	dvbscan utility

	Copyright (C) 2006 Andrew de Quincey (adq_dvb@lidskialf.net)

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the

	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "dvbscan.h"


#define OUTPUT_TYPE_RAW 	1
#define OUTPUT_TYPE_CHANNELS 	2
#define OUTPUT_TYPE_VDR12 	3
#define OUTPUT_TYPE_VDR13 	4

#define SERVICE_FILTER_TV		1
#define SERVICE_FILTER_RADIO		2
#define SERVICE_FILTER_OTHER		4
#define SERVICE_FILTER_ENCRYPTED	8


// transponders we have yet to scan
static struct transponder *toscan = NULL;
static struct transponder *toscan_end = NULL;

// transponders we have scanned
static struct transponder *scanned = NULL;
static struct transponder *scanned_end = NULL;


static void usage(void)
{
	static const char *_usage = "\n"
		" dvbscan: A digital tv channel scanning utility\n"
		" Copyright (C) 2006 Andrew de Quincey (adq_dvb@lidskialf.net)\n\n"
		" usage: dvbscan <options> as follows:\n"
		" -h			help\n"
		" -adapter <id>		adapter to use (default 0)\n"
		" -frontend <id>	frontend to use (default 0)\n"
		" -demux <id>		demux to use (default 0)\n"
		" -secfile <filename>	Optional sec.conf file.\n"
		" -secid <secid>	ID of the SEC configuration to use, one of:\n"
		"			 * UNIVERSAL (default) - Europe, 10800 to 11800 MHz and 11600 to 12700 Mhz,\n"
		" 						 Dual LO, loband 9750, hiband 10600 MHz.\n"
		"			 * DBS - Expressvu, North America, 12200 to 12700 MHz, Single LO, 11250 MHz.\n"
		"			 * STANDARD - 10945 to 11450 Mhz, Single LO, 10000 Mhz.\n"
		"			 * ENHANCED - Astra, 10700 to 11700 MHz, Single LO, 9750 MHz.\n"
		"			 * C-BAND - Big Dish, 3700 to 4200 MHz, Single LO, 5150 Mhz.\n"
		"			 * C-MULTI - Big Dish - Multipoint LNBf, 3700 to 4200 MHz,\n"
		"						Dual LO, H:5150MHz, V:5750MHz.\n"
		"			 * One of the sec definitions from the secfile if supplied\n"
		" -switchpos <position>	Specify DISEQC switch position for DVB-S.\n"
		" -inversion <on|off|auto> Specify inversion (default: auto).\n"
		" -uk-ordering 		Use UK DVB-T channel ordering if present.\n"
		" -timeout <secs>	Specify filter timeout to use (standard specced values will be used by default)\n"
		" -filter <filter>	Specify service filter, a comma seperated list of the following tokens:\n"
		" 			 (If no filter is supplied, all services will be output)\n"
		"			 * tv - Output TV channels\n"
		"			 * radio - Output radio channels\n"
		"			 * other - Output other channels\n"
		"			 * encrypted - Output encrypted channels\n"
		" -out raw <filename>|-	 Output in raw format to <filename> or stdout\n"
		"      channels <filename>|-  Output in channels.conf format to <filename> or stdout.\n"
		"      vdr12 <filename>|- Output in vdr 1.2.x format to <filename> or stdout.\n"
		"      vdr13 <filename>|- Output in vdr 1.3.x format to <filename> or stdout.\n"
		" <initial scan file>\n";
	fprintf(stderr, "%s\n", _usage);

	exit(1);
}

int main(int argc, char *argv[])
{
	int argpos = 1;
	int adapter_id = 0;
	int frontend_id = 0;
	int demux_id = 0;
	char *secfile = NULL;
	char *secid = NULL;
	int switchpos = -1;
	enum dvbfe_spectral_inversion inversion = DVBFE_INVERSION_AUTO;
	int service_filter = -1;
	int uk_ordering = 0;
	int timeout = 5;
	int output_type = OUTPUT_TYPE_RAW;
	char *output_filename = NULL;
	char *scan_filename = NULL;


	while(argpos != argc) {
		if (!strcmp(argv[argpos], "-h")) {
			usage();
		} else if (!strcmp(argv[argpos], "-adapter")) {
			if ((argc - argpos) < 2)
				usage();
			if (sscanf(argv[argpos+1], "%i", &adapter_id) != 1)
				usage();
			argpos+=2;
		} else if (!strcmp(argv[argpos], "-frontend")) {
			if ((argc - argpos) < 2)
				usage();
			if (sscanf(argv[argpos+1], "%i", &frontend_id) != 1)
				usage();
			argpos+=2;
		} else if (!strcmp(argv[argpos], "-demux")) {
			if ((argc - argpos) < 2)
				usage();
			if (sscanf(argv[argpos+1], "%i", &demux_id) != 1)
				usage();
			argpos+=2;
		} else if (!strcmp(argv[argpos], "-secfile")) {
			if ((argc - argpos) < 2)
				usage();
			secfile = argv[argpos+1];
			argpos+=2;
		} else if (!strcmp(argv[argpos], "-secid")) {
			if ((argc - argpos) < 2)
				usage();
			secid = argv[argpos+1];
			argpos+=2;
		} else if (!strcmp(argv[argpos], "-switchpos")) {
			if ((argc - argpos) < 2)
				usage();
			if (sscanf(argv[argpos+1], "%i", &switchpos) != 1)
				usage();
			argpos+=2;
		} else if (!strcmp(argv[argpos], "-inversion")) {
			if ((argc - argpos) < 2)
				usage();
			if (!strcmp(argv[argpos+1], "off")) {
				inversion = DVBFE_INVERSION_OFF;
			} else if (!strcmp(argv[argpos+1], "on")) {
				inversion = DVBFE_INVERSION_ON;
			} else if (!strcmp(argv[argpos+1], "auto")) {
				inversion = DVBFE_INVERSION_AUTO;
			} else {
				usage();
			}
			argpos+=2;
		} else if (!strcmp(argv[argpos], "-uk-ordering")) {
			if ((argc - argpos) < 1)
				usage();
			uk_ordering = 1;
		} else if (!strcmp(argv[argpos], "-timeout")) {
			if ((argc - argpos) < 2)
				usage();
			if (sscanf(argv[argpos+1], "%i", &timeout) != 1)
				usage();
			argpos+=2;
		} else if (!strcmp(argv[argpos], "-filter")) {
			if ((argc - argpos) < 2)
				usage();
			service_filter = 0;
			if (!strstr(argv[argpos+1], "tv")) {
				service_filter |= SERVICE_FILTER_TV;
			}
 			if (!strstr(argv[argpos+1], "radio")) {
				service_filter |= SERVICE_FILTER_RADIO;
 			}
			if (!strstr(argv[argpos+1], "other")) {
				service_filter |= SERVICE_FILTER_OTHER;
			}
			if (!strstr(argv[argpos+1], "encrypted")) {
				service_filter |= SERVICE_FILTER_ENCRYPTED;
			}
			argpos+=2;
		} else if (!strcmp(argv[argpos], "-out")) {
			if ((argc - argpos) < 3)
				usage();
			if (!strcmp(argv[argpos+1], "raw")) {
				output_type = OUTPUT_TYPE_RAW;
			} else if (!strcmp(argv[argpos+1], "channels")) {
				output_type = OUTPUT_TYPE_CHANNELS;
			} else if (!strcmp(argv[argpos+1], "vdr12")) {
				output_type = OUTPUT_TYPE_VDR12;
			} else if (!strcmp(argv[argpos+1], "vdr13")) {
				output_type = OUTPUT_TYPE_VDR13;
			} else {
				usage();
			}
			output_filename = argv[argpos+2];
			if (!strcmp(output_filename, "-"))
				output_filename = NULL;
		} else {
			if ((argc - argpos) != 1)
				usage();
			scan_filename = argv[argpos];
			argpos++;
		}
	}

	// FIXME: post-parsing checks etc.

	// FIXME: load the initial scan file

	// main scan loop
	while(toscan) {
		// FIXME: have we already scanned this one?

		// FIXME: tune it

		// FIXME: scan it

		// add to scanned list.
		if (scanned_end == NULL) {
			scanned = toscan;
		} else {
			scanned_end->next = toscan;
		}
		scanned_end = toscan;
		toscan->next = NULL;

		// remove from toscan list
		toscan = toscan->next;
		if (toscan == NULL)
			toscan_end = NULL;
	}

	// FIXME: output the data

	return 0;
}
