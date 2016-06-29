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

/**
 * DOC: wlan_hdd_cfg80211.c
 *
 * WLAN Host Device Driver cfg80211 APIs implementation
 *
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/etherdevice.h>
#include <linux/wireless.h>
#include <wlan_hdd_includes.h>
#include <net/arp.h>
#include <net/cfg80211.h>
#include <qdf_trace.h>
#include <wlan_hdd_wowl.h>
#include <ani_global.h>
#include "sir_params.h"
#include "dot11f.h"
#include "wlan_hdd_assoc.h"
#include "wlan_hdd_wext.h"
#include "sme_api.h"
#include "sme_power_save_api.h"
#include "wlan_hdd_p2p.h"
#include "wlan_hdd_cfg80211.h"
#include "wlan_hdd_hostapd.h"
#include "wlan_hdd_softap_tx_rx.h"
#include "wlan_hdd_main.h"
#include "wlan_hdd_power.h"
#include "wlan_hdd_trace.h"
#include "qdf_types.h"
#include "qdf_trace.h"
#include "cds_utils.h"
#include "cds_sched.h"
#include "wlan_hdd_scan.h"
#include <qc_sap_ioctl.h>
#include "wlan_hdd_tdls.h"
#include "wlan_hdd_wmm.h"
#include "wma_types.h"
#include "wlan_hdd_misc.h"
#include "wlan_hdd_nan.h"
#include <wlan_hdd_ipa.h>
#include "wlan_logging_sock_svc.h"

#ifdef FEATURE_WLAN_EXTSCAN
#include "wlan_hdd_ext_scan.h"
#endif

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
#include "wlan_hdd_stats.h"
#endif
#include "cds_concurrency.h"
#include "qwlan_version.h"
#include "wlan_hdd_memdump.h"

#include "wlan_hdd_ocb.h"
#include "wlan_hdd_tsf.h"
#include "ol_txrx.h"

#include "wlan_hdd_subnet_detect.h"
#include <wlan_hdd_regulatory.h>
#include "wlan_hdd_lpass.h"
#include "wlan_hdd_nan_datapath.h"

#define g_mode_rates_size (12)
#define a_mode_rates_size (8)
#define GET_IE_LEN_IN_BSS_DESC(lenInBss) (lenInBss + sizeof(lenInBss) - \
					   ((uintptr_t)OFFSET_OF(tSirBssDescription, ieFields)))

/*
 * Android CTS verifier needs atleast this much wait time (in msec)
 */
#define MAX_REMAIN_ON_CHANNEL_DURATION (5000)

/*
 * Refer @tCfgProtection structure for definition of the bit map.
 * below value is obtained by setting the following bit-fields.
 * enable obss, fromllb, overlapOBSS and overlapFromllb protection.
 */
#define IBSS_CFG_PROTECTION_ENABLE_MASK 0x8282

#define HDD2GHZCHAN(freq, chan, flag)   {     \
		.band =  IEEE80211_BAND_2GHZ, \
		.center_freq = (freq), \
		.hw_value = (chan), \
		.flags = (flag), \
		.max_antenna_gain = 0, \
		.max_power = 30, \
}

#define HDD5GHZCHAN(freq, chan, flag)   {     \
		.band =  IEEE80211_BAND_5GHZ, \
		.center_freq = (freq), \
		.hw_value = (chan), \
		.flags = (flag), \
		.max_antenna_gain = 0, \
		.max_power = 30, \
}

#define HDD_G_MODE_RATETAB(rate, rate_id, flag)	\
	{ \
		.bitrate = rate, \
		.hw_value = rate_id, \
		.flags = flag, \
	}

#define WLAN_AKM_SUITE_FT_8021X         0x000FAC03
#define WLAN_AKM_SUITE_FT_PSK           0x000FAC04

#define HDD_CHANNEL_14 14

#define MAX_TXPOWER_SCALE 4
#define CDS_MAX_FEATURE_SET   8

static const u32 hdd_cipher_suites[] = {
	WLAN_CIPHER_SUITE_WEP40,
	WLAN_CIPHER_SUITE_WEP104,
	WLAN_CIPHER_SUITE_TKIP,
#ifdef FEATURE_WLAN_ESE
#define WLAN_CIPHER_SUITE_BTK 0x004096fe        /* use for BTK */
#define WLAN_CIPHER_SUITE_KRK 0x004096ff        /* use for KRK */
	WLAN_CIPHER_SUITE_BTK,
	WLAN_CIPHER_SUITE_KRK,
	WLAN_CIPHER_SUITE_CCMP,
#else
	WLAN_CIPHER_SUITE_CCMP,
#endif
#ifdef FEATURE_WLAN_WAPI
	WLAN_CIPHER_SUITE_SMS4,
#endif
#ifdef WLAN_FEATURE_11W
	WLAN_CIPHER_SUITE_AES_CMAC,
#endif
};

static const struct ieee80211_channel hdd_channels_2_4_ghz[] = {
	HDD2GHZCHAN(2412, 1, 0),
	HDD2GHZCHAN(2417, 2, 0),
	HDD2GHZCHAN(2422, 3, 0),
	HDD2GHZCHAN(2427, 4, 0),
	HDD2GHZCHAN(2432, 5, 0),
	HDD2GHZCHAN(2437, 6, 0),
	HDD2GHZCHAN(2442, 7, 0),
	HDD2GHZCHAN(2447, 8, 0),
	HDD2GHZCHAN(2452, 9, 0),
	HDD2GHZCHAN(2457, 10, 0),
	HDD2GHZCHAN(2462, 11, 0),
	HDD2GHZCHAN(2467, 12, 0),
	HDD2GHZCHAN(2472, 13, 0),
	HDD2GHZCHAN(2484, 14, 0),
};

static const struct ieee80211_channel hdd_channels_5_ghz[] = {
	HDD5GHZCHAN(5180, 36, 0),
	HDD5GHZCHAN(5200, 40, 0),
	HDD5GHZCHAN(5220, 44, 0),
	HDD5GHZCHAN(5240, 48, 0),
	HDD5GHZCHAN(5260, 52, 0),
	HDD5GHZCHAN(5280, 56, 0),
	HDD5GHZCHAN(5300, 60, 0),
	HDD5GHZCHAN(5320, 64, 0),
	HDD5GHZCHAN(5500, 100, 0),
	HDD5GHZCHAN(5520, 104, 0),
	HDD5GHZCHAN(5540, 108, 0),
	HDD5GHZCHAN(5560, 112, 0),
	HDD5GHZCHAN(5580, 116, 0),
	HDD5GHZCHAN(5600, 120, 0),
	HDD5GHZCHAN(5620, 124, 0),
	HDD5GHZCHAN(5640, 128, 0),
	HDD5GHZCHAN(5660, 132, 0),
	HDD5GHZCHAN(5680, 136, 0),
	HDD5GHZCHAN(5700, 140, 0),
	HDD5GHZCHAN(5720, 144, 0),
	HDD5GHZCHAN(5745, 149, 0),
	HDD5GHZCHAN(5765, 153, 0),
	HDD5GHZCHAN(5785, 157, 0),
	HDD5GHZCHAN(5805, 161, 0),
	HDD5GHZCHAN(5825, 165, 0),
	HDD5GHZCHAN(5852, 170, 0),
	HDD5GHZCHAN(5855, 171, 0),
	HDD5GHZCHAN(5860, 172, 0),
	HDD5GHZCHAN(5865, 173, 0),
	HDD5GHZCHAN(5870, 174, 0),
	HDD5GHZCHAN(5875, 175, 0),
	HDD5GHZCHAN(5880, 176, 0),
	HDD5GHZCHAN(5885, 177, 0),
	HDD5GHZCHAN(5890, 178, 0),
	HDD5GHZCHAN(5895, 179, 0),
	HDD5GHZCHAN(5900, 180, 0),
	HDD5GHZCHAN(5905, 181, 0),
	HDD5GHZCHAN(5910, 182, 0),
	HDD5GHZCHAN(5915, 183, 0),
	HDD5GHZCHAN(5920, 184, 0),
};

static struct ieee80211_rate g_mode_rates[] = {
	HDD_G_MODE_RATETAB(10, 0x1, 0),
	HDD_G_MODE_RATETAB(20, 0x2, 0),
	HDD_G_MODE_RATETAB(55, 0x4, 0),
	HDD_G_MODE_RATETAB(110, 0x8, 0),
	HDD_G_MODE_RATETAB(60, 0x10, 0),
	HDD_G_MODE_RATETAB(90, 0x20, 0),
	HDD_G_MODE_RATETAB(120, 0x40, 0),
	HDD_G_MODE_RATETAB(180, 0x80, 0),
	HDD_G_MODE_RATETAB(240, 0x100, 0),
	HDD_G_MODE_RATETAB(360, 0x200, 0),
	HDD_G_MODE_RATETAB(480, 0x400, 0),
	HDD_G_MODE_RATETAB(540, 0x800, 0),
};

static struct ieee80211_rate a_mode_rates[] = {
	HDD_G_MODE_RATETAB(60, 0x10, 0),
	HDD_G_MODE_RATETAB(90, 0x20, 0),
	HDD_G_MODE_RATETAB(120, 0x40, 0),
	HDD_G_MODE_RATETAB(180, 0x80, 0),
	HDD_G_MODE_RATETAB(240, 0x100, 0),
	HDD_G_MODE_RATETAB(360, 0x200, 0),
	HDD_G_MODE_RATETAB(480, 0x400, 0),
	HDD_G_MODE_RATETAB(540, 0x800, 0),
};

static struct ieee80211_supported_band wlan_hdd_band_2_4_ghz = {
	.channels = NULL,
	.n_channels = ARRAY_SIZE(hdd_channels_2_4_ghz),
	.band = IEEE80211_BAND_2GHZ,
	.bitrates = g_mode_rates,
	.n_bitrates = g_mode_rates_size,
	.ht_cap.ht_supported = 1,
	.ht_cap.cap = IEEE80211_HT_CAP_SGI_20
		      | IEEE80211_HT_CAP_GRN_FLD
		      | IEEE80211_HT_CAP_DSSSCCK40
		      | IEEE80211_HT_CAP_LSIG_TXOP_PROT
		      | IEEE80211_HT_CAP_SGI_40 | IEEE80211_HT_CAP_SUP_WIDTH_20_40,
	.ht_cap.ampdu_factor = IEEE80211_HT_MAX_AMPDU_64K,
	.ht_cap.ampdu_density = IEEE80211_HT_MPDU_DENSITY_16,
	.ht_cap.mcs.rx_mask = {0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
	.ht_cap.mcs.rx_highest = cpu_to_le16(72),
	.ht_cap.mcs.tx_params = IEEE80211_HT_MCS_TX_DEFINED,
};

static struct ieee80211_supported_band wlan_hdd_band_5_ghz = {
	.channels = NULL,
	.n_channels = ARRAY_SIZE(hdd_channels_5_ghz),
	.band = IEEE80211_BAND_5GHZ,
	.bitrates = a_mode_rates,
	.n_bitrates = a_mode_rates_size,
	.ht_cap.ht_supported = 1,
	.ht_cap.cap = IEEE80211_HT_CAP_SGI_20
		      | IEEE80211_HT_CAP_GRN_FLD
		      | IEEE80211_HT_CAP_DSSSCCK40
		      | IEEE80211_HT_CAP_LSIG_TXOP_PROT
		      | IEEE80211_HT_CAP_SGI_40 | IEEE80211_HT_CAP_SUP_WIDTH_20_40,
	.ht_cap.ampdu_factor = IEEE80211_HT_MAX_AMPDU_64K,
	.ht_cap.ampdu_density = IEEE80211_HT_MPDU_DENSITY_16,
	.ht_cap.mcs.rx_mask = {0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
	.ht_cap.mcs.rx_highest = cpu_to_le16(72),
	.ht_cap.mcs.tx_params = IEEE80211_HT_MCS_TX_DEFINED,
	.vht_cap.vht_supported = 1,
};

/* This structure contain information what kind of frame are expected in
     TX/RX direction for each kind of interface */
static const struct ieee80211_txrx_stypes
	wlan_hdd_txrx_stypes[NUM_NL80211_IFTYPES] = {
	[NL80211_IFTYPE_STATION] = {
		.tx = 0xffff,
		.rx = BIT(SIR_MAC_MGMT_ACTION) |
		      BIT(SIR_MAC_MGMT_PROBE_REQ),
	},
	[NL80211_IFTYPE_AP] = {
		.tx = 0xffff,
		.rx = BIT(SIR_MAC_MGMT_ASSOC_REQ) |
		      BIT(SIR_MAC_MGMT_REASSOC_REQ) |
		      BIT(SIR_MAC_MGMT_PROBE_REQ) |
		      BIT(SIR_MAC_MGMT_DISASSOC) |
		      BIT(SIR_MAC_MGMT_AUTH) |
		      BIT(SIR_MAC_MGMT_DEAUTH) |
		      BIT(SIR_MAC_MGMT_ACTION),
	},
	[NL80211_IFTYPE_ADHOC] = {
		.tx = 0xffff,
		.rx = BIT(SIR_MAC_MGMT_ASSOC_REQ) |
		      BIT(SIR_MAC_MGMT_REASSOC_REQ) |
		      BIT(SIR_MAC_MGMT_PROBE_REQ) |
		      BIT(SIR_MAC_MGMT_DISASSOC) |
		      BIT(SIR_MAC_MGMT_AUTH) |
		      BIT(SIR_MAC_MGMT_DEAUTH) |
		      BIT(SIR_MAC_MGMT_ACTION),
	},
	[NL80211_IFTYPE_P2P_CLIENT] = {
		.tx = 0xffff,
		.rx = BIT(SIR_MAC_MGMT_ACTION) |
		      BIT(SIR_MAC_MGMT_PROBE_REQ),
	},
	[NL80211_IFTYPE_P2P_GO] = {
		/* This is also same as for SoftAP */
		.tx = 0xffff,
		.rx = BIT(SIR_MAC_MGMT_ASSOC_REQ) |
		      BIT(SIR_MAC_MGMT_REASSOC_REQ) |
		      BIT(SIR_MAC_MGMT_PROBE_REQ) |
		      BIT(SIR_MAC_MGMT_DISASSOC) |
		      BIT(SIR_MAC_MGMT_AUTH) |
		      BIT(SIR_MAC_MGMT_DEAUTH) |
		      BIT(SIR_MAC_MGMT_ACTION),
	},
};

/* Interface limits and combinations registered by the driver */

/* STA ( + STA ) combination */
static const struct ieee80211_iface_limit
	wlan_hdd_sta_iface_limit[] = {
	{
		.max = 3,       /* p2p0 is a STA as well */
		.types = BIT(NL80211_IFTYPE_STATION),
	},
};

#ifndef QCA_WIFI_3_0_EMU
/* ADHOC (IBSS) limit */
static const struct ieee80211_iface_limit
	wlan_hdd_adhoc_iface_limit[] = {
	{
		.max = 1,
		.types = BIT(NL80211_IFTYPE_STATION),
	},
	{
		.max = 1,
		.types = BIT(NL80211_IFTYPE_ADHOC),
	},
};
#else
/* ADHOC (IBSS) limit */
static const struct ieee80211_iface_limit
	wlan_hdd_adhoc_iface_limit[] = {
	{
		.max = 1,
		.types = BIT(NL80211_IFTYPE_STATION) | BIT(NL80211_IFTYPE_AP),
	},
	{
		.max = 1,
		.types = BIT(NL80211_IFTYPE_ADHOC),
	},
};
#endif

/* AP ( + AP ) combination */
static const struct ieee80211_iface_limit
	wlan_hdd_ap_iface_limit[] = {
	{
		.max = (QDF_MAX_NO_OF_SAP_MODE + SAP_MAX_OBSS_STA_CNT),
		.types = BIT(NL80211_IFTYPE_AP),
	},
};

/* P2P limit */
static const struct ieee80211_iface_limit
	wlan_hdd_p2p_iface_limit[] = {
	{
		.max = 1,
		.types = BIT(NL80211_IFTYPE_P2P_CLIENT),
	},
	{
		.max = 1,
		.types = BIT(NL80211_IFTYPE_P2P_GO),
	},
};

static const struct ieee80211_iface_limit
	wlan_hdd_sta_ap_iface_limit[] = {
	{
		/* We need 1 extra STA interface for OBSS scan when SAP starts
		 * with HT40 in STA+SAP concurrency mode
		 */
		.max = (1 + SAP_MAX_OBSS_STA_CNT),
		.types = BIT(NL80211_IFTYPE_STATION),
	},
	{
		.max = QDF_MAX_NO_OF_SAP_MODE,
		.types = BIT(NL80211_IFTYPE_AP),
	},
};

/* STA + P2P combination */
static const struct ieee80211_iface_limit
	wlan_hdd_sta_p2p_iface_limit[] = {
	{
		/* One reserved for dedicated P2PDEV usage */
		.max = 2,
		.types = BIT(NL80211_IFTYPE_STATION)
	},
	{
		/* Support for two identical (GO + GO or CLI + CLI)
		 * or dissimilar (GO + CLI) P2P interfaces
		 */
		.max = 2,
		.types = BIT(NL80211_IFTYPE_P2P_GO) | BIT(NL80211_IFTYPE_P2P_CLIENT),
	},
};

/* STA + AP + P2PGO combination */
static const struct ieee80211_iface_limit
wlan_hdd_sta_ap_p2pgo_iface_limit[] = {
	/* Support for AP+P2PGO interfaces */
	{
	   .max = 2,
	   .types = BIT(NL80211_IFTYPE_STATION)
	},
	{
	   .max = 1,
	   .types = BIT(NL80211_IFTYPE_P2P_GO)
	},
	{
	   .max = 1,
	   .types = BIT(NL80211_IFTYPE_AP)
	}
};

/* SAP + P2P combination */
static const struct ieee80211_iface_limit
wlan_hdd_sap_p2p_iface_limit[] = {
	{
	   /* 1 dedicated for p2p0 which is a STA type */
	   .max = 1,
	   .types = BIT(NL80211_IFTYPE_STATION)
	},
	{
	   /* The p2p interface in SAP+P2P can be GO/CLI.
	    * The p2p connection can be formed on p2p0 or p2p-p2p0-x.
	    */
	   .max = 1,
	   .types = BIT(NL80211_IFTYPE_P2P_GO) | BIT(NL80211_IFTYPE_P2P_CLIENT)
	},
	{
	   /* SAP+GO to support only one SAP interface */
	   .max = 1,
	   .types = BIT(NL80211_IFTYPE_AP)
	}
};

/* P2P + P2P combination */
static const struct ieee80211_iface_limit
wlan_hdd_p2p_p2p_iface_limit[] = {
	{
	   /* 1 dedicated for p2p0 which is a STA type */
	   .max = 1,
	   .types = BIT(NL80211_IFTYPE_STATION)
	},
	{
	   /* The p2p interface in P2P+P2P can be GO/CLI.
	    * For P2P+P2P, the new interfaces are formed on p2p-p2p0-x.
	    */
	   .max = 2,
	   .types = BIT(NL80211_IFTYPE_P2P_GO) | BIT(NL80211_IFTYPE_P2P_CLIENT)
	},
};

static const struct ieee80211_iface_limit
	wlan_hdd_mon_iface_limit[] = {
	{
		.max = 3,       /* Monitor interface */
		.types = BIT(NL80211_IFTYPE_MONITOR),
	},
};

static struct ieee80211_iface_combination
	wlan_hdd_mon_iface[] = {
	{
		.limits = wlan_hdd_mon_iface_limit,
		.max_interfaces = 3,
		.num_different_channels = 2,
		.n_limits = ARRAY_SIZE(wlan_hdd_mon_iface_limit),
	},
};

static struct ieee80211_iface_combination
	wlan_hdd_iface_combination[] = {
	/* STA */
	{
		.limits = wlan_hdd_sta_iface_limit,
		.num_different_channels = 2,
		.max_interfaces = 3,
		.n_limits = ARRAY_SIZE(wlan_hdd_sta_iface_limit),
	},
	/* ADHOC */
	{
		.limits = wlan_hdd_adhoc_iface_limit,
		.num_different_channels = 2,
		.max_interfaces = 2,
		.n_limits = ARRAY_SIZE(wlan_hdd_adhoc_iface_limit),
	},
	/* AP */
	{
		.limits = wlan_hdd_ap_iface_limit,
		.num_different_channels = 2,
		.max_interfaces = (SAP_MAX_OBSS_STA_CNT + QDF_MAX_NO_OF_SAP_MODE),
		.n_limits = ARRAY_SIZE(wlan_hdd_ap_iface_limit),
	},
	/* P2P */
	{
		.limits = wlan_hdd_p2p_iface_limit,
		.num_different_channels = 2,
		.max_interfaces = 2,
		.n_limits = ARRAY_SIZE(wlan_hdd_p2p_iface_limit),
	},
	/* STA + AP */
	{
		.limits = wlan_hdd_sta_ap_iface_limit,
		.num_different_channels = 2,
		.max_interfaces = (1 + SAP_MAX_OBSS_STA_CNT + QDF_MAX_NO_OF_SAP_MODE),
		.n_limits = ARRAY_SIZE(wlan_hdd_sta_ap_iface_limit),
		.beacon_int_infra_match = true,
	},
	/* STA + P2P */
	{
		.limits = wlan_hdd_sta_p2p_iface_limit,
		.num_different_channels = 2,
		/* one interface reserved for P2PDEV dedicated usage */
		.max_interfaces = 4,
		.n_limits = ARRAY_SIZE(wlan_hdd_sta_p2p_iface_limit),
		.beacon_int_infra_match = true,
	},
	/* STA + P2P GO + SAP */
	{
		.limits = wlan_hdd_sta_ap_p2pgo_iface_limit,
		/* we can allow 3 channels for three different persona
		 * but due to firmware limitation, allow max 2 concrnt channels.
		 */
		.num_different_channels = 2,
		/* one interface reserved for P2PDEV dedicated usage */
		.max_interfaces = 4,
		.n_limits = ARRAY_SIZE(wlan_hdd_sta_ap_p2pgo_iface_limit),
		.beacon_int_infra_match = true,
	},
	/* SAP + P2P */
	{
		.limits = wlan_hdd_sap_p2p_iface_limit,
		.num_different_channels = 2,
		/* 1-p2p0 + 1-SAP + 1-P2P (on p2p0 or p2p-p2p0-x) */
		.max_interfaces = 3,
		.n_limits = ARRAY_SIZE(wlan_hdd_sap_p2p_iface_limit),
		.beacon_int_infra_match = true,
	},
	/* P2P + P2P */
	{
		.limits = wlan_hdd_p2p_p2p_iface_limit,
		.num_different_channels = 2,
		/* 1-p2p0 + 2-P2P (on p2p-p2p0-x) */
		.max_interfaces = 3,
		.n_limits = ARRAY_SIZE(wlan_hdd_p2p_p2p_iface_limit),
		.beacon_int_infra_match = true,
	},
};

static struct cfg80211_ops wlan_hdd_cfg80211_ops;
struct hdd_bpf_context bpf_context;

#ifdef WLAN_NL80211_TESTMODE
enum wlan_hdd_tm_attr {
	WLAN_HDD_TM_ATTR_INVALID = 0,
	WLAN_HDD_TM_ATTR_CMD = 1,
	WLAN_HDD_TM_ATTR_DATA = 2,
	WLAN_HDD_TM_ATTR_STREAM_ID = 3,
	WLAN_HDD_TM_ATTR_TYPE = 4,
	/* keep last */
	WLAN_HDD_TM_ATTR_AFTER_LAST,
	WLAN_HDD_TM_ATTR_MAX = WLAN_HDD_TM_ATTR_AFTER_LAST - 1,
};

enum wlan_hdd_tm_cmd {
	WLAN_HDD_TM_CMD_WLAN_FTM = 0,
	WLAN_HDD_TM_CMD_WLAN_HB = 1,
};

#define WLAN_HDD_TM_DATA_MAX_LEN    5000

static const struct nla_policy wlan_hdd_tm_policy[WLAN_HDD_TM_ATTR_MAX + 1] = {
	[WLAN_HDD_TM_ATTR_CMD] = {.type = NLA_U32},
	[WLAN_HDD_TM_ATTR_DATA] = {.type = NLA_BINARY,
				   .len = WLAN_HDD_TM_DATA_MAX_LEN},
};
#endif /* WLAN_NL80211_TESTMODE */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0))
static const struct wiphy_wowlan_support wowlan_support_cfg80211_init = {
	.flags = WIPHY_WOWLAN_MAGIC_PKT,
	.n_patterns = WOWL_MAX_PTRNS_ALLOWED,
	.pattern_min_len = 1,
	.pattern_max_len = WOWL_PTRN_MAX_SIZE,
};
#endif

/**
 * hdd_add_channel_switch_support()- Adds Channel Switch flag if supported
 * @flags: Pointer to the flags to Add channel switch flag.
 *
 * This Function adds Channel Switch support flag, if channel switch is
 * supported by kernel.
 * Return: void.
 */
#ifdef CHANNEL_SWITCH_SUPPORTED
static inline void hdd_add_channel_switch_support(uint32_t *flags)
{
	*flags |= WIPHY_FLAG_HAS_CHANNEL_SWITCH;
	return;
}
#else
static inline void hdd_add_channel_switch_support(uint32_t *flags)
{
	return;
}
#endif

#ifdef FEATURE_WLAN_TDLS

/* TDLS capabilities params */
#define PARAM_MAX_TDLS_SESSION \
		QCA_WLAN_VENDOR_ATTR_TDLS_GET_CAPS_MAX_CONC_SESSIONS
#define PARAM_TDLS_FEATURE_SUPPORT \
		QCA_WLAN_VENDOR_ATTR_TDLS_GET_CAPS_FEATURES_SUPPORTED

/**
 * __wlan_hdd_cfg80211_get_tdls_capabilities() - Provide TDLS Capabilites.
 * @wiphy:    WIPHY structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of the data received
 *
 * This function provides TDLS capabilities
 *
 * Return: 0 on success and errno on failure
 */
static int __wlan_hdd_cfg80211_get_tdls_capabilities(struct wiphy *wiphy,
						     struct wireless_dev *wdev,
						     const void *data,
						     int data_len)
{
	int status;
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	struct sk_buff *skb;
	uint32_t set = 0;

	ENTER_DEV(wdev->netdev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	status = wlan_hdd_validate_context(hdd_ctx);
	if (status)
		return status;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, (2 * sizeof(u32)) +
						   NLMSG_HDRLEN);
	if (!skb) {
		hdd_err("cfg80211_vendor_cmd_alloc_reply_skb failed");
		goto fail;
	}

	if (false == hdd_ctx->config->fEnableTDLSSupport) {
		hdd_err("TDLS feature not Enabled or Not supported in FW");
		if (nla_put_u32(skb, PARAM_MAX_TDLS_SESSION, 0) ||
			nla_put_u32(skb, PARAM_TDLS_FEATURE_SUPPORT, 0)) {
			hdd_err("nla put fail");
			goto fail;
		}
	} else {
		set = set | WIFI_TDLS_SUPPORT;
		set = set | (hdd_ctx->config->fTDLSExternalControl ?
					WIFI_TDLS_EXTERNAL_CONTROL_SUPPORT : 0);
		set = set | (hdd_ctx->config->fEnableTDLSOffChannel ?
					WIIF_TDLS_OFFCHANNEL_SUPPORT : 0);
		hdd_notice("TDLS Feature supported value %x", set);
		if (nla_put_u32(skb, PARAM_MAX_TDLS_SESSION,
				 hdd_ctx->max_num_tdls_sta) ||
			nla_put_u32(skb, PARAM_TDLS_FEATURE_SUPPORT,
				 set)) {
			hdd_err("nla put fail");
			goto fail;
		}
	}
	return cfg80211_vendor_cmd_reply(skb);
fail:
	if (skb)
		kfree_skb(skb);
	return -EINVAL;
}

/**
 * wlan_hdd_cfg80211_get_tdls_capabilities() - Provide TDLS Capabilites.
 * @wiphy:    WIPHY structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of the data received
 *
 * This function provides TDLS capabilities
 *
 * Return: 0 on success and errno on failure
 */
static int
wlan_hdd_cfg80211_get_tdls_capabilities(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data,
					int data_len)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_get_tdls_capabilities(wiphy, wdev,
							data, data_len);
	cds_ssr_unprotect(__func__);

	return ret;
}
#endif

#ifdef QCA_HT_2040_COEX
static void wlan_hdd_cfg80211_start_pending_acs(struct work_struct *work);
#endif

#if defined(FEATURE_WLAN_CH_AVOID) || defined(FEATURE_WLAN_FORCE_SAP_SCC)
/*
 * FUNCTION: wlan_hdd_send_avoid_freq_event
 * This is called when wlan driver needs to send vendor specific
 * avoid frequency range event to userspace
 */
int wlan_hdd_send_avoid_freq_event(hdd_context_t *pHddCtx,
				   tHddAvoidFreqList *pAvoidFreqList)
{
	struct sk_buff *vendor_event;

	ENTER();

	if (!pHddCtx) {
		hdd_err("HDD context is null");
		return -EINVAL;
	}

	if (!pAvoidFreqList) {
		hdd_err("pAvoidFreqList is null");
		return -EINVAL;
	}

	vendor_event = cfg80211_vendor_event_alloc(pHddCtx->wiphy,
						   NULL,
						   sizeof(tHddAvoidFreqList),
						   QCA_NL80211_VENDOR_SUBCMD_AVOID_FREQUENCY_INDEX,
						   GFP_KERNEL);
	if (!vendor_event) {
		hdd_err("cfg80211_vendor_event_alloc failed");
		return -EINVAL;
	}

	memcpy(skb_put(vendor_event, sizeof(tHddAvoidFreqList)),
	       (void *)pAvoidFreqList, sizeof(tHddAvoidFreqList));

	cfg80211_vendor_event(vendor_event, GFP_KERNEL);

	EXIT();
	return 0;
}
#endif /* FEATURE_WLAN_CH_AVOID || FEATURE_WLAN_FORCE_SAP_SCC */

/* vendor specific events */
static const struct nl80211_vendor_cmd_info wlan_hdd_cfg80211_vendor_events[] = {
#ifdef FEATURE_WLAN_CH_AVOID
	[QCA_NL80211_VENDOR_SUBCMD_AVOID_FREQUENCY_INDEX] = {
		.vendor_id =
			QCA_NL80211_VENDOR_ID,
		.subcmd =
			QCA_NL80211_VENDOR_SUBCMD_AVOID_FREQUENCY
	},
#endif /* FEATURE_WLAN_CH_AVOID */

#ifdef WLAN_FEATURE_NAN
	[QCA_NL80211_VENDOR_SUBCMD_NAN_INDEX] = {
		.vendor_id =
			QCA_NL80211_VENDOR_ID,
		.subcmd =
			QCA_NL80211_VENDOR_SUBCMD_NAN
	},
#endif

#ifdef WLAN_FEATURE_STATS_EXT
	[QCA_NL80211_VENDOR_SUBCMD_STATS_EXT_INDEX] = {
		.vendor_id =
			QCA_NL80211_VENDOR_ID,
		.subcmd =
			QCA_NL80211_VENDOR_SUBCMD_STATS_EXT
	},
#endif /* WLAN_FEATURE_STATS_EXT */
#ifdef FEATURE_WLAN_EXTSCAN
	[QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_START_INDEX] = {
		.vendor_id =
			QCA_NL80211_VENDOR_ID,
		.subcmd =
			QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_START
	},
	[QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_STOP_INDEX] = {
		.vendor_id =
			QCA_NL80211_VENDOR_ID,
		.subcmd =
			QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_STOP
	},
	[QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_GET_CAPABILITIES_INDEX] = {
		.
		vendor_id
			=
				QCA_NL80211_VENDOR_ID,
		.subcmd =
			QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_GET_CAPABILITIES
	},
	[QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_GET_CACHED_RESULTS_INDEX] = {
		.
		vendor_id
			=
				QCA_NL80211_VENDOR_ID,
		.
		subcmd =
			QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_GET_CACHED_RESULTS
	},
	[QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SCAN_RESULTS_AVAILABLE_INDEX] = {
		.
		vendor_id
			=
				QCA_NL80211_VENDOR_ID,
		.
		subcmd
			=
				QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SCAN_RESULTS_AVAILABLE
	},
	[QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_FULL_SCAN_RESULT_INDEX] = {
		.
		vendor_id
			=
				QCA_NL80211_VENDOR_ID,
		.subcmd =
			QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_FULL_SCAN_RESULT
	},
	[QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SCAN_EVENT_INDEX] = {
		.vendor_id =
			QCA_NL80211_VENDOR_ID,
		.subcmd =
			QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SCAN_EVENT
	},
	[QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_HOTLIST_AP_FOUND_INDEX] = {
		.
		vendor_id
			=
				QCA_NL80211_VENDOR_ID,
		.subcmd =
			QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_HOTLIST_AP_FOUND
	},
	[QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SET_BSSID_HOTLIST_INDEX] = {
		.
		vendor_id
			=
				QCA_NL80211_VENDOR_ID,
		.subcmd =
			QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SET_BSSID_HOTLIST
	},
	[QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_RESET_BSSID_HOTLIST_INDEX] = {
		.
		vendor_id
			=
				QCA_NL80211_VENDOR_ID,
		.
		subcmd
			=
				QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_RESET_BSSID_HOTLIST
	},
	[QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SIGNIFICANT_CHANGE_INDEX] = {
		.
		vendor_id
			=
				QCA_NL80211_VENDOR_ID,
		.
		subcmd =
			QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SIGNIFICANT_CHANGE
	},
	[QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SET_SIGNIFICANT_CHANGE_INDEX] = {
		.
		vendor_id
			=
				QCA_NL80211_VENDOR_ID,
		.
		subcmd
			=
				QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SET_SIGNIFICANT_CHANGE
	},
	[QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_RESET_SIGNIFICANT_CHANGE_INDEX] = {
		.
		vendor_id
			=
				QCA_NL80211_VENDOR_ID,
		.
		subcmd
			=
				QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_RESET_SIGNIFICANT_CHANGE
	},
	[QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_HOTLIST_SSID_FOUND_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_HOTLIST_SSID_FOUND
	},
	[QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_HOTLIST_SSID_LOST_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_HOTLIST_SSID_LOST
	},
#endif /* FEATURE_WLAN_EXTSCAN */

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
	[QCA_NL80211_VENDOR_SUBCMD_LL_STATS_SET_INDEX] = {
		.vendor_id =
			QCA_NL80211_VENDOR_ID,
		.subcmd =
			QCA_NL80211_VENDOR_SUBCMD_LL_STATS_SET
	},
	[QCA_NL80211_VENDOR_SUBCMD_LL_STATS_GET_INDEX] = {
		.vendor_id =
			QCA_NL80211_VENDOR_ID,
		.subcmd =
			QCA_NL80211_VENDOR_SUBCMD_LL_STATS_GET
	},
	[QCA_NL80211_VENDOR_SUBCMD_LL_STATS_CLR_INDEX] = {
		.vendor_id =
			QCA_NL80211_VENDOR_ID,
		.subcmd =
			QCA_NL80211_VENDOR_SUBCMD_LL_STATS_CLR
	},
	[QCA_NL80211_VENDOR_SUBCMD_LL_RADIO_STATS_INDEX] = {
		.vendor_id =
			QCA_NL80211_VENDOR_ID,
		.subcmd =
			QCA_NL80211_VENDOR_SUBCMD_LL_STATS_RADIO_RESULTS
	},
	[QCA_NL80211_VENDOR_SUBCMD_LL_IFACE_STATS_INDEX] = {
		.vendor_id =
			QCA_NL80211_VENDOR_ID,
		.subcmd =
			QCA_NL80211_VENDOR_SUBCMD_LL_STATS_IFACE_RESULTS
	},
	[QCA_NL80211_VENDOR_SUBCMD_LL_PEER_INFO_STATS_INDEX] = {
		.vendor_id =
			QCA_NL80211_VENDOR_ID,
		.subcmd =
			QCA_NL80211_VENDOR_SUBCMD_LL_STATS_PEERS_RESULTS
	},
#endif /* WLAN_FEATURE_LINK_LAYER_STATS */
	[QCA_NL80211_VENDOR_SUBCMD_TDLS_STATE_CHANGE_INDEX] = {
		.vendor_id =
			QCA_NL80211_VENDOR_ID,
		.subcmd =
			QCA_NL80211_VENDOR_SUBCMD_TDLS_STATE
	},
	[QCA_NL80211_VENDOR_SUBCMD_DO_ACS_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_DO_ACS
	},
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	[QCA_NL80211_VENDOR_SUBCMD_KEY_MGMT_ROAM_AUTH_INDEX] = {
		.vendor_id =
			QCA_NL80211_VENDOR_ID,
		.subcmd =
			QCA_NL80211_VENDOR_SUBCMD_KEY_MGMT_ROAM_AUTH
	},
#endif
	[QCA_NL80211_VENDOR_SUBCMD_DFS_OFFLOAD_CAC_STARTED_INDEX] =  {
		.vendor_id =
			QCA_NL80211_VENDOR_ID,
		.subcmd =
			QCA_NL80211_VENDOR_SUBCMD_DFS_OFFLOAD_CAC_STARTED
	},
	[QCA_NL80211_VENDOR_SUBCMD_DFS_OFFLOAD_CAC_FINISHED_INDEX] =  {
		.vendor_id =
			QCA_NL80211_VENDOR_ID,
		.subcmd =
			QCA_NL80211_VENDOR_SUBCMD_DFS_OFFLOAD_CAC_FINISHED
	},
	[QCA_NL80211_VENDOR_SUBCMD_DFS_OFFLOAD_CAC_ABORTED_INDEX] =  {
		.vendor_id =
			QCA_NL80211_VENDOR_ID,
		.subcmd =
			QCA_NL80211_VENDOR_SUBCMD_DFS_OFFLOAD_CAC_ABORTED
	},
	[QCA_NL80211_VENDOR_SUBCMD_DFS_OFFLOAD_CAC_NOP_FINISHED_INDEX] =  {
		.vendor_id =
			QCA_NL80211_VENDOR_ID,
		.subcmd =
			QCA_NL80211_VENDOR_SUBCMD_DFS_OFFLOAD_CAC_NOP_FINISHED
	},
	[QCA_NL80211_VENDOR_SUBCMD_DFS_OFFLOAD_RADAR_DETECTED_INDEX] =  {
		.vendor_id =
			QCA_NL80211_VENDOR_ID,
		.subcmd =
			QCA_NL80211_VENDOR_SUBCMD_DFS_OFFLOAD_RADAR_DETECTED
	},
#ifdef FEATURE_WLAN_EXTSCAN
	[QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_PNO_NETWORK_FOUND_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_PNO_NETWORK_FOUND
	},
	[QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_PNO_PASSPOINT_NETWORK_FOUND_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_PNO_PASSPOINT_NETWORK_FOUND
	},
	[QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SET_SSID_HOTLIST_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SET_SSID_HOTLIST
	},
	[QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_RESET_SSID_HOTLIST_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_RESET_SSID_HOTLIST
	},
	[QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_HOTLIST_AP_LOST_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_HOTLIST_AP_LOST
	},
#endif /* FEATURE_WLAN_EXTSCAN */
	[QCA_NL80211_VENDOR_SUBCMD_MONITOR_RSSI_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_MONITOR_RSSI
	},
#ifdef WLAN_FEATURE_MEMDUMP
	[QCA_NL80211_VENDOR_SUBCMD_WIFI_LOGGER_MEMORY_DUMP_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_WIFI_LOGGER_MEMORY_DUMP
	},
#endif /* WLAN_FEATURE_MEMDUMP */
#ifdef WLAN_FEATURE_TSF
	[QCA_NL80211_VENDOR_SUBCMD_TSF_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_TSF
	},
#endif
	[QCA_NL80211_VENDOR_SUBCMD_SCAN_DONE_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_SCAN_DONE
	},
	[QCA_NL80211_VENDOR_SUBCMD_SCAN_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_TRIGGER_SCAN
	},
	/* OCB events */
	[QCA_NL80211_VENDOR_SUBCMD_DCC_STATS_EVENT_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_DCC_STATS_EVENT
	},
#ifdef FEATURE_LFR_SUBNET_DETECTION
	[QCA_NL80211_VENDOR_SUBCMD_GW_PARAM_CONFIG_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_GW_PARAM_CONFIG
	},
#endif /*FEATURE_LFR_SUBNET_DETECTION */

#ifdef WLAN_FEATURE_NAN_DATAPATH
	[QCA_NL80211_VENDOR_SUBCMD_NDP_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_NDP
	},
#endif /* WLAN_FEATURE_NAN_DATAPATH */
};

/**
 * __is_driver_dfs_capable() - get driver DFS capability
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * This function is called by userspace to indicate whether or not
 * the driver supports DFS offload.
 *
 * Return: 0 on success, negative errno on failure
 */
static int __is_driver_dfs_capable(struct wiphy *wiphy,
				   struct wireless_dev *wdev,
				   const void *data,
				   int data_len)
{
	u32 dfs_capability = 0;
	struct sk_buff *temp_skbuff;
	int ret_val;
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);

	ENTER_DEV(wdev->netdev);

	ret_val = wlan_hdd_validate_context(hdd_ctx);
	if (ret_val)
		return ret_val;

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	dfs_capability = !!(wiphy->flags & WIPHY_FLAG_DFS_OFFLOAD);

	temp_skbuff = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, sizeof(u32) +
							  NLMSG_HDRLEN);

	if (temp_skbuff != NULL) {
		ret_val = nla_put_u32(temp_skbuff, QCA_WLAN_VENDOR_ATTR_DFS,
				      dfs_capability);
		if (ret_val) {
			hdd_err("QCA_WLAN_VENDOR_ATTR_DFS put fail");
			kfree_skb(temp_skbuff);

			return ret_val;
		}

		return cfg80211_vendor_cmd_reply(temp_skbuff);
	}

	hdd_err("dfs capability: buffer alloc fail");
	return -ENOMEM;
}

/**
 * is_driver_dfs_capable() - get driver DFS capability
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * This function is called by userspace to indicate whether or not
 * the driver supports DFS offload.  This is an SSR-protected
 * wrapper function.
 *
 * Return: 0 on success, negative errno on failure
 */
static int is_driver_dfs_capable(struct wiphy *wiphy,
				 struct wireless_dev *wdev,
				 const void *data,
				 int data_len)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __is_driver_dfs_capable(wiphy, wdev, data, data_len);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * wlan_hdd_sap_cfg_dfs_override() - DFS MCC restriction check
 *
 * @adapter: SAP adapter pointer
 *
 * DFS in MCC is not supported for Multi bssid SAP mode due to single physical
 * radio. So in case of DFS MCC scenario override current SAP given config
 * to follow concurrent SAP DFS config
 *
 * Return: 0 - No DFS issue, 1 - Override done and negative error codes
 */

#ifdef WLAN_FEATURE_MBSSID
int wlan_hdd_sap_cfg_dfs_override(hdd_adapter_t *adapter)
{
	hdd_adapter_t *con_sap_adapter;
	tsap_Config_t *sap_config, *con_sap_config;
	int con_ch;

	/*
	 * Check if AP+AP case, once primary AP chooses a DFS
	 * channel secondary AP should always follow primary APs channel
	 */
	if (!cds_concurrent_beaconing_sessions_running())
		return 0;

	con_sap_adapter = hdd_get_con_sap_adapter(adapter, true);
	if (!con_sap_adapter)
		return 0;

	sap_config = &adapter->sessionCtx.ap.sapConfig;
	con_sap_config = &con_sap_adapter->sessionCtx.ap.sapConfig;
	con_ch = con_sap_adapter->sessionCtx.ap.operatingChannel;

	if (!CDS_IS_DFS_CH(con_ch))
		return 0;

	hdd_err("Only SCC AP-AP DFS Permitted (ch=%d, con_ch=%d)",
						sap_config->channel, con_ch);
	hdd_notice("Overriding guest AP's channel");
	sap_config->channel = con_ch;

	if (con_sap_config->acs_cfg.acs_mode == true) {
		if (con_ch != con_sap_config->acs_cfg.pri_ch &&
				con_ch != con_sap_config->acs_cfg.ht_sec_ch) {
			hdd_err("Primary AP channel config error");
			hdd_err("Operating ch: %d ACS ch: %d %d",
				con_ch, con_sap_config->acs_cfg.pri_ch,
				con_sap_config->acs_cfg.ht_sec_ch);
			return -EINVAL;
		}
		/* Sec AP ACS info is overwritten with Pri AP due to DFS
		 * MCC restriction. So free ch list allocated in do_acs
		 * func for Sec AP and realloc for Pri AP ch list size
		 */
		if (sap_config->acs_cfg.ch_list)
			qdf_mem_free(sap_config->acs_cfg.ch_list);

		qdf_mem_copy(&sap_config->acs_cfg,
					&con_sap_config->acs_cfg,
					sizeof(struct sap_acs_cfg));
		sap_config->acs_cfg.ch_list = qdf_mem_malloc(
					sizeof(uint8_t) *
					con_sap_config->acs_cfg.ch_list_count);
		if (!sap_config->acs_cfg.ch_list) {
			hdd_err("ACS config alloc fail");
			return -ENOMEM;
		}

		qdf_mem_copy(sap_config->acs_cfg.ch_list,
					con_sap_config->acs_cfg.ch_list,
					con_sap_config->acs_cfg.ch_list_count);

	} else {
		sap_config->acs_cfg.pri_ch = con_ch;
		if (sap_config->acs_cfg.ch_width > eHT_CHANNEL_WIDTH_20MHZ)
			sap_config->acs_cfg.ht_sec_ch = con_sap_config->sec_ch;
	}

	return con_ch;
}
#else
int wlan_hdd_sap_cfg_dfs_override(hdd_adapter_t *adapter)
{
	return 0;
}
#endif

/**
 * wlan_hdd_set_acs_ch_range : Start ACS channel range values
 * @sap_cfg: pointer to SAP config struct
 *
 * This function sets the default ACS start and end channel for the given band
 * and also parses the given ACS channel list.
 *
 * Return: None
 */

static void wlan_hdd_set_acs_ch_range(tsap_Config_t *sap_cfg, bool ht_enabled,
							bool vht_enabled)
{
	int i;
	if (sap_cfg->acs_cfg.hw_mode == QCA_ACS_MODE_IEEE80211B) {
		sap_cfg->acs_cfg.hw_mode = eCSR_DOT11_MODE_11b;
		sap_cfg->acs_cfg.start_ch = CDS_CHANNEL_NUM(CHAN_ENUM_1);
		sap_cfg->acs_cfg.end_ch = CDS_CHANNEL_NUM(CHAN_ENUM_14);
	} else if (sap_cfg->acs_cfg.hw_mode == QCA_ACS_MODE_IEEE80211G) {
		sap_cfg->acs_cfg.hw_mode = eCSR_DOT11_MODE_11g;
		sap_cfg->acs_cfg.start_ch = CDS_CHANNEL_NUM(CHAN_ENUM_1);
		sap_cfg->acs_cfg.end_ch = CDS_CHANNEL_NUM(CHAN_ENUM_13);
	} else if (sap_cfg->acs_cfg.hw_mode == QCA_ACS_MODE_IEEE80211A) {
		sap_cfg->acs_cfg.hw_mode = eCSR_DOT11_MODE_11a;
		sap_cfg->acs_cfg.start_ch = CDS_CHANNEL_NUM(CHAN_ENUM_36);
		sap_cfg->acs_cfg.end_ch = CDS_CHANNEL_NUM(CHAN_ENUM_165);
	} else if (sap_cfg->acs_cfg.hw_mode == QCA_ACS_MODE_IEEE80211ANY) {
		sap_cfg->acs_cfg.hw_mode = eCSR_DOT11_MODE_abg;
		sap_cfg->acs_cfg.start_ch = CDS_CHANNEL_NUM(CHAN_ENUM_1);
		sap_cfg->acs_cfg.end_ch = CDS_CHANNEL_NUM(CHAN_ENUM_165);
	}

	if (ht_enabled)
		sap_cfg->acs_cfg.hw_mode = eCSR_DOT11_MODE_11n;

	if (vht_enabled)
		sap_cfg->acs_cfg.hw_mode = eCSR_DOT11_MODE_11ac;


	/* Parse ACS Chan list from hostapd */
	if (!sap_cfg->acs_cfg.ch_list)
		return;

	sap_cfg->acs_cfg.start_ch = sap_cfg->acs_cfg.ch_list[0];
	sap_cfg->acs_cfg.end_ch =
		sap_cfg->acs_cfg.ch_list[sap_cfg->acs_cfg.ch_list_count - 1];
	for (i = 0; i < sap_cfg->acs_cfg.ch_list_count; i++) {
		if (sap_cfg->acs_cfg.start_ch > sap_cfg->acs_cfg.ch_list[i])
			sap_cfg->acs_cfg.start_ch = sap_cfg->acs_cfg.ch_list[i];
		if (sap_cfg->acs_cfg.end_ch < sap_cfg->acs_cfg.ch_list[i])
			sap_cfg->acs_cfg.end_ch = sap_cfg->acs_cfg.ch_list[i];
	}
}


static void wlan_hdd_cfg80211_start_pending_acs(struct work_struct *work);

/**
 * wlan_hdd_cfg80211_start_acs : Start ACS Procedure for SAP
 * @adapter: pointer to SAP adapter struct
 *
 * This function starts the ACS procedure if there are no
 * constraints like MBSSID DFS restrictions.
 *
 * Return: Status of ACS Start procedure
 */

static int wlan_hdd_cfg80211_start_acs(hdd_adapter_t *adapter)
{

	hdd_context_t *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	tsap_Config_t *sap_config;
	tpWLAN_SAPEventCB acs_event_callback;
	int status;

	sap_config = &adapter->sessionCtx.ap.sapConfig;
	sap_config->channel = AUTO_CHANNEL_SELECT;

	status = wlan_hdd_sap_cfg_dfs_override(adapter);
	if (status < 0) {
		return status;
	} else {
		if (status > 0) {
			/*notify hostapd about channel override */
			wlan_hdd_cfg80211_acs_ch_select_evt(adapter);
			clear_bit(ACS_IN_PROGRESS, &hdd_ctx->g_event_flags);
			return 0;
		}
	}
	status = wlan_hdd_config_acs(hdd_ctx, adapter);
	if (status) {
		hdd_err("ACS config failed");
		return -EINVAL;
	}

	acs_event_callback = hdd_hostapd_sap_event_cb;

	qdf_mem_copy(sap_config->self_macaddr.bytes,
		adapter->macAddressCurrent.bytes, sizeof(struct qdf_mac_addr));
	hdd_notice("ACS Started for wlan%d", adapter->dev->ifindex);
	status = wlansap_acs_chselect(
#ifdef WLAN_FEATURE_MBSSID
		WLAN_HDD_GET_SAP_CTX_PTR(adapter),
#else
		hdd_ctx->pcds_context,
#endif
		acs_event_callback, sap_config, adapter->dev);


	if (status) {
		hdd_err("ACS channel select failed");
		return -EINVAL;
	}
	sap_config->acs_cfg.acs_mode = true;
	set_bit(ACS_IN_PROGRESS, &hdd_ctx->g_event_flags);

	return 0;
}

/**
 * __wlan_hdd_cfg80211_do_acs : CFG80211 handler function for DO_ACS Vendor CMD
 * @wiphy:  Linux wiphy struct pointer
 * @wdev:   Linux wireless device struct pointer
 * @data:   ACS information from hostapd
 * @data_len: ACS information length
 *
 * This function handle DO_ACS Vendor command from hostapd, parses ACS config
 * and starts ACS procedure.
 *
 * Return: ACS procedure start status
 */

static int __wlan_hdd_cfg80211_do_acs(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data, int data_len)
{
	struct net_device *ndev = wdev->netdev;
	hdd_adapter_t *adapter = WLAN_HDD_GET_PRIV_PTR(ndev);
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	tsap_Config_t *sap_config;
	struct sk_buff *temp_skbuff;
	int status = -EINVAL, i = 0;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_ACS_MAX + 1];
	bool ht_enabled, ht40_enabled, vht_enabled;
	uint8_t ch_width;
	uint8_t weight_list[QDF_MAX_NUM_CHAN];

	/* ***Note*** Donot set SME config related to ACS operation here because
	 * ACS operation is not synchronouse and ACS for Second AP may come when
	 * ACS operation for first AP is going on. So only do_acs is split to
	 * seperate start_acs routine. Also SME-PMAC struct that is used to
	 * pass paremeters from HDD to SAP is global. Thus All ACS related SME
	 * config shall be set only from start_acs.
	 */

	/* nla_policy Policy template. Policy not applied as some attributes are
	 * optional and QCA_WLAN_VENDOR_ATTR_ACS_CH_LIST has variable length
	 *
	 * [QCA_WLAN_VENDOR_ATTR_ACS_HW_MODE] = { .type = NLA_U8 },
	 * [QCA_WLAN_VENDOR_ATTR_ACS_HT_ENABLED] = { .type = NLA_FLAG },
	 * [QCA_WLAN_VENDOR_ATTR_ACS_HT40_ENABLED] = { .type = NLA_FLAG },
	 * [QCA_WLAN_VENDOR_ATTR_ACS_VHT_ENABLED] = { .type = NLA_FLAG },
	 * [QCA_WLAN_VENDOR_ATTR_ACS_CHWIDTH] = { .type = NLA_U16 },
	 * [QCA_WLAN_VENDOR_ATTR_ACS_CH_LIST] = { .type = NLA_NESTED },
	 */

	ENTER_DEV(ndev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	if (hdd_ctx->config->force_sap_acs) {
		hdd_err("Hostapd ACS rejected as Driver ACS enabled");
		return -EPERM;
	}

	status = wlan_hdd_validate_context(hdd_ctx);
	if (status)
		goto out;

	sap_config = &adapter->sessionCtx.ap.sapConfig;
	qdf_mem_zero(&sap_config->acs_cfg, sizeof(struct sap_acs_cfg));

	status = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_ACS_MAX, data, data_len,
						NULL);
	if (status) {
		hdd_err("Invalid ATTR");
		goto out;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_ACS_HW_MODE]) {
		hdd_err("Attr hw_mode failed");
		goto out;
	}
	sap_config->acs_cfg.hw_mode = nla_get_u8(
					tb[QCA_WLAN_VENDOR_ATTR_ACS_HW_MODE]);

	if (tb[QCA_WLAN_VENDOR_ATTR_ACS_HT_ENABLED])
		ht_enabled =
			nla_get_flag(tb[QCA_WLAN_VENDOR_ATTR_ACS_HT_ENABLED]);
	else
		ht_enabled = 0;

	if (tb[QCA_WLAN_VENDOR_ATTR_ACS_HT40_ENABLED])
		ht40_enabled =
			nla_get_flag(tb[QCA_WLAN_VENDOR_ATTR_ACS_HT40_ENABLED]);
	else
		ht40_enabled = 0;

	if (tb[QCA_WLAN_VENDOR_ATTR_ACS_VHT_ENABLED])
		vht_enabled =
			nla_get_flag(tb[QCA_WLAN_VENDOR_ATTR_ACS_VHT_ENABLED]);
	else
		vht_enabled = 0;

	if (tb[QCA_WLAN_VENDOR_ATTR_ACS_CHWIDTH]) {
		ch_width = nla_get_u16(tb[QCA_WLAN_VENDOR_ATTR_ACS_CHWIDTH]);
	} else {
		if (ht_enabled && ht40_enabled)
			ch_width = 40;
		else
			ch_width = 20;
	}
	if (ch_width == 80)
		sap_config->acs_cfg.ch_width = CH_WIDTH_80MHZ;
	else if (ch_width == 40)
		sap_config->acs_cfg.ch_width = CH_WIDTH_40MHZ;
	else
		sap_config->acs_cfg.ch_width = CH_WIDTH_20MHZ;

	/* hw_mode = a/b/g: QCA_WLAN_VENDOR_ATTR_ACS_CH_LIST and
	 * QCA_WLAN_VENDOR_ATTR_ACS_FREQ_LIST attrs are present, and
	 * QCA_WLAN_VENDOR_ATTR_ACS_CH_LIST is used for obtaining the
	 * channel list, QCA_WLAN_VENDOR_ATTR_ACS_FREQ_LIST is ignored
	 * since it contains the frequency values of the channels in
	 * the channel list.
	 * hw_mode = any: only QCA_WLAN_VENDOR_ATTR_ACS_FREQ_LIST attr
	 * is present
	 */
	if (tb[QCA_WLAN_VENDOR_ATTR_ACS_CH_LIST]) {
		char *tmp = nla_data(tb[QCA_WLAN_VENDOR_ATTR_ACS_CH_LIST]);
		sap_config->acs_cfg.ch_list_count = nla_len(
					tb[QCA_WLAN_VENDOR_ATTR_ACS_CH_LIST]);
		if (sap_config->acs_cfg.ch_list_count) {
			sap_config->acs_cfg.ch_list = qdf_mem_malloc(
					sizeof(uint8_t) *
					sap_config->acs_cfg.ch_list_count);
			if (sap_config->acs_cfg.ch_list == NULL)
				goto out;

			qdf_mem_copy(sap_config->acs_cfg.ch_list, tmp,
					sap_config->acs_cfg.ch_list_count);
		}
	} else if (tb[QCA_WLAN_VENDOR_ATTR_ACS_FREQ_LIST]) {
		uint32_t *freq =
			nla_data(tb[QCA_WLAN_VENDOR_ATTR_ACS_FREQ_LIST]);
		sap_config->acs_cfg.ch_list_count = nla_len(
			tb[QCA_WLAN_VENDOR_ATTR_ACS_FREQ_LIST]) /
				sizeof(uint32_t);
		if (sap_config->acs_cfg.ch_list_count) {
			sap_config->acs_cfg.ch_list = qdf_mem_malloc(
				sap_config->acs_cfg.ch_list_count);
			if (sap_config->acs_cfg.ch_list == NULL) {
				hdd_err("ACS config alloc fail");
				status = -ENOMEM;
				goto out;
			}

			/* convert frequency to channel */
			for (i = 0; i < sap_config->acs_cfg.ch_list_count; i++)
				sap_config->acs_cfg.ch_list[i] =
					ieee80211_frequency_to_channel(freq[i]);
		}
	}

	hdd_debug("get pcl for DO_ACS vendor command");

	/* consult policy manager to get PCL */
	status = cds_get_pcl(CDS_SAP_MODE,
				sap_config->acs_cfg.pcl_channels,
				&sap_config->acs_cfg.pcl_ch_count,
				weight_list, QDF_ARRAY_SIZE(weight_list));
	if (QDF_STATUS_SUCCESS != status)
		hdd_err("Get PCL failed");

	/* ACS override for android */
	if (hdd_ctx->config->sap_p2p_11ac_override && ht_enabled) {
		hdd_notice("ACS Config override for 11AC");
		vht_enabled = 1;
		sap_config->acs_cfg.hw_mode = eCSR_DOT11_MODE_11ac;
		sap_config->acs_cfg.ch_width =
					hdd_ctx->config->vhtChannelWidth;
		/* No VHT80 in 2.4G so perform ACS accordingly */
		if (sap_config->acs_cfg.end_ch <= 14 &&
			sap_config->acs_cfg.ch_width == eHT_CHANNEL_WIDTH_80MHZ)
			sap_config->acs_cfg.ch_width = eHT_CHANNEL_WIDTH_40MHZ;
	}

	wlan_hdd_set_acs_ch_range(sap_config, ht_enabled, vht_enabled);

	hdd_notice("ACS Config for wlan%d: HW_MODE: %d ACS_BW: %d HT: %d VHT: %d START_CH: %d END_CH: %d",
		adapter->dev->ifindex, sap_config->acs_cfg.hw_mode,
		ch_width, ht_enabled, vht_enabled,
		sap_config->acs_cfg.start_ch, sap_config->acs_cfg.end_ch);

	if (sap_config->acs_cfg.ch_list_count) {
		hdd_notice("ACS channel list: len: %d",
					sap_config->acs_cfg.ch_list_count);
		for (i = 0; i < sap_config->acs_cfg.ch_list_count; i++)
			hdd_notice("%d ", sap_config->acs_cfg.ch_list[i]);
	}
	sap_config->acs_cfg.acs_mode = true;
	if (test_bit(ACS_IN_PROGRESS, &hdd_ctx->g_event_flags)) {
		/* ***Note*** Completion variable usage is not allowed
		 * here since ACS scan operation may take max 2.2 sec
		 * for 5G band:
		 *   9 Active channel X 40 ms active scan time +
		 *   16 Passive channel X 110ms passive scan time
		 * Since this CFG80211 call lock rtnl mutex, we cannot hold on
		 * for this long. So we split up the scanning part.
		 */
		set_bit(ACS_PENDING, &adapter->event_flags);
		hdd_notice("ACS Pending for wlan%d", adapter->dev->ifindex);
		status = 0;
	} else {
		status = wlan_hdd_cfg80211_start_acs(adapter);
	}

out:
	if (0 == status) {
		temp_skbuff = cfg80211_vendor_cmd_alloc_reply_skb(wiphy,
							      NLMSG_HDRLEN);
		if (temp_skbuff != NULL)
			return cfg80211_vendor_cmd_reply(temp_skbuff);
	}

	clear_bit(ACS_IN_PROGRESS, &hdd_ctx->g_event_flags);

	return status;
}

 /**
 * wlan_hdd_cfg80211_do_acs : CFG80211 handler function for DO_ACS Vendor CMD
 * @wiphy:  Linux wiphy struct pointer
 * @wdev:   Linux wireless device struct pointer
 * @data:   ACS information from hostapd
 * @data_len: ACS information len
 *
 * This function handle DO_ACS Vendor command from hostapd, parses ACS config
 * and starts ACS procedure.
 *
 * Return: ACS procedure start status
 */

static int wlan_hdd_cfg80211_do_acs(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    const void *data, int data_len)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_do_acs(wiphy, wdev, data, data_len);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * wlan_hdd_cfg80211_start_pending_acs : Start pending ACS procedure for SAP
 * @work:  Linux workqueue struct pointer for ACS work
 *
 * This function starts the ACS procedure which was marked pending when an ACS
 * procedure was in progress for a concurrent SAP interface.
 *
 * Return: None
 */

static void wlan_hdd_cfg80211_start_pending_acs(struct work_struct *work)
{
	hdd_adapter_t *adapter = container_of(work, hdd_adapter_t,
							acs_pending_work.work);
	wlan_hdd_cfg80211_start_acs(adapter);
}

/**
 * wlan_hdd_cfg80211_acs_ch_select_evt: Callback function for ACS evt
 * @adapter: Pointer to SAP adapter struct
 * @pri_channel: SAP ACS procedure selected Primary channel
 * @sec_channel: SAP ACS procedure selected secondary channel
 *
 * This is a callback function from SAP module on ACS procedure is completed.
 * This function send the ACS selected channel information to hostapd
 *
 * Return: None
 */

void wlan_hdd_cfg80211_acs_ch_select_evt(hdd_adapter_t *adapter)
{
	hdd_context_t *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	tsap_Config_t *sap_cfg = &(WLAN_HDD_GET_AP_CTX_PTR(adapter))->sapConfig;
	struct sk_buff *vendor_event;
	int ret_val;
	hdd_adapter_t *con_sap_adapter;
	uint16_t ch_width;

	vendor_event = cfg80211_vendor_event_alloc(hdd_ctx->wiphy,
			&(adapter->wdev),
			4 * sizeof(u8) + 1 * sizeof(u16) + 4 + NLMSG_HDRLEN,
			QCA_NL80211_VENDOR_SUBCMD_DO_ACS_INDEX,
			GFP_KERNEL);

	if (!vendor_event) {
		hdd_err("cfg80211_vendor_event_alloc failed");
		return;
	}

	ret_val = nla_put_u8(vendor_event,
				QCA_WLAN_VENDOR_ATTR_ACS_PRIMARY_CHANNEL,
				sap_cfg->acs_cfg.pri_ch);
	if (ret_val) {
		hdd_err("QCA_WLAN_VENDOR_ATTR_ACS_PRIMARY_CHANNEL put fail");
		kfree_skb(vendor_event);
		return;
	}

	ret_val = nla_put_u8(vendor_event,
				QCA_WLAN_VENDOR_ATTR_ACS_SECONDARY_CHANNEL,
				sap_cfg->acs_cfg.ht_sec_ch);
	if (ret_val) {
		hdd_err("QCA_WLAN_VENDOR_ATTR_ACS_SECONDARY_CHANNEL put fail");
		kfree_skb(vendor_event);
		return;
	}

	ret_val = nla_put_u8(vendor_event,
			QCA_WLAN_VENDOR_ATTR_ACS_VHT_SEG0_CENTER_CHANNEL,
			sap_cfg->acs_cfg.vht_seg0_center_ch);
	if (ret_val) {
		hdd_err("QCA_WLAN_VENDOR_ATTR_ACS_VHT_SEG0_CENTER_CHANNEL put fail");
		kfree_skb(vendor_event);
		return;
	}

	ret_val = nla_put_u8(vendor_event,
			QCA_WLAN_VENDOR_ATTR_ACS_VHT_SEG1_CENTER_CHANNEL,
			sap_cfg->acs_cfg.vht_seg1_center_ch);
	if (ret_val) {
		hdd_err("QCA_WLAN_VENDOR_ATTR_ACS_VHT_SEG1_CENTER_CHANNEL put fail");
		kfree_skb(vendor_event);
		return;
	}

	if (sap_cfg->acs_cfg.ch_width == CH_WIDTH_80MHZ)
		ch_width = 80;
	else if (sap_cfg->acs_cfg.ch_width == CH_WIDTH_40MHZ)
		ch_width = 40;
	else
		ch_width = 20;

	ret_val = nla_put_u16(vendor_event,
				QCA_WLAN_VENDOR_ATTR_ACS_CHWIDTH,
				ch_width);
	if (ret_val) {
		hdd_err("QCA_WLAN_VENDOR_ATTR_ACS_CHWIDTH put fail");
		kfree_skb(vendor_event);
		return;
	}
	if (sap_cfg->acs_cfg.pri_ch > 14)
		ret_val = nla_put_u8(vendor_event,
					QCA_WLAN_VENDOR_ATTR_ACS_HW_MODE,
					QCA_ACS_MODE_IEEE80211A);
	else
		ret_val = nla_put_u8(vendor_event,
					QCA_WLAN_VENDOR_ATTR_ACS_HW_MODE,
					QCA_ACS_MODE_IEEE80211G);

	if (ret_val) {
		hdd_err("QCA_WLAN_VENDOR_ATTR_ACS_HW_MODE put fail");
		kfree_skb(vendor_event);
		return;
	}

	hdd_notice("ACS result for wlan%d: PRI_CH: %d SEC_CH: %d VHT_SEG0: %d VHT_SEG1: %d ACS_BW: %d",
		adapter->dev->ifindex, sap_cfg->acs_cfg.pri_ch,
		sap_cfg->acs_cfg.ht_sec_ch, sap_cfg->acs_cfg.vht_seg0_center_ch,
		sap_cfg->acs_cfg.vht_seg1_center_ch, ch_width);

	cfg80211_vendor_event(vendor_event, GFP_KERNEL);
	/* ***Note*** As already mentioned Completion variable usage is not
	 * allowed here since ACS scan operation may take max 2.2 sec.
	 * Further in AP-AP mode pending ACS is resumed here to serailize ACS
	 * operation.
	 * TODO: Delayed operation is used since SME-PMAC strut is global. Thus
	 * when Primary AP ACS is complete and secondary AP ACS is started here
	 * immediately, Primary AP start_bss may come inbetween ACS operation
	 * and overwrite Sec AP ACS paramters. Thus Sec AP ACS is executed with
	 * delay. This path and below constraint will be removed on sessionizing
	 * SAP acs parameters and decoupling SAP from PMAC (WIP).
	 * As per design constraint user space control application must take
	 * care of serailizing hostapd start for each VIF in AP-AP mode to avoid
	 * this code path. Sec AP hostapd should be started after Primary AP
	 * start beaconing which can be confirmed by getchannel iwpriv command
	 */

	con_sap_adapter = hdd_get_con_sap_adapter(adapter, false);
	if (con_sap_adapter &&
		test_bit(ACS_PENDING, &con_sap_adapter->event_flags)) {
		INIT_DELAYED_WORK(&con_sap_adapter->acs_pending_work,
				      wlan_hdd_cfg80211_start_pending_acs);
		/* Lets give 500ms for OBSS + START_BSS to complete */
		schedule_delayed_work(&con_sap_adapter->acs_pending_work,
							msecs_to_jiffies(500));
		clear_bit(ACS_PENDING, &con_sap_adapter->event_flags);
	}

	return;
}

static int
__wlan_hdd_cfg80211_get_supported_features(struct wiphy *wiphy,
					 struct wireless_dev *wdev,
					 const void *data,
					 int data_len)
{
	hdd_context_t *pHddCtx = wiphy_priv(wiphy);
	struct sk_buff *skb = NULL;
	uint32_t fset = 0;
	int ret;

	ENTER_DEV(wdev->netdev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	ret = wlan_hdd_validate_context(pHddCtx);
	if (ret)
		return ret;

	if (wiphy->interface_modes & BIT(NL80211_IFTYPE_STATION)) {
		hdd_notice("Infra Station mode is supported by driver");
		fset |= WIFI_FEATURE_INFRA;
	}
	if (true == hdd_is_5g_supported(pHddCtx)) {
		hdd_notice("INFRA_5G is supported by firmware");
		fset |= WIFI_FEATURE_INFRA_5G;
	}
#ifdef WLAN_FEATURE_P2P
	if ((wiphy->interface_modes & BIT(NL80211_IFTYPE_P2P_CLIENT)) &&
	    (wiphy->interface_modes & BIT(NL80211_IFTYPE_P2P_GO))) {
		hdd_notice("WiFi-Direct is supported by driver");
		fset |= WIFI_FEATURE_P2P;
	}
#endif
	fset |= WIFI_FEATURE_SOFT_AP;

	/* HOTSPOT is a supplicant feature, enable it by default */
	fset |= WIFI_FEATURE_HOTSPOT;

#ifdef FEATURE_WLAN_EXTSCAN
	if (sme_is_feature_supported_by_fw(EXTENDED_SCAN)) {
		hdd_notice("EXTScan is supported by firmware");
		fset |= WIFI_FEATURE_EXTSCAN | WIFI_FEATURE_HAL_EPNO;
	}
#endif
	if (wlan_hdd_nan_is_supported()) {
		hdd_notice("NAN is supported by firmware");
		fset |= WIFI_FEATURE_NAN;
	}
	if (sme_is_feature_supported_by_fw(RTT)) {
		hdd_notice("RTT is supported by firmware");
		fset |= WIFI_FEATURE_D2D_RTT;
		fset |= WIFI_FEATURE_D2AP_RTT;
	}
#ifdef FEATURE_WLAN_SCAN_PNO
	if (pHddCtx->config->configPNOScanSupport &&
	    sme_is_feature_supported_by_fw(PNO)) {
		hdd_notice("PNO is supported by firmware");
		fset |= WIFI_FEATURE_PNO;
	}
#endif
	fset |= WIFI_FEATURE_ADDITIONAL_STA;
#ifdef FEATURE_WLAN_TDLS
	if ((true == pHddCtx->config->fEnableTDLSSupport) &&
	    sme_is_feature_supported_by_fw(TDLS)) {
		hdd_notice("TDLS is supported by firmware");
		fset |= WIFI_FEATURE_TDLS;
	}
	if (sme_is_feature_supported_by_fw(TDLS) &&
	    (true == pHddCtx->config->fEnableTDLSOffChannel) &&
	    sme_is_feature_supported_by_fw(TDLS_OFF_CHANNEL)) {
		hdd_notice("TDLS off-channel is supported by firmware");
		fset |= WIFI_FEATURE_TDLS_OFFCHANNEL;
	}
#endif
#ifdef WLAN_AP_STA_CONCURRENCY
	fset |= WIFI_FEATURE_AP_STA;
#endif
	fset |= WIFI_FEATURE_RSSI_MONITOR;

	if (hdd_link_layer_stats_supported())
		fset |= WIFI_FEATURE_LINK_LAYER_STATS;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, sizeof(fset) +
						  NLMSG_HDRLEN);
	if (!skb) {
		hdd_err("cfg80211_vendor_cmd_alloc_reply_skb failed");
		return -EINVAL;
	}
	hdd_notice("Supported Features : 0x%x", fset);
	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_FEATURE_SET, fset)) {
		hdd_err("nla put fail");
		goto nla_put_failure;
	}
	ret = cfg80211_vendor_cmd_reply(skb);
	EXIT();
	return ret;
nla_put_failure:
	kfree_skb(skb);
	return -EINVAL;
}

/**
 * wlan_hdd_cfg80211_get_supported_features() - get supported features
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return:   Return the Success or Failure code.
 */
static int
wlan_hdd_cfg80211_get_supported_features(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data, int data_len)
{
	int ret = 0;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_get_supported_features(wiphy, wdev,
						data, data_len);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * __wlan_hdd_cfg80211_set_scanning_mac_oui() - set scan MAC
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Set the MAC address that is to be used for scanning.
 *
 * Return:   Return the Success or Failure code.
 */
static int
__wlan_hdd_cfg80211_set_scanning_mac_oui(struct wiphy *wiphy,
					 struct wireless_dev *wdev,
					 const void *data,
					 int data_len)
{
	tpSirScanMacOui pReqMsg = NULL;
	hdd_context_t *pHddCtx = wiphy_priv(wiphy);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SET_SCANNING_MAC_OUI_MAX + 1];
	QDF_STATUS status;
	int ret;

	ENTER_DEV(wdev->netdev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	ret = wlan_hdd_validate_context(pHddCtx);
	if (ret)
		return ret;

	if (false == pHddCtx->config->enable_mac_spoofing) {
		hdd_warn("MAC address spoofing is not enabled");
		return -ENOTSUPP;
	}

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_SET_SCANNING_MAC_OUI_MAX,
		      data, data_len, NULL)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}
	pReqMsg = qdf_mem_malloc(sizeof(*pReqMsg));
	if (!pReqMsg) {
		hdd_err("qdf_mem_malloc failed");
		return -ENOMEM;
	}
	if (!tb[QCA_WLAN_VENDOR_ATTR_SET_SCANNING_MAC_OUI]) {
		hdd_err("attr mac oui failed");
		goto fail;
	}
	nla_memcpy(&pReqMsg->oui[0],
		   tb[QCA_WLAN_VENDOR_ATTR_SET_SCANNING_MAC_OUI],
		   sizeof(pReqMsg->oui));
	hdd_notice("Oui (%02x:%02x:%02x)", pReqMsg->oui[0],
	       pReqMsg->oui[1], pReqMsg->oui[2]);
	status = sme_set_scanning_mac_oui(pHddCtx->hHal, pReqMsg);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("sme_set_scanning_mac_oui failed(err=%d)", status);
		goto fail;
	}
	return 0;
fail:
	qdf_mem_free(pReqMsg);
	return -EINVAL;
}

/**
 * wlan_hdd_cfg80211_set_scanning_mac_oui() - set scan MAC
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Set the MAC address that is to be used for scanning.  This is an
 * SSR-protecting wrapper function.
 *
 * Return:   Return the Success or Failure code.
 */
static int
wlan_hdd_cfg80211_set_scanning_mac_oui(struct wiphy *wiphy,
				       struct wireless_dev *wdev,
				       const void *data,
				       int data_len)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_set_scanning_mac_oui(wiphy, wdev,
						       data, data_len);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * __wlan_hdd_cfg80211_get_concurrency_matrix() - to retrieve concurrency matrix
 * @wiphy: pointer phy adapter
 * @wdev: pointer to wireless device structure
 * @data: pointer to data buffer
 * @data_len: length of data
 *
 * This routine will give concurrency matrix
 *
 * Return: int status code
 */
static int __wlan_hdd_cfg80211_get_concurrency_matrix(struct wiphy *wiphy,
					 struct wireless_dev *wdev,
					 const void *data,
					 int data_len)
{
	uint32_t feature_set_matrix[CDS_MAX_FEATURE_SET] = {0};
	uint8_t i, feature_sets, max_feature_sets;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_GET_CONCURRENCY_MATRIX_MAX + 1];
	struct sk_buff *reply_skb;
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	int ret;

	ENTER_DEV(wdev->netdev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_GET_CONCURRENCY_MATRIX_MAX,
			data, data_len, NULL)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}

	/* Parse and fetch max feature set */
	if (!tb[QCA_WLAN_VENDOR_ATTR_GET_CONCURRENCY_MATRIX_CONFIG_PARAM_SET_SIZE_MAX]) {
		hdd_err("Attr max feature set size failed");
		return -EINVAL;
	}
	max_feature_sets = nla_get_u32(tb[
		QCA_WLAN_VENDOR_ATTR_GET_CONCURRENCY_MATRIX_CONFIG_PARAM_SET_SIZE_MAX]);
	hdd_info("Max feature set size: %d", max_feature_sets);

	/* Fill feature combination matrix */
	feature_sets = 0;
	feature_set_matrix[feature_sets++] = WIFI_FEATURE_INFRA |
		WIFI_FEATURE_P2P;
	/* Add more feature combinations here */

	feature_sets = QDF_MIN(feature_sets, max_feature_sets);
	hdd_info("Number of feature sets:%d", feature_sets);
	hdd_info("Feature set matrix");
	for (i = 0; i < feature_sets; i++)
		hdd_info("[%d] 0x%02X", i, feature_set_matrix[i]);

	reply_skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, sizeof(u32) +
			sizeof(u32) * feature_sets + NLMSG_HDRLEN);
	if (!reply_skb) {
		hdd_err("Feature set matrix: buffer alloc fail");
		return -ENOMEM;
	}

	if (nla_put_u32(reply_skb,
		QCA_WLAN_VENDOR_ATTR_GET_CONCURRENCY_MATRIX_RESULTS_SET_SIZE,
		feature_sets) ||
	    nla_put(reply_skb,
		QCA_WLAN_VENDOR_ATTR_GET_CONCURRENCY_MATRIX_RESULTS_SET,
		sizeof(u32) * feature_sets,
		feature_set_matrix)) {
			hdd_err("nla put fail");
			kfree_skb(reply_skb);
			return -EINVAL;
	}
	return cfg80211_vendor_cmd_reply(reply_skb);
}

/**
 * wlan_hdd_cfg80211_get_concurrency_matrix() - get concurrency matrix
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Retrieves the concurrency feature set matrix
 *
 * Return: 0 on success, negative errno on failure
 */
static int
wlan_hdd_cfg80211_get_concurrency_matrix(struct wiphy *wiphy,
				       struct wireless_dev *wdev,
				       const void *data,
				       int data_len)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_get_concurrency_matrix(wiphy, wdev,
							data, data_len);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * wlan_hdd_cfg80211_set_feature() - Set the bitmask for supported features
 * @feature_flags: pointer to the byte array of features.
 * @feature: Feature to be turned ON in the byte array.
 *
 * Return: None
 *
 * This is called to turn ON or SET the feature flag for the requested feature.
 **/
#define NUM_BITS_IN_BYTE       8
void wlan_hdd_cfg80211_set_feature(uint8_t *feature_flags, uint8_t feature)
{
	uint32_t index;
	uint8_t bit_mask;

	index = feature / NUM_BITS_IN_BYTE;
	bit_mask = 1 << (feature % NUM_BITS_IN_BYTE);
	feature_flags[index] |= bit_mask;
}

/**
 * __wlan_hdd_cfg80211_get_features() - Get the Driver Supported features
 * @wiphy: pointer to wireless wiphy structure.
 * @wdev: pointer to wireless_dev structure.
 * @data: Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * This is called when wlan driver needs to send supported feature set to
 * supplicant upon a request/query from the supplicant.
 *
 * Return: Return the Success or Failure code.
 **/
#define MAX_CONCURRENT_CHAN_ON_24G    2
#define MAX_CONCURRENT_CHAN_ON_5G     2
static int
__wlan_hdd_cfg80211_get_features(struct wiphy *wiphy,
				 struct wireless_dev *wdev,
				 const void *data, int data_len)
{
	struct sk_buff *skb = NULL;
	uint32_t dbs_capability = 0;
	bool one_by_one_dbs, two_by_two_dbs;
	QDF_STATUS ret = QDF_STATUS_E_FAILURE;
	int ret_val;

	uint8_t feature_flags[(NUM_QCA_WLAN_VENDOR_FEATURES + 7) / 8] = {0};
	hdd_context_t *hdd_ctx_ptr = wiphy_priv(wiphy);

	ENTER_DEV(wdev->netdev);

	ret_val = wlan_hdd_validate_context(hdd_ctx_ptr);
	if (ret_val)
		return ret_val;

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	if (roaming_offload_enabled(hdd_ctx_ptr)) {
		hdd_notice("Key Mgmt Offload is supported");
		wlan_hdd_cfg80211_set_feature(feature_flags,
				QCA_WLAN_VENDOR_FEATURE_KEY_MGMT_OFFLOAD);
	}

	wlan_hdd_cfg80211_set_feature(feature_flags,
				QCA_WLAN_VENDOR_FEATURE_SUPPORT_HW_MODE_ANY);
	if (wma_is_scan_simultaneous_capable())
		wlan_hdd_cfg80211_set_feature(feature_flags,
			QCA_WLAN_VENDOR_FEATURE_OFFCHANNEL_SIMULTANEOUS);
	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, sizeof(feature_flags) +
			NLMSG_HDRLEN);

	if (!skb) {
		hdd_err("cfg80211_vendor_cmd_alloc_reply_skb failed");
		return -ENOMEM;
	}

	if (nla_put(skb, QCA_WLAN_VENDOR_ATTR_FEATURE_FLAGS,
			sizeof(feature_flags), feature_flags))
		goto nla_put_failure;

	ret = wma_get_dbs_hw_modes(&one_by_one_dbs, &two_by_two_dbs);
	if (QDF_STATUS_SUCCESS == ret) {
		if (one_by_one_dbs)
			dbs_capability = DRV_DBS_CAPABILITY_1X1;

		if (two_by_two_dbs)
			dbs_capability = DRV_DBS_CAPABILITY_2X2;

		if (!one_by_one_dbs && !two_by_two_dbs)
			dbs_capability = DRV_DBS_CAPABILITY_DISABLED;
	} else {
		hdd_err("wma_get_dbs_hw_mode failed");
		dbs_capability = DRV_DBS_CAPABILITY_DISABLED;
	}

	hdd_info("dbs_capability is %d", dbs_capability);

	if (nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_MAX_CONCURRENT_CHANNELS_2_4_BAND,
			MAX_CONCURRENT_CHAN_ON_24G))
		goto nla_put_failure;

	if (nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_MAX_CONCURRENT_CHANNELS_5_0_BAND,
			MAX_CONCURRENT_CHAN_ON_5G))
		goto nla_put_failure;

	return cfg80211_vendor_cmd_reply(skb);

nla_put_failure:
	kfree_skb(skb);
	return -EINVAL;
}

/**
 * wlan_hdd_cfg80211_get_features() - Get the Driver Supported features
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * This is called when wlan driver needs to send supported feature set to
 * supplicant upon a request/query from the supplicant.
 *
 * Return:   Return the Success or Failure code.
 */
static int
wlan_hdd_cfg80211_get_features(struct wiphy *wiphy,
		struct wireless_dev *wdev,
		const void *data, int data_len)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_get_features(wiphy, wdev,
					       data, data_len);
	cds_ssr_unprotect(__func__);

	return ret;
}


/**
 * __wlan_hdd_cfg80211_set_ext_roam_params() - Settings for roaming parameters
 * @wiphy:                 The wiphy structure
 * @wdev:                  The wireless device
 * @data:                  Data passed by framework
 * @data_len:              Parameters to be configured passed as data
 *
 * The roaming related parameters are configured by the framework
 * using this interface.
 *
 * Return: Return either success or failure code.
 */
static int
__wlan_hdd_cfg80211_set_ext_roam_params(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void *data, int data_len)
{
	struct net_device *dev = wdev->netdev;
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *pHddCtx = wiphy_priv(wiphy);
	uint8_t session_id;
	struct roam_ext_params roam_params;
	uint32_t cmd_type, req_id;
	struct nlattr *curr_attr;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_MAX + 1];
	struct nlattr *tb2[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_MAX + 1];
	int rem, i;
	uint32_t buf_len = 0;
	int ret;

	ENTER_DEV(dev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	ret = wlan_hdd_validate_context(pHddCtx);
	if (ret)
		return ret;

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_MAX,
		data, data_len,
		NULL)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}
	/* Parse and fetch Command Type*/
	if (!tb[QCA_WLAN_VENDOR_ATTR_ROAMING_SUBCMD]) {
		hdd_err("roam cmd type failed");
		goto fail;
	}
	session_id = pAdapter->sessionId;
	qdf_mem_set(&roam_params, sizeof(roam_params), 0);
	cmd_type = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_ROAMING_SUBCMD]);
	if (!tb[QCA_WLAN_VENDOR_ATTR_ROAMING_REQ_ID]) {
		hdd_err("attr request id failed");
		goto fail;
	}
	req_id = nla_get_u32(
		tb[QCA_WLAN_VENDOR_ATTR_ROAMING_REQ_ID]);
	hdd_debug("Req Id (%d)", req_id);
	hdd_debug("Cmd Type (%d)", cmd_type);
	switch (cmd_type) {
	case QCA_WLAN_VENDOR_ATTR_ROAM_SUBCMD_SSID_WHITE_LIST:
		i = 0;
		nla_for_each_nested(curr_attr,
			tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_WHITE_LIST_SSID_LIST],
			rem) {
			if (nla_parse(tb2,
				QCA_WLAN_VENDOR_ATTR_ROAM_SUBCMD_MAX,
				nla_data(curr_attr), nla_len(curr_attr),
				NULL)) {
				hdd_err("nla_parse failed");
				goto fail;
			}
			/* Parse and Fetch allowed SSID list*/
			if (!tb2[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_WHITE_LIST_SSID]) {
				hdd_err("attr allowed ssid failed");
				goto fail;
			}
			buf_len = nla_len(tb2[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_WHITE_LIST_SSID]);
			/*
			 * Upper Layers include a null termination character.
			 * Check for the actual permissible length of SSID and
			 * also ensure not to copy the NULL termination
			 * character to the driver buffer.
			 */
			if (buf_len && (i < MAX_SSID_ALLOWED_LIST) &&
				((buf_len - 1) <= SIR_MAC_MAX_SSID_LENGTH)) {
				nla_memcpy(
					roam_params.ssid_allowed_list[i].ssId,
					tb2[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_WHITE_LIST_SSID],
					buf_len - 1);
				roam_params.ssid_allowed_list[i].length =
					buf_len - 1;
				hdd_debug("SSID[%d]: %.*s,length = %d", i,
					roam_params.ssid_allowed_list[i].length,
					roam_params.ssid_allowed_list[i].ssId,
					roam_params.ssid_allowed_list[i].length);
				i++;
			} else {
				hdd_err("Invalid buffer length");
			}
		}
		roam_params.num_ssid_allowed_list = i;
		hdd_debug("Num of Allowed SSID %d",
			roam_params.num_ssid_allowed_list);
		sme_update_roam_params(pHddCtx->hHal, session_id,
				roam_params, REASON_ROAM_SET_SSID_ALLOWED);
		break;
	case QCA_WLAN_VENDOR_ATTR_ROAM_SUBCMD_SET_EXTSCAN_ROAM_PARAMS:
		/* Parse and fetch 5G Boost Threshold */
		if (!tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_A_BAND_BOOST_THRESHOLD]) {
			hdd_err("5G boost threshold failed");
			goto fail;
		}
		roam_params.raise_rssi_thresh_5g = nla_get_s32(
			tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_A_BAND_BOOST_THRESHOLD]);
		hdd_debug("5G Boost Threshold (%d)",
			roam_params.raise_rssi_thresh_5g);
		/* Parse and fetch 5G Penalty Threshold */
		if (!tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_A_BAND_PENALTY_THRESHOLD]) {
			hdd_err("5G penalty threshold failed");
			goto fail;
		}
		roam_params.drop_rssi_thresh_5g = nla_get_s32(
			tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_A_BAND_PENALTY_THRESHOLD]);
		hdd_debug("5G Penalty Threshold (%d)",
			roam_params.drop_rssi_thresh_5g);
		/* Parse and fetch 5G Boost Factor */
		if (!tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_A_BAND_BOOST_FACTOR]) {
			hdd_err("5G boost Factor failed");
			goto fail;
		}
		roam_params.raise_factor_5g = nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_A_BAND_BOOST_FACTOR]);
		hdd_debug("5G Boost Factor (%d)",
			roam_params.raise_factor_5g);
		/* Parse and fetch 5G Penalty factor */
		if (!tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_A_BAND_PENALTY_FACTOR]) {
			hdd_err("5G Penalty Factor failed");
			goto fail;
		}
		roam_params.drop_factor_5g = nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_A_BAND_PENALTY_FACTOR]);
		hdd_debug("5G Penalty factor (%d)",
			roam_params.drop_factor_5g);
		/* Parse and fetch 5G Max Boost */
		if (!tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_A_BAND_MAX_BOOST]) {
			hdd_err("5G Max Boost failed");
			goto fail;
		}
		roam_params.max_raise_rssi_5g = nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_A_BAND_MAX_BOOST]);
		hdd_debug("5G Max Boost (%d)",
			roam_params.max_raise_rssi_5g);
		/* Parse and fetch Rssi Diff */
		if (!tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_LAZY_ROAM_HISTERESYS]) {
			hdd_err("Rssi Diff failed");
			goto fail;
		}
		roam_params.rssi_diff = nla_get_s32(
			tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_LAZY_ROAM_HISTERESYS]);
		hdd_debug("RSSI Diff (%d)",
			roam_params.rssi_diff);
		/* Parse and fetch Alert Rssi Threshold */
		if (!tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_ALERT_ROAM_RSSI_TRIGGER]) {
			hdd_err("Alert Rssi Threshold failed");
			goto fail;
		}
		roam_params.alert_rssi_threshold = nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_ALERT_ROAM_RSSI_TRIGGER]);
		hdd_debug("Alert RSSI Threshold (%d)",
			roam_params.alert_rssi_threshold);
		sme_update_roam_params(pHddCtx->hHal, session_id,
			roam_params,
			REASON_ROAM_EXT_SCAN_PARAMS_CHANGED);
		break;
	case QCA_WLAN_VENDOR_ATTR_ROAM_SUBCMD_SET_LAZY_ROAM:
		/* Parse and fetch Activate Good Rssi Roam */
		if (!tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_LAZY_ROAM_ENABLE]) {
			hdd_err("Activate Good Rssi Roam failed");
			goto fail;
		}
		roam_params.good_rssi_roam = nla_get_s32(
			tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_LAZY_ROAM_ENABLE]);
		hdd_debug("Activate Good Rssi Roam (%d)",
			roam_params.good_rssi_roam);
		sme_update_roam_params(pHddCtx->hHal, session_id,
			roam_params, REASON_ROAM_GOOD_RSSI_CHANGED);
		break;
	case QCA_WLAN_VENDOR_ATTR_ROAM_SUBCMD_SET_BSSID_PREFS:
		/* Parse and fetch number of preferred BSSID */
		if (!tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_LAZY_ROAM_NUM_BSSID]) {
			hdd_err("attr num of preferred bssid failed");
			goto fail;
		}
		roam_params.num_bssid_favored = nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_LAZY_ROAM_NUM_BSSID]);
		hdd_debug("Num of Preferred BSSID (%d)",
			roam_params.num_bssid_favored);
		i = 0;
		nla_for_each_nested(curr_attr,
			tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_BSSID_PREFS],
			rem) {
			if (nla_parse(tb2,
				QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_MAX,
				nla_data(curr_attr), nla_len(curr_attr),
				NULL)) {
				hdd_err("nla_parse failed");
				goto fail;
			}
			/* Parse and fetch MAC address */
			if (!tb2[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_LAZY_ROAM_BSSID]) {
				hdd_err("attr mac address failed");
				goto fail;
			}
			nla_memcpy(roam_params.bssid_favored[i].bytes,
				tb2[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_LAZY_ROAM_BSSID],
				QDF_MAC_ADDR_SIZE);
			hdd_debug(MAC_ADDRESS_STR,
			    MAC_ADDR_ARRAY(roam_params.bssid_favored[i].bytes));
			/* Parse and fetch preference factor*/
			if (!tb2[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_LAZY_ROAM_RSSI_MODIFIER]) {
				hdd_err("BSSID Preference score failed");
				goto fail;
			}
			roam_params.bssid_favored_factor[i] = nla_get_u32(
				tb2[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_LAZY_ROAM_RSSI_MODIFIER]);
			hdd_debug("BSSID Preference score (%d)",
				roam_params.bssid_favored_factor[i]);
			i++;
		}
		sme_update_roam_params(pHddCtx->hHal, session_id,
			roam_params, REASON_ROAM_SET_FAVORED_BSSID);
		break;
	case QCA_WLAN_VENDOR_ATTR_ROAM_SUBCMD_SET_BLACKLIST_BSSID:
		/* Parse and fetch number of blacklist BSSID */
		if (!tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_BSSID_PARAMS_NUM_BSSID]) {
			hdd_err("attr num of blacklist bssid failed");
			goto fail;
		}
		roam_params.num_bssid_avoid_list = nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_BSSID_PARAMS_NUM_BSSID]);
		hdd_debug("Num of blacklist BSSID (%d)",
			roam_params.num_bssid_avoid_list);
		i = 0;
		nla_for_each_nested(curr_attr,
			tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_BSSID_PARAMS],
			rem) {
			if (nla_parse(tb2,
				QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_MAX,
				nla_data(curr_attr), nla_len(curr_attr),
				NULL)) {
				hdd_err("nla_parse failed");
				goto fail;
			}
			/* Parse and fetch MAC address */
			if (!tb2[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_BSSID_PARAMS_BSSID]) {
				hdd_err("attr blacklist addr failed");
				goto fail;
			}
			nla_memcpy(roam_params.bssid_avoid_list[i].bytes,
				tb2[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_BSSID_PARAMS_BSSID],
				QDF_MAC_ADDR_SIZE);
			hdd_debug(MAC_ADDRESS_STR,
				MAC_ADDR_ARRAY(
				roam_params.bssid_avoid_list[i].bytes));
			i++;
		}
		sme_update_roam_params(pHddCtx->hHal, session_id,
			roam_params, REASON_ROAM_SET_BLACKLIST_BSSID);
		break;
	}
	return 0;
fail:
	return -EINVAL;
}

/**
 * wlan_hdd_cfg80211_set_ext_roam_params() - set ext scan roam params
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return:   Return the Success or Failure code.
 */
static int
wlan_hdd_cfg80211_set_ext_roam_params(struct wiphy *wiphy,
				struct wireless_dev *wdev,
				const void *data,
				int data_len)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_set_ext_roam_params(wiphy, wdev,
							data, data_len);
	cds_ssr_unprotect(__func__);

	return ret;
}

static const struct nla_policy
wlan_hdd_set_no_dfs_flag_config_policy[QCA_WLAN_VENDOR_ATTR_SET_NO_DFS_FLAG_MAX
				       +1] = {
	[QCA_WLAN_VENDOR_ATTR_SET_NO_DFS_FLAG] = {.type = NLA_U32 },
};

/**
 *  wlan_hdd_check_dfs_channel_for_adapter() - check dfs channel in adapter
 *  @hdd_ctx:      HDD context
 *  @device_mode:    device mode
 *  Return:         bool
 */
static bool wlan_hdd_check_dfs_channel_for_adapter(hdd_context_t *hdd_ctx,
				enum tQDF_ADAPTER_MODE device_mode)
{
	hdd_adapter_t *adapter;
	hdd_adapter_list_node_t *adapter_node = NULL, *next = NULL;
	hdd_ap_ctx_t *ap_ctx;
	hdd_station_ctx_t *sta_ctx;
	QDF_STATUS qdf_status;

	qdf_status = hdd_get_front_adapter(hdd_ctx,
					   &adapter_node);

	while ((NULL != adapter_node) &&
	       (QDF_STATUS_SUCCESS == qdf_status)) {
		adapter = adapter_node->pAdapter;

		if ((device_mode == adapter->device_mode) &&
		    (device_mode == QDF_SAP_MODE)) {
			ap_ctx =
				WLAN_HDD_GET_AP_CTX_PTR(adapter);

			/*
			 *  if there is SAP already running on DFS channel,
			 *  do not disable scan on dfs channels. Note that
			 *  with SAP on DFS, there cannot be conurrency on
			 *  single radio. But then we can have multiple
			 *  radios !!
			 */
			if (CHANNEL_STATE_DFS ==
			    cds_get_channel_state(
				    ap_ctx->operatingChannel)) {
				hdd_err("SAP running on DFS channel");
				return true;
			}
		}

		if ((device_mode == adapter->device_mode) &&
		    (device_mode == QDF_STA_MODE)) {
			sta_ctx =
				WLAN_HDD_GET_STATION_CTX_PTR(adapter);

			/*
			 *  if STA is already connected on DFS channel,
			 *  do not disable scan on dfs channels
			 */
			if (hdd_conn_is_connected(sta_ctx) &&
			    (CHANNEL_STATE_DFS ==
			     cds_get_channel_state(
				     sta_ctx->conn_info.operationChannel))) {
				hdd_err("client connected on DFS channel");
				return true;
			}
		}

		qdf_status = hdd_get_next_adapter(hdd_ctx,
						  adapter_node,
						  &next);
		adapter_node = next;
	}

	return false;
}

/**
 * wlan_hdd_disable_dfs_chan_scan() - disable/enable DFS channels
 * @hdd_ctx: HDD context within host driver
 * @adapter: Adapter pointer
 * @no_dfs_flag: If TRUE, DFS channels cannot be used for scanning
 *
 * Loops through devices to see who is operating on DFS channels
 * and then disables/enables DFS channels by calling SME API.
 * Fails the disable request if any device is active on a DFS channel.
 *
 * Return: 0 or other error codes.
 */

int wlan_hdd_disable_dfs_chan_scan(hdd_context_t *hdd_ctx,
				   hdd_adapter_t *adapter,
				   uint32_t no_dfs_flag)
{
	tHalHandle h_hal = WLAN_HDD_GET_HAL_CTX(adapter);
	QDF_STATUS status;
	int ret_val = -EPERM;

	if (no_dfs_flag == hdd_ctx->config->enableDFSChnlScan) {
		if (no_dfs_flag) {
			status = wlan_hdd_check_dfs_channel_for_adapter(
				hdd_ctx, QDF_STA_MODE);

			if (true == status)
				return -EOPNOTSUPP;

			status = wlan_hdd_check_dfs_channel_for_adapter(
				hdd_ctx, QDF_SAP_MODE);

			if (true == status)
				return -EOPNOTSUPP;
		}

		hdd_ctx->config->enableDFSChnlScan = !no_dfs_flag;

		hdd_abort_mac_scan_all_adapters(hdd_ctx);

		/*
		 *  call the SME API to tunnel down the new channel list
		 *  to the firmware
		 */
		status = sme_handle_dfs_chan_scan(
			h_hal, hdd_ctx->config->enableDFSChnlScan);

		if (QDF_STATUS_SUCCESS == status) {
			ret_val = 0;

			/*
			 * Clear the SME scan cache also. Note that the
			 * clearing of scan results is independent of session;
			 * so no need to iterate over
			 * all sessions
			 */
			status = sme_scan_flush_result(h_hal);
			if (QDF_STATUS_SUCCESS != status)
				ret_val = -EPERM;
		}

	} else {
		hdd_notice(" the DFS flag has not changed");
		ret_val = 0;
	}
	return ret_val;
}

/**
 *  __wlan_hdd_cfg80211_disable_dfs_chan_scan() - DFS channel configuration
 *  @wiphy:          corestack handler
 *  @wdev:           wireless device
 *  @data:           data
 *  @data_len:       data length
 *  Return:         success(0) or reason code for failure
 */
static int __wlan_hdd_cfg80211_disable_dfs_chan_scan(struct wiphy *wiphy,
						     struct wireless_dev *wdev,
						     const void *data,
						     int data_len)
{
	struct net_device *dev = wdev->netdev;
	hdd_adapter_t *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *hdd_ctx  = wiphy_priv(wiphy);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SET_NO_DFS_FLAG_MAX + 1];
	int ret_val;
	uint32_t no_dfs_flag = 0;

	ENTER_DEV(dev);

	ret_val = wlan_hdd_validate_context(hdd_ctx);
	if (ret_val)
		return ret_val;

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_SET_NO_DFS_FLAG_MAX,
		      data, data_len,
		      wlan_hdd_set_no_dfs_flag_config_policy)) {
		hdd_err("invalid attr");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_SET_NO_DFS_FLAG]) {
		hdd_err("attr dfs flag failed");
		return -EINVAL;
	}

	no_dfs_flag = nla_get_u32(
		tb[QCA_WLAN_VENDOR_ATTR_SET_NO_DFS_FLAG]);

	hdd_notice(" DFS flag = %d", no_dfs_flag);

	if (no_dfs_flag > 1) {
		hdd_err("invalid value of dfs flag");
		return -EINVAL;
	}

	ret_val = wlan_hdd_disable_dfs_chan_scan(hdd_ctx, adapter,
						 no_dfs_flag);
	return ret_val;
}

/**
 * wlan_hdd_cfg80211_disable_dfs_chan_scan () - DFS scan vendor command
 *
 * @wiphy: wiphy device pointer
 * @wdev: wireless device pointer
 * @data: Vendor command data buffer
 * @data_len: Buffer length
 *
 * Handles QCA_WLAN_VENDOR_ATTR_SET_NO_DFS_FLAG_MAX. Validate it and
 * call wlan_hdd_disable_dfs_chan_scan to send it to firmware.
 *
 * Return: EOK or other error codes.
 */

static int wlan_hdd_cfg80211_disable_dfs_chan_scan(struct wiphy *wiphy,
						   struct wireless_dev *wdev,
						   const void *data,
						   int data_len)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_disable_dfs_chan_scan(wiphy, wdev,
							data, data_len);
	cds_ssr_unprotect(__func__);

	return ret;
}

static const struct nla_policy
wlan_hdd_wisa_cmd_policy[QCA_WLAN_VENDOR_ATTR_WISA_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_WISA_MODE] = {.type = NLA_U32 },
};

/**
 * __wlan_hdd_cfg80211_handle_wisa_cmd() - Handle WISA vendor cmd
 * @wiphy: wiphy device pointer
 * @wdev: wireless device pointer
 * @data: Vendor command data buffer
 * @data_len: Buffer length
 *
 * Handles QCA_WLAN_VENDOR_SUBCMD_WISA. Validate cmd attributes and
 * setup WISA Mode features.
 *
 * Return: Success(0) or reason code for failure
 */
static int __wlan_hdd_cfg80211_handle_wisa_cmd(struct wiphy *wiphy,
		struct wireless_dev *wdev, const void *data, int data_len)
{
	struct net_device *dev = wdev->netdev;
	hdd_adapter_t *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *hdd_ctx  = wiphy_priv(wiphy);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_WISA_MAX + 1];
	struct sir_wisa_params wisa;
	int ret_val;
	QDF_STATUS status;
	bool wisa_mode;

	ENTER_DEV(dev);
	ret_val = wlan_hdd_validate_context(hdd_ctx);
	if (ret_val)
		goto err;

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_WISA_MAX, data, data_len,
		      wlan_hdd_wisa_cmd_policy)) {
		hdd_err("Invalid WISA cmd attributes");
		ret_val = -EINVAL;
		goto err;
	}
	if (!tb[QCA_WLAN_VENDOR_ATTR_WISA_MODE]) {
		hdd_err("Invalid WISA mode");
		ret_val = -EINVAL;
		goto err;
	}

	wisa_mode = !!nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_WISA_MODE]);
	hdd_info("WISA Mode: %d", wisa_mode);
	wisa.mode = wisa_mode;
	wisa.vdev_id = adapter->sessionId;
	status = sme_set_wisa_params(hdd_ctx->hHal, &wisa);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("Unable to set WISA mode: %d to FW", wisa_mode);
		ret_val = -EINVAL;
	}
	if (QDF_IS_STATUS_SUCCESS(status) || wisa_mode == false)
		ol_txrx_set_wisa_mode(ol_txrx_get_vdev_from_vdev_id(
					adapter->sessionId), wisa_mode);
err:
	EXIT();
	return ret_val;
}

/**
 * wlan_hdd_cfg80211_handle_wisa_cmd() - Handle WISA vendor cmd
 * @wiphy:          corestack handler
 * @wdev:           wireless device
 * @data:           data
 * @data_len:       data length
 *
 * Handles QCA_WLAN_VENDOR_SUBCMD_WISA. Validate cmd attributes and
 * setup WISA mode features.
 *
 * Return: Success(0) or reason code for failure
 */
static int wlan_hdd_cfg80211_handle_wisa_cmd(struct wiphy *wiphy,
						   struct wireless_dev *wdev,
						   const void *data,
						   int data_len)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_handle_wisa_cmd(wiphy, wdev, data, data_len);
	cds_ssr_unprotect(__func__);

	return ret;
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * __wlan_hdd_cfg80211_keymgmt_set_key() - Store the Keys in the driver session
 * @wiphy: pointer to wireless wiphy structure.
 * @wdev: pointer to wireless_dev structure.
 * @data: Pointer to the Key data
 * @data_len:Length of the data passed
 *
 * This is called when wlan driver needs to save the keys received via
 * vendor specific command.
 *
 * Return: Return the Success or Failure code.
 */
static int __wlan_hdd_cfg80211_keymgmt_set_key(struct wiphy *wiphy,
					       struct wireless_dev *wdev,
					       const void *data, int data_len)
{
	uint8_t local_pmk[SIR_ROAM_SCAN_PSK_SIZE];
	struct net_device *dev = wdev->netdev;
	hdd_adapter_t *hdd_adapter_ptr = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *hdd_ctx_ptr;
	int status;

	ENTER_DEV(dev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	if ((data == NULL) || (data_len == 0) ||
			(data_len > SIR_ROAM_SCAN_PSK_SIZE)) {
		hdd_err("Invalid data");
		return -EINVAL;
	}

	hdd_ctx_ptr = WLAN_HDD_GET_CTX(hdd_adapter_ptr);
	if (!hdd_ctx_ptr) {
		hdd_err("HDD context is null");
		return -EINVAL;
	}

	status = wlan_hdd_validate_context(hdd_ctx_ptr);
	if (status)
		return status;

	sme_update_roam_key_mgmt_offload_enabled(hdd_ctx_ptr->hHal,
			hdd_adapter_ptr->sessionId,
			true);
	qdf_mem_zero(&local_pmk, SIR_ROAM_SCAN_PSK_SIZE);
	qdf_mem_copy(local_pmk, data, data_len);
	sme_roam_set_psk_pmk(WLAN_HDD_GET_HAL_CTX(hdd_adapter_ptr),
			hdd_adapter_ptr->sessionId, local_pmk, data_len);
	return 0;
}

/**
 * wlan_hdd_cfg80211_keymgmt_set_key() - Store the Keys in the driver session
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the Key data
 * @data_len:Length of the data passed
 *
 * This is called when wlan driver needs to save the keys received via
 * vendor specific command.
 *
 * Return:   Return the Success or Failure code.
 */
static int wlan_hdd_cfg80211_keymgmt_set_key(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data, int data_len)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_keymgmt_set_key(wiphy, wdev, data, data_len);
	cds_ssr_unprotect(__func__);

	return ret;
}
#endif

static const struct nla_policy qca_wlan_vendor_get_wifi_info_policy[
			QCA_WLAN_VENDOR_ATTR_WIFI_INFO_GET_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_WIFI_INFO_DRIVER_VERSION] = {.type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_WIFI_INFO_FIRMWARE_VERSION] = {.type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_WIFI_INFO_RADIO_INDEX] = {.type = NLA_U32 },
};

/**
 * __wlan_hdd_cfg80211_get_wifi_info() - Get the wifi driver related info
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * This is called when wlan driver needs to send wifi driver related info
 * (driver/fw version) to the user space application upon request.
 *
 * Return:   Return the Success or Failure code.
 */
static int
__wlan_hdd_cfg80211_get_wifi_info(struct wiphy *wiphy,
				  struct wireless_dev *wdev,
				  const void *data, int data_len)
{
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	struct nlattr *tb_vendor[QCA_WLAN_VENDOR_ATTR_WIFI_INFO_GET_MAX + 1];
	tSirVersionString driver_version;
	tSirVersionString firmware_version;
	uint32_t major_spid = 0, minor_spid = 0, siid = 0, crmid = 0;
	int status;
	struct sk_buff *reply_skb;
	uint32_t skb_len = 0, count = 0;

	ENTER_DEV(wdev->netdev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	status = wlan_hdd_validate_context(hdd_ctx);
	if (status)
		return status;

	if (nla_parse(tb_vendor, QCA_WLAN_VENDOR_ATTR_WIFI_INFO_GET_MAX, data,
		      data_len, qca_wlan_vendor_get_wifi_info_policy)) {
		hdd_err("WIFI_INFO_GET NL CMD parsing failed");
		return -EINVAL;
	}

	if (tb_vendor[QCA_WLAN_VENDOR_ATTR_WIFI_INFO_DRIVER_VERSION]) {
		hdd_err("Rcvd req for Driver version");
		strlcpy(driver_version, QWLAN_VERSIONSTR,
			sizeof(driver_version));
		skb_len += strlen(driver_version) + 1;
		count++;
	}

	if (tb_vendor[QCA_WLAN_VENDOR_ATTR_WIFI_INFO_FIRMWARE_VERSION]) {
		hdd_info("Rcvd req for FW version");
		hdd_get_fw_version(hdd_ctx, &major_spid, &minor_spid, &siid,
				   &crmid);
		snprintf(firmware_version, sizeof(firmware_version),
			 "%d:%d:%d:%d", major_spid, minor_spid, siid, crmid);
		skb_len += strlen(firmware_version) + 1;
		count++;
	}

	if (count == 0) {
		hdd_err("unknown attribute in get_wifi_info request");
		return -EINVAL;
	}

	skb_len += (NLA_HDRLEN * count) + NLMSG_HDRLEN;
	reply_skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, skb_len);

	if (!reply_skb) {
		hdd_err("cfg80211_vendor_cmd_alloc_reply_skb failed");
		return -ENOMEM;
	}

	if (tb_vendor[QCA_WLAN_VENDOR_ATTR_WIFI_INFO_DRIVER_VERSION]) {
		if (nla_put_string(reply_skb,
			    QCA_WLAN_VENDOR_ATTR_WIFI_INFO_DRIVER_VERSION,
			    driver_version))
			goto error_nla_fail;
	}

	if (tb_vendor[QCA_WLAN_VENDOR_ATTR_WIFI_INFO_DRIVER_VERSION]) {
		if (nla_put_string(reply_skb,
			    QCA_WLAN_VENDOR_ATTR_WIFI_INFO_FIRMWARE_VERSION,
			    firmware_version))
			goto error_nla_fail;
	}

	if (tb_vendor[QCA_WLAN_VENDOR_ATTR_WIFI_INFO_RADIO_INDEX]) {
		if (nla_put_u32(reply_skb,
				QCA_WLAN_VENDOR_ATTR_WIFI_INFO_RADIO_INDEX,
				hdd_ctx->radio_index))
			goto error_nla_fail;
	}

	return cfg80211_vendor_cmd_reply(reply_skb);

error_nla_fail:
	hdd_err("nla put fail");
	kfree_skb(reply_skb);
	return -EINVAL;
}

/**
 * wlan_hdd_cfg80211_get_wifi_info() - Get the wifi driver related info
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * This is called when wlan driver needs to send wifi driver related info
 * (driver/fw version) to the user space application upon request.
 *
 * Return:   Return the Success or Failure code.
 */
static int
wlan_hdd_cfg80211_get_wifi_info(struct wiphy *wiphy,
		struct wireless_dev *wdev,
		const void *data, int data_len)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_get_wifi_info(wiphy, wdev, data, data_len);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * __wlan_hdd_cfg80211_get_logger_supp_feature() - Get the wifi logger features
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * This is called by userspace to know the supported logger features
 *
 * Return:   Return the Success or Failure code.
 */
static int
__wlan_hdd_cfg80211_get_logger_supp_feature(struct wiphy *wiphy,
		struct wireless_dev *wdev,
		const void *data, int data_len)
{
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	int status;
	uint32_t features;
	struct sk_buff *reply_skb = NULL;

	ENTER_DEV(wdev->netdev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	status = wlan_hdd_validate_context(hdd_ctx);
	if (status)
		return status;

	features = 0;

	if (hdd_is_memdump_supported())
		features |= WIFI_LOGGER_MEMORY_DUMP_SUPPORTED;
	features |= WIFI_LOGGER_PER_PACKET_TX_RX_STATUS_SUPPORTED;
	features |= WIFI_LOGGER_CONNECT_EVENT_SUPPORTED;
	features |= WIFI_LOGGER_WAKE_LOCK_SUPPORTED;

	reply_skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy,
			sizeof(uint32_t) + NLA_HDRLEN + NLMSG_HDRLEN);
	if (!reply_skb) {
		hdd_err("cfg80211_vendor_cmd_alloc_reply_skb failed");
		return -ENOMEM;
	}

	hdd_notice("Supported logger features: 0x%0x", features);
	if (nla_put_u32(reply_skb, QCA_WLAN_VENDOR_ATTR_LOGGER_SUPPORTED,
				   features)) {
		hdd_err("nla put fail");
		kfree_skb(reply_skb);
		return -EINVAL;
	}

	return cfg80211_vendor_cmd_reply(reply_skb);
}

/**
 * wlan_hdd_cfg80211_get_logger_supp_feature() - Get the wifi logger features
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * This is called by userspace to know the supported logger features
 *
 * Return:   Return the Success or Failure code.
 */
static int
wlan_hdd_cfg80211_get_logger_supp_feature(struct wiphy *wiphy,
		struct wireless_dev *wdev,
		const void *data, int data_len)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_get_logger_supp_feature(wiphy, wdev,
							  data, data_len);
	cds_ssr_unprotect(__func__);

	return ret;
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * wlan_hdd_send_roam_auth_event() - Send the roamed and authorized event
 * @hdd_ctx_ptr: pointer to HDD Context.
 * @bssid: pointer to bssid of roamed AP.
 * @req_rsn_ie: Pointer to request RSN IE
 * @req_rsn_len: Length of the request RSN IE
 * @rsp_rsn_ie: Pointer to response RSN IE
 * @rsp_rsn_len: Length of the response RSN IE
 * @roam_info_ptr: Pointer to the roaming related information
 *
 * This is called when wlan driver needs to send the roaming and
 * authorization information after roaming.
 *
 * The information that would be sent is the request RSN IE, response
 * RSN IE and BSSID of the newly roamed AP.
 *
 * If the Authorized status is authenticated, then additional parameters
 * like PTK's KCK and KEK and Replay Counter would also be passed to the
 * supplicant.
 *
 * The supplicant upon receiving this event would ignore the legacy
 * cfg80211_roamed call and use the entire information from this event.
 * The cfg80211_roamed should still co-exist since the kernel will
 * make use of the parameters even if the supplicant ignores it.
 *
 * Return: Return the Success or Failure code.
 */
int wlan_hdd_send_roam_auth_event(hdd_context_t *hdd_ctx_ptr, uint8_t *bssid,
		uint8_t *req_rsn_ie, uint32_t req_rsn_len, uint8_t *rsp_rsn_ie,
		uint32_t rsp_rsn_len, tCsrRoamInfo *roam_info_ptr)
{
	struct sk_buff *skb = NULL;
	eCsrAuthType auth_type;
	ENTER();

	if (wlan_hdd_validate_context(hdd_ctx_ptr))
		return -EINVAL;

	if (!roaming_offload_enabled(hdd_ctx_ptr) ||
			!roam_info_ptr->roamSynchInProgress)
		return 0;

	skb = cfg80211_vendor_event_alloc(hdd_ctx_ptr->wiphy,
			NULL,
			ETH_ALEN + req_rsn_len + rsp_rsn_len +
			sizeof(uint8_t) + SIR_REPLAY_CTR_LEN +
			SIR_KCK_KEY_LEN + SIR_KCK_KEY_LEN +
			sizeof(uint8_t) + (8 * NLMSG_HDRLEN),
			QCA_NL80211_VENDOR_SUBCMD_KEY_MGMT_ROAM_AUTH_INDEX,
			GFP_KERNEL);

	if (!skb) {
		hdd_err("cfg80211_vendor_event_alloc failed");
		return -EINVAL;
	}

	if (nla_put(skb, QCA_WLAN_VENDOR_ATTR_ROAM_AUTH_BSSID,
				ETH_ALEN, bssid) ||
			nla_put(skb, QCA_WLAN_VENDOR_ATTR_ROAM_AUTH_REQ_IE,
				req_rsn_len, req_rsn_ie) ||
			nla_put(skb, QCA_WLAN_VENDOR_ATTR_ROAM_AUTH_RESP_IE,
				rsp_rsn_len, rsp_rsn_ie)) {
		hdd_err("nla put fail");
		goto nla_put_failure;
	}
	hdd_debug("Auth Status = %d", roam_info_ptr->synchAuthStatus);
	if (roam_info_ptr->synchAuthStatus ==
			CSR_ROAM_AUTH_STATUS_AUTHENTICATED) {
		hdd_debug("Include Auth Params TLV's");
		if (nla_put_u8(skb,
			QCA_WLAN_VENDOR_ATTR_ROAM_AUTH_AUTHORIZED, true)) {
			hdd_err("nla put fail");
			goto nla_put_failure;
		}
		auth_type = roam_info_ptr->u.pConnectedProfile->AuthType;
		/* if FT or CCKM connection: dont send replay counter */
		if (auth_type != eCSR_AUTH_TYPE_FT_RSN &&
		    auth_type != eCSR_AUTH_TYPE_FT_RSN_PSK &&
		    auth_type != eCSR_AUTH_TYPE_CCKM_WPA &&
		    auth_type != eCSR_AUTH_TYPE_CCKM_RSN &&
		    nla_put(skb,
			    QCA_WLAN_VENDOR_ATTR_ROAM_AUTH_KEY_REPLAY_CTR,
			    SIR_REPLAY_CTR_LEN,
			    roam_info_ptr->replay_ctr)) {
			hdd_err("non FT/non CCKM connection.");
			hdd_err("failed to send replay counter.");
			goto nla_put_failure;
		}
		if (nla_put(skb,
			QCA_WLAN_VENDOR_ATTR_ROAM_AUTH_PTK_KCK,
			SIR_KCK_KEY_LEN, roam_info_ptr->kck) ||
		    nla_put(skb,
			QCA_WLAN_VENDOR_ATTR_ROAM_AUTH_PTK_KEK,
			SIR_KEK_KEY_LEN, roam_info_ptr->kek)) {
			hdd_err("nla put fail");
			goto nla_put_failure;
		}
	} else {
		hdd_debug("No Auth Params TLV's");
		if (nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_ROAM_AUTH_AUTHORIZED,
					false)) {
			hdd_err("nla put fail");
			goto nla_put_failure;
		}
	}

	hdd_debug("Subnet Change Status = %d",
		roam_info_ptr->subnet_change_status);

	/*
	 * Add subnet change status if subnet has changed
	 * 0 = unchanged
	 * 1 = changed
	 * 2 = unknown
	 */
	if (roam_info_ptr->subnet_change_status) {
		if (nla_put_u8(skb,
				QCA_WLAN_VENDOR_ATTR_ROAM_AUTH_SUBNET_STATUS,
				roam_info_ptr->subnet_change_status)) {
			hdd_err("nla put fail");
			goto nla_put_failure;
		}
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);
	return 0;

nla_put_failure:
	kfree_skb(skb);
	return -EINVAL;
}
#endif

static const struct nla_policy
wlan_hdd_wifi_config_policy[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1] = {

	[QCA_WLAN_VENDOR_ATTR_CONFIG_MODULATED_DTIM] = {.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_CONFIG_STATS_AVG_FACTOR] = {.type = NLA_U16 },
	[QCA_WLAN_VENDOR_ATTR_CONFIG_GUARD_TIME] = {.type = NLA_U32 },
};


/**
 * __wlan_hdd_cfg80211_wifi_configuration_set() - Wifi configuration
 * vendor command
 *
 * @wiphy: wiphy device pointer
 * @wdev: wireless device pointer
 * @data: Vendor command data buffer
 * @data_len: Buffer length
 *
 * Handles QCA_WLAN_VENDOR_ATTR_CONFIG_MAX.
 *
 * Return: Error code.
 */
static int
__wlan_hdd_cfg80211_wifi_configuration_set(struct wiphy *wiphy,
					   struct wireless_dev *wdev,
					   const void *data,
					   int data_len)
{
	struct net_device *dev = wdev->netdev;
	hdd_adapter_t *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *hdd_ctx  = wiphy_priv(wiphy);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];
	int ret_val = 0;
	u32 modulated_dtim;
	u16 stats_avg_factor;
	u32 guard_time;
	u32 ftm_capab;
	QDF_STATUS status;

	ENTER_DEV(dev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	ret_val = wlan_hdd_validate_context(hdd_ctx);
	if (ret_val)
		return ret_val;

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
		      data, data_len,
		      wlan_hdd_wifi_config_policy)) {
		hdd_err("invalid attr");
		return -EINVAL;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_FINE_TIME_MEASUREMENT]) {
		ftm_capab = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_CONFIG_FINE_TIME_MEASUREMENT]);
		hdd_ctx->config->fine_time_meas_cap =
			hdd_ctx->fine_time_meas_cap_target & ftm_capab;
		sme_update_fine_time_measurement_capab(hdd_ctx->hHal,
			hdd_ctx->config->fine_time_meas_cap);
		hdd_info("FTM capability: user value: 0x%x, target value: 0x%x, final value: 0x%x",
			 ftm_capab, hdd_ctx->fine_time_meas_cap_target,
			 hdd_ctx->config->fine_time_meas_cap);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MODULATED_DTIM]) {
		modulated_dtim = nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MODULATED_DTIM]);

		status = sme_configure_modulated_dtim(hdd_ctx->hHal,
						      adapter->sessionId,
						      modulated_dtim);

		if (QDF_STATUS_SUCCESS != status)
			ret_val = -EPERM;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_STATS_AVG_FACTOR]) {
		stats_avg_factor = nla_get_u16(
			tb[QCA_WLAN_VENDOR_ATTR_CONFIG_STATS_AVG_FACTOR]);
		status = sme_configure_stats_avg_factor(hdd_ctx->hHal,
							adapter->sessionId,
							stats_avg_factor);

		if (QDF_STATUS_SUCCESS != status)
			ret_val = -EPERM;
	}


	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GUARD_TIME]) {
		guard_time = nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GUARD_TIME]);
		status = sme_configure_guard_time(hdd_ctx->hHal,
						  adapter->sessionId,
						  guard_time);

		if (QDF_STATUS_SUCCESS != status)
			ret_val = -EPERM;
	}

	return ret_val;
}

/**
 * wlan_hdd_cfg80211_wifi_configuration_set() - Wifi configuration
 * vendor command
 *
 * @wiphy: wiphy device pointer
 * @wdev: wireless device pointer
 * @data: Vendor command data buffer
 * @data_len: Buffer length
 *
 * Handles QCA_WLAN_VENDOR_ATTR_CONFIG_MAX.
 *
 * Return: EOK or other error codes.
 */
static int wlan_hdd_cfg80211_wifi_configuration_set(struct wiphy *wiphy,
						    struct wireless_dev *wdev,
						    const void *data,
						    int data_len)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_wifi_configuration_set(wiphy, wdev,
							 data, data_len);
	cds_ssr_unprotect(__func__);

	return ret;
}

static const struct
nla_policy
qca_wlan_vendor_wifi_logger_start_policy
[QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_START_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_RING_ID]
		= {.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_VERBOSE_LEVEL]
		= {.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_FLAGS]
		= {.type = NLA_U32 },
};

/**
 * __wlan_hdd_cfg80211_wifi_logger_start() - This function is used to enable
 * or disable the collection of packet statistics from the firmware
 * @wiphy:    WIPHY structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of the data received
 *
 * This function enables or disables the collection of packet statistics from
 * the firmware
 *
 * Return: 0 on success and errno on failure
 */
static int __wlan_hdd_cfg80211_wifi_logger_start(struct wiphy *wiphy,
		struct wireless_dev *wdev,
		const void *data,
		int data_len)
{
	QDF_STATUS status;
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_START_MAX + 1];
	struct sir_wifi_start_log start_log;

	ENTER_DEV(wdev->netdev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	status = wlan_hdd_validate_context(hdd_ctx);
	if (status)
		return status;


	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_START_MAX,
				data, data_len,
				qca_wlan_vendor_wifi_logger_start_policy)) {
		hdd_err("Invalid attribute");
		return -EINVAL;
	}

	/* Parse and fetch ring id */
	if (!tb[QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_RING_ID]) {
		hdd_err("attr ATTR failed");
		return -EINVAL;
	}
	start_log.ring_id = nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_RING_ID]);
	hdd_info("Ring ID=%d", start_log.ring_id);

	/* Parse and fetch verbose level */
	if (!tb[QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_VERBOSE_LEVEL]) {
		hdd_err("attr verbose_level failed");
		return -EINVAL;
	}
	start_log.verbose_level = nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_VERBOSE_LEVEL]);
	hdd_info("verbose_level=%d", start_log.verbose_level);

	/* Parse and fetch flag */
	if (!tb[QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_FLAGS]) {
		hdd_err("attr flag failed");
		return -EINVAL;
	}
	start_log.flag = nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_FLAGS]);
	hdd_info("flag=%d", start_log.flag);

	cds_set_ring_log_level(start_log.ring_id, start_log.verbose_level);

	if (start_log.ring_id == RING_ID_WAKELOCK) {
		/* Start/stop wakelock events */
		if (start_log.verbose_level > WLAN_LOG_LEVEL_OFF)
			cds_set_wakelock_logging(true);
		else
			cds_set_wakelock_logging(false);
		return 0;
	}

	status = sme_wifi_start_logger(hdd_ctx->hHal, start_log);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("sme_wifi_start_logger failed(err=%d)",
				status);
		return -EINVAL;
	}
	return 0;
}

/**
 * wlan_hdd_cfg80211_wifi_logger_start() - Wrapper function used to enable
 * or disable the collection of packet statistics from the firmware
 * @wiphy:    WIPHY structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of the data received
 *
 * This function is used to enable or disable the collection of packet
 * statistics from the firmware
 *
 * Return: 0 on success and errno on failure
 */
static int wlan_hdd_cfg80211_wifi_logger_start(struct wiphy *wiphy,
		struct wireless_dev *wdev,
		const void *data,
		int data_len)
{
	int ret = 0;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_wifi_logger_start(wiphy,
			wdev, data, data_len);
	cds_ssr_unprotect(__func__);

	return ret;
}

static const struct
nla_policy
qca_wlan_vendor_wifi_logger_get_ring_data_policy
[QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_GET_RING_DATA_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_GET_RING_DATA_ID]
		= {.type = NLA_U32 },
};

/**
 * __wlan_hdd_cfg80211_wifi_logger_get_ring_data() - Flush per packet stats
 * @wiphy:    WIPHY structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of the data received
 *
 * This function is used to flush or retrieve the per packet statistics from
 * the driver
 *
 * Return: 0 on success and errno on failure
 */
static int __wlan_hdd_cfg80211_wifi_logger_get_ring_data(struct wiphy *wiphy,
		struct wireless_dev *wdev,
		const void *data,
		int data_len)
{
	QDF_STATUS status;
	uint32_t ring_id;
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	struct nlattr *tb
		[QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_GET_RING_DATA_MAX + 1];

	ENTER_DEV(wdev->netdev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	status = wlan_hdd_validate_context(hdd_ctx);
	if (status)
		return status;

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_GET_RING_DATA_MAX,
			data, data_len,
			qca_wlan_vendor_wifi_logger_get_ring_data_policy)) {
		hdd_err("Invalid attribute");
		return -EINVAL;
	}

	/* Parse and fetch ring id */
	if (!tb[QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_GET_RING_DATA_ID]) {
		hdd_err("attr ATTR failed");
		return -EINVAL;
	}

	ring_id = nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_GET_RING_DATA_ID]);

	if (ring_id == RING_ID_PER_PACKET_STATS) {
		wlan_logging_set_per_pkt_stats();
		hdd_notice("Flushing/Retrieving packet stats");
	}

	hdd_notice("Bug report triggered by framework");

	status = cds_flush_logs(WLAN_LOG_TYPE_NON_FATAL,
			WLAN_LOG_INDICATOR_FRAMEWORK,
			WLAN_LOG_REASON_CODE_UNUSED,
			true, false);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("Failed to trigger bug report");
		return -EINVAL;
	}

	return 0;
}

/**
 * wlan_hdd_cfg80211_wifi_logger_get_ring_data() - Wrapper to flush packet stats
 * @wiphy:    WIPHY structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of the data received
 *
 * This function is used to flush or retrieve the per packet statistics from
 * the driver
 *
 * Return: 0 on success and errno on failure
 */
static int wlan_hdd_cfg80211_wifi_logger_get_ring_data(struct wiphy *wiphy,
		struct wireless_dev *wdev,
		const void *data,
		int data_len)
{
	int ret = 0;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_wifi_logger_get_ring_data(wiphy,
			wdev, data, data_len);
	cds_ssr_unprotect(__func__);

	return ret;
}

#ifdef WLAN_FEATURE_OFFLOAD_PACKETS
/**
 * hdd_map_req_id_to_pattern_id() - map request id to pattern id
 * @hdd_ctx: HDD context
 * @request_id: [input] request id
 * @pattern_id: [output] pattern id
 *
 * This function loops through request id to pattern id array
 * if the slot is available, store the request id and return pattern id
 * if entry exists, return the pattern id
 *
 * Return: 0 on success and errno on failure
 */
static int hdd_map_req_id_to_pattern_id(hdd_context_t *hdd_ctx,
					  uint32_t request_id,
					  uint8_t *pattern_id)
{
	uint32_t i;

	mutex_lock(&hdd_ctx->op_ctx.op_lock);
	for (i = 0; i < MAXNUM_PERIODIC_TX_PTRNS; i++) {
		if (hdd_ctx->op_ctx.op_table[i].request_id == MAX_REQUEST_ID) {
			hdd_ctx->op_ctx.op_table[i].request_id = request_id;
			*pattern_id = hdd_ctx->op_ctx.op_table[i].pattern_id;
			mutex_unlock(&hdd_ctx->op_ctx.op_lock);
			return 0;
		} else if (hdd_ctx->op_ctx.op_table[i].request_id ==
					request_id) {
			*pattern_id = hdd_ctx->op_ctx.op_table[i].pattern_id;
			mutex_unlock(&hdd_ctx->op_ctx.op_lock);
			return 0;
		}
	}
	mutex_unlock(&hdd_ctx->op_ctx.op_lock);
	return -EINVAL;
}

/**
 * hdd_unmap_req_id_to_pattern_id() - unmap request id to pattern id
 * @hdd_ctx: HDD context
 * @request_id: [input] request id
 * @pattern_id: [output] pattern id
 *
 * This function loops through request id to pattern id array
 * reset request id to 0 (slot available again) and
 * return pattern id
 *
 * Return: 0 on success and errno on failure
 */
static int hdd_unmap_req_id_to_pattern_id(hdd_context_t *hdd_ctx,
					  uint32_t request_id,
					  uint8_t *pattern_id)
{
	uint32_t i;

	mutex_lock(&hdd_ctx->op_ctx.op_lock);
	for (i = 0; i < MAXNUM_PERIODIC_TX_PTRNS; i++) {
		if (hdd_ctx->op_ctx.op_table[i].request_id == request_id) {
			hdd_ctx->op_ctx.op_table[i].request_id = MAX_REQUEST_ID;
			*pattern_id = hdd_ctx->op_ctx.op_table[i].pattern_id;
			mutex_unlock(&hdd_ctx->op_ctx.op_lock);
			return 0;
		}
	}
	mutex_unlock(&hdd_ctx->op_ctx.op_lock);
	return -EINVAL;
}


/*
 * define short names for the global vendor params
 * used by __wlan_hdd_cfg80211_offloaded_packets()
 */
#define PARAM_MAX QCA_WLAN_VENDOR_ATTR_OFFLOADED_PACKETS_MAX
#define PARAM_REQUEST_ID \
		QCA_WLAN_VENDOR_ATTR_OFFLOADED_PACKETS_REQUEST_ID
#define PARAM_CONTROL \
		QCA_WLAN_VENDOR_ATTR_OFFLOADED_PACKETS_SENDING_CONTROL
#define PARAM_IP_PACKET \
		QCA_WLAN_VENDOR_ATTR_OFFLOADED_PACKETS_IP_PACKET_DATA
#define PARAM_SRC_MAC_ADDR \
		QCA_WLAN_VENDOR_ATTR_OFFLOADED_PACKETS_SRC_MAC_ADDR
#define PARAM_DST_MAC_ADDR \
		QCA_WLAN_VENDOR_ATTR_OFFLOADED_PACKETS_DST_MAC_ADDR
#define PARAM_PERIOD QCA_WLAN_VENDOR_ATTR_OFFLOADED_PACKETS_PERIOD

/**
 * wlan_hdd_add_tx_ptrn() - add tx pattern
 * @adapter: adapter pointer
 * @hdd_ctx: hdd context
 * @tb: nl attributes
 *
 * This function reads the NL attributes and forms a AddTxPtrn message
 * posts it to SME.
 *
 */
static int
wlan_hdd_add_tx_ptrn(hdd_adapter_t *adapter, hdd_context_t *hdd_ctx,
			struct nlattr **tb)
{
	struct sSirAddPeriodicTxPtrn *add_req;
	QDF_STATUS status;
	uint32_t request_id, ret, len;
	uint8_t pattern_id = 0;
	struct qdf_mac_addr dst_addr;
	uint16_t eth_type = htons(ETH_P_IP);

	if (!hdd_conn_is_connected(WLAN_HDD_GET_STATION_CTX_PTR(adapter))) {
		hdd_err("Not in Connected state!");
		return -ENOTSUPP;
	}

	add_req = qdf_mem_malloc(sizeof(*add_req));
	if (!add_req) {
		hdd_err("memory allocation failed");
		return -ENOMEM;
	}

	/* Parse and fetch request Id */
	if (!tb[PARAM_REQUEST_ID]) {
		hdd_err("attr request id failed");
		goto fail;
	}

	request_id = nla_get_u32(tb[PARAM_REQUEST_ID]);
	if (request_id == MAX_REQUEST_ID) {
		hdd_err("request_id cannot be MAX");
		return -EINVAL;
	}
	hdd_notice("Request Id: %u", request_id);

	if (!tb[PARAM_PERIOD]) {
		hdd_err("attr period failed");
		goto fail;
	}
	add_req->usPtrnIntervalMs = nla_get_u32(tb[PARAM_PERIOD]);
	hdd_notice("Period: %u ms", add_req->usPtrnIntervalMs);
	if (add_req->usPtrnIntervalMs == 0) {
		hdd_err("Invalid interval zero, return failure");
		goto fail;
	}

	if (!tb[PARAM_SRC_MAC_ADDR]) {
		hdd_err("attr source mac address failed");
		goto fail;
	}
	nla_memcpy(add_req->mac_address.bytes, tb[PARAM_SRC_MAC_ADDR],
			QDF_MAC_ADDR_SIZE);
	hdd_notice("input src mac address: "MAC_ADDRESS_STR,
			MAC_ADDR_ARRAY(add_req->mac_address.bytes));

	if (!qdf_is_macaddr_equal(&add_req->mac_address,
				  &adapter->macAddressCurrent)) {
		hdd_err("input src mac address and connected ap bssid are different");
		goto fail;
	}

	if (!tb[PARAM_DST_MAC_ADDR]) {
		hdd_err("attr dst mac address failed");
		goto fail;
	}
	nla_memcpy(dst_addr.bytes, tb[PARAM_DST_MAC_ADDR], QDF_MAC_ADDR_SIZE);
	hdd_notice("input dst mac address: "MAC_ADDRESS_STR,
			MAC_ADDR_ARRAY(dst_addr.bytes));

	if (!tb[PARAM_IP_PACKET]) {
		hdd_err("attr ip packet failed");
		goto fail;
	}
	add_req->ucPtrnSize = nla_len(tb[PARAM_IP_PACKET]);
	hdd_notice("IP packet len: %u", add_req->ucPtrnSize);

	if (add_req->ucPtrnSize < 0 ||
		add_req->ucPtrnSize > (PERIODIC_TX_PTRN_MAX_SIZE -
					ETH_HLEN)) {
		hdd_err("Invalid IP packet len: %d",
				add_req->ucPtrnSize);
		goto fail;
	}

	len = 0;
	qdf_mem_copy(&add_req->ucPattern[0], dst_addr.bytes, QDF_MAC_ADDR_SIZE);
	len += QDF_MAC_ADDR_SIZE;
	qdf_mem_copy(&add_req->ucPattern[len], add_req->mac_address.bytes,
			QDF_MAC_ADDR_SIZE);
	len += QDF_MAC_ADDR_SIZE;
	qdf_mem_copy(&add_req->ucPattern[len], &eth_type, 2);
	len += 2;

	/*
	 * This is the IP packet, add 14 bytes Ethernet (802.3) header
	 * ------------------------------------------------------------
	 * | 14 bytes Ethernet (802.3) header | IP header and payload |
	 * ------------------------------------------------------------
	 */
	qdf_mem_copy(&add_req->ucPattern[len],
			nla_data(tb[PARAM_IP_PACKET]),
			add_req->ucPtrnSize);
	add_req->ucPtrnSize += len;

	ret = hdd_map_req_id_to_pattern_id(hdd_ctx, request_id, &pattern_id);
	if (ret) {
		hdd_warn("req id to pattern id failed (ret=%d)", ret);
		goto fail;
	}
	add_req->ucPtrnId = pattern_id;
	hdd_notice("pattern id: %d", add_req->ucPtrnId);

	status = sme_add_periodic_tx_ptrn(hdd_ctx->hHal, add_req);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("sme_add_periodic_tx_ptrn failed (err=%d)", status);
		goto fail;
	}

	EXIT();
	qdf_mem_free(add_req);
	return 0;

fail:
	qdf_mem_free(add_req);
	return -EINVAL;
}

/**
 * wlan_hdd_del_tx_ptrn() - delete tx pattern
 * @adapter: adapter pointer
 * @hdd_ctx: hdd context
 * @tb: nl attributes
 *
 * This function reads the NL attributes and forms a DelTxPtrn message
 * posts it to SME.
 *
 */
static int
wlan_hdd_del_tx_ptrn(hdd_adapter_t *adapter, hdd_context_t *hdd_ctx,
			struct nlattr **tb)
{
	struct sSirDelPeriodicTxPtrn *del_req;
	QDF_STATUS status;
	uint32_t request_id, ret;
	uint8_t pattern_id = 0;

	/* Parse and fetch request Id */
	if (!tb[PARAM_REQUEST_ID]) {
		hdd_err("attr request id failed");
		return -EINVAL;
	}
	request_id = nla_get_u32(tb[PARAM_REQUEST_ID]);
	if (request_id == MAX_REQUEST_ID) {
		hdd_err("request_id cannot be MAX");
		return -EINVAL;
	}

	ret = hdd_unmap_req_id_to_pattern_id(hdd_ctx, request_id, &pattern_id);
	if (ret) {
		hdd_warn("req id to pattern id failed (ret=%d)", ret);
		return -EINVAL;
	}

	del_req = qdf_mem_malloc(sizeof(*del_req));
	if (!del_req) {
		hdd_err("memory allocation failed");
		return -ENOMEM;
	}

	qdf_copy_macaddr(&del_req->mac_address, &adapter->macAddressCurrent);
	hdd_info(MAC_ADDRESS_STR, MAC_ADDR_ARRAY(del_req->mac_address.bytes));
	del_req->ucPtrnId = pattern_id;
	hdd_notice("Request Id: %u Pattern id: %d",
			 request_id, del_req->ucPtrnId);

	status = sme_del_periodic_tx_ptrn(hdd_ctx->hHal, del_req);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("sme_del_periodic_tx_ptrn failed (err=%d)", status);
		goto fail;
	}

	EXIT();
	qdf_mem_free(del_req);
	return 0;

fail:
	qdf_mem_free(del_req);
	return -EINVAL;
}


/**
 * __wlan_hdd_cfg80211_offloaded_packets() - send offloaded packets
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
static int
__wlan_hdd_cfg80211_offloaded_packets(struct wiphy *wiphy,
				     struct wireless_dev *wdev,
				     const void *data,
				     int data_len)
{
	struct net_device *dev = wdev->netdev;
	hdd_adapter_t *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	struct nlattr *tb[PARAM_MAX + 1];
	uint8_t control;
	int ret;
	static const struct nla_policy policy[PARAM_MAX + 1] = {
			[PARAM_REQUEST_ID] = { .type = NLA_U32 },
			[PARAM_CONTROL] = { .type = NLA_U32 },
			[PARAM_SRC_MAC_ADDR] = { .type = NLA_BINARY,
						.len = QDF_MAC_ADDR_SIZE },
			[PARAM_DST_MAC_ADDR] = { .type = NLA_BINARY,
						.len = QDF_MAC_ADDR_SIZE },
			[PARAM_PERIOD] = { .type = NLA_U32 },
	};

	ENTER_DEV(dev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (!sme_is_feature_supported_by_fw(WLAN_PERIODIC_TX_PTRN)) {
		hdd_err("Periodic Tx Pattern Offload feature is not supported in FW!");
		return -ENOTSUPP;
	}

	if (nla_parse(tb, PARAM_MAX, data, data_len, policy)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}

	if (!tb[PARAM_CONTROL]) {
		hdd_err("attr control failed");
		return -EINVAL;
	}
	control = nla_get_u32(tb[PARAM_CONTROL]);
	hdd_notice("Control: %d", control);

	if (control == WLAN_START_OFFLOADED_PACKETS)
		return wlan_hdd_add_tx_ptrn(adapter, hdd_ctx, tb);
	else if (control == WLAN_STOP_OFFLOADED_PACKETS)
		return wlan_hdd_del_tx_ptrn(adapter, hdd_ctx, tb);
	else {
		hdd_err("Invalid control: %d", control);
		return -EINVAL;
	}
}

/*
 * done with short names for the global vendor params
 * used by __wlan_hdd_cfg80211_offloaded_packets()
 */
#undef PARAM_MAX
#undef PARAM_REQUEST_ID
#undef PARAM_CONTROL
#undef PARAM_IP_PACKET
#undef PARAM_SRC_MAC_ADDR
#undef PARAM_DST_MAC_ADDR
#undef PARAM_PERIOD

/**
 * wlan_hdd_cfg80211_offloaded_packets() - Wrapper to offload packets
 * @wiphy:    wiphy structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of @data
 *
 * Return: 0 on success; errno on failure
 */
static int wlan_hdd_cfg80211_offloaded_packets(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len)
{
	int ret = 0;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_offloaded_packets(wiphy,
					wdev, data, data_len);
	cds_ssr_unprotect(__func__);

	return ret;
}
#endif

/*
 * define short names for the global vendor params
 * used by __wlan_hdd_cfg80211_monitor_rssi()
 */
#define PARAM_MAX QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_MAX
#define PARAM_REQUEST_ID QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_REQUEST_ID
#define PARAM_CONTROL QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_CONTROL
#define PARAM_MIN_RSSI QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_MIN_RSSI
#define PARAM_MAX_RSSI QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_MAX_RSSI

/**
 * __wlan_hdd_cfg80211_monitor_rssi() - monitor rssi
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
static int
__wlan_hdd_cfg80211_monitor_rssi(struct wiphy *wiphy,
				 struct wireless_dev *wdev,
				 const void *data,
				 int data_len)
{
	struct net_device *dev = wdev->netdev;
	hdd_adapter_t *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	struct nlattr *tb[PARAM_MAX + 1];
	struct rssi_monitor_req req;
	QDF_STATUS status;
	int ret;
	uint32_t control;
	static const struct nla_policy policy[PARAM_MAX + 1] = {
			[PARAM_REQUEST_ID] = { .type = NLA_U32 },
			[PARAM_CONTROL] = { .type = NLA_U32 },
			[PARAM_MIN_RSSI] = { .type = NLA_S8 },
			[PARAM_MAX_RSSI] = { .type = NLA_S8 },
	};

	ENTER_DEV(dev);

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (!hdd_conn_is_connected(WLAN_HDD_GET_STATION_CTX_PTR(adapter))) {
		hdd_err("Not in Connected state!");
		return -ENOTSUPP;
	}

	if (nla_parse(tb, PARAM_MAX, data, data_len, policy)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}

	if (!tb[PARAM_REQUEST_ID]) {
		hdd_err("attr request id failed");
		return -EINVAL;
	}

	if (!tb[PARAM_CONTROL]) {
		hdd_err("attr control failed");
		return -EINVAL;
	}

	req.request_id = nla_get_u32(tb[PARAM_REQUEST_ID]);
	req.session_id = adapter->sessionId;
	control = nla_get_u32(tb[PARAM_CONTROL]);

	if (control == QCA_WLAN_RSSI_MONITORING_START) {
		req.control = true;
		if (!tb[PARAM_MIN_RSSI]) {
			hdd_err("attr min rssi failed");
			return -EINVAL;
		}

		if (!tb[PARAM_MAX_RSSI]) {
			hdd_err("attr max rssi failed");
			return -EINVAL;
		}

		req.min_rssi = nla_get_s8(tb[PARAM_MIN_RSSI]);
		req.max_rssi = nla_get_s8(tb[PARAM_MAX_RSSI]);

		if (!(req.min_rssi < req.max_rssi)) {
			hdd_warn("min_rssi: %d must be less than max_rssi: %d",
					req.min_rssi, req.max_rssi);
			return -EINVAL;
		}
		hdd_notice("Min_rssi: %d Max_rssi: %d",
			req.min_rssi, req.max_rssi);

	} else if (control == QCA_WLAN_RSSI_MONITORING_STOP)
		req.control = false;
	else {
		hdd_err("Invalid control cmd: %d", control);
		return -EINVAL;
	}
	hdd_notice("Request Id: %u Session_id: %d Control: %d",
			req.request_id, req.session_id, req.control);

	status = sme_set_rssi_monitoring(hdd_ctx->hHal, &req);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("sme_set_rssi_monitoring failed(err=%d)", status);
		return -EINVAL;
	}

	return 0;
}

/*
 * done with short names for the global vendor params
 * used by __wlan_hdd_cfg80211_monitor_rssi()
 */
#undef PARAM_MAX
#undef PARAM_CONTROL
#undef PARAM_REQUEST_ID
#undef PARAM_MAX_RSSI
#undef PARAM_MIN_RSSI

/**
 * wlan_hdd_cfg80211_monitor_rssi() - SSR wrapper to rssi monitoring
 * @wiphy:    wiphy structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of @data
 *
 * Return: 0 on success; errno on failure
 */
static int
wlan_hdd_cfg80211_monitor_rssi(struct wiphy *wiphy, struct wireless_dev *wdev,
			       const void *data, int data_len)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_monitor_rssi(wiphy, wdev, data, data_len);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * hdd_rssi_threshold_breached() - rssi breached NL event
 * @hddctx: HDD context
 * @data: rssi breached event data
 *
 * This function reads the rssi breached event %data and fill in the skb with
 * NL attributes and send up the NL event.
 *
 * Return: none
 */
void hdd_rssi_threshold_breached(void *hddctx,
				 struct rssi_breach_event *data)
{
	hdd_context_t *hdd_ctx  = hddctx;
	struct sk_buff *skb;

	ENTER();

	if (wlan_hdd_validate_context(hdd_ctx))
		return;
	if (!data) {
		hdd_err("data is null");
		return;
	}

	skb = cfg80211_vendor_event_alloc(hdd_ctx->wiphy,
				  NULL,
				  EXTSCAN_EVENT_BUF_SIZE + NLMSG_HDRLEN,
				  QCA_NL80211_VENDOR_SUBCMD_MONITOR_RSSI_INDEX,
				  GFP_KERNEL);

	if (!skb) {
		hdd_err("cfg80211_vendor_event_alloc failed");
		return;
	}

	hdd_notice("Req Id: %u Current rssi: %d",
			data->request_id, data->curr_rssi);
	hdd_notice("Current BSSID: "MAC_ADDRESS_STR,
			MAC_ADDR_ARRAY(data->curr_bssid.bytes));

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_REQUEST_ID,
		data->request_id) ||
	    nla_put(skb, QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_CUR_BSSID,
		sizeof(data->curr_bssid), data->curr_bssid.bytes) ||
	    nla_put_s8(skb, QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_CUR_RSSI,
		data->curr_rssi)) {
		hdd_err("nla put fail");
		goto fail;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);
	return;

fail:
	kfree_skb(skb);
	return;
}

/** __wlan_hdd_cfg80211_get_preferred_freq_list() - get preferred frequency list
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * This function return the preferred frequency list generated by the policy
 * manager.
 *
 * Return: success or failure code
 */
static int __wlan_hdd_cfg80211_get_preferred_freq_list(struct wiphy *wiphy,
						 struct wireless_dev
						 *wdev, const void *data,
						 int data_len)
{
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	int i, ret = 0;
	QDF_STATUS status;
	uint8_t pcl[QDF_MAX_NUM_CHAN], weight_list[QDF_MAX_NUM_CHAN];
	uint32_t pcl_len = 0;
	uint32_t freq_list[QDF_MAX_NUM_CHAN];
	enum cds_con_mode intf_mode;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_GET_PREFERRED_FREQ_LIST_MAX + 1];
	struct sk_buff *reply_skb;

	ENTER_DEV(wdev->netdev);

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return -EINVAL;

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_GET_PREFERRED_FREQ_LIST_MAX,
		      data, data_len, NULL)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_GET_PREFERRED_FREQ_LIST_IFACE_TYPE]) {
		hdd_err("attr interface type failed");
		return -EINVAL;
	}

	intf_mode = nla_get_u32(tb
		    [QCA_WLAN_VENDOR_ATTR_GET_PREFERRED_FREQ_LIST_IFACE_TYPE]);

	if (intf_mode < CDS_STA_MODE || intf_mode >= CDS_MAX_NUM_OF_MODE) {
		hdd_err("Invalid interface type");
		return -EINVAL;
	}

	hdd_debug("Userspace requested pref freq list");

	status = cds_get_pcl(intf_mode, pcl, &pcl_len,
				weight_list, QDF_ARRAY_SIZE(weight_list));
	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("Get pcl failed");
		return -EINVAL;
	}

	/* convert channel number to frequency */
	for (i = 0; i < pcl_len; i++) {
		if (pcl[i] <= ARRAY_SIZE(hdd_channels_2_4_ghz))
			freq_list[i] =
				ieee80211_channel_to_frequency(pcl[i],
							IEEE80211_BAND_2GHZ);
		else
			freq_list[i] =
				ieee80211_channel_to_frequency(pcl[i],
							IEEE80211_BAND_5GHZ);
	}

	/* send the freq_list back to supplicant */
	reply_skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, sizeof(u32) +
							sizeof(u32) *
							pcl_len +
							NLMSG_HDRLEN);

	if (!reply_skb) {
		hdd_err("Allocate reply_skb failed");
		return -EINVAL;
	}

	if (nla_put_u32(reply_skb,
		QCA_WLAN_VENDOR_ATTR_GET_PREFERRED_FREQ_LIST_IFACE_TYPE,
			intf_mode) ||
		nla_put(reply_skb,
			QCA_WLAN_VENDOR_ATTR_GET_PREFERRED_FREQ_LIST,
			sizeof(uint32_t) * pcl_len,
			freq_list)) {
		hdd_err("nla put fail");
		kfree_skb(reply_skb);
		return -EINVAL;
	}

	return cfg80211_vendor_cmd_reply(reply_skb);
}

/** wlan_hdd_cfg80211_get_preferred_freq_list () - get preferred frequency list
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * This function return the preferred frequency list generated by the policy
 * manager.
 *
 * Return: success or failure code
 */
static int wlan_hdd_cfg80211_get_preferred_freq_list(struct wiphy *wiphy,
						 struct wireless_dev
						 *wdev, const void *data,
						 int data_len)
{
	int ret = 0;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_get_preferred_freq_list(wiphy, wdev,
						data, data_len);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * __wlan_hdd_cfg80211_set_probable_oper_channel () - set probable channel
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
static int __wlan_hdd_cfg80211_set_probable_oper_channel(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len)
{
	struct net_device *ndev = wdev->netdev;
	hdd_adapter_t *adapter = WLAN_HDD_GET_PRIV_PTR(ndev);
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	int ret = 0;
	enum cds_con_mode intf_mode;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_GET_PREFERRED_FREQ_LIST_MAX + 1];
	uint32_t channel_hint;

	ENTER_DEV(ndev);

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_PROBABLE_OPER_CHANNEL_MAX,
		      data, data_len, NULL)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_PROBABLE_OPER_CHANNEL_IFACE_TYPE]) {
		hdd_err("attr interface type failed");
		return -EINVAL;
	}

	intf_mode = nla_get_u32(tb
		    [QCA_WLAN_VENDOR_ATTR_PROBABLE_OPER_CHANNEL_IFACE_TYPE]);

	if (intf_mode < CDS_STA_MODE || intf_mode >= CDS_MAX_NUM_OF_MODE) {
		hdd_err("Invalid interface type");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_PROBABLE_OPER_CHANNEL_FREQ]) {
		hdd_err("attr probable freq failed");
		return -EINVAL;
	}

	channel_hint = cds_freq_to_chan(nla_get_u32(tb
			[QCA_WLAN_VENDOR_ATTR_PROBABLE_OPER_CHANNEL_FREQ]));

	/* check pcl table */
	if (!cds_allow_concurrency(intf_mode,
					channel_hint, HW_MODE_20_MHZ)) {
		hdd_err("Set channel hint failed due to concurrency check");
		return -EINVAL;
	}

	if (0 != wlan_hdd_check_remain_on_channel(adapter))
		hdd_warn("Remain On Channel Pending");

	if (hdd_ctx->config->policy_manager_enabled) {
		ret = qdf_reset_connection_update();
		if (!QDF_IS_STATUS_SUCCESS(ret))
			hdd_err("clearing event failed");

		ret = cds_current_connections_update(adapter->sessionId,
					channel_hint,
					SIR_UPDATE_REASON_SET_OPER_CHAN);
		if (QDF_STATUS_E_FAILURE == ret) {
			/* return in the failure case */
			hdd_err("ERROR: connections update failed!!");
			return -EINVAL;
		}

		if (QDF_STATUS_SUCCESS == ret) {
			/*
			 * Success is the only case for which we expect hw mode
			 * change to take place, hence we need to wait.
			 * For any other return value it should be a pass
			 * through
			 */
			ret = qdf_wait_for_connection_update();
			if (!QDF_IS_STATUS_SUCCESS(ret)) {
				hdd_err("ERROR: qdf wait for event failed!!");
				return -EINVAL;
			}

		}
	}

	return 0;
}

/**
 * wlan_hdd_cfg80211_set_probable_oper_channel () - set probable channel
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
static int wlan_hdd_cfg80211_set_probable_oper_channel(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len)
{
	int ret = 0;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_set_probable_oper_channel(wiphy, wdev,
						data, data_len);
	cds_ssr_unprotect(__func__);

	return ret;
}

static const struct
nla_policy
qca_wlan_vendor_attr_policy[QCA_WLAN_VENDOR_ATTR_MAX+1] = {
	[QCA_WLAN_VENDOR_ATTR_MAC_ADDR] = { .type = NLA_UNSPEC },
};

/**
 * __wlan_hdd_cfg80211_get_link_properties() - Get link properties
 * @wiphy: WIPHY structure pointer
 * @wdev: Wireless device structure pointer
 * @data: Pointer to the data received
 * @data_len: Length of the data received
 *
 * This function is used to get link properties like nss, rate flags and
 * operating frequency for the active connection with the given peer.
 *
 * Return: 0 on success and errno on failure
 */
static int __wlan_hdd_cfg80211_get_link_properties(struct wiphy *wiphy,
						   struct wireless_dev *wdev,
						   const void *data,
						   int data_len)
{
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	hdd_adapter_t *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_station_ctx_t *hdd_sta_ctx;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_MAX+1];
	uint8_t peer_mac[QDF_MAC_ADDR_SIZE];
	uint32_t sta_id;
	struct sk_buff *reply_skb;
	uint32_t rate_flags = 0;
	uint8_t nss;
	uint8_t final_rate_flags = 0;
	uint32_t freq;

	ENTER_DEV(dev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	if (0 != wlan_hdd_validate_context(hdd_ctx))
		return -EINVAL;

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_MAX, data, data_len,
		      qca_wlan_vendor_attr_policy)) {
		hdd_err("Invalid attribute");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_MAC_ADDR]) {
		hdd_err("Attribute peerMac not provided for mode=%d",
		       adapter->device_mode);
		return -EINVAL;
	}

	qdf_mem_copy(peer_mac, nla_data(tb[QCA_WLAN_VENDOR_ATTR_MAC_ADDR]),
		     QDF_MAC_ADDR_SIZE);
	hdd_notice("peerMac="MAC_ADDRESS_STR" for device_mode:%d",
	       MAC_ADDR_ARRAY(peer_mac), adapter->device_mode);

	if (adapter->device_mode == QDF_STA_MODE ||
	    adapter->device_mode == QDF_P2P_CLIENT_MODE) {
		hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
		if ((hdd_sta_ctx->conn_info.connState !=
			eConnectionState_Associated) ||
		    qdf_mem_cmp(hdd_sta_ctx->conn_info.bssId.bytes,
			peer_mac, QDF_MAC_ADDR_SIZE)) {
			hdd_err("Not Associated to mac "MAC_ADDRESS_STR,
			       MAC_ADDR_ARRAY(peer_mac));
			return -EINVAL;
		}

		nss  = hdd_sta_ctx->conn_info.nss;
		freq = cds_chan_to_freq(
				hdd_sta_ctx->conn_info.operationChannel);
		rate_flags = hdd_sta_ctx->conn_info.rate_flags;
	} else if (adapter->device_mode == QDF_P2P_GO_MODE ||
		   adapter->device_mode == QDF_SAP_MODE) {

		for (sta_id = 0; sta_id < WLAN_MAX_STA_COUNT; sta_id++) {
			if (adapter->aStaInfo[sta_id].isUsed &&
			    !qdf_is_macaddr_broadcast(
				&adapter->aStaInfo[sta_id].macAddrSTA) &&
			    !qdf_mem_cmp(
				&adapter->aStaInfo[sta_id].macAddrSTA.bytes,
				peer_mac, QDF_MAC_ADDR_SIZE))
				break;
		}

		if (WLAN_MAX_STA_COUNT == sta_id) {
			hdd_err("No active peer with mac="MAC_ADDRESS_STR,
			       MAC_ADDR_ARRAY(peer_mac));
			return -EINVAL;
		}

		nss = adapter->aStaInfo[sta_id].nss;
		freq = cds_chan_to_freq(
			(WLAN_HDD_GET_AP_CTX_PTR(adapter))->operatingChannel);
		rate_flags = adapter->aStaInfo[sta_id].rate_flags;
	} else {
		hdd_err("Not Associated! with mac "MAC_ADDRESS_STR,
		       MAC_ADDR_ARRAY(peer_mac));
		return -EINVAL;
	}

	if (!(rate_flags & eHAL_TX_RATE_LEGACY)) {
		if (rate_flags & eHAL_TX_RATE_VHT80) {
			final_rate_flags |= RATE_INFO_FLAGS_VHT_MCS;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 0, 0)) && !defined(WITH_BACKPORTS)
			final_rate_flags |= RATE_INFO_FLAGS_80_MHZ_WIDTH;
#endif
		} else if (rate_flags & eHAL_TX_RATE_VHT40) {
			final_rate_flags |= RATE_INFO_FLAGS_VHT_MCS;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 0, 0)) && !defined(WITH_BACKPORTS)
			final_rate_flags |= RATE_INFO_FLAGS_40_MHZ_WIDTH;
#endif
		} else if (rate_flags & eHAL_TX_RATE_VHT20) {
			final_rate_flags |= RATE_INFO_FLAGS_VHT_MCS;
		} else if (rate_flags &
				(eHAL_TX_RATE_HT20 | eHAL_TX_RATE_HT40)) {
			final_rate_flags |= RATE_INFO_FLAGS_MCS;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 0, 0)) && !defined(WITH_BACKPORTS)
			if (rate_flags & eHAL_TX_RATE_HT40)
				final_rate_flags |=
					RATE_INFO_FLAGS_40_MHZ_WIDTH;
#endif
		}

		if (rate_flags & eHAL_TX_RATE_SGI) {
			if (!(final_rate_flags & RATE_INFO_FLAGS_VHT_MCS))
				final_rate_flags |= RATE_INFO_FLAGS_MCS;
			final_rate_flags |= RATE_INFO_FLAGS_SHORT_GI;
		}
	}

	reply_skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy,
			sizeof(u8) + sizeof(u8) + sizeof(u32) + NLMSG_HDRLEN);

	if (NULL == reply_skb) {
		hdd_err("getLinkProperties: skb alloc failed");
		return -EINVAL;
	}

	if (nla_put_u8(reply_skb,
		QCA_WLAN_VENDOR_ATTR_LINK_PROPERTIES_NSS,
		nss) ||
	    nla_put_u8(reply_skb,
		QCA_WLAN_VENDOR_ATTR_LINK_PROPERTIES_RATE_FLAGS,
		final_rate_flags) ||
	    nla_put_u32(reply_skb,
		QCA_WLAN_VENDOR_ATTR_LINK_PROPERTIES_FREQ,
		freq)) {
		hdd_err("nla_put failed");
		kfree_skb(reply_skb);
		return -EINVAL;
	}

	return cfg80211_vendor_cmd_reply(reply_skb);
}

/**
 * wlan_hdd_cfg80211_get_link_properties() - Wrapper function to get link
 * properties.
 * @wiphy: WIPHY structure pointer
 * @wdev: Wireless device structure pointer
 * @data: Pointer to the data received
 * @data_len: Length of the data received
 *
 * This function is used to get link properties like nss, rate flags and
 * operating frequency for the active connection with the given peer.
 *
 * Return: 0 on success and errno on failure
 */
static int wlan_hdd_cfg80211_get_link_properties(struct wiphy *wiphy,
						 struct wireless_dev *wdev,
						 const void *data,
						 int data_len)
{
	int ret = 0;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_get_link_properties(wiphy,
			wdev, data, data_len);
	cds_ssr_unprotect(__func__);

	return ret;
}

static const struct
nla_policy
qca_wlan_vendor_ota_test_policy
[QCA_WLAN_VENDOR_ATTR_OTA_TEST_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_OTA_TEST_ENABLE] = {.type = NLA_U8 },
};

/**
 * __wlan_hdd_cfg80211_set_ota_test () - enable/disable OTA test
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
static int __wlan_hdd_cfg80211_set_ota_test(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len)
{
	struct net_device *dev = wdev->netdev;
	hdd_adapter_t *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	tHalHandle hal = WLAN_HDD_GET_HAL_CTX(adapter);
	hdd_context_t *hdd_ctx  = wiphy_priv(wiphy);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_OTA_TEST_MAX + 1];
	uint8_t ota_enable = 0;
	QDF_STATUS status;
	uint32_t current_roam_state;

	ENTER_DEV(dev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	if (0 != wlan_hdd_validate_context(hdd_ctx))
		return -EINVAL;

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_OTA_TEST_MAX,
		      data, data_len,
		      qca_wlan_vendor_ota_test_policy)) {
		hdd_err("invalid attr");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_OTA_TEST_ENABLE]) {
		hdd_err("attr ota test failed");
		return -EINVAL;
	}

	ota_enable = nla_get_u8(
		tb[QCA_WLAN_VENDOR_ATTR_OTA_TEST_ENABLE]);

	hdd_info(" OTA test enable = %d", ota_enable);
	if (ota_enable != 1) {
		hdd_err("Invalid value, only enable test mode is supported!");
		return -EINVAL;
	}

	current_roam_state =
			sme_get_current_roam_state(hal, adapter->sessionId);
	status = sme_stop_roaming(hal, adapter->sessionId,
					eCsrHddIssued);
	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("Enable/Disable roaming failed");
		return -EINVAL;
	}

	status = sme_ps_enable_disable(hal, adapter->sessionId,
					SME_PS_DISABLE);
	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("Enable/Disable power save failed");
		/* restore previous roaming setting */
		if (current_roam_state == eCSR_ROAMING_STATE_JOINING ||
			 current_roam_state == eCSR_ROAMING_STATE_JOINED)
			status = sme_start_roaming(hal, adapter->sessionId,
						eCsrHddIssued);
		else if (current_roam_state == eCSR_ROAMING_STATE_STOP ||
			 current_roam_state == eCSR_ROAMING_STATE_IDLE)
			status = sme_stop_roaming(hal, adapter->sessionId,
						eCsrHddIssued);

		if (status != QDF_STATUS_SUCCESS)
			hdd_err("Restoring roaming state failed");

		return -EINVAL;
	}


	return 0;
}

/**
 * wlan_hdd_cfg80211_set_ota_test () - Enable or disable OTA test
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
static int wlan_hdd_cfg80211_set_ota_test(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data,
					int data_len)
{
	int ret = 0;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_set_ota_test(wiphy, wdev, data, data_len);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * __wlan_hdd_cfg80211_txpower_scale () - txpower scaling
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
static int __wlan_hdd_cfg80211_txpower_scale(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len)
{
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	hdd_adapter_t *adapter;
	int ret;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE_MAX + 1];
	uint8_t scale_value;
	QDF_STATUS status;

	ENTER_DEV(dev);

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	adapter = WLAN_HDD_GET_PRIV_PTR(dev);

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE_MAX,
		      data, data_len, NULL)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE]) {
		hdd_err("attr tx power scale failed");
		return -EINVAL;
	}

	scale_value = nla_get_u8(tb
		    [QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE]);

	if (scale_value > MAX_TXPOWER_SCALE) {
		hdd_err("Invalid tx power scale level");
		return -EINVAL;
	}

	status = wma_set_tx_power_scale(adapter->sessionId, scale_value);

	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("Set tx power scale failed");
		return -EINVAL;
	}

	return 0;
}

/**
 * wlan_hdd_cfg80211_txpower_scale () - txpower scaling
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
static int wlan_hdd_cfg80211_txpower_scale(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_txpower_scale(wiphy, wdev,
						data, data_len);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * __wlan_hdd_cfg80211_txpower_scale_decr_db () - txpower scaling
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
static int __wlan_hdd_cfg80211_txpower_scale_decr_db(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len)
{
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	hdd_adapter_t *adapter;
	int ret;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE_DECR_DB_MAX + 1];
	uint8_t scale_value;
	QDF_STATUS status;

	ENTER_DEV(dev);

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	adapter = WLAN_HDD_GET_PRIV_PTR(dev);

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE_DECR_DB_MAX,
		      data, data_len, NULL)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE_DECR_DB]) {
		hdd_err("attr tx power decrease db value failed");
		return -EINVAL;
	}

	scale_value = nla_get_u8(tb
		    [QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE_DECR_DB]);

	status = wma_set_tx_power_scale_decr_db(adapter->sessionId,
							scale_value);

	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("Set tx power decrease db failed");
		return -EINVAL;
	}

	return 0;
}

/**
 * wlan_hdd_cfg80211_txpower_scale_decr_db () - txpower scaling
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
static int wlan_hdd_cfg80211_txpower_scale_decr_db(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_txpower_scale_decr_db(wiphy, wdev,
						data, data_len);
	cds_ssr_unprotect(__func__);

	return ret;
}
/*
 * define short names for the global vendor params
 * used by __wlan_hdd_cfg80211_bpf_offload()
 */
#define BPF_INVALID \
	QCA_WLAN_VENDOR_ATTR_PACKET_FILTER_INVALID
#define BPF_SET_RESET \
	QCA_WLAN_VENDOR_ATTR_SET_RESET_PACKET_FILTER
#define BPF_VERSION \
	QCA_WLAN_VENDOR_ATTR_PACKET_FILTER_VERSION
#define BPF_FILTER_ID \
	QCA_WLAN_VENDOR_ATTR_PACKET_FILTER_ID
#define BPF_PACKET_SIZE \
	QCA_WLAN_VENDOR_ATTR_PACKET_FILTER_SIZE
#define BPF_CURRENT_OFFSET \
	QCA_WLAN_VENDOR_ATTR_PACKET_FILTER_CURRENT_OFFSET
#define BPF_PROGRAM \
	QCA_WLAN_VENDOR_ATTR_PACKET_FILTER_PROGRAM
#define BPF_MAX \
	QCA_WLAN_VENDOR_ATTR_PACKET_FILTER_MAX

static const struct nla_policy
wlan_hdd_bpf_offload_policy[BPF_MAX + 1] = {
	[BPF_SET_RESET] = {.type = NLA_U32},
	[BPF_VERSION] = {.type = NLA_U32},
	[BPF_FILTER_ID] = {.type = NLA_U32},
	[BPF_PACKET_SIZE] = {.type = NLA_U32},
	[BPF_CURRENT_OFFSET] = {.type = NLA_U32},
	[BPF_PROGRAM] = {.type = NLA_U8},
};

/**
 * hdd_get_bpf_offload_cb() - Callback function to BPF Offload
 * @hdd_context: hdd_context
 * @bpf_get_offload: struct for get offload
 *
 * This function receives the response/data from the lower layer and
 * checks to see if the thread is still waiting then post the results to
 * upper layer, if the request has timed out then ignore.
 *
 * Return: None
 */
void hdd_get_bpf_offload_cb(void *hdd_context,
			    struct sir_bpf_get_offload *data)
{
	hdd_context_t *hdd_ctx = hdd_context;
	struct hdd_bpf_context *context;

	ENTER();

	if (wlan_hdd_validate_context(hdd_ctx) || !data) {
		hdd_err("HDD context is invalid or data(%p) is null",
			data);
		return;
	}

	spin_lock(&hdd_context_lock);

	context = &bpf_context;
	/* The caller presumably timed out so there is nothing we can do */
	if (context->magic != BPF_CONTEXT_MAGIC) {
		spin_unlock(&hdd_context_lock);
		return;
	}

	/* context is valid so caller is still waiting */
	/* paranoia: invalidate the magic */
	context->magic = 0;

	context->capability_response = *data;
	complete(&context->completion);

	spin_unlock(&hdd_context_lock);

	return;
}

/**
 * hdd_post_get_bpf_capabilities_rsp() - Callback function to BPF Offload
 * @hdd_context: hdd_context
 * @bpf_get_offload: struct for get offload
 *
 * Return: 0 on success, error number otherwise.
 */
static int hdd_post_get_bpf_capabilities_rsp(hdd_context_t *hdd_ctx,
			    struct sir_bpf_get_offload *bpf_get_offload)
{
	struct sk_buff *skb;
	uint32_t nl_buf_len;

	ENTER();

	nl_buf_len = NLMSG_HDRLEN;
	nl_buf_len +=
		(sizeof(bpf_get_offload->max_bytes_for_bpf_inst) + NLA_HDRLEN) +
		(sizeof(bpf_get_offload->bpf_version) + NLA_HDRLEN);

	skb = cfg80211_vendor_cmd_alloc_reply_skb(hdd_ctx->wiphy, nl_buf_len);
	if (!skb) {
		hdd_err("cfg80211_vendor_cmd_alloc_reply_skb failed");
		return -ENOMEM;
	}

	hdd_notice("BPF Version: %u BPF max bytes: %u",
			bpf_get_offload->bpf_version,
			bpf_get_offload->max_bytes_for_bpf_inst);

	if (nla_put_u32(skb, BPF_PACKET_SIZE,
			bpf_get_offload->max_bytes_for_bpf_inst) ||
	    nla_put_u32(skb, BPF_VERSION, bpf_get_offload->bpf_version)) {
		hdd_err("nla put failure");
		goto nla_put_failure;
	}

	cfg80211_vendor_cmd_reply(skb);
	EXIT();
	return 0;

nla_put_failure:
	kfree_skb(skb);
	return -EINVAL;
}

/**
 * hdd_get_bpf_offload - Get BPF offload Capabilities
 * @hdd_ctx: Hdd context
 *
 * Return: 0 on success, errno on failure
 */
static int hdd_get_bpf_offload(hdd_context_t *hdd_ctx)
{
	unsigned long rc;
	struct hdd_bpf_context *context;
	QDF_STATUS status;
	int ret;

	ENTER();

	spin_lock(&hdd_context_lock);
	context = &bpf_context;
	context->magic = BPF_CONTEXT_MAGIC;
	INIT_COMPLETION(context->completion);
	spin_unlock(&hdd_context_lock);

	status = sme_get_bpf_offload_capabilities(hdd_ctx->hHal);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("Unable to retrieve BPF caps");
		return -EINVAL;
	}
	/* request was sent -- wait for the response */
	rc = wait_for_completion_timeout(&context->completion,
			msecs_to_jiffies(WLAN_WAIT_TIME_BPF));
	if (!rc) {
		hdd_err("Target response timed out");
		spin_lock(&hdd_context_lock);
		context->magic = 0;
		spin_unlock(&hdd_context_lock);

		return -ETIMEDOUT;
	}
	ret = hdd_post_get_bpf_capabilities_rsp(hdd_ctx,
					&bpf_context.capability_response);
	if (ret)
		hdd_err("Failed to post get bpf capabilities");

	EXIT();
	return ret;
}

/**
 * hdd_set_reset_bpf_offload - Post set/reset bpf to SME
 * @hdd_ctx: Hdd context
 * @tb: Length of @data
 * @session_id: Session identifier
 *
 * Return: 0 on success; errno on failure
 */
static int hdd_set_reset_bpf_offload(hdd_context_t *hdd_ctx,
				     struct nlattr **tb,
				     uint8_t session_id)
{
	struct sir_bpf_set_offload *bpf_set_offload;
	QDF_STATUS status;
	int prog_len;

	ENTER();

	bpf_set_offload = qdf_mem_malloc(sizeof(*bpf_set_offload));
	if (bpf_set_offload == NULL) {
		hdd_err("qdf_mem_malloc failed for bpf_set_offload");
		return -ENOMEM;
	}
	qdf_mem_zero(bpf_set_offload, sizeof(*bpf_set_offload));

	/* Parse and fetch bpf packet size */
	if (!tb[BPF_PACKET_SIZE]) {
		hdd_err("attr bpf packet size failed");
		goto fail;
	}
	bpf_set_offload->total_length = nla_get_u32(tb[BPF_PACKET_SIZE]);

	if (!bpf_set_offload->total_length) {
		hdd_notice("BPF reset packet filter received");
		goto post_sme;
	}

	/* Parse and fetch bpf program */
	if (!tb[BPF_PROGRAM]) {
		hdd_err("attr bpf program failed");
		goto fail;
	}

	prog_len = nla_len(tb[BPF_PROGRAM]);
	bpf_set_offload->program = qdf_mem_malloc(sizeof(uint8_t) * prog_len);
	bpf_set_offload->current_length = prog_len;
	nla_memcpy(bpf_set_offload->program, tb[BPF_PROGRAM], prog_len);
	bpf_set_offload->session_id = session_id;

	/* Parse and fetch filter Id */
	if (!tb[BPF_FILTER_ID]) {
		hdd_err("attr filter id failed");
		goto fail;
	}
	bpf_set_offload->filter_id = nla_get_u32(tb[BPF_FILTER_ID]);

	/* Parse and fetch current offset */
	if (!tb[BPF_CURRENT_OFFSET]) {
		hdd_err("attr current offset failed");
		goto fail;
	}
	bpf_set_offload->current_offset = nla_get_u32(tb[BPF_CURRENT_OFFSET]);

post_sme:
	hdd_notice("Posting BPF SET/RESET to SME, session_id: %d Bpf Version: %d filter ID: %d total_length: %d current_length: %d current offset: %d",
			bpf_set_offload->session_id,
			bpf_set_offload->version,
			bpf_set_offload->filter_id,
			bpf_set_offload->total_length,
			bpf_set_offload->current_length,
			bpf_set_offload->current_offset);

	status = sme_set_bpf_instructions(hdd_ctx->hHal, bpf_set_offload);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("sme_set_bpf_instructions failed(err=%d)", status);
		goto fail;
	}
	EXIT();
	if (bpf_set_offload->current_length)
		qdf_mem_free(bpf_set_offload->program);
	qdf_mem_free(bpf_set_offload);
	return 0;

fail:
	if (bpf_set_offload->current_length)
		qdf_mem_free(bpf_set_offload->program);
	qdf_mem_free(bpf_set_offload);
	return -EINVAL;
}

/**
 * wlan_hdd_cfg80211_bpf_offload() - Set/Reset to BPF Offload
 * @wiphy:    wiphy structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of @data
 *
 * Return: 0 on success; errno on failure
 */
static int
__wlan_hdd_cfg80211_bpf_offload(struct wiphy *wiphy,
				struct wireless_dev *wdev,
				const void *data, int data_len)
{
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	hdd_adapter_t *pAdapter =  WLAN_HDD_GET_PRIV_PTR(dev);
	struct nlattr *tb[BPF_MAX + 1];
	int ret_val, packet_filter_subcmd;

	ENTER();

	ret_val = wlan_hdd_validate_context(hdd_ctx);
	if (ret_val)
		return ret_val;

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EINVAL;
	}

	if (!hdd_ctx->bpf_enabled) {
		hdd_err("BPF offload is not supported by firmware");
		return -ENOTSUPP;
	}

	if (nla_parse(tb, BPF_MAX, data, data_len,
				wlan_hdd_bpf_offload_policy)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}

	if (!tb[BPF_SET_RESET]) {
		hdd_err("attr bpf set reset failed");
		return -EINVAL;
	}

	packet_filter_subcmd = nla_get_u32(tb[BPF_SET_RESET]);

	if (packet_filter_subcmd == QCA_WLAN_GET_PACKET_FILTER)
		return hdd_get_bpf_offload(hdd_ctx);
	else
		return hdd_set_reset_bpf_offload(hdd_ctx, tb,
						 pAdapter->sessionId);
}

/**
 * wlan_hdd_cfg80211_bpf_offload() - SSR Wrapper to BPF Offload
 * @wiphy:    wiphy structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of @data
 *
 * Return: 0 on success; errno on failure
 */

static int wlan_hdd_cfg80211_bpf_offload(struct wiphy *wiphy,
		struct wireless_dev *wdev,
		const void *data, int data_len)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_bpf_offload(wiphy, wdev, data, data_len);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * hdd_init_bpf_completion() - Initialize the completion event for bpf
 *
 * Return: None
 */
void hdd_init_bpf_completion(void)
{
	init_completion(&bpf_context.completion);
}

static const struct nla_policy
wlan_hdd_sap_config_policy[QCA_WLAN_VENDOR_ATTR_SAP_CONFIG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_SAP_CONFIG_CHANNEL] = {.type = NLA_U8 },
};

/**
 * __wlan_hdd_cfg80211_sap_configuration_set() - ask driver to restart SAP if
 * SAP is on unsafe channel.
 * @wiphy:    wiphy structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of @data
 *
 * __wlan_hdd_cfg80211_sap_configuration_set function set SAP params to
 * driver.
 * QCA_WLAN_VENDOR_ATTR_SAP_CONFIG_CHAN will set sap config channel and
 * will initiate restart of sap.
 *
 * Return: 0 on success; errno on failure
 */
static int
__wlan_hdd_cfg80211_sap_configuration_set(struct wiphy *wiphy,
		struct wireless_dev *wdev,
		const void *data, int data_len)
{
	struct net_device *ndev = wdev->netdev;
	hdd_adapter_t *hostapd_adapter = WLAN_HDD_GET_PRIV_PTR(ndev);
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SAP_CONFIG_MAX + 1];
	uint8_t config_channel = 0;
	hdd_ap_ctx_t *ap_ctx;
	int ret;
	QDF_STATUS status;

	ENTER();

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EINVAL;
	}

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return -EINVAL;

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_SAP_CONFIG_MAX,
				data, data_len,
				wlan_hdd_sap_config_policy)) {
		hdd_err("invalid attr");
		return -EINVAL;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SAP_CONFIG_CHANNEL]) {
		if (!test_bit(SOFTAP_BSS_STARTED,
					&hostapd_adapter->event_flags)) {
			hdd_err("SAP is not started yet. Restart sap will be invalid");
			return -EINVAL;
		}

		config_channel =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SAP_CONFIG_CHANNEL]);

		if (!((IS_24G_CH(config_channel)) ||
			(IS_5G_CH(config_channel)))) {
			hdd_err("Channel  %d is not valid to restart SAP",
					config_channel);
			return -ENOTSUPP;
		}

		ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(hostapd_adapter);
		ap_ctx->sapConfig.channel = config_channel;
		ap_ctx->sapConfig.ch_params.ch_width =
					ap_ctx->sapConfig.ch_width_orig;

		cds_set_channel_params(ap_ctx->sapConfig.channel,
				ap_ctx->sapConfig.sec_ch,
				&ap_ctx->sapConfig.ch_params);

		cds_restart_sap(hostapd_adapter);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SAP_MANDATORY_FREQUENCY_LIST]) {
		uint32_t freq_len, i;
		uint32_t *freq;
		uint8_t chans[QDF_MAX_NUM_CHAN];

		hdd_debug("setting mandatory freq/chan list");

		freq_len = nla_len(
		    tb[QCA_WLAN_VENDOR_ATTR_SAP_MANDATORY_FREQUENCY_LIST])/
		    sizeof(uint32_t);

		if (freq_len > QDF_MAX_NUM_CHAN) {
			hdd_err("insufficient space to hold channels");
			return -ENOMEM;
		}

		freq = nla_data(
		    tb[QCA_WLAN_VENDOR_ATTR_SAP_MANDATORY_FREQUENCY_LIST]);

		hdd_debug("freq_len=%d", freq_len);

		for (i = 0; i < freq_len; i++) {
			chans[i] = ieee80211_frequency_to_channel(freq[i]);
			hdd_debug("freq[%d]=%d", i, freq[i]);
		}

		status = cds_set_sap_mandatory_channels(chans, freq_len);
		if (QDF_IS_STATUS_ERROR(status))
			return -EINVAL;
	}

	return 0;
}

/**
 * wlan_hdd_cfg80211_sap_configuration_set() - sap configuration vendor command
 * @wiphy:    wiphy structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of @data
 *
 * __wlan_hdd_cfg80211_sap_configuration_set function set SAP params to
 * driver.
 * QCA_WLAN_VENDOR_ATTR_SAP_CONFIG_CHAN will set sap config channel and
 * will initiate restart of sap.
 *
 * Return: 0 on success; errno on failure
 */
static int wlan_hdd_cfg80211_sap_configuration_set(struct wiphy *wiphy,
		struct wireless_dev *wdev,
		const void *data, int data_len)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_sap_configuration_set(wiphy,
			wdev, data, data_len);
	cds_ssr_unprotect(__func__);

	return ret;
}

#undef BPF_INVALID
#undef BPF_SET_RESET
#undef BPF_VERSION
#undef BPF_ID
#undef BPF_PACKET_SIZE
#undef BPF_CURRENT_OFFSET
#undef BPF_PROGRAM
#undef BPF_MAX
const struct wiphy_vendor_command hdd_wiphy_vendor_commands[] = {
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_DFS_CAPABILITY,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV | WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = is_driver_dfs_capable
	},

#ifdef WLAN_FEATURE_NAN
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_NAN,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV | WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_nan_request
	},
#endif

#ifdef WLAN_FEATURE_STATS_EXT
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_STATS_EXT,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV | WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_stats_ext_request
	},
#endif
#ifdef FEATURE_WLAN_EXTSCAN
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_START,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV | WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_extscan_start
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_STOP,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV | WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_extscan_stop
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_GET_VALID_CHANNELS,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wlan_hdd_cfg80211_extscan_get_valid_channels
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_GET_CAPABILITIES,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV | WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_extscan_get_capabilities
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_GET_CACHED_RESULTS,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV | WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_extscan_get_cached_results
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SET_BSSID_HOTLIST,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV | WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_extscan_set_bssid_hotlist
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_RESET_BSSID_HOTLIST,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV | WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_extscan_reset_bssid_hotlist
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd =
			QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SET_SIGNIFICANT_CHANGE,
		.flags =
			WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV |
			WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_extscan_set_significant_change
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd =
			QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_RESET_SIGNIFICANT_CHANGE,
		.flags =
			WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV |
			WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_extscan_reset_significant_change
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_PNO_SET_LIST,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV |
			WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_set_epno_list
	},
#endif /* FEATURE_WLAN_EXTSCAN */

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_LL_STATS_CLR,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV | WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_ll_stats_clear
	},

	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_LL_STATS_SET,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV | WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_ll_stats_set
	},

	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_LL_STATS_GET,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV | WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_ll_stats_get
	},
#endif /* WLAN_FEATURE_LINK_LAYER_STATS */
#ifdef FEATURE_WLAN_TDLS
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_TDLS_ENABLE,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV | WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_exttdls_enable
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_TDLS_DISABLE,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV | WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_exttdls_disable
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_TDLS_GET_STATUS,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wlan_hdd_cfg80211_exttdls_get_status
	},
#endif
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_SUPPORTED_FEATURES,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wlan_hdd_cfg80211_get_supported_features
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SCANNING_MAC_OUI,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wlan_hdd_cfg80211_set_scanning_mac_oui
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_CONCURRENCY_MATRIX,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wlan_hdd_cfg80211_get_concurrency_matrix
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_NO_DFS_FLAG,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wlan_hdd_cfg80211_disable_dfs_chan_scan
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_WISA,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wlan_hdd_cfg80211_handle_wisa_cmd
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_DO_ACS,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
				WIPHY_VENDOR_CMD_NEED_NETDEV |
				WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_do_acs
	},

	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_FEATURES,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wlan_hdd_cfg80211_get_features
	},
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_KEY_MGMT_SET_KEY,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV |
			WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_keymgmt_set_key
	},
#endif
#ifdef FEATURE_WLAN_EXTSCAN
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_PNO_SET_PASSPOINT_LIST,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV |
			WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_set_passpoint_list
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_PNO_RESET_PASSPOINT_LIST,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV |
			WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_reset_passpoint_list
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SET_SSID_HOTLIST,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV |
			WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_extscan_set_ssid_hotlist
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_RESET_SSID_HOTLIST,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV |
			WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_extscan_reset_ssid_hotlist
	},
#endif /* FEATURE_WLAN_EXTSCAN */
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_INFO,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wlan_hdd_cfg80211_get_wifi_info
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV |
			WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_wifi_configuration_set
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_ROAM,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wlan_hdd_cfg80211_set_ext_roam_params
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_WIFI_LOGGER_START,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wlan_hdd_cfg80211_wifi_logger_start
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_RING_DATA,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wlan_hdd_cfg80211_wifi_logger_get_ring_data
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd =
			QCA_NL80211_VENDOR_SUBCMD_GET_PREFERRED_FREQ_LIST,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV |
			WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_get_preferred_freq_list
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd =
			QCA_NL80211_VENDOR_SUBCMD_SET_PROBABLE_OPER_CHANNEL,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV |
			WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_set_probable_oper_channel
	},
#ifdef WLAN_FEATURE_TSF
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_TSF,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV |
			WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_handle_tsf_cmd
	},
#endif
#ifdef FEATURE_WLAN_TDLS
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_TDLS_GET_CAPABILITIES,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV |
			WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_get_tdls_capabilities
	},
#endif
#ifdef WLAN_FEATURE_OFFLOAD_PACKETS
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_OFFLOADED_PACKETS,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV |
			WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_offloaded_packets
	},
#endif
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_MONITOR_RSSI,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV |
			WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_monitor_rssi
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_LOGGER_FEATURE_SET,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_get_logger_supp_feature
	},
#ifdef WLAN_FEATURE_MEMDUMP
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_WIFI_LOGGER_MEMORY_DUMP,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_get_fw_mem_dump
	},
#endif /* WLAN_FEATURE_MEMDUMP */
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_TRIGGER_SCAN,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV |
			WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_vendor_scan
	},

	/* OCB commands */
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_OCB_SET_CONFIG,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
				 WIPHY_VENDOR_CMD_NEED_NETDEV |
				 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_ocb_set_config
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_OCB_SET_UTC_TIME,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
				 WIPHY_VENDOR_CMD_NEED_NETDEV |
				 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_ocb_set_utc_time
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd =
			QCA_NL80211_VENDOR_SUBCMD_OCB_START_TIMING_ADVERT,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
				 WIPHY_VENDOR_CMD_NEED_NETDEV |
				 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_ocb_start_timing_advert
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_OCB_STOP_TIMING_ADVERT,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
				 WIPHY_VENDOR_CMD_NEED_NETDEV |
				 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_ocb_stop_timing_advert
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_OCB_GET_TSF_TIMER,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
				 WIPHY_VENDOR_CMD_NEED_NETDEV |
				 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_ocb_get_tsf_timer
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_DCC_GET_STATS,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
				 WIPHY_VENDOR_CMD_NEED_NETDEV |
				 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_dcc_get_stats
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_DCC_CLEAR_STATS,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
				 WIPHY_VENDOR_CMD_NEED_NETDEV |
				 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_dcc_clear_stats
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_DCC_UPDATE_NDL,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
				 WIPHY_VENDOR_CMD_NEED_NETDEV |
				 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_dcc_update_ndl
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_LINK_PROPERTIES,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_get_link_properties
	},
	{
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_OTA_TEST,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
				 WIPHY_VENDOR_CMD_NEED_NETDEV |
				 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_set_ota_test
	},
#ifdef FEATURE_LFR_SUBNET_DETECTION
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GW_PARAM_CONFIG,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
				WIPHY_VENDOR_CMD_NEED_NETDEV |
				WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_set_gateway_params
	},
#endif /* FEATURE_LFR_SUBNET_DETECTION */
	{
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SET_TXPOWER_SCALE,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
				 WIPHY_VENDOR_CMD_NEED_NETDEV |
				 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_txpower_scale
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd =
			QCA_NL80211_VENDOR_SUBCMD_SET_TXPOWER_SCALE_DECR_DB,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
				 WIPHY_VENDOR_CMD_NEED_NETDEV |
				 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_txpower_scale_decr_db
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_PACKET_FILTER,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV |
			WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_bpf_offload
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SET_SAP_CONFIG,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV |
			WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_sap_configuration_set
	},

#ifdef WLAN_FEATURE_NAN_DATAPATH
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_NDP,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV |
			WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_process_ndp_cmd
	},
#endif
};

/**
 * hdd_cfg80211_wiphy_alloc() - Allocate wiphy context
 * @priv_size:         Size of the hdd context.
 *
 * Allocate wiphy context and hdd context.
 *
 * Return: hdd context on success and NULL on failure.
 */
hdd_context_t *hdd_cfg80211_wiphy_alloc(int priv_size)
{
	struct wiphy *wiphy;
	hdd_context_t *hdd_ctx;

	ENTER();

	wiphy = wiphy_new(&wlan_hdd_cfg80211_ops, priv_size);

	if (!wiphy) {
		hdd_err("wiphy init failed!\n");
		return NULL;
	}

	hdd_ctx = wiphy_priv(wiphy);

	hdd_ctx->wiphy = wiphy;

	return hdd_ctx;
}

/*
 * FUNCTION: wlan_hdd_cfg80211_update_band
 * This function is called from the supplicant through a
 * private ioctl to change the band value
 */
int wlan_hdd_cfg80211_update_band(struct wiphy *wiphy, eCsrBand eBand)
{
	int i, j;
	enum channel_state channelEnabledState;

	ENTER();

	for (i = 0; i < IEEE80211_NUM_BANDS; i++) {

		if (NULL == wiphy->bands[i])
			continue;

		for (j = 0; j < wiphy->bands[i]->n_channels; j++) {
			struct ieee80211_supported_band *band = wiphy->bands[i];

			channelEnabledState =
				cds_get_channel_state(band->channels[j].
								 hw_value);

			if (IEEE80211_BAND_2GHZ == i && eCSR_BAND_5G == eBand) {
				/* 5G only */
#ifdef WLAN_ENABLE_SOCIAL_CHANNELS_5G_ONLY
				/* Enable Social channels for P2P */
				if (WLAN_HDD_IS_SOCIAL_CHANNEL
					    (band->channels[j].center_freq)
				    && CHANNEL_STATE_ENABLE ==
				    channelEnabledState)
					band->channels[j].flags &=
						~IEEE80211_CHAN_DISABLED;
				else
#endif
				band->channels[j].flags |=
					IEEE80211_CHAN_DISABLED;
				continue;
			} else if (IEEE80211_BAND_5GHZ == i &&
					eCSR_BAND_24 == eBand) {
				/* 2G only */
				band->channels[j].flags |=
					IEEE80211_CHAN_DISABLED;
				continue;
			}

			if (CHANNEL_STATE_DISABLE != channelEnabledState)
				band->channels[j].flags &=
					~IEEE80211_CHAN_DISABLED;
		}
	}
	return 0;
}

/*
 * FUNCTION: wlan_hdd_cfg80211_init
 * This function is called by hdd_wlan_startup()
 * during initialization.
 * This function is used to initialize and register wiphy structure.
 */
int wlan_hdd_cfg80211_init(struct device *dev,
			   struct wiphy *wiphy, struct hdd_config *pCfg)
{
	int i, j;
	hdd_context_t *pHddCtx = wiphy_priv(wiphy);

	ENTER();

	/* Now bind the underlying wlan device with wiphy */
	set_wiphy_dev(wiphy, dev);

	wiphy->mgmt_stypes = wlan_hdd_txrx_stypes;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
	wiphy->regulatory_flags |= REGULATORY_DISABLE_BEACON_HINTS;
	wiphy->regulatory_flags |= REGULATORY_COUNTRY_IE_IGNORE;
#else
	wiphy->flags |= WIPHY_FLAG_DISABLE_BEACON_HINTS;
	wiphy->country_ie_pref |= NL80211_COUNTRY_IE_IGNORE_CORE;
#endif

	wiphy->flags |= WIPHY_FLAG_HAVE_AP_SME
			| WIPHY_FLAG_AP_PROBE_RESP_OFFLOAD
			| WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL
#ifdef FEATURE_WLAN_STA_4ADDR_SCHEME
			| WIPHY_FLAG_4ADDR_STATION
#endif
			| WIPHY_FLAG_OFFCHAN_TX;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0))
	wiphy->wowlan = &wowlan_support_cfg80211_init;
#else
	wiphy->wowlan.flags = WIPHY_WOWLAN_MAGIC_PKT;
	wiphy->wowlan.n_patterns = WOWL_MAX_PTRNS_ALLOWED;
	wiphy->wowlan.pattern_min_len = 1;
	wiphy->wowlan.pattern_max_len = WOWL_PTRN_MAX_SIZE;
#endif

	if (pCfg->isFastTransitionEnabled || pCfg->isFastRoamIniFeatureEnabled
#ifdef FEATURE_WLAN_ESE
	    || pCfg->isEseIniFeatureEnabled
#endif
	    ) {
		wiphy->flags |= WIPHY_FLAG_SUPPORTS_FW_ROAM;
	}
#ifdef FEATURE_WLAN_TDLS
	wiphy->flags |= WIPHY_FLAG_SUPPORTS_TDLS
			| WIPHY_FLAG_TDLS_EXTERNAL_SETUP;
#endif

	wiphy->features |= NL80211_FEATURE_HT_IBSS;

#ifdef FEATURE_WLAN_SCAN_PNO
	if (pCfg->configPNOScanSupport) {
		wiphy->flags |= WIPHY_FLAG_SUPPORTS_SCHED_SCAN;
		wiphy->max_sched_scan_ssids = SIR_PNO_MAX_SUPP_NETWORKS;
		wiphy->max_match_sets = SIR_PNO_MAX_SUPP_NETWORKS;
		wiphy->max_sched_scan_ie_len = SIR_MAC_MAX_IE_LENGTH;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)) || defined(WITH_BACKPORTS)
		wiphy->max_sched_scan_plans = SIR_PNO_MAX_PLAN_REQUEST;
#endif
	}
#endif /*FEATURE_WLAN_SCAN_PNO */

#if  defined QCA_WIFI_FTM
	if (cds_get_conparam() != QDF_GLOBAL_FTM_MODE) {
#endif

	/* even with WIPHY_FLAG_CUSTOM_REGULATORY,
	   driver can still register regulatory callback and
	   it will get regulatory settings in wiphy->band[], but
	   driver need to determine what to do with both
	   regulatory settings */

	wiphy->reg_notifier = hdd_reg_notifier;

#if  defined QCA_WIFI_FTM
}
#endif

	wiphy->max_scan_ssids = MAX_SCAN_SSID;

	wiphy->max_scan_ie_len = SIR_MAC_MAX_ADD_IE_LENGTH;

	wiphy->max_acl_mac_addrs = MAX_ACL_MAC_ADDRESS;

	if (cds_get_conparam() != QDF_GLOBAL_MONITOR_MODE) {
		/* Supports STATION & AD-HOC modes right now */
		wiphy->interface_modes = BIT(NL80211_IFTYPE_STATION)
					 | BIT(NL80211_IFTYPE_ADHOC)
					 | BIT(NL80211_IFTYPE_P2P_CLIENT)
					 | BIT(NL80211_IFTYPE_P2P_GO)
					 | BIT(NL80211_IFTYPE_AP);

		if (pCfg->advertiseConcurrentOperation) {
			if (pCfg->enableMCC) {
				int i;

				for (i = 0;
				     i < ARRAY_SIZE(wlan_hdd_iface_combination);
				     i++) {
					if (!pCfg->allowMCCGODiffBI)
						wlan_hdd_iface_combination[i].
						beacon_int_infra_match = true;
				}
			}
			wiphy->n_iface_combinations =
				ARRAY_SIZE(wlan_hdd_iface_combination);
			wiphy->iface_combinations = wlan_hdd_iface_combination;
		}
	} else {
		wiphy->interface_modes = BIT(NL80211_IFTYPE_MONITOR);
		wiphy->n_iface_combinations =
			ARRAY_SIZE(wlan_hdd_mon_iface);
		wiphy->iface_combinations = wlan_hdd_mon_iface;
	}

	/* Before registering we need to update the ht capabilitied based
	 * on ini values*/
	if (!pCfg->ShortGI20MhzEnable) {
		wlan_hdd_band_2_4_ghz.ht_cap.cap &= ~IEEE80211_HT_CAP_SGI_20;
		wlan_hdd_band_5_ghz.ht_cap.cap &= ~IEEE80211_HT_CAP_SGI_20;
	}

	if (!pCfg->ShortGI40MhzEnable) {
		wlan_hdd_band_5_ghz.ht_cap.cap &= ~IEEE80211_HT_CAP_SGI_40;
	}

	if (!pCfg->nChannelBondingMode5GHz) {
		wlan_hdd_band_5_ghz.ht_cap.cap &=
			~IEEE80211_HT_CAP_SUP_WIDTH_20_40;
	}

	/*
	 * In case of static linked driver at the time of driver unload,
	 * module exit doesn't happens. Module cleanup helps in cleaning
	 * of static memory.
	 * If driver load happens statically, at the time of driver unload,
	 * wiphy flags don't get reset because of static memory.
	 * It's better not to store channel in static memory.
	 */
	wiphy->bands[IEEE80211_BAND_2GHZ] = &wlan_hdd_band_2_4_ghz;
	wiphy->bands[IEEE80211_BAND_2GHZ]->channels =
		qdf_mem_malloc(sizeof(hdd_channels_2_4_ghz));
	if (wiphy->bands[IEEE80211_BAND_2GHZ]->channels == NULL) {
		hdd_err("Not enough memory to allocate channels");
		return -ENOMEM;
	}
	qdf_mem_copy(wiphy->bands[IEEE80211_BAND_2GHZ]->channels,
			&hdd_channels_2_4_ghz[0],
			sizeof(hdd_channels_2_4_ghz));
	if (true == hdd_is_5g_supported(pHddCtx)) {
		wiphy->bands[IEEE80211_BAND_5GHZ] = &wlan_hdd_band_5_ghz;
		wiphy->bands[IEEE80211_BAND_5GHZ]->channels =
			qdf_mem_malloc(sizeof(hdd_channels_5_ghz));
		if (wiphy->bands[IEEE80211_BAND_5GHZ]->channels == NULL) {
			hdd_err("Not enough memory to allocate channels");
			qdf_mem_free(wiphy->
				bands[IEEE80211_BAND_2GHZ]->channels);
			wiphy->bands[IEEE80211_BAND_2GHZ]->channels = NULL;
			return -ENOMEM;
		}
		qdf_mem_copy(wiphy->bands[IEEE80211_BAND_5GHZ]->channels,
			&hdd_channels_5_ghz[0],
			sizeof(hdd_channels_5_ghz));
	}

	for (i = 0; i < IEEE80211_NUM_BANDS; i++) {

		if (NULL == wiphy->bands[i])
			continue;

		for (j = 0; j < wiphy->bands[i]->n_channels; j++) {
			struct ieee80211_supported_band *band = wiphy->bands[i];

			if (IEEE80211_BAND_2GHZ == i &&
				eCSR_BAND_5G == pCfg->nBandCapability) {
				/* 5G only */
#ifdef WLAN_ENABLE_SOCIAL_CHANNELS_5G_ONLY
				/* Enable social channels for P2P */
				if (WLAN_HDD_IS_SOCIAL_CHANNEL
					    (band->channels[j].center_freq))
					band->channels[j].flags &=
						~IEEE80211_CHAN_DISABLED;
				else
#endif
				band->channels[j].flags |=
					IEEE80211_CHAN_DISABLED;
				continue;
			} else if (IEEE80211_BAND_5GHZ == i &&
					eCSR_BAND_24 == pCfg->nBandCapability) {
				/* 2G only */
				band->channels[j].flags |=
					IEEE80211_CHAN_DISABLED;
				continue;
			}
		}
	}
	/*Initialise the supported cipher suite details */
	wiphy->cipher_suites = hdd_cipher_suites;
	wiphy->n_cipher_suites = ARRAY_SIZE(hdd_cipher_suites);

	/*signal strength in mBm (100*dBm) */
	wiphy->signal_type = CFG80211_SIGNAL_TYPE_MBM;
	wiphy->max_remain_on_channel_duration = MAX_REMAIN_ON_CHANNEL_DURATION;

	if (cds_get_conparam() != QDF_GLOBAL_FTM_MODE) {
		wiphy->n_vendor_commands =
				ARRAY_SIZE(hdd_wiphy_vendor_commands);
		wiphy->vendor_commands = hdd_wiphy_vendor_commands;

		wiphy->vendor_events = wlan_hdd_cfg80211_vendor_events;
		wiphy->n_vendor_events =
				ARRAY_SIZE(wlan_hdd_cfg80211_vendor_events);
	}

	if (pCfg->enableDFSMasterCap) {
		wiphy->flags |= WIPHY_FLAG_DFS_OFFLOAD;
	}

	wiphy->max_ap_assoc_sta = pCfg->maxNumberOfPeers;

#ifdef QCA_HT_2040_COEX
	wiphy->features |= NL80211_FEATURE_AP_MODE_CHAN_WIDTH_CHANGE;
#endif

	hdd_add_channel_switch_support(&wiphy->flags);

	EXIT();
	return 0;
}

/**
 * wlan_hdd_cfg80211_deinit - Deinit cfg80211
 * @ wiphy: the wiphy to validate against
 *
 * this function deinit cfg80211 and cleanup the
 * memory allocated in wlan_hdd_cfg80211_init also
 * reset the global reg params.
 *
 * Return: void
 */
void wlan_hdd_cfg80211_deinit(struct wiphy *wiphy)
{
	int i;

	for (i = 0; i < IEEE80211_NUM_BANDS; i++) {
		if (NULL != wiphy->bands[i] &&
		   (NULL != wiphy->bands[i]->channels)) {
			qdf_mem_free(wiphy->bands[i]->channels);
			wiphy->bands[i]->channels = NULL;
		}
	}
	hdd_reset_global_reg_params();
}

/*
 * In this function, wiphy structure is updated after QDF
 * initialization. In wlan_hdd_cfg80211_init, only the
 * default values will be initialized. The final initialization
 * of all required members can be done here.
 */
void wlan_hdd_update_wiphy(struct wiphy *wiphy, struct hdd_config *pCfg)
{
	wiphy->max_ap_assoc_sta = pCfg->maxNumberOfPeers;
}

/* In this function we are registering wiphy. */
int wlan_hdd_cfg80211_register(struct wiphy *wiphy)
{
	ENTER();
	/* Register our wiphy dev with cfg80211 */
	if (0 > wiphy_register(wiphy)) {
		/* print error */
		hdd_err("wiphy register failed");
		return -EIO;
	}

	EXIT();
	return 0;
}

/*
   HDD function to update wiphy capability based on target offload status.

   wlan_hdd_cfg80211_init() does initialization of all wiphy related
   capability even before downloading firmware to the target. In discrete
   case, host will get know certain offload capability (say sched_scan
   caps) only after downloading firmware to the target and target boots up.
   This function is used to override setting done in wlan_hdd_cfg80211_init()
   based on target capability.
 */
void wlan_hdd_cfg80211_update_wiphy_caps(struct wiphy *wiphy)
{
#ifdef FEATURE_WLAN_SCAN_PNO
	hdd_context_t *pHddCtx = wiphy_priv(wiphy);
	struct hdd_config *pCfg = pHddCtx->config;

	/* wlan_hdd_cfg80211_init() sets sched_scan caps already in wiphy before
	 * control comes here. Here just we need to clear it if firmware doesn't
	 * have PNO support. */
	if (!pCfg->PnoOffload) {
		wiphy->flags &= ~WIPHY_FLAG_SUPPORTS_SCHED_SCAN;
		wiphy->max_sched_scan_ssids = 0;
		wiphy->max_match_sets = 0;
		wiphy->max_sched_scan_ie_len = 0;
	}
#endif
}

/* This function registers for all frame which supplicant is interested in */
void wlan_hdd_cfg80211_register_frames(hdd_adapter_t *pAdapter)
{
	tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
	/* Register for all P2P action, public action etc frames */
	uint16_t type = (SIR_MAC_MGMT_FRAME << 2) | (SIR_MAC_MGMT_ACTION << 4);

	ENTER();

	/* Register frame indication call back */
	sme_register_mgmt_frame_ind_callback(hHal, hdd_indicate_mgmt_frame);

	/* Right now we are registering these frame when driver is getting
	   initialized. Once we will move to 2.6.37 kernel, in which we have
	   frame register ops, we will move this code as a part of that */
	/* GAS Initial Request */
	sme_register_mgmt_frame(hHal, SME_SESSION_ID_ANY, type,
				(uint8_t *) GAS_INITIAL_REQ,
				GAS_INITIAL_REQ_SIZE);

	/* GAS Initial Response */
	sme_register_mgmt_frame(hHal, SME_SESSION_ID_ANY, type,
				(uint8_t *) GAS_INITIAL_RSP,
				GAS_INITIAL_RSP_SIZE);

	/* GAS Comeback Request */
	sme_register_mgmt_frame(hHal, SME_SESSION_ID_ANY, type,
				(uint8_t *) GAS_COMEBACK_REQ,
				GAS_COMEBACK_REQ_SIZE);

	/* GAS Comeback Response */
	sme_register_mgmt_frame(hHal, SME_SESSION_ID_ANY, type,
				(uint8_t *) GAS_COMEBACK_RSP,
				GAS_COMEBACK_RSP_SIZE);

	/* P2P Public Action */
	sme_register_mgmt_frame(hHal, SME_SESSION_ID_ANY, type,
				(uint8_t *) P2P_PUBLIC_ACTION_FRAME,
				P2P_PUBLIC_ACTION_FRAME_SIZE);

	/* P2P Action */
	sme_register_mgmt_frame(hHal, SME_SESSION_ID_ANY, type,
				(uint8_t *) P2P_ACTION_FRAME,
				P2P_ACTION_FRAME_SIZE);

	/* WNM BSS Transition Request frame */
	sme_register_mgmt_frame(hHal, SME_SESSION_ID_ANY, type,
				(uint8_t *) WNM_BSS_ACTION_FRAME,
				WNM_BSS_ACTION_FRAME_SIZE);

	/* WNM-Notification */
	sme_register_mgmt_frame(hHal, pAdapter->sessionId, type,
				(uint8_t *) WNM_NOTIFICATION_FRAME,
				WNM_NOTIFICATION_FRAME_SIZE);
}

void wlan_hdd_cfg80211_deregister_frames(hdd_adapter_t *pAdapter)
{
	tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
	/* Register for all P2P action, public action etc frames */
	uint16_t type = (SIR_MAC_MGMT_FRAME << 2) | (SIR_MAC_MGMT_ACTION << 4);

	ENTER();

	/* Right now we are registering these frame when driver is getting
	   initialized. Once we will move to 2.6.37 kernel, in which we have
	   frame register ops, we will move this code as a part of that */
	/* GAS Initial Request */

	sme_deregister_mgmt_frame(hHal, SME_SESSION_ID_ANY, type,
				  (uint8_t *) GAS_INITIAL_REQ,
				  GAS_INITIAL_REQ_SIZE);

	/* GAS Initial Response */
	sme_deregister_mgmt_frame(hHal, SME_SESSION_ID_ANY, type,
				  (uint8_t *) GAS_INITIAL_RSP,
				  GAS_INITIAL_RSP_SIZE);

	/* GAS Comeback Request */
	sme_deregister_mgmt_frame(hHal, SME_SESSION_ID_ANY, type,
				  (uint8_t *) GAS_COMEBACK_REQ,
				  GAS_COMEBACK_REQ_SIZE);

	/* GAS Comeback Response */
	sme_deregister_mgmt_frame(hHal, SME_SESSION_ID_ANY, type,
				  (uint8_t *) GAS_COMEBACK_RSP,
				  GAS_COMEBACK_RSP_SIZE);

	/* P2P Public Action */
	sme_deregister_mgmt_frame(hHal, SME_SESSION_ID_ANY, type,
				  (uint8_t *) P2P_PUBLIC_ACTION_FRAME,
				  P2P_PUBLIC_ACTION_FRAME_SIZE);

	/* P2P Action */
	sme_deregister_mgmt_frame(hHal, SME_SESSION_ID_ANY, type,
				  (uint8_t *) P2P_ACTION_FRAME,
				  P2P_ACTION_FRAME_SIZE);

	/* WNM-Notification */
	sme_deregister_mgmt_frame(hHal, pAdapter->sessionId, type,
				  (uint8_t *) WNM_NOTIFICATION_FRAME,
				  WNM_NOTIFICATION_FRAME_SIZE);
}

#ifdef FEATURE_WLAN_WAPI
void wlan_hdd_cfg80211_set_key_wapi(hdd_adapter_t *pAdapter, uint8_t key_index,
				    const uint8_t *mac_addr, const uint8_t *key,
				    int key_Len)
{
	hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
	tCsrRoamSetKey setKey;
	bool isConnected = true;
	int status = 0;
	uint32_t roamId = 0xFF;
	uint8_t *pKeyPtr = NULL;
	int n = 0;

	hdd_notice("Device_mode %s(%d)",
		hdd_device_mode_to_string(pAdapter->device_mode),
		pAdapter->device_mode);

	qdf_mem_zero(&setKey, sizeof(tCsrRoamSetKey));
	setKey.keyId = key_index;       /* Store Key ID */
	setKey.encType = eCSR_ENCRYPT_TYPE_WPI; /* SET WAPI Encryption */
	setKey.keyDirection = eSIR_TX_RX;       /* Key Directionn both TX and RX */
	setKey.paeRole = 0;     /* the PAE role */
	if (!mac_addr || is_broadcast_ether_addr(mac_addr)) {
		qdf_set_macaddr_broadcast(&setKey.peerMac);
	} else {
		qdf_mem_copy(setKey.peerMac.bytes, mac_addr, QDF_MAC_ADDR_SIZE);
	}
	setKey.keyLength = key_Len;
	pKeyPtr = setKey.Key;
	memcpy(pKeyPtr, key, key_Len);

	hdd_notice("WAPI KEY LENGTH:0x%04x", key_Len);
	for (n = 0; n < key_Len; n++)
		hdd_notice("WAPI KEY Data[%d]:%02x ",
			   n, setKey.Key[n]);

	pHddStaCtx->roam_info.roamingState = HDD_ROAM_STATE_SETTING_KEY;
	if (isConnected) {
		status = sme_roam_set_key(WLAN_HDD_GET_HAL_CTX(pAdapter),
					  pAdapter->sessionId, &setKey, &roamId);
	}
	if (status != 0) {
		hdd_err("sme_roam_set_key returned ERROR status= %d",
			status);
		pHddStaCtx->roam_info.roamingState = HDD_ROAM_STATE_NONE;
	}
}
#endif /* FEATURE_WLAN_WAPI */

uint8_t *wlan_hdd_cfg80211_get_ie_ptr(const uint8_t *ies_ptr, int length,
				      uint8_t eid)
{
	int left = length;
	uint8_t *ptr = (uint8_t *)ies_ptr;
	uint8_t elem_id, elem_len;

	while (left >= 2) {
		elem_id = ptr[0];
		elem_len = ptr[1];
		left -= 2;
		if (elem_len > left) {
			hdd_alert("Invalid IEs eid = %d elem_len=%d left=%d",
			       eid, elem_len, left);
			return NULL;
		}
		if (elem_id == eid) {
			return ptr;
		}

		left -= elem_len;
		ptr += (elem_len + 2);
	}
	return NULL;
}

/*
 * FUNCTION: wlan_hdd_validate_operation_channel
 * called by wlan_hdd_cfg80211_start_bss() and
 * wlan_hdd_set_channel()
 * This function validates whether given channel is part of valid
 * channel list.
 */
QDF_STATUS wlan_hdd_validate_operation_channel(hdd_adapter_t *pAdapter,
					       int channel)
{

	uint32_t num_ch = 0;
	u8 valid_ch[WNI_CFG_VALID_CHANNEL_LIST_LEN];
	u32 indx = 0;
	tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
	uint8_t fValidChannel = false, count = 0;
	struct hdd_config *hdd_pConfig_ini = (WLAN_HDD_GET_CTX(pAdapter))->config;

	num_ch = WNI_CFG_VALID_CHANNEL_LIST_LEN;

	if (hdd_pConfig_ini->sapAllowAllChannel) {
		/* Validate the channel */
		for (count = CHAN_ENUM_1; count <= CHAN_ENUM_165; count++) {
			if (channel == CDS_CHANNEL_NUM(count)) {
				fValidChannel = true;
				break;
			}
		}
		if (fValidChannel != true) {
			hdd_err("Invalid Channel [%d]", channel);
			return QDF_STATUS_E_FAILURE;
		}
	} else {
		if (0 != sme_cfg_get_str(hHal, WNI_CFG_VALID_CHANNEL_LIST,
					 valid_ch, &num_ch)) {
			hdd_err("failed to get valid channel list");
			return QDF_STATUS_E_FAILURE;
		}
		for (indx = 0; indx < num_ch; indx++) {
			if (channel == valid_ch[indx]) {
				break;
			}
		}

		if (indx >= num_ch) {
			hdd_err("Invalid Channel [%d]", channel);
			return QDF_STATUS_E_FAILURE;
		}
	}
	return QDF_STATUS_SUCCESS;

}

#ifdef DHCP_SERVER_OFFLOAD
static void wlan_hdd_set_dhcp_server_offload(hdd_adapter_t *pHostapdAdapter)
{
	hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pHostapdAdapter);
	tpSirDhcpSrvOffloadInfo pDhcpSrvInfo;
	uint8_t numEntries = 0;
	uint8_t srv_ip[IPADDR_NUM_ENTRIES];
	uint8_t num;
	uint32_t temp;
	pDhcpSrvInfo = qdf_mem_malloc(sizeof(*pDhcpSrvInfo));
	if (NULL == pDhcpSrvInfo) {
		hdd_err("could not allocate tDhcpSrvOffloadInfo!");
		return;
	}
	qdf_mem_zero(pDhcpSrvInfo, sizeof(*pDhcpSrvInfo));
	pDhcpSrvInfo->vdev_id = pHostapdAdapter->sessionId;
	pDhcpSrvInfo->dhcpSrvOffloadEnabled = true;
	pDhcpSrvInfo->dhcpClientNum = pHddCtx->config->dhcpMaxNumClients;
	hdd_string_to_u8_array(pHddCtx->config->dhcpServerIP,
			       srv_ip, &numEntries, IPADDR_NUM_ENTRIES);
	if (numEntries != IPADDR_NUM_ENTRIES) {
		hdd_err("incorrect IP address (%s) assigned for DHCP server!", pHddCtx->config->dhcpServerIP);
		goto end;
	}
	if ((srv_ip[0] >= 224) && (srv_ip[0] <= 239)) {
		hdd_err("invalid IP address (%s)! It could NOT be multicast IP address!", pHddCtx->config->dhcpServerIP);
		goto end;
	}
	if (srv_ip[IPADDR_NUM_ENTRIES - 1] >= 100) {
		hdd_err("invalid IP address (%s)! The last field must be less than 100!", pHddCtx->config->dhcpServerIP);
		goto end;
	}
	for (num = 0; num < numEntries; num++) {
		temp = srv_ip[num];
		pDhcpSrvInfo->dhcpSrvIP |= (temp << (8 * num));
	}
	if (QDF_STATUS_SUCCESS !=
	    sme_set_dhcp_srv_offload(pHddCtx->hHal, pDhcpSrvInfo)) {
		hdd_err("sme_setDHCPSrvOffload fail!");
		goto end;
	}
	hdd_info("enable DHCP Server offload successfully!");
end:
	qdf_mem_free(pDhcpSrvInfo);
	return;
}
#endif /* DHCP_SERVER_OFFLOAD */

static int __wlan_hdd_cfg80211_change_bss(struct wiphy *wiphy,
					  struct net_device *dev,
					  struct bss_parameters *params)
{
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
	int ret = 0;
	QDF_STATUS qdf_ret_status;

	ENTER();

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EINVAL;
	}

	MTRACE(qdf_trace(QDF_MODULE_ID_HDD,
			 TRACE_CODE_HDD_CFG80211_CHANGE_BSS,
			 pAdapter->sessionId, params->ap_isolate));
	hdd_notice("Device_mode %s(%d), ap_isolate = %d",
		hdd_device_mode_to_string(pAdapter->device_mode),
		pAdapter->device_mode, params->ap_isolate);

	pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
	ret = wlan_hdd_validate_context(pHddCtx);
	if (0 != ret)
		return ret;

	if (!(pAdapter->device_mode == QDF_SAP_MODE ||
	      pAdapter->device_mode == QDF_P2P_GO_MODE)) {
		return -EOPNOTSUPP;
	}

	/* ap_isolate == -1 means that in change bss, upper layer doesn't
	 * want to update this parameter */
	if (-1 != params->ap_isolate) {
		pAdapter->sessionCtx.ap.apDisableIntraBssFwd =
			!!params->ap_isolate;

		qdf_ret_status = sme_ap_disable_intra_bss_fwd(pHddCtx->hHal,
							      pAdapter->sessionId,
							      pAdapter->sessionCtx.
							      ap.
							      apDisableIntraBssFwd);
		if (!QDF_IS_STATUS_SUCCESS(qdf_ret_status)) {
			ret = -EINVAL;
		}
	}

	EXIT();
	return ret;
}

/**
 * wlan_hdd_change_client_iface_to_new_mode() - to change iface to provided mode
 * @ndev: pointer to net device provided by supplicant
 * @type: type of the interface, upper layer wanted to change
 *
 * Upper layer provides the new interface mode that needs to be changed
 * for given net device
 *
 * Return: success or failure in terms of integer value
 */
static int wlan_hdd_change_client_iface_to_new_mode(struct net_device *ndev,
					     enum nl80211_iftype type)
{
	hdd_adapter_t *adapter = WLAN_HDD_GET_PRIV_PTR(ndev);
	hdd_context_t *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct hdd_config *config = hdd_ctx->config;
	hdd_wext_state_t *wext;
	struct wireless_dev *wdev;
	QDF_STATUS status;

	ENTER();

	if (test_bit(ACS_IN_PROGRESS, &hdd_ctx->g_event_flags)) {
		hdd_notice("ACS is in progress, don't change iface!");
		return 0;
	}

	wdev = ndev->ieee80211_ptr;
	hdd_stop_adapter(hdd_ctx, adapter, true);
	hdd_deinit_adapter(hdd_ctx, adapter, true);
	wdev->iftype = type;
	/*Check for sub-string p2p to confirm its a p2p interface */
	if (NULL != strnstr(ndev->name, "p2p", 3)) {
		adapter->device_mode =
			(type == NL80211_IFTYPE_STATION) ?
			QDF_P2P_DEVICE_MODE : QDF_P2P_CLIENT_MODE;
	} else if (type == NL80211_IFTYPE_ADHOC) {
		adapter->device_mode = QDF_IBSS_MODE;
	} else {
		adapter->device_mode =
			(type == NL80211_IFTYPE_STATION) ?
			QDF_STA_MODE : QDF_P2P_CLIENT_MODE;
	}
	memset(&adapter->sessionCtx, 0, sizeof(adapter->sessionCtx));
	hdd_set_station_ops(adapter->dev);
	status = hdd_init_station_mode(adapter);
	wext = WLAN_HDD_GET_WEXT_STATE_PTR(adapter);
	wext->roamProfile.pAddIEScan = adapter->scan_info.scanAddIE.addIEdata;
	wext->roamProfile.nAddIEScanLength =
		adapter->scan_info.scanAddIE.length;
	if (type == NL80211_IFTYPE_ADHOC) {
		wext->roamProfile.BSSType = eCSR_BSS_TYPE_START_IBSS;
		wext->roamProfile.phyMode =
			hdd_cfg_xlate_to_csr_phy_mode(config->dot11Mode);
	}
	EXIT();
	return status;
}

static int wlan_hdd_cfg80211_change_bss(struct wiphy *wiphy,
					struct net_device *dev,
					struct bss_parameters *params)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_change_bss(wiphy, dev, params);
	cds_ssr_unprotect(__func__);

	return ret;
}

/* FUNCTION: wlan_hdd_change_country_code_cd
 *  to wait for contry code completion
 */
void *wlan_hdd_change_country_code_cb(void *pAdapter)
{
	hdd_adapter_t *call_back_pAdapter = pAdapter;
	complete(&call_back_pAdapter->change_country_code);
	return NULL;
}

/**
 * __wlan_hdd_cfg80211_change_iface() - change interface cfg80211 op
 * @wiphy: Pointer to the wiphy structure
 * @ndev: Pointer to the net device
 * @type: Interface type
 * @flags: Flags for change interface
 * @params: Pointer to change interface parameters
 *
 * Return: 0 for success, error number on failure.
 */
static int __wlan_hdd_cfg80211_change_iface(struct wiphy *wiphy,
					    struct net_device *ndev,
					    enum nl80211_iftype type,
					    u32 *flags,
					    struct vif_params *params)
{
	struct wireless_dev *wdev;
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(ndev);
	hdd_context_t *pHddCtx;
	tCsrRoamProfile *pRoamProfile = NULL;
	eCsrRoamBssType LastBSSType;
	struct hdd_config *pConfig = NULL;
	QDF_STATUS vstatus;
	int status;

	ENTER();

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EINVAL;
	}

	pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
	status = wlan_hdd_validate_context(pHddCtx);
	if (0 != status)
		return status;

	MTRACE(qdf_trace(QDF_MODULE_ID_HDD,
			 TRACE_CODE_HDD_CFG80211_CHANGE_IFACE,
			 pAdapter->sessionId, type));

	hdd_notice("Device_mode = %d, IFTYPE = 0x%x",
	       pAdapter->device_mode, type);

	if (!cds_allow_concurrency(
				wlan_hdd_convert_nl_iftype_to_hdd_type(type),
				0, HW_MODE_20_MHZ)) {
		hdd_debug("This concurrency combination is not allowed");
		return -EINVAL;
	}

	pConfig = pHddCtx->config;
	wdev = ndev->ieee80211_ptr;

	/* Reset the current device mode bit mask */
	cds_clear_concurrency_mode(pAdapter->device_mode);

	hdd_tdls_notify_mode_change(pAdapter, pHddCtx);

	if ((pAdapter->device_mode == QDF_STA_MODE) ||
	    (pAdapter->device_mode == QDF_P2P_CLIENT_MODE) ||
	    (pAdapter->device_mode == QDF_P2P_DEVICE_MODE) ||
	    (pAdapter->device_mode == QDF_IBSS_MODE)) {
		hdd_wext_state_t *pWextState =
			WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);

		pRoamProfile = &pWextState->roamProfile;
		LastBSSType = pRoamProfile->BSSType;

		switch (type) {
		case NL80211_IFTYPE_STATION:
		case NL80211_IFTYPE_P2P_CLIENT:
		case NL80211_IFTYPE_ADHOC:
			if (type == NL80211_IFTYPE_ADHOC) {
				wlan_hdd_tdls_exit(pAdapter);
				hdd_deregister_tx_flow_control(pAdapter);
				hdd_notice("Setting interface Type to ADHOC");
			}
			vstatus = wlan_hdd_change_client_iface_to_new_mode(ndev,
					type);
			if (vstatus != QDF_STATUS_SUCCESS)
				return -EINVAL;

			/*
			 * for ibss interface type flow control is not required
			 * so don't register tx flow control
			 */
			if (type != NL80211_IFTYPE_ADHOC)
				hdd_register_tx_flow_control(pAdapter,
					hdd_tx_resume_timer_expired_handler,
					hdd_tx_resume_cb);

			goto done;
		case NL80211_IFTYPE_AP:
		case NL80211_IFTYPE_P2P_GO:
		{
			hdd_info("Setting interface Type to %s",
			       (type ==
				NL80211_IFTYPE_AP) ? "SoftAP" :
			       "P2pGo");

			/* Cancel any remain on channel for GO mode */
			if (NL80211_IFTYPE_P2P_GO == type) {
				wlan_hdd_cancel_existing_remain_on_channel
					(pAdapter);
			}
			hdd_stop_adapter(pHddCtx, pAdapter, true);

			/* De-init the adapter */
			hdd_deinit_adapter(pHddCtx, pAdapter, true);
			memset(&pAdapter->sessionCtx, 0,
			       sizeof(pAdapter->sessionCtx));
			pAdapter->device_mode =
				(type ==
				 NL80211_IFTYPE_AP) ? QDF_SAP_MODE :
				QDF_P2P_GO_MODE;

			/*
			 * Fw will take care incase of concurrency
			 */

			if ((QDF_SAP_MODE == pAdapter->device_mode)
			    && (pConfig->apRandomBssidEnabled)) {
				/* To meet Android requirements create a randomized
				   MAC address of the form 02:1A:11:Fx:xx:xx */
				get_random_bytes(&ndev->dev_addr[3], 3);
				ndev->dev_addr[0] = 0x02;
				ndev->dev_addr[1] = 0x1A;
				ndev->dev_addr[2] = 0x11;
				ndev->dev_addr[3] |= 0xF0;
				memcpy(pAdapter->macAddressCurrent.
				       bytes, ndev->dev_addr,
				       QDF_MAC_ADDR_SIZE);
				pr_info("wlan: Generated HotSpot BSSID "
					MAC_ADDRESS_STR "\n",
					MAC_ADDR_ARRAY(ndev->dev_addr));
			}

			hdd_set_ap_ops(pAdapter->dev);

			vstatus = hdd_init_ap_mode(pAdapter);
			if (vstatus != QDF_STATUS_SUCCESS) {
				hdd_alert("Error initializing the ap mode");
				return -EINVAL;
			}

			hdd_register_tx_flow_control(pAdapter,
				hdd_softap_tx_resume_timer_expired_handler,
				hdd_softap_tx_resume_cb);

			/* Interface type changed update in wiphy structure */
			if (wdev) {
				wdev->iftype = type;
			} else {
				hdd_err("Wireless dev is NULL");
				return -EINVAL;
			}
			goto done;
		}

		default:
			hdd_err("Unsupported interface type (%d)",
			       type);
			return -EOPNOTSUPP;
		}
	} else if ((pAdapter->device_mode == QDF_SAP_MODE) ||
		   (pAdapter->device_mode == QDF_P2P_GO_MODE)) {
		switch (type) {
		case NL80211_IFTYPE_STATION:
		case NL80211_IFTYPE_P2P_CLIENT:
		case NL80211_IFTYPE_ADHOC:
			status = wlan_hdd_change_client_iface_to_new_mode(ndev,
					type);
			if (status != QDF_STATUS_SUCCESS)
				return status;

			if ((NL80211_IFTYPE_P2P_CLIENT == type) ||
			    (NL80211_IFTYPE_STATION == type)) {

				hdd_register_tx_flow_control(pAdapter,
					hdd_tx_resume_timer_expired_handler,
					hdd_tx_resume_cb);
			}
			goto done;

		case NL80211_IFTYPE_AP:
		case NL80211_IFTYPE_P2P_GO:
			wdev->iftype = type;
			pAdapter->device_mode = (type == NL80211_IFTYPE_AP) ?
						QDF_SAP_MODE : QDF_P2P_GO_MODE;

			hdd_register_tx_flow_control(pAdapter,
				hdd_softap_tx_resume_timer_expired_handler,
				hdd_softap_tx_resume_cb);
			goto done;

		default:
			hdd_err("Unsupported interface type(%d)",
			       type);
			return -EOPNOTSUPP;
		}
	} else {
		hdd_err("Unsupported device mode(%d)",
		       pAdapter->device_mode);
		return -EOPNOTSUPP;
	}
done:
	/* Set bitmask based on updated value */
	cds_set_concurrency_mode(pAdapter->device_mode);

	wlan_hdd_send_all_scan_intf_info(pHddCtx);

	EXIT();
	return 0;
}

/**
 * wlan_hdd_cfg80211_change_iface() - change interface cfg80211 op
 * @wiphy: Pointer to the wiphy structure
 * @ndev: Pointer to the net device
 * @type: Interface type
 * @flags: Flags for change interface
 * @params: Pointer to change interface parameters
 *
 * Return: 0 for success, error number on failure.
 */
static int wlan_hdd_cfg80211_change_iface(struct wiphy *wiphy,
					  struct net_device *ndev,
					  enum nl80211_iftype type,
					  u32 *flags,
					  struct vif_params *params)
{
	int ret;

	cds_ssr_protect(__func__);
	ret =
		__wlan_hdd_cfg80211_change_iface(wiphy, ndev, type, flags, params);
	cds_ssr_unprotect(__func__);

	return ret;
}

#ifdef FEATURE_WLAN_TDLS
static bool wlan_hdd_is_duplicate_channel(uint8_t *arr,
					  int index, uint8_t match)
{
	int i;
	for (i = 0; i < index; i++) {
		if (arr[i] == match)
			return true;
	}
	return false;
}
#endif

/**
 * __wlan_hdd_change_station() - change station
 * @wiphy: Pointer to the wiphy structure
 * @dev: Pointer to the net device.
 * @mac: bssid
 * @params: Pointer to station parameters
 *
 * Return: 0 for success, error number on failure.
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0))
static int __wlan_hdd_change_station(struct wiphy *wiphy,
				   struct net_device *dev,
				   const uint8_t *mac,
				   struct station_parameters *params)
#else
static int __wlan_hdd_change_station(struct wiphy *wiphy,
				   struct net_device *dev,
				   uint8_t *mac,
				   struct station_parameters *params)
#endif
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *pHddCtx;
	hdd_station_ctx_t *pHddStaCtx;
	struct qdf_mac_addr STAMacAddress;
#ifdef FEATURE_WLAN_TDLS
	tCsrStaParams StaParams = { 0 };
	uint8_t isBufSta = 0;
	uint8_t isOffChannelSupported = 0;
#endif
	int ret;

	ENTER();

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EINVAL;
	}

	MTRACE(qdf_trace(QDF_MODULE_ID_HDD,
			 TRACE_CODE_HDD_CHANGE_STATION,
			 pAdapter->sessionId, params->listen_interval));

	pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
	ret = wlan_hdd_validate_context(pHddCtx);
	if (0 != ret)
		return ret;

	pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

	qdf_mem_copy(STAMacAddress.bytes, mac, QDF_MAC_ADDR_SIZE);

	if ((pAdapter->device_mode == QDF_SAP_MODE) ||
	    (pAdapter->device_mode == QDF_P2P_GO_MODE)) {
		if (params->sta_flags_set & BIT(NL80211_STA_FLAG_AUTHORIZED)) {
			status =
				hdd_softap_change_sta_state(pAdapter,
							    &STAMacAddress,
							    OL_TXRX_PEER_STATE_AUTH);

			if (status != QDF_STATUS_SUCCESS) {
				hdd_notice("Not able to change TL state to AUTHENTICATED");
				return -EINVAL;
			}
		}
	} else if ((pAdapter->device_mode == QDF_STA_MODE) ||
		   (pAdapter->device_mode == QDF_P2P_CLIENT_MODE)) {
#ifdef FEATURE_WLAN_TDLS
		if (params->sta_flags_set & BIT(NL80211_STA_FLAG_TDLS_PEER)) {
			StaParams.capability = params->capability;
			StaParams.uapsd_queues = params->uapsd_queues;
			StaParams.max_sp = params->max_sp;

			/* Convert (first channel , number of channels) tuple to
			 * the total list of channels. This goes with the assumption
			 * that if the first channel is < 14, then the next channels
			 * are an incremental of 1 else an incremental of 4 till the number
			 * of channels.
			 */
			hdd_notice("params->supported_channels_len: %d", params->supported_channels_len);
			if (0 != params->supported_channels_len) {
				int i = 0, j = 0, k = 0, no_of_channels = 0;
				int num_unique_channels;
				int next;
				for (i = 0;
				     i < params->supported_channels_len
				     && j < SIR_MAC_MAX_SUPP_CHANNELS; i += 2) {
					int wifi_chan_index;
					if (!wlan_hdd_is_duplicate_channel
						    (StaParams.supported_channels, j,
						    params->supported_channels[i])) {
						StaParams.
						supported_channels[j] =
							params->
							supported_channels[i];
					} else {
						continue;
					}
					wifi_chan_index =
						((StaParams.supported_channels[j] <=
						  HDD_CHANNEL_14) ? 1 : 4);
					no_of_channels =
						params->supported_channels[i + 1];

					hdd_notice("i: %d, j: %d, k: %d, StaParams.supported_channels[%d]: %d, wifi_chan_index: %d, no_of_channels: %d", i, j, k, j,
						  StaParams.
						  supported_channels[j],
						  wifi_chan_index,
						  no_of_channels);
					for (k = 1; k <= no_of_channels &&
					     j < SIR_MAC_MAX_SUPP_CHANNELS - 1;
					     k++) {
						next =
								StaParams.
								supported_channels[j] +
								wifi_chan_index;
						if (!wlan_hdd_is_duplicate_channel(StaParams.supported_channels, j + 1, next)) {
							StaParams.
							supported_channels[j
									   +
									   1]
								= next;
						} else {
							continue;
						}
						hdd_notice("i: %d, j: %d, k: %d, StaParams.supported_channels[%d]: %d", i, j, k,
							  j + 1,
							  StaParams.
							  supported_channels[j +
									     1]);
						j += 1;
					}
				}
				num_unique_channels = j + 1;
				hdd_notice("Unique Channel List");
				for (i = 0; i < num_unique_channels; i++) {
					hdd_notice("StaParams.supported_channels[%d]: %d,", i,
						  StaParams.
						  supported_channels[i]);
				}
				if (MAX_CHANNEL < num_unique_channels)
					num_unique_channels = MAX_CHANNEL;
				StaParams.supported_channels_len =
					num_unique_channels;
				hdd_notice("After removing duplcates StaParams.supported_channels_len: %d",
					  StaParams.supported_channels_len);
			}
			qdf_mem_copy(StaParams.supported_oper_classes,
				     params->supported_oper_classes,
				     params->supported_oper_classes_len);
			StaParams.supported_oper_classes_len =
				params->supported_oper_classes_len;

			if (0 != params->ext_capab_len)
				qdf_mem_copy(StaParams.extn_capability,
					     params->ext_capab,
					     sizeof(StaParams.extn_capability));

			if (NULL != params->ht_capa) {
				StaParams.htcap_present = 1;
				qdf_mem_copy(&StaParams.HTCap, params->ht_capa,
					     sizeof(tSirHTCap));
			}

			StaParams.supported_rates_len =
				params->supported_rates_len;

			/* Note : The Maximum sizeof supported_rates sent by the Supplicant is 32.
			 * The supported_rates array , for all the structures propogating till Add Sta
			 * to the firmware has to be modified , if the supplicant (ieee80211) is
			 * modified to send more rates.
			 */

			/* To avoid Data Currption , set to max length to SIR_MAC_MAX_SUPP_RATES
			 */
			if (StaParams.supported_rates_len >
			    SIR_MAC_MAX_SUPP_RATES)
				StaParams.supported_rates_len =
					SIR_MAC_MAX_SUPP_RATES;

			if (0 != StaParams.supported_rates_len) {
				int i = 0;
				qdf_mem_copy(StaParams.supported_rates,
					     params->supported_rates,
					     StaParams.supported_rates_len);
				hdd_notice("Supported Rates with Length %d",
					  StaParams.supported_rates_len);
				for (i = 0; i < StaParams.supported_rates_len;
				     i++)
					hdd_notice("[%d]: %0x", i,
						  StaParams.supported_rates[i]);
			}

			if (NULL != params->vht_capa) {
				StaParams.vhtcap_present = 1;
				qdf_mem_copy(&StaParams.VHTCap,
					     params->vht_capa,
					     sizeof(tSirVHTCap));
			}

			if (0 != params->ext_capab_len) {
				/*Define A Macro : TODO Sunil */
				if ((1 << 4) & StaParams.extn_capability[3]) {
					isBufSta = 1;
				}
				/* TDLS Channel Switching Support */
				if ((1 << 6) & StaParams.extn_capability[3]) {
					isOffChannelSupported = 1;
				}
			}

			status = wlan_hdd_tdls_set_peer_caps(pAdapter, mac,
							     &StaParams,
							     isBufSta,
							     isOffChannelSupported);
			if (QDF_STATUS_SUCCESS != status) {
				hdd_err("wlan_hdd_tdls_set_peer_caps failed!");
				return -EINVAL;
			}

			status =
				wlan_hdd_tdls_add_station(wiphy, dev, mac, 1,
							  &StaParams);
			if (QDF_STATUS_SUCCESS != status) {
				hdd_err("wlan_hdd_tdls_add_station failed!");
				return -EINVAL;
			}
		}
#endif
	}
	EXIT();
	return ret;
}

/**
 * wlan_hdd_change_station() - cfg80211 change station handler function
 * @wiphy: Pointer to the wiphy structure
 * @dev: Pointer to the net device.
 * @mac: bssid
 * @params: Pointer to station parameters
 *
 * This is the cfg80211 change station handler function which invokes
 * the internal function @__wlan_hdd_change_station with
 * SSR protection.
 *
 * Return: 0 for success, error number on failure.
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)) || defined(WITH_BACKPORTS)
static int wlan_hdd_change_station(struct wiphy *wiphy,
				   struct net_device *dev,
				   const u8 *mac,
				   struct station_parameters *params)
#else
static int wlan_hdd_change_station(struct wiphy *wiphy,
				   struct net_device *dev,
				   u8 *mac,
				   struct station_parameters *params)
#endif
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_change_station(wiphy, dev, mac, params);
	cds_ssr_unprotect(__func__);

	return ret;
}

/*
 * FUNCTION: __wlan_hdd_cfg80211_add_key
 * This function is used to initialize the key information
 */
static int __wlan_hdd_cfg80211_add_key(struct wiphy *wiphy,
				       struct net_device *ndev,
				       u8 key_index, bool pairwise,
				       const u8 *mac_addr,
				       struct key_params *params)
{
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(ndev);
	tCsrRoamSetKey setKey;
	int status;
	uint32_t roamId = 0xFF;
#ifndef WLAN_FEATURE_MBSSID
	v_CONTEXT_t p_cds_context = (WLAN_HDD_GET_CTX(pAdapter))->pcds_context;
#endif
	hdd_hostapd_state_t *pHostapdState;
	QDF_STATUS qdf_ret_status;
	hdd_context_t *pHddCtx;
	hdd_ap_ctx_t *ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(pAdapter);

	ENTER();

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EINVAL;
	}

	MTRACE(qdf_trace(QDF_MODULE_ID_HDD,
			 TRACE_CODE_HDD_CFG80211_ADD_KEY,
			 pAdapter->sessionId, params->key_len));
	pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
	status = wlan_hdd_validate_context(pHddCtx);

	if (0 != status)
		return status;

	hdd_notice("Device_mode %s(%d)",
		hdd_device_mode_to_string(pAdapter->device_mode),
		pAdapter->device_mode);

	if (CSR_MAX_NUM_KEY <= key_index) {
		hdd_err("Invalid key index %d", key_index);

		return -EINVAL;
	}

	if (CSR_MAX_KEY_LEN < params->key_len) {
		hdd_err("Invalid key length %d", params->key_len);

		return -EINVAL;
	}

	hdd_notice("called with key index = %d & key length %d", key_index, params->key_len);

	/*extract key idx, key len and key */
	qdf_mem_zero(&setKey, sizeof(tCsrRoamSetKey));
	setKey.keyId = key_index;
	setKey.keyLength = params->key_len;
	qdf_mem_copy(&setKey.Key[0], params->key, params->key_len);

	switch (params->cipher) {
	case WLAN_CIPHER_SUITE_WEP40:
		setKey.encType = eCSR_ENCRYPT_TYPE_WEP40_STATICKEY;
		break;

	case WLAN_CIPHER_SUITE_WEP104:
		setKey.encType = eCSR_ENCRYPT_TYPE_WEP104_STATICKEY;
		break;

	case WLAN_CIPHER_SUITE_TKIP:
	{
		u8 *pKey = &setKey.Key[0];
		setKey.encType = eCSR_ENCRYPT_TYPE_TKIP;

		qdf_mem_zero(pKey, CSR_MAX_KEY_LEN);

		/*Supplicant sends the 32bytes key in this order

		   |--------------|----------|----------|
		 |   Tk1        |TX-MIC    |  RX Mic  |
		 |||--------------|----------|----------|
		   <---16bytes---><--8bytes--><--8bytes-->

		 */
		/*Sme expects the 32 bytes key to be in the below order

		   |--------------|----------|----------|
		 |   Tk1        |RX-MIC    |  TX Mic  |
		 |||--------------|----------|----------|
		   <---16bytes---><--8bytes--><--8bytes-->
		 */
		/* Copy the Temporal Key 1 (TK1) */
		qdf_mem_copy(pKey, params->key, 16);

		/*Copy the rx mic first */
		qdf_mem_copy(&pKey[16], &params->key[24], 8);

		/*Copy the tx mic */
		qdf_mem_copy(&pKey[24], &params->key[16], 8);

		break;
	}

	case WLAN_CIPHER_SUITE_CCMP:
		setKey.encType = eCSR_ENCRYPT_TYPE_AES;
		break;

#ifdef FEATURE_WLAN_WAPI
	case WLAN_CIPHER_SUITE_SMS4:
	{
		qdf_mem_zero(&setKey, sizeof(tCsrRoamSetKey));
		wlan_hdd_cfg80211_set_key_wapi(pAdapter, key_index,
					       mac_addr, params->key,
					       params->key_len);
		return 0;
	}
#endif

#ifdef FEATURE_WLAN_ESE
	case WLAN_CIPHER_SUITE_KRK:
		setKey.encType = eCSR_ENCRYPT_TYPE_KRK;
		break;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	case WLAN_CIPHER_SUITE_BTK:
		setKey.encType = eCSR_ENCRYPT_TYPE_BTK;
		break;
#endif
#endif

#ifdef WLAN_FEATURE_11W
	case WLAN_CIPHER_SUITE_AES_CMAC:
		setKey.encType = eCSR_ENCRYPT_TYPE_AES_CMAC;
		break;
#endif

	default:
		hdd_err("unsupported cipher type %u", params->cipher);
		return -EOPNOTSUPP;
	}

	hdd_info("encryption type %d", setKey.encType);

	if (!pairwise) {
		/* set group key */
		hdd_notice("%s- %d: setting Broadcast key", __func__, __LINE__);
		setKey.keyDirection = eSIR_RX_ONLY;
		qdf_set_macaddr_broadcast(&setKey.peerMac);
	} else {
		/* set pairwise key */
		hdd_notice("%s- %d: setting pairwise key", __func__, __LINE__);
		setKey.keyDirection = eSIR_TX_RX;
		qdf_mem_copy(setKey.peerMac.bytes, mac_addr, QDF_MAC_ADDR_SIZE);
	}
	if ((QDF_IBSS_MODE == pAdapter->device_mode) && !pairwise) {
		/* if a key is already installed, block all subsequent ones */
		if (pAdapter->sessionCtx.station.ibss_enc_key_installed) {
			hdd_info("IBSS key installed already");
			return 0;
		}

		setKey.keyDirection = eSIR_TX_RX;
		/*Set the group key */
		status = sme_roam_set_key(WLAN_HDD_GET_HAL_CTX(pAdapter),
					  pAdapter->sessionId, &setKey, &roamId);

		if (0 != status) {
			hdd_err("sme_roam_set_key failed, returned %d", status);
			return -EINVAL;
		}
		/*Save the keys here and call sme_roam_set_key for setting
		   the PTK after peer joins the IBSS network */
		qdf_mem_copy(&pAdapter->sessionCtx.station.ibss_enc_key,
			     &setKey, sizeof(tCsrRoamSetKey));

		pAdapter->sessionCtx.station.ibss_enc_key_installed = 1;
		return status;
	}
	if ((pAdapter->device_mode == QDF_SAP_MODE) ||
	    (pAdapter->device_mode == QDF_P2P_GO_MODE)) {
		pHostapdState = WLAN_HDD_GET_HOSTAP_STATE_PTR(pAdapter);
		if (pHostapdState->bssState == BSS_START) {
#ifdef WLAN_FEATURE_MBSSID
			status =
				wlansap_set_key_sta(WLAN_HDD_GET_SAP_CTX_PTR
							    (pAdapter), &setKey);
#else
			status = wlansap_set_key_sta(p_cds_context, &setKey);
#endif
			if (status != QDF_STATUS_SUCCESS) {
				hdd_err("[%4d] wlansap_set_key_sta returned ERROR status= %d",
					  __LINE__, status);
			}
		}

		/* Save the key in ap ctx for use on START_BASS and restart */
		if (pairwise ||
			eCSR_ENCRYPT_TYPE_WEP40_STATICKEY == setKey.encType ||
			eCSR_ENCRYPT_TYPE_WEP104_STATICKEY == setKey.encType)
			qdf_mem_copy(&ap_ctx->wepKey[key_index], &setKey,
				     sizeof(tCsrRoamSetKey));
		else
			qdf_mem_copy(&ap_ctx->groupKey, &setKey,
				     sizeof(tCsrRoamSetKey));

	} else if ((pAdapter->device_mode == QDF_STA_MODE) ||
		   (pAdapter->device_mode == QDF_P2P_CLIENT_MODE)) {
		hdd_wext_state_t *pWextState =
			WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
		hdd_station_ctx_t *pHddStaCtx =
			WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

		if (!pairwise) {
			/* set group key */
			if (pHddStaCtx->roam_info.deferKeyComplete) {
				hdd_notice("%s- %d: Perform Set key Complete",
					  __func__, __LINE__);
				hdd_perform_roam_set_key_complete(pAdapter);
			}
		}

		pWextState->roamProfile.Keys.KeyLength[key_index] =
			(u8) params->key_len;

		pWextState->roamProfile.Keys.defaultIndex = key_index;

		qdf_mem_copy(&pWextState->roamProfile.Keys.
			     KeyMaterial[key_index][0], params->key,
			     params->key_len);

		pHddStaCtx->roam_info.roamingState = HDD_ROAM_STATE_SETTING_KEY;

		hdd_info("Set key for peerMac "MAC_ADDRESS_STR" direction %d",
		       MAC_ADDR_ARRAY(setKey.peerMac.bytes),
		       setKey.keyDirection);

		/* The supplicant may attempt to set the PTK once pre-authentication
		   is done. Save the key in the UMAC and include it in the ADD BSS
		   request */
		qdf_ret_status = sme_ft_update_key(WLAN_HDD_GET_HAL_CTX(pAdapter),
						   pAdapter->sessionId, &setKey);
		if (qdf_ret_status == QDF_STATUS_FT_PREAUTH_KEY_SUCCESS) {
			hdd_info("Update PreAuth Key success");
			return 0;
		} else if (qdf_ret_status == QDF_STATUS_FT_PREAUTH_KEY_FAILED) {
			hdd_err("Update PreAuth Key failed");
			return -EINVAL;
		}

		/* issue set key request to SME */
		status = sme_roam_set_key(WLAN_HDD_GET_HAL_CTX(pAdapter),
					  pAdapter->sessionId, &setKey, &roamId);

		if (0 != status) {
			hdd_err("sme_roam_set_key failed, returned %d", status);
			pHddStaCtx->roam_info.roamingState =
				HDD_ROAM_STATE_NONE;
			return -EINVAL;
		}

		/* in case of IBSS as there was no information available about WEP keys during
		 * IBSS join, group key intialized with NULL key, so re-initialize group key
		 * with correct value*/
		if ((eCSR_BSS_TYPE_START_IBSS ==
		     pWextState->roamProfile.BSSType)
		    &&
		    !((IW_AUTH_KEY_MGMT_802_1X ==
		       (pWextState->authKeyMgmt & IW_AUTH_KEY_MGMT_802_1X))
		      && (eCSR_AUTH_TYPE_OPEN_SYSTEM ==
			  pHddStaCtx->conn_info.authType)
		      )
		    && ((WLAN_CIPHER_SUITE_WEP40 == params->cipher)
			|| (WLAN_CIPHER_SUITE_WEP104 == params->cipher)
			)
		    ) {
			setKey.keyDirection = eSIR_RX_ONLY;
			qdf_set_macaddr_broadcast(&setKey.peerMac);

			hdd_info("Set key peerMac "MAC_ADDRESS_STR" direction %d",
			       MAC_ADDR_ARRAY(setKey.peerMac.bytes),
			       setKey.keyDirection);

			status = sme_roam_set_key(WLAN_HDD_GET_HAL_CTX(pAdapter),
						  pAdapter->sessionId, &setKey,
						  &roamId);

			if (0 != status) {
				hdd_err("sme_roam_set_key failed for group key (IBSS), returned %d", status);
				pHddStaCtx->roam_info.roamingState =
					HDD_ROAM_STATE_NONE;
				return -EINVAL;
			}
		}
	}
	EXIT();
	return 0;
}

static int wlan_hdd_cfg80211_add_key(struct wiphy *wiphy,
				     struct net_device *ndev,
				     u8 key_index, bool pairwise,
				     const u8 *mac_addr,
				     struct key_params *params)
{
	int ret;
	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_add_key(wiphy, ndev, key_index, pairwise,
					  mac_addr, params);
	cds_ssr_unprotect(__func__);

	return ret;
}

/*
 * FUNCTION: __wlan_hdd_cfg80211_get_key
 * This function is used to get the key information
 */
static int __wlan_hdd_cfg80211_get_key(struct wiphy *wiphy,
				       struct net_device *ndev,
				       u8 key_index, bool pairwise,
				       const u8 *mac_addr, void *cookie,
				       void (*callback)(void *cookie,
							struct key_params *)
				       )
{
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(ndev);
	hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
	tCsrRoamProfile *pRoamProfile = &(pWextState->roamProfile);
	struct key_params params;

	ENTER();

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EINVAL;
	}

	hdd_notice("Device_mode %s(%d)",
		hdd_device_mode_to_string(pAdapter->device_mode),
		pAdapter->device_mode);

	memset(&params, 0, sizeof(params));

	if (CSR_MAX_NUM_KEY <= key_index) {
		hdd_err("invalid key index %d",
		       key_index);
		return -EINVAL;
	}

	switch (pRoamProfile->EncryptionType.encryptionType[0]) {
	case eCSR_ENCRYPT_TYPE_NONE:
		params.cipher = IW_AUTH_CIPHER_NONE;
		break;

	case eCSR_ENCRYPT_TYPE_WEP40_STATICKEY:
	case eCSR_ENCRYPT_TYPE_WEP40:
		params.cipher = WLAN_CIPHER_SUITE_WEP40;
		break;

	case eCSR_ENCRYPT_TYPE_WEP104_STATICKEY:
	case eCSR_ENCRYPT_TYPE_WEP104:
		params.cipher = WLAN_CIPHER_SUITE_WEP104;
		break;

	case eCSR_ENCRYPT_TYPE_TKIP:
		params.cipher = WLAN_CIPHER_SUITE_TKIP;
		break;

	case eCSR_ENCRYPT_TYPE_AES:
		params.cipher = WLAN_CIPHER_SUITE_AES_CMAC;
		break;

	default:
		params.cipher = IW_AUTH_CIPHER_NONE;
		break;
	}

	MTRACE(qdf_trace(QDF_MODULE_ID_HDD,
			 TRACE_CODE_HDD_CFG80211_GET_KEY,
			 pAdapter->sessionId, params.cipher));

	params.key_len = pRoamProfile->Keys.KeyLength[key_index];
	params.seq_len = 0;
	params.seq = NULL;
	params.key = &pRoamProfile->Keys.KeyMaterial[key_index][0];
	callback(cookie, &params);

	EXIT();
	return 0;
}

static int wlan_hdd_cfg80211_get_key(struct wiphy *wiphy,
				     struct net_device *ndev,
				     u8 key_index, bool pairwise,
				     const u8 *mac_addr, void *cookie,
				     void (*callback)(void *cookie,
						      struct key_params *)
				     )
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_get_key(wiphy, ndev, key_index, pairwise,
					  mac_addr, cookie, callback);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * __wlan_hdd_cfg80211_del_key() - Delete the encryption key for station
 * @wiphy: wiphy interface context
 * @ndev: pointer to net device
 * @key_index: Key index used in 802.11 frames
 * @unicast: true if it is unicast key
 * @multicast: true if it is multicast key
 *
 * This function is required for cfg80211_ops API.
 * It is used to delete the key information
 * Underlying hardware implementation does not have API to delete the
 * encryption key. It is automatically deleted when the peer is
 * removed. Hence this function currently does nothing.
 * Future implementation may interprete delete key operation to
 * replacing the key with a random junk value, effectively making it
 * useless.
 *
 * Return: status code, always 0.
 */

static int __wlan_hdd_cfg80211_del_key(struct wiphy *wiphy,
				     struct net_device *ndev,
				     u8 key_index,
				     bool pairwise, const u8 *mac_addr)
{
	EXIT();
	return 0;
}

/**
 * wlan_hdd_cfg80211_del_key() - cfg80211 delete key handler function
 * @wiphy: Pointer to wiphy structure.
 * @dev: Pointer to net_device structure.
 * @key_index: key index
 * @pairwise: pairwise
 * @mac_addr: mac address
 *
 * This is the cfg80211 delete key handler function which invokes
 * the internal function @__wlan_hdd_cfg80211_del_key with
 * SSR protection.
 *
 * Return: 0 for success, error number on failure.
 */
static int wlan_hdd_cfg80211_del_key(struct wiphy *wiphy,
					struct net_device *dev,
					u8 key_index,
					bool pairwise, const u8 *mac_addr)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_del_key(wiphy, dev, key_index,
					  pairwise, mac_addr);
	cds_ssr_unprotect(__func__);

	return ret;
}

/*
 * FUNCTION: __wlan_hdd_cfg80211_set_default_key
 * This function is used to set the default tx key index
 */
static int __wlan_hdd_cfg80211_set_default_key(struct wiphy *wiphy,
					       struct net_device *ndev,
					       u8 key_index,
					       bool unicast, bool multicast)
{
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(ndev);
	hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
	hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
	hdd_context_t *pHddCtx;
	int status;

	ENTER();

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EINVAL;
	}

	MTRACE(qdf_trace(QDF_MODULE_ID_HDD,
			 TRACE_CODE_HDD_CFG80211_SET_DEFAULT_KEY,
			 pAdapter->sessionId, key_index));

	hdd_notice("Device_mode %s(%d) key_index = %d",
		hdd_device_mode_to_string(pAdapter->device_mode),
		pAdapter->device_mode, key_index);

	if (CSR_MAX_NUM_KEY <= key_index) {
		hdd_err("Invalid key index %d", key_index);
		return -EINVAL;
	}

	pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
	status = wlan_hdd_validate_context(pHddCtx);

	if (0 != status)
		return status;

	if ((pAdapter->device_mode == QDF_STA_MODE) ||
	    (pAdapter->device_mode == QDF_P2P_CLIENT_MODE)) {
		if ((eCSR_ENCRYPT_TYPE_TKIP !=
		     pHddStaCtx->conn_info.ucEncryptionType) &&
		    (eCSR_ENCRYPT_TYPE_AES !=
		     pHddStaCtx->conn_info.ucEncryptionType)) {
			/* If default key index is not same as previous one,
			 * then update the default key index */

			tCsrRoamSetKey setKey;
			uint32_t roamId = 0xFF;
			tCsrKeys *Keys = &pWextState->roamProfile.Keys;

			hdd_info("Default tx key index %d", key_index);

			Keys->defaultIndex = (u8) key_index;
			qdf_mem_zero(&setKey, sizeof(tCsrRoamSetKey));
			setKey.keyId = key_index;
			setKey.keyLength = Keys->KeyLength[key_index];

			qdf_mem_copy(&setKey.Key[0],
				     &Keys->KeyMaterial[key_index][0],
				     Keys->KeyLength[key_index]);

			setKey.keyDirection = eSIR_TX_RX;

			qdf_copy_macaddr(&setKey.peerMac,
					 &pHddStaCtx->conn_info.bssId);

			if (Keys->KeyLength[key_index] == CSR_WEP40_KEY_LEN &&
			    pWextState->roamProfile.EncryptionType.
			    encryptionType[0] == eCSR_ENCRYPT_TYPE_WEP104) {
				/* In the case of dynamic wep supplicant hardcodes DWEP type
				* to eCSR_ENCRYPT_TYPE_WEP104 even though ap is configured for
				* WEP-40 encryption. In this canse the key length is 5 but the
				* encryption type is 104 hence checking the key langht(5) and
				* encryption type(104) and switching encryption type to 40*/
				pWextState->roamProfile.EncryptionType.
				encryptionType[0] = eCSR_ENCRYPT_TYPE_WEP40;
				pWextState->roamProfile.mcEncryptionType.
				encryptionType[0] = eCSR_ENCRYPT_TYPE_WEP40;
			}

			setKey.encType =
				pWextState->roamProfile.EncryptionType.
				encryptionType[0];

			/* Issue set key request */
			status = sme_roam_set_key(WLAN_HDD_GET_HAL_CTX(pAdapter),
						  pAdapter->sessionId, &setKey,
						  &roamId);

			if (0 != status) {
				hdd_err("sme_roam_set_key failed, returned %d",
				       status);
				return -EINVAL;
			}
		}
	} else if (QDF_SAP_MODE == pAdapter->device_mode) {
		/* In SoftAp mode setting key direction for default mode */
		if ((eCSR_ENCRYPT_TYPE_TKIP !=
		     pWextState->roamProfile.EncryptionType.encryptionType[0])
		    && (eCSR_ENCRYPT_TYPE_AES !=
			pWextState->roamProfile.EncryptionType.
			encryptionType[0])) {
			/* Saving key direction for default key index to TX default */
			hdd_ap_ctx_t *pAPCtx =
				WLAN_HDD_GET_AP_CTX_PTR(pAdapter);
			pAPCtx->wepKey[key_index].keyDirection =
				eSIR_TX_DEFAULT;
		}
	}

	EXIT();
	return status;
}

static int wlan_hdd_cfg80211_set_default_key(struct wiphy *wiphy,
					     struct net_device *ndev,
					     u8 key_index,
					     bool unicast, bool multicast)
{
	int ret;
	cds_ssr_protect(__func__);
	ret =
		__wlan_hdd_cfg80211_set_default_key(wiphy, ndev, key_index, unicast,
						    multicast);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * wlan_hdd_cfg80211_update_bss_list() - update bss list to NL80211
 * @pAdapter: Pointer to adapter
 * @pRoamInfo: Pointer to roam info
 *
 * Return: struct cfg80211_bss pointer
 */
struct cfg80211_bss *wlan_hdd_cfg80211_update_bss_list(hdd_adapter_t *pAdapter,
						       tCsrRoamInfo *pRoamInfo)
{
	struct net_device *dev = pAdapter->dev;
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct wiphy *wiphy = wdev->wiphy;
	tSirBssDescription *pBssDesc = pRoamInfo->pBssDesc;
	int chan_no;
	unsigned int freq;
	struct ieee80211_channel *chan;
	struct cfg80211_bss *bss = NULL;

	ENTER();

	if (NULL == pBssDesc) {
		hdd_err("pBssDesc is NULL");
		return bss;
	}

	if (NULL == pRoamInfo->pProfile) {
		hdd_err("Roam profile is NULL");
		return bss;
	}

	chan_no = pBssDesc->channelId;

	if (chan_no <= ARRAY_SIZE(hdd_channels_2_4_ghz)) {
		freq =
			ieee80211_channel_to_frequency(chan_no,
						       IEEE80211_BAND_2GHZ);
	} else {
		freq =
			ieee80211_channel_to_frequency(chan_no,
						       IEEE80211_BAND_5GHZ);
	}

	chan = __ieee80211_get_channel(wiphy, freq);

	if (!chan) {
		hdd_err("chan pointer is NULL");
		return NULL;
	}

	bss = cfg80211_get_bss(wiphy, chan, pBssDesc->bssId,
			       &pRoamInfo->pProfile->SSIDs.SSIDList->SSID.
			       ssId[0],
			       pRoamInfo->pProfile->SSIDs.SSIDList->SSID.length,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)) && !defined(WITH_BACKPORTS)
			       WLAN_CAPABILITY_ESS, WLAN_CAPABILITY_ESS);
#else
			       IEEE80211_BSS_TYPE_ESS, IEEE80211_PRIVACY_ANY);
#endif
	if (bss == NULL) {
		hdd_err("BSS not present");
	} else {
		hdd_notice("cfg80211_unlink_bss called for BSSID "
				MAC_ADDRESS_STR,
		       MAC_ADDR_ARRAY(pBssDesc->bssId));
		cfg80211_unlink_bss(wiphy, bss);
	}
	return bss;
}

/**
 * wlan_hdd_cfg80211_inform_bss_frame() - inform bss details to NL80211
 * @pAdapter: Pointer to adapter
 * @bss_desc: Pointer to bss descriptor
 *
 * This function is used to inform the BSS details to nl80211 interface.
 *
 * Return: struct cfg80211_bss pointer
 */
static struct cfg80211_bss *
wlan_hdd_cfg80211_inform_bss_frame(hdd_adapter_t *pAdapter,
				   tSirBssDescription *bss_desc)
{
	/*
	 * cfg80211_inform_bss() is not updating ie field of bss entry, if entry
	 * already exists in bss data base of cfg80211 for that particular BSS
	 * ID. Using cfg80211_inform_bss_frame to update the bss entry instead
	 * of cfg80211_inform_bss, But this call expects mgmt packet as input.
	 * As of now there is no possibility to get the mgmt(probe response)
	 * frame from PE, converting bss_desc to ieee80211_mgmt(probe response)
	 * and passing to cfg80211_inform_bss_frame.
	 */
	struct net_device *dev = pAdapter->dev;
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct wiphy *wiphy = wdev->wiphy;
	int chan_no = bss_desc->channelId;
#ifdef WLAN_ENABLE_AGEIE_ON_SCAN_RESULTS
	qcom_ie_age *qie_age = NULL;
	int ie_length =
		GET_IE_LEN_IN_BSS_DESC(bss_desc->length) + sizeof(qcom_ie_age);
#else
	int ie_length = GET_IE_LEN_IN_BSS_DESC(bss_desc->length);
#endif
	const char *ie =
		((ie_length != 0) ? (const char *)&bss_desc->ieFields : NULL);
	unsigned int freq;
	struct ieee80211_channel *chan;
	struct ieee80211_mgmt *mgmt = NULL;
	struct cfg80211_bss *bss_status = NULL;
	size_t frame_len = sizeof(struct ieee80211_mgmt) + ie_length;
	int rssi = 0;
	hdd_context_t *pHddCtx;
	int status;
	struct timespec ts;
	struct hdd_config *cfg_param;

	pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
	status = wlan_hdd_validate_context(pHddCtx);
	if (0 != status)
		return NULL;

	cfg_param = pHddCtx->config;
	mgmt = kzalloc((sizeof(struct ieee80211_mgmt) + ie_length), GFP_KERNEL);
	if (!mgmt) {
		hdd_err("memory allocation failed");
		return NULL;
	}

	memcpy(mgmt->bssid, bss_desc->bssId, ETH_ALEN);

	/* Android does not want the timestamp from the frame.
	   Instead it wants a monotonic increasing value */
	get_monotonic_boottime(&ts);
	mgmt->u.probe_resp.timestamp =
		((u64) ts.tv_sec * 1000000) + (ts.tv_nsec / 1000);

	mgmt->u.probe_resp.beacon_int = bss_desc->beaconInterval;
	mgmt->u.probe_resp.capab_info = bss_desc->capabilityInfo;

#ifdef WLAN_ENABLE_AGEIE_ON_SCAN_RESULTS
	/* GPS Requirement: need age ie per entry. Using vendor specific. */
	/* Assuming this is the last IE, copy at the end */
	ie_length -= sizeof(qcom_ie_age);
	qie_age = (qcom_ie_age *) (mgmt->u.probe_resp.variable + ie_length);
	qie_age->element_id = QCOM_VENDOR_IE_ID;
	qie_age->len = QCOM_VENDOR_IE_AGE_LEN;
	qie_age->oui_1 = QCOM_OUI1;
	qie_age->oui_2 = QCOM_OUI2;
	qie_age->oui_3 = QCOM_OUI3;
	qie_age->type = QCOM_VENDOR_IE_AGE_TYPE;
	qie_age->age =
		qdf_mc_timer_get_system_ticks() - bss_desc->nReceivedTime;
	qie_age->tsf_delta = bss_desc->tsf_delta;
	memcpy(&qie_age->beacon_tsf, bss_desc->timeStamp,
	       sizeof(qie_age->beacon_tsf));
#endif

	memcpy(mgmt->u.probe_resp.variable, ie, ie_length);
	if (bss_desc->fProbeRsp) {
		mgmt->frame_control |=
			(u16) (IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_PROBE_RESP);
	} else {
		mgmt->frame_control |=
			(u16) (IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_BEACON);
	}

	if (chan_no <= ARRAY_SIZE(hdd_channels_2_4_ghz) &&
	    (wiphy->bands[IEEE80211_BAND_2GHZ] != NULL)) {
		freq =
			ieee80211_channel_to_frequency(chan_no,
						       IEEE80211_BAND_2GHZ);
	} else if ((chan_no > ARRAY_SIZE(hdd_channels_2_4_ghz))
		   && (wiphy->bands[IEEE80211_BAND_5GHZ] != NULL)) {
		freq =
			ieee80211_channel_to_frequency(chan_no,
						       IEEE80211_BAND_5GHZ);
	} else {
		hdd_err("Invalid chan_no %d", chan_no);
		kfree(mgmt);
		return NULL;
	}

	chan = __ieee80211_get_channel(wiphy, freq);
	/* When the band is changed on the fly using the GUI, three things are done
	 * 1. scan abort
	 * 2. flush scan results from cache
	 * 3. update the band with the new band user specified (refer to the
	 * hdd_set_band_helper function) as part of the scan abort, message will be
	 * queued to PE and we proceed with flushing and changinh the band.
	 * PE will stop the scanning further and report back the results what ever
	 * it had till now by calling the call back function.
	 * if the time between update band and scandone call back is sufficient
	 * enough the band change reflects in SME, SME validates the channels
	 * and discards the channels correponding to previous band and calls back
	 * with zero bss results. but if the time between band update and scan done
	 * callback is very small then band change will not reflect in SME and SME
	 * reports to HDD all the channels correponding to previous band.this is due
	 * to race condition.but those channels are invalid to the new band and so
	 * this function __ieee80211_get_channel will return NULL.Each time we
	 * report scan result with this pointer null warning kernel trace is printed.
	 * if the scan results contain large number of APs continuosly kernel
	 * warning trace is printed and it will lead to apps watch dog bark.
	 * So drop the bss and continue to next bss.
	 */
	if (chan == NULL) {
		hdd_err("chan pointer is NULL");
		kfree(mgmt);
		return NULL;
	}

	/* Based on .ini configuration, raw rssi can be reported for bss.
	 * Raw rssi is typically used for estimating power.
	 */

	rssi = (cfg_param->inform_bss_rssi_raw) ? bss_desc->rssi_raw :
			bss_desc->rssi;

	/* Supplicant takes the signal strength in terms of mBm(100*dBm) */
	rssi = QDF_MIN(rssi, 0) * 100;

	hdd_notice("BSSID: " MAC_ADDRESS_STR " Channel:%d RSSI:%d TSF %u",
	       MAC_ADDR_ARRAY(mgmt->bssid), chan->center_freq,
	       (int)(rssi / 100),
	       bss_desc->timeStamp[0]);

	bss_status =
		cfg80211_inform_bss_frame(wiphy, chan, mgmt, frame_len, rssi,
					  GFP_KERNEL);
	kfree(mgmt);
	return bss_status;
}

/**
 * wlan_hdd_cfg80211_update_bss_db() - update bss database of CF80211
 * @pAdapter: Pointer to adapter
 * @pRoamInfo: Pointer to roam info
 *
 * This function is used to update the BSS data base of CFG8011
 *
 * Return: struct cfg80211_bss pointer
 */
struct cfg80211_bss *wlan_hdd_cfg80211_update_bss_db(hdd_adapter_t *pAdapter,
						     tCsrRoamInfo *pRoamInfo)
{
	tCsrRoamConnectedProfile roamProfile;
	tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
	struct cfg80211_bss *bss = NULL;

	memset(&roamProfile, 0, sizeof(tCsrRoamConnectedProfile));
	sme_roam_get_connect_profile(hHal, pAdapter->sessionId, &roamProfile);

	if (NULL != roamProfile.pBssDesc) {
		bss = wlan_hdd_cfg80211_inform_bss_frame(pAdapter,
							 roamProfile.pBssDesc);

		if (NULL == bss)
			hdd_notice("wlan_hdd_cfg80211_inform_bss_frame returned NULL");

		sme_roam_free_connect_profile(&roamProfile);
	} else {
		hdd_err("roamProfile.pBssDesc is NULL");
	}
	return bss;
}
/**
 * wlan_hdd_cfg80211_update_bss() - update bss
 * @wiphy: Pointer to wiphy
 * @pAdapter: Pointer to adapter
 * @scan_time: scan request timestamp
 *
 * Return: zero if success, non-zero otherwise
 */
int wlan_hdd_cfg80211_update_bss(struct wiphy *wiphy,
				 hdd_adapter_t *pAdapter,
				 uint32_t scan_time)
{
	tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
	tCsrScanResultInfo *pScanResult;
	QDF_STATUS status = 0;
	tScanResultHandle pResult;
	struct cfg80211_bss *bss_status = NULL;
	hdd_context_t *pHddCtx;
	int ret;

	ENTER();

	MTRACE(qdf_trace(QDF_MODULE_ID_HDD,
			 TRACE_CODE_HDD_CFG80211_UPDATE_BSS,
			 NO_SESSION, pAdapter->sessionId));

	pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
	ret = wlan_hdd_validate_context(pHddCtx);
	if (0 != ret)
		return ret;

	/* start getting scan results and populate cgf80211 BSS database */
	status = sme_scan_get_result(hHal, pAdapter->sessionId, NULL, &pResult);

	/* no scan results */
	if (NULL == pResult) {
		hdd_err("No scan result Status %d", status);
		return status;
	}

	pScanResult = sme_scan_result_get_first(hHal, pResult);

	while (pScanResult) {
		/*
		 * - cfg80211_inform_bss() is not updating ie field of bss
		 * entry if entry already exists in bss data base of cfg80211
		 * for that particular BSS ID.  Using cfg80211_inform_bss_frame
		 * to update thebss entry instead of cfg80211_inform_bss,
		 * But this call expects mgmt packet as input. As of now
		 * there is no possibility to get the mgmt(probe response)
		 * frame from PE, converting bss_desc to
		 * ieee80211_mgmt(probe response) and passing to c
		 * fg80211_inform_bss_frame.
		 * - Update BSS only if beacon timestamp is later than
		 * scan request timestamp.
		 */
		if ((scan_time == 0) ||
			(scan_time <
				pScanResult->BssDescriptor.nReceivedTime)) {
			bss_status =
				wlan_hdd_cfg80211_inform_bss_frame(pAdapter,
						&pScanResult->BssDescriptor);

			if (NULL == bss_status) {
				hdd_info("NULL returned by cfg80211_inform_bss_frame");
			} else {
				cfg80211_put_bss(
					wiphy,
					bss_status);
			}
		} else {
			hdd_info("BSSID: " MAC_ADDRESS_STR " Skipped",
			MAC_ADDR_ARRAY(pScanResult->BssDescriptor.bssId));
		}
		pScanResult = sme_scan_result_get_next(hHal, pResult);
	}

	sme_scan_result_purge(hHal, pResult);
	/*
	 * For SAP mode, scan is invoked by hostapd during SAP start
	 * if hostapd is restarted, we need to flush previous scan
	 * result so that it will reflect environment change
	 */
	if (pAdapter->device_mode == QDF_SAP_MODE
#ifdef FEATURE_WLAN_AP_AP_ACS_OPTIMIZE
		&& pHddCtx->skip_acs_scan_status != eSAP_SKIP_ACS_SCAN
#endif
	)
		sme_scan_flush_result(hHal);

	EXIT();
	return 0;
}

/**
 * wlan_hdd_cfg80211_pmksa_candidate_notify() - notify a new PMSKA candidate
 * @pAdapter: Pointer to adapter
 * @pRoamInfo: Pointer to roam info
 * @index: Index
 * @preauth: Preauth flag
 *
 * This function is used to notify the supplicant of a new PMKSA candidate.
 *
 * Return: 0 for success, non-zero for failure
 */
int wlan_hdd_cfg80211_pmksa_candidate_notify(hdd_adapter_t *pAdapter,
					     tCsrRoamInfo *pRoamInfo,
					     int index, bool preauth)
{
#ifdef FEATURE_WLAN_OKC
	struct net_device *dev = pAdapter->dev;
	hdd_context_t *pHddCtx = (hdd_context_t *) pAdapter->pHddCtx;

	ENTER();
	hdd_notice("is going to notify supplicant of:");

	if (NULL == pRoamInfo) {
		hdd_alert("pRoamInfo is NULL");
		return -EINVAL;
	}

	if (true == hdd_is_okc_mode_enabled(pHddCtx)) {
		hdd_notice(MAC_ADDRESS_STR,
		       MAC_ADDR_ARRAY(pRoamInfo->bssid.bytes));
		cfg80211_pmksa_candidate_notify(dev, index,
						pRoamInfo->bssid.bytes,
						preauth, GFP_KERNEL);
	}
#endif /* FEATURE_WLAN_OKC */
	return 0;
}

#ifdef FEATURE_WLAN_LFR_METRICS
/**
 * wlan_hdd_cfg80211_roam_metrics_preauth() - roam metrics preauth
 * @pAdapter: Pointer to adapter
 * @pRoamInfo: Pointer to roam info
 *
 * 802.11r/LFR metrics reporting function to report preauth initiation
 *
 * Return: QDF status
 */
#define MAX_LFR_METRICS_EVENT_LENGTH 100
QDF_STATUS wlan_hdd_cfg80211_roam_metrics_preauth(hdd_adapter_t *pAdapter,
						  tCsrRoamInfo *pRoamInfo)
{
	unsigned char metrics_notification[MAX_LFR_METRICS_EVENT_LENGTH + 1];
	union iwreq_data wrqu;

	ENTER();

	if (NULL == pAdapter) {
		hdd_err("pAdapter is NULL!");
		return QDF_STATUS_E_FAILURE;
	}

	/* create the event */
	memset(&wrqu, 0, sizeof(wrqu));
	memset(metrics_notification, 0, sizeof(metrics_notification));

	wrqu.data.pointer = metrics_notification;
	wrqu.data.length = scnprintf(metrics_notification,
				     sizeof(metrics_notification),
				     "QCOM: LFR_PREAUTH_INIT " MAC_ADDRESS_STR,
				     MAC_ADDR_ARRAY(pRoamInfo->bssid));

	wireless_send_event(pAdapter->dev, IWEVCUSTOM, &wrqu,
			    metrics_notification);

	EXIT();

	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_hdd_cfg80211_roam_metrics_handover() - roam metrics hand over
 * @pAdapter: Pointer to adapter
 * @pRoamInfo: Pointer to roam info
 * @preauth_status: Preauth status
 *
 * 802.11r/LFR metrics reporting function to report handover initiation
 *
 * Return: QDF status
 */
QDF_STATUS
wlan_hdd_cfg80211_roam_metrics_preauth_status(hdd_adapter_t *pAdapter,
					      tCsrRoamInfo *pRoamInfo,
					      bool preauth_status)
{
	unsigned char metrics_notification[MAX_LFR_METRICS_EVENT_LENGTH + 1];
	union iwreq_data wrqu;

	ENTER();

	if (NULL == pAdapter) {
		hdd_err("pAdapter is NULL!");
		return QDF_STATUS_E_FAILURE;
	}

	/* create the event */
	memset(&wrqu, 0, sizeof(wrqu));
	memset(metrics_notification, 0, sizeof(metrics_notification));

	scnprintf(metrics_notification, sizeof(metrics_notification),
		  "QCOM: LFR_PREAUTH_STATUS " MAC_ADDRESS_STR,
		  MAC_ADDR_ARRAY(pRoamInfo->bssid));

	if (1 == preauth_status)
		strlcat(metrics_notification, " true",
				sizeof(metrics_notification));
	else
		strlcat(metrics_notification, " false",
				sizeof(metrics_notification));

	wrqu.data.pointer = metrics_notification;
	wrqu.data.length = strlen(metrics_notification);

	wireless_send_event(pAdapter->dev, IWEVCUSTOM, &wrqu,
			    metrics_notification);

	EXIT();

	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_hdd_cfg80211_roam_metrics_handover() - roam metrics hand over
 * @pAdapter: Pointer to adapter
 * @pRoamInfo: Pointer to roam info
 *
 * 802.11r/LFR metrics reporting function to report handover initiation
 *
 * Return: QDF status
 */
QDF_STATUS wlan_hdd_cfg80211_roam_metrics_handover(hdd_adapter_t *pAdapter,
						   tCsrRoamInfo *pRoamInfo)
{
	unsigned char metrics_notification[MAX_LFR_METRICS_EVENT_LENGTH + 1];
	union iwreq_data wrqu;

	ENTER();

	if (NULL == pAdapter) {
		hdd_err("pAdapter is NULL!");
		return QDF_STATUS_E_FAILURE;
	}

	/* create the event */
	memset(&wrqu, 0, sizeof(wrqu));
	memset(metrics_notification, 0, sizeof(metrics_notification));

	wrqu.data.pointer = metrics_notification;
	wrqu.data.length = scnprintf(metrics_notification,
				     sizeof(metrics_notification),
				     "QCOM: LFR_PREAUTH_HANDOVER "
				     MAC_ADDRESS_STR,
				     MAC_ADDR_ARRAY(pRoamInfo->bssid));

	wireless_send_event(pAdapter->dev, IWEVCUSTOM, &wrqu,
			    metrics_notification);

	EXIT();

	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * hdd_select_cbmode() - select channel bonding mode
 * @pAdapter: Pointer to adapter
 * @operatingChannel: Operating channel
 *
 * Return: none
 */
void hdd_select_cbmode(hdd_adapter_t *pAdapter, uint8_t operationChannel)
{
	uint8_t iniDot11Mode = (WLAN_HDD_GET_CTX(pAdapter))->config->dot11Mode;
	eHddDot11Mode hddDot11Mode = iniDot11Mode;
	struct ch_params_s ch_params;
	hdd_station_ctx_t *station_ctx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
	struct hdd_mon_set_ch_info *ch_info = &station_ctx->ch_info;
	uint8_t sec_ch = 0;

	ch_params.ch_width =
			(WLAN_HDD_GET_CTX(pAdapter))->config->vhtChannelWidth;

	hdd_notice("Channel Bonding Mode Selected is %u", iniDot11Mode);
	switch (iniDot11Mode) {
	case eHDD_DOT11_MODE_AUTO:
	case eHDD_DOT11_MODE_11ac:
	case eHDD_DOT11_MODE_11ac_ONLY:
		if (sme_is_feature_supported_by_fw(DOT11AC))
			hddDot11Mode = eHDD_DOT11_MODE_11ac;
		else
			hddDot11Mode = eHDD_DOT11_MODE_11n;
		break;
	case eHDD_DOT11_MODE_11n:
	case eHDD_DOT11_MODE_11n_ONLY:
		hddDot11Mode = eHDD_DOT11_MODE_11n;
		break;
	default:
		hddDot11Mode = iniDot11Mode;
		break;
	}
	/*
	 * CDS api expects secondary channel for calculating
	 * the channel params
	 */
	if ((ch_params.ch_width == CH_WIDTH_40MHZ) &&
	    (CDS_IS_CHANNEL_24GHZ(operationChannel))) {
		if (operationChannel >= 1 && operationChannel <= 5)
			sec_ch = operationChannel + 4;
		else if (operationChannel >= 6 && operationChannel <= 13)
			sec_ch = operationChannel - 4;
	}

	/* This call decides required channel bonding mode */
	cds_set_channel_params(operationChannel, sec_ch, &ch_params);

	if (QDF_GLOBAL_MONITOR_MODE == cds_get_conparam()) {
		ch_info->channel_width = ch_params.ch_width;
		ch_info->phy_mode = hdd_cfg_xlate_to_csr_phy_mode(hddDot11Mode);
		ch_info->channel = operationChannel;
		ch_info->cb_mode = ch_params.ch_width;
		hdd_info("ch_info width %d, phymode %d channel %d",
			 ch_info->channel_width, ch_info->phy_mode,
			 ch_info->channel);
	}
}

/**
 * wlan_hdd_handle_sap_sta_dfs_conc() - to handle SAP STA DFS conc
 * @adapter: STA adapter
 * @roam_profile: STA roam profile
 *
 * This routine will move SAP from dfs to non-dfs, if sta is coming up.
 *
 * Return: false if sta-sap conc is not allowed, else return true
 */
static bool wlan_hdd_handle_sap_sta_dfs_conc(hdd_adapter_t *adapter,
						tCsrRoamProfile *roam_profile)
{
	hdd_context_t *hdd_ctx;
	hdd_adapter_t *ap_adapter;
	hdd_ap_ctx_t *hdd_ap_ctx;
	hdd_hostapd_state_t *hostapd_state;
	uint8_t channel = 0;
	QDF_STATUS status;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		hdd_err("HDD context is NULL");
		return true;
	}

	ap_adapter = hdd_get_adapter(hdd_ctx, QDF_SAP_MODE);
	/* probably no sap running, no handling required */
	if (ap_adapter == NULL)
		return true;

	/*
	 * sap is not in started state, so it is fine to go ahead with sta.
	 * if sap is currently doing CAC then don't allow sta to go further.
	 */
	if (!test_bit(SOFTAP_BSS_STARTED, &(ap_adapter)->event_flags) &&
	    (hdd_ctx->dev_dfs_cac_status != DFS_CAC_IN_PROGRESS))
		return true;

	if (hdd_ctx->dev_dfs_cac_status == DFS_CAC_IN_PROGRESS) {
		hdd_err("Concurrent SAP is in CAC state, STA is not allowed");
		return false;
	}

	/*
	 * log and return error, if we allow STA to go through, we don't
	 * know what is going to happen better stop sta connection
	 */
	hdd_ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(ap_adapter);
	if (NULL == hdd_ap_ctx) {
		hdd_err("AP context not found");
		return false;
	}

	/* sap is on non-dfs channel, nothing to handle */
	if (!CDS_IS_DFS_CH(hdd_ap_ctx->operatingChannel)) {
		hdd_info("sap is on non-dfs channel, sta is allowed");
		return true;
	}
	/*
	 * find out by looking in to scan cache where sta is going to
	 * connect by passing its roam_profile. if channel is 0 or DFS then
	 * better to call pcl and find out the best channel. if channel
	 * is non-dfs then better move SAP to STA's channel to make
	 * scc, so we have room for 3port MCC scenario
	 */
	status = cds_get_channel_from_scan_result(adapter,
			roam_profile, &channel);

	if ((QDF_STATUS_SUCCESS != status) || (0 == channel) ||
	    CDS_IS_DFS_CH(channel))
		channel = cds_get_nondfs_preferred_channel(CDS_SAP_MODE,
								true);

	hostapd_state = WLAN_HDD_GET_HOSTAP_STATE_PTR(ap_adapter);
	qdf_event_reset(&hostapd_state->qdf_event);
	status = wlansap_set_channel_change_with_csa(
			WLAN_HDD_GET_SAP_CTX_PTR(ap_adapter), channel,
			hdd_ap_ctx->sapConfig.ch_width_orig);

	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("Set channel with CSA IE failed, can't allow STA");
		return false;
	}

	/*
	 * wait here for SAP to finish the channel switch. When channel
	 * switch happens, SAP sends few beacons with CSA_IE. After
	 * successfully Transmission of those beacons, it will move its
	 * state from started to disconnected and move to new channel.
	 * once it moves to new channel, sap again moves its state
	 * machine from disconnected to started and set this event.
	 * wait for 10 secs to finish this.
	 */
	status = qdf_wait_single_event(&hostapd_state->qdf_event, 10000);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("wait for qdf_event failed, STA not allowed!!");
		return false;
	}

	return true;
}

/*
 * FUNCTION: wlan_hdd_cfg80211_connect_start
 * wlan_hdd_cfg80211_connect_start() - start connection
 * @pAdapter: Pointer to adapter
 * @ssid: Pointer ot ssid
 * @ssid_len: Length of ssid
 * @bssid: Pointer to bssid
 * @operatingChannel: Operating channel
 *
 * This function is used to start the association process
 *
 * Return: 0 for success, non-zero for failure
 */
int wlan_hdd_cfg80211_connect_start(hdd_adapter_t *pAdapter,
				    const u8 *ssid, size_t ssid_len,
				    const u8 *bssid_hint, const u8 *bssid,
				    u8 operatingChannel)
{
	int status = 0;
	hdd_wext_state_t *pWextState;
	hdd_context_t *pHddCtx;
	uint32_t roamId;
	tCsrRoamProfile *pRoamProfile;
	eCsrAuthType RSNAuthType;
	tSmeConfigParams *sme_config;

	ENTER();

	pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
	pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

	status = wlan_hdd_validate_context(pHddCtx);
	if (status)
		return status;

	if (SIR_MAC_MAX_SSID_LENGTH < ssid_len) {
		hdd_err("wrong SSID len");
		return -EINVAL;
	}

	pRoamProfile = &pWextState->roamProfile;

	if (pRoamProfile) {
		hdd_station_ctx_t *pHddStaCtx;
		pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

		if (HDD_WMM_USER_MODE_NO_QOS ==
		    (WLAN_HDD_GET_CTX(pAdapter))->config->WmmMode) {
			/*QoS not enabled in cfg file */
			pRoamProfile->uapsd_mask = 0;
		} else {
			/*QoS enabled, update uapsd mask from cfg file */
			pRoamProfile->uapsd_mask =
				(WLAN_HDD_GET_CTX(pAdapter))->config->UapsdMask;
		}

		pRoamProfile->SSIDs.numOfSSIDs = 1;
		pRoamProfile->SSIDs.SSIDList->SSID.length = ssid_len;
		qdf_mem_zero(pRoamProfile->SSIDs.SSIDList->SSID.ssId,
			     sizeof(pRoamProfile->SSIDs.SSIDList->SSID.ssId));
		qdf_mem_copy((void *)(pRoamProfile->SSIDs.SSIDList->SSID.ssId),
			     ssid, ssid_len);

		if (bssid) {
			pRoamProfile->BSSIDs.numOfBSSIDs = 1;
			qdf_mem_copy((void *)(pRoamProfile->BSSIDs.bssid),
				     bssid, QDF_MAC_ADDR_SIZE);
			/* Save BSSID in seperate variable as well, as RoamProfile
			   BSSID is getting zeroed out in the association process. And in
			   case of join failure we should send valid BSSID to supplicant
			 */
			qdf_mem_copy((void *)(pWextState->req_bssId.bytes),
					bssid, QDF_MAC_ADDR_SIZE);
		} else if (bssid_hint) {
			pRoamProfile->BSSIDs.numOfBSSIDs = 1;
			qdf_mem_copy((void *)(pRoamProfile->BSSIDs.bssid),
						bssid_hint, QDF_MAC_ADDR_SIZE);
			/* Save BSSID in separate variable as well, as
			   RoamProfile BSSID is getting zeroed out in the
			   association process. And in case of join failure
			   we should send valid BSSID to supplicant
			 */
			qdf_mem_copy((void *)(pWextState->req_bssId.bytes),
					bssid_hint, QDF_MAC_ADDR_SIZE);
			hdd_warn(" bssid_hint "MAC_ADDRESS_STR,
					MAC_ADDR_ARRAY(bssid_hint));
		} else {
			qdf_mem_zero((void *)(pRoamProfile->BSSIDs.bssid),
				     QDF_MAC_ADDR_SIZE);
		}

		hdd_notice("Connect to SSID: %.*s operating Channel: %u",
		       pRoamProfile->SSIDs.SSIDList->SSID.length,
		       pRoamProfile->SSIDs.SSIDList->SSID.ssId,
		       operatingChannel);

		if ((IW_AUTH_WPA_VERSION_WPA == pWextState->wpaVersion) ||
		    (IW_AUTH_WPA_VERSION_WPA2 == pWextState->wpaVersion)) {
			/*set gen ie */
			hdd_set_genie_to_csr(pAdapter, &RSNAuthType);
			/*set auth */
			hdd_set_csr_auth_type(pAdapter, RSNAuthType);
		}
#ifdef FEATURE_WLAN_WAPI
		if (pAdapter->wapi_info.nWapiMode) {
			hdd_notice("Setting WAPI AUTH Type and Encryption Mode values");
			switch (pAdapter->wapi_info.wapiAuthMode) {
			case WAPI_AUTH_MODE_PSK:
			{
				hdd_notice("WAPI AUTH TYPE: PSK: %d",
				       pAdapter->wapi_info.wapiAuthMode);
				pRoamProfile->AuthType.authType[0] =
					eCSR_AUTH_TYPE_WAPI_WAI_PSK;
				break;
			}
			case WAPI_AUTH_MODE_CERT:
			{
				hdd_notice("WAPI AUTH TYPE: CERT: %d",
				       pAdapter->wapi_info.wapiAuthMode);
				pRoamProfile->AuthType.authType[0] =
					eCSR_AUTH_TYPE_WAPI_WAI_CERTIFICATE;
				break;
			}
			} /* End of switch */
			if (pAdapter->wapi_info.wapiAuthMode ==
			    WAPI_AUTH_MODE_PSK
			    || pAdapter->wapi_info.wapiAuthMode ==
			    WAPI_AUTH_MODE_CERT) {
				hdd_notice("WAPI PAIRWISE/GROUP ENCRYPTION: WPI");
				pRoamProfile->AuthType.numEntries = 1;
				pRoamProfile->EncryptionType.numEntries = 1;
				pRoamProfile->EncryptionType.encryptionType[0] =
					eCSR_ENCRYPT_TYPE_WPI;
				pRoamProfile->mcEncryptionType.numEntries = 1;
				pRoamProfile->mcEncryptionType.
				encryptionType[0] = eCSR_ENCRYPT_TYPE_WPI;
			}
		}
#endif /* FEATURE_WLAN_WAPI */
#ifdef WLAN_FEATURE_GTK_OFFLOAD
		/* Initializing gtkOffloadReqParams */
		if ((QDF_STA_MODE == pAdapter->device_mode) ||
		    (QDF_P2P_CLIENT_MODE == pAdapter->device_mode)) {
			memset(&pHddStaCtx->gtkOffloadReqParams, 0,
			       sizeof(tSirGtkOffloadParams));
			pHddStaCtx->gtkOffloadReqParams.ulFlags =
				GTK_OFFLOAD_DISABLE;
		}
#endif
		pRoamProfile->csrPersona = pAdapter->device_mode;

		if (operatingChannel) {
			pRoamProfile->ChannelInfo.ChannelList =
				&operatingChannel;
			pRoamProfile->ChannelInfo.numOfChannels = 1;
		} else {
			pRoamProfile->ChannelInfo.ChannelList = NULL;
			pRoamProfile->ChannelInfo.numOfChannels = 0;
		}
		if ((QDF_IBSS_MODE == pAdapter->device_mode)
		    && operatingChannel) {
			/*
			 * Need to post the IBSS power save parameters
			 * to WMA. WMA will configure this parameters
			 * to firmware if power save is enabled by the
			 * firmware.
			 */
			status = hdd_set_ibss_power_save_params(pAdapter);

			if (QDF_STATUS_SUCCESS != status) {
				hdd_err("Set IBSS Power Save Params Failed");
				return -EINVAL;
			}
			hdd_select_cbmode(pAdapter, operatingChannel);
		}

		if (pHddCtx->config->policy_manager_enabled &&
			(true == cds_is_connection_in_progress())) {
			hdd_err("Connection refused: conn in progress");
			return -EINVAL;
		}

		/* After 8-way handshake supplicant should give the scan command
		 * in that it update the additional IEs, But because of scan
		 * enhancements, the supplicant is not issuing the scan command now.
		 * So the unicast frames which are sent from the host are not having
		 * the additional IEs. If it is P2P CLIENT and there is no additional
		 * IE present in roamProfile, then use the addtional IE form scan_info
		 */

		if ((pAdapter->device_mode == QDF_P2P_CLIENT_MODE) &&
		    (!pRoamProfile->pAddIEScan)) {
			pRoamProfile->pAddIEScan =
				&pAdapter->scan_info.scanAddIE.addIEdata[0];
			pRoamProfile->nAddIEScanLength =
				pAdapter->scan_info.scanAddIE.length;
		}
		/*
		 * When policy manager is enabled from ini file, we shouldn't
		 * check for other concurrency rules.
		 */
		if (!pHddCtx->config->policy_manager_enabled) {
			cds_handle_conc_rule1(pAdapter, pRoamProfile);
			if (true != cds_handle_conc_rule2(
					pAdapter, pRoamProfile, &roamId))
				return 0;
		}

		if (pHddCtx->config->policy_manager_enabled &&
			(false == wlan_hdd_handle_sap_sta_dfs_conc(pAdapter,
				pRoamProfile))) {
			hdd_err("sap-sta conc will fail, can't allow sta");
			hdd_conn_set_connection_state(pAdapter,
					eConnectionState_NotConnected);
			return -ENOMEM;
		}

		sme_config = qdf_mem_malloc(sizeof(*sme_config));
		if (!sme_config) {
			hdd_err("unable to allocate sme_config");
			hdd_conn_set_connection_state(pAdapter,
					eConnectionState_NotConnected);
			return -ENOMEM;
		}
		qdf_mem_zero(sme_config, sizeof(*sme_config));
		sme_get_config_param(pHddCtx->hHal, sme_config);
		/* These values are not sessionized. So, any change in these SME
		 * configs on an older or parallel interface will affect the
		 * cb mode. So, restoring the default INI params before starting
		 * interfaces such as sta, cli etc.,
		 */
		sme_config->csrConfig.channelBondingMode5GHz =
			pHddCtx->config->nChannelBondingMode5GHz;
		sme_config->csrConfig.channelBondingMode24GHz =
			pHddCtx->config->nChannelBondingMode24GHz;
		sme_update_config(pHddCtx->hHal, sme_config);
		qdf_mem_free(sme_config);
		/*
		 * Change conn_state to connecting before sme_roam_connect(),
		 * because sme_roam_connect() has a direct path to call
		 * hdd_sme_roam_callback(), which will change the conn_state
		 * If direct path, conn_state will be accordingly changed to
		 * NotConnected or Associated by either
		 * hdd_association_completion_handler() or
		 * hdd_dis_connect_handler() in sme_RoamCallback()if
		 * sme_RomConnect is to be queued,
		 * Connecting state will remain until it is completed.
		 *
		 * If connection state is not changed, connection state will
		 * remain in eConnectionState_NotConnected state.
		 * In hdd_association_completion_handler, "hddDisconInProgress"
		 * is set to true if conn state is
		 * eConnectionState_NotConnected.
		 * If "hddDisconInProgress" is set to true then cfg80211 layer
		 * is not informed of connect result indication which
		 * is an issue.
		 */
		if (QDF_STA_MODE == pAdapter->device_mode ||
			QDF_P2P_CLIENT_MODE == pAdapter->device_mode)
			hdd_conn_set_connection_state(pAdapter,
			eConnectionState_Connecting);

		status = sme_roam_connect(WLAN_HDD_GET_HAL_CTX(pAdapter),
					  pAdapter->sessionId, pRoamProfile,
					  &roamId);

		if ((QDF_STATUS_SUCCESS != status) &&
		    (QDF_STA_MODE == pAdapter->device_mode ||
		     QDF_P2P_CLIENT_MODE == pAdapter->device_mode)) {
			hdd_err("sme_roam_connect (session %d) failed with "
			       "status %d. -> NotConnected",
			       pAdapter->sessionId, status);
			/* change back to NotAssociated */
			hdd_conn_set_connection_state(pAdapter,
						      eConnectionState_NotConnected);
		}

		pRoamProfile->ChannelInfo.ChannelList = NULL;
		pRoamProfile->ChannelInfo.numOfChannels = 0;

	} else {
		hdd_err("No valid Roam profile");
		return -EINVAL;
	}
	EXIT();
	return status;
}

/**
 * wlan_hdd_cfg80211_set_auth_type() - set auth type
 * @pAdapter: Pointer to adapter
 * @auth_type: Auth type
 *
 * This function is used to set the authentication type (OPEN/SHARED).
 *
 * Return: 0 for success, non-zero for failure
 */
static int wlan_hdd_cfg80211_set_auth_type(hdd_adapter_t *pAdapter,
					   enum nl80211_auth_type auth_type)
{
	hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
	hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

	/*set authentication type */
	switch (auth_type) {
	case NL80211_AUTHTYPE_AUTOMATIC:
		hdd_notice("set authentication type to AUTOSWITCH");
		pHddStaCtx->conn_info.authType = eCSR_AUTH_TYPE_AUTOSWITCH;
		break;

	case NL80211_AUTHTYPE_OPEN_SYSTEM:
	case NL80211_AUTHTYPE_FT:
		hdd_notice("set authentication type to OPEN");
		pHddStaCtx->conn_info.authType = eCSR_AUTH_TYPE_OPEN_SYSTEM;
		break;

	case NL80211_AUTHTYPE_SHARED_KEY:
		hdd_notice("set authentication type to SHARED");
		pHddStaCtx->conn_info.authType = eCSR_AUTH_TYPE_SHARED_KEY;
		break;
#ifdef FEATURE_WLAN_ESE
	case NL80211_AUTHTYPE_NETWORK_EAP:
		hdd_notice("set authentication type to CCKM WPA");
		pHddStaCtx->conn_info.authType = eCSR_AUTH_TYPE_CCKM_WPA;
		break;
#endif

	default:
		hdd_err("Unsupported authentication type %d", auth_type);
		pHddStaCtx->conn_info.authType = eCSR_AUTH_TYPE_UNKNOWN;
		return -EINVAL;
	}

	pWextState->roamProfile.AuthType.authType[0] =
		pHddStaCtx->conn_info.authType;
	return 0;
}

/**
 * wlan_hdd_set_akm_suite() - set key management type
 * @pAdapter: Pointer to adapter
 * @key_mgmt: Key management type
 *
 * This function is used to set the key mgmt type(PSK/8021x).
 *
 * Return: 0 for success, non-zero for failure
 */
static int wlan_hdd_set_akm_suite(hdd_adapter_t *pAdapter, u32 key_mgmt)
{
	hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);

#define WLAN_AKM_SUITE_8021X_SHA256 0x000FAC05
#define WLAN_AKM_SUITE_PSK_SHA256   0x000FAC06
	/*set key mgmt type */
	switch (key_mgmt) {
	case WLAN_AKM_SUITE_PSK:
	case WLAN_AKM_SUITE_PSK_SHA256:
	case WLAN_AKM_SUITE_FT_PSK:
		hdd_notice("setting key mgmt type to PSK");
		pWextState->authKeyMgmt |= IW_AUTH_KEY_MGMT_PSK;
		break;

	case WLAN_AKM_SUITE_8021X_SHA256:
	case WLAN_AKM_SUITE_8021X:
	case WLAN_AKM_SUITE_FT_8021X:
		hdd_notice("setting key mgmt type to 8021x");
		pWextState->authKeyMgmt |= IW_AUTH_KEY_MGMT_802_1X;
		break;
#ifdef FEATURE_WLAN_ESE
#define WLAN_AKM_SUITE_CCKM         0x00409600  /* Should be in ieee802_11_defs.h */
#define IW_AUTH_KEY_MGMT_CCKM       8   /* Should be in linux/wireless.h */
	case WLAN_AKM_SUITE_CCKM:
		hdd_notice("setting key mgmt type to CCKM");
		pWextState->authKeyMgmt |= IW_AUTH_KEY_MGMT_CCKM;
		break;
#endif
#ifndef WLAN_AKM_SUITE_OSEN
#define WLAN_AKM_SUITE_OSEN         0x506f9a01  /* Should be in ieee802_11_defs.h */
#endif
	case WLAN_AKM_SUITE_OSEN:
		hdd_notice("setting key mgmt type to OSEN");
		pWextState->authKeyMgmt |= IW_AUTH_KEY_MGMT_802_1X;
		break;

	default:
		hdd_err("Unsupported key mgmt type %d", key_mgmt);
		return -EINVAL;

	}
	return 0;
}

/**
 * wlan_hdd_cfg80211_set_cipher() - set encryption type
 * @pAdapter: Pointer to adapter
 * @cipher: Cipher type
 * @ucast: Unicast flag
 *
 * This function is used to set the encryption type
 * (NONE/WEP40/WEP104/TKIP/CCMP).
 *
 * Return: 0 for success, non-zero for failure
 */
static int wlan_hdd_cfg80211_set_cipher(hdd_adapter_t *pAdapter,
					u32 cipher, bool ucast)
{
	eCsrEncryptionType encryptionType = eCSR_ENCRYPT_TYPE_NONE;
	hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
	hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

	if (!cipher) {
		hdd_info("received cipher %d - considering none", cipher);
		encryptionType = eCSR_ENCRYPT_TYPE_NONE;
	} else {

		/*set encryption method */
		switch (cipher) {
		case IW_AUTH_CIPHER_NONE:
			encryptionType = eCSR_ENCRYPT_TYPE_NONE;
			break;

		case WLAN_CIPHER_SUITE_WEP40:
			encryptionType = eCSR_ENCRYPT_TYPE_WEP40;
			break;

		case WLAN_CIPHER_SUITE_WEP104:
			encryptionType = eCSR_ENCRYPT_TYPE_WEP104;
			break;

		case WLAN_CIPHER_SUITE_TKIP:
			encryptionType = eCSR_ENCRYPT_TYPE_TKIP;
			break;

		case WLAN_CIPHER_SUITE_CCMP:
			encryptionType = eCSR_ENCRYPT_TYPE_AES;
			break;
#ifdef FEATURE_WLAN_WAPI
		case WLAN_CIPHER_SUITE_SMS4:
			encryptionType = eCSR_ENCRYPT_TYPE_WPI;
			break;
#endif

#ifdef FEATURE_WLAN_ESE
		case WLAN_CIPHER_SUITE_KRK:
			encryptionType = eCSR_ENCRYPT_TYPE_KRK;
			break;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
		case WLAN_CIPHER_SUITE_BTK:
			encryptionType = eCSR_ENCRYPT_TYPE_BTK;
			break;
#endif
#endif
		default:
			hdd_err("Unsupported cipher type %d", cipher);
			return -EOPNOTSUPP;
		}
	}

	if (ucast) {
		hdd_notice("setting unicast cipher type to %d", encryptionType);
		pHddStaCtx->conn_info.ucEncryptionType = encryptionType;
		pWextState->roamProfile.EncryptionType.numEntries = 1;
		pWextState->roamProfile.EncryptionType.encryptionType[0] =
			encryptionType;
	} else {
		hdd_notice("setting mcast cipher type to %d", encryptionType);
		pHddStaCtx->conn_info.mcEncryptionType = encryptionType;
		pWextState->roamProfile.mcEncryptionType.numEntries = 1;
		pWextState->roamProfile.mcEncryptionType.encryptionType[0] =
			encryptionType;
	}

	return 0;
}

/**
 * wlan_hdd_cfg80211_set_ie() - set IEs
 * @pAdapter: Pointer to adapter
 * @ie: Pointer ot ie
 * @ie: IE length
 *
 * Return: 0 for success, non-zero for failure
 */
int wlan_hdd_cfg80211_set_ie(hdd_adapter_t *pAdapter, const uint8_t *ie,
			     size_t ie_len)
{
	hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
	const uint8_t *genie = ie;
	uint16_t remLen = ie_len;
#ifdef FEATURE_WLAN_WAPI
	uint32_t akmsuite[MAX_NUM_AKM_SUITES];
	u16 *tmp;
	uint16_t akmsuiteCount;
	int *akmlist;
#endif

	/* clear previous assocAddIE */
	pWextState->assocAddIE.length = 0;
	pWextState->roamProfile.bWPSAssociation = false;
	pWextState->roamProfile.bOSENAssociation = false;

	while (remLen >= 2) {
		uint16_t eLen = 0;
		uint8_t elementId;
		elementId = *genie++;
		eLen = *genie++;
		remLen -= 2;

		hdd_notice("IE[0x%X], LEN[%d]", elementId, eLen);

		switch (elementId) {
		case DOT11F_EID_WPA:
			if (4 > eLen) { /* should have at least OUI which is 4 bytes so extra 2 bytes not needed */
				hdd_err("Invalid WPA IE");
				return -EINVAL;
			} else if (0 ==
				   memcmp(&genie[0], "\x00\x50\xf2\x04", 4)) {
				uint16_t curAddIELen =
					pWextState->assocAddIE.length;
				hdd_notice("Set WPS IE(len %d)", eLen + 2);

				if (SIR_MAC_MAX_ADD_IE_LENGTH <
				    (pWextState->assocAddIE.length + eLen)) {
					hdd_err("Cannot accommodate assocAddIE. Need bigger buffer space");
					QDF_ASSERT(0);
					return -ENOMEM;
				}
				/* WSC IE is saved to Additional IE ; it should be accumulated to handle WPS IE + P2P IE */
				memcpy(pWextState->assocAddIE.addIEdata +
				       curAddIELen, genie - 2, eLen + 2);
				pWextState->assocAddIE.length += eLen + 2;

				pWextState->roamProfile.bWPSAssociation = true;
				pWextState->roamProfile.pAddIEAssoc =
					pWextState->assocAddIE.addIEdata;
				pWextState->roamProfile.nAddIEAssocLength =
					pWextState->assocAddIE.length;
			} else if (0 == memcmp(&genie[0], "\x00\x50\xf2", 3)) {
				hdd_notice("Set WPA IE (len %d)", eLen + 2);
				memset(pWextState->WPARSNIE, 0,
				       MAX_WPA_RSN_IE_LEN);
				memcpy(pWextState->WPARSNIE, genie - 2,
				       (eLen + 2));
				pWextState->roamProfile.pWPAReqIE =
					pWextState->WPARSNIE;
				pWextState->roamProfile.nWPAReqIELength = eLen + 2;     /* ie_len; */
			} else if ((0 == memcmp(&genie[0], P2P_OUI_TYPE,
						P2P_OUI_TYPE_SIZE))) {
				uint16_t curAddIELen =
					pWextState->assocAddIE.length;
				hdd_notice("Set P2P IE(len %d)", eLen + 2);

				if (SIR_MAC_MAX_ADD_IE_LENGTH <
				    (pWextState->assocAddIE.length + eLen)) {
					hdd_err("Cannot accommodate assocAddIE Need bigger buffer space");
					QDF_ASSERT(0);
					return -ENOMEM;
				}
				/* P2P IE is saved to Additional IE ; it should be accumulated to handle WPS IE + P2P IE */
				memcpy(pWextState->assocAddIE.addIEdata +
				       curAddIELen, genie - 2, eLen + 2);
				pWextState->assocAddIE.length += eLen + 2;

				pWextState->roamProfile.pAddIEAssoc =
					pWextState->assocAddIE.addIEdata;
				pWextState->roamProfile.nAddIEAssocLength =
					pWextState->assocAddIE.length;
			}
#ifdef WLAN_FEATURE_WFD
			else if ((0 == memcmp(&genie[0], WFD_OUI_TYPE,
					      WFD_OUI_TYPE_SIZE)) &&
				/* Consider WFD IE, only for P2P Client */
				 (QDF_P2P_CLIENT_MODE ==
				     pAdapter->device_mode)) {
				uint16_t curAddIELen =
					pWextState->assocAddIE.length;
				hdd_notice("Set WFD IE(len %d)", eLen + 2);

				if (SIR_MAC_MAX_ADD_IE_LENGTH <
				    (pWextState->assocAddIE.length + eLen)) {
					hdd_err("Cannot accommodate assocAddIE Need bigger buffer space");
					QDF_ASSERT(0);
					return -ENOMEM;
				}
				/* WFD IE is saved to Additional IE ; it should
				 * be accumulated to handle WPS IE + P2P IE +
				 * WFD IE */
				memcpy(pWextState->assocAddIE.addIEdata +
				       curAddIELen, genie - 2, eLen + 2);
				pWextState->assocAddIE.length += eLen + 2;

				pWextState->roamProfile.pAddIEAssoc =
					pWextState->assocAddIE.addIEdata;
				pWextState->roamProfile.nAddIEAssocLength =
					pWextState->assocAddIE.length;
			}
#endif
			/* Appending HS 2.0 Indication Element in Assiciation Request */
			else if ((0 == memcmp(&genie[0], HS20_OUI_TYPE,
					      HS20_OUI_TYPE_SIZE))) {
				uint16_t curAddIELen =
					pWextState->assocAddIE.length;
				hdd_notice("Set HS20 IE(len %d)", eLen + 2);

				if (SIR_MAC_MAX_ADD_IE_LENGTH <
				    (pWextState->assocAddIE.length + eLen)) {
					hdd_err("Cannot accommodate assocAddIE Need bigger buffer space");
					QDF_ASSERT(0);
					return -ENOMEM;
				}
				memcpy(pWextState->assocAddIE.addIEdata +
				       curAddIELen, genie - 2, eLen + 2);
				pWextState->assocAddIE.length += eLen + 2;

				pWextState->roamProfile.pAddIEAssoc =
					pWextState->assocAddIE.addIEdata;
				pWextState->roamProfile.nAddIEAssocLength =
					pWextState->assocAddIE.length;
			}
			/* Appending OSEN Information  Element in Assiciation Request */
			else if ((0 == memcmp(&genie[0], OSEN_OUI_TYPE,
					      OSEN_OUI_TYPE_SIZE))) {
				uint16_t curAddIELen =
					pWextState->assocAddIE.length;
				hdd_notice("Set OSEN IE(len %d)", eLen + 2);

				if (SIR_MAC_MAX_ADD_IE_LENGTH <
				    (pWextState->assocAddIE.length + eLen)) {
					hdd_err("Cannot accommodate assocAddIE Need bigger buffer space");
					QDF_ASSERT(0);
					return -ENOMEM;
				}
				memcpy(pWextState->assocAddIE.addIEdata +
				       curAddIELen, genie - 2, eLen + 2);
				pWextState->assocAddIE.length += eLen + 2;

				pWextState->roamProfile.bOSENAssociation = true;
				pWextState->roamProfile.pAddIEAssoc =
					pWextState->assocAddIE.addIEdata;
				pWextState->roamProfile.nAddIEAssocLength =
					pWextState->assocAddIE.length;
			} else {
				uint16_t add_ie_len =
					pWextState->assocAddIE.length;

				hdd_info("Set OSEN IE(len %d)", eLen + 2);

				if (SIR_MAC_MAX_ADD_IE_LENGTH <
				    (pWextState->assocAddIE.length + eLen)) {
					hdd_err("Cannot accommodate assocAddIE Need bigger buffer space");
					QDF_ASSERT(0);
					return -ENOMEM;
				}

				memcpy(pWextState->assocAddIE.addIEdata +
				       add_ie_len, genie - 2, eLen + 2);
				pWextState->assocAddIE.length += eLen + 2;

				pWextState->roamProfile.pAddIEAssoc =
					pWextState->assocAddIE.addIEdata;
				pWextState->roamProfile.nAddIEAssocLength =
					pWextState->assocAddIE.length;
			}
			break;
		case DOT11F_EID_RSN:
			hdd_notice("Set RSN IE(len %d)", eLen + 2);
			memset(pWextState->WPARSNIE, 0, MAX_WPA_RSN_IE_LEN);
			memcpy(pWextState->WPARSNIE, genie - 2,
			       (eLen + 2));
			pWextState->roamProfile.pRSNReqIE =
				pWextState->WPARSNIE;
			pWextState->roamProfile.nRSNReqIELength = eLen + 2;     /* ie_len; */
			break;
		/*
		 * Appending Extended Capabilities with Interworking bit set
		 * in Assoc Req.
		 *
		 * In assoc req this EXT Cap will only be taken into account if
		 * interworkingService bit is set to 1. Currently
		 * driver is only interested in interworkingService capability
		 * from supplicant. If in future any other EXT Cap info is
		 * required from supplicat, it needs to be handled while
		 * sending Assoc Req in LIM.
		 */
		case DOT11F_EID_EXTCAP:
		{
			uint16_t curAddIELen =
				pWextState->assocAddIE.length;
			hdd_notice("Set Extended CAPS IE(len %d)", eLen + 2);

			if (SIR_MAC_MAX_ADD_IE_LENGTH <
			    (pWextState->assocAddIE.length + eLen)) {
				hdd_err("Cannot accommodate assocAddIE Need bigger buffer space");
				QDF_ASSERT(0);
				return -ENOMEM;
			}
			memcpy(pWextState->assocAddIE.addIEdata +
			       curAddIELen, genie - 2, eLen + 2);
			pWextState->assocAddIE.length += eLen + 2;

			pWextState->roamProfile.pAddIEAssoc =
				pWextState->assocAddIE.addIEdata;
			pWextState->roamProfile.nAddIEAssocLength =
				pWextState->assocAddIE.length;
			break;
		}
#ifdef FEATURE_WLAN_WAPI
		case WLAN_EID_WAPI:
			/* Setting WAPI Mode to ON=1 */
			pAdapter->wapi_info.nWapiMode = 1;
			hdd_notice("WAPI MODE IS %u", pAdapter->wapi_info.nWapiMode);
			tmp = (u16 *) ie;
			tmp = tmp + 2;  /* Skip element Id and Len, Version */
			akmsuiteCount = WPA_GET_LE16(tmp);
			tmp = tmp + 1;
			akmlist = (int *)(tmp);
			if (akmsuiteCount <= MAX_NUM_AKM_SUITES) {
				memcpy(akmsuite, akmlist, (4 * akmsuiteCount));
			} else {
				hdd_err("Invalid akmSuite count");
				QDF_ASSERT(0);
				return -EINVAL;
			}

			if (WAPI_PSK_AKM_SUITE == akmsuite[0]) {
				hdd_notice("WAPI AUTH MODE SET TO PSK");
				pAdapter->wapi_info.wapiAuthMode =
					WAPI_AUTH_MODE_PSK;
			}
			if (WAPI_CERT_AKM_SUITE == akmsuite[0]) {
				hdd_notice("WAPI AUTH MODE SET TO CERTIFICATE");
				pAdapter->wapi_info.wapiAuthMode =
					WAPI_AUTH_MODE_CERT;
			}
			break;
#endif
		default:
			hdd_err("Set UNKNOWN IE %X", elementId);
			/* when Unknown IE is received we should break and continue
			 * to the next IE in the buffer instead we were returning
			 * so changing this to break */
			break;
		}
		genie += eLen;
		remLen -= eLen;
	}
	return 0;
}

/**
 * hdd_is_wpaie_present() - check for WPA ie
 * @ie: Pointer to ie
 * @ie_len: Ie length
 *
 * Parse the received IE to find the WPA IE
 *
 * Return: true if wpa ie is found else false
 */
static bool hdd_is_wpaie_present(const uint8_t *ie, uint8_t ie_len)
{
	uint8_t eLen = 0;
	uint16_t remLen = ie_len;
	uint8_t elementId = 0;

	while (remLen >= 2) {
		elementId = *ie++;
		eLen = *ie++;
		remLen -= 2;
		if (eLen > remLen) {
			hdd_err("IE length is wrong %d", eLen);
			return false;
		}
		if ((elementId == DOT11F_EID_WPA) && (remLen > 5)) {
			/* OUI - 0x00 0X50 0XF2
			 * WPA Information Element - 0x01
			 * WPA version - 0x01
			 */
			if (0 == memcmp(&ie[0], "\x00\x50\xf2\x01\x01", 5))
				return true;
		}
		ie += eLen;
		remLen -= eLen;
	}
	return false;
}

/**
 * wlan_hdd_cfg80211_set_privacy() - set security parameters during connection
 * @pAdapter: Pointer to adapter
 * @req: Pointer to security parameters
 *
 * Return: 0 for success, non-zero for failure
 */
int wlan_hdd_cfg80211_set_privacy(hdd_adapter_t *pAdapter,
				  struct cfg80211_connect_params *req)
{
	int status = 0;
	hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
	ENTER();

	/*set wpa version */
	pWextState->wpaVersion = IW_AUTH_WPA_VERSION_DISABLED;

	if (req->crypto.wpa_versions) {
		if (NL80211_WPA_VERSION_1 == req->crypto.wpa_versions) {
			pWextState->wpaVersion = IW_AUTH_WPA_VERSION_WPA;
		} else if (NL80211_WPA_VERSION_2 == req->crypto.wpa_versions) {
			pWextState->wpaVersion = IW_AUTH_WPA_VERSION_WPA2;
		}
	}

	hddLog(LOG1, FL("set wpa version to %d"), pWextState->wpaVersion);

	/*set authentication type */
	status = wlan_hdd_cfg80211_set_auth_type(pAdapter, req->auth_type);

	if (0 > status) {
		hddLog(LOGE, FL("failed to set authentication type "));
		return status;
	}

	/*set key mgmt type */
	if (req->crypto.n_akm_suites) {
		status =
			wlan_hdd_set_akm_suite(pAdapter, req->crypto.akm_suites[0]);
		if (0 > status) {
			hddLog(LOGE, FL("failed to set akm suite"));
			return status;
		}
	}

	/*set pairwise cipher type */
	if (req->crypto.n_ciphers_pairwise) {
		status = wlan_hdd_cfg80211_set_cipher(pAdapter,
						      req->crypto.
						      ciphers_pairwise[0],
						      true);
		if (0 > status) {
			hddLog(LOGE, FL("failed to set unicast cipher type"));
			return status;
		}
	} else {
		/*Reset previous cipher suite to none */
		status = wlan_hdd_cfg80211_set_cipher(pAdapter, 0, true);
		if (0 > status) {
			hddLog(LOGE, FL("failed to set unicast cipher type"));
			return status;
		}
	}

	/*set group cipher type */
	status =
		wlan_hdd_cfg80211_set_cipher(pAdapter, req->crypto.cipher_group,
					     false);

	if (0 > status) {
		hddLog(LOGE, FL("failed to set mcast cipher type"));
		return status;
	}
#ifdef WLAN_FEATURE_11W
	pWextState->roamProfile.MFPEnabled = (req->mfp == NL80211_MFP_REQUIRED);
#endif

	/*parse WPA/RSN IE, and set the correspoing fileds in Roam profile */
	if (req->ie_len) {
		status =
			wlan_hdd_cfg80211_set_ie(pAdapter, req->ie, req->ie_len);
		if (0 > status) {
			hddLog(LOGE, FL("failed to parse the WPA/RSN IE"));
			return status;
		}
	}

	/*incase of WEP set default key information */
	if (req->key && req->key_len) {
		if ((WLAN_CIPHER_SUITE_WEP40 == req->crypto.ciphers_pairwise[0])
		    || (WLAN_CIPHER_SUITE_WEP104 ==
			req->crypto.ciphers_pairwise[0])
		    ) {
			if (IW_AUTH_KEY_MGMT_802_1X
			    ==
			    (pWextState->
			     authKeyMgmt & IW_AUTH_KEY_MGMT_802_1X)) {
				hddLog(LOGE,
				       FL("Dynamic WEP not supported"));
				return -EOPNOTSUPP;
			} else {
				u8 key_len = req->key_len;
				u8 key_idx = req->key_idx;

				if ((eCSR_SECURITY_WEP_KEYSIZE_MAX_BYTES >=
				     key_len)
				    && (CSR_MAX_NUM_KEY > key_idx)
				    ) {
					hddLog(LOG1,
						FL("setting default wep key, key_idx = %hu key_len %hu"),
						key_idx, key_len);
					qdf_mem_copy(&pWextState->roamProfile.
						     Keys.
						     KeyMaterial[key_idx][0],
						     req->key, key_len);
					pWextState->roamProfile.Keys.
					KeyLength[key_idx] = (u8) key_len;
					pWextState->roamProfile.Keys.
					defaultIndex = (u8) key_idx;
				}
			}
		}
	}

	return status;
}

/**
 * wlan_hdd_try_disconnect() - try disconnnect from previous connection
 * @pAdapter: Pointer to adapter
 *
 * This function is used to disconnect from previous connection
 *
 * Return: 0 for success, non-zero for failure
 */
static int wlan_hdd_try_disconnect(hdd_adapter_t *pAdapter)
{
	unsigned long rc;
	hdd_station_ctx_t *pHddStaCtx;
	eMib_dot11DesiredBssType connectedBssType;
	int status, result = 0;

	pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

	hdd_conn_get_connected_bss_type(pHddStaCtx, &connectedBssType);

	if ((eMib_dot11DesiredBssType_independent == connectedBssType) ||
	  (eConnectionState_Associated == pHddStaCtx->conn_info.connState) ||
	  (eConnectionState_Connecting == pHddStaCtx->conn_info.connState) ||
	  (eConnectionState_IbssConnected == pHddStaCtx->conn_info.connState)) {
		hdd_conn_set_connection_state(pAdapter,
						eConnectionState_Disconnecting);
		/* Issue disconnect to CSR */
		INIT_COMPLETION(pAdapter->disconnect_comp_var);

		status = sme_roam_disconnect(WLAN_HDD_GET_HAL_CTX(pAdapter),
				pAdapter->sessionId,
				eCSR_DISCONNECT_REASON_UNSPECIFIED);
		/*
		 * Wait here instead of returning directly, this will block the
		 * next connect command and allow processing of the scan for
		 * ssid and the previous connect command in CSR. Else we might
		 * hit some race conditions leading to SME and HDD out of sync.
		 */
		if (QDF_STATUS_CMD_NOT_QUEUED == status) {
			hdd_info("Already disconnected or connect was in sme/roam pending list and removed by disconnect");
		} else if (0 != status) {
			hdd_err("csrRoamDisconnect failure, returned %d",
				(int)status);
			pHddStaCtx->staDebugState = status;
			result = -EINVAL;
			goto disconnected;
		}

		rc = wait_for_completion_timeout(
			&pAdapter->disconnect_comp_var,
			msecs_to_jiffies(WLAN_WAIT_TIME_DISCONNECT));
		if (!rc && (QDF_STATUS_CMD_NOT_QUEUED != status)) {
			hdd_err("Sme disconnect event timed out session Id %d staDebugState %d",
				pAdapter->sessionId, pHddStaCtx->staDebugState);
			result = -ETIMEDOUT;
		}
	} else if (eConnectionState_Disconnecting ==
				pHddStaCtx->conn_info.connState) {
		rc = wait_for_completion_timeout(&pAdapter->disconnect_comp_var,
				msecs_to_jiffies(WLAN_WAIT_TIME_DISCONNECT));
		if (!rc) {
			hdd_err("Disconnect event timed out session Id %d staDebugState %d",
				pAdapter->sessionId, pHddStaCtx->staDebugState);
			result = -ETIMEDOUT;
		}
	}
disconnected:
	hdd_conn_set_connection_state(pAdapter, eConnectionState_NotConnected);
	return result;
}

/**
 * __wlan_hdd_cfg80211_connect() - cfg80211 connect api
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @req: Pointer to cfg80211 connect request
 *
 * This function is used to start the association process
 *
 * Return: 0 for success, non-zero for failure
 */
static int __wlan_hdd_cfg80211_connect(struct wiphy *wiphy,
				       struct net_device *ndev,
				       struct cfg80211_connect_params *req)
{
	int status;
	u16 channel;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 15, 0))
	const u8 *bssid_hint = req->bssid_hint;
#else
	const u8 *bssid_hint = NULL;
#endif
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(ndev);
	hdd_context_t *pHddCtx;

	ENTER();

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hddLog(LOGE, FL("Command not allowed in FTM mode"));
		return -EINVAL;
	}

	MTRACE(qdf_trace(QDF_MODULE_ID_HDD,
			 TRACE_CODE_HDD_CFG80211_CONNECT,
			 pAdapter->sessionId, pAdapter->device_mode));
	hddLog(LOG1, FL("Device_mode %s(%d)"),
		hdd_device_mode_to_string(pAdapter->device_mode),
		pAdapter->device_mode);

	if (pAdapter->device_mode != QDF_STA_MODE &&
		pAdapter->device_mode != QDF_P2P_CLIENT_MODE) {
		hddLog(LOGE, FL("Device_mode %s(%d) is not supported"),
			hdd_device_mode_to_string(pAdapter->device_mode),
			pAdapter->device_mode);
		return -EINVAL;
	}

	pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
	if (!pHddCtx) {
		hddLog(LOGE, FL("HDD context is null"));
		return -EINVAL;
	}

	status = wlan_hdd_validate_context(pHddCtx);
	if (0 != status)
		return status;
	if (req->channel) {
		if (!cds_allow_concurrency(
				cds_convert_device_mode_to_qdf_type(
				pAdapter->device_mode),
				req->channel->hw_value, HW_MODE_20_MHZ)) {
			hdd_err("This concurrency combination is not allowed");
			return -ECONNREFUSED;
		}
	} else {
		if (!cds_allow_concurrency(
				cds_convert_device_mode_to_qdf_type(
				pAdapter->device_mode), 0, HW_MODE_20_MHZ)) {
			hdd_err("This concurrency combination is not allowed");
			return -ECONNREFUSED;
		}
	}
	wlan_hdd_disable_roaming(pAdapter);

	/*Try disconnecting if already in connected state */
	status = wlan_hdd_try_disconnect(pAdapter);
	if (0 > status) {
		hddLog(LOGE,
			FL("Failed to disconnect the existing connection"));
		return -EALREADY;
	}

	/*initialise security parameters */
	status = wlan_hdd_cfg80211_set_privacy(pAdapter, req);

	if (0 > status) {
		hddLog(LOGE,
			FL("failed to set security params"));
		return status;
	}

	if (req->channel)
		channel = req->channel->hw_value;
	else
		channel = 0;
	status = wlan_hdd_cfg80211_connect_start(pAdapter, req->ssid,
						 req->ssid_len, req->bssid,
						 bssid_hint, channel);
	if (0 > status) {
		hddLog(LOGE, FL("connect failed"));
		return status;
	}
	EXIT();
	return status;
}

/**
 * wlan_hdd_cfg80211_connect() - cfg80211 connect api
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @req: Pointer to cfg80211 connect request
 *
 * Return: 0 for success, non-zero for failure
 */
static int wlan_hdd_cfg80211_connect(struct wiphy *wiphy,
				     struct net_device *ndev,
				     struct cfg80211_connect_params *req)
{
	int ret;
	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_connect(wiphy, ndev, req);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * wlan_hdd_disconnect() - hdd disconnect api
 * @pAdapter: Pointer to adapter
 * @reason: Disconnect reason code
 *
 * This function is used to issue a disconnect request to SME
 *
 * Return: 0 for success, non-zero for failure
 */
int wlan_hdd_disconnect(hdd_adapter_t *pAdapter, u16 reason)
{
	int status, result = 0;
	unsigned long rc;
	hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
	hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

	ENTER();

	status = wlan_hdd_validate_context(pHddCtx);

	if (0 != status)
		return status;

	/*stop tx queues */
	hddLog(LOG1, FL("Disabling queues"));
	wlan_hdd_netif_queue_control(pAdapter, WLAN_NETIF_TX_DISABLE_N_CARRIER,
				   WLAN_CONTROL_PATH);
	hddLog(LOG1,
		FL("Set HDD connState to eConnectionState_Disconnecting"));
	pHddStaCtx->conn_info.connState = eConnectionState_Disconnecting;
	INIT_COMPLETION(pAdapter->disconnect_comp_var);

	/*issue disconnect */

	status = sme_roam_disconnect(WLAN_HDD_GET_HAL_CTX(pAdapter),
				     pAdapter->sessionId, reason);
	/*
	 * Wait here instead of returning directly, this will block the next
	 * connect command and allow processing of the scan for ssid and
	 * the previous connect command in CSR. Else we might hit some
	 * race conditions leading to SME and HDD out of sync.
	 */
	if (QDF_STATUS_CMD_NOT_QUEUED == status) {
		hdd_info("Already disconnected or connect was in sme/roam pending list and removed by disconnect");
	} else if (0 != status) {
		hddLog(LOGE,
			FL("csr_roam_disconnect failure, returned %d"),
			(int)status);
		pHddStaCtx->staDebugState = status;
		result = -EINVAL;
		goto disconnected;
	}
	rc = wait_for_completion_timeout(&pAdapter->disconnect_comp_var,
					 msecs_to_jiffies
						 (WLAN_WAIT_TIME_DISCONNECT));

	if (!rc && (QDF_STATUS_CMD_NOT_QUEUED != status)) {
		hddLog(LOGE,
			FL("Failed to disconnect, timed out"));
		result = -ETIMEDOUT;
	}
disconnected:
	hdd_conn_set_connection_state(pAdapter, eConnectionState_NotConnected);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
	/* Sending disconnect event to userspace for kernel version < 3.11
	 * is handled by __cfg80211_disconnect call to __cfg80211_disconnected
	 */
	hddLog(LOG1, FL("Send disconnected event to userspace"));
	wlan_hdd_cfg80211_indicate_disconnect(pAdapter->dev, true,
						WLAN_REASON_UNSPECIFIED);
#endif

	return result;
}

/**
 * hdd_ieee80211_reason_code_to_str() - return string conversion of reason code
 * @reason: ieee80211 reason code.
 *
 * This utility function helps log string conversion of reason code.
 *
 * Return: string conversion of reason code, if match found;
 *         "Unknown" otherwise.
 */
static const char *hdd_ieee80211_reason_code_to_str(uint16_t reason)
{
	switch (reason) {
	CASE_RETURN_STRING(WLAN_REASON_UNSPECIFIED);
	CASE_RETURN_STRING(WLAN_REASON_PREV_AUTH_NOT_VALID);
	CASE_RETURN_STRING(WLAN_REASON_DEAUTH_LEAVING);
	CASE_RETURN_STRING(WLAN_REASON_DISASSOC_DUE_TO_INACTIVITY);
	CASE_RETURN_STRING(WLAN_REASON_DISASSOC_AP_BUSY);
	CASE_RETURN_STRING(WLAN_REASON_CLASS2_FRAME_FROM_NONAUTH_STA);
	CASE_RETURN_STRING(WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA);
	CASE_RETURN_STRING(WLAN_REASON_DISASSOC_STA_HAS_LEFT);
	CASE_RETURN_STRING(WLAN_REASON_STA_REQ_ASSOC_WITHOUT_AUTH);
	CASE_RETURN_STRING(WLAN_REASON_DISASSOC_BAD_POWER);
	CASE_RETURN_STRING(WLAN_REASON_DISASSOC_BAD_SUPP_CHAN);
	CASE_RETURN_STRING(WLAN_REASON_INVALID_IE);
	CASE_RETURN_STRING(WLAN_REASON_MIC_FAILURE);
	CASE_RETURN_STRING(WLAN_REASON_4WAY_HANDSHAKE_TIMEOUT);
	CASE_RETURN_STRING(WLAN_REASON_GROUP_KEY_HANDSHAKE_TIMEOUT);
	CASE_RETURN_STRING(WLAN_REASON_IE_DIFFERENT);
	CASE_RETURN_STRING(WLAN_REASON_INVALID_GROUP_CIPHER);
	CASE_RETURN_STRING(WLAN_REASON_INVALID_PAIRWISE_CIPHER);
	CASE_RETURN_STRING(WLAN_REASON_INVALID_AKMP);
	CASE_RETURN_STRING(WLAN_REASON_UNSUPP_RSN_VERSION);
	CASE_RETURN_STRING(WLAN_REASON_INVALID_RSN_IE_CAP);
	CASE_RETURN_STRING(WLAN_REASON_IEEE8021X_FAILED);
	CASE_RETURN_STRING(WLAN_REASON_CIPHER_SUITE_REJECTED);
	CASE_RETURN_STRING(WLAN_REASON_DISASSOC_UNSPECIFIED_QOS);
	CASE_RETURN_STRING(WLAN_REASON_DISASSOC_QAP_NO_BANDWIDTH);
	CASE_RETURN_STRING(WLAN_REASON_DISASSOC_LOW_ACK);
	CASE_RETURN_STRING(WLAN_REASON_DISASSOC_QAP_EXCEED_TXOP);
	CASE_RETURN_STRING(WLAN_REASON_QSTA_LEAVE_QBSS);
	CASE_RETURN_STRING(WLAN_REASON_QSTA_NOT_USE);
	CASE_RETURN_STRING(WLAN_REASON_QSTA_REQUIRE_SETUP);
	CASE_RETURN_STRING(WLAN_REASON_QSTA_TIMEOUT);
	CASE_RETURN_STRING(WLAN_REASON_QSTA_CIPHER_NOT_SUPP);
	CASE_RETURN_STRING(WLAN_REASON_MESH_PEER_CANCELED);
	CASE_RETURN_STRING(WLAN_REASON_MESH_MAX_PEERS);
	CASE_RETURN_STRING(WLAN_REASON_MESH_CONFIG);
	CASE_RETURN_STRING(WLAN_REASON_MESH_CLOSE);
	CASE_RETURN_STRING(WLAN_REASON_MESH_MAX_RETRIES);
	CASE_RETURN_STRING(WLAN_REASON_MESH_CONFIRM_TIMEOUT);
	CASE_RETURN_STRING(WLAN_REASON_MESH_INVALID_GTK);
	CASE_RETURN_STRING(WLAN_REASON_MESH_INCONSISTENT_PARAM);
	CASE_RETURN_STRING(WLAN_REASON_MESH_INVALID_SECURITY);
	CASE_RETURN_STRING(WLAN_REASON_MESH_PATH_ERROR);
	CASE_RETURN_STRING(WLAN_REASON_MESH_PATH_NOFORWARD);
	CASE_RETURN_STRING(WLAN_REASON_MESH_PATH_DEST_UNREACHABLE);
	CASE_RETURN_STRING(WLAN_REASON_MAC_EXISTS_IN_MBSS);
	CASE_RETURN_STRING(WLAN_REASON_MESH_CHAN_REGULATORY);
	CASE_RETURN_STRING(WLAN_REASON_MESH_CHAN);
	default:
		return "Unknown";
	}
}

/**
 * __wlan_hdd_cfg80211_disconnect() - cfg80211 disconnect api
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @reason: Disconnect reason code
 *
 * This function is used to issue a disconnect request to SME
 *
 * Return: 0 for success, non-zero for failure
 */
static int __wlan_hdd_cfg80211_disconnect(struct wiphy *wiphy,
					  struct net_device *dev, u16 reason)
{
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
	int status;
	hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
	hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
#ifdef FEATURE_WLAN_TDLS
	uint8_t staIdx;
#endif

	ENTER();

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hddLog(LOGE, FL("Command not allowed in FTM mode"));
		return -EINVAL;
	}

	MTRACE(qdf_trace(QDF_MODULE_ID_HDD,
			 TRACE_CODE_HDD_CFG80211_DISCONNECT,
			 pAdapter->sessionId, reason));
	hddLog(LOG1, FL("Device_mode %s(%d) reason code(%d)"),
		hdd_device_mode_to_string(pAdapter->device_mode),
		pAdapter->device_mode, reason);

	status = wlan_hdd_validate_context(pHddCtx);

	if (0 != status)
		return status;

	/* Issue disconnect request to SME, if station is in connected state */
	if ((pHddStaCtx->conn_info.connState == eConnectionState_Associated) ||
	    (pHddStaCtx->conn_info.connState == eConnectionState_Connecting)) {
		eCsrRoamDisconnectReason reasonCode =
			eCSR_DISCONNECT_REASON_UNSPECIFIED;
		hdd_scaninfo_t *pScanInfo;

		switch (reason) {
		case WLAN_REASON_MIC_FAILURE:
			reasonCode = eCSR_DISCONNECT_REASON_MIC_ERROR;
			break;

		case WLAN_REASON_DISASSOC_DUE_TO_INACTIVITY:
		case WLAN_REASON_DISASSOC_AP_BUSY:
		case WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA:
			reasonCode = eCSR_DISCONNECT_REASON_DISASSOC;
			break;

		case WLAN_REASON_PREV_AUTH_NOT_VALID:
		case WLAN_REASON_CLASS2_FRAME_FROM_NONAUTH_STA:
			reasonCode = eCSR_DISCONNECT_REASON_DEAUTH;
			break;

		case WLAN_REASON_DEAUTH_LEAVING:
			reasonCode =
				pHddCtx->config->
				gEnableDeauthToDisassocMap ?
				eCSR_DISCONNECT_REASON_STA_HAS_LEFT :
				eCSR_DISCONNECT_REASON_DEAUTH;
			break;
		case WLAN_REASON_DISASSOC_STA_HAS_LEFT:
			reasonCode = eCSR_DISCONNECT_REASON_STA_HAS_LEFT;
			break;
		default:
			reasonCode = eCSR_DISCONNECT_REASON_UNSPECIFIED;
			break;
		}
		hddLog(LOG1,
			FL("convert to internal reason %d to reasonCode %d"),
			reason, reasonCode);
		pScanInfo = &pAdapter->scan_info;
		if (pScanInfo->mScanPending) {
			hddLog(LOG1,
				FL("Disconnect is in progress, Aborting Scan"));
			hdd_abort_mac_scan(pHddCtx, pAdapter->sessionId,
					   eCSR_SCAN_ABORT_DEFAULT);
		}
#ifdef FEATURE_WLAN_TDLS
		/* First clean up the tdls peers if any */
		for (staIdx = 0; staIdx < pHddCtx->max_num_tdls_sta; staIdx++) {
			if ((pHddCtx->tdlsConnInfo[staIdx].sessionId ==
			     pAdapter->sessionId)
			    && (pHddCtx->tdlsConnInfo[staIdx].staId)) {
				uint8_t *mac;
				mac =
					pHddCtx->tdlsConnInfo[staIdx].peerMac.bytes;
				hddLog(LOG1,
				       "%s: call sme_delete_tdls_peer_sta staId %d sessionId %d "
				       MAC_ADDRESS_STR, __func__,
				       pHddCtx->tdlsConnInfo[staIdx].staId,
				       pAdapter->sessionId,
				       MAC_ADDR_ARRAY(mac));
				sme_delete_tdls_peer_sta(WLAN_HDD_GET_HAL_CTX
								 (pAdapter),
							 pAdapter->sessionId, mac);
			}
		}
#endif
		hddLog(LOG1, FL("Disconnecting with reasoncode:%u"),
		       reasonCode);
		hdd_info("Disconnect request from user space with reason: %s",
			hdd_ieee80211_reason_code_to_str(reason));
		status = wlan_hdd_disconnect(pAdapter, reasonCode);
		if (0 != status) {
			hddLog(LOGE,
			       FL("failure, returned %d"), status);
			return -EINVAL;
		}
	} else {
		hddLog(LOGE,
		       FL("unexpected cfg disconnect called while in state (%d)"),
		       pHddStaCtx->conn_info.connState);
	}

	return status;
}

/**
 * wlan_hdd_cfg80211_disconnect() - cfg80211 disconnect api
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @reason: Disconnect reason code
 *
 * Return: 0 for success, non-zero for failure
 */
static int wlan_hdd_cfg80211_disconnect(struct wiphy *wiphy,
					struct net_device *dev, u16 reason)
{
	int ret;
	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_disconnect(wiphy, dev, reason);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * wlan_hdd_cfg80211_set_privacy_ibss() - set ibss privacy
 * @pAdapter: Pointer to adapter
 * @param: Pointer to IBSS parameters
 *
 * This function is used to initialize the security settings in IBSS mode
 *
 * Return: 0 for success, non-zero for failure
 */
static int wlan_hdd_cfg80211_set_privacy_ibss(hdd_adapter_t *pAdapter,
					      struct cfg80211_ibss_params
					      *params)
{
	int status = 0;
	hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
	eCsrEncryptionType encryptionType = eCSR_ENCRYPT_TYPE_NONE;
	hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

	ENTER();

	pWextState->wpaVersion = IW_AUTH_WPA_VERSION_DISABLED;
	qdf_mem_zero(&pHddStaCtx->ibss_enc_key, sizeof(tCsrRoamSetKey));
	pHddStaCtx->ibss_enc_key_installed = 0;

	if (params->ie_len && (NULL != params->ie)) {
		if (wlan_hdd_cfg80211_get_ie_ptr(params->ie,
					 params->ie_len, WLAN_EID_RSN)) {
			pWextState->wpaVersion = IW_AUTH_WPA_VERSION_WPA2;
			encryptionType = eCSR_ENCRYPT_TYPE_AES;
		} else if (hdd_is_wpaie_present(params->ie, params->ie_len)) {
			tDot11fIEWPA dot11WPAIE;
			tHalHandle halHandle = WLAN_HDD_GET_HAL_CTX(pAdapter);
			u8 *ie;

			memset(&dot11WPAIE, 0, sizeof(dot11WPAIE));
			ie = wlan_hdd_cfg80211_get_ie_ptr(params->ie,
							  params->ie_len,
							  DOT11F_EID_WPA);
			if (NULL != ie) {
				pWextState->wpaVersion =
					IW_AUTH_WPA_VERSION_WPA;
				/* Unpack the WPA IE */
				/* Skip past the EID byte and length byte - and four byte WiFi OUI */
				dot11f_unpack_ie_wpa((tpAniSirGlobal) halHandle,
						     &ie[2 + 4],
						     ie[1] - 4, &dot11WPAIE);
				/*Extract the multicast cipher, the encType for unicast
				   cipher for wpa-none is none */
				encryptionType =
					hdd_translate_wpa_to_csr_encryption_type
						(dot11WPAIE.multicast_cipher);
			}
		}

		status =
			wlan_hdd_cfg80211_set_ie(pAdapter, params->ie,
						 params->ie_len);

		if (0 > status) {
			hddLog(LOGE, FL("failed to parse WPA/RSN IE"));
			return status;
		}
	}

	pWextState->roamProfile.AuthType.authType[0] =
		pHddStaCtx->conn_info.authType = eCSR_AUTH_TYPE_OPEN_SYSTEM;

	if (params->privacy) {
		/* Security enabled IBSS, At this time there is no information
		 * available about the security paramters, so initialise the
		 * encryption type to eCSR_ENCRYPT_TYPE_WEP40_STATICKEY.
		 * The correct security parameters will be updated later in
		 * wlan_hdd_cfg80211_add_key Hal expects encryption type to be
		 * set inorder enable privacy bit in beacons
		 */

		encryptionType = eCSR_ENCRYPT_TYPE_WEP40_STATICKEY;
	}
	hddLog(LOG2, FL("encryptionType=%d"), encryptionType);
	pHddStaCtx->conn_info.ucEncryptionType = encryptionType;
	pWextState->roamProfile.EncryptionType.numEntries = 1;
	pWextState->roamProfile.EncryptionType.encryptionType[0] =
		encryptionType;
	return status;
}

/**
 * __wlan_hdd_cfg80211_join_ibss() - join ibss
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @param: Pointer to IBSS join parameters
 *
 * This function is used to create/join an IBSS network
 *
 * Return: 0 for success, non-zero for failure
 */
static int __wlan_hdd_cfg80211_join_ibss(struct wiphy *wiphy,
					 struct net_device *dev,
					 struct cfg80211_ibss_params *params)
{
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
	tCsrRoamProfile *pRoamProfile;
	int status;
	hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
	hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
	struct qdf_mac_addr bssid;
	u8 channelNum = 0;

	ENTER();

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hddLog(LOGE, FL("Command not allowed in FTM mode"));
		return -EINVAL;
	}

	MTRACE(qdf_trace(QDF_MODULE_ID_HDD,
			 TRACE_CODE_HDD_CFG80211_JOIN_IBSS,
			 pAdapter->sessionId, pAdapter->device_mode));
	hddLog(LOG1, FL("Device_mode %s(%d)"),
		hdd_device_mode_to_string(pAdapter->device_mode),
		pAdapter->device_mode);

	status = wlan_hdd_validate_context(pHddCtx);

	if (0 != status)
		return status;

	if (NULL !=
		params->chandef.chan) {
		uint32_t numChans = WNI_CFG_VALID_CHANNEL_LIST_LEN;
		uint8_t validChan[WNI_CFG_VALID_CHANNEL_LIST_LEN];
		tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
		int indx;

		/* Get channel number */
		channelNum = ieee80211_frequency_to_channel(
			params->
			chandef.
			chan->
			center_freq);

		if (0 != sme_cfg_get_str(hHal, WNI_CFG_VALID_CHANNEL_LIST,
					 validChan, &numChans)) {
			hddLog(LOGE, FL("No valid channel list"));
			return -EOPNOTSUPP;
		}

		for (indx = 0; indx < numChans; indx++) {
			if (channelNum == validChan[indx]) {
				break;
			}
		}
		if (indx >= numChans) {
			hddLog(LOGE,
				FL("Not valid Channel %d"), channelNum);
			return -EINVAL;
		}
	}

	if (!cds_allow_concurrency(CDS_IBSS_MODE, channelNum,
		HW_MODE_20_MHZ)) {
		hdd_err("This concurrency combination is not allowed");
		return -ECONNREFUSED;
	}
	if (pHddCtx->config->policy_manager_enabled) {
		status = qdf_reset_connection_update();
		if (!QDF_IS_STATUS_SUCCESS(status))
			hdd_err("ERR: clear event failed");

		status = cds_current_connections_update(pAdapter->sessionId,
						channelNum,
						SIR_UPDATE_REASON_JOIN_IBSS);
		if (QDF_STATUS_E_FAILURE == status) {
			hdd_err("ERROR: connections update failed!!");
			return -EINVAL;
		}

		if (QDF_STATUS_SUCCESS == status) {
			status = qdf_wait_for_connection_update();
			if (!QDF_IS_STATUS_SUCCESS(status)) {
				hdd_err("ERROR: qdf wait for event failed!!");
				return -EINVAL;
			}
		}
	}

	/*Try disconnecting if already in connected state */
	status = wlan_hdd_try_disconnect(pAdapter);
	if (0 > status) {
		hddLog(LOGE,
		       FL("Failed to disconnect the existing IBSS connection"));
		return -EALREADY;
	}

	pRoamProfile = &pWextState->roamProfile;

	if (eCSR_BSS_TYPE_START_IBSS != pRoamProfile->BSSType) {
		hddLog(LOGE,
			FL("Interface type is not set to IBSS"));
		return -EINVAL;
	}

	/* enable selected protection checks in IBSS mode */
	pRoamProfile->cfg_protection = IBSS_CFG_PROTECTION_ENABLE_MASK;

	if (QDF_STATUS_E_FAILURE == sme_cfg_set_int(pHddCtx->hHal,
						    WNI_CFG_IBSS_ATIM_WIN_SIZE,
						    pHddCtx->config->
						    ibssATIMWinSize)) {
		hddLog(LOGE,
			FL("Could not pass on WNI_CFG_IBSS_ATIM_WIN_SIZE to CCM"));
	}

	/* BSSID is provided by upper layers hence no need to AUTO generate */
	if (NULL != params->bssid) {
		if (sme_cfg_set_int(pHddCtx->hHal, WNI_CFG_IBSS_AUTO_BSSID, 0)
				== QDF_STATUS_E_FAILURE) {
			hddLog(LOGE,
				FL("ccmCfgStInt failed for WNI_CFG_IBSS_AUTO_BSSID"));
			return -EIO;
		}
		qdf_mem_copy(bssid.bytes, params->bssid, QDF_MAC_ADDR_SIZE);
	} else if (pHddCtx->config->isCoalesingInIBSSAllowed == 0) {
		if (sme_cfg_set_int(pHddCtx->hHal, WNI_CFG_IBSS_AUTO_BSSID, 0)
				== QDF_STATUS_E_FAILURE) {
			hddLog(LOGE,
				FL("ccmCfgStInt failed for WNI_CFG_IBSS_AUTO_BSSID"));
			return -EIO;
		}
		qdf_copy_macaddr(&bssid, &pHddCtx->config->IbssBssid);
	}
	if ((params->beacon_interval > CFG_BEACON_INTERVAL_MIN)
	    && (params->beacon_interval <= CFG_BEACON_INTERVAL_MAX))
		pRoamProfile->beaconInterval = params->beacon_interval;
	else {
		pRoamProfile->beaconInterval = CFG_BEACON_INTERVAL_DEFAULT;
		hddLog(LOG2,
		       FL("input beacon interval %d TU is invalid, use default %d TU"),
			params->beacon_interval, pRoamProfile->beaconInterval);
	}

	/* Set Channel */
	if (channelNum)	{
		/* Set the Operational Channel */
		hddLog(LOG2, FL("set channel %d"), channelNum);
		pRoamProfile->ChannelInfo.numOfChannels = 1;
		pHddStaCtx->conn_info.operationChannel = channelNum;
		pRoamProfile->ChannelInfo.ChannelList =
			&pHddStaCtx->conn_info.operationChannel;
	}

	/* Initialize security parameters */
	status = wlan_hdd_cfg80211_set_privacy_ibss(pAdapter, params);
	if (status < 0) {
		hddLog(LOGE,
			FL("failed to set security parameters"));
		return status;
	}

	/* Issue connect start */
	status = wlan_hdd_cfg80211_connect_start(pAdapter, params->ssid,
						 params->ssid_len,
						 bssid.bytes,
						 NULL,
						 pHddStaCtx->conn_info.
						 operationChannel);

	if (0 > status) {
		hddLog(LOGE, FL("connect failed"));
		return status;
	}
	EXIT();
	return 0;
}

/**
 * wlan_hdd_cfg80211_join_ibss() - join ibss
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @param: Pointer to IBSS join parameters
 *
 * This function is used to create/join an IBSS network
 *
 * Return: 0 for success, non-zero for failure
 */
static int wlan_hdd_cfg80211_join_ibss(struct wiphy *wiphy,
				       struct net_device *dev,
				       struct cfg80211_ibss_params *params)
{
	int ret = 0;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_join_ibss(wiphy, dev, params);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * __wlan_hdd_cfg80211_leave_ibss() - leave ibss
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 *
 * This function is used to leave an IBSS network
 *
 * Return: 0 for success, non-zero for failure
 */
static int __wlan_hdd_cfg80211_leave_ibss(struct wiphy *wiphy,
					  struct net_device *dev)
{
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
	tCsrRoamProfile *pRoamProfile;
	hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
	int status;
	QDF_STATUS hal_status;
	unsigned long rc;
	tSirUpdateIE updateIE;

	ENTER();

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hddLog(LOGE, FL("Command not allowed in FTM mode"));
		return -EINVAL;
	}

	MTRACE(qdf_trace(QDF_MODULE_ID_HDD,
			 TRACE_CODE_HDD_CFG80211_LEAVE_IBSS,
			 pAdapter->sessionId,
			 eCSR_DISCONNECT_REASON_IBSS_LEAVE));
	status = wlan_hdd_validate_context(pHddCtx);
	if (0 != status)
		return status;

	hddLog(LOG1, FL("Device_mode %s(%d)"),
		hdd_device_mode_to_string(pAdapter->device_mode),
		pAdapter->device_mode);
	if (NULL == pWextState) {
		hddLog(LOGE, FL("Data Storage Corruption"));
		return -EIO;
	}

	pRoamProfile = &pWextState->roamProfile;

	/* Issue disconnect only if interface type is set to IBSS */
	if (eCSR_BSS_TYPE_START_IBSS != pRoamProfile->BSSType) {
		hddLog(LOGE,
		       FL("BSS Type is not set to IBSS"));
		return -EINVAL;
	}
	/* Clearing add IE of beacon */
	qdf_mem_copy(updateIE.bssid.bytes, pAdapter->macAddressCurrent.bytes,
		     sizeof(tSirMacAddr));
	updateIE.smeSessionId = pAdapter->sessionId;
	updateIE.ieBufferlength = 0;
	updateIE.pAdditionIEBuffer = NULL;
	updateIE.append = true;
	updateIE.notify = true;
	if (sme_update_add_ie(WLAN_HDD_GET_HAL_CTX(pAdapter),
			      &updateIE,
			      eUPDATE_IE_PROBE_BCN) == QDF_STATUS_E_FAILURE) {
		hddLog(LOGE, FL("Could not pass on PROBE_RSP_BCN data to PE"));
	}

	/* Reset WNI_CFG_PROBE_RSP Flags */
	wlan_hdd_reset_prob_rspies(pAdapter);

	/* Issue Disconnect request */
	INIT_COMPLETION(pAdapter->disconnect_comp_var);
	hal_status = sme_roam_disconnect(WLAN_HDD_GET_HAL_CTX(pAdapter),
					 pAdapter->sessionId,
					 eCSR_DISCONNECT_REASON_IBSS_LEAVE);
	if (!QDF_IS_STATUS_SUCCESS(hal_status)) {
		hddLog(LOGE,
		       FL("sme_roam_disconnect failed hal_status(%d)"),
		       hal_status);
		return -EAGAIN;
	}

	/* wait for mc thread to cleanup and then return to upper stack
	 * so by the time upper layer calls the change interface, we are
	 * all set to proceed further
	 */
	rc = wait_for_completion_timeout(&pAdapter->disconnect_comp_var,
			msecs_to_jiffies(WLAN_WAIT_TIME_DISCONNECT));
	if (!rc) {
		hdd_err("Failed to disconnect, timed out");
		return -ETIMEDOUT;
	}

	EXIT();
	return 0;
}

/**
 * wlan_hdd_cfg80211_leave_ibss() - leave ibss
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 *
 * This function is used to leave an IBSS network
 *
 * Return: 0 for success, non-zero for failure
 */
static int wlan_hdd_cfg80211_leave_ibss(struct wiphy *wiphy,
					struct net_device *dev)
{
	int ret = 0;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_leave_ibss(wiphy, dev);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * __wlan_hdd_cfg80211_set_wiphy_params() - set wiphy parameters
 * @wiphy: Pointer to wiphy
 * @changed: Parameters changed
 *
 * This function is used to set the phy parameters. RTS Threshold/FRAG
 * Threshold/Retry Count etc.
 *
 * Return: 0 for success, non-zero for failure
 */
static int __wlan_hdd_cfg80211_set_wiphy_params(struct wiphy *wiphy,
						u32 changed)
{
	hdd_context_t *pHddCtx = wiphy_priv(wiphy);
	tHalHandle hHal = pHddCtx->hHal;
	int status;

	ENTER();

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hddLog(LOGE, FL("Command not allowed in FTM mode"));
		return -EINVAL;
	}

	MTRACE(qdf_trace(QDF_MODULE_ID_HDD,
			 TRACE_CODE_HDD_CFG80211_SET_WIPHY_PARAMS,
			 NO_SESSION, wiphy->rts_threshold));
	status = wlan_hdd_validate_context(pHddCtx);

	if (0 != status)
		return status;

	if (changed & WIPHY_PARAM_RTS_THRESHOLD) {
		u32 rts_threshold = (wiphy->rts_threshold == -1) ?
				    WNI_CFG_RTS_THRESHOLD_STAMAX : wiphy->rts_threshold;

		if ((WNI_CFG_RTS_THRESHOLD_STAMIN > rts_threshold) ||
		    (WNI_CFG_RTS_THRESHOLD_STAMAX < rts_threshold)) {
			hddLog(LOGE,
				FL("Invalid RTS Threshold value %u"),
				rts_threshold);
			return -EINVAL;
		}

		if (0 != sme_cfg_set_int(hHal, WNI_CFG_RTS_THRESHOLD,
					rts_threshold)) {
			hddLog(LOGE,
				FL("sme_cfg_set_int failed for rts_threshold value %u"),
				rts_threshold);
			return -EIO;
		}

		hddLog(LOG2, FL("set rts threshold %u"), rts_threshold);
	}

	if (changed & WIPHY_PARAM_FRAG_THRESHOLD) {
		u16 frag_threshold = (wiphy->frag_threshold == -1) ?
				     WNI_CFG_FRAGMENTATION_THRESHOLD_STAMAX :
				     wiphy->frag_threshold;

		if ((WNI_CFG_FRAGMENTATION_THRESHOLD_STAMIN > frag_threshold) ||
		    (WNI_CFG_FRAGMENTATION_THRESHOLD_STAMAX < frag_threshold)) {
			hddLog(LOGE,
				FL("Invalid frag_threshold value %hu"),
				frag_threshold);
			return -EINVAL;
		}

		if (0 != sme_cfg_set_int(hHal, WNI_CFG_FRAGMENTATION_THRESHOLD,
					 frag_threshold)) {
			hddLog(LOGE,
				FL("sme_cfg_set_int failed for frag_threshold value %hu"),
				frag_threshold);
			return -EIO;
		}

		hddLog(LOG2, FL("set frag threshold %hu"), frag_threshold);
	}

	if ((changed & WIPHY_PARAM_RETRY_SHORT)
	    || (changed & WIPHY_PARAM_RETRY_LONG)) {
		u8 retry_value = (changed & WIPHY_PARAM_RETRY_SHORT) ?
				 wiphy->retry_short : wiphy->retry_long;

		if ((WNI_CFG_LONG_RETRY_LIMIT_STAMIN > retry_value) ||
		    (WNI_CFG_LONG_RETRY_LIMIT_STAMAX < retry_value)) {
			hddLog(LOGE,
				FL("Invalid Retry count %hu"), retry_value);
			return -EINVAL;
		}

		if (changed & WIPHY_PARAM_RETRY_SHORT) {
			if (0 != sme_cfg_set_int(hHal,
						WNI_CFG_LONG_RETRY_LIMIT,
						retry_value)) {
				hddLog(LOGE,
					FL("sme_cfg_set_int failed for long retry count %hu"),
					retry_value);
				return -EIO;
			}
			hddLog(LOG2,
				FL("set long retry count %hu"), retry_value);
		} else if (changed & WIPHY_PARAM_RETRY_SHORT) {
			if (0 != sme_cfg_set_int(hHal,
						WNI_CFG_SHORT_RETRY_LIMIT,
						retry_value)) {
				hddLog(LOGE,
					FL("sme_cfg_set_int failed for short retry count %hu"),
					retry_value);
				return -EIO;
			}
			hddLog(LOG2,
			       FL("set short retry count %hu"), retry_value);
		}
	}
	EXIT();
	return 0;
}

/**
 * wlan_hdd_cfg80211_set_wiphy_params() - set wiphy parameters
 * @wiphy: Pointer to wiphy
 * @changed: Parameters changed
 *
 * Return: 0 for success, non-zero for failure
 */
static int wlan_hdd_cfg80211_set_wiphy_params(struct wiphy *wiphy, u32 changed)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_set_wiphy_params(wiphy, changed);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * __wlan_hdd_set_default_mgmt_key() - dummy implementation of set default mgmt
 *				     key
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @key_index: Key index
 *
 * Return: 0
 */
static int __wlan_hdd_set_default_mgmt_key(struct wiphy *wiphy,
					 struct net_device *netdev,
					 u8 key_index)
{
	ENTER();
	return 0;
}

/**
 * wlan_hdd_set_default_mgmt_key() - SSR wrapper for
 *				wlan_hdd_set_default_mgmt_key
 * @wiphy: pointer to wiphy
 * @netdev: pointer to net_device structure
 * @key_index: key index
 *
 * Return: 0 on success, error number on failure
 */
static int wlan_hdd_set_default_mgmt_key(struct wiphy *wiphy,
					   struct net_device *netdev,
					   u8 key_index)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_set_default_mgmt_key(wiphy, netdev, key_index);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * __wlan_hdd_set_txq_params() - dummy implementation of set tx queue params
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @params: Pointer to tx queue parameters
 *
 * Return: 0
 */
static int __wlan_hdd_set_txq_params(struct wiphy *wiphy,
				   struct net_device *dev,
				   struct ieee80211_txq_params *params)
{
	ENTER();
	return 0;
}

/**
 * wlan_hdd_set_txq_params() - SSR wrapper for wlan_hdd_set_txq_params
 * @wiphy: pointer to wiphy
 * @netdev: pointer to net_device structure
 * @params: pointer to ieee80211_txq_params
 *
 * Return: 0 on success, error number on failure
 */
static int wlan_hdd_set_txq_params(struct wiphy *wiphy,
				   struct net_device *dev,
				   struct ieee80211_txq_params *params)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_set_txq_params(wiphy, dev, params);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * __wlan_hdd_cfg80211_del_station() - delete station v2
 * @wiphy: Pointer to wiphy
 * @param: Pointer to delete station parameter
 *
 * Return: 0 for success, non-zero for failure
 */
static
int __wlan_hdd_cfg80211_del_station(struct wiphy *wiphy,
				    struct net_device *dev,
				    struct tagCsrDelStaParams *pDelStaParams)
{
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *pHddCtx;
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;
	hdd_hostapd_state_t *hapd_state;
	int status;
	uint8_t staId;
	uint8_t *mac;

	ENTER();

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hddLog(LOGE, FL("Command not allowed in FTM mode"));
		return -EINVAL;
	}

	MTRACE(qdf_trace(QDF_MODULE_ID_HDD,
			 TRACE_CODE_HDD_CFG80211_DEL_STA,
			 pAdapter->sessionId, pAdapter->device_mode));

	pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
	status = wlan_hdd_validate_context(pHddCtx);

	if (0 != status)
		return status;

	mac = (uint8_t *) pDelStaParams->peerMacAddr.bytes;

	if ((QDF_SAP_MODE == pAdapter->device_mode) ||
	    (QDF_P2P_GO_MODE == pAdapter->device_mode)) {

		hapd_state = WLAN_HDD_GET_HOSTAP_STATE_PTR(pAdapter);
		if (!hapd_state) {
			hddLog(LOGE, "%s: Hostapd State is Null", __func__);
			return 0;
		}

		if (qdf_is_macaddr_broadcast((struct qdf_mac_addr *) mac)) {
			uint16_t i;
			for (i = 0; i < WLAN_MAX_STA_COUNT; i++) {
				if ((pAdapter->aStaInfo[i].isUsed) &&
				    (!pAdapter->aStaInfo[i].
				     isDeauthInProgress)) {
					qdf_mem_copy(
						mac,
						pAdapter->aStaInfo[i].
							macAddrSTA.bytes,
						QDF_MAC_ADDR_SIZE);
					if (hdd_ipa_uc_is_enabled(pHddCtx)) {
						hdd_ipa_wlan_evt(pAdapter,
							pAdapter->
								 aStaInfo[i].
								 ucSTAId,
							WLAN_CLIENT_DISCONNECT,
							mac);
					}
					hddLog(LOG1,
					       FL("Delete STA with MAC::"
						  MAC_ADDRESS_STR),
					       MAC_ADDR_ARRAY(mac));

					if (pHddCtx->dev_dfs_cac_status ==
							DFS_CAC_IN_PROGRESS)
						goto fn_end;

					qdf_event_reset(&hapd_state->qdf_event);
					hdd_softap_sta_disassoc(pAdapter,
								mac);
					qdf_status =
						hdd_softap_sta_deauth(pAdapter,
							pDelStaParams);
					if (QDF_IS_STATUS_SUCCESS(qdf_status)) {
						pAdapter->aStaInfo[i].
						isDeauthInProgress = true;
						qdf_status =
							qdf_wait_single_event(
								&hapd_state->qdf_event,
								1000);
						if (!QDF_IS_STATUS_SUCCESS(
								qdf_status))
							hddLog(LOGE,
								"%s: Deauth wait time expired",
								__func__);
					}
				}
			}
		} else {
			qdf_status =
				hdd_softap_get_sta_id(pAdapter,
					      (struct qdf_mac_addr *) mac,
					      &staId);
			if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
				hddLog(LOG1,
				       FL("Skip DEL STA as this is not used::"
					  MAC_ADDRESS_STR),
				       MAC_ADDR_ARRAY(mac));
				return -ENOENT;
			}

			if (hdd_ipa_uc_is_enabled(pHddCtx)) {
				hdd_ipa_wlan_evt(pAdapter, staId,
						 WLAN_CLIENT_DISCONNECT, mac);
			}

			if (pAdapter->aStaInfo[staId].isDeauthInProgress ==
			    true) {
				hddLog(LOG1,
				       FL("Skip DEL STA as deauth is in progress::"
					  MAC_ADDRESS_STR),
					  MAC_ADDR_ARRAY(mac));
				return -ENOENT;
			}

			pAdapter->aStaInfo[staId].isDeauthInProgress = true;

			hddLog(LOG1,
			       FL("Delete STA with MAC::" MAC_ADDRESS_STR),
			       MAC_ADDR_ARRAY(mac));

			/* Case: SAP in ACS selected DFS ch and client connected
			 * Now Radar detected. Then if random channel is another
			 * DFS ch then new CAC is initiated and no TX allowed.
			 * So do not send any mgmt frames as it will timeout
			 * during CAC.
			 */

			if (pHddCtx->dev_dfs_cac_status == DFS_CAC_IN_PROGRESS)
				goto fn_end;

			qdf_event_reset(&hapd_state->qdf_event);
			hdd_softap_sta_disassoc(pAdapter, mac);
			qdf_status = hdd_softap_sta_deauth(pAdapter,
							   pDelStaParams);
			if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
				pAdapter->aStaInfo[staId].isDeauthInProgress =
					false;
				hddLog(LOG1,
				       FL("STA removal failed for ::"
					  MAC_ADDRESS_STR),
				       MAC_ADDR_ARRAY(mac));
				return -ENOENT;
			} else {
				qdf_status = qdf_wait_single_event(
							&hapd_state->qdf_event,
							1000);
				if (!QDF_IS_STATUS_SUCCESS(qdf_status))
					hddLog(LOGE,
						"%s: Deauth wait time expired",
						__func__);
			}
		}
	}

fn_end:
	EXIT();
	return 0;
}

#if defined(USE_CFG80211_DEL_STA_V2)
/**
 * wlan_hdd_del_station() - delete station wrapper
 * @adapter: pointer to the hdd adapter
 *
 * Return: None
 */
void wlan_hdd_del_station(hdd_adapter_t *adapter)
{
	struct station_del_parameters del_sta;
	del_sta.mac = NULL;
	del_sta.subtype = SIR_MAC_MGMT_DEAUTH >> 4;
	del_sta.reason_code = eCsrForcedDeauthSta;

	wlan_hdd_cfg80211_del_station(adapter->wdev.wiphy, adapter->dev,
				      &del_sta);
}
#else
void wlan_hdd_del_station(hdd_adapter_t *adapter)
{
	wlan_hdd_cfg80211_del_station(adapter->wdev.wiphy, adapter->dev, NULL);
}
#endif

#if defined(USE_CFG80211_DEL_STA_V2)
/**
 * wlan_hdd_cfg80211_del_station() - delete station v2
 * @wiphy: Pointer to wiphy
 * @param: Pointer to delete station parameter
 *
 * Return: 0 for success, non-zero for failure
 */
int wlan_hdd_cfg80211_del_station(struct wiphy *wiphy,
				  struct net_device *dev,
				  struct station_del_parameters *param)
#else
/**
 * wlan_hdd_cfg80211_del_station() - delete station
 * @wiphy: Pointer to wiphy
 * @mac: Pointer to station mac address
 *
 * Return: 0 for success, non-zero for failure
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0))
int wlan_hdd_cfg80211_del_station(struct wiphy *wiphy,
				  struct net_device *dev,
				  const uint8_t *mac)
#else
int wlan_hdd_cfg80211_del_station(struct wiphy *wiphy,
				  struct net_device *dev,
				  uint8_t *mac)
#endif
#endif
{
	int ret;
	struct tagCsrDelStaParams delStaParams;

	cds_ssr_protect(__func__);
#if defined(USE_CFG80211_DEL_STA_V2)
	if (NULL == param) {
		hdd_err("Invalid argument passed");
		return -EINVAL;
	}
	wlansap_populate_del_sta_params(param->mac, param->reason_code,
					param->subtype, &delStaParams);
#else
	wlansap_populate_del_sta_params(mac, eSIR_MAC_DEAUTH_LEAVING_BSS_REASON,
					(SIR_MAC_MGMT_DEAUTH >> 4),
					&delStaParams);
#endif
	ret = __wlan_hdd_cfg80211_del_station(wiphy, dev, &delStaParams);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * __wlan_hdd_cfg80211_add_station() - add station
 * @wiphy: Pointer to wiphy
 * @mac: Pointer to station mac address
 * @pmksa: Pointer to add station parameter
 *
 * Return: 0 for success, non-zero for failure
 */
static int __wlan_hdd_cfg80211_add_station(struct wiphy *wiphy,
					   struct net_device *dev,
					   const uint8_t *mac,
					   struct station_parameters *params)
{
	int status = -EPERM;
#ifdef FEATURE_WLAN_TDLS
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *pHddCtx = wiphy_priv(wiphy);
	u32 mask, set;

	ENTER();

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hddLog(LOGE, FL("Command not allowed in FTM mode"));
		return -EINVAL;
	}

	MTRACE(qdf_trace(QDF_MODULE_ID_HDD,
			 TRACE_CODE_HDD_CFG80211_ADD_STA,
			 pAdapter->sessionId, params->listen_interval));

	if (0 != wlan_hdd_validate_context(pHddCtx))
		return -EINVAL;

	mask = params->sta_flags_mask;

	set = params->sta_flags_set;

	hddLog(LOG1, FL("mask 0x%x set 0x%x " MAC_ADDRESS_STR), mask, set,
		MAC_ADDR_ARRAY(mac));

	if (mask & BIT(NL80211_STA_FLAG_TDLS_PEER)) {
		if (set & BIT(NL80211_STA_FLAG_TDLS_PEER)) {
			status =
				wlan_hdd_tdls_add_station(wiphy, dev, mac, 0, NULL);
		}
	}
#endif
	EXIT();
	return status;
}

/**
 * wlan_hdd_cfg80211_add_station() - add station
 * @wiphy: Pointer to wiphy
 * @mac: Pointer to station mac address
 * @pmksa: Pointer to add station parameter
 *
 * Return: 0 for success, non-zero for failure
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0))
static int wlan_hdd_cfg80211_add_station(struct wiphy *wiphy,
					 struct net_device *dev,
					 const uint8_t *mac,
					 struct station_parameters *params)
#else
static int wlan_hdd_cfg80211_add_station(struct wiphy *wiphy,
					 struct net_device *dev, uint8_t *mac,
					 struct station_parameters *params)
#endif
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_add_station(wiphy, dev, mac, params);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * __wlan_hdd_cfg80211_set_pmksa() - set pmksa
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @pmksa: Pointer to set pmksa parameter
 *
 * Return: 0 for success, non-zero for failure
 */
static int __wlan_hdd_cfg80211_set_pmksa(struct wiphy *wiphy,
					 struct net_device *dev,
					 struct cfg80211_pmksa *pmksa)
{
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
	tHalHandle halHandle;
	QDF_STATUS result = QDF_STATUS_SUCCESS;
	int status;
	tPmkidCacheInfo pmk_id;

	ENTER();

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hddLog(LOGE, FL("Command not allowed in FTM mode"));
		return -EINVAL;
	}

	if (!pmksa) {
		hddLog(LOGE, FL("pmksa is NULL"));
		return -EINVAL;
	}

	if (!pmksa->bssid || !pmksa->pmkid) {
		hddLog(LOGE, FL("pmksa->bssid(%p) or pmksa->pmkid(%p) is NULL"),
		       pmksa->bssid, pmksa->pmkid);
		return -EINVAL;
	}

	hddLog(LOGW, FL("set PMKSA for " MAC_ADDRESS_STR),
		MAC_ADDR_ARRAY(pmksa->bssid));

	status = wlan_hdd_validate_context(pHddCtx);

	if (0 != status)
		return status;

	halHandle = WLAN_HDD_GET_HAL_CTX(pAdapter);

	qdf_mem_copy(pmk_id.BSSID.bytes, pmksa->bssid, QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(pmk_id.PMKID, pmksa->pmkid, CSR_RSN_PMKID_SIZE);

	/* Add to the PMKSA ID Cache in CSR */
	result = sme_roam_set_pmkid_cache(halHandle, pAdapter->sessionId,
					  &pmk_id, 1, false);

	MTRACE(qdf_trace(QDF_MODULE_ID_HDD,
			 TRACE_CODE_HDD_CFG80211_SET_PMKSA,
			 pAdapter->sessionId, result));

	EXIT();
	return QDF_IS_STATUS_SUCCESS(result) ? 0 : -EINVAL;
}

/**
 * wlan_hdd_cfg80211_set_pmksa() - set pmksa
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @pmksa: Pointer to set pmksa parameter
 *
 * Return: 0 for success, non-zero for failure
 */
static int wlan_hdd_cfg80211_set_pmksa(struct wiphy *wiphy,
				       struct net_device *dev,
				       struct cfg80211_pmksa *pmksa)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_set_pmksa(wiphy, dev, pmksa);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * __wlan_hdd_cfg80211_del_pmksa() - delete pmksa
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @pmksa: Pointer to pmksa parameter
 *
 * Return: 0 for success, non-zero for failure
 */
static int __wlan_hdd_cfg80211_del_pmksa(struct wiphy *wiphy,
					 struct net_device *dev,
					 struct cfg80211_pmksa *pmksa)
{
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
	tHalHandle halHandle;
	int status = 0;

	ENTER();

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hddLog(LOGE, FL("Command not allowed in FTM mode"));
		return -EINVAL;
	}

	if (!pmksa) {
		hddLog(LOGE, FL("pmksa is NULL"));
		return -EINVAL;
	}

	if (!pmksa->bssid) {
		hddLog(LOGE, FL("pmksa->bssid is NULL"));
		return -EINVAL;
	}

	hddLog(QDF_TRACE_LEVEL_DEBUG, FL("Deleting PMKSA for " MAC_ADDRESS_STR),
	       MAC_ADDR_ARRAY(pmksa->bssid));

	status = wlan_hdd_validate_context(pHddCtx);

	if (0 != status)
		return status;

	halHandle = WLAN_HDD_GET_HAL_CTX(pAdapter);

	MTRACE(qdf_trace(QDF_MODULE_ID_HDD,
			 TRACE_CODE_HDD_CFG80211_DEL_PMKSA,
			 pAdapter->sessionId, 0));
	/* Delete the PMKID CSR cache */
	if (QDF_STATUS_SUCCESS !=
	    sme_roam_del_pmkid_from_cache(halHandle,
					  pAdapter->sessionId, pmksa->bssid,
					  false)) {
		hddLog(LOGE, FL("Failed to delete PMKSA for " MAC_ADDRESS_STR),
		       MAC_ADDR_ARRAY(pmksa->bssid));
		status = -EINVAL;
	}
	EXIT();
	return status;
}

/**
 * wlan_hdd_cfg80211_del_pmksa() - delete pmksa
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @pmksa: Pointer to pmksa parameter
 *
 * Return: 0 for success, non-zero for failure
 */
static int wlan_hdd_cfg80211_del_pmksa(struct wiphy *wiphy,
				       struct net_device *dev,
				       struct cfg80211_pmksa *pmksa)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_del_pmksa(wiphy, dev, pmksa);
	cds_ssr_unprotect(__func__);

	return ret;

}

/**
 * __wlan_hdd_cfg80211_flush_pmksa() - flush pmksa
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 *
 * Return: 0 for success, non-zero for failure
 */
static int __wlan_hdd_cfg80211_flush_pmksa(struct wiphy *wiphy,
					   struct net_device *dev)
{
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
	tHalHandle halHandle;
	int status = 0;

	ENTER();

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hddLog(LOGE, FL("Command not allowed in FTM mode"));
		return -EINVAL;
	}

	hddLog(LOGW, FL("Flushing PMKSA"));

	pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
	status = wlan_hdd_validate_context(pHddCtx);

	if (0 != status)
		return status;

	/* Retrieve halHandle */
	halHandle = WLAN_HDD_GET_HAL_CTX(pAdapter);

	/* Flush the PMKID cache in CSR */
	if (QDF_STATUS_SUCCESS !=
	    sme_roam_del_pmkid_from_cache(halHandle, pAdapter->sessionId, NULL,
					  true)) {
		hddLog(LOGE, FL("Cannot flush PMKIDCache"));
		status = -EINVAL;
	}
	EXIT();
	return status;
}

/**
 * wlan_hdd_cfg80211_flush_pmksa() - flush pmksa
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 *
 * Return: 0 for success, non-zero for failure
 */
static int wlan_hdd_cfg80211_flush_pmksa(struct wiphy *wiphy,
					 struct net_device *dev)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_flush_pmksa(wiphy, dev);
	cds_ssr_unprotect(__func__);

	return ret;
}

#if defined(KERNEL_SUPPORT_11R_CFG80211)
/**
 * __wlan_hdd_cfg80211_update_ft_ies() - update fast transition ies
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @ftie: Pointer to fast transition ie parameter
 *
 * Return: 0 for success, non-zero for failure
 */
static int
__wlan_hdd_cfg80211_update_ft_ies(struct wiphy *wiphy,
				  struct net_device *dev,
				  struct cfg80211_update_ft_ies_params *ftie)
{
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
	int status;

	ENTER();

	status = wlan_hdd_validate_context(hdd_ctx);
	if (status)
		return status;

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hddLog(LOGE, FL("Command not allowed in FTM mode"));
		return -EINVAL;
	}

	MTRACE(qdf_trace(QDF_MODULE_ID_HDD,
			 TRACE_CODE_HDD_CFG80211_UPDATE_FT_IES,
			 pAdapter->sessionId, pHddStaCtx->conn_info.connState));
	/* Added for debug on reception of Re-assoc Req. */
	if (eConnectionState_Associated != pHddStaCtx->conn_info.connState) {
		hddLog(LOGE,
		       FL("Called with Ie of length = %zu when not associated"),
		       ftie->ie_len);
		hddLog(LOGE, FL("Should be Re-assoc Req IEs"));
	}
	hddLog(LOG1, FL("%s called with Ie of length = %zu"), __func__,
	       ftie->ie_len);

	/* Pass the received FT IEs to SME */
	sme_set_ft_ies(WLAN_HDD_GET_HAL_CTX(pAdapter), pAdapter->sessionId,
		       (const u8 *)ftie->ie, ftie->ie_len);
	EXIT();
	return 0;
}

/**
 * wlan_hdd_cfg80211_update_ft_ies() - update fast transition ies
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @ftie: Pointer to fast transition ie parameter
 *
 * Return: 0 for success, non-zero for failure
 */
static int
wlan_hdd_cfg80211_update_ft_ies(struct wiphy *wiphy,
				struct net_device *dev,
				struct cfg80211_update_ft_ies_params *ftie)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_update_ft_ies(wiphy, dev, ftie);
	cds_ssr_unprotect(__func__);

	return ret;
}
#endif

#ifdef WLAN_FEATURE_GTK_OFFLOAD
/**
 * wlan_hdd_cfg80211_update_replay_counter_callback() - replay counter callback
 * @callbackContext: Callback context
 * @pGtkOffloadGetInfoRsp: Pointer to gtk offload response parameter
 *
 * Callback rountine called upon receiving response for get offload info
 *
 * Return: none
 */
void wlan_hdd_cfg80211_update_replay_counter_callback(void *callbackContext,
						tpSirGtkOffloadGetInfoRspParams
						pGtkOffloadGetInfoRsp)
{
	hdd_adapter_t *pAdapter = (hdd_adapter_t *) callbackContext;
	uint8_t tempReplayCounter[8];
	hdd_station_ctx_t *pHddStaCtx;

	ENTER();

	if (NULL == pAdapter) {
		hddLog(LOGE, FL("HDD adapter is Null"));
		return;
	}

	if (NULL == pGtkOffloadGetInfoRsp) {
		hddLog(LOGE, FL("pGtkOffloadGetInfoRsp is Null"));
		return;
	}

	if (QDF_STATUS_SUCCESS != pGtkOffloadGetInfoRsp->ulStatus) {
		hddLog(LOGE, FL("wlan Failed to get replay counter value"));
		return;
	}

	pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
	/* Update replay counter */
	pHddStaCtx->gtkOffloadReqParams.ullKeyReplayCounter =
		pGtkOffloadGetInfoRsp->ullKeyReplayCounter;

	{
		/* changing from little to big endian since supplicant
		 * works on big endian format
		 */
		int i;
		uint8_t *p =
			(uint8_t *) &pGtkOffloadGetInfoRsp->ullKeyReplayCounter;

		for (i = 0; i < 8; i++) {
			tempReplayCounter[7 - i] = (uint8_t) p[i];
		}
	}

	/* Update replay counter to NL */
	cfg80211_gtk_rekey_notify(pAdapter->dev,
				  pGtkOffloadGetInfoRsp->bssid.bytes,
				  tempReplayCounter, GFP_KERNEL);
}

/**
 * __wlan_hdd_cfg80211_set_rekey_data() - set rekey data
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @data: Pointer to rekey data
 *
 * This function is used to offload GTK rekeying job to the firmware.
 *
 * Return: 0 for success, non-zero for failure
 */
int __wlan_hdd_cfg80211_set_rekey_data(struct wiphy *wiphy,
				       struct net_device *dev,
				       struct cfg80211_gtk_rekey_data *data)
{
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *pHddCtx = wiphy_priv(wiphy);
	hdd_station_ctx_t *pHddStaCtx;
	tHalHandle hHal;
	int result;
	tSirGtkOffloadParams hddGtkOffloadReqParams;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	ENTER();

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hddLog(LOGE, FL("Command not allowed in FTM mode"));
		return -EINVAL;
	}

	MTRACE(qdf_trace(QDF_MODULE_ID_HDD,
			 TRACE_CODE_HDD_CFG80211_SET_REKEY_DATA,
			 pAdapter->sessionId, pAdapter->device_mode));

	result = wlan_hdd_validate_context(pHddCtx);

	if (0 != result)
		return result;

	pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
	hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
	if (NULL == hHal) {
		hddLog(LOGE, FL("HAL context is Null!!!"));
		return -EAGAIN;
	}

	pHddStaCtx->gtkOffloadReqParams.ulFlags = GTK_OFFLOAD_ENABLE;
	memcpy(pHddStaCtx->gtkOffloadReqParams.aKCK, data->kck,
	       NL80211_KCK_LEN);
	memcpy(pHddStaCtx->gtkOffloadReqParams.aKEK, data->kek,
	       NL80211_KEK_LEN);
	qdf_copy_macaddr(&pHddStaCtx->gtkOffloadReqParams.bssid,
			 &pHddStaCtx->conn_info.bssId);
	{
		/* changing from big to little endian since driver
		 * works on little endian format
		 */
		uint8_t *p =
			(uint8_t *) &pHddStaCtx->gtkOffloadReqParams.
			ullKeyReplayCounter;
		int i;

		for (i = 0; i < 8; i++) {
			p[7 - i] = data->replay_ctr[i];
		}
	}

	if (true == pHddCtx->hdd_wlan_suspended) {
		/* if wlan is suspended, enable GTK offload directly from here */
		memcpy(&hddGtkOffloadReqParams,
		       &pHddStaCtx->gtkOffloadReqParams,
		       sizeof(tSirGtkOffloadParams));
		status =
			sme_set_gtk_offload(hHal, &hddGtkOffloadReqParams,
					    pAdapter->sessionId);

		if (QDF_STATUS_SUCCESS != status) {
			hddLog(LOGE, FL("sme_set_gtk_offload failed, status(%d)"),
			       status);
			return -EINVAL;
		}
		hddLog(LOG1, FL("sme_set_gtk_offload successful"));
	} else {
		hddLog(LOG1,
		       FL("wlan not suspended GTKOffload request is stored"));
	}
	EXIT();
	return result;
}

/**
 * wlan_hdd_cfg80211_set_rekey_data() - set rekey data
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @data: Pointer to rekey data
 *
 * This function is used to offload GTK rekeying job to the firmware.
 *
 * Return: 0 for success, non-zero for failure
 */
int wlan_hdd_cfg80211_set_rekey_data(struct wiphy *wiphy,
				     struct net_device *dev,
				     struct cfg80211_gtk_rekey_data *data)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_set_rekey_data(wiphy, dev, data);
	cds_ssr_unprotect(__func__);

	return ret;
}
#endif /*WLAN_FEATURE_GTK_OFFLOAD */

/**
 * __wlan_hdd_cfg80211_set_mac_acl() - set access control policy
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @param: Pointer to access control parameter
 *
 * Return: 0 for success, non-zero for failure
 */
static int __wlan_hdd_cfg80211_set_mac_acl(struct wiphy *wiphy,
					 struct net_device *dev,
					 const struct cfg80211_acl_data *params)
{
	int i;
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_hostapd_state_t *pHostapdState;
	tsap_Config_t *pConfig;
	v_CONTEXT_t p_cds_context = NULL;
	hdd_context_t *pHddCtx;
	int status;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

	ENTER();

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hddLog(LOGE, FL("Command not allowed in FTM mode"));
		return -EINVAL;
	}

	if (NULL == params) {
		hddLog(LOGE, FL("params is Null"));
		return -EINVAL;
	}

	pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
	status = wlan_hdd_validate_context(pHddCtx);

	if (0 != status)
		return status;

	p_cds_context = pHddCtx->pcds_context;
	pHostapdState = WLAN_HDD_GET_HOSTAP_STATE_PTR(pAdapter);

	if (NULL == pHostapdState) {
		hddLog(LOGE, FL("pHostapdState is Null"));
		return -EINVAL;
	}

	hddLog(LOGE, "acl policy: = %d no acl entries = %d", params->acl_policy,
		params->n_acl_entries);

	MTRACE(qdf_trace(QDF_MODULE_ID_HDD,
			 TRACE_CODE_HDD_CFG80211_SET_MAC_ACL,
			 pAdapter->sessionId, pAdapter->device_mode));
	if (QDF_SAP_MODE == pAdapter->device_mode) {
		pConfig = &pAdapter->sessionCtx.ap.sapConfig;

		/* default value */
		pConfig->num_accept_mac = 0;
		pConfig->num_deny_mac = 0;

		/**
		 * access control policy
		 * @NL80211_ACL_POLICY_ACCEPT_UNLESS_LISTED: Deny stations which are
		 *   listed in hostapd.deny file.
		 * @NL80211_ACL_POLICY_DENY_UNLESS_LISTED: Allow stations which are
		 *   listed in hostapd.accept file.
		 */
		if (NL80211_ACL_POLICY_DENY_UNLESS_LISTED == params->acl_policy) {
			pConfig->SapMacaddr_acl = eSAP_DENY_UNLESS_ACCEPTED;
		} else if (NL80211_ACL_POLICY_ACCEPT_UNLESS_LISTED ==
			   params->acl_policy) {
			pConfig->SapMacaddr_acl = eSAP_ACCEPT_UNLESS_DENIED;
		} else {
			hddLog(LOGE, FL("Acl Policy : %d is not supported"),
				params->acl_policy);
			return -ENOTSUPP;
		}

		if (eSAP_DENY_UNLESS_ACCEPTED == pConfig->SapMacaddr_acl) {
			pConfig->num_accept_mac = params->n_acl_entries;
			for (i = 0; i < params->n_acl_entries; i++) {
				hddLog(LOG1,
					FL("** Add ACL MAC entry %i in WhiletList :"
					MAC_ADDRESS_STR), i,
					MAC_ADDR_ARRAY(
						params->mac_addrs[i].addr));

				qdf_mem_copy(&pConfig->accept_mac[i],
					     params->mac_addrs[i].addr,
					     sizeof(qcmacaddr));
			}
		} else if (eSAP_ACCEPT_UNLESS_DENIED == pConfig->SapMacaddr_acl) {
			pConfig->num_deny_mac = params->n_acl_entries;
			for (i = 0; i < params->n_acl_entries; i++) {
				hddLog(LOG1,
					FL("** Add ACL MAC entry %i in BlackList :"
					MAC_ADDRESS_STR), i,
					MAC_ADDR_ARRAY(
						params->mac_addrs[i].addr));

				qdf_mem_copy(&pConfig->deny_mac[i],
					     params->mac_addrs[i].addr,
					     sizeof(qcmacaddr));
			}
		}
#ifdef WLAN_FEATURE_MBSSID
		qdf_status =
			wlansap_set_mac_acl(WLAN_HDD_GET_SAP_CTX_PTR(pAdapter),
					    pConfig);
#else
		qdf_status = wlansap_set_mac_acl(p_cds_context, pConfig);
#endif
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			hddLog(LOGE, FL("SAP Set Mac Acl fail"));
			return -EINVAL;
		}
	} else {
		hddLog(LOG1, FL("Invalid device_mode %s(%d)"),
			hdd_device_mode_to_string(pAdapter->device_mode),
			pAdapter->device_mode);
		return -EINVAL;
	}
	EXIT();
	return 0;
}

/**
 * wlan_hdd_cfg80211_set_mac_acl() - SSR wrapper for
 *				__wlan_hdd_cfg80211_set_mac_acl
 * @wiphy: pointer to wiphy structure
 * @dev: pointer to net_device
 * @params: pointer to cfg80211_acl_data
 *
 * Return; 0 on success, error number otherwise
 */
static int
wlan_hdd_cfg80211_set_mac_acl(struct wiphy *wiphy,
			      struct net_device *dev,
			      const struct cfg80211_acl_data *params)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_set_mac_acl(wiphy, dev, params);
	cds_ssr_unprotect(__func__);

	return ret;
}

#ifdef WLAN_NL80211_TESTMODE
#ifdef FEATURE_WLAN_LPHB
/**
 * wlan_hdd_cfg80211_lphb_ind_handler() - handle low power heart beat indication
 * @pHddCtx: Pointer to hdd context
 * @lphbInd: Pointer to low power heart beat indication parameter
 *
 * Return: none
 */
void wlan_hdd_cfg80211_lphb_ind_handler(void *pHddCtx, tSirLPHBInd *lphbInd)
{
	struct sk_buff *skb;

	hddLog(LOGE, FL("LPHB indication arrived"));

	if (0 != wlan_hdd_validate_context((hdd_context_t *) pHddCtx))
		return;

	if (NULL == lphbInd) {
		hddLog(LOGE, FL("invalid argument lphbInd"));
		return;
	}

	skb = cfg80211_testmode_alloc_event_skb(((hdd_context_t *) pHddCtx)->
						wiphy, sizeof(tSirLPHBInd),
						GFP_ATOMIC);
	if (!skb) {
		hddLog(LOGE, FL("LPHB timeout, NL buffer alloc fail"));
		return;
	}

	if (nla_put_u32(skb, WLAN_HDD_TM_ATTR_CMD, WLAN_HDD_TM_CMD_WLAN_HB)) {
		hddLog(LOGE, FL("WLAN_HDD_TM_ATTR_CMD put fail"));
		goto nla_put_failure;
	}
	if (nla_put_u32(skb, WLAN_HDD_TM_ATTR_TYPE, lphbInd->protocolType)) {
		hddLog(LOGE, FL("WLAN_HDD_TM_ATTR_TYPE put fail"));
		goto nla_put_failure;
	}
	if (nla_put(skb, WLAN_HDD_TM_ATTR_DATA, sizeof(tSirLPHBInd), lphbInd)) {
		hddLog(LOGE, FL("WLAN_HDD_TM_ATTR_DATA put fail"));
		goto nla_put_failure;
	}
	cfg80211_testmode_event(skb, GFP_ATOMIC);
	return;

nla_put_failure:
	hddLog(LOGE, FL("NLA Put fail"));
	kfree_skb(skb);

	return;
}
#endif /* FEATURE_WLAN_LPHB */

/**
 * __wlan_hdd_cfg80211_testmode() - test mode
 * @wiphy: Pointer to wiphy
 * @data: Data pointer
 * @len: Data length
 *
 * Return: 0 for success, non-zero for failure
 */
static int __wlan_hdd_cfg80211_testmode(struct wiphy *wiphy,
					void *data, int len)
{
	struct nlattr *tb[WLAN_HDD_TM_ATTR_MAX + 1];
	int err;
	hdd_context_t *pHddCtx = wiphy_priv(wiphy);

	ENTER();

	err = wlan_hdd_validate_context(pHddCtx);
	if (err)
		return err;

	err = nla_parse(tb, WLAN_HDD_TM_ATTR_MAX, data,
			len, wlan_hdd_tm_policy);
	if (err) {
		hddLog(LOGE, FL("Testmode INV ATTR"));
		return err;
	}

	if (!tb[WLAN_HDD_TM_ATTR_CMD]) {
		hddLog(LOGE, FL("Testmode INV CMD"));
		return -EINVAL;
	}

	MTRACE(qdf_trace(QDF_MODULE_ID_HDD,
			 TRACE_CODE_HDD_CFG80211_TESTMODE,
			 NO_SESSION, nla_get_u32(tb[WLAN_HDD_TM_ATTR_CMD])));
	switch (nla_get_u32(tb[WLAN_HDD_TM_ATTR_CMD])) {
#ifdef FEATURE_WLAN_LPHB
	/* Low Power Heartbeat configuration request */
	case WLAN_HDD_TM_CMD_WLAN_HB:
	{
		int buf_len;
		void *buf;
		tSirLPHBReq *hb_params = NULL;
		tSirLPHBReq *hb_params_temp = NULL;
		QDF_STATUS smeStatus;

		if (!tb[WLAN_HDD_TM_ATTR_DATA]) {
			hddLog(LOGE, FL("Testmode INV DATA"));
			return -EINVAL;
		}

		buf = nla_data(tb[WLAN_HDD_TM_ATTR_DATA]);
		buf_len = nla_len(tb[WLAN_HDD_TM_ATTR_DATA]);

		hb_params_temp = (tSirLPHBReq *) buf;
		if ((hb_params_temp->cmd == LPHB_SET_TCP_PARAMS_INDID)
		    && (hb_params_temp->params.lphbTcpParamReq.
			timePeriodSec == 0))
			return -EINVAL;

		hb_params =
			(tSirLPHBReq *) qdf_mem_malloc(sizeof(tSirLPHBReq));
		if (NULL == hb_params) {
			hddLog(LOGE, FL("Request Buffer Alloc Fail"));
			return -ENOMEM;
		}

		qdf_mem_copy(hb_params, buf, buf_len);
		smeStatus =
			sme_lphb_config_req((tHalHandle) (pHddCtx->hHal),
					    hb_params,
					    wlan_hdd_cfg80211_lphb_ind_handler);
		if (QDF_STATUS_SUCCESS != smeStatus) {
			hddLog(LOGE, "LPHB Config Fail, disable");
			qdf_mem_free(hb_params);
		}
		return 0;
	}
#endif /* FEATURE_WLAN_LPHB */

#if  defined(QCA_WIFI_FTM)
	case WLAN_HDD_TM_CMD_WLAN_FTM:
	{
		int buf_len;
		void *buf;
		QDF_STATUS status;
		if (!tb[WLAN_HDD_TM_ATTR_DATA]) {
			hdd_err("WLAN_HDD_TM_ATTR_DATA attribute is invalid");
			return -EINVAL;
		}

		buf = nla_data(tb[WLAN_HDD_TM_ATTR_DATA]);
		buf_len = nla_len(tb[WLAN_HDD_TM_ATTR_DATA]);

		hdd_info("****FTM Tx cmd len = %d*****", buf_len);

		status = wlan_hdd_ftm_testmode_cmd(buf, buf_len);

		if (status != QDF_STATUS_SUCCESS)
			err = -EBUSY;
		break;
	}
#endif

	default:
		hddLog(LOGE, FL("command %d not supported"),
		       nla_get_u32(tb[WLAN_HDD_TM_ATTR_CMD]));
		return -EOPNOTSUPP;
	}
	EXIT();
	return err;
}

/**
 * wlan_hdd_cfg80211_testmode() - test mode
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @data: Data pointer
 * @len: Data length
 *
 * Return: 0 for success, non-zero for failure
 */
static int wlan_hdd_cfg80211_testmode(struct wiphy *wiphy,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 12, 0))
				      struct wireless_dev *wdev,
#endif
				      void *data, int len)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_testmode(wiphy, data, len);
	cds_ssr_unprotect(__func__);

	return ret;
}

#if  defined(QCA_WIFI_FTM)
/**
 * wlan_hdd_testmode_rx_event() - test mode rx event handler
 * @buf: Pointer to buffer
 * @buf_len: Buffer length
 *
 * Return: none
 */
void wlan_hdd_testmode_rx_event(void *buf, size_t buf_len)
{
	struct sk_buff *skb;
	hdd_context_t *hdd_ctx;

	if (!buf || !buf_len) {
		QDF_TRACE(QDF_MODULE_ID_HDD, QDF_TRACE_LEVEL_ERROR,
			  "%s: buf or buf_len invalid, buf = %p buf_len = %zu",
			  __func__, buf, buf_len);
		return;
	}

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		QDF_TRACE(QDF_MODULE_ID_HDD, QDF_TRACE_LEVEL_ERROR,
			  "%s: hdd context invalid", __func__);
		return;
	}

	skb = cfg80211_testmode_alloc_event_skb(hdd_ctx->wiphy,
						buf_len, GFP_KERNEL);
	if (!skb) {
		QDF_TRACE(QDF_MODULE_ID_HDD, QDF_TRACE_LEVEL_ERROR,
			  "%s: failed to allocate testmode rx skb!", __func__);
		return;
	}

	if (nla_put_u32(skb, WLAN_HDD_TM_ATTR_CMD, WLAN_HDD_TM_CMD_WLAN_FTM) ||
	    nla_put(skb, WLAN_HDD_TM_ATTR_DATA, buf_len, buf))
		goto nla_put_failure;

	hdd_info("****FTM Rx cmd len = %zu*****", buf_len);

	cfg80211_testmode_event(skb, GFP_KERNEL);
	return;

nla_put_failure:
	kfree_skb(skb);
	QDF_TRACE(QDF_MODULE_ID_HDD, QDF_TRACE_LEVEL_ERROR,
		  "%s: nla_put failed on testmode rx skb!", __func__);
}
#endif
#endif /* CONFIG_NL80211_TESTMODE */

#ifdef QCA_HT_2040_COEX
/**
 * __wlan_hdd_cfg80211_set_ap_channel_width() - set ap channel bandwidth
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @chandef: Pointer to channel definition parameter
 *
 * Return: 0 for success, non-zero for failure
 */
static int
__wlan_hdd_cfg80211_set_ap_channel_width(struct wiphy *wiphy,
					 struct net_device *dev,
					 struct cfg80211_chan_def *chandef)
{
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *pHddCtx;
	QDF_STATUS status;
	tSmeConfigParams sme_config;
	bool cbModeChange;

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hddLog(LOGE, FL("Command not allowed in FTM mode"));
		return -EINVAL;
	}

	pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
	status = wlan_hdd_validate_context(pHddCtx);
	if (status)
		return status;

	qdf_mem_zero(&sme_config, sizeof(tSmeConfigParams));
	sme_get_config_param(pHddCtx->hHal, &sme_config);
	switch (chandef->width) {
	case NL80211_CHAN_WIDTH_20:
		if (sme_config.csrConfig.channelBondingMode24GHz !=
		    eCSR_INI_SINGLE_CHANNEL_CENTERED) {
			sme_config.csrConfig.channelBondingMode24GHz =
				eCSR_INI_SINGLE_CHANNEL_CENTERED;
			sme_update_config(pHddCtx->hHal, &sme_config);
			cbModeChange = true;
		}
		break;

	case NL80211_CHAN_WIDTH_40:
		if (sme_config.csrConfig.channelBondingMode24GHz ==
		    eCSR_INI_SINGLE_CHANNEL_CENTERED) {
			if (NL80211_CHAN_HT40MINUS ==
			    cfg80211_get_chandef_type(chandef))
				sme_config.csrConfig.channelBondingMode24GHz =
					eCSR_INI_DOUBLE_CHANNEL_HIGH_PRIMARY;
			else
				sme_config.csrConfig.channelBondingMode24GHz =
					eCSR_INI_DOUBLE_CHANNEL_LOW_PRIMARY;
			sme_update_config(pHddCtx->hHal, &sme_config);
			cbModeChange = true;
		}
		break;

	default:
		hddLog(LOGE, FL("Error!!! Invalid HT20/40 mode !"));
		return -EINVAL;
	}

	if (!cbModeChange)
		return 0;

	if (QDF_SAP_MODE != pAdapter->device_mode)
		return 0;

	hddLog(LOG1, FL("Channel bonding changed to %d"),
	       sme_config.csrConfig.channelBondingMode24GHz);

	/* Change SAP ht2040 mode */
	status = hdd_set_sap_ht2040_mode(pAdapter,
					 cfg80211_get_chandef_type(chandef));
	if (status != QDF_STATUS_SUCCESS) {
		hddLog(LOGE, FL("Error!!! Cannot set SAP HT20/40 mode!"));
		return -EINVAL;
	}

	return 0;
}

/**
 * wlan_hdd_cfg80211_set_ap_channel_width() - set ap channel bandwidth
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @chandef: Pointer to channel definition parameter
 *
 * Return: 0 for success, non-zero for failure
 */
static int
wlan_hdd_cfg80211_set_ap_channel_width(struct wiphy *wiphy,
				       struct net_device *dev,
				       struct cfg80211_chan_def *chandef)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_set_ap_channel_width(wiphy, dev, chandef);
	cds_ssr_unprotect(__func__);

	return ret;
}
#endif

#ifdef CHANNEL_SWITCH_SUPPORTED
/**
 * __wlan_hdd_cfg80211_channel_switch()- function to switch
 * channel in SAP/GO
 * @wiphy:  wiphy pointer
 * @dev: dev pointer.
 * @csa_params: Change channel params
 *
 * This function is called to switch channel in SAP/GO
 *
 * Return: 0 if success else return non zero
 */
static int __wlan_hdd_cfg80211_channel_switch(struct wiphy *wiphy,
				struct net_device *dev,
				struct cfg80211_csa_settings *csa_params)
{
	hdd_adapter_t *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *hdd_ctx;
	uint8_t channel;
	uint16_t freq;
	int ret;
	enum phy_ch_width ch_width;

	hddLog(LOG1, FL("Set Freq %d"),
		  csa_params->chandef.chan->center_freq);

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);

	if (0 != ret)
		return ret;

	if ((QDF_P2P_GO_MODE != adapter->device_mode) &&
		(QDF_SAP_MODE != adapter->device_mode))
		return -ENOTSUPP;

	freq = csa_params->chandef.chan->center_freq;
	channel = cds_freq_to_chan(freq);

	ch_width = hdd_map_nl_chan_width(csa_params->chandef.width);

	ret = hdd_softap_set_channel_change(dev, channel, ch_width);
	return ret;
}

/**
 * wlan_hdd_cfg80211_channel_switch()- function to switch
 * channel in SAP/GO
 * @wiphy:  wiphy pointer
 * @dev: dev pointer.
 * @csa_params: Change channel params
 *
 * This function is called to switch channel in SAP/GO
 *
 * Return: 0 if success else return non zero
 */
static int wlan_hdd_cfg80211_channel_switch(struct wiphy *wiphy,
				struct net_device *dev,
				struct cfg80211_csa_settings *csa_params)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_channel_switch(wiphy, dev, csa_params);
	cds_ssr_unprotect(__func__);
	return ret;
}
#endif

/**
 * wlan_hdd_convert_nl_iftype_to_hdd_type() - provides the type
 * translation from NL to policy manager type
 * @type: Generic connection mode type defined in NL
 *
 *
 * This function provides the type translation
 *
 * Return: cds_con_mode enum
 */
enum cds_con_mode wlan_hdd_convert_nl_iftype_to_hdd_type(
						enum nl80211_iftype type)
{
	enum cds_con_mode mode = CDS_MAX_NUM_OF_MODE;
	switch (type) {
	case NL80211_IFTYPE_STATION:
		mode = CDS_STA_MODE;
		break;
	case NL80211_IFTYPE_P2P_CLIENT:
		mode = CDS_P2P_CLIENT_MODE;
		break;
	case NL80211_IFTYPE_P2P_GO:
		mode = CDS_P2P_GO_MODE;
		break;
	case NL80211_IFTYPE_AP:
		mode = CDS_SAP_MODE;
		break;
	case NL80211_IFTYPE_ADHOC:
		mode = CDS_IBSS_MODE;
		break;
	default:
		hddLog(LOGE, FL("Unsupported interface type (%d)"),
			   type);
	}
	return mode;
}

/**
 * wlan_hdd_cfg80211_set_mon_ch() - Set monitor mode capture channel
 * @wiphy: Handle to struct wiphy to get handle to module context.
 * @chandef: Contains information about the capture channel to be set.
 *
 * This interface is called if and only if monitor mode interface alone is
 * active.
 *
 * Return: 0 success or error code on failure.
 */
static int __wlan_hdd_cfg80211_set_mon_ch(struct wiphy *wiphy,
				       struct cfg80211_chan_def *chandef)
{
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	hdd_adapter_t *adapter;
	hdd_station_ctx_t *sta_ctx;
	struct hdd_mon_set_ch_info *ch_info;
	QDF_STATUS status;
	tHalHandle hal_hdl;
	struct qdf_mac_addr bssid;
	tCsrRoamProfile roam_profile;
	struct ch_params_s ch_params;
	uint8_t sec_ch = 0;
	int ret;
	uint16_t chan_num = cds_freq_to_chan(chandef->chan->center_freq);

	ENTER();

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	hal_hdl = hdd_ctx->hHal;

	adapter = hdd_get_adapter(hdd_ctx, QDF_MONITOR_MODE);
	if (!adapter)
		return -EIO;

	hdd_info("%s: set monitor mode Channel %d and freq %d",
		 adapter->dev->name, chan_num, chandef->chan->center_freq);

	sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	ch_info = &sta_ctx->ch_info;
	hdd_select_cbmode(adapter, chan_num);
	roam_profile.ChannelInfo.ChannelList = &ch_info->channel;
	roam_profile.ChannelInfo.numOfChannels = 1;
	roam_profile.phyMode = ch_info->phy_mode;
	roam_profile.ch_params.ch_width = chandef->width;

	qdf_mem_copy(bssid.bytes, adapter->macAddressCurrent.bytes,
		     QDF_MAC_ADDR_SIZE);

	ch_params.ch_width = chandef->width;
	/*
	 * CDS api expects secondary channel for calculating
	 * the channel params
	 */
	if ((ch_params.ch_width == CH_WIDTH_40MHZ) &&
	    (CDS_IS_CHANNEL_24GHZ(chan_num))) {
		if (chan_num >= 1 && chan_num <= 5)
			sec_ch = chan_num + 4;
		else if (chan_num >= 6 && chan_num <= 13)
			sec_ch = chan_num - 4;
	}
	cds_set_channel_params(chan_num, sec_ch, &ch_params);
	status = sme_roam_channel_change_req(hal_hdl, bssid, &ch_params,
						 &roam_profile);
	if (status) {
		hdd_err("Status: %d Failed to set sme_RoamChannel for monitor mode",
			status);
		ret = qdf_status_to_os_return(status);
		return ret;
	}
	EXIT();
	return 0;
}

/**
 * wlan_hdd_cfg80211_set_mon_ch() - Set monitor mode capture channel
 * @wiphy: Handle to struct wiphy to get handle to module context.
 * @chandef: Contains information about the capture channel to be set.
 *
 * This interface is called if and only if monitor mode interface alone is
 * active.
 *
 * Return: 0 success or error code on failure.
 */
static int wlan_hdd_cfg80211_set_mon_ch(struct wiphy *wiphy,
				       struct cfg80211_chan_def *chandef)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_set_mon_ch(wiphy, chandef);
	cds_ssr_unprotect(__func__);
	return ret;
}

/**
 * struct cfg80211_ops - cfg80211_ops
 *
 * @add_virtual_intf: Add virtual interface
 * @del_virtual_intf: Delete virtual interface
 * @change_virtual_intf: Change virtual interface
 * @change_station: Change station
 * @add_beacon: Add beacon in sap mode
 * @del_beacon: Delete beacon in sap mode
 * @set_beacon: Set beacon in sap mode
 * @start_ap: Start ap
 * @change_beacon: Change beacon
 * @stop_ap: Stop ap
 * @change_bss: Change bss
 * @add_key: Add key
 * @get_key: Get key
 * @del_key: Delete key
 * @set_default_key: Set default key
 * @set_channel: Set channel
 * @scan: Scan
 * @connect: Connect
 * @disconnect: Disconnect
 * @join_ibss = Join ibss
 * @leave_ibss = Leave ibss
 * @set_wiphy_params = Set wiphy params
 * @set_tx_power = Set tx power
 * @get_tx_power = get tx power
 * @remain_on_channel = Remain on channel
 * @cancel_remain_on_channel = Cancel remain on channel
 * @mgmt_tx = Tx management frame
 * @mgmt_tx_cancel_wait = Cancel management tx wait
 * @set_default_mgmt_key = Set default management key
 * @set_txq_params = Set tx queue parameters
 * @get_station = Get station
 * @set_power_mgmt = Set power management
 * @del_station = Delete station
 * @add_station = Add station
 * @set_pmksa = Set pmksa
 * @del_pmksa = Delete pmksa
 * @flush_pmksa = Flush pmksa
 * @update_ft_ies = Update FT IEs
 * @tdls_mgmt = Tdls management
 * @tdls_oper = Tdls operation
 * @set_rekey_data = Set rekey data
 * @sched_scan_start = Scheduled scan start
 * @sched_scan_stop = Scheduled scan stop
 * @resume = Resume wlan
 * @suspend = Suspend wlan
 * @set_mac_acl = Set mac acl
 * @testmode_cmd = Test mode command
 * @set_ap_chanwidth = Set AP channel bandwidth
 * @dump_survey = Dump survey
 * @key_mgmt_set_pmk = Set pmk key management
 */
static struct cfg80211_ops wlan_hdd_cfg80211_ops = {
	.add_virtual_intf = wlan_hdd_add_virtual_intf,
	.del_virtual_intf = wlan_hdd_del_virtual_intf,
	.change_virtual_intf = wlan_hdd_cfg80211_change_iface,
	.change_station = wlan_hdd_change_station,
	.start_ap = wlan_hdd_cfg80211_start_ap,
	.change_beacon = wlan_hdd_cfg80211_change_beacon,
	.stop_ap = wlan_hdd_cfg80211_stop_ap,
	.change_bss = wlan_hdd_cfg80211_change_bss,
	.add_key = wlan_hdd_cfg80211_add_key,
	.get_key = wlan_hdd_cfg80211_get_key,
	.del_key = wlan_hdd_cfg80211_del_key,
	.set_default_key = wlan_hdd_cfg80211_set_default_key,
	.scan = wlan_hdd_cfg80211_scan,
	.connect = wlan_hdd_cfg80211_connect,
	.disconnect = wlan_hdd_cfg80211_disconnect,
	.join_ibss = wlan_hdd_cfg80211_join_ibss,
	.leave_ibss = wlan_hdd_cfg80211_leave_ibss,
	.set_wiphy_params = wlan_hdd_cfg80211_set_wiphy_params,
	.set_tx_power = wlan_hdd_cfg80211_set_txpower,
	.get_tx_power = wlan_hdd_cfg80211_get_txpower,
	.remain_on_channel = wlan_hdd_cfg80211_remain_on_channel,
	.cancel_remain_on_channel = wlan_hdd_cfg80211_cancel_remain_on_channel,
	.mgmt_tx = wlan_hdd_mgmt_tx,
	.mgmt_tx_cancel_wait = wlan_hdd_cfg80211_mgmt_tx_cancel_wait,
	.set_default_mgmt_key = wlan_hdd_set_default_mgmt_key,
	.set_txq_params = wlan_hdd_set_txq_params,
	.get_station = wlan_hdd_cfg80211_get_station,
	.set_power_mgmt = wlan_hdd_cfg80211_set_power_mgmt,
	.del_station = wlan_hdd_cfg80211_del_station,
	.add_station = wlan_hdd_cfg80211_add_station,
	.set_pmksa = wlan_hdd_cfg80211_set_pmksa,
	.del_pmksa = wlan_hdd_cfg80211_del_pmksa,
	.flush_pmksa = wlan_hdd_cfg80211_flush_pmksa,
#if defined(KERNEL_SUPPORT_11R_CFG80211)
	.update_ft_ies = wlan_hdd_cfg80211_update_ft_ies,
#endif
#ifdef FEATURE_WLAN_TDLS
	.tdls_mgmt = wlan_hdd_cfg80211_tdls_mgmt,
	.tdls_oper = wlan_hdd_cfg80211_tdls_oper,
#endif
#ifdef WLAN_FEATURE_GTK_OFFLOAD
	.set_rekey_data = wlan_hdd_cfg80211_set_rekey_data,
#endif /* WLAN_FEATURE_GTK_OFFLOAD */
#ifdef FEATURE_WLAN_SCAN_PNO
	.sched_scan_start = wlan_hdd_cfg80211_sched_scan_start,
	.sched_scan_stop = wlan_hdd_cfg80211_sched_scan_stop,
#endif /*FEATURE_WLAN_SCAN_PNO */
	.resume = wlan_hdd_cfg80211_resume_wlan,
	.suspend = wlan_hdd_cfg80211_suspend_wlan,
	.set_mac_acl = wlan_hdd_cfg80211_set_mac_acl,
#ifdef WLAN_NL80211_TESTMODE
	.testmode_cmd = wlan_hdd_cfg80211_testmode,
#endif
#ifdef QCA_HT_2040_COEX
	.set_ap_chanwidth = wlan_hdd_cfg80211_set_ap_channel_width,
#endif
	.dump_survey = wlan_hdd_cfg80211_dump_survey,
#ifdef CHANNEL_SWITCH_SUPPORTED
	.channel_switch = wlan_hdd_cfg80211_channel_switch,
#endif
	.set_monitor_channel = wlan_hdd_cfg80211_set_mon_ch,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)) || \
    defined(CFG80211_ABORT_SCAN)
	.abort_scan = wlan_hdd_cfg80211_abort_scan,
#endif
};
