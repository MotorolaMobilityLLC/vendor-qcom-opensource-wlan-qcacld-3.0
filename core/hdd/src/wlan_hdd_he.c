/*
 * Copyright (c) 2017 The Linux Foundation. All rights reserved.
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
 * DOC : wlan_hdd_he.c
 *
 * WLAN Host Device Driver file for 802.11ax (High Efficiency) support.
 *
 */

#include "wlan_hdd_main.h"
#include "wlan_hdd_he.h"

/**
 * hdd_he_wni_cfg_to_string() - return string conversion of HE WNI CFG
 * @cfg_id: Config ID.
 *
 * This utility function helps log string conversion of WNI config ID.
 *
 * Return: string conversion of the HE WNI config ID, if match found;
 *	"Invalid" otherwise.
 */
static const char *hdd_he_wni_cfg_to_string(uint16_t cfg_id)
{
	switch (cfg_id) {
	default:
		return "Invalid";
	CASE_RETURN_STRING(WNI_CFG_HE_CONTROL);
	CASE_RETURN_STRING(WNI_CFG_HE_TWT_REQUESTOR);
	CASE_RETURN_STRING(WNI_CFG_HE_TWT_RESPONDER);
	CASE_RETURN_STRING(WNI_CFG_HE_FRAGMENTATION);
	CASE_RETURN_STRING(WNI_CFG_HE_MAX_FRAG_MSDU);
	CASE_RETURN_STRING(WNI_CFG_HE_MIN_FRAG_SIZE);
	CASE_RETURN_STRING(WNI_CFG_HE_TRIG_PAD);
	CASE_RETURN_STRING(WNI_CFG_HE_MTID_AGGR);
	CASE_RETURN_STRING(WNI_CFG_HE_LINK_ADAPTATION);
	CASE_RETURN_STRING(WNI_CFG_HE_ALL_ACK);
	CASE_RETURN_STRING(WNI_CFG_HE_UL_MU_RSP_SCHEDULING);
	CASE_RETURN_STRING(WNI_CFG_HE_BUFFER_STATUS_RPT);
	CASE_RETURN_STRING(WNI_CFG_HE_BCAST_TWT);
	CASE_RETURN_STRING(WNI_CFG_HE_BA_32BIT);
	CASE_RETURN_STRING(WNI_CFG_HE_MU_CASCADING);
	CASE_RETURN_STRING(WNI_CFG_HE_MULTI_TID);
	CASE_RETURN_STRING(WNI_CFG_HE_DL_MU_BA);
	CASE_RETURN_STRING(WNI_CFG_HE_OMI);
	CASE_RETURN_STRING(WNI_CFG_HE_OFDMA_RA);
	CASE_RETURN_STRING(WNI_CFG_HE_MAX_AMPDU_LEN);
	CASE_RETURN_STRING(WNI_CFG_HE_AMSDU_FRAG);
	CASE_RETURN_STRING(WNI_CFG_HE_FLEX_TWT_SCHED);
	CASE_RETURN_STRING(WNI_CFG_HE_RX_CTRL);
	CASE_RETURN_STRING(WNI_CFG_HE_BSRP_AMPDU_AGGR);
	CASE_RETURN_STRING(WNI_CFG_HE_QTP);
	CASE_RETURN_STRING(WNI_CFG_HE_A_BQR);
	CASE_RETURN_STRING(WNI_CFG_HE_DUAL_BAND);
	CASE_RETURN_STRING(WNI_CFG_HE_CHAN_WIDTH);
	CASE_RETURN_STRING(WNI_CFG_HE_RX_PREAM_PUNC);
	CASE_RETURN_STRING(WNI_CFG_HE_CLASS_OF_DEVICE);
	CASE_RETURN_STRING(WNI_CFG_HE_LDPC);
	CASE_RETURN_STRING(WNI_CFG_HE_LTF_PPDU);
	CASE_RETURN_STRING(WNI_CFG_HE_LTF_NDP);
	CASE_RETURN_STRING(WNI_CFG_HE_STBC);
	CASE_RETURN_STRING(WNI_CFG_HE_DOPPLER);
	CASE_RETURN_STRING(WNI_CFG_HE_UL_MUMIMO);
	CASE_RETURN_STRING(WNI_CFG_HE_DCM_TX);
	CASE_RETURN_STRING(WNI_CFG_HE_DCM_RX);
	CASE_RETURN_STRING(WNI_CFG_HE_MU_PPDU);
	CASE_RETURN_STRING(WNI_CFG_HE_SU_BEAMFORMER);
	CASE_RETURN_STRING(WNI_CFG_HE_SU_BEAMFORMEE);
	CASE_RETURN_STRING(WNI_CFG_HE_MU_BEAMFORMER);
	CASE_RETURN_STRING(WNI_CFG_HE_BFEE_STS_LT80);
	CASE_RETURN_STRING(WNI_CFG_HE_NSTS_TOT_LT80);
	CASE_RETURN_STRING(WNI_CFG_HE_BFEE_STS_GT80);
	CASE_RETURN_STRING(WNI_CFG_HE_NSTS_TOT_GT80);
	CASE_RETURN_STRING(WNI_CFG_HE_NUM_SOUND_LT80);
	CASE_RETURN_STRING(WNI_CFG_HE_NUM_SOUND_GT80);
	CASE_RETURN_STRING(WNI_CFG_HE_SU_FEED_TONE16);
	CASE_RETURN_STRING(WNI_CFG_HE_MU_FEED_TONE16);
	CASE_RETURN_STRING(WNI_CFG_HE_CODEBOOK_SU);
	CASE_RETURN_STRING(WNI_CFG_HE_CODEBOOK_MU);
	CASE_RETURN_STRING(WNI_CFG_HE_BFRM_FEED);
	CASE_RETURN_STRING(WNI_CFG_HE_ER_SU_PPDU);
	CASE_RETURN_STRING(WNI_CFG_HE_DL_PART_BW);
	CASE_RETURN_STRING(WNI_CFG_HE_PPET_PRESENT);
	CASE_RETURN_STRING(WNI_CFG_HE_SRP);
	CASE_RETURN_STRING(WNI_CFG_HE_POWER_BOOST);
	CASE_RETURN_STRING(WNI_CFG_HE_4x_LTF_GI);
	CASE_RETURN_STRING(WNI_CFG_HE_NSS);
	CASE_RETURN_STRING(WNI_CFG_HE_MCS);
	CASE_RETURN_STRING(WNI_CFG_HE_PPET);
	}
}

/**
 * hdd_he_set_wni_cfg() - Update WNI CFG
 * @hdd_ctx: HDD context
 * @cfg_id: CFG to be udpated
 * @new_value: Value to be updated
 *
 * Update WNI CFG with the value passed.
 *
 * Return: 0 on success and errno on failure
 */
static int hdd_he_set_wni_cfg(struct hdd_context_s *hdd_ctx,
				     uint16_t cfg_id, uint32_t new_value)
{
	QDF_STATUS status;

	status = sme_cfg_set_int(hdd_ctx->hHal, cfg_id, new_value);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("could not set %s", hdd_he_wni_cfg_to_string(cfg_id));

	return qdf_status_to_os_return(status);
}

/**
 * hdd_update_tgt_he_cap() - Update HE related capabilities
 * @hdd_ctx: HDD context
 * @he_cap: Target HE capabilities
 *
 * This function updaates WNI CFG with Target capabilities received as part of
 * Default values present in WNI CFG are the values supported by FW/HW.
 * INI should be introduced if user control is required to control the value.
 *
 * Return: None
 */
void hdd_update_tgt_he_cap(struct hdd_context_s *hdd_ctx,
			   struct wma_tgt_cfg *cfg)
{
	uint32_t ppet_size = sizeof(tDot11fIEppe_threshold);
	QDF_STATUS status;
	tDot11fIEvendor_he_cap *he_cap = &cfg->he_cap;

	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_CONTROL, he_cap->htc_he);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_TWT_REQUESTOR,
			   he_cap->twt_request);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_TWT_RESPONDER,
			   he_cap->twt_responder);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_FRAGMENTATION,
			   he_cap->fragmentation);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_MAX_FRAG_MSDU,
			   he_cap->max_num_frag_msdu);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_MIN_FRAG_SIZE,
			   he_cap->min_frag_size);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_TRIG_PAD,
			   he_cap->trigger_frm_mac_pad);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_MTID_AGGR,
			   he_cap->multi_tid_aggr);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_LINK_ADAPTATION,
			   he_cap->he_link_adaptation);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_ALL_ACK, he_cap->all_ack);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_UL_MU_RSP_SCHEDULING,
			   he_cap->ul_mu_rsp_sched);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_BUFFER_STATUS_RPT,
			   he_cap->a_bsr);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_BCAST_TWT,
			   he_cap->broadcast_twt);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_BA_32BIT,
			   he_cap->ba_32bit_bitmap);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_MU_CASCADING,
			   he_cap->mu_cascade);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_MULTI_TID,
			   he_cap->ack_enabled_multitid);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_DL_MU_BA, he_cap->dl_mu_ba);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_OMI, he_cap->omi_a_ctrl);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_OFDMA_RA, he_cap->ofdma_ra);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_MAX_AMPDU_LEN,
			   he_cap->max_ampdu_len);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_AMSDU_FRAG, he_cap->amsdu_frag);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_FLEX_TWT_SCHED,
			   he_cap->flex_twt_sched);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_RX_CTRL, he_cap->rx_ctrl_frame);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_BSRP_AMPDU_AGGR,
			   he_cap->bsrp_ampdu_aggr);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_QTP, he_cap->qtp);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_A_BQR, he_cap->a_bqr);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_DUAL_BAND, he_cap->dual_band);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_CHAN_WIDTH, he_cap->chan_width);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_RX_PREAM_PUNC,
			   he_cap->rx_pream_puncturing);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_CLASS_OF_DEVICE,
			   he_cap->device_class);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_LDPC, he_cap->ldpc_coding);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_LTF_PPDU,
			   he_cap->he_ltf_gi_ppdu);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_LTF_NDP, he_cap->he_ltf_gi_ndp);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_STBC, he_cap->stbc);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_DOPPLER, he_cap->doppler);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_UL_MUMIMO, he_cap->ul_mu);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_DCM_TX, he_cap->dcm_enc_tx);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_DCM_RX, he_cap->dcm_enc_rx);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_MU_PPDU, he_cap->ul_he_mu);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_SU_BEAMFORMER,
			   he_cap->su_beamformer);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_SU_BEAMFORMEE,
			   he_cap->su_beamformee);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_MU_BEAMFORMER,
			   he_cap->mu_beamformer);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_BFEE_STS_LT80,
			   he_cap->bfee_sts_lt_80);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_NSTS_TOT_LT80,
			   he_cap->nsts_tol_lt_80);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_BFEE_STS_GT80,
			   he_cap->bfee_sta_gt_80);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_NSTS_TOT_GT80,
			   he_cap->nsts_tot_gt_80);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_NUM_SOUND_LT80,
			   he_cap->num_sounding_lt_80);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_NUM_SOUND_GT80,
			   he_cap->num_sounding_gt_80);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_SU_FEED_TONE16,
			   he_cap->su_feedback_tone16);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_MU_FEED_TONE16,
			   he_cap->mu_feedback_tone16);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_CODEBOOK_SU,
			   he_cap->codebook_su);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_CODEBOOK_MU,
			   he_cap->codebook_mu);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_BFRM_FEED,
			   he_cap->beamforming_feedback);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_ER_SU_PPDU,
			   he_cap->he_er_su_ppdu);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_DL_PART_BW,
			   he_cap->dl_mu_mimo_part_bw);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_PPET_PRESENT,
			   he_cap->ppet_present);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_SRP, he_cap->srp);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_POWER_BOOST,
			   he_cap->power_boost);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_4x_LTF_GI, he_cap->he_ltf_gi_4x);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_NSS, he_cap->nss_supported);
	hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_MCS, he_cap->mcs_supported);

	/* PPET can not be configured by user - Set values from FW */
	status = sme_cfg_set_str(hdd_ctx->hHal, WNI_CFG_HE_PPET,
				 (void *)&he_cap->ppe_threshold, ppet_size);
	if (status == QDF_STATUS_E_FAILURE)
		hdd_alert("could not set HE PPET");
}

/**
 * wlan_hdd_check_11ax_support() - check if beacon IE and update hw mode
 * @beacon: beacon IE buffer
 * @config: pointer to sap config
 *
 * Check if HE cap IE is present in beacon IE, if present update hw mode
 * to 11ax.
 *
 * Return: None
 */
void wlan_hdd_check_11ax_support(beacon_data_t *beacon, tsap_Config_t *config)
{
	uint8_t *ie;

	ie = wlan_hdd_get_vendor_oui_ie_ptr(HE_CAP_OUI_TYPE, HE_CAP_OUI_SIZE,
					    beacon->tail, beacon->tail_len);
	if (ie)
		config->SapHw_mode = eCSR_DOT11_MODE_11ax;
}

/**
 * hdd_he_print_ini_config()- Print 11AX(HE) specific INI configuration
 * @hdd_ctx: handle to hdd context
 *
 * Return: None
 */
void hdd_he_print_ini_config(hdd_context_t *hdd_ctx)
{
	hdd_info("Name = [%s] Value = [%d]", CFG_ENABLE_UL_MIMO_NAME,
		hdd_ctx->config->enable_ul_mimo);
	hdd_info("Name = [%s] Value = [%d]", CFG_ENABLE_UL_OFDMA_NAME,
		hdd_ctx->config->enable_ul_ofdma);
}

/**
 * hdd_update_he_cap_in_cfg() - update HE cap in global CFG
 * @hdd_ctx: pointer to hdd context
 *
 * This API will update the HE config in CFG after taking intersection
 * of INI and firmware capabilities provided reading CFG
 *
 * Return: 0 on success and errno on failure
 */
int hdd_update_he_cap_in_cfg(hdd_context_t *hdd_ctx)
{
	uint32_t val, val1 = 0;
	QDF_STATUS status;
	int ret;
	struct hdd_config *config = hdd_ctx->config;

	status = sme_cfg_get_int(hdd_ctx->hHal, WNI_CFG_HE_UL_MUMIMO, &val);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("could not get WNI_CFG_HE_UL_MUMIMO");
		return qdf_status_to_os_return(status);
	}

	/* In val,
	 * Bit 1 - corresponds to UL MIMO
	 * Bit 2 - corresponds to UL OFDMA
	 */
	if (val & 0x1)
		val1 = config->enable_ul_mimo & 0x1;

	if ((val >> 1) & 0x1)
		val1 |= ((config->enable_ul_ofdma & 0x1) << 1);

	ret = hdd_he_set_wni_cfg(hdd_ctx, WNI_CFG_HE_UL_MUMIMO, val1);

	return ret;
}

/**
 * hdd_he_set_sme_config() - set HE related SME config param
 * @sme_config: pointer to SME config
 * @config: pointer to INI config
 *
 * Return: None
 */
void hdd_he_set_sme_config(tSmeConfigParams *sme_config,
			   struct hdd_config *config)
{
	sme_config->csrConfig.enable_ul_ofdma = config->enable_ul_ofdma;
	sme_config->csrConfig.enable_ul_mimo = config->enable_ul_mimo;
}
