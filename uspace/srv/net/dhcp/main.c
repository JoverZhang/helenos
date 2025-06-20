/*
 * Copyright (c) 2025 Jiri Svoboda
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

/** @addtogroup dhcp
 * @{
 */
/**
 * @file
 */

#include <async.h>
#include <errno.h>
#include <str_error.h>
#include <io/log.h>
#include <ipc/dhcp.h>
#include <ipc/services.h>
#include <loc.h>
#include <stdio.h>
#include <stdlib.h>
#include <task.h>

#include "dhcp.h"

#define NAME  "dhcp"

static void dhcp_client_conn(ipc_call_t *, void *);

static errno_t dhcp_init(void)
{
	loc_srv_t *srv;
	errno_t rc;

	log_msg(LOG_DEFAULT, LVL_DEBUG, "dhcp_init()");

	dhcpsrv_links_init();

	async_set_fallback_port_handler(dhcp_client_conn, NULL);

	rc = loc_server_register(NAME, &srv);
	if (rc != EOK) {
		log_msg(LOG_DEFAULT, LVL_ERROR, "Failed registering server: %s.", str_error(rc));
		return EEXIST;
	}

	service_id_t sid;
	rc = loc_service_register(srv, SERVICE_NAME_DHCP, fallback_port_id,
	    &sid);
	if (rc != EOK) {
		loc_server_unregister(srv);
		log_msg(LOG_DEFAULT, LVL_ERROR, "Failed registering service: %s.", str_error(rc));
		return EEXIST;
	}

	return EOK;
}

static void dhcp_link_add_srv(ipc_call_t *call)
{
	sysarg_t link_id;
	errno_t rc;

	log_msg(LOG_DEFAULT, LVL_DEBUG, "dhcp_link_add_srv()");

	link_id = ipc_get_arg1(call);

	rc = dhcpsrv_link_add(link_id);
	async_answer_0(call, rc);
}

static void dhcp_link_remove_srv(ipc_call_t *call)
{
	sysarg_t link_id;
	errno_t rc;

	log_msg(LOG_DEFAULT, LVL_DEBUG, "dhcp_link_remove_srv()");

	link_id = ipc_get_arg1(call);

	rc = dhcpsrv_link_remove(link_id);
	async_answer_0(call, rc);
}

static void dhcp_discover_srv(ipc_call_t *call)
{
	sysarg_t link_id;
	errno_t rc;

	log_msg(LOG_DEFAULT, LVL_DEBUG, "dhcp_discover_srv()");

	link_id = ipc_get_arg1(call);

	rc = dhcpsrv_discover(link_id);
	async_answer_0(call, rc);
}

static void dhcp_client_conn(ipc_call_t *icall, void *arg)
{
	log_msg(LOG_DEFAULT, LVL_DEBUG, "dhcp_client_conn()");

	/* Accept the connection */
	async_accept_0(icall);

	while (true) {
		ipc_call_t call;
		async_get_call(&call);
		sysarg_t method = ipc_get_imethod(&call);

		if (!method) {
			/* The other side has hung up */
			async_answer_0(&call, EOK);
			return;
		}

		switch (method) {
		case DHCP_LINK_ADD:
			dhcp_link_add_srv(&call);
			break;
		case DHCP_LINK_REMOVE:
			dhcp_link_remove_srv(&call);
			break;
		case DHCP_DISCOVER:
			dhcp_discover_srv(&call);
			break;
		default:
			async_answer_0(&call, EINVAL);
		}
	}
}

int main(int argc, char *argv[])
{
	errno_t rc;

	printf("%s: DHCP Service\n", NAME);

	if (log_init(NAME) != EOK) {
		printf(NAME ": Failed to initialize logging.\n");
		return 1;
	}

	rc = dhcp_init();
	if (rc != EOK)
		return 1;

	printf(NAME ": Accepting connections.\n");
	task_retval(0);
	async_manager();

	return 0;
}

/** @}
 */
