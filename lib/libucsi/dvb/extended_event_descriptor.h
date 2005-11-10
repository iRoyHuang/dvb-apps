/*
 * section and descriptor parser
 *
 * Copyright (C) 2005 Kenneth Aafloy (kenneth@linuxtv.org)
 * Copyright (C) 2005 Andrew de Quincey (adq_dvb@lidskialf.net)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#ifndef _UCSI_DVB_EXTENDED_EVENT_DESCRIPTOR
#define _UCSI_DVB_EXTENDED_EVENT_DESCRIPTOR 1

#include <ucsi/descriptor.h>
#include <ucsi/common.h>

struct dvb_extended_event_descriptor {
	struct descriptor d;

  EBIT2(uint8_t descriptor_number	: 4; ,
	uint8_t last_descriptor_number	: 4; );
	uint8_t iso_639_language_code[3];
	uint8_t length_of_items;
	/* struct dvb_extended_event_item items[] */
	/* struct dvb_extended_event_descriptor_part2 part2 */
} packed;

struct dvb_extended_event_item {
	uint8_t item_description_length;
	/* uint8_t item_description[] */
	/* struct dvb_extended_event_item_part2 part2 */
} packed;

struct dvb_extended_event_item_part2 {
	uint8_t item_length;
	/* uint8_t item[] */
} packed;

struct dvb_extended_event_descriptor_part2 {
	uint8_t text_length;
	/* uint8_t text[] */
} packed;

static inline struct dvb_extended_event_descriptor*
	dvb_extended_event_descriptor_parse(struct descriptor* d)
{
	uint8_t* buf = (uint8_t*) d + 2;
	int pos = 0;
	int len = d->len;
	struct dvb_extended_event_descriptor * p =
		(struct dvb_extended_event_descriptor *) d;
	struct dvb_extended_event_descriptor_part2 *p2;

	pos += sizeof(struct dvb_extended_event_descriptor) - 2;

	if (pos > len)
		return NULL;

	pos += p->length_of_items;

	if (pos > len)
		return NULL;

	p2 = (struct dvb_extended_event_descriptor_part2*) (buf+pos);

	pos += sizeof(struct dvb_extended_event_descriptor_part2);

	if (pos > len)
		return NULL;

	pos += p2->text_length;

	if (pos != len)
		return NULL;

	return p;
}

#define dvb_extended_event_descriptor_items_for_each(d, pos) \
	for ((pos) = dvb_extended_event_descriptor_items_first(d); \
	     (pos); \
	     (pos) = dvb_extended_event_descriptor_items_next(d, pos))

static inline uint8_t*
	dvb_extended_event_item_descripton(struct dvb_extended_event_item *d)
{
	return (uint8_t*) d + sizeof(struct dvb_extended_event_item);
}

static inline struct dvb_extended_event_item_part2*
	dvb_extended_event_item_part2(struct dvb_extended_event_item *d)
{
	return (struct dvb_extended_event_item_part2*)
		((uint8_t*) d + sizeof(struct dvb_extended_event_item) +
		 d->item_description_length);
}

static inline uint8_t*
	dvb_extended_event_item_item(struct dvb_extended_event_item_part2 *d)
{
	return (uint8_t*) d + sizeof(struct dvb_extended_event_item_part2);
}

static inline struct dvb_extended_event_descriptor_part2*
	dvb_extended_event_descriptor_part2(struct dvb_extended_event_descriptor *d)
{
	return (struct dvb_extended_event_descriptor_part2*)
		((uint8_t*) d + sizeof(struct dvb_extended_event_descriptor) +
		 d->length_of_items);
}










/******************************** PRIVATE CODE ********************************/
static inline struct dvb_extended_event_item*
	dvb_extended_event_descriptor_items_first(struct dvb_extended_event_descriptor *d)
{
	if (d->d.len == 5)
		return NULL;

	return (struct dvb_extended_event_item *)
		(uint8_t*) d + sizeof(struct dvb_extended_event_descriptor);
}

static inline struct dvb_extended_event_item*
	dvb_extended_event_descriptor_items_next(struct dvb_extended_event_descriptor *d,
						 struct dvb_extended_event_item *pos)
{
	struct dvb_extended_event_item_part2* part2 =
		dvb_extended_event_item_part2(pos);
	uint8_t *end = (uint8_t*) d + 2 + d->d.len;
	uint8_t *next =	(uint8_t *) part2 +
			sizeof(struct dvb_extended_event_item_part2) +
			part2->item_length;

	if (next >= end)
		return NULL;

	return (struct dvb_extended_event_item *) next;
}

#endif