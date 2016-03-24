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

/*============================================================================
   FILE:         cds_reg_service.c
   OVERVIEW:     This source file contains definitions for CDS regulatory APIs
   DEPENDENCIES: None
   ============================================================================*/

#include <net/cfg80211.h>
#include "qdf_types.h"
#include "cds_reg_service.h"
#include "qdf_trace.h"
#include "sme_api.h"
#include "wlan_hdd_main.h"
#include "cds_regdomain.h"
#include "cds_regdomain_common.h"

#define WORLD_SKU_MASK          0x00F0
#define WORLD_SKU_PREFIX        0x0060
#define REG_WAIT_TIME           50

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

static const struct ieee80211_regdomain cds_world_regdom_60_61_62 = {
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

static const struct ieee80211_regdomain cds_world_regdom_63_65 = {
	.n_reg_rules = 4,
	.alpha2 =  "00",
	.reg_rules = {
		REG_RULE_2412_2462,
		REG_RULE_2467_2472,
		REG_RULE_5180_5320,
		REG_RULE_5745_5925,
	}
};

static const struct ieee80211_regdomain cds_world_regdom_64 = {
	.n_reg_rules = 3,
	.alpha2 =  "00",
	.reg_rules = {
		REG_RULE_2412_2462,
		REG_RULE_5180_5320,
		REG_RULE_5745_5925,
	}
};

static const struct ieee80211_regdomain cds_world_regdom_66_69 = {
	.n_reg_rules = 4,
	.alpha2 =  "00",
	.reg_rules = {
		REG_RULE_2412_2462,
		REG_RULE_5180_5320,
		REG_RULE_5500_5720,
		REG_RULE_5745_5925,
	}
};

static const struct ieee80211_regdomain cds_world_regdom_67_68_6A_6C = {
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

const struct chan_map chan_mapping[NUM_RF_CHANNELS] = {
	{2412, 1},
	{2417, 2},
	{2422, 3},
	{2427, 4},
	{2432, 5},
	{2437, 6},
	{2442, 7},
	{2447, 8},
	{2452, 9},
	{2457, 10},
	{2462, 11},
	{2467, 12},
	{2472, 13},
	{2484, 14},
	{5180, 36},
	{5200, 40},
	{5220, 44},
	{5240, 48},
	{5260, 52},
	{5280, 56},
	{5300, 60},
	{5320, 64},
	{5500, 100},
	{5520, 104},
	{5540, 108},
	{5560, 112},
	{5580, 116},
	{5600, 120},
	{5620, 124},
	{5640, 128},
	{5660, 132},
	{5680, 136},
	{5700, 140},
	{5720, 144},
	{5745, 149},
	{5765, 153},
	{5785, 157},
	{5805, 161},
	{5825, 165},
	{5852, 170},
	{5855, 171},
	{5860, 172},
	{5865, 173},
	{5870, 174},
	{5875, 175},
	{5880, 176},
	{5885, 177},
	{5890, 178},
	{5895, 179},
	{5900, 180},
	{5905, 181},
	{5910, 182},
	{5915, 183},
	{5920, 184},
	{2422, 3},
	{2427, 4},
	{2432, 5},
	{2437, 6},
	{2442, 7},
	{2447, 8},
	{2452, 9},
	{2457, 10},
	{2462, 11},
	{5190, 38},
	{5210, 42},
	{5230, 46},
	{5250, 50},
	{5270, 54},
	{5290, 58},
	{5310, 62},
	{5510, 102},
	{5530, 106},
	{5550, 110},
	{5570, 114},
	{5590, 118},
	{5610, 122},
	{5630, 126},
	{5650, 130},
	{5670, 134},
	{5690, 138},
	{5710, 142},
	{5755, 151},
	{5775, 155},
	{5795, 159},
	{5815, 163},
};

struct regulatory_channel reg_channels[NUM_RF_CHANNELS];
static bool init_by_driver;
static bool init_by_reg_core;

/**
 * cds_is_world_regdomain() - whether world regdomain
 * @regd: integer regulatory domain
 *
 * Return: bool
 */
bool cds_is_world_regdomain(uint32_t reg_domain)
{
	uint32_t temp_regd = reg_domain & ~WORLDWIDE_ROAMING_FLAG;

	return ((temp_regd & COUNTRY_ERD_FLAG) != COUNTRY_ERD_FLAG) &&
		(((temp_regd & WORLD_SKU_MASK) == WORLD_SKU_PREFIX) ||
		 (temp_regd == WORLD));

}


/**
 * cds_world_regdomain() - which constant world regdomain
 * @reg: regulatory data
 *
 * Return: regdomain ptr
 */
static const struct ieee80211_regdomain
*cds_world_regdomain(struct regulatory *reg)
{
	REG_DMN_PAIR_MAPPING *regpair =
		(REG_DMN_PAIR_MAPPING *)reg->regpair;

	switch (regpair->regDmnEnum) {
	case 0x60:
	case 0x61:
	case 0x62:
		return &cds_world_regdom_60_61_62;
	case 0x63:
	case 0x65:
		return &cds_world_regdom_63_65;
	case 0x64:
		return &cds_world_regdom_64;
	case 0x66:
	case 0x69:
		return &cds_world_regdom_66_69;
	case 0x67:
	case 0x68:
	case 0x6A:
	case 0x6C:
		return &cds_world_regdom_67_68_6A_6C;
	default:
		WARN_ON(1);
		return &cds_world_regdom_60_61_62;
	}
}

/**
 * cds_regulatory_wiphy_init() - regulatory wiphy init
 * @hdd_ctx: hdd context
 * @reg: regulatory data
 * @wiphy: wiphy structure
 *
 * Return: int
 */
static int cds_regulatory_wiphy_init(hdd_context_t *hdd_ctx,
				     struct regulatory *reg,
				     struct wiphy *wiphy)
{
	const struct ieee80211_regdomain *reg_domain;

	if (cds_is_world_regdomain(reg->reg_domain)) {
		reg_domain = cds_world_regdomain(reg);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)) || defined(WITH_BACKPORTS)
		wiphy->regulatory_flags |= REGULATORY_CUSTOM_REG;
#else
		wiphy->flags |= WIPHY_FLAG_CUSTOM_REGULATORY;
#endif
	} else if (hdd_ctx->config->fRegChangeDefCountry) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)) || defined(WITH_BACKPORTS)
		wiphy->regulatory_flags |= REGULATORY_CUSTOM_REG;
#else
		wiphy->flags |= WIPHY_FLAG_CUSTOM_REGULATORY;
#endif
		reg_domain = &cds_world_regdom_60_61_62;
	} else {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)) || defined(WITH_BACKPORTS)
		wiphy->regulatory_flags |= REGULATORY_STRICT_REG;
#else
		wiphy->flags |= WIPHY_FLAG_STRICT_REGULATORY;
#endif
		reg_domain = &cds_world_regdom_60_61_62;
	}

	/*
	 * save the original driver regulatory flags
	 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)) || defined(WITH_BACKPORTS)
	hdd_ctx->reg.reg_flags = wiphy->regulatory_flags;
#else
	hdd_ctx->reg.reg_flags = wiphy->flags;
#endif
	wiphy_apply_custom_regulatory(wiphy, reg_domain);

	/*
	 * restore the driver regulatory flags since
	 * wiphy_apply_custom_regulatory may have
	 * changed them
	 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)) || defined(WITH_BACKPORTS)
	wiphy->regulatory_flags = hdd_ctx->reg.reg_flags;
#else
	wiphy->flags = hdd_ctx->reg.reg_flags;
#endif

	return 0;
}

/**
 * cds_update_regulatory_info() - update regulatory info
 * @hdd_ctx: hdd context
 *
 * Return: QDF_STATUS
 */
static void cds_update_regulatory_info(hdd_context_t *hdd_ctx)
{
	uint32_t country_code;

	country_code = cds_get_country_from_alpha2(hdd_ctx->reg.alpha2);

	hdd_ctx->reg.reg_domain = COUNTRY_ERD_FLAG;
	hdd_ctx->reg.reg_domain |= country_code;

	cds_fill_some_regulatory_info(&hdd_ctx->reg);

	return;
}


/**
 * cds_get_channel_list_with_power() - retrieve channel list with power
 * @base_channels: base channels
 * @num_base_channels: number of base channels
 * @channels_40mhz: 40 MHz channels
 * @num_40mhz_channels: number of 40 Mhz channels
 *
 * Return: QDF_STATUS_SUCCESS
 */
QDF_STATUS cds_get_channel_list_with_power(struct channel_power
					   *base_channels,
					   uint8_t *num_base_channels,
					   struct channel_power
					   *channels_40mhz,
					   uint8_t *num_40mhz_channels)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	int i, count;

	if (base_channels && num_base_channels) {
		count = 0;
		for (i = 0; i <= RF_CHAN_14; i++) {
			if (reg_channels[i].state) {
				base_channels[count].chan_num =
					chan_mapping[i].chan_num;
				base_channels[count++].power =
					reg_channels[i].pwr_limit;
			}
		}
		for (i = RF_CHAN_36; i <= RF_CHAN_184; i++) {
			if (reg_channels[i].state) {
				base_channels[count].chan_num =
					chan_mapping[i].chan_num;
				base_channels[count++].power =
					reg_channels[i].pwr_limit;
			}
		}
		*num_base_channels = count;
	}

	if (channels_40mhz && num_40mhz_channels) {
		count = 0;

		for (i = RF_CHAN_BOND_3; i <= RF_CHAN_BOND_11; i++) {
			if (reg_channels[i].state) {
				channels_40mhz[count].chan_num =
					chan_mapping[i].chan_num;
				channels_40mhz[count++].power =
					reg_channels[i].pwr_limit;
			}
		}

		for (i = RF_CHAN_BOND_38; i <= RF_CHAN_BOND_163; i++) {
			if (reg_channels[i].state) {
				channels_40mhz[count].chan_num =
					chan_mapping[i].chan_num;
				channels_40mhz[count++].power =
					reg_channels[i].pwr_limit;
			}
		}
		*num_40mhz_channels = count;
	}

	return status;
}

/**
 * cds_read_default_country() - set the default country
 * @default_country: default country
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cds_read_default_country(uint8_t *default_country)
{
	hdd_context_t *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			  "invalid hdd_ctx pointer");
		return QDF_STATUS_E_FAULT;
	}

	memcpy(default_country,
	       hdd_ctx->reg.def_country,
	       CDS_COUNTRY_CODE_LEN + 1);

	QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_INFO,
		  "default country is %c%c\n",
		  default_country[0],
		  default_country[1]);

	return QDF_STATUS_SUCCESS;
}

/**
 * cds_get_channel_enum() - get the channel enumeration
 * @chan_num: channel number
 *
 * Return: enum for the channel
 */
static enum channel_enum cds_get_channel_enum(uint32_t chan_num)
{
	uint32_t loop;

	for (loop = 0; loop <= RF_CHAN_184; loop++)
		if (chan_mapping[loop].chan_num == chan_num)
			return loop;

	QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
		  "invalid channel number %d", chan_num);

	return INVALID_RF_CHANNEL;
}


/**
 * cds_get_channel_state() - get the channel state
 * @chan_num: channel number
 *
 * Return: channel state
 */
enum channel_state cds_get_channel_state(uint32_t chan_num)
{
	enum channel_enum chan_enum;

	chan_enum = cds_get_channel_enum(chan_num);
	if (INVALID_RF_CHANNEL == chan_enum)
		return CHANNEL_STATE_INVALID;
	else
		return reg_channels[chan_enum].state;
}


/**
 * cds_get_bonded_channel_state() - get the bonded channel state
 * @channel_num: channel number
 *
 * Return: channel state
 */
enum channel_state cds_get_bonded_channel_state(uint32_t chan_num,
					   enum channel_width ch_width)
{
	enum channel_enum chan_enum;
	bool bw_enabled = false;

	chan_enum = cds_get_channel_enum(chan_num);
	if (INVALID_RF_CHANNEL == chan_enum)
		return CHANNEL_STATE_INVALID;

	if (reg_channels[chan_enum].state) {
		if (CHAN_WIDTH_5MHZ == ch_width)
			bw_enabled = 1;
		else if (CHAN_WIDTH_10MHZ == ch_width)
			bw_enabled = !(reg_channels[chan_enum].flags &
				       IEEE80211_CHAN_NO_10MHZ);
		else if (CHAN_WIDTH_20MHZ == ch_width)
			bw_enabled = !(reg_channels[chan_enum].flags &
				       IEEE80211_CHAN_NO_20MHZ);
		else if (CHAN_WIDTH_40MHZ == ch_width)
			bw_enabled = !(reg_channels[chan_enum].flags &
				       IEEE80211_CHAN_NO_HT40);
		else if (CHAN_WIDTH_80MHZ == ch_width)
			bw_enabled = !(reg_channels[chan_enum].flags &
				       IEEE80211_CHAN_NO_80MHZ);
		else if (CHAN_WIDTH_160MHZ == ch_width)
			bw_enabled = !(reg_channels[chan_enum].flags &
				       IEEE80211_CHAN_NO_160MHZ);
	}

	if (bw_enabled)
		return reg_channels[chan_enum].state;
	else
		return CHANNEL_STATE_DISABLE;
}

/**
 * cds_get_max_channel_bw() - get the max channel bandwidth
 * @channel_num: channel number
 *
 * Return: channel_width
 */
enum channel_width cds_get_max_channel_bw(uint32_t chan_num)
{
	enum channel_enum chan_enum;
	enum channel_width chan_bw = CHAN_WIDTH_0MHZ;

	chan_enum = cds_get_channel_enum(chan_num);

	if ((INVALID_RF_CHANNEL != chan_enum) &&
	    (CHANNEL_STATE_DISABLE != reg_channels[chan_enum].state)) {

		if (!(reg_channels[chan_enum].flags &
		      IEEE80211_CHAN_NO_160MHZ))
			chan_bw = CHAN_WIDTH_160MHZ;
		else if (!(reg_channels[chan_enum].flags &
			   IEEE80211_CHAN_NO_80MHZ))
			chan_bw = CHAN_WIDTH_80MHZ;
		else if (!(reg_channels[chan_enum].flags &
			   IEEE80211_CHAN_NO_HT40))
			chan_bw = CHAN_WIDTH_40MHZ;
		else if (!(reg_channels[chan_enum].flags &
			   IEEE80211_CHAN_NO_20MHZ))
			chan_bw = CHAN_WIDTH_20MHZ;
		else if (!(reg_channels[chan_enum].flags &
			   IEEE80211_CHAN_NO_10MHZ))
			chan_bw = CHAN_WIDTH_10MHZ;
		else
			chan_bw = CHAN_WIDTH_5MHZ;
	}

	return chan_bw;

}

static int cds_bw20_ch_index_to_bw40_ch_index(int k)
{
	int m = -1;
	if (k >= RF_CHAN_1 && k <= RF_CHAN_13) {
		/*
		 * Channel bonding is not valid for channel 14,
		 * Hence don't consider it
		 */
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
 * cds_set_dfs_region() - set the dfs_region
 * @dfs_region: the dfs_region to set
 *
 * Return: QDF_STATUS_SUCCESS if dfs_region set correctly
 *         QDF_STATUS_E_EXISTS if hdd context not found
 */
QDF_STATUS cds_set_dfs_region(uint8_t dfs_region)
{
	hdd_context_t *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	if (NULL == hdd_ctx)
		return QDF_STATUS_E_EXISTS;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)) || defined(WITH_BACKPORTS)
	hdd_ctx->reg.dfs_region = dfs_region;
#else

	/* remap the ctl code to dfs region code */
	switch (hdd_ctx->reg.ctl_5g) {
	case FCC:
		hdd_ctx->reg.dfs_region = DFS_FCC_DOMAIN;
		break;
	case ETSI:
		hdd_ctx->reg.dfs_region = DFS_ETSI_DOMAIN;
		break;
	case MKK:
		hdd_ctx->reg.dfs_region = DFS_MKK4_DOMAIN;
		break;
	default:
		/* set default dfs_region to FCC */
		hdd_ctx->reg.dfs_region = DFS_FCC_DOMAIN;
		break;
	}
#endif
	return QDF_STATUS_SUCCESS;
}

/**
 * cds_get_dfs_region() - get the dfs_region
 * @dfs_region: the dfs_region to return
 *
 * Return: QDF_STATUS_SUCCESS if dfs_region set correctly
 *         QDF_STATUS_E_EXISTS if hdd context not found
 */
QDF_STATUS cds_get_dfs_region(uint8_t *dfs_region)
{
	hdd_context_t *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	if (NULL == hdd_ctx)
		return QDF_STATUS_E_EXISTS;

	*dfs_region = hdd_ctx->reg.dfs_region;

	return QDF_STATUS_SUCCESS;
}

/**
 * cds_get_reg_domain_from_country_code() - get the regulatory domain
 * @reg_domain_ptr: ptr to store regulatory domain
 *
 * Return: QDF_STATUS_SUCCESS on success
 *         QDF_STATUS_E_FAULT on error
 *         QDF_STATUS_E_EMPTY country table empty
 */
QDF_STATUS cds_get_reg_domain_from_country_code(v_REGDOMAIN_t *reg_domain_ptr,
						const uint8_t *country_alpha2,
						enum country_src source)
{
	hdd_context_t *hdd_ctx = NULL;
	struct wiphy *wiphy = NULL;

	if (NULL == reg_domain_ptr) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			  ("Invalid reg domain pointer"));
		return QDF_STATUS_E_FAULT;
	}

	*reg_domain_ptr = 0;

	if (SOURCE_QUERY == source)
		return QDF_STATUS_SUCCESS;

	if (NULL == country_alpha2) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			  ("Country code array is NULL"));
		return QDF_STATUS_E_FAULT;
	}

	if (cds_is_driver_recovering()) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			  "SSR in progress, return");
		return QDF_STATUS_SUCCESS;
	}

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	if (NULL == hdd_ctx) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			  ("Invalid pHddCtx pointer"));
		return QDF_STATUS_E_FAULT;
	}

	wiphy = hdd_ctx->wiphy;

	if ((SOURCE_DRIVER == source) && (false == init_by_reg_core)) {
		init_by_driver = true;
		if (('0' != country_alpha2[0]) || ('0' != country_alpha2[1])) {
			INIT_COMPLETION(hdd_ctx->reg_init);
			regulatory_hint(wiphy, country_alpha2);
			wait_for_completion_timeout(&hdd_ctx->reg_init,
					       msecs_to_jiffies(REG_WAIT_TIME));
		}
	} else if (SOURCE_11D == source || SOURCE_USERSPACE == source) {
		regulatory_hint_user(country_alpha2,
				     NL80211_USER_REG_HINT_USER);
	}

	return QDF_STATUS_SUCCESS;
}

/*
 * cds_is_dsrc_channel() - is the channel DSRC
 * @center_freq: center freq of the channel
 *
 * Return: true if dsrc channel
 *         false otherwise
 */
bool cds_is_dsrc_channel(uint16_t center_freq)
{
	if (center_freq >= 5852 &&
	    center_freq <= 5920)
		return 1;

	return 0;
}

/**
 * cds_process_regulatory_data() - process regulatory data
 * @wiphy: wiphy
 * @band_capability: band_capability
 *
 * Return: int
 */
static int cds_process_regulatory_data(struct wiphy *wiphy,
				       uint8_t band_capability,
				       bool reset)
{
	int i, j, m;
	int k = 0, n = 0;
	hdd_context_t *hdd_ctx;
	const struct ieee80211_reg_rule *reg_rule;
	struct ieee80211_channel *chan;
	struct regulatory_channel *temp_chan_k, *temp_chan_n, *temp_chan;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (NULL == hdd_ctx) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			  "invalid hdd_ctx pointer");
		return QDF_STATUS_E_FAULT;
	}

	hdd_ctx->isVHT80Allowed = 0;

	if (band_capability == eCSR_BAND_24)
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_INFO,
			  "band capability is set to 2G only");

	for (i = 0, m = 0; i < IEEE80211_NUM_BANDS; i++) {

		if (i == IEEE80211_BAND_2GHZ && band_capability == eCSR_BAND_5G)
			continue;

		else if (i == IEEE80211_BAND_5GHZ
			 && band_capability == eCSR_BAND_24)
			continue;

		if (wiphy->bands[i] == NULL)
			continue;

		if (i == 0)
			m = 0;
		else
			m = wiphy->bands[i-1]->n_channels + m;

		for (j = 0; j < wiphy->bands[i]->n_channels; j++) {

			k = m + j;
			n = cds_bw20_ch_index_to_bw40_ch_index(k);

			chan = &(wiphy->bands[i]->channels[j]);
			temp_chan_k = &(reg_channels[k]);
			temp_chan_n = &(reg_channels[n]);

			if ((!reset) &&
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)) || defined(WITH_BACKPORTS)
			    (wiphy->regulatory_flags &
			     REGULATORY_CUSTOM_REG)) {
#else
				(wiphy->flags &
				 WIPHY_FLAG_CUSTOM_REGULATORY)) {
#endif
				reg_rule = freq_reg_info(wiphy,
							 MHZ_TO_KHZ(chan->
								 center_freq));

				if (!IS_ERR(reg_rule)) {
					chan->flags &=
						~IEEE80211_CHAN_DISABLED;

					if (!(reg_rule->flags &
					      NL80211_RRF_DFS)) {
						QDF_TRACE(QDF_MODULE_ID_QDF,
							  QDF_TRACE_LEVEL_INFO,
							  "%s: Remove passive scan restriction for %u",
							  __func__,
							  chan->center_freq);
						chan->flags &=
							~IEEE80211_CHAN_RADAR;
					}

					if (!(reg_rule->flags &
					      NL80211_RRF_PASSIVE_SCAN)) {
						QDF_TRACE(QDF_MODULE_ID_QDF,
							  QDF_TRACE_LEVEL_INFO,
							  "%s: Remove passive scan restriction for %u",
							  __func__,
							  chan->center_freq);
						chan->flags &=
						   ~IEEE80211_CHAN_PASSIVE_SCAN;
					}

					if (!(reg_rule->flags &
					      NL80211_RRF_NO_IBSS)) {
						QDF_TRACE(QDF_MODULE_ID_QDF,
							  QDF_TRACE_LEVEL_INFO,
							  "%s: Remove no ibss restriction for %u",
							  __func__,
							  chan->center_freq);
						chan->flags &=
							~IEEE80211_CHAN_NO_IBSS;
					}

					chan->max_power = MBM_TO_DBM(reg_rule->
								     power_rule.
								     max_eirp);
				}
			}

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
				    IEEE80211_CHAN_PASSIVE_SCAN
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0))
					|
					IEEE80211_CHAN_INDOOR_ONLY
#endif
				)) {

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0))
				if (chan->flags &
				    IEEE80211_CHAN_INDOOR_ONLY)
					chan->flags |=
						IEEE80211_CHAN_PASSIVE_SCAN;
#endif
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

	if (k == 0)
		return -1;

	return QDF_STATUS_SUCCESS;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0)) && !defined(WITH_BACKPORTS)
/**
 * restore_custom_reg_settings() - restore custom reg settings
 * @wiphy: wiphy structure
 *
 * Return: void
 */
static void restore_custom_reg_settings(struct wiphy *wiphy)
{
	struct ieee80211_supported_band *sband;
	enum ieee80211_band band;
	struct ieee80211_channel *chan;
	int i;

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
}
#endif

/**
 * __hdd_reg_notifier() - regulatory notifier
 * @wiphy: wiphy
 * @request: regulatory request
 *
 * Return: void or int
 */
void __hdd_reg_notifier(struct wiphy *wiphy,
		      struct regulatory_request *request)
{
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	eCsrBand band_capability = eCSR_BAND_ALL;
	bool vht80_allowed;
	bool reset = false;

	QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_INFO,
		  FL("country: %c%c, initiator %d, dfs_region: %d"),
		  request->alpha2[0],
		  request->alpha2[1],
		  request->initiator,
		  request->dfs_region);

	if (NULL == hdd_ctx) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			  ("Invalid pHddCtx pointer"));
		return;
	}

	if (cds_is_driver_unloading() || cds_is_driver_recovering()) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			  "%s: Unloading or SSR in Progress, Ignore!!!",
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

			if (NL80211_REGDOM_SET_BY_CORE == request->initiator) {
				return;
			}
			init_by_reg_core = true;
		}

		if ((NL80211_REGDOM_SET_BY_DRIVER == request->initiator) &&
		    (true == init_by_driver)) {

			/*
			 * restore the driver regulatory flags since
			 * regulatory_hint may have
			 * changed them
			 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)) || defined(WITH_BACKPORTS)
			wiphy->regulatory_flags = hdd_ctx->reg.reg_flags;
#else
			wiphy->flags = hdd_ctx->reg.reg_flags;
#endif
		}

		if (NL80211_REGDOM_SET_BY_CORE == request->initiator) {
			hdd_ctx->reg.cc_src = SOURCE_CORE;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)) || defined(WITH_BACKPORTS)
			if (wiphy->regulatory_flags & REGULATORY_CUSTOM_REG)
#else
			if (wiphy->flags & WIPHY_FLAG_CUSTOM_REGULATORY)
#endif
				reset = true;
		} else if (NL80211_REGDOM_SET_BY_DRIVER == request->initiator)
			hdd_ctx->reg.cc_src = SOURCE_DRIVER;
		else {
			hdd_ctx->reg.cc_src = SOURCE_USERSPACE;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0)) && !defined(WITH_BACKPORTS)
			if ((request->alpha2[0] == '0') &&
			    (request->alpha2[1] == '0') &&
			    (wiphy->flags & WIPHY_FLAG_CUSTOM_REGULATORY)) {
				restore_custom_reg_settings(wiphy);
				reset = true;
			}
#endif
		}

		hdd_ctx->reg.alpha2[0] = request->alpha2[0];
		hdd_ctx->reg.alpha2[1] = request->alpha2[1];

		cds_update_regulatory_info(hdd_ctx);

		vht80_allowed = hdd_ctx->isVHT80Allowed;
		if (cds_process_regulatory_data(wiphy, band_capability,
						reset) == 0) {
			QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_INFO,
				  (" regulatory entry created"));
		}
		if (hdd_ctx->isVHT80Allowed != vht80_allowed)
			hdd_checkandupdate_phymode(hdd_ctx);

		if (NL80211_REGDOM_SET_BY_DRIVER == request->initiator)
			complete(&hdd_ctx->reg_init);

		sme_generic_change_country_code(hdd_ctx->hHal,
						hdd_ctx->reg.alpha2);

		cds_fill_and_send_ctl_to_fw(&hdd_ctx->reg);

		cds_set_dfs_region(request->dfs_region);

		cds_set_wma_dfs_region(&hdd_ctx->reg);
	default:
		break;
	}

	return;
}

/**
 * hdd_reg_notifier() - regulatory notifier
 * @wiphy: wiphy
 * @request: regulatory request
 *
 * Return: void or int
 */
void hdd_reg_notifier(struct wiphy *wiphy,
		      struct regulatory_request *request)
{
	cds_ssr_protect(__func__);
	__hdd_reg_notifier(wiphy, request);
	cds_ssr_unprotect(__func__);
}

/**
 * cds_regulatory_init() - regulatory_init
 * Return: QDF_STATUS
 */
QDF_STATUS cds_regulatory_init(void)
{
	hdd_context_t *hdd_ctx = NULL;
	struct wiphy *wiphy = NULL;
	int ret_val = 0;
	struct regulatory *reg_info;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			  "invalid hdd_ctx pointer");
		return QDF_STATUS_E_FAULT;
	}

	wiphy = hdd_ctx->wiphy;

	reg_info = &hdd_ctx->reg;

	cds_regulatory_wiphy_init(hdd_ctx, reg_info, wiphy);

	if (cds_process_regulatory_data(wiphy,
					hdd_ctx->config->
					nBandCapability, true) != 0) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			  ("Error while creating regulatory entry"));
		return QDF_STATUS_E_FAULT;
	}

	reg_info->cc_src = SOURCE_DRIVER;

	ret_val = cds_fill_some_regulatory_info(reg_info);
	if (ret_val) {
		qdf_print(KERN_ERR "Error in getting country code\n");
		return ret_val;
	}

	hdd_ctx->reg.def_country[0] = reg_info->alpha2[0];
	hdd_ctx->reg.def_country[1] = reg_info->alpha2[1];

	init_completion(&hdd_ctx->reg_init);

	cds_fill_and_send_ctl_to_fw(reg_info);

	return QDF_STATUS_SUCCESS;
}

/**
 * cds_set_reg_domain() - set regulatory domain
 * @client_ctxt: client context
 * @reg_domain: regulatory domain
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cds_set_reg_domain(void *client_ctxt, v_REGDOMAIN_t reg_domain)
{
	if (reg_domain >= REGDOMAIN_COUNT) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			  "CDS set reg domain, invalid REG domain ID %d",
			  reg_domain);
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * cds_set_ch_params() - set channel parameters
 * @ch: channel
 * @phy_mode: physical mode
 * @ch_param: channel parameters will be returned
 *
 * Return: None
 */
void cds_set_ch_params(uint8_t ch, uint32_t phy_mode,
		chan_params_t *ch_params)
{
	tHalHandle *hal_ctx = cds_get_context(QDF_MODULE_ID_PE);
	if (!hal_ctx) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			("Invalid hal_ctx pointer"));
		return;
	}
	/*
	 * TODO: remove SME call and move the SME set channel
	 * param functionality to CDS.
	 */
	sme_set_ch_params(hal_ctx, phy_mode, ch, 0, ch_params);
}
