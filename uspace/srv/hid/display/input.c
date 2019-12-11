/*
 * Copyright (c) 2019 Jiri Svoboda
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

/** @addtogroup display
 * @{
 */
/** @file Input events
 */

#include <errno.h>
#include <io/input.h>
#include <loc.h>
#include <stdio.h>
#include <str_error.h>
#include "display.h"
#include "input.h"
#include "main.h"

static errno_t ds_input_ev_active(input_t *);
static errno_t ds_input_ev_deactive(input_t *);
static errno_t ds_input_ev_key(input_t *, kbd_event_type_t, keycode_t, keymod_t, wchar_t);
static errno_t ds_input_ev_move(input_t *, int, int);
static errno_t ds_input_ev_abs_move(input_t *, unsigned, unsigned, unsigned, unsigned);
static errno_t ds_input_ev_button(input_t *, int, int);

static input_ev_ops_t ds_input_ev_ops = {
	.active = ds_input_ev_active,
	.deactive = ds_input_ev_deactive,
	.key = ds_input_ev_key,
	.move = ds_input_ev_move,
	.abs_move = ds_input_ev_abs_move,
	.button = ds_input_ev_button
};

static errno_t ds_input_ev_active(input_t *input)
{
	return EOK;
}

static errno_t ds_input_ev_deactive(input_t *input)
{
	return EOK;
}

static errno_t ds_input_ev_key(input_t *input, kbd_event_type_t type,
    keycode_t key, keymod_t mods, wchar_t c)
{
	ds_display_t *disp = (ds_display_t *) input->user;
	kbd_event_t event;

	printf("ds_input_ev_key\n");
	event.type = type;
	event.key = key;
	event.mods = mods;
	event.c = c;

	return ds_display_post_kbd_event(disp, &event);
}

static errno_t ds_input_ev_move(input_t *input, int dx, int dy)
{
	ds_display_t *disp = (ds_display_t *) input->user;
	ptd_event_t event;

	printf("ds_input_ev_move\n");
	event.type = PTD_MOVE;
	event.dmove.x = dx;
	event.dmove.y = dy;

	return ds_display_post_ptd_event(disp, &event);
}

static errno_t ds_input_ev_abs_move(input_t *input, unsigned x, unsigned y,
    unsigned max_x, unsigned max_y)
{
	printf("ds_input_ev_abs_move x=%u y=%u mx=%u my=%u\n",
	    x, y, max_x, max_y);
	return EOK;
}

static errno_t ds_input_ev_button(input_t *input, int bnum, int bpress)
{
	ds_display_t *disp = (ds_display_t *) input->user;
	ptd_event_t event;

	printf("ds_input_ev_abs_button\n");
	event.type = bpress ? PTD_PRESS : PTD_RELEASE;
	event.btn_num = bnum;
	event.dmove.x = 0;
	event.dmove.y = 0;

	return ds_display_post_ptd_event(disp, &event);
}

/** Open input service.
 *
 * @param display Display
 * @return EOK on success or an error code
 */
errno_t ds_input_open(ds_display_t *display)
{
	async_sess_t *sess;
	service_id_t dsid;
	const char *svc = "hid/input";

	printf("ds_input_open\n");
	errno_t rc = loc_service_get_id(svc, &dsid, 0);
	if (rc != EOK) {
		printf("%s: Input service %s not found\n", NAME, svc);
		return rc;
	}

	sess = loc_service_connect(dsid, INTERFACE_INPUT, 0);
	if (sess == NULL) {
		printf("%s: Unable to connect to input service %s\n", NAME,
		    svc);
		return EIO;
	}

	rc = input_open(sess, &ds_input_ev_ops, (void *) display,
	    &display->input);
	if (rc != EOK) {
		async_hangup(sess);
		printf("%s: Unable to communicate with service %s (%s)\n",
		    NAME, svc, str_error(rc));
		return rc;
	}

	input_activate(display->input);
	printf("ds_input_open: DONE\n");
	return EOK;
}

void ds_input_close(ds_display_t *display)
{
	input_close(display->input);
	display->input = NULL;
}

/** @}
 */
