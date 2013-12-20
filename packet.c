/*
* Copyright 2011 Anders Ma (andersma.com). All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
*
* 3. The name of the copyright holder may not be used to endorse or promote
* products derived from this software without specific prior written
* permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL WILLIAM TISÄTER BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*/

#include <stdio.h>
#include <string.h>
#include "packet.h"

#define DEBUG_PACKET 0

int packet_encode(packet_t *p)
{
	/*
	 * encode packet according 'raw'
	 * make packet data buffer containing all data to be send
	*/
	unsigned char a, b, c;
	unsigned char i, j, k;
	char buffer[MAX_PACKET_DATA_LENGTH];

	switch (p->type) {
	case PTYPE_RFID_CMD:
		memcpy(p->data, (p->raw).rfid_cmd, sizeof((p->raw).rfid_cmd));
		p->data_length = sizeof((p->raw).rfid_cmd);
		break;
	}

#if DEBUG_PACKET
	g_print("[E] ");
	for (i = 0; i < p->data_length; i++) {
		g_print("%x", p->data[i]);
	}
	g_print("\n");
#endif

	return 0;
}

int packet_decode(packet_t *p)
{
#if DEBUG_PACKET
	int i;
	g_print("[D] ");
	for (i = 0; i < p->data_length; i++) {
		g_print("%x,", p->data[i]);
	}
	g_print("\n");
#endif
	/* PTYPE_RFID_ID */
	if (*(p->data + 0) == 0xAA && *(p->data + 1) == 0xBB && *(p->data + 2) == 0x06 
		&& *(p->data + 3) == 0x20) {
		p->type = PTYPE_RFID_ID;
		memcpy((p->raw).rfid_id, &p->data[4], 5);
		return 0; 
	}
	return -1;
}

