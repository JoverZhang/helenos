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

/** @addtogroup libdevice
 * @{
 */
/**
 * @file
 * @brief Block device client interface
 */

#include <async.h>
#include <assert.h>
#include <bd.h>
#include <errno.h>
#include <ipc/bd.h>
#include <ipc/services.h>
#include <loc.h>
#include <macros.h>
#include <stdlib.h>
#include <offset.h>

errno_t bd_open(async_sess_t *sess, bd_t **rbd)
{
	bd_t *bd;

	bd = calloc(1, sizeof(bd_t));
	if (bd == NULL)
		return ENOMEM;

	bd->sess = sess;

	*rbd = bd;
	return EOK;
}

void bd_close(bd_t *bd)
{
	free(bd);
}

errno_t bd_read_blocks(bd_t *bd, aoff64_t ba, size_t cnt, void *data, size_t size)
{
	async_exch_t *exch = async_exchange_begin(bd->sess);

	ipc_call_t answer;
	aid_t req = async_send_3(exch, BD_READ_BLOCKS, LOWER32(ba),
	    UPPER32(ba), cnt, &answer);
	errno_t rc = async_data_read_start(exch, data, size);
	async_exchange_end(exch);

	if (rc != EOK) {
		async_forget(req);
		return rc;
	}

	errno_t retval;
	async_wait_for(req, &retval);

	if (retval != EOK)
		return retval;

	return EOK;
}

errno_t bd_read_toc(bd_t *bd, uint8_t session, void *buf, size_t size)
{
	async_exch_t *exch = async_exchange_begin(bd->sess);

	ipc_call_t answer;
	aid_t req = async_send_1(exch, BD_READ_TOC, session, &answer);
	errno_t rc = async_data_read_start(exch, buf, size);
	async_exchange_end(exch);

	if (rc != EOK) {
		async_forget(req);
		return rc;
	}

	errno_t retval;
	async_wait_for(req, &retval);

	if (retval != EOK)
		return retval;

	return EOK;
}

errno_t bd_write_blocks(bd_t *bd, aoff64_t ba, size_t cnt, const void *data,
    size_t size)
{
	async_exch_t *exch = async_exchange_begin(bd->sess);

	ipc_call_t answer;
	aid_t req = async_send_3(exch, BD_WRITE_BLOCKS, LOWER32(ba),
	    UPPER32(ba), cnt, &answer);
	errno_t rc = async_data_write_start(exch, data, size);
	async_exchange_end(exch);

	if (rc != EOK) {
		async_forget(req);
		return rc;
	}

	errno_t retval;
	async_wait_for(req, &retval);
	if (retval != EOK)
		return retval;

	return EOK;
}

errno_t bd_sync_cache(bd_t *bd, aoff64_t ba, size_t cnt)
{
	async_exch_t *exch = async_exchange_begin(bd->sess);

	errno_t rc = async_req_3_0(exch, BD_SYNC_CACHE, LOWER32(ba),
	    UPPER32(ba), cnt);
	async_exchange_end(exch);

	return rc;
}

errno_t bd_get_block_size(bd_t *bd, size_t *rbsize)
{
	sysarg_t bsize;
	async_exch_t *exch = async_exchange_begin(bd->sess);

	errno_t rc = async_req_0_1(exch, BD_GET_BLOCK_SIZE, &bsize);
	async_exchange_end(exch);

	if (rc != EOK)
		return rc;

	*rbsize = bsize;
	return EOK;
}

errno_t bd_get_num_blocks(bd_t *bd, aoff64_t *rnb)
{
	sysarg_t nb_l;
	sysarg_t nb_h;
	async_exch_t *exch = async_exchange_begin(bd->sess);

	errno_t rc = async_req_0_2(exch, BD_GET_NUM_BLOCKS, &nb_l, &nb_h);
	async_exchange_end(exch);

	if (rc != EOK)
		return rc;

	*rnb = (aoff64_t) MERGE_LOUP32(nb_l, nb_h);
	return EOK;
}

errno_t bd_eject(bd_t *bd)
{
	async_exch_t *exch = async_exchange_begin(bd->sess);

	errno_t rc = async_req_0_0(exch, BD_EJECT);
	async_exchange_end(exch);

	return rc;
}

/** @}
 */
