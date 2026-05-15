/*
 * Copyright (c) 2026 Jiri Svoboda
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

/** @addtogroup libui
 * @{
 */
/**
 * @file Progress bar
 */

#include <errno.h>
#include <gfx/color.h>
#include <gfx/context.h>
#include <gfx/render.h>
#include <gfx/text.h>
#include <io/pos_event.h>
#include <stdio.h>
#include <stdlib.h>
#include <str.h>
#include <ui/control.h>
#include <ui/paint.h>
#include <ui/progress.h>
#include "../private/progress.h"
#include "../private/resource.h"

enum {
	progress_box_w = 16,
	progress_box_h = 16,
	progress_label_margin = 8,
	progress_cross_n = 5,
	progress_cross_w = 2,
	progress_cross_h = 2
};

static void ui_progress_ctl_destroy(void *);
static errno_t ui_progress_ctl_paint(void *);
static ui_evclaim_t ui_progress_ctl_pos_event(void *, pos_event_t *);

/** Progress bar control ops */
ui_control_ops_t ui_progress_ops = {
	.destroy = ui_progress_ctl_destroy,
	.paint = ui_progress_ctl_paint,
	.pos_event = ui_progress_ctl_pos_event
};

/** Create new progress bar.
 *
 * @param resource UI resource
 * @param value Initial progress value
 * @param rprogress Place to store pointer to new progress bar
 * @return EOK on success, ENOMEM if out of memory
 */
errno_t ui_progress_create(ui_resource_t *resource, unsigned value,
    ui_progress_t **rprogress)
{
	ui_progress_t *progress;
	errno_t rc;

	progress = calloc(1, sizeof(ui_progress_t));
	if (progress == NULL)
		return ENOMEM;

	rc = ui_control_new(&ui_progress_ops, (void *) progress,
	    &progress->control);
	if (rc != EOK) {
		free(progress);
		return rc;
	}

	progress->value = value;
	progress->res = resource;
	*rprogress = progress;
	return EOK;
}

/** Destroy progress bar.
 *
 * @param progress Progress bar or @c NULL
 */
void ui_progress_destroy(ui_progress_t *progress)
{
	if (progress == NULL)
		return;

	ui_control_delete(progress->control);
	free(progress);
}

/** Get base control from progress bar.
 *
 * @param progress Progress bar
 * @return Control
 */
ui_control_t *ui_progress_ctl(ui_progress_t *progress)
{
	return progress->control;
}

/** Set progress bar rectangle.
 *
 * @param progress Progress bar
 * @param rect New progress bar rectangle
 */
void ui_progress_set_rect(ui_progress_t *progress, gfx_rect_t *rect)
{
	progress->rect = *rect;
}

/** Set progress bar value.
 *
 * @param progress Progress bar
 * @param value New progress bar value
 */
void ui_progress_set_value(ui_progress_t *progress, unsigned value)
{
	progress->value = value;
}

/** Paint done/remaining part of progress bar.
 *
 * @param progress Progress bar
 * @param bool remain @c true to paint remaining part
 * @param rect Rectangle of the part
 * @return EOK on success or an error code
 */
static errno_t ui_progress_paint_part(ui_progress_t *progress,
    gfx_rect_t *rect, bool remain)
{
	gfx_coord2_t pos;
	gfx_text_fmt_t fmt;
	char buf[6];
	errno_t rc;

	/* Set clipping rectangle. */
	rc = gfx_set_clip_rect(progress->res->gc, rect);
	if (rc != EOK)
		return rc;

	/* Paint background */

	rc = gfx_set_color(progress->res->gc, remain ?
	    progress->res->entry_bg_color :
	    progress->res->entry_sel_text_bg_color);
	if (rc != EOK)
		goto error;

	rc = gfx_fill_rect(progress->res->gc, rect);
	if (rc != EOK)
		goto error;

	/* Paint label */

	pos.x = (progress->rect.p0.x + progress->rect.p1.x) / 2;
	pos.y = (progress->rect.p0.y + progress->rect.p1.y) / 2;

	gfx_text_fmt_init(&fmt);
	fmt.font = progress->res->font;
	fmt.color = remain ? progress->res->entry_fg_color :
	    progress->res->entry_sel_text_fg_color;
	fmt.halign = gfx_halign_center;
	fmt.valign = gfx_valign_center;

	snprintf(buf, sizeof(buf), "%u %%", progress->value);

	rc = gfx_puttext(&pos, &fmt, buf);
	if (rc != EOK)
		goto error;

	rc = gfx_update(progress->res->gc);
	if (rc != EOK)
		goto error;

	return gfx_set_clip_rect(progress->res->gc, NULL);
error:
	(void)gfx_set_clip_rect(progress->res->gc, NULL);
	return rc;
}

/** Paint progress bar.
 *
 * @param progress Progress bar
 * @return EOK on success or an error code
 */
errno_t ui_progress_paint(ui_progress_t *progress)
{
	gfx_rect_t inside;
	gfx_rect_t part;
	gfx_coord_t width;
	errno_t rc;

	if (progress->res->textmode) {
		/* no frame in textmode */
		inside = progress->rect;
	} else {
		/* Paint progress frame */

		rc = ui_paint_inset_frame(progress->res, &progress->rect,
		    &inside);
		if (rc != EOK)
			goto error;
	}

	width = inside.p1.x - inside.p0.x;

	/* Paint completed part. */
	part.p0 = inside.p0;
	part.p1.x = inside.p0.x + width * progress->value / 100;
	part.p1.y = inside.p1.y;

	rc = ui_progress_paint_part(progress, &part, false);
	if (rc != EOK)
		goto error;

	/* Paint remaining part. */
	part.p0.x = part.p1.x;
	part.p1.x = inside.p1.x;

	rc = ui_progress_paint_part(progress, &part, true);
	if (rc != EOK)
		goto error;

	rc = gfx_update(progress->res->gc);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	return rc;
}

/** Destroy progress bar control.
 *
 * @param arg Argument (ui_progress_t *)
 */
void ui_progress_ctl_destroy(void *arg)
{
	ui_progress_t *progress = (ui_progress_t *) arg;

	ui_progress_destroy(progress);
}

/** Paint progress bar control.
 *
 * @param arg Argument (ui_progress_t *)
 * @return EOK on success or an error code
 */
errno_t ui_progress_ctl_paint(void *arg)
{
	ui_progress_t *progress = (ui_progress_t *) arg;

	return ui_progress_paint(progress);
}

/** Handle progress bar position event.
 *
 * @param arg Argument (ui_progress_t *)
 * @param pos_event Position event
 * @return @c ui_claimed iff the event is claimed
 */
ui_evclaim_t ui_progress_ctl_pos_event(void *arg, pos_event_t *event)
{
	ui_progress_t *progress = (ui_progress_t *) arg;

	(void) progress;
	return ui_unclaimed;
}

/** @}
 */
