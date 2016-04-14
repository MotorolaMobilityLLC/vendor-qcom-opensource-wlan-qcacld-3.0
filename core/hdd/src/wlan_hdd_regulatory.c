/*
 * Copyright (c) 2014-2016 The Linux Foundation. All rights reserved.
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

/**
 * DOC: wlan_hdd_regulatory.c
 *
 * hdd regulatory implementation
 */

#include "qdf_types.h"
#include "cds_reg_service.h"
#include "qdf_trace.h"
#include "sme_api.h"
#include "wlan_hdd_main.h"
#include "cds_regdomain.h"
#include "cds_regdomain_common.h"
#include "wlan_hdd_regulatory.h"

#define WORLD_SKU_MASK      0x00F0
#define WORLD_SKU_PREFIX    0x0060
#define REG_WAIT_TIME       50

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)) || defined(WITH_BACKPORTS)
#define IEEE80211_CHAN_PASSIVE_SCAN IEEE80211_CHAN_NO_IR
#define IEEE80211_CHAN_NO_IBSS IEEE80211_CHAN_NO_IR
#endif

#define REG_RULE_2412_2462    REG_RULE(2412-10, 2462+10, 40, 0, 20, 0)

#define REG_RULE_2467_2472    REG_RULE(2467-10, 2472+10, 40, 0, 20, \
			      NL80211_RRF_PASSIVE_SCAN)

#define REG_RULE_2484         REG_RULE(2484-10, 2484+10, 40, 0, 20, \
		NL80211_RRF_PASSIVE_SCAN | NL80211_RRF_NO_OFDM)

#define REG_RULE_5180_5320    REG_RULE(5180-10, 5320+10, 80, 0, 20, \
		NL80211_RRF_PASSIVE_SCAN | NL80211_RRF_NO_IBSS)

#define REG_RULE_5500_5720    REG_RULE(5500-10, 5720+10, 80, 0, 20, \
		NL80211_RRF_PASSIVE_SCAN | NL80211_RRF_NO_IBSS)

#define REG_RULE_5745_5925    REG_RULE(5745-10, 5925+10, 80, 0, 20, \
		NL80211_RRF_PASSIVE_SCAN | NL80211_RRF_NO_IBSS)

static bool init_by_driver;
static bool init_by_reg_core;

static const struct ieee80211_regdomain
hdd_world_regrules_60_61_62 = {
	.n_reg_rules = 6,
	.alpha2 =  "00",
	.reg_rules = {
		REG_RULE_2412_2462,
		REG_RULE_2467_2472,
		REG_RULE_2484,
		REG_RULE_5180_5320,
		REG_RULE_5500_5720,
		REG_RULE_5745_5925,
	}
};

static const struct ieee80211_regdomain
hdd_world_regrules_63_65 = {
	.n_reg_rules = 4,
	.alpha2 =  "00",
	.reg_rules = {
		REG_RULE_2412_2462,
		REG_RULE_2467_2472,
		REG_RULE_5180_5320,
		REG_RULE_5745_5925,
	}
};

static const struct ieee80211_regdomain
hdd_world_regrules_64 = {
	.n_reg_rules = 3,
	.alpha2 =  "00",
	.reg_rules = {
		REG_RULE_2412_2462,
		REG_RULE_5180_5320,
		REG_RULE_5745_5925,
	}
};

static const struct ieee80211_regdomain
hdd_world_regrules_66_69 = {
	.n_reg_rules = 4,
	.alpha2 =  "00",
	.reg_rules = {
		REG_RULE_2412_2462,
		REG_RULE_5180_5320,
		REG_RULE_5500_5720,
		REG_RULE_5745_5925,
	}
};

static const struct ieee80211_regdomain
hdd_world_regrules_67_68_6A_6C = {
	.n_reg_rules = 5,
	.alpha2 =  "00",
	.reg_rules = {
		REG_RULE_2412_2462,
		REG_RULE_2467_2472,
		REG_RULE_5180_5320,
		REG_RULE_5500_5720,
		REG_RULE_5745_5925,
	}
};

/**
 * hdd_get_world_regrules() - get the appropriate world regrules
 * @reg: regulatory data
 *
 * Return: regulatory rules ptr
 */
static const struct ieee80211_regdomain *hdd_get_world_regrules(
struct regulatory *reg)
{
	REG_DMN_PAIR_MAPPING *regpair =
		(REG_DMN_PAIR_MAPPING *)reg->regpair;

	switch (regpair->regDmnEnum) {
	case 0x60:
	case 0x61:
	case 0x62:
		return &hdd_world_regrules_60_61_62;
	case 0x63:
	case 0x65:
		return &hdd_world_regrules_63_65;
	case 0x64:
		return &hdd_world_regrules_64;
	case 0x66:
	case 0x69:
		return &hdd_world_regrules_66_69;
	case 0x67:
	case 0x68:
	case 0x6A:
	case 0x6C:
		return &hdd_world_regrules_67_68_6A_6C;
	default:
		hdd_warn("invalid world mode in BDF");
		return &hdd_world_regrules_60_61_62;
	}
}

/**
 * hdd_is_world_regdomain() - whether world regdomain
 * @reg_domain: integer regulatory domain
 *
 * Return: bool
 */
bool hdd_is_world_regdomain(uint32_t reg_domain)
{
	uint32_t temp_regd = reg_domain & ~WORLDWIDE_ROAMING_FLAG;

	return ((temp_regd & COUNTRY_ERD_FLAG) != COUNTRY_ERD_FLAG) &&
		(((temp_regd & WORLD_SKU_MASK) == WORLD_SKU_PREFIX) ||
		 (temp_regd == WORLD));
}


/**
 * hdd_update_regulatory_info() - update regulatory info
 * @hdd_ctx: hdd context
 *
 * Return: void
 */
static void hdd_update_regulatory_info(hdd_context_t *hdd_ctx)
{
	uint32_t country_code;

	country_code = cds_get_country_from_alpha2(hdd_ctx->reg.alpha2);

	hdd_ctx->reg.reg_domain = COUNTRY_ERD_FLAG;
	hdd_ctx->reg.reg_domain |= country_code;

	cds_fill_some_regulatory_info(&hdd_ctx->reg);

}

/**
 * hdd_regulatory_wiphy_init() - regulatory wiphy init
 * @hdd_ctx: hdd context
 * @reg: regulatory data
 * @wiphy: wiphy structure
 *
 * Return: void
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)) || defined(WITH_BACKPORTS)
static void hdd_regulatory_wiphy_init(hdd_context_t *hdd_ctx,
				     struct regulatory *reg,
				     struct wiphy *wiphy)
{
	const struct ieee80211_regdomain *reg_rules;

	if (hdd_is_world_regdomain(reg->reg_domain)) {
		reg_rules = hdd_get_world_regrules(reg);
		wiphy->regulatory_flags |= REGULATORY_CUSTOM_REG;
	} else if (hdd_ctx->config->fRegChangeDefCountry) {
		wiphy->regulatory_flags |= REGULATORY_CUSTOM_REG;
		reg_rules = &hdd_world_regrules_60_61_62;
	} else {
		wiphy->regulatory_flags |= REGULATORY_STRICT_REG;
		reg_rules = &hdd_world_regrules_60_61_62;
	}

	/*
	 * save the original driver regulatory flags
	 */
	hdd_ctx->reg.reg_flags = wiphy->regulatory_flags;
	wiphy_apply_custom_regulatory(wiphy, reg_rules);

	/*
	 * restore the driver regulatory flags since
	 * wiphy_apply_custom_regulatory may have
	 * changed them
	 */
	wiphy->regulatory_flags = hdd_ctx->reg.reg_flags;

}
#else
static void hdd_regulatory_wiphy_init(hdd_context_t *hdd_ctx,
				     struct regulatory *reg,
				     struct wiphy *wiphy)
{
	const struct ieee80211_regdomain *reg_rules;

	if (hdd_is_world_regdomain(reg->reg_domain)) {
		reg_rules = hdd_get_world_regrules(reg);
		wiphy->flags |= WIPHY_FLAG_CUSTOM_REGULATORY;
	} else if (hdd_ctx->config->fRegChangeDefCountry) {
		wiphy->flags |= WIPHY_FLAG_CUSTOM_REGULATORY;
		reg_rules = &hdd_world_regrules_60_61_62;
	} else {
		wiphy->flags |= WIPHY_FLAG_STRICT_REGULATORY;
		reg_rules = &hdd_world_regrules_60_61_62;
	}

	/*
	 * save the original driver regulatory flags
	 */
	hdd_ctx->reg.reg_flags = wiphy->flags;
	wiphy_apply_custom_regulatory(wiphy, reg_rules);

	/*
	 * restore the driver regulatory flags since
	 * wiphy_apply_custom_regulatory may have
	 * changed them
	 */
	wiphy->flags = hdd_ctx->reg.reg_flags;

}
#endif

/**
 * hdd_bw20_ch_index_to_() - convert 20 mhhz channel index to 40 mhz index
 * @k: 20 mhz channel index
 *
 * Return: void
 */
static int hdd_bw20_ch_index_to_bw40_ch_index(int k)
{
	int m = -1;

	if (k >= RF_CHAN_1 && k <= RF_CHAN_14) {
		m = k - RF_CHAN_1 + RF_CHAN_BOND_3;
		if (m > RF_CHAN_BOND_11)
			m = RF_CHAN_BOND_11;
	} else if (k >= RF_CHAN_36 && k <= RF_CHAN_64) {
		m = k - RF_CHAN_36 + RF_CHAN_BOND_38;
		if (m > RF_CHAN_BOND_62)
			m = RF_CHAN_BOND_62;
	} else if (k >= RF_CHAN_100 && k <= RF_CHAN_144) {
		m = k - RF_CHAN_100 + RF_CHAN_BOND_102;
		if (m > RF_CHAN_BOND_142)
			m = RF_CHAN_BOND_142;
	} else if (k >= RF_CHAN_149 && k <= RF_CHAN_165) {
		m = k - RF_CHAN_149 + RF_CHAN_BOND_151;
		if (m > RF_CHAN_BOND_163)
			m = RF_CHAN_BOND_163;
	}

	return m;
}

/**
 * is_wiphy_custom_regulatory() - is custom regulatory defined
 * @wiphy: wiphy
 *
 * Return: int
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)) || defined(WITH_BACKPORTS)
static int is_wiphy_custom_regulatory(struct wiphy *wiphy)
{

	return wiphy->regulatory_flags & REGULATORY_CUSTOM_REG;
}
#else
static int is_wiphy_custom_regulatory(struct wiphy *wiphy)
{
	return wiphy->flags & WIPHY_FLAG_CUSTOM_REGULATORY;
}
#endif


/**
 * hdd_modify_wiphy() - modify wiphy
 * @wiphy: wiphy
 * @chan: channel structure
 *
 * Return: void
 */
static void hdd_modify_wiphy(struct wiphy  *wiphy,
			     struct ieee80211_channel *chan)
{
	const struct ieee80211_reg_rule *reg_rule;

	if (is_wiphy_custom_regulatory(wiphy)) {
		reg_rule = freq_reg_info(wiphy, MHZ_TO_KHZ(chan->center_freq));
		if (!IS_ERR(reg_rule)) {
			chan->flags &= ~IEEE80211_CHAN_DISABLED;

			if (!(reg_rule->flags & NL80211_RRF_DFS)) {
				hdd_info("%s: remove dfs restriction for %u",
					 __func__, chan->center_freq);
				chan->flags &= ~IEEE80211_CHAN_RADAR;
			}

			if (!(reg_rule->flags & NL80211_RRF_PASSIVE_SCAN)) {
				hdd_info("%s: remove passive restriction for %u",
					 __func__, chan->center_freq);
				chan->flags &= ~IEEE80211_CHAN_PASSIVE_SCAN;
			}

			if (!(reg_rule->flags & NL80211_RRF_NO_IBSS)) {
				hdd_info("%s: remove no ibss restriction for %u",
					 __func__, chan->center_freq);
				chan->flags &= ~IEEE80211_CHAN_NO_IBSS;
			}

			chan->max_power =
				MBM_TO_DBM(reg_rule->power_rule.max_eirp);
		}
	}
}

/**
 * hdd_process_regulatory_data() - process regulatory data
 * @hdd_ctx: hdd context
 * @wiphy: wiphy
 * @reset: whether to reset channel data
 *
 * Return: void
 */
static void hdd_process_regulatory_data(hdd_context_t *hdd_ctx,
					struct wiphy *wiphy,
					bool reset)
{
	int i, j, m;
	int k = 0, n = 0;
	struct ieee80211_channel *chan;
	struct regulatory_channel *temp_chan_k, *temp_chan_n, *temp_chan;
	uint8_t band_capability;

	band_capability = hdd_ctx->config->nBandCapability;

	hdd_ctx->isVHT80Allowed = 0;

	if (band_capability == eCSR_BAND_24)
		hdd_info("band capability is set to 2G only");

	for (i = 0, m = 0; i < IEEE80211_NUM_BANDS; i++) {

		if (i == IEEE80211_BAND_2GHZ &&
		    band_capability == eCSR_BAND_5G)
			continue;

		else if (i == IEEE80211_BAND_5GHZ &&
			 band_capability == eCSR_BAND_24)
			continue;

		if (wiphy->bands[i] == NULL) {
			hdd_info("wiphy band no %d is NULL", i);
			continue;
		}

		if (i == 0)
			m = 0;
		else
			m = wiphy->bands[i-1]->n_channels + m;

		for (j = 0; j < wiphy->bands[i]->n_channels; j++) {

			k = m + j;
			n = hdd_bw20_ch_index_to_bw40_ch_index(k);

			chan = &(wiphy->bands[i]->channels[j]);
			temp_chan_k = &(reg_channels[k]);
			temp_chan_n = &(reg_channels[n]);

			if (!reset)
				hdd_modify_wiphy(wiphy, chan);

			if (chan->flags & IEEE80211_CHAN_DISABLED) {
				temp_chan_k->state =
					CHANNEL_STATE_DISABLE;
				temp_chan_k->flags = chan->flags;
				if (n != -1) {
					temp_chan_n->state =
						CHANNEL_STATE_DISABLE;
					temp_chan_n->flags = chan->flags;
				}
			} else if (chan->flags &
				   (IEEE80211_CHAN_RADAR |
				    IEEE80211_CHAN_PASSIVE_SCAN |
				    IEEE80211_CHAN_INDOOR_ONLY)) {

				if (chan->flags &
				    IEEE80211_CHAN_INDOOR_ONLY)
					chan->flags |=
						IEEE80211_CHAN_PASSIVE_SCAN;
				temp_chan_k->state = CHANNEL_STATE_DFS;
				temp_chan_k->pwr_limit =
					chan->max_power;
				temp_chan_k->flags = chan->flags;

				if (n != -1) {
					if ((chan->flags &
					     IEEE80211_CHAN_NO_HT40) ==
					    IEEE80211_CHAN_NO_HT40) {
						temp_chan_n->state =
							CHANNEL_STATE_DISABLE;
					} else {
						temp_chan_n->state =
							CHANNEL_STATE_DFS;
						temp_chan_n->pwr_limit =
							 chan->max_power-3;
					}
					temp_chan_n->flags = chan->flags;
				}
				if ((chan->flags &
				     IEEE80211_CHAN_NO_80MHZ) == 0)
					hdd_ctx->isVHT80Allowed = 1;
			} else {
				temp_chan_k->state = CHANNEL_STATE_ENABLE;
				temp_chan_k->pwr_limit = chan->max_power;
				temp_chan_k->flags = chan->flags;
				if (n != -1) {
					if ((chan->flags &
					     IEEE80211_CHAN_NO_HT40) ==
					    IEEE80211_CHAN_NO_HT40) {
						temp_chan_n->state =
							CHANNEL_STATE_DISABLE;
					} else {
						temp_chan_n->state =
							CHANNEL_STATE_ENABLE;
						temp_chan_n->pwr_limit =
							chan->max_power - 3;
					}
					temp_chan_n->flags = chan->flags;
				}
				if ((chan->flags &
				     IEEE80211_CHAN_NO_80MHZ) == 0)
					hdd_ctx->isVHT80Allowed = 1;
			}
		}
	}

	if (0 == (hdd_ctx->reg.eeprom_rd_ext &
		  (1 << WHAL_REG_EXT_FCC_CH_144))) {
		temp_chan = &(reg_channels[RF_CHAN_144]);
		temp_chan->state =
			CHANNEL_STATE_DISABLE;
	}

}


/**
 * hdd_regulatory_init() - regulatory_init
 * @hdd_ctx: hdd context
 * @wiphy: wiphy
 *
 * Return: int
 */
int hdd_regulatory_init(hdd_context_t *hdd_ctx, struct wiphy *wiphy)
{
	int ret_val;
	struct regulatory *reg_info;

	reg_info = &hdd_ctx->reg;

	hdd_regulatory_wiphy_init(hdd_ctx, reg_info, wiphy);

	hdd_process_regulatory_data(hdd_ctx, wiphy, true);

	reg_info->cc_src = SOURCE_DRIVER;

	ret_val = cds_fill_some_regulatory_info(reg_info);
	if (ret_val) {
		hdd_err("incorrect BDF regulatory data");
		return ret_val;
	}

	cds_put_default_country(reg_info->alpha2);

	init_completion(&hdd_ctx->reg_init);

	cds_fill_and_send_ctl_to_fw(reg_info);

	return 0;
}

/**
 * hdd_program_country_code() - process channel information from country code
 * @hdd_ctx: hddc context
 *
 * Return: void
 */
void hdd_program_country_code(hdd_context_t *hdd_ctx)
{
	struct wiphy *wiphy = hdd_ctx->wiphy;
	uint8_t *country_alpha2 = hdd_ctx->reg.alpha2;

	if (false == init_by_reg_core) {
		init_by_driver = true;
		if (('0' != country_alpha2[0]) ||
		    ('0' != country_alpha2[1])) {
			INIT_COMPLETION(hdd_ctx->reg_init);
			regulatory_hint(wiphy, country_alpha2);
			wait_for_completion_timeout(&hdd_ctx->reg_init,
					      msecs_to_jiffies(REG_WAIT_TIME));
		}
	}
}


/**
 * hdd_set_dfs_region() - set the dfs_region
 * @dfs_region: the dfs_region to set
 *
 * Return: void
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)) || defined(WITH_BACKPORTS)
static void hdd_set_dfs_region(hdd_context_t *hdd_ctx,
			       uint8_t dfs_reg)
{
	cds_put_dfs_region(dfs_reg);
}
#else
static void hdd_set_dfs_region(hdd_context_t *hdd_ctx,
				     uint8_t dfs_reg)
{

	/* remap the ctl code to dfs region code */
	switch (hdd_ctx->reg.ctl_5g) {
	case FCC:
		cds_put_dfs_region(DFS_FCC_DOMAIN);
		break;
	case ETSI:
		cds_put_dfs_region(DFS_ETSI_DOMAIN);
		break;
	case MKK:
		cds_put_dfs_region(DFS_MKK4_DOMAIN);
		break;
	default:
		/* set default dfs_region to FCC */
		cds_put_dfs_region(DFS_FCC_DOMAIN);
		break;
	}

}
#endif

/**
 * hdd_restore_custom_reg_settings() - restore custom reg settings
 * @wiphy: wiphy structure
 * @country_alpha2: alpha2 of the country
 * @reset: whether wiphy is reset
 *
 * Return: void
 */
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 14, 0)) || defined(WITH_BACKPORTS)
static void hdd_restore_custom_reg_settings(struct wiphy *wiphy,
					    uint8_t *country_alpha2,
					    bool *reset)
{
}
#else
static void hdd_restore_custom_reg_settings(struct wiphy *wiphy,
					    uint8_t *country_alpha2,
					    bool *reset)
{
	struct ieee80211_supported_band *sband;
	enum ieee80211_band band;
	struct ieee80211_channel *chan;
	int i;

	if ((country_alpha2[0] == '0') &&
	    (country_alpha2[1] == '0') &&
	    (wiphy->flags & WIPHY_FLAG_CUSTOM_REGULATORY)) {

		for (band = 0; band < IEEE80211_NUM_BANDS; band++) {
			sband = wiphy->bands[band];
			if (!sband)
				continue;
			for (i = 0; i < sband->n_channels; i++) {
				chan = &sband->channels[i];
				chan->flags = chan->orig_flags;
				chan->max_antenna_gain = chan->orig_mag;
				chan->max_power = chan->orig_mpwr;
			}
		}
		*reset = true;
	}
}
#endif


/**
 * hdd_restore_reg_flags() - restore regulatory flags
 * @flags: regulatory flags
 *
 * Return: void
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)) || defined(WITH_BACKPORTS)
static void hdd_restore_reg_flags(struct wiphy *wiphy, uint32_t flags)
{
	wiphy->regulatory_flags = flags;
}
#else
static void hdd_restore_reg_flags(struct wiphy *wiphy, uint32_t flags)
{
	wiphy->flags = flags;
}
#endif


/**
 * hdd_reg_notifier() - regulatory notifier
 * @wiphy: wiphy
 * @request: regulatory request
 *
 * Return: void
 */
void hdd_reg_notifier(struct wiphy *wiphy,
		      struct regulatory_request *request)
{
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	eCsrBand band_capability = eCSR_BAND_ALL;
	bool vht80_allowed;
	bool reset = false;
	uint8_t dfs_reg;

	hdd_info("country: %c%c, initiator %d, dfs_region: %d",
		  request->alpha2[0],
		  request->alpha2[1],
		  request->initiator,
		  request->dfs_region);

	if (NULL == hdd_ctx) {
		hdd_err("invalid hdd_ctx pointer");
		return;
	}

	if (cds_is_driver_unloading() || cds_is_driver_recovering()) {
		hdd_err("%s: unloading or ssr in progress, ignore",
			__func__);
		return;
	}

	sme_get_freq_band(hdd_ctx->hHal, &band_capability);

	/* first check if this callback is in response to the driver callback */

	switch (request->initiator) {
	case NL80211_REGDOM_SET_BY_DRIVER:
	case NL80211_REGDOM_SET_BY_CORE:
	case NL80211_REGDOM_SET_BY_USER:

		if ((false == init_by_driver) &&
		    (false == init_by_reg_core)) {

			if (NL80211_REGDOM_SET_BY_CORE == request->initiator)
				return;
			init_by_reg_core = true;
		}

		if ((NL80211_REGDOM_SET_BY_DRIVER == request->initiator) &&
		    (true == init_by_driver)) {

			/*
			 * restore the driver regulatory flags since
			 * regulatory_hint may have
			 * changed them
			 */
			hdd_restore_reg_flags(wiphy, hdd_ctx->reg.reg_flags);
		}

		if (NL80211_REGDOM_SET_BY_CORE == request->initiator) {
			hdd_ctx->reg.cc_src = SOURCE_CORE;
			if (is_wiphy_custom_regulatory(wiphy))
				reset = true;
		} else if (NL80211_REGDOM_SET_BY_DRIVER == request->initiator)
			hdd_ctx->reg.cc_src = SOURCE_DRIVER;
		else {
			hdd_ctx->reg.cc_src = SOURCE_USERSPACE;
			hdd_restore_custom_reg_settings(wiphy,
							request->alpha2,
							&reset);
		}

		hdd_ctx->reg.alpha2[0] = request->alpha2[0];
		hdd_ctx->reg.alpha2[1] = request->alpha2[1];

		hdd_update_regulatory_info(hdd_ctx);

		vht80_allowed = hdd_ctx->isVHT80Allowed;

		hdd_process_regulatory_data(hdd_ctx, wiphy, reset);

		if (hdd_ctx->isVHT80Allowed != vht80_allowed)
			hdd_checkandupdate_phymode(hdd_ctx);

		if (NL80211_REGDOM_SET_BY_DRIVER == request->initiator)
			complete(&hdd_ctx->reg_init);

		sme_generic_change_country_code(hdd_ctx->hHal,
						hdd_ctx->reg.alpha2);

		cds_fill_and_send_ctl_to_fw(&hdd_ctx->reg);

		hdd_set_dfs_region(hdd_ctx, request->dfs_region);

		cds_get_dfs_region(&dfs_reg);
		cds_set_wma_dfs_region(dfs_reg);
		break;

	default:
		break;
	}
}
