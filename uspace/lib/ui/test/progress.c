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

#include <gfx/context.h>
#include <gfx/coord.h>
#include <mem.h>
#include <pcut/pcut.h>
#include <stdbool.h>
#include <ui/control.h>
#include <ui/progress.h>
#include <ui/resource.h>
#include "../private/progress.h"
#include "../private/testgc.h"

PCUT_INIT;

PCUT_TEST_SUITE(progress);

/** Create and destroy progress bar */
PCUT_TEST(create_destroy)
{
	ui_progress_t *progress = NULL;
	errno_t rc;

	rc = ui_progress_create(NULL, 0, &progress);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_NOT_NULL(progress);

	ui_progress_destroy(progress);
}

/** ui_progress_destroy() can take NULL argument (no-op) */
PCUT_TEST(destroy_null)
{
	ui_progress_destroy(NULL);
}

/** ui_progress_ctl() returns control that has a working virtual destructor */
PCUT_TEST(ctl)
{
	ui_progress_t *progress;
	ui_control_t *control;
	errno_t rc;

	rc = ui_progress_create(NULL, 0, &progress);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	control = ui_progress_ctl(progress);
	PCUT_ASSERT_NOT_NULL(control);

	ui_control_destroy(control);
}

/** Set progress bar rectangle sets internal field */
PCUT_TEST(set_rect)
{
	ui_progress_t *progress;
	gfx_rect_t rect;
	errno_t rc;

	rc = ui_progress_create(NULL, 0, &progress);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rect.p0.x = 1;
	rect.p0.y = 2;
	rect.p1.x = 3;
	rect.p1.y = 4;

	ui_progress_set_rect(progress, &rect);
	PCUT_ASSERT_INT_EQUALS(rect.p0.x, progress->rect.p0.x);
	PCUT_ASSERT_INT_EQUALS(rect.p0.y, progress->rect.p0.y);
	PCUT_ASSERT_INT_EQUALS(rect.p1.x, progress->rect.p1.x);
	PCUT_ASSERT_INT_EQUALS(rect.p1.y, progress->rect.p1.y);

	ui_progress_destroy(progress);
}

/** Set progress bar value sets internal field */
PCUT_TEST(set_value)
{
	ui_progress_t *progress;
	errno_t rc;

	rc = ui_progress_create(NULL, 0, &progress);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	ui_progress_set_value(progress, 42);
	PCUT_ASSERT_INT_EQUALS(42, progress->value);

	ui_progress_destroy(progress);
}

/** Paint progress bar */
PCUT_TEST(paint)
{
	errno_t rc;
	gfx_context_t *gc = NULL;
	test_gc_t tgc;
	ui_resource_t *resource = NULL;
	ui_progress_t *progress;

	memset(&tgc, 0, sizeof(tgc));
	rc = gfx_context_new(&ops, &tgc, &gc);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = ui_resource_create(gc, false, &resource);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_NOT_NULL(resource);

	rc = ui_progress_create(resource, 0, &progress);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = ui_progress_paint(progress);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	ui_progress_destroy(progress);
	ui_resource_destroy(resource);

	rc = gfx_context_delete(gc);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

PCUT_EXPORT(progress);
