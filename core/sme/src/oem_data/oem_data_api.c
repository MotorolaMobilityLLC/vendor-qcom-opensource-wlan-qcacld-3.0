/*
 * Copyright (c) 2012-2016 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

#ifdef FEATURE_OEM_DATA_SUPPORT
/** ------------------------------------------------------------------------- *
    ------------------------------------------------------------------------- *

    \file oem_data_api.c

    Implementation for the OEM DATA REQ/RSP interfaces.
   ========================================================================== */
#include "ani_global.h"
#include "oem_data_api.h"
#include "cds_mq.h"
#include "sme_inside.h"
#include "sms_debug.h"
#include "qdf_util.h"

#include "csr_support.h"

#include "host_diag_core_log.h"
#include "host_diag_core_event.h"

/* ---------------------------------------------------------------------------
    \fn oem_data_oem_data_req_open
    \brief This function must be called before any API call to (OEM DATA REQ/RSP module)
    \return QDF_STATUS
   -------------------------------------------------------------------------------*/

QDF_STATUS oem_data_oem_data_req_open(tHalHandle hHal)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

	do {
		/* initialize all the variables to null */
		qdf_mem_set(&(pMac->oemData), sizeof(tOemDataStruct), 0);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sms_log(pMac, LOGE,
				"oem_data_oem_data_req_open: Cannot allocate memory for the timer function");
			break;
		}
	} while (0);

	return status;
}

/* ---------------------------------------------------------------------------
    \fn oem_data_oem_data_req_close
    \brief This function must be called before closing the csr module
    \return QDF_STATUS
   -------------------------------------------------------------------------------*/

QDF_STATUS oem_data_oem_data_req_close(tHalHandle hHal)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

	do {
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sms_log(pMac, LOGE,
				"oem_data_oem_data_req_close: Failed in oem_data_oem_data_req_close at StopTimers");
			break;
		}

		/* initialize all the variables to null */
		qdf_mem_set(&(pMac->oemData), sizeof(tOemDataStruct), 0);

	} while (0);

	return QDF_STATUS_SUCCESS;
}
#endif
