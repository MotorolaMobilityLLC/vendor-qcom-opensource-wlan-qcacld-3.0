/*
 * Copyright (c) 2015-2016 The Linux Foundation. All rights reserved.
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

#include "sme_power_save.h"
#include "sme_power_save_api.h"
#include "sms_debug.h"
#include "cdf_memory.h"
#include "qdf_types.h"
#include "wma_types.h"
#include "wmm_apsd.h"
#include "cfg_api.h"
#include "csr_inside_api.h"

/**
 * sme_post_ps_msg_to_wma(): post message to WMA.
 * @type: type
 * @body: body pointer
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_post_ps_msg_to_wma(uint16_t type, void *body)
{
	cds_msg_t msg;

	msg.type = type;
	msg.reserved = 0;
	msg.bodyptr = body;
	msg.bodyval = 0;

	if (QDF_STATUS_SUCCESS != cds_mq_post_message(
				QDF_MODULE_ID_WMA, &msg)) {
		CDF_TRACE(QDF_MODULE_ID_SME, CDF_TRACE_LEVEL_ERROR,
				"%s: Posting message %d failed",
				__func__, type);
		cdf_mem_free(body);
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * sme_ps_enable_ps_req_params(): enables power save req params
 * @mac_ctx: global mac context
 * @session_id: session id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_ps_enable_ps_req_params(tpAniSirGlobal mac_ctx,
		uint32_t session_id)
{
	struct sEnablePsParams *enable_ps_req_params;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	enable_ps_req_params =  cdf_mem_malloc(sizeof(*enable_ps_req_params));
	if (NULL == enable_ps_req_params) {
		sms_log(mac_ctx, LOGE,
			FL("Memory allocation failed for enable_ps_req_params"));
		return QDF_STATUS_E_NOMEM;
	}
	enable_ps_req_params->psSetting = eSIR_ADDON_NOTHING;
	enable_ps_req_params->sessionid = session_id;

	status = sme_post_ps_msg_to_wma(WMA_ENTER_PS_REQ, enable_ps_req_params);
	if (!QDF_IS_STATUS_SUCCESS(status))
		return QDF_STATUS_E_FAILURE;
	CDF_TRACE(QDF_MODULE_ID_SME, CDF_TRACE_LEVEL_INFO,
		FL("Message WMA_ENTER_PS_REQ Successfully sent to WMA"));
	return QDF_STATUS_SUCCESS;
}

/**
 * sme_ps_disable_ps_req_params(): Disable power save req params
 * @mac_ctx: global mac context
 * @session_id: session id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_ps_disable_ps_req_params(tpAniSirGlobal mac_ctx,
		uint32_t session_id)
{
	struct  sDisablePsParams *disable_ps_req_params;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	disable_ps_req_params = cdf_mem_malloc(sizeof(*disable_ps_req_params));
	if (NULL == disable_ps_req_params) {
		sms_log(mac_ctx, LOGE,
			FL("Memory allocation failed for sDisablePsParams"));
		return QDF_STATUS_E_NOMEM;
	}

	disable_ps_req_params->psSetting = eSIR_ADDON_NOTHING;
	disable_ps_req_params->sessionid = session_id;

	status = sme_post_ps_msg_to_wma(WMA_EXIT_PS_REQ, disable_ps_req_params);
	if (!QDF_IS_STATUS_SUCCESS(status))
		return QDF_STATUS_E_FAILURE;
	CDF_TRACE(QDF_MODULE_ID_SME, CDF_TRACE_LEVEL_INFO,
			FL("Message WMA_EXIT_PS_REQ Successfully sent to WMA"));
	return QDF_STATUS_SUCCESS;
}

/**
 * sme_ps_enable_uapsd_req_params(): enables UASPD req params
 * @mac_ctx: global mac context
 * @session_id: session id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_ps_enable_uapsd_req_params(tpAniSirGlobal mac_ctx,
		uint32_t session_id)
{

	struct sEnableUapsdParams *enable_uapsd_req_params;
	uint8_t uapsd_delivery_mask = 0;
	uint8_t uapsd_trigger_mask = 0;
	struct ps_global_info *ps_global_info = &mac_ctx->sme.ps_global_info;
	struct ps_params *ps_param = &ps_global_info->ps_params[session_id];
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	enable_uapsd_req_params =
		cdf_mem_malloc(sizeof(*enable_uapsd_req_params));
	if (NULL == enable_uapsd_req_params) {
		sms_log(mac_ctx, LOGE,
			FL("Memory allocation failed for enable_uapsd_req_params"));
		return QDF_STATUS_E_NOMEM;
	}


	uapsd_delivery_mask =
		ps_param->uapsd_per_ac_bit_mask |
		ps_param->uapsd_per_ac_delivery_enable_mask;

	uapsd_trigger_mask =
		ps_param->uapsd_per_ac_bit_mask |
		ps_param->uapsd_per_ac_trigger_enable_mask;


	enable_uapsd_req_params->uapsdParams.bkDeliveryEnabled =
		LIM_UAPSD_GET(ACBK, uapsd_delivery_mask);

	enable_uapsd_req_params->uapsdParams.beDeliveryEnabled =
		LIM_UAPSD_GET(ACBE, uapsd_delivery_mask);

	enable_uapsd_req_params->uapsdParams.viDeliveryEnabled =
		LIM_UAPSD_GET(ACVI, uapsd_delivery_mask);

	enable_uapsd_req_params->uapsdParams.voDeliveryEnabled =
		LIM_UAPSD_GET(ACVO, uapsd_delivery_mask);

	enable_uapsd_req_params->uapsdParams.bkTriggerEnabled =
		LIM_UAPSD_GET(ACBK, uapsd_trigger_mask);

	enable_uapsd_req_params->uapsdParams.beTriggerEnabled =
		LIM_UAPSD_GET(ACBE, uapsd_trigger_mask);

	enable_uapsd_req_params->uapsdParams.viTriggerEnabled =
		LIM_UAPSD_GET(ACVI, uapsd_trigger_mask);

	enable_uapsd_req_params->uapsdParams.voTriggerEnabled =
		LIM_UAPSD_GET(ACVO, uapsd_trigger_mask);

	enable_uapsd_req_params->sessionid = session_id;

	status = sme_post_ps_msg_to_wma(WMA_ENABLE_UAPSD_REQ,
					enable_uapsd_req_params);
	if (!QDF_IS_STATUS_SUCCESS(status))
		return QDF_STATUS_E_FAILURE;

	CDF_TRACE(QDF_MODULE_ID_SME, CDF_TRACE_LEVEL_INFO,
		    FL("Msg WMA_ENABLE_UAPSD_REQ Successfully sent to WMA"));
	return QDF_STATUS_SUCCESS;
}

/**
 * sme_ps_disable_uapsd_req_params(): disables UASPD req params
 * @mac_ctx: global mac context
 * @session_id: session id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_ps_disable_uapsd_req_params(tpAniSirGlobal mac_ctx,
		uint32_t session_id)
{
	struct sDisableUapsdParams *disable_uapsd_req_params;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	disable_uapsd_req_params =
		cdf_mem_malloc(sizeof(*disable_uapsd_req_params));
	if (NULL == disable_uapsd_req_params) {
		sms_log(mac_ctx, LOGE,
			FL("Memory allocation failed for disable_uapsd_req_params"));
		return QDF_STATUS_E_NOMEM;
	}

	disable_uapsd_req_params->sessionid = session_id;
	status = sme_post_ps_msg_to_wma(WMA_DISABLE_UAPSD_REQ,
					disable_uapsd_req_params);
	if (!QDF_IS_STATUS_SUCCESS(status))
		return QDF_STATUS_E_FAILURE;

	CDF_TRACE(QDF_MODULE_ID_SME, CDF_TRACE_LEVEL_INFO,
		FL("Message WMA_DISABLE_UAPSD_REQ Successfully sent to WMA"));
	return QDF_STATUS_SUCCESS;
}

/**
 * sme_ps_enter_wowl_req_params(): enable WOWL req Parama
 * @mac_ctx: global mac context
 * @session_id: session id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_ps_enter_wowl_req_params(tpAniSirGlobal mac_ctx,
		uint32_t session_id)
{
	struct sSirHalWowlEnterParams *hal_wowl_params;
	struct sSirSmeWowlEnterParams *sme_wowl_params;
	uint32_t cfg_value = 0;
	struct ps_global_info *ps_global_info = &mac_ctx->sme.ps_global_info;

	sme_wowl_params =
		&ps_global_info->ps_params[session_id].wowl_enter_params;

	hal_wowl_params = cdf_mem_malloc(sizeof(*hal_wowl_params));
	if (NULL == hal_wowl_params) {
		sms_log(mac_ctx, LOGP,
			FL("Fail to allocate memory for Enter Wowl Request"));
		return  QDF_STATUS_E_NOMEM;
	}
	cdf_mem_set((uint8_t *) hal_wowl_params, sizeof(*hal_wowl_params), 0);

	/* fill in the message field */
	hal_wowl_params->ucMagicPktEnable = sme_wowl_params->ucMagicPktEnable;
	hal_wowl_params->ucPatternFilteringEnable =
		sme_wowl_params->ucPatternFilteringEnable;
	cdf_copy_macaddr(&hal_wowl_params->magic_ptrn,
			 &sme_wowl_params->magic_ptrn);

#ifdef WLAN_WAKEUP_EVENTS
	hal_wowl_params->ucWoWEAPIDRequestEnable =
		sme_wowl_params->ucWoWEAPIDRequestEnable;
	hal_wowl_params->ucWoWEAPOL4WayEnable =
		sme_wowl_params->ucWoWEAPOL4WayEnable;
	hal_wowl_params->ucWowNetScanOffloadMatch =
		sme_wowl_params->ucWowNetScanOffloadMatch;
	hal_wowl_params->ucWowGTKRekeyError =
		sme_wowl_params->ucWowGTKRekeyError;
	hal_wowl_params->ucWoWBSSConnLoss =
		sme_wowl_params->ucWoWBSSConnLoss;
#endif /* WLAN_WAKEUP_EVENTS */

	if (wlan_cfg_get_int
			(mac_ctx, WNI_CFG_WOWLAN_UCAST_PATTERN_FILTER_ENABLE,
			 &cfg_value) != eSIR_SUCCESS) {
		sms_log(mac_ctx, LOGP,
			FL("cfgGet failed for WNI_CFG_WOWLAN_UCAST_PATTERN_FILTER_ENABLE"));
		goto end;
	}
	hal_wowl_params->ucUcastPatternFilteringEnable = (uint8_t) cfg_value;

	if (wlan_cfg_get_int
			(mac_ctx, WNI_CFG_WOWLAN_CHANNEL_SWITCH_ENABLE,
			 &cfg_value) != eSIR_SUCCESS) {
		sms_log(mac_ctx, LOGP,
			FL("cfgGet failed for WNI_CFG_WOWLAN_CHANNEL_SWITCH_ENABLE"));
		goto end;
	}
	hal_wowl_params->ucWowChnlSwitchRcv = (uint8_t) cfg_value;

	if (wlan_cfg_get_int
			(mac_ctx, WNI_CFG_WOWLAN_DEAUTH_ENABLE, &cfg_value) !=
			eSIR_SUCCESS) {
		sms_log(mac_ctx, LOGP,
			FL("cfgGet failed for WNI_CFG_WOWLAN_DEAUTH_ENABLE "));
		goto end;
	}
	hal_wowl_params->ucWowDeauthRcv = (uint8_t) cfg_value;

	if (wlan_cfg_get_int
			(mac_ctx, WNI_CFG_WOWLAN_DISASSOC_ENABLE, &cfg_value) !=
			eSIR_SUCCESS) {
		sms_log(mac_ctx, LOGP,
		      FL("cfgGet failed for WNI_CFG_WOWLAN_DISASSOC_ENABLE "));
		goto end;
	}
	hal_wowl_params->ucWowDisassocRcv = (uint8_t) cfg_value;

	if (wlan_cfg_get_int(mac_ctx, WNI_CFG_WOWLAN_MAX_MISSED_BEACON,
				&cfg_value) !=	eSIR_SUCCESS) {
		sms_log(mac_ctx, LOGP,
		    FL("cfgGet failed for WNI_CFG_WOWLAN_MAX_MISSED_BEACON "));
		goto end;
	}
	hal_wowl_params->ucWowMaxMissedBeacons = (uint8_t) cfg_value;

	if (wlan_cfg_get_int(mac_ctx, WNI_CFG_WOWLAN_MAX_SLEEP_PERIOD,
				&cfg_value) != eSIR_SUCCESS) {
		sms_log(mac_ctx, LOGP,
		     FL("cfgGet failed for WNI_CFG_WOWLAN_MAX_SLEEP_PERIOD "));
		goto end;
	}
	hal_wowl_params->ucWowMaxSleepUsec = (uint8_t) cfg_value;

	hal_wowl_params->sessionId = sme_wowl_params->sessionId;

	if (QDF_STATUS_SUCCESS == sme_post_ps_msg_to_wma(WMA_WOWL_ENTER_REQ,
							hal_wowl_params)){
		CDF_TRACE(QDF_MODULE_ID_SME, CDF_TRACE_LEVEL_INFO,
			FL("Msg WMA_WOWL_ENTER_REQ Successfully sent to WMA"));
		return QDF_STATUS_SUCCESS;
	} else
		goto end;

end:
	if (hal_wowl_params != NULL)
		cdf_mem_free(hal_wowl_params);
	return QDF_STATUS_E_FAILURE;
}

/**
 * sme_ps_exit_wowl_req_params(): Exit WOWL req params
 * @mac_ctx: global mac context
 * @session_id: session id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_ps_exit_wowl_req_params(tpAniSirGlobal mac_ctx,
		uint32_t session_id)
{
	struct sSirHalWowlExitParams *hal_wowl_msg;
	hal_wowl_msg = cdf_mem_malloc(sizeof(*hal_wowl_msg));
	if (NULL == hal_wowl_msg) {
		sms_log(mac_ctx, LOGP,
			FL("Fail to allocate memory for WoWLAN Add Bcast Pattern "));
		return  QDF_STATUS_E_NOMEM;
	}
	cdf_mem_set((uint8_t *) hal_wowl_msg,
			sizeof(*hal_wowl_msg), 0);
	hal_wowl_msg->sessionId = session_id;

	if (QDF_STATUS_SUCCESS == sme_post_ps_msg_to_wma(WMA_WOWL_EXIT_REQ,
							hal_wowl_msg)){
		CDF_TRACE(QDF_MODULE_ID_SME, CDF_TRACE_LEVEL_INFO,
			FL("Msg WMA_WOWL_EXIT_REQ Successfully sent to WMA"));
		return QDF_STATUS_SUCCESS;
	}
	if (hal_wowl_msg != NULL)
		cdf_mem_free(hal_wowl_msg);
	return QDF_STATUS_E_FAILURE;
}

/**
 * sme_ps_process_command(): Sme process power save messages
 *			and pass messages to WMA.
 * @mac_ctx: global mac context
 * @session_id: session id
 * sme_ps_cmd: power save message
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_ps_process_command(tpAniSirGlobal mac_ctx, uint32_t session_id,
		enum sme_ps_cmd command)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!CSR_IS_SESSION_VALID(mac_ctx, session_id)) {
		sms_log(mac_ctx, LOGE, "Invalid Session_id %x", session_id);
		return eSIR_FAILURE;
	}
	CDF_TRACE(QDF_MODULE_ID_SME, CDF_TRACE_LEVEL_INFO,
			FL("Power Save command %d"), command);
	switch (command) {
	case SME_PS_ENABLE:
		status = sme_ps_enable_ps_req_params(mac_ctx, session_id);
		break;
	case SME_PS_DISABLE:
		status = sme_ps_disable_ps_req_params(mac_ctx, session_id);
		break;
	case SME_PS_UAPSD_ENABLE:
		status = sme_ps_enable_uapsd_req_params(mac_ctx, session_id);
		break;
	case SME_PS_UAPSD_DISABLE:
		status = sme_ps_disable_uapsd_req_params(mac_ctx, session_id);
		break;
	case SME_PS_WOWL_ENTER:
		status = sme_ps_enter_wowl_req_params(mac_ctx, session_id);
		break;
	case SME_PS_WOWL_EXIT:
		status = sme_ps_exit_wowl_req_params(mac_ctx, session_id);
		break;

	default:
		sms_log(mac_ctx, LOGE, FL("Invalid command type %d"),
				command);
		status = QDF_STATUS_E_FAILURE;
		break;
	}
	if (status != QDF_STATUS_SUCCESS) {
		CDF_TRACE(QDF_MODULE_ID_SME, CDF_TRACE_LEVEL_ERROR,
			FL("Not able to enter in PS, Command: %d"), command);
	}
	return status;
}

/**
 * sme_enable_sta_ps_check(): Checks if it is ok to enable power save or not.
 * @mac_ctx: global mac context
 * @session_id: session id
 *
 *Pre Condition for enabling sta mode power save
 *1) Sta Mode Ps should be enabled in ini file.
 *2) Session should be in infra mode and in connected state.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_enable_sta_ps_check(tpAniSirGlobal mac_ctx, uint32_t session_id)
{
	struct ps_global_info *ps_global_info = &mac_ctx->sme.ps_global_info;

	/* Check if Sta Ps is enabled. */
	if (!ps_global_info->ps_enabled) {
		sms_log(mac_ctx, LOG1,
			"Cannot initiate PS. PS is disabled in ini");
		return QDF_STATUS_E_FAILURE;
	}

	/* Check whether the given session is Infra and in Connected State */
	if (!csr_is_conn_state_connected_infra(mac_ctx, session_id)) {
		sms_log(mac_ctx, LOGE, "Sta not infra/connected state %d",
				session_id);
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;

}

/**
 * sme_ps_enable_disable(): function to enable/disable PS.
 * @hal_ctx: global hal_handle
 * @session_id: session id
 * sme_ps_cmd: power save message
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_ps_enable_disable(tHalHandle hal_ctx, uint32_t session_id,
		enum sme_ps_cmd command)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	tpAniSirGlobal mac_ctx = PMAC_STRUCT(hal_ctx);
	status =  sme_enable_sta_ps_check(mac_ctx, session_id);
	if (status != QDF_STATUS_SUCCESS)
		return status;
	status = sme_ps_process_command(mac_ctx, session_id, command);
	return status;
}

/**
 * sme_ps_uapsd_enable(): function to enable UAPSD.
 * @hal_ctx: global hal_handle
 * @session_id: session id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_ps_uapsd_enable(tHalHandle hal_ctx, uint32_t session_id)
{

	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	tpAniSirGlobal mac_ctx = PMAC_STRUCT(hal_ctx);
	status =  sme_enable_sta_ps_check(mac_ctx, session_id);
	if (status != QDF_STATUS_SUCCESS)
		return status;
	status = sme_ps_process_command(mac_ctx, session_id,
			SME_PS_UAPSD_ENABLE);
	if (status == QDF_STATUS_SUCCESS)
		sme_offload_qos_process_into_uapsd_mode(mac_ctx, session_id);

	return status;
}

/**
 * sme_ps_uapsd_disable(): function to disable UAPSD.
 * @hal_ctx: global hal_handle
 * @session_id: session id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_ps_uapsd_disable(tHalHandle hal_ctx, uint32_t session_id)
{

	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	tpAniSirGlobal mac_ctx = PMAC_STRUCT(hal_ctx);
	status =  sme_enable_sta_ps_check(mac_ctx, session_id);
	if (status != QDF_STATUS_SUCCESS)
		return status;
	status = sme_ps_process_command(mac_ctx, session_id,
			SME_PS_UAPSD_DISABLE);
	if (status == QDF_STATUS_SUCCESS)
		sme_offload_qos_process_out_of_uapsd_mode(mac_ctx, session_id);

	return status;
}

/**
 * sme_set_tspec_uapsd_mask_per_session(): set tspec UAPSD mask per session
 * @mac_ctx: global mac context
 * @ts_info: tspec info.
 * @session_id: session id
 *
 * Return: QDF_STATUS
 */
void sme_set_tspec_uapsd_mask_per_session(tpAniSirGlobal mac_ctx,
		tSirMacTSInfo *ts_info,
		uint8_t session_id)
{
	uint8_t user_prio = (uint8_t) ts_info->traffic.userPrio;
	uint16_t direction = ts_info->traffic.direction;
	uint8_t ac = upToAc(user_prio);
	struct ps_global_info *ps_global_info = &mac_ctx->sme.ps_global_info;
	struct ps_params *ps_param = &ps_global_info->ps_params[session_id];
	sms_log(mac_ctx, LOGE, FL("Set UAPSD mask for AC %d, dir %d, action=%d")
			, ac, direction, ts_info->traffic.psb);

	/* Converting AC to appropriate Uapsd Bit Mask
	 * AC_BE(0) --> UAPSD_BITOFFSET_ACVO(3)
	 * AC_BK(1) --> UAPSD_BITOFFSET_ACVO(2)
	 * AC_VI(2) --> UAPSD_BITOFFSET_ACVO(1)
	 * AC_VO(3) --> UAPSD_BITOFFSET_ACVO(0)
	 */
	ac = ((~ac) & 0x3);
	if (ts_info->traffic.psb) {
		if (direction == SIR_MAC_DIRECTION_UPLINK)
			ps_param->uapsd_per_ac_trigger_enable_mask |=
				(1 << ac);
		else if (direction == SIR_MAC_DIRECTION_DNLINK)
			ps_param->uapsd_per_ac_delivery_enable_mask |=
				(1 << ac);
		else if (direction == SIR_MAC_DIRECTION_BIDIR) {
			ps_param->uapsd_per_ac_trigger_enable_mask |=
				(1 << ac);
			ps_param->uapsd_per_ac_delivery_enable_mask |=
				(1 << ac);
		}
	} else {
		if (direction == SIR_MAC_DIRECTION_UPLINK)
			ps_param->uapsd_per_ac_trigger_enable_mask &=
				~(1 << ac);
		else if (direction == SIR_MAC_DIRECTION_DNLINK)
			ps_param->uapsd_per_ac_delivery_enable_mask &=
				~(1 << ac);
		else if (direction == SIR_MAC_DIRECTION_BIDIR) {
			ps_param->uapsd_per_ac_trigger_enable_mask &=
				~(1 << ac);
			ps_param->uapsd_per_ac_delivery_enable_mask &=
				~(1 << ac);
		}
	}

	/*
	 * ADDTS success, so AC is now admitted. We shall now use the default
	 * EDCA parameters as advertised by AP and send the updated EDCA params
	 * to HAL.
	 */
	if (direction == SIR_MAC_DIRECTION_UPLINK) {
		ps_param->ac_admit_mask[SIR_MAC_DIRECTION_UPLINK] |=
			(1 << ac);
	} else if (direction == SIR_MAC_DIRECTION_DNLINK) {
		ps_param->ac_admit_mask[SIR_MAC_DIRECTION_DNLINK] |=
			(1 << ac);
	} else if (direction == SIR_MAC_DIRECTION_BIDIR) {
		ps_param->ac_admit_mask[SIR_MAC_DIRECTION_UPLINK] |=
			(1 << ac);
		ps_param->ac_admit_mask[SIR_MAC_DIRECTION_DNLINK] |=
			(1 << ac);
	}

	sms_log(mac_ctx, LOG1,
		FL("New ps_param->uapsd_per_ac_trigger_enable_mask = 0x%x "),
		ps_param->uapsd_per_ac_trigger_enable_mask);
	sms_log(mac_ctx, LOG1,
		FL("New  ps_param->uapsd_per_ac_delivery_enable_mask = 0x%x "),
		ps_param->uapsd_per_ac_delivery_enable_mask);
	sms_log(mac_ctx, LOG1,
		FL("New ps_param->ac_admit_mask[SIR_MAC_DIRECTION_UPLINK] = 0x%x "),
		ps_param->ac_admit_mask[SIR_MAC_DIRECTION_UPLINK]);
	return;
}

/**
 * sme_ps_start_uapsd(): function to start UAPSD.
 * @hal_ctx: global hal_handle
 * @session_id: session id
 * @uapsd_start_ind_cb: uapsd start indiation cb
 * @callback_context: callback context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_ps_start_uapsd(tHalHandle hal_ctx, uint32_t session_id,
		uapsd_start_indication_cb uapsd_start_ind_cb,
		void *callback_context)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	status = sme_ps_uapsd_enable(hal_ctx, session_id);
	return status;
}

#ifdef FEATURE_WLAN_SCAN_PNO
static tSirRetStatus
sme_populate_mac_header(tpAniSirGlobal mac_ctx,
			uint8_t *bd,
			uint8_t type,
			uint8_t sub_type,
			tSirMacAddr peer_addr, tSirMacAddr self_mac_addr)
{
	tSirRetStatus status_code = eSIR_SUCCESS;
	tpSirMacMgmtHdr mac_hdr;

	/* / Prepare MAC management header */
	mac_hdr = (tpSirMacMgmtHdr) (bd);

	/* Prepare FC */
	mac_hdr->fc.protVer = SIR_MAC_PROTOCOL_VERSION;
	mac_hdr->fc.type = type;
	mac_hdr->fc.subType = sub_type;

	/* Prepare Address 1 */
	cdf_mem_copy((uint8_t *) mac_hdr->da, (uint8_t *) peer_addr,
		     sizeof(tSirMacAddr));

	sir_copy_mac_addr(mac_hdr->sa, self_mac_addr);

	/* Prepare Address 3 */
	cdf_mem_copy((uint8_t *) mac_hdr->bssId, (uint8_t *) peer_addr,
		     sizeof(tSirMacAddr));
	return status_code;
} /*** sme_populate_mac_header() ***/

static tSirRetStatus
sme_prepare_probe_req_template(tpAniSirGlobal mac_ctx,
			       uint8_t channel_num,
			       uint32_t dot11mode,
			       tSirMacAddr self_mac_addr,
			       uint8_t *frame,
			       uint16_t *pus_len, tCsrRoamSession *psession)
{
	tDot11fProbeRequest pr;
	uint32_t status, bytes, payload;
	tSirRetStatus sir_status;
	/*Bcast tx */
	tSirMacAddr bss_id = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	/**
	 * The scheme here is to fill out a 'tDot11fProbeRequest' structure
	 * and then hand it off to 'dot11f_pack_probe_request' (for
	 * serialization).  We start by zero-initializing the structure:
	 */
	cdf_mem_set((uint8_t *) &pr, sizeof(pr), 0);

	populate_dot11f_supp_rates(mac_ctx, channel_num, &pr.SuppRates, NULL);

	if (WNI_CFG_DOT11_MODE_11B != dot11mode) {
		populate_dot11f_ext_supp_rates1(mac_ctx, channel_num,
						&pr.ExtSuppRates);
	}

	if (IS_DOT11_MODE_HT(dot11mode)) {
		populate_dot11f_ht_caps(mac_ctx, NULL, &pr.HTCaps);
		pr.HTCaps.advCodingCap = psession->htConfig.ht_rx_ldpc;
		pr.HTCaps.txSTBC = psession->htConfig.ht_tx_stbc;
		pr.HTCaps.rxSTBC = psession->htConfig.ht_rx_stbc;
		if (!psession->htConfig.ht_sgi)
			pr.HTCaps.shortGI20MHz = pr.HTCaps.shortGI40MHz = 0;
	}
	/**
	 * That's it-- now we pack it.  First, how much space are we going to
	 * need?
	 */
	status = dot11f_get_packed_probe_request_size(mac_ctx, &pr, &payload);
	if (DOT11F_FAILED(status)) {
		CDF_TRACE(QDF_MODULE_ID_SME, CDF_TRACE_LEVEL_ERROR,
			  FL("Failed to calculate the packed size for a Probe Request (0x%08x)."),
				  status);

		/* We'll fall back on the worst case scenario: */
		payload = sizeof(tDot11fProbeRequest);
	} else if (DOT11F_WARNED(status)) {
		CDF_TRACE(QDF_MODULE_ID_SME, CDF_TRACE_LEVEL_ERROR,
			  FL("There were warnings while calculating the packed size for a Probe Request (0x%08x)."),
			  status);
	}

	bytes = payload + sizeof(tSirMacMgmtHdr);

	/* Prepare outgoing frame */
	cdf_mem_set(frame, bytes, 0);

	/* Next, we fill out the buffer descriptor: */
	sir_status = sme_populate_mac_header(mac_ctx, frame, SIR_MAC_MGMT_FRAME,
					     SIR_MAC_MGMT_PROBE_REQ, bss_id,
					     self_mac_addr);

	if (eSIR_SUCCESS != sir_status) {
		CDF_TRACE(QDF_MODULE_ID_SME, CDF_TRACE_LEVEL_ERROR,
		FL("Failed to populate the buffer descriptor for a Probe Request (%d)."),
			sir_status);
		return sir_status;      /* allocated! */
	}
	/* That done, pack the Probe Request: */
	status = dot11f_pack_probe_request(mac_ctx, &pr, frame +
					    sizeof(tSirMacMgmtHdr),
					    payload, &payload);
	if (DOT11F_FAILED(status)) {
		CDF_TRACE(QDF_MODULE_ID_SME, CDF_TRACE_LEVEL_ERROR,
			  "Failed to pack a Probe Request (0x%08x).", status);
		return eSIR_FAILURE;    /* allocated! */
	} else if (DOT11F_WARNED(status)) {
		CDF_TRACE(QDF_MODULE_ID_SME, CDF_TRACE_LEVEL_ERROR,
			  "There were warnings while packing a Probe Request");
	}

	*pus_len = payload + sizeof(tSirMacMgmtHdr);
	return eSIR_SUCCESS;
} /* End sme_prepare_probe_req_template. */
/**
 * sme_set_pno_channel_prediction() - Prepare PNO buffer
 * @request_buf:        Buffer to be filled up to send to WMA
 * @mac_ctx:            MAC context
 *
 * Fill up the PNO buffer with the channel prediction configuration
 * parameters and send them to WMA
 *
 * Return: None
 **/
void sme_set_pno_channel_prediction(tpSirPNOScanReq request_buf,
		tpAniSirGlobal mac_ctx)
{
	request_buf->pno_channel_prediction =
		mac_ctx->roam.configParam.pno_channel_prediction;
	request_buf->top_k_num_of_channels =
		mac_ctx->roam.configParam.top_k_num_of_channels;
	request_buf->stationary_thresh =
		mac_ctx->roam.configParam.stationary_thresh;
	request_buf->channel_prediction_full_scan =
		mac_ctx->roam.configParam.channel_prediction_full_scan;
	CDF_TRACE(QDF_MODULE_ID_SME, CDF_TRACE_LEVEL_DEBUG,
			FL("channel_prediction: %d, top_k_num_of_channels: %d"),
			request_buf->pno_channel_prediction,
			request_buf->top_k_num_of_channels);
	CDF_TRACE(QDF_MODULE_ID_SME, CDF_TRACE_LEVEL_DEBUG,
			FL("stationary_thresh: %d, ch_predict_full_scan: %d"),
			request_buf->stationary_thresh,
			request_buf->channel_prediction_full_scan);
}
QDF_STATUS sme_set_ps_preferred_network_list(tHalHandle hal_ctx,
		tpSirPNOScanReq request,
		uint8_t session_id,
		preferred_network_found_ind_cb callback_routine,
		void *callback_context)
{
	tpSirPNOScanReq request_buf;
	cds_msg_t msg;
	tpAniSirGlobal mac_ctx = PMAC_STRUCT(hal_ctx);
	tCsrRoamSession *session = CSR_GET_SESSION(mac_ctx, session_id);
	uint8_t uc_dot11_mode;

	if (NULL == session) {
		CDF_TRACE(QDF_MODULE_ID_SME, CDF_TRACE_LEVEL_ERROR,
				"%s: session is NULL", __func__);
		return QDF_STATUS_E_FAILURE;
	}
	CDF_TRACE(QDF_MODULE_ID_SME, CDF_TRACE_LEVEL_INFO,
			"%s: SSID = 0x%08x%08x%08x%08x%08x%08x%08x%08x, 0x%08x%08x%08x%08x%08x%08x%08x%08x", __func__,
			*((uint32_t *) &request->aNetworks[0].ssId.ssId[0]),
			*((uint32_t *) &request->aNetworks[0].ssId.ssId[4]),
			*((uint32_t *) &request->aNetworks[0].ssId.ssId[8]),
			*((uint32_t *) &request->aNetworks[0].ssId.ssId[12]),
			*((uint32_t *) &request->aNetworks[0].ssId.ssId[16]),
			*((uint32_t *) &request->aNetworks[0].ssId.ssId[20]),
			*((uint32_t *) &request->aNetworks[0].ssId.ssId[24]),
			*((uint32_t *) &request->aNetworks[0].ssId.ssId[28]),
			*((uint32_t *) &request->aNetworks[1].ssId.ssId[0]),
			*((uint32_t *) &request->aNetworks[1].ssId.ssId[4]),
			*((uint32_t *) &request->aNetworks[1].ssId.ssId[8]),
			*((uint32_t *) &request->aNetworks[1].ssId.ssId[12]),
			*((uint32_t *) &request->aNetworks[1].ssId.ssId[16]),
			*((uint32_t *) &request->aNetworks[1].ssId.ssId[20]),
			*((uint32_t *) &request->aNetworks[1].ssId.ssId[24]),
			*((uint32_t *) &request->aNetworks[1].ssId.ssId[28]));

	if (!session) {
		CDF_TRACE(QDF_MODULE_ID_SME, CDF_TRACE_LEVEL_ERROR,
				"%s: session is NULL", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	request_buf = cdf_mem_malloc(sizeof(tSirPNOScanReq));
	if (NULL == request_buf) {
		CDF_TRACE(QDF_MODULE_ID_SME, CDF_TRACE_LEVEL_ERROR,
			FL("Not able to allocate memory for PNO request"));
		return QDF_STATUS_E_NOMEM;
	}

	cdf_mem_copy(request_buf, request, sizeof(tSirPNOScanReq));

	/*Must translate the mode first */
	uc_dot11_mode = (uint8_t) csr_translate_to_wni_cfg_dot11_mode(mac_ctx,
			csr_find_best_phy_mode
			(mac_ctx,
			 mac_ctx->roam.
			 configParam.
			 phyMode));

	/*Prepare a probe request for 2.4GHz band and one for 5GHz band */
	if (eSIR_SUCCESS ==
			sme_prepare_probe_req_template(mac_ctx,
				SIR_PNO_24G_DEFAULT_CH,
				uc_dot11_mode, session->selfMacAddr.bytes,
				request_buf->p24GProbeTemplate,
				&request_buf->us24GProbeTemplateLen,
				session)) {
		/* Append IE passed by supplicant(if any)
		 * to probe request
		 */
		if ((0 < request->us24GProbeTemplateLen) &&
				((request_buf->us24GProbeTemplateLen +
				  request->us24GProbeTemplateLen) <
				 SIR_PNO_MAX_PB_REQ_SIZE)) {
			cdf_mem_copy((uint8_t *) &request_buf->
					p24GProbeTemplate +
					request_buf->us24GProbeTemplateLen,
					(uint8_t *) &request->p24GProbeTemplate,
					request->us24GProbeTemplateLen);
			request_buf->us24GProbeTemplateLen +=
				request->us24GProbeTemplateLen;
			CDF_TRACE(QDF_MODULE_ID_SME,
				CDF_TRACE_LEVEL_INFO,
				FL("request->us24GProbeTemplateLen = %d"),
				request->us24GProbeTemplateLen);
		} else {
			CDF_TRACE(QDF_MODULE_ID_SME,
				CDF_TRACE_LEVEL_INFO,
				FL("Extra ie discarded on 2.4G, IE len = %d"),
				request->us24GProbeTemplateLen);
		}
	}

	if (eSIR_SUCCESS ==
			sme_prepare_probe_req_template(mac_ctx,
				SIR_PNO_5G_DEFAULT_CH, uc_dot11_mode,
				session->selfMacAddr.bytes,
				request_buf->p5GProbeTemplate,
				&request_buf->us5GProbeTemplateLen,
				session)) {
		/* Append IE passed by supplicant(if any)
		 * to probe request
		 */
		if ((0 < request->us5GProbeTemplateLen) &&
				((request_buf->us5GProbeTemplateLen +
				  request->us5GProbeTemplateLen) <
				 SIR_PNO_MAX_PB_REQ_SIZE)) {
			cdf_mem_copy((uint8_t *) &request_buf->
					p5GProbeTemplate +
					request_buf->us5GProbeTemplateLen,
					(uint8_t *) &request->p5GProbeTemplate,
					request->us5GProbeTemplateLen);
			request_buf->us5GProbeTemplateLen +=
				request->us5GProbeTemplateLen;
			CDF_TRACE(QDF_MODULE_ID_SME, CDF_TRACE_LEVEL_INFO,
				FL("request_buf->us5GProbeTemplateLen = %d"),
				request->us5GProbeTemplateLen);
		} else {
			CDF_TRACE(QDF_MODULE_ID_SME, CDF_TRACE_LEVEL_INFO,
				FL("Extra IE discarded on 5G, IE length = %d"),
				request->us5GProbeTemplateLen);
		}
	}

	if (mac_ctx->pnoOffload) {
		if (request_buf->enable)
			session->pnoStarted = true;
		else
			session->pnoStarted = false;

		request_buf->sessionId = session_id;
	}
	sme_set_pno_channel_prediction(request_buf, mac_ctx);

	if (csr_is_p2p_session_connected(mac_ctx)) {
		/* if AP-STA concurrency is active */
		request_buf->active_max_time =
			mac_ctx->roam.configParam.nActiveMaxChnTimeConc;
		request_buf->active_min_time =
			mac_ctx->roam.configParam.nActiveMinChnTimeConc;
		request_buf->passive_max_time =
			mac_ctx->roam.configParam.nPassiveMaxChnTimeConc;
		request_buf->passive_min_time =
			mac_ctx->roam.configParam.nPassiveMinChnTimeConc;
	} else {
		request_buf->active_max_time =
			mac_ctx->roam.configParam.nActiveMaxChnTime;
		request_buf->active_min_time =
			mac_ctx->roam.configParam.nActiveMinChnTime;
		request_buf->passive_max_time =
			mac_ctx->roam.configParam.nPassiveMaxChnTime;
		request_buf->passive_min_time =
			mac_ctx->roam.configParam.nPassiveMinChnTime;
	}

	msg.type = WMA_SET_PNO_REQ;
	msg.reserved = 0;
	msg.bodyptr = request_buf;
	if (!QDF_IS_STATUS_SUCCESS
			(cds_mq_post_message(QDF_MODULE_ID_WMA, &msg))) {
		CDF_TRACE(QDF_MODULE_ID_SME, CDF_TRACE_LEVEL_ERROR,
			FL("Not able to post WMA_SET_PNO_REQ message to WMA"));
		cdf_mem_free(request_buf);
		return QDF_STATUS_E_FAILURE;
	}

	/* Cache the Preferred Network Found Indication callback information */
	mac_ctx->sme.pref_netw_found_cb =
		callback_routine;
	mac_ctx->sme.preferred_network_found_ind_cb_ctx =
		callback_context;

	CDF_TRACE(QDF_MODULE_ID_SME, CDF_TRACE_LEVEL_INFO, "-%s", __func__);

	return QDF_STATUS_SUCCESS;
}
#endif /* FEATURE_WLAN_SCAN_PNO */

/**
 * sme_set_ps_host_offload(): Set the host offload feature.
 * @hal_ctx - The handle returned by mac_open.
 * @request - Pointer to the offload request.
 *
 * Return QDF_STATUS
 *            QDF_STATUS_E_FAILURE  Cannot set the offload.
 *            QDF_STATUS_SUCCESS  Request accepted.
 */
QDF_STATUS sme_set_ps_host_offload(tHalHandle hal_ctx,
		tpSirHostOffloadReq request,
		uint8_t session_id)
{
	tpSirHostOffloadReq request_buf;
	cds_msg_t msg;
	tpAniSirGlobal mac_ctx = PMAC_STRUCT(hal_ctx);
	tCsrRoamSession *session = CSR_GET_SESSION(mac_ctx, session_id);

	CDF_TRACE(QDF_MODULE_ID_SME, CDF_TRACE_LEVEL_INFO,
			"%s: IP address = %d.%d.%d.%d", __func__,
			request->params.hostIpv4Addr[0],
			request->params.hostIpv4Addr[1],
			request->params.hostIpv4Addr[2],
			request->params.hostIpv4Addr[3]);

	if (NULL == session) {
		CDF_TRACE(QDF_MODULE_ID_SME, CDF_TRACE_LEVEL_ERROR,
				"%s: SESSION not Found", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	request_buf = cdf_mem_malloc(sizeof(tSirHostOffloadReq));
	if (NULL == request_buf) {
		CDF_TRACE(QDF_MODULE_ID_SME, CDF_TRACE_LEVEL_ERROR,
		   FL("Not able to allocate memory for host offload request"));
		return QDF_STATUS_E_NOMEM;
	}

	cdf_copy_macaddr(&request->bssid, &session->connectedProfile.bssid);

	cdf_mem_copy(request_buf, request, sizeof(tSirHostOffloadReq));

	msg.type = WMA_SET_HOST_OFFLOAD;
	msg.reserved = 0;
	msg.bodyptr = request_buf;
	if (QDF_STATUS_SUCCESS !=
			cds_mq_post_message(QDF_MODULE_ID_WMA, &msg)) {
		CDF_TRACE(QDF_MODULE_ID_SME, CDF_TRACE_LEVEL_ERROR,
		      FL("Not able to post WMA_SET_HOST_OFFLOAD msg to WMA"));
		cdf_mem_free(request_buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_NS_OFFLOAD
/**
 * sme_set_ps_ns_offload(): Set the host offload feature.
 * @hal_ctx - The handle returned by mac_open.
 * @request - Pointer to the offload request.
 *
 * Return QDF_STATUS
 *		QDF_STATUS_E_FAILURE  Cannot set the offload.
 *		QDF_STATUS_SUCCESS  Request accepted.
 */
QDF_STATUS sme_set_ps_ns_offload(tHalHandle hal_ctx,
		tpSirHostOffloadReq request,
		uint8_t session_id)
{
	tpAniSirGlobal mac_ctx = PMAC_STRUCT(hal_ctx);
	tpSirHostOffloadReq request_buf;
	cds_msg_t msg;
	tCsrRoamSession *session = CSR_GET_SESSION(mac_ctx, session_id);

	if (NULL == session) {
		sms_log(mac_ctx, LOGE, FL("Session not found "));
		return QDF_STATUS_E_FAILURE;
	}

	cdf_copy_macaddr(&request->bssid, &session->connectedProfile.bssid);

	request_buf = cdf_mem_malloc(sizeof(*request_buf));
	if (NULL == request_buf) {
		CDF_TRACE(QDF_MODULE_ID_SME, CDF_TRACE_LEVEL_ERROR,
			FL("Not able to allocate memory for NS offload request"));
		return QDF_STATUS_E_NOMEM;
	}
	*request_buf = *request;

	msg.type = WMA_SET_NS_OFFLOAD;
	msg.reserved = 0;
	msg.bodyptr = request_buf;
	if (QDF_STATUS_SUCCESS !=
			cds_mq_post_message(QDF_MODULE_ID_WMA, &msg)) {
		CDF_TRACE(QDF_MODULE_ID_SME, CDF_TRACE_LEVEL_ERROR,
			FL("Not able to post SIR_HAL_SET_HOST_OFFLOAD message to HAL"));
		cdf_mem_free(request_buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

#endif /* WLAN_NS_OFFLOAD */
/* -------------------------------------------------------------------- */
/**
 * sme_post_pe_message
 *
 * FUNCTION:
 * Post a message to the pmm message queue
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param msg pointer to message
 * @return None
 */

tSirRetStatus sme_post_pe_message(tpAniSirGlobal mac_ctx, tpSirMsgQ msg)
{
	QDF_STATUS qdf_status;
	qdf_status = cds_mq_post_message(CDS_MQ_ID_PE, (cds_msg_t *) msg);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		sms_log(mac_ctx, LOGP,
			FL("cds_mq_post_message failed with status code %d"),
			qdf_status);
		return eSIR_FAILURE;
	}

	return eSIR_SUCCESS;
}

QDF_STATUS sme_ps_enable_auto_ps_timer(tHalHandle hal_ctx,
		uint32_t session_id,
		bool is_reassoc)
{
	tpAniSirGlobal mac_ctx = PMAC_STRUCT(hal_ctx);
	struct ps_global_info *ps_global_info = &mac_ctx->sme.ps_global_info;
	struct ps_params *ps_param = &ps_global_info->ps_params[session_id];
	QDF_STATUS qdf_status;
	uint32_t timer_value;

	if (is_reassoc)
		timer_value = AUTO_PS_ENTRY_TIMER_DEFAULT_VALUE;
	else
		timer_value = AUTO_DEFERRED_PS_ENTRY_TIMER_DEFAULT_VALUE;

	sms_log(mac_ctx, LOGE, FL("Start auto_ps_timer for %d is_reassoc:%d "),
			timer_value, is_reassoc);

	qdf_status = cdf_mc_timer_start(&ps_param->auto_ps_enable_timer,
			timer_value);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		if (QDF_STATUS_E_ALREADY == qdf_status) {
			/* Consider this ok since the timer is already started*/
			sms_log(mac_ctx, LOGW,
					FL("auto_ps_timer is already started"));
		} else {
			sms_log(mac_ctx, LOGP,
					FL("Cannot start auto_ps_timer"));
			return QDF_STATUS_E_FAILURE;
		}
	}
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS sme_ps_disable_auto_ps_timer(tHalHandle hal_ctx,
		uint32_t session_id)
{
	tpAniSirGlobal mac_ctx = PMAC_STRUCT(hal_ctx);
	struct ps_global_info *ps_global_info = &mac_ctx->sme.ps_global_info;
	struct ps_params *ps_param = &ps_global_info->ps_params[session_id];
	/*
	 * Stop the auto ps entry timer if runnin
	 */
	if (CDF_TIMER_STATE_RUNNING ==
			cdf_mc_timer_get_current_state(
				&ps_param->auto_ps_enable_timer)) {
		sms_log(mac_ctx, LOGE,
				FL("Stop auto_ps_enable_timer Timer for session ID:%d "),
				session_id);
		cdf_mc_timer_stop(&ps_param->auto_ps_enable_timer);
	}
	return QDF_STATUS_SUCCESS;
}


QDF_STATUS sme_ps_open(tHalHandle hal_ctx)
{

	uint32_t i;
	tpAniSirGlobal mac_ctx = PMAC_STRUCT(hal_ctx);

	sms_log(mac_ctx, LOG1, FL("Enter"));

	for (i = 0; i < MAX_SME_SESSIONS; i++) {
		if (QDF_STATUS_SUCCESS != sme_ps_open_per_session(hal_ctx, i)) {
			sms_log(mac_ctx, LOGE,
				FL("PMC Init Failed for session %d"), i);
			return QDF_STATUS_E_FAILURE;
		}
	}
	return QDF_STATUS_SUCCESS;
}


QDF_STATUS sme_ps_open_per_session(tHalHandle hal_ctx, uint32_t session_id)
{
	tpAniSirGlobal mac_ctx = PMAC_STRUCT(hal_ctx);
	struct ps_global_info *ps_global_info = &mac_ctx->sme.ps_global_info;
	struct ps_params *ps_param = &ps_global_info->ps_params[session_id];
	ps_param->session_id = session_id;
	ps_param->mac_ctx = mac_ctx;

	sms_log(mac_ctx, LOG1, FL("Enter"));
	/* Allocate a timer to enable ps automatically */
	if (!QDF_IS_STATUS_SUCCESS(cdf_mc_timer_init(
					&ps_param->auto_ps_enable_timer,
					QDF_TIMER_TYPE_SW,
					sme_auto_ps_entry_timer_expired,
					ps_param)))     {
		sms_log(mac_ctx, LOGE,
				FL("Cannot allocate timer for auto ps entry"));
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;

}

void sme_auto_ps_entry_timer_expired(void *data)
{
	struct ps_params *ps_params =   (struct ps_params *)data;
	tpAniSirGlobal mac_ctx = (tpAniSirGlobal)ps_params->mac_ctx;
	uint32_t session_id = ps_params->session_id;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	status = sme_enable_sta_ps_check(mac_ctx, session_id);

	if (QDF_STATUS_SUCCESS == status) {
		sme_ps_enable_disable((tHalHandle)mac_ctx, session_id,
				SME_PS_ENABLE);
	} else {
		status =
			cdf_mc_timer_start(&ps_params->auto_ps_enable_timer,
					AUTO_PS_ENTRY_TIMER_DEFAULT_VALUE);
		if (!QDF_IS_STATUS_SUCCESS(status)
				&& (QDF_STATUS_E_ALREADY != status)) {
			sms_log(mac_ctx, LOGP,
					FL("Cannot start traffic timer"));
		}
	}
}

QDF_STATUS sme_ps_close(tHalHandle hal_ctx)
{
	uint32_t i;
	tpAniSirGlobal mac_ctx = PMAC_STRUCT(hal_ctx);

	sms_log(mac_ctx, LOG2, FL("Enter"));

	for (i = 0; i < CSR_ROAM_SESSION_MAX; i++)
		sme_ps_close_per_session(hal_ctx, i);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS sme_ps_close_per_session(tHalHandle hal_ctx, uint32_t session_id)
{

	tpAniSirGlobal mac_ctx = PMAC_STRUCT(hal_ctx);
	struct ps_global_info *ps_global_info = &mac_ctx->sme.ps_global_info;
	struct ps_params *ps_param = &ps_global_info->ps_params[session_id];
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

	/*
	 * Stop the auto ps entry timer if running
	 */
	if (CDF_TIMER_STATE_RUNNING ==
			cdf_mc_timer_get_current_state(
				&ps_param->auto_ps_enable_timer)) {
		cdf_mc_timer_stop(&ps_param->auto_ps_enable_timer);
	}
	qdf_status =
		cdf_mc_timer_destroy(&ps_param->auto_ps_enable_timer);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status))
		sms_log(mac_ctx, LOGE, FL("Cannot deallocate suto PS timer"));
	return qdf_status;
}

QDF_STATUS sme_is_auto_ps_timer_running(tHalHandle hal_ctx,
		uint32_t session_id)
{
	tpAniSirGlobal mac_ctx = PMAC_STRUCT(hal_ctx);
	struct ps_global_info *ps_global_info = &mac_ctx->sme.ps_global_info;
	struct ps_params *ps_param = &ps_global_info->ps_params[session_id];
	bool status = false;
	/*
	 * Check if the auto ps entry timer if running
	 */
	if (CDF_TIMER_STATE_RUNNING ==
			cdf_mc_timer_get_current_state(
				&ps_param->auto_ps_enable_timer)) {
		status = true;
	}
	return status;
}

