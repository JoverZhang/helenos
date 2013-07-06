/*
 * Copyright (c) 2012 Jiri Svoboda
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** @addtogroup inet
 * @{
 */
/**
 * @file
 * @brief
 */

#include <stdbool.h>
#include <errno.h>
#include <fibril_synch.h>
#include <inet/iplink.h>
#include <io/log.h>
#include <loc.h>
#include <stdlib.h>
#include <str.h>
#include <net/socket_codes.h>
#include "addrobj.h"
#include "inetsrv.h"
#include "inet_link.h"
#include "pdu.h"

static int inet_link_open(service_id_t);
static int inet_iplink_recv(iplink_t *, iplink_recv_sdu_t *, uint16_t);

static iplink_ev_ops_t inet_iplink_ev_ops = {
	.recv = inet_iplink_recv
};

static LIST_INITIALIZE(inet_link_list);
static FIBRIL_MUTEX_INITIALIZE(inet_discovery_lock);

static int inet_iplink_recv(iplink_t *iplink, iplink_recv_sdu_t *sdu, uint16_t af)
{
	log_msg(LOG_DEFAULT, LVL_DEBUG, "inet_iplink_recv()");
	
	int rc;
	inet_packet_t packet;
	
	switch (af) {
	case AF_INET:
		rc = inet_pdu_decode(sdu->data, sdu->size, &packet);
		break;
	case AF_INET6:
		rc = inet_pdu_decode6(sdu->data, sdu->size, &packet);
		break;
	default:
		log_msg(LOG_DEFAULT, LVL_DEBUG, "invalid address family");
		return EINVAL;
	}
	
	if (rc != EOK) {
		log_msg(LOG_DEFAULT, LVL_DEBUG, "failed decoding PDU");
		return rc;
	}
	
	log_msg(LOG_DEFAULT, LVL_DEBUG, "call inet_recv_packet()");
	rc = inet_recv_packet(&packet);
	log_msg(LOG_DEFAULT, LVL_DEBUG, "call inet_recv_packet -> %d", rc);
	free(packet.data);
	
	return rc;
}

static int inet_link_check_new(void)
{
	bool already_known;
	category_id_t iplink_cat;
	service_id_t *svcs;
	size_t count, i;
	int rc;

	fibril_mutex_lock(&inet_discovery_lock);

	rc = loc_category_get_id("iplink", &iplink_cat, IPC_FLAG_BLOCKING);
	if (rc != EOK) {
		log_msg(LOG_DEFAULT, LVL_ERROR, "Failed resolving category 'iplink'.");
		fibril_mutex_unlock(&inet_discovery_lock);
		return ENOENT;
	}

	rc = loc_category_get_svcs(iplink_cat, &svcs, &count);
	if (rc != EOK) {
		log_msg(LOG_DEFAULT, LVL_ERROR, "Failed getting list of IP links.");
		fibril_mutex_unlock(&inet_discovery_lock);
		return EIO;
	}

	for (i = 0; i < count; i++) {
		already_known = false;

		list_foreach(inet_link_list, ilink_link) {
			inet_link_t *ilink = list_get_instance(ilink_link,
			    inet_link_t, link_list);
			if (ilink->svc_id == svcs[i]) {
				already_known = true;
				break;
			}
		}

		if (!already_known) {
			log_msg(LOG_DEFAULT, LVL_DEBUG, "Found IP link '%lu'",
			    (unsigned long) svcs[i]);
			rc = inet_link_open(svcs[i]);
			if (rc != EOK)
				log_msg(LOG_DEFAULT, LVL_ERROR, "Could not open IP link.");
		}
	}

	fibril_mutex_unlock(&inet_discovery_lock);
	return EOK;
}

static inet_link_t *inet_link_new(void)
{
	inet_link_t *ilink = calloc(1, sizeof(inet_link_t));

	if (ilink == NULL) {
		log_msg(LOG_DEFAULT, LVL_ERROR, "Failed allocating link structure. "
		    "Out of memory.");
		return NULL;
	}

	link_initialize(&ilink->link_list);

	return ilink;
}

static void inet_link_delete(inet_link_t *ilink)
{
	if (ilink->svc_name != NULL)
		free(ilink->svc_name);
	free(ilink);
}

static int inet_link_open(service_id_t sid)
{
	inet_link_t *ilink;
	inet_addr_t iaddr;
	int rc;

	log_msg(LOG_DEFAULT, LVL_DEBUG, "inet_link_open()");
	ilink = inet_link_new();
	if (ilink == NULL)
		return ENOMEM;

	ilink->svc_id = sid;
	ilink->iplink = NULL;

	rc = loc_service_get_name(sid, &ilink->svc_name);
	if (rc != EOK) {
		log_msg(LOG_DEFAULT, LVL_ERROR, "Failed getting service name.");
		goto error;
	}

	ilink->sess = loc_service_connect(EXCHANGE_SERIALIZE, sid, 0);
	if (ilink->sess == NULL) {
		log_msg(LOG_DEFAULT, LVL_ERROR, "Failed connecting '%s'", ilink->svc_name);
		goto error;
	}

	rc = iplink_open(ilink->sess, &inet_iplink_ev_ops, &ilink->iplink);
	if (rc != EOK) {
		log_msg(LOG_DEFAULT, LVL_ERROR, "Failed opening IP link '%s'",
		    ilink->svc_name);
		goto error;
	}

	rc = iplink_get_mtu(ilink->iplink, &ilink->def_mtu);
	if (rc != EOK) {
		log_msg(LOG_DEFAULT, LVL_ERROR, "Failed determinning MTU of link '%s'",
		    ilink->svc_name);
		goto error;
	}

	log_msg(LOG_DEFAULT, LVL_DEBUG, "Opened IP link '%s'", ilink->svc_name);
	list_append(&ilink->link_list, &inet_link_list);

	inet_addrobj_t *addr;
	inet_addrobj_t *addr6;

	static int first = 1;
	
	addr = inet_addrobj_new();
	addr6 = inet_addrobj_new();
	
	if (first) {
		inet_naddr(&addr->naddr, 127, 0, 0, 1, 24);
		inet_naddr6(&addr6->naddr, 0, 0, 0, 0, 0, 0, 0, 1, 128);
		first = 0;
	} else {
		/*
		 * FIXME
		 * Setting static IP addresses for testing purposes
		 * 10.0.2.15/24
		 * fd19:1680::4/120
		 */
		inet_naddr(&addr->naddr, 10, 0, 2, 15, 24);
		inet_naddr6(&addr6->naddr, 0xfd19, 0x1680, 0, 0, 0, 0, 0, 4, 120);
	}
	
	addr->ilink = ilink;
	addr6->ilink = ilink;
	addr->name = str_dup("v4a");
	addr6->name = str_dup("v6a");
	
	rc = inet_addrobj_add(addr);
	if (rc != EOK) {
		log_msg(LOG_DEFAULT, LVL_ERROR, "Failed adding IPv4 address.");
		inet_addrobj_delete(addr);
		/* XXX Roll back */
		return rc;
	}
	
	rc = inet_addrobj_add(addr6);
	if (rc != EOK) {
		log_msg(LOG_DEFAULT, LVL_ERROR, "Failed adding IPv6 address.");
		inet_addrobj_delete(addr6);
		/* XXX Roll back */
		return rc;
	}
	
	inet_naddr_addr(&addr->naddr, &iaddr);
	rc = iplink_addr_add(ilink->iplink, &iaddr);
	if (rc != EOK) {
		log_msg(LOG_DEFAULT, LVL_ERROR, "Failed setting IPv4 address on internet link.");
		inet_addrobj_remove(addr);
		inet_addrobj_delete(addr);
		/* XXX Roll back */
		return rc;
	}
	
	inet_naddr_addr(&addr6->naddr, &iaddr);
	rc = iplink_addr_add(ilink->iplink, &iaddr);
	if (rc != EOK) {
		log_msg(LOG_DEFAULT, LVL_ERROR, "Failed setting IPv6 address on internet link.");
		inet_addrobj_remove(addr6);
		inet_addrobj_delete(addr6);
		/* XXX Roll back */
		return rc;
	}
	
	return EOK;
	
error:
	if (ilink->iplink != NULL)
		iplink_close(ilink->iplink);
	
	inet_link_delete(ilink);
	return rc;
}

static void inet_link_cat_change_cb(void)
{
	(void) inet_link_check_new();
}

int inet_link_discovery_start(void)
{
	int rc;

	rc = loc_register_cat_change_cb(inet_link_cat_change_cb);
	if (rc != EOK) {
		log_msg(LOG_DEFAULT, LVL_ERROR, "Failed registering callback for IP link "
		    "discovery (%d).", rc);
		return rc;
	}

	return inet_link_check_new();
}

/** Send datagram over Internet link */
int inet_link_send_dgram(inet_link_t *ilink, inet_addr_t *lsrc,
    inet_addr_t *ldest, inet_dgram_t *dgram, uint8_t proto, uint8_t ttl, int df)
{
	/*
	 * Fill packet structure. Fragmentation is performed by
	 * inet_pdu_encode().
	 */
	
	inet_packet_t packet;
	
	packet.src = dgram->src;
	packet.dest = dgram->dest;
	packet.tos = dgram->tos;
	packet.proto = proto;
	packet.ttl = ttl;
	packet.df = df;
	packet.data = dgram->data;
	packet.size = dgram->size;
	
	iplink_sdu_t sdu;
	size_t offs = 0;
	int rc;
	
	sdu.src = *lsrc;
	sdu.dest = *ldest;
	
	do {
		/* Encode one fragment */
		size_t roffs;
		rc = inet_pdu_encode(&packet, offs, ilink->def_mtu, &sdu.data,
		    &sdu.size, &roffs);
		if (rc != EOK)
			return rc;
		
		/* Send the PDU */
		rc = iplink_send(ilink->iplink, &sdu);
		free(sdu.data);
		
		offs = roffs;
	} while (offs < packet.size);
	
	return rc;
}

inet_link_t *inet_link_get_by_id(sysarg_t link_id)
{
	fibril_mutex_lock(&inet_discovery_lock);

	list_foreach(inet_link_list, elem) {
		inet_link_t *ilink = list_get_instance(elem, inet_link_t,
		    link_list);

		if (ilink->svc_id == link_id) {
			fibril_mutex_unlock(&inet_discovery_lock);
			return ilink;
		}
	}

	fibril_mutex_unlock(&inet_discovery_lock);
	return NULL;
}

/** @}
 */
