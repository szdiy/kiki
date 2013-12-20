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

#ifndef HACKER_H_
#define HACKER_H_

#include <pthread.h>
#include <glib.h>

#define HACKERS_XML_FILENAME "hackers.xml"

#define HACKER_RFID_NODE "RFID"
#define HACKER_NAME_NODE "name"
#define HACKER_PROF_NODE "profession"
#define HACKER_WHY_NODE "momentum"
#define HACKER_EMAIL_NODE "email"
#define HACKER_MAC_NODE "MAC"

#define HACKER_DEF_RFID "00000000"
#define HACKER_DEF_NAME "SZDIYer"
#define HACKER_DEF_PROF "Hacker"
#define HACKER_DEF_WHY "@@"
#define HACKER_DEF_EMAIL "hacker@nirvana"
#define HACKER_DEF_MAC "00:11:22:33:44:55"

typedef struct hacker_struct {
	char* rfid;
	char* name;
	char* prof;
	char* why;
	char* email;
	char* mac;
	char* login_time;	
	int login;
} hacker_t;

typedef struct hackers_struct {
	GSList *list;
	pthread_mutex_t mutex;
} hacker_list_t;

extern hacker_t* new_hacker(hacker_list_t *l);
extern void hackers_init(hacker_list_t *l);
extern void hackers_reload(hacker_list_t* l);
extern void hackers_destroy(hacker_list_t *l);
extern void hackers_save(hacker_list_t *l);
extern hacker_t* search_hacker(hacker_list_t *l, char* rfid);
#endif
