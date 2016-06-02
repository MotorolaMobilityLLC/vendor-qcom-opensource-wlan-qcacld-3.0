/*
 * Copyright (c) 2016 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
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

/**
 * DOC: sme_nan_datapath.h
 *
 * SME NAN Data path API specification
 */

#ifndef __SME_NAN_DATAPATH_H
#define __SME_NAN_DATAPATH_H

#ifdef WLAN_FEATURE_NAN_DATAPATH
#include "qdf_types.h"
#include "sir_api.h"
#include "ani_global.h"
#include "sme_inside.h"

/**
 * struct sir_sme_ndp_initiator_req - sme request struct for ndp initiator req
 * @mesgType: SME msg type(eWNI_SME_NDP_INITIATOR_REQ)
 * @mesgLen: lenght of message
 * @req: actual ndp initiator request
 *
 */
struct sir_sme_ndp_initiator_req {
	uint16_t msg_type;
	uint16_t msg_len;
	struct ndp_initiator_req req;
};

/* NAN initiator request handler */
QDF_STATUS sme_ndp_initiator_req_handler(tHalHandle hal,
					struct ndp_initiator_req *req_params);

/* NAN responder request handler */
QDF_STATUS sme_ndp_responder_req_handler(uint32_t session_id,
					struct ndp_responder_req *req_params);

/* NAN indication response handler */
QDF_STATUS sme_ndp_end_req_handler(uint32_t session_id,
					struct ndp_end_req *req_params);

/* NAN schedule update request handler */
QDF_STATUS sme_ndp_sched_req_handler(uint32_t session_id,
				struct ndp_schedule_update_req *req_params);

/* Function to handle NDP messages from lower layers */
void sme_ndp_message_processor(tpAniSirGlobal mac_ctx, uint16_t msg_type,
				void *msg);

/* Start NDI BSS */
QDF_STATUS csr_roam_start_ndi(tpAniSirGlobal mac_ctx, uint32_t session_id,
			      tCsrRoamProfile *profile);

void csr_roam_fill_roaminfo_ndp(tpAniSirGlobal mac_ctx,
				tCsrRoamInfo *roam_info,
				eCsrRoamResult roam_result,
				tSirResultCodes status_code,
				uint32_t reason_code,
				uint32_t transaction_id);

void csr_roam_save_ndi_connected_info(tpAniSirGlobal mac_ctx,
				      uint32_t session_id,
				      tCsrRoamProfile *roam_profile,
				      tSirBssDescription *bss_desc);

void csr_roam_update_ndp_return_params(tpAniSirGlobal mac_ctx,
					uint32_t result,
					uint32_t *roam_status,
					uint32_t *roam_result,
					struct tagCsrRoamInfo *roam_info);
QDF_STATUS csr_process_ndp_initiator_request(tpAniSirGlobal mac_ctx,
					     tSmeCmd *cmd);

void sme_ndp_msg_processor(tpAniSirGlobal mac_ctx, cds_msg_t *msg);

#else

/* Start NDI BSS */
static inline QDF_STATUS csr_roam_start_ndi(tpAniSirGlobal mac_ctx,
					uint32_t session_id,
					tCsrRoamProfile *profile)
{
	return QDF_STATUS_SUCCESS;
}

/* Fill in ndp information in roam_info */
static inline void csr_roam_fill_roaminfo_ndp(tpAniSirGlobal mac_ctx,
					      tCsrRoamInfo *roam_info,
					      eCsrRoamResult roam_result,
					      tSirResultCodes status_code,
					      uint32_t reason_code,
					      uint32_t transaction_id)
{
}

static inline void csr_roam_save_ndi_connected_info(tpAniSirGlobal mac_ctx,
					uint32_t session_id,
					tCsrRoamProfile *roam_profile,
					tSirBssDescription *bss_desc)
{
}

static inline void csr_roam_update_ndp_return_params(tpAniSirGlobal mac_ctx,
					uint32_t result,
					uint32_t *roam_status,
					uint32_t *roam_result,
					struct tagCsrRoamInfo *roam_info)
{
}

/* NaN indication response handler */
QDF_STATUS sme_ndp_end_req_handler(uint32_t session_id,
					struct ndp_end_req *req_params);

/* NaN schedule update request handler */
QDF_STATUS sme_ndp_sched_req_handler(uint32_t session_id,
				struct ndp_schedule_update_req *req_params);

static inline eHalStatus csr_process_ndp_initiator_request(
				tpAniSirGlobal mac_ctx, tSmeCmd *cmd)
{
	return QDF_STATUS_SUCCESS;
}

static inline void sme_ndp_msg_processor(tpAniSirGlobal mac_ctx, cds_msg_t *msg)
{
}

#endif /* WLAN_FEATURE_NAN_DATAPATH */
#endif /* __SME_NAN_DATAPATH_H */
