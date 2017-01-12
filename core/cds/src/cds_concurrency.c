/*
 * Copyright (c) 2012-2017 The Linux Foundation. All rights reserved.
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
 * DOC: cds_concurrency.c
 *
 * WLAN Concurrenct Connection Management functions
 *
 */

/* Include files */

#include <cds_api.h>
#include <cds_sched.h>
#include <linux/etherdevice.h>
#include <linux/firmware.h>
#include <wlan_hdd_tx_rx.h>
#include <wni_api.h>
#include "wlan_hdd_trace.h"
#include "wlan_hdd_hostapd.h"
#include "cds_concurrency.h"
#include "cds_concurrency_1x1_dbs.h"
#include "cds_concurrency_2x2_dbs.h"
#include "cds_concurrency_no_dbs.h"
#include "qdf_types.h"
#include "qdf_trace.h"

#include <net/addrconf.h>
#include <linux/wireless.h>
#include <net/cfg80211.h>
#include <linux/inetdevice.h>
#include <net/addrconf.h>
#include <linux/rtnetlink.h>
#include "sap_api.h"
#include <linux/semaphore.h>
#include <linux/ctype.h>
#include <linux/compat.h>
#include "cfg_api.h"
#include "qwlan_version.h"
#include "wma_types.h"
#include "wma.h"
#include "wma_api.h"
#include "cds_utils.h"
#include "cds_reg_service.h"
#include "wlan_hdd_ipa.h"
#include "cdp_txrx_flow_ctrl_legacy.h"
#include "pld_common.h"
#include "wlan_hdd_green_ap.h"

static struct cds_conc_connection_info
	conc_connection_list[MAX_NUMBER_OF_CONC_CONNECTIONS];

#define CONC_CONNECTION_LIST_VALID_INDEX(index) \
		((MAX_NUMBER_OF_CONC_CONNECTIONS > index) && \
			(conc_connection_list[index].in_use))

#define CDS_MAX_CON_STRING_LEN   100
/**
 * first_connection_pcl_table - table which provides PCL for the
 * very first connection in the system
 */
static const enum cds_pcl_type
first_connection_pcl_table[CDS_MAX_NUM_OF_MODE]
			[CDS_MAX_CONC_PRIORITY_MODE] = {
	[CDS_STA_MODE] = {CDS_NONE, CDS_NONE, CDS_NONE},
	[CDS_SAP_MODE] = {CDS_5G,   CDS_5G,   CDS_5G  },
	[CDS_P2P_CLIENT_MODE] = {CDS_5G,   CDS_5G,   CDS_5G  },
	[CDS_P2P_GO_MODE] = {CDS_5G,   CDS_5G,   CDS_5G  },
	[CDS_IBSS_MODE] = {CDS_NONE, CDS_NONE, CDS_NONE},
};

static dbs_pcl_second_connection_table_type *second_connection_pcl_dbs_table;
static dbs_pcl_third_connection_table_type *third_connection_pcl_dbs_table;
static next_action_two_connection_table_type *next_action_two_connection_table;
static next_action_three_connection_table_type
					*next_action_three_connection_table;
enum cds_conc_next_action (*cds_get_current_pref_hw_mode_ptr)(void);

static enum cds_conc_next_action cds_get_current_pref_hw_mode_dbs_2x2(void);
static enum cds_conc_next_action cds_get_current_pref_hw_mode_dbs_1x1(void);

/**
 * cds_get_connection_count() - provides the count of
 * current connections
 *
 * This function provides the count of current connections
 *
 * Return: connection count
 */
uint32_t cds_get_connection_count(void)
{
	uint32_t conn_index, count = 0;
	cds_context_type *cds_ctx;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return count;
	}

	qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
		conn_index++) {
		if (conc_connection_list[conn_index].in_use)
			count++;
	}
	qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);

	return count;
}

/**
 * cds_is_sta_connection_pending() - This function will check if sta connection
 *                                   is pending or not.
 *
 * This function will return the status of flag is_sta_connection_pending
 *
 * Return: true or false
 */
bool cds_is_sta_connection_pending(void)
{
	bool status;
	hdd_context_t *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return false;
	}

	qdf_spin_lock(&hdd_ctx->sta_update_info_lock);
	status = hdd_ctx->is_sta_connection_pending;
	qdf_spin_unlock(&hdd_ctx->sta_update_info_lock);
	return status;
}

/**
 * cds_change_sta_conn_pending_status() - This function will change the value
 *                                        of is_sta_connection_pending
 * @value: value to set
 *
 * This function will change the value of is_sta_connection_pending
 *
 * Return: none
 */
void cds_change_sta_conn_pending_status(bool value)
{
	hdd_context_t *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return;
	}

	qdf_spin_lock(&hdd_ctx->sta_update_info_lock);
	hdd_ctx->is_sta_connection_pending = value;
	qdf_spin_unlock(&hdd_ctx->sta_update_info_lock);
}

/**
 * cds_is_sap_restart_required() - This function will check if sap restart
 *                                 is pending or not.
 *
 * This function will return the status of flag is_sap_restart_required.
 *
 * Return: true or false
 */
static bool cds_is_sap_restart_required(void)
{
	bool status;
	hdd_context_t *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return false;
	}

	qdf_spin_lock(&hdd_ctx->sap_update_info_lock);
	status = hdd_ctx->is_sap_restart_required;
	qdf_spin_unlock(&hdd_ctx->sap_update_info_lock);
	return status;
}

/**
 * cds_change_sap_restart_required_status() - This function will change the
 *                                            value of is_sap_restart_required
 * @value: value to set
 *
 * This function will change the value of is_sap_restart_required
 *
 * Return: none
 */
void cds_change_sap_restart_required_status(bool value)
{
	hdd_context_t *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return;
	}

	qdf_spin_lock(&hdd_ctx->sap_update_info_lock);
	hdd_ctx->is_sap_restart_required = value;
	qdf_spin_unlock(&hdd_ctx->sap_update_info_lock);
}

/**
 * cds_set_connection_in_progress() - to set the connection in progress flag
 * @value: value to set
 *
 * This function will set the passed value to connection in progress flag.
 * If value is previously being set to true then no need to set it again.
 *
 * Return: true if value is being set correctly and false otherwise.
 */
bool cds_set_connection_in_progress(bool value)
{
	bool status = true;
	hdd_context_t *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return false;
	}

	qdf_spin_lock(&hdd_ctx->connection_status_lock);
	/*
	 * if the value is set to true previously and if someone is
	 * trying to make it true again then it could be some race
	 * condition being triggered. Avoid this situation by returning
	 * false
	 */
	if (hdd_ctx->connection_in_progress && value)
		status = false;
	else
		hdd_ctx->connection_in_progress = value;
	qdf_spin_unlock(&hdd_ctx->connection_status_lock);
	return status;
}

/**
 * cds_update_conc_list() - Update the concurrent connection list
 * @conn_index: Connection index
 * @mode: Mode
 * @chan: Channel
 * @bw: Bandwidth
 * @mac: Mac id
 * @chain_mask: Chain mask
 * @vdev_id: vdev id
 * @in_use: Flag to indicate if the index is in use or not
 *
 * Updates the index value of the concurrent connection list
 *
 * Return: None
 */
static void cds_update_conc_list(uint32_t conn_index,
		enum cds_con_mode mode,
		uint8_t chan,
		enum hw_mode_bandwidth bw,
		uint8_t mac,
		enum cds_chain_mode chain_mask,
		uint32_t original_nss,
		uint32_t vdev_id,
		bool in_use)
{
	cds_context_type *cds_ctx;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return;
	}

	if (conn_index >= MAX_NUMBER_OF_CONC_CONNECTIONS) {
		cds_err("Number of connections exceeded conn_index: %d",
			conn_index);
		return;
	}
	conc_connection_list[conn_index].mode = mode;
	conc_connection_list[conn_index].chan = chan;
	conc_connection_list[conn_index].bw = bw;
	conc_connection_list[conn_index].mac = mac;
	conc_connection_list[conn_index].chain_mask = chain_mask;
	conc_connection_list[conn_index].original_nss = original_nss;
	conc_connection_list[conn_index].vdev_id = vdev_id;
	conc_connection_list[conn_index].in_use = in_use;

	cds_dump_connection_status_info();
	if (cds_ctx->cdp_update_mac_id)
		cds_ctx->cdp_update_mac_id(cds_get_context(QDF_MODULE_ID_SOC),
			vdev_id, mac);

}

/**
 * cds_mode_specific_connection_count() - provides the
 * count of connections of specific mode
 * @mode: type of connection
 * @list: To provide the indices on conc_connection_list
 *	(optional)
 *
 * This function provides the count of current connections
 *
 * Return: connection count of specific type
 */
uint32_t cds_mode_specific_connection_count(enum cds_con_mode mode,
						uint32_t *list)
{
	uint32_t conn_index = 0, count = 0;
	cds_context_type *cds_ctx;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return count;
	}
	qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
		 conn_index++) {
		if ((conc_connection_list[conn_index].mode == mode) &&
			conc_connection_list[conn_index].in_use) {
			if (list != NULL)
				list[count] = conn_index;
			 count++;
		}
	}
	qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
	return count;
}

/**
 * cds_store_and_del_conn_info() - Store and del a connection info
 * @mode: Mode whose entry has to be deleted
 * @info: Struture pointer where the connection info will be saved
 *
 * Saves the connection info corresponding to the provided mode
 * and deleted that corresponding entry based on vdev from the
 * connection info structure
 *
 * Return: None
 */
static void cds_store_and_del_conn_info(enum cds_con_mode mode,
				struct cds_conc_connection_info *info)
{
	uint32_t conn_index = 0;
	bool found = false;
	cds_context_type *cds_ctx;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return;
	}

	qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
	while (CONC_CONNECTION_LIST_VALID_INDEX(conn_index)) {
		if (mode == conc_connection_list[conn_index].mode) {
			found = true;
			break;
		}
		conn_index++;
	}

	if (!found) {
		qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
		cds_err("Mode:%d not available in the conn info", mode);
		return;
	}

	/* Storing the STA entry which will be temporarily deleted */
	*info = conc_connection_list[conn_index];
	qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);

	/* Deleting the STA entry */
	cds_decr_connection_count(info->vdev_id);

	cds_info("Stored %d (%d), deleted STA entry with vdev id %d, index %d",
		info->vdev_id, info->mode, info->vdev_id, conn_index);

	/* Caller should set the PCL and restore the STA entry in conn info */
}

/**
 * cds_restore_deleted_conn_info() - Restore connection info
 * @info: Saved connection info that is to be restored
 *
 * Restores the connection info of STA that was saved before
 * updating the PCL to the FW
 *
 * Return: None
 */
static void cds_restore_deleted_conn_info(
					struct cds_conc_connection_info *info)
{
	uint32_t conn_index;
	cds_context_type *cds_ctx;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return;
	}

	conn_index = cds_get_connection_count();
	if (MAX_NUMBER_OF_CONC_CONNECTIONS <= conn_index) {
		cds_err("Failed to restore the deleted information %d/%d",
			conn_index, MAX_NUMBER_OF_CONC_CONNECTIONS);
		return;
	}

	qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
	conc_connection_list[conn_index] = *info;
	qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);

	cds_info("Restored the deleleted conn info, vdev:%d, index:%d",
		info->vdev_id, conn_index);
}

/**
 * cds_update_hw_mode_conn_info() - Update connection info based on HW mode
 * @num_vdev_mac_entries: Number of vdev-mac id entries that follow
 * @vdev_mac_map: Mapping of vdev-mac id
 * @hw_mode: HW mode
 *
 * Updates the connection info parameters based on the new HW mode
 *
 * Return: None
 */
static void cds_update_hw_mode_conn_info(uint32_t num_vdev_mac_entries,
				       struct sir_vdev_mac_map *vdev_mac_map,
				       struct sir_hw_mode_params hw_mode)
{
	uint32_t i, conn_index, found;
	cds_context_type *cds_ctx;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return;
	}

	qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
	for (i = 0; i < num_vdev_mac_entries; i++) {
		conn_index = 0;
		found = 0;
		while (CONC_CONNECTION_LIST_VALID_INDEX(conn_index)) {
			if (vdev_mac_map[i].vdev_id ==
				conc_connection_list[conn_index].vdev_id) {
				found = 1;
				break;
			}
			conn_index++;
		}
		if (found) {
			conc_connection_list[conn_index].mac =
				vdev_mac_map[i].mac_id;
			cds_info("vdev:%d, mac:%d",
			  conc_connection_list[conn_index].vdev_id,
			  conc_connection_list[conn_index].mac);
		}
	}
	qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
	cds_dump_connection_status_info();
}

/**
 * cds_soc_set_dual_mac_cfg_cb() - Callback for set dual mac config
 * @status: Status of set dual mac config
 * @scan_config: Current scan config whose status is the first param
 * @fw_mode_config: Current FW mode config whose status is the first param
 *
 * Callback on setting the dual mac configuration
 *
 * Return: None
 */
void cds_soc_set_dual_mac_cfg_cb(enum set_hw_mode_status status,
		uint32_t scan_config,
		uint32_t fw_mode_config)
{
	cds_info("Status:%d for scan_config:%x fw_mode_config:%x",
			status, scan_config, fw_mode_config);
}

/**
 * cds_set_dual_mac_scan_config() - Set the dual MAC scan config
 * @dbs_val: Value of DBS bit
 * @dbs_plus_agile_scan_val: Value of DBS plus agile scan bit
 * @single_mac_scan_with_dbs_val: Value of Single MAC scan with DBS
 *
 * Set the values of scan config. For FW mode config, the existing values
 * will be retained
 *
 * Return: None
 */
void cds_set_dual_mac_scan_config(uint8_t dbs_val,
		uint8_t dbs_plus_agile_scan_val,
		uint8_t single_mac_scan_with_dbs_val)
{
	struct sir_dual_mac_config cfg;
	QDF_STATUS status;
	hdd_context_t *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return;
	}

	/* Any non-zero positive value is treated as 1 */
	if (dbs_val != 0)
		dbs_val = 1;
	if (dbs_plus_agile_scan_val != 0)
		dbs_plus_agile_scan_val = 1;
	if (single_mac_scan_with_dbs_val != 0)
		single_mac_scan_with_dbs_val = 1;

	status = wma_get_updated_scan_config(&cfg.scan_config,
			dbs_val,
			dbs_plus_agile_scan_val,
			single_mac_scan_with_dbs_val);
	if (status != QDF_STATUS_SUCCESS) {
		cds_err("wma_get_updated_scan_config failed %d", status);
		return;
	}

	status = wma_get_updated_fw_mode_config(&cfg.fw_mode_config,
			wma_get_dbs_config(),
			wma_get_agile_dfs_config());
	if (status != QDF_STATUS_SUCCESS) {
		cds_err("wma_get_updated_fw_mode_config failed %d", status);
		return;
	}

	cfg.set_dual_mac_cb = (void *)cds_soc_set_dual_mac_cfg_cb;

	cds_info("scan_config:%x fw_mode_config:%x",
			cfg.scan_config, cfg.fw_mode_config);

	status = sme_soc_set_dual_mac_config(hdd_ctx->hHal, cfg);
	if (status != QDF_STATUS_SUCCESS) {
		cds_err("sme_soc_set_dual_mac_config failed %d", status);
		return;
	}
}

/**
 * cds_set_dual_mac_fw_mode_config() - Set the dual mac FW mode config
 * @dbs: DBS bit
 * @dfs: Agile DFS bit
 *
 * Set the values of fw mode config. For scan config, the existing values
 * will be retain.
 *
 * Return: None
 */
void cds_set_dual_mac_fw_mode_config(uint8_t dbs, uint8_t dfs)
{
	struct sir_dual_mac_config cfg;
	QDF_STATUS status;
	hdd_context_t *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return;
	}

	/* Any non-zero positive value is treated as 1 */
	if (dbs != 0)
		dbs = 1;
	if (dfs != 0)
		dfs = 1;

	status = wma_get_updated_scan_config(&cfg.scan_config,
			wma_get_dbs_scan_config(),
			wma_get_dbs_plus_agile_scan_config(),
			wma_get_single_mac_scan_with_dfs_config());
	if (status != QDF_STATUS_SUCCESS) {
		cds_err("wma_get_updated_scan_config failed %d", status);
		return;
	}

	status = wma_get_updated_fw_mode_config(&cfg.fw_mode_config,
			dbs, dfs);
	if (status != QDF_STATUS_SUCCESS) {
		cds_err("wma_get_updated_fw_mode_config failed %d", status);
		return;
	}

	cfg.set_dual_mac_cb = (void *)cds_soc_set_dual_mac_cfg_cb;

	cds_info("scan_config:%x fw_mode_config:%x",
			cfg.scan_config, cfg.fw_mode_config);

	status = sme_soc_set_dual_mac_config(hdd_ctx->hHal, cfg);
	if (status != QDF_STATUS_SUCCESS) {
		cds_err("sme_soc_set_dual_mac_config failed %d", status);
		return;
	}
}

/**
 * cds_pdev_set_hw_mode_cb() - Callback for set hw mode
 * @status: Status
 * @cfgd_hw_mode_index: Configured HW mode index
 * @num_vdev_mac_entries: Number of vdev-mac id mapping that follows
 * @vdev_mac_map: vdev-mac id map. This memory will be freed by the caller.
 * So, make local copy if needed.
 *
 * Provides the status and configured hw mode index set
 * by the FW
 *
 * Return: None
 */
static void cds_pdev_set_hw_mode_cb(uint32_t status,
				 uint32_t cfgd_hw_mode_index,
				 uint32_t num_vdev_mac_entries,
				 struct sir_vdev_mac_map *vdev_mac_map)
{
	QDF_STATUS ret;
	struct sir_hw_mode_params hw_mode;
	uint32_t i;

	if (status != SET_HW_MODE_STATUS_OK) {
		cds_err("Set HW mode failed with status %d", status);
		return;
	}

	if (!vdev_mac_map) {
		cds_err("vdev_mac_map is NULL");
		return;
	}

	cds_info("cfgd_hw_mode_index=%d", cfgd_hw_mode_index);

	for (i = 0; i < num_vdev_mac_entries; i++)
		cds_info("vdev_id:%d mac_id:%d",
				vdev_mac_map[i].vdev_id,
				vdev_mac_map[i].mac_id);

	ret = wma_get_hw_mode_from_idx(cfgd_hw_mode_index, &hw_mode);
	if (ret != QDF_STATUS_SUCCESS) {
		cds_err("Get HW mode failed: %d", ret);
		return;
	}

	cds_info("MAC0: TxSS:%d, RxSS:%d, Bw:%d",
		hw_mode.mac0_tx_ss, hw_mode.mac0_rx_ss, hw_mode.mac0_bw);
	cds_info("MAC1: TxSS:%d, RxSS:%d, Bw:%d",
		hw_mode.mac1_tx_ss, hw_mode.mac1_rx_ss, hw_mode.mac1_bw);
	cds_info("DBS:%d, Agile DFS:%d, SBS:%d",
		hw_mode.dbs_cap, hw_mode.agile_dfs_cap, hw_mode.sbs_cap);

	/* update conc_connection_list */
	cds_update_hw_mode_conn_info(num_vdev_mac_entries,
			vdev_mac_map,
			hw_mode);

	ret = qdf_set_connection_update();
	if (!QDF_IS_STATUS_SUCCESS(ret))
		cds_err("ERROR: set connection_update_done event failed");

	return;
}

/**
 * cds_hw_mode_transition_cb() - Callback for HW mode transition from FW
 * @old_hw_mode_index: Old HW mode index
 * @new_hw_mode_index: New HW mode index
 * @num_vdev_mac_entries: Number of vdev-mac id mapping that follows
 * @vdev_mac_map: vdev-mac id map. This memory will be freed by the caller.
 * So, make local copy if needed.
 *
 * Provides the old and new HW mode index set by the FW
 *
 * Return: None
 */
void cds_hw_mode_transition_cb(uint32_t old_hw_mode_index,
			uint32_t new_hw_mode_index,
			uint32_t num_vdev_mac_entries,
			struct sir_vdev_mac_map *vdev_mac_map)
{
	QDF_STATUS status;
	struct sir_hw_mode_params hw_mode;
	uint32_t i;

	if (!vdev_mac_map) {
		cds_err("vdev_mac_map is NULL");
		return;
	}

	cds_info("old_hw_mode_index=%d, new_hw_mode_index=%d",
		old_hw_mode_index, new_hw_mode_index);

	for (i = 0; i < num_vdev_mac_entries; i++)
		cds_info("vdev_id:%d mac_id:%d",
			vdev_mac_map[i].vdev_id,
			vdev_mac_map[i].mac_id);

	status = wma_get_hw_mode_from_idx(new_hw_mode_index, &hw_mode);
	if (status != QDF_STATUS_SUCCESS) {
		cds_err("Get HW mode failed: %d", status);
		return;
	}

	cds_info("MAC0: TxSS:%d, RxSS:%d, Bw:%d",
		hw_mode.mac0_tx_ss, hw_mode.mac0_rx_ss, hw_mode.mac0_bw);
	cds_info("MAC1: TxSS:%d, RxSS:%d, Bw:%d",
		hw_mode.mac1_tx_ss, hw_mode.mac1_rx_ss, hw_mode.mac1_bw);
	cds_info("DBS:%d, Agile DFS:%d, SBS:%d",
		hw_mode.dbs_cap, hw_mode.agile_dfs_cap, hw_mode.sbs_cap);

	/* update conc_connection_list */
	cds_update_hw_mode_conn_info(num_vdev_mac_entries,
					  vdev_mac_map,
					  hw_mode);

	return;
}

/**
 * cds_pdev_set_hw_mode() - Set HW mode command to SME
 * @session_id: Session ID
 * @mac0_ss: MAC0 spatial stream configuration
 * @mac0_bw: MAC0 bandwidth configuration
 * @mac1_ss: MAC1 spatial stream configuration
 * @mac1_bw: MAC1 bandwidth configuration
 * @dbs: HW DBS capability
 * @dfs: HW Agile DFS capability
 * @sbs: HW SBS capability
 * @reason: Reason for connection update
 *
 * Sends the set hw mode to the SME module which will pass on
 * this message to WMA layer
 *
 * e.g.: To configure 2x2_80
 *       mac0_ss = HW_MODE_SS_2x2, mac0_bw = HW_MODE_80_MHZ
 *       mac1_ss = HW_MODE_SS_0x0, mac1_bw = HW_MODE_BW_NONE
 *       dbs = HW_MODE_DBS_NONE, dfs = HW_MODE_AGILE_DFS_NONE,
 *       sbs = HW_MODE_SBS_NONE
 * e.g.: To configure 1x1_80_1x1_40 (DBS)
 *       mac0_ss = HW_MODE_SS_1x1, mac0_bw = HW_MODE_80_MHZ
 *       mac1_ss = HW_MODE_SS_1x1, mac1_bw = HW_MODE_40_MHZ
 *       dbs = HW_MODE_DBS, dfs = HW_MODE_AGILE_DFS_NONE,
 *       sbs = HW_MODE_SBS_NONE
 * e.g.: To configure 1x1_80_1x1_40 (Agile DFS)
 *       mac0_ss = HW_MODE_SS_1x1, mac0_bw = HW_MODE_80_MHZ
 *       mac1_ss = HW_MODE_SS_1x1, mac1_bw = HW_MODE_40_MHZ
 *       dbs = HW_MODE_DBS, dfs = HW_MODE_AGILE_DFS,
 *       sbs = HW_MODE_SBS_NONE
 *
 * Return: Success if the message made it down to the next layer
 */
QDF_STATUS cds_pdev_set_hw_mode(uint32_t session_id,
		enum hw_mode_ss_config mac0_ss,
		enum hw_mode_bandwidth mac0_bw,
		enum hw_mode_ss_config mac1_ss,
		enum hw_mode_bandwidth mac1_bw,
		enum hw_mode_dbs_capab dbs,
		enum hw_mode_agile_dfs_capab dfs,
		enum hw_mode_sbs_capab sbs,
		enum sir_conn_update_reason reason)
{
	int8_t hw_mode_index;
	struct sir_hw_mode msg;
	QDF_STATUS status;
	hdd_context_t *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("Invalid HDD context");
		return QDF_STATUS_E_FAILURE;
	}

	/*
	 * if HW is not capable of doing 2x2 or ini config disabled 2x2, don't
	 * allow to request FW for 2x2
	 */
	if ((HW_MODE_SS_2x2 == mac0_ss) && (!hdd_ctx->config->enable2x2)) {
		cds_info("2x2 is not allowed downgrading to 1x1 for mac0");
		mac0_ss = HW_MODE_SS_1x1;
	}
	if ((HW_MODE_SS_2x2 == mac1_ss) && (!hdd_ctx->config->enable2x2)) {
		cds_info("2x2 is not allowed downgrading to 1x1 for mac1");
		mac1_ss = HW_MODE_SS_1x1;
	}

	hw_mode_index = wma_get_hw_mode_idx_from_dbs_hw_list(mac0_ss,
			mac0_bw, mac1_ss, mac1_bw, dbs, dfs, sbs);
	if (hw_mode_index < 0) {
		cds_err("Invalid HW mode index obtained");
		return QDF_STATUS_E_FAILURE;
	}

	msg.hw_mode_index = hw_mode_index;
	msg.set_hw_mode_cb = (void *)cds_pdev_set_hw_mode_cb;
	msg.reason = reason;
	msg.session_id = session_id;

	cds_info("set hw mode to sme: hw_mode_index: %d session:%d reason:%d",
		msg.hw_mode_index, msg.session_id, msg.reason);

	status = sme_pdev_set_hw_mode(hdd_ctx->hHal, msg);
	if (status != QDF_STATUS_SUCCESS) {
		cds_err("Failed to set hw mode to SME");
		return status;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * cds_is_connection_in_progress() - check if connection is in progress
 * @hdd_ctx - HDD context
 *
 * Go through each adapter and check if Connection is in progress
 *
 * Return: true if connection is in progress else false
 */
bool cds_is_connection_in_progress(void)
{
	hdd_adapter_list_node_t *adapter_node = NULL, *next = NULL;
	hdd_station_ctx_t *hdd_sta_ctx = NULL;
	hdd_adapter_t *adapter = NULL;
	QDF_STATUS status = 0;
	uint8_t sta_id = 0;
	uint8_t *sta_mac = NULL;
	hdd_context_t *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return false;
	}

	if (true == hdd_ctx->btCoexModeSet) {
		cds_info("BTCoex Mode operation in progress");
		return true;
	}
	status = hdd_get_front_adapter(hdd_ctx, &adapter_node);
	while (NULL != adapter_node && QDF_STATUS_SUCCESS == status) {
		adapter = adapter_node->pAdapter;
		if (!adapter)
			goto end;

		cds_info("Adapter with device mode %s(%d) exists",
			hdd_device_mode_to_string(adapter->device_mode),
			adapter->device_mode);
		if (((QDF_STA_MODE == adapter->device_mode)
			|| (QDF_P2P_CLIENT_MODE == adapter->device_mode)
			|| (QDF_P2P_DEVICE_MODE == adapter->device_mode))
			&& (eConnectionState_Connecting ==
				(WLAN_HDD_GET_STATION_CTX_PTR(adapter))->
					conn_info.connState)) {
			cds_err("%p(%d) Connection is in progress",
				WLAN_HDD_GET_STATION_CTX_PTR(adapter),
				adapter->sessionId);
			return true;
		}
		if ((QDF_STA_MODE == adapter->device_mode) &&
				sme_neighbor_middle_of_roaming(
					WLAN_HDD_GET_HAL_CTX(adapter),
					adapter->sessionId)) {
			cds_err("%p(%d) Reassociation in progress",
				WLAN_HDD_GET_STATION_CTX_PTR(adapter),
				adapter->sessionId);
			return true;
		}
		if ((QDF_STA_MODE == adapter->device_mode) ||
			(QDF_P2P_CLIENT_MODE == adapter->device_mode) ||
			(QDF_P2P_DEVICE_MODE == adapter->device_mode)) {
			hdd_sta_ctx =
				WLAN_HDD_GET_STATION_CTX_PTR(adapter);
			if ((eConnectionState_Associated ==
				hdd_sta_ctx->conn_info.connState)
				&& (false ==
				hdd_sta_ctx->conn_info.uIsAuthenticated)) {
				sta_mac = (uint8_t *)
					&(adapter->macAddressCurrent.bytes[0]);
				cds_err("client " MAC_ADDRESS_STR
					" is in middle of WPS/EAPOL exchange.",
					MAC_ADDR_ARRAY(sta_mac));
				return true;
			}
		} else if ((QDF_SAP_MODE == adapter->device_mode) ||
				(QDF_P2P_GO_MODE == adapter->device_mode)) {
			for (sta_id = 0; sta_id < WLAN_MAX_STA_COUNT;
				sta_id++) {
				if (!((adapter->aStaInfo[sta_id].isUsed)
				    && (OL_TXRX_PEER_STATE_CONN ==
				    adapter->aStaInfo[sta_id].tlSTAState)))
					continue;

				sta_mac = (uint8_t *)
						&(adapter->aStaInfo[sta_id].
							macAddrSTA.bytes[0]);
				cds_err("client " MAC_ADDRESS_STR
				" of SAP/GO is in middle of WPS/EAPOL exchange",
				MAC_ADDR_ARRAY(sta_mac));
				return true;
			}
			if (hdd_ctx->connection_in_progress) {
				cds_err("AP/GO: connection is in progress");
				return true;
			}
		}
end:
		status = hdd_get_next_adapter(hdd_ctx, adapter_node, &next);
		adapter_node = next;
	}
	return false;
}

/**
 * cds_dump_current_concurrency_one_connection() - To dump the
 * current concurrency info with one connection
 * @cc_mode: connection string
 * @length: Maximum size of the string
 *
 * This routine is called to dump the concurrency info
 *
 * Return: length of the string
 */
static uint32_t cds_dump_current_concurrency_one_connection(char *cc_mode,
			uint32_t length)
{
	uint32_t count = 0;
	enum cds_con_mode mode;
	cds_context_type *cds_ctx;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return count;
	}

	mode = conc_connection_list[0].mode;

	switch (mode) {
	case CDS_STA_MODE:
		count = strlcat(cc_mode, "STA",
					length);
		break;
	case CDS_SAP_MODE:
		count = strlcat(cc_mode, "SAP",
					length);
		break;
	case CDS_P2P_CLIENT_MODE:
		count = strlcat(cc_mode, "P2P CLI",
					length);
		break;
	case CDS_P2P_GO_MODE:
		count = strlcat(cc_mode, "P2P GO",
					length);
		break;
	case CDS_IBSS_MODE:
		count = strlcat(cc_mode, "IBSS",
					length);
		break;
	default:
		cds_err("unexpected mode %d", mode);
		break;
	}
	return count;
}

/**
 * cds_dump_current_concurrency_two_connection() - To dump the
 * current concurrency info with two connections
 * @cc_mode: connection string
 * @length: Maximum size of the string
 *
 * This routine is called to dump the concurrency info
 *
 * Return: length of the string
 */
static uint32_t cds_dump_current_concurrency_two_connection(char *cc_mode,
			uint32_t length)
{
	uint32_t count = 0;
	enum cds_con_mode mode;
	cds_context_type *cds_ctx;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return count;
	}

	mode = conc_connection_list[1].mode;

	switch (mode) {
	case CDS_STA_MODE:
		count = cds_dump_current_concurrency_one_connection(
				cc_mode, length);
		count += strlcat(cc_mode, "+STA",
					length);
		break;
	case CDS_SAP_MODE:
		count = cds_dump_current_concurrency_one_connection(
				cc_mode, length);
		count += strlcat(cc_mode, "+SAP",
					length);
		break;
	case CDS_P2P_CLIENT_MODE:
		count = cds_dump_current_concurrency_one_connection(
				cc_mode, length);
		count += strlcat(cc_mode, "+P2P CLI",
					length);
		break;
	case CDS_P2P_GO_MODE:
		count = cds_dump_current_concurrency_one_connection(
				cc_mode, length);
		count += strlcat(cc_mode, "+P2P GO",
					length);
		break;
	case CDS_IBSS_MODE:
		count = cds_dump_current_concurrency_one_connection(
				cc_mode, length);
		count += strlcat(cc_mode, "+IBSS",
					length);
		break;
	default:
		cds_err("unexpected mode %d", mode);
		break;
	}
	return count;
}

/**
 * cds_dump_current_concurrency_three_connection() - To dump the
 * current concurrency info with three connections
 * @cc_mode: connection string
 * @length: Maximum size of the string
 *
 * This routine is called to dump the concurrency info
 *
 * Return: length of the string
 */
static uint32_t cds_dump_current_concurrency_three_connection(char *cc_mode,
			uint32_t length)
{
	uint32_t count = 0;
	enum cds_con_mode mode;
	cds_context_type *cds_ctx;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return count;
	}

	mode = conc_connection_list[2].mode;

	switch (mode) {
	case CDS_STA_MODE:
		count = cds_dump_current_concurrency_two_connection(
				cc_mode, length);
		count += strlcat(cc_mode, "+STA",
					length);
		break;
	case CDS_SAP_MODE:
		count = cds_dump_current_concurrency_two_connection(
				cc_mode, length);
		count += strlcat(cc_mode, "+SAP",
					length);
		break;
	case CDS_P2P_CLIENT_MODE:
		count = cds_dump_current_concurrency_two_connection(
				cc_mode, length);
		count += strlcat(cc_mode, "+P2P CLI",
					length);
		break;
	case CDS_P2P_GO_MODE:
		count = cds_dump_current_concurrency_two_connection(
				cc_mode, length);
		count += strlcat(cc_mode, "+P2P GO",
					length);
		break;
	case CDS_IBSS_MODE:
		count = cds_dump_current_concurrency_two_connection(
				cc_mode, length);
		count += strlcat(cc_mode, "+IBSS",
					length);
		break;
	default:
		cds_err("unexpected mode %d", mode);
		break;
	}
	return count;
}

/**
 * cds_dump_dbs_concurrency() - To dump the dbs concurrency
 * combination
 * @cc_mode: connection string
 *
 * This routine is called to dump the concurrency info
 *
 * Return: None
 */
static void cds_dump_dbs_concurrency(char *cc_mode, uint32_t length)
{
	char buf[4] = {0};
	uint8_t mac = 0;
	cds_context_type *cds_ctx;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return;
	}

	strlcat(cc_mode, " DBS", length);
	qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
	if (conc_connection_list[0].mac ==
		conc_connection_list[1].mac) {
		if (conc_connection_list[0].chan ==
			conc_connection_list[1].chan)
			strlcat(cc_mode,
				" with SCC for 1st two connections on mac ",
				length);
		else
			strlcat(cc_mode,
				" with MCC for 1st two connections on mac ",
				length);
		mac = conc_connection_list[0].mac;
	}
	if (conc_connection_list[0].mac == conc_connection_list[2].mac) {
		if (conc_connection_list[0].chan ==
			conc_connection_list[2].chan)
			strlcat(cc_mode,
				" with SCC for 1st & 3rd connections on mac ",
				length);
		else
			strlcat(cc_mode,
				" with MCC for 1st & 3rd connections on mac ",
				length);
		mac = conc_connection_list[0].mac;
	}
	if (conc_connection_list[1].mac == conc_connection_list[2].mac) {
		if (conc_connection_list[1].chan ==
			conc_connection_list[2].chan)
			strlcat(cc_mode,
				" with SCC for 2nd & 3rd connections on mac ",
				length);
		else
			strlcat(cc_mode,
				" with MCC for 2nd & 3rd connections on mac ",
				length);
		mac = conc_connection_list[1].mac;
	}
	qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
	snprintf(buf, sizeof(buf), "%d ", mac);
	strlcat(cc_mode, buf, length);
}

/**
 * cds_dump_current_concurrency() - To dump the current
 * concurrency combination
 *
 * This routine is called to dump the concurrency info
 *
 * Return: None
 */
static void cds_dump_current_concurrency(void)
{
	uint32_t num_connections = 0;
	char cc_mode[CDS_MAX_CON_STRING_LEN] = {0};
	uint32_t count = 0;
	cds_context_type *cds_ctx;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return;
	}

	num_connections = cds_get_connection_count();

	switch (num_connections) {
	case 1:
		cds_dump_current_concurrency_one_connection(cc_mode,
					sizeof(cc_mode));
		cds_err("%s Standalone", cc_mode);
		break;
	case 2:
		count = cds_dump_current_concurrency_two_connection(
			cc_mode, sizeof(cc_mode));
		qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
		if (conc_connection_list[0].chan ==
			conc_connection_list[1].chan) {
			strlcat(cc_mode, " SCC", sizeof(cc_mode));
		} else if (conc_connection_list[0].mac ==
					conc_connection_list[1].mac) {
			strlcat(cc_mode, " MCC", sizeof(cc_mode));
		} else
			strlcat(cc_mode, " DBS", sizeof(cc_mode));
		qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
		cds_err("%s", cc_mode);
		break;
	case 3:
		count = cds_dump_current_concurrency_three_connection(
			cc_mode, sizeof(cc_mode));
		qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
		if ((conc_connection_list[0].chan ==
			conc_connection_list[1].chan) &&
			(conc_connection_list[0].chan ==
				conc_connection_list[2].chan)){
			qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
				strlcat(cc_mode, " SCC",
						sizeof(cc_mode));
		} else if ((conc_connection_list[0].mac ==
				conc_connection_list[1].mac)
				&& (conc_connection_list[0].mac ==
					conc_connection_list[2].mac)) {
			qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
					strlcat(cc_mode, " MCC on single MAC",
						sizeof(cc_mode));
		} else {
			qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
			cds_dump_dbs_concurrency(cc_mode, sizeof(cc_mode));
		}
		cds_err("%s", cc_mode);
		break;
	default:
		cds_err("unexpected num_connections value %d",
			num_connections);
		break;
	}

	return;
}

/**
 * cds_current_concurrency_is_mcc() - To check the current
 * concurrency combination if it is doing MCC
 *
 * This routine is called to check if it is doing MCC
 *
 * Return: True - MCC, False - Otherwise
 */
static bool cds_current_concurrency_is_mcc(void)
{
	uint32_t num_connections = 0;
	bool is_mcc = false;

	num_connections = cds_get_connection_count();

	switch (num_connections) {
	case 1:
		break;
	case 2:
		if ((conc_connection_list[0].chan !=
			conc_connection_list[1].chan) &&
		    (conc_connection_list[0].mac ==
			conc_connection_list[1].mac)) {
			is_mcc = true;
		}
		break;
	case 3:
		if ((conc_connection_list[0].chan !=
			conc_connection_list[1].chan) ||
		    (conc_connection_list[0].chan !=
			conc_connection_list[2].chan) ||
		    (conc_connection_list[1].chan !=
			conc_connection_list[2].chan)){
				is_mcc = true;
		}
		break;
	default:
		cds_err("unexpected num_connections value %d",
			num_connections);
		break;
	}

	return is_mcc;
}

/**
 * cds_dump_concurrency_info() - To dump concurrency info
 *
 * This routine is called to dump the concurrency info
 *
 * Return: None
 */
void cds_dump_concurrency_info(void)
{
	hdd_adapter_list_node_t *adapterNode = NULL, *pNext = NULL;
	QDF_STATUS status;
	hdd_adapter_t *adapter;
	hdd_station_ctx_t *pHddStaCtx;
	hdd_ap_ctx_t *hdd_ap_ctx;
	hdd_hostapd_state_t *hostapd_state;
	struct qdf_mac_addr staBssid = QDF_MAC_ADDR_ZERO_INITIALIZER;
	struct qdf_mac_addr p2pBssid = QDF_MAC_ADDR_ZERO_INITIALIZER;
	struct qdf_mac_addr apBssid = QDF_MAC_ADDR_ZERO_INITIALIZER;
	uint8_t staChannel = 0, p2pChannel = 0, apChannel = 0;
	const char *p2pMode = "DEV";
	hdd_context_t *hdd_ctx;
	cds_context_type *cds_ctx;
#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
	uint8_t targetChannel = 0;
	uint8_t preAdapterChannel = 0;
	uint8_t channel24;
	uint8_t channel5;
	hdd_adapter_t *preAdapterContext = NULL;
	hdd_adapter_t *adapter2_4 = NULL;
	hdd_adapter_t *adapter5 = NULL;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL */

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return;
	}

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return;
	}

	status = hdd_get_front_adapter(hdd_ctx, &adapterNode);
	while (NULL != adapterNode && QDF_STATUS_SUCCESS == status) {
		adapter = adapterNode->pAdapter;
		switch (adapter->device_mode) {
		case QDF_STA_MODE:
			pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
			if (eConnectionState_Associated ==
			    pHddStaCtx->conn_info.connState) {
				staChannel =
					pHddStaCtx->conn_info.operationChannel;
				qdf_copy_macaddr(&staBssid,
						 &pHddStaCtx->conn_info.bssId);
#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
				targetChannel = staChannel;
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL */
			}
			break;
		case QDF_P2P_CLIENT_MODE:
			pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
			if (eConnectionState_Associated ==
			    pHddStaCtx->conn_info.connState) {
				p2pChannel =
					pHddStaCtx->conn_info.operationChannel;
				qdf_copy_macaddr(&p2pBssid,
						&pHddStaCtx->conn_info.bssId);
				p2pMode = "CLI";
#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
				targetChannel = p2pChannel;
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL */
			}
			break;
		case QDF_P2P_GO_MODE:
			hdd_ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(adapter);
			hostapd_state = WLAN_HDD_GET_HOSTAP_STATE_PTR(adapter);
			if (hostapd_state->bssState == BSS_START
			    && hostapd_state->qdf_status ==
			    QDF_STATUS_SUCCESS) {
				p2pChannel = hdd_ap_ctx->operatingChannel;
				qdf_copy_macaddr(&p2pBssid,
						 &adapter->macAddressCurrent);
#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
				targetChannel = p2pChannel;
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL */
			}
			p2pMode = "GO";
			break;
		case QDF_SAP_MODE:
			hdd_ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(adapter);
			hostapd_state = WLAN_HDD_GET_HOSTAP_STATE_PTR(adapter);
			if (hostapd_state->bssState == BSS_START
			    && hostapd_state->qdf_status ==
			    QDF_STATUS_SUCCESS) {
				apChannel = hdd_ap_ctx->operatingChannel;
				qdf_copy_macaddr(&apBssid,
						&adapter->macAddressCurrent);
#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
				targetChannel = apChannel;
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL */
			}
			break;
		case QDF_IBSS_MODE:
		default:
			break;
		}
#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
		if (targetChannel) {
			/*
			 * This is first adapter detected as active
			 * set as default for none concurrency case
			 */
			if (!preAdapterChannel) {
				/* If IPA UC data path is enabled,
				 * target should reserve extra tx descriptors
				 * for IPA data path.
				 * Then host data path should allow less TX
				 * packet pumping in case IPA
				 * data path enabled
				 */
				if (hdd_ipa_uc_is_enabled(hdd_ctx) &&
				    (QDF_SAP_MODE == adapter->device_mode)) {
					adapter->tx_flow_low_watermark =
					hdd_ctx->config->TxFlowLowWaterMark +
					WLAN_TFC_IPAUC_TX_DESC_RESERVE;
				} else {
					adapter->tx_flow_low_watermark =
						hdd_ctx->config->
							TxFlowLowWaterMark;
				}
				adapter->tx_flow_high_watermark_offset =
				   hdd_ctx->config->TxFlowHighWaterMarkOffset;
				cdp_fc_ll_set_tx_pause_q_depth(soc,
					adapter->sessionId,
					hdd_ctx->config->TxFlowMaxQueueDepth);
				cds_info("MODE %d,CH %d,LWM %d,HWM %d,TXQDEP %d",
				    adapter->device_mode,
				    targetChannel,
				    adapter->tx_flow_low_watermark,
				    adapter->tx_flow_low_watermark +
				    adapter->tx_flow_high_watermark_offset,
				    hdd_ctx->config->TxFlowMaxQueueDepth);
				preAdapterChannel = targetChannel;
				preAdapterContext = adapter;
			} else {
				/*
				 * SCC, disable TX flow control for both
				 * SCC each adapter cannot reserve dedicated
				 * channel resource, as a result, if any adapter
				 * blocked OS Q by flow control,
				 * blocked adapter will lost chance to recover
				 */
				if (preAdapterChannel == targetChannel) {
					/* Current adapter */
					adapter->tx_flow_low_watermark = 0;
					adapter->
					tx_flow_high_watermark_offset = 0;
					cdp_fc_ll_set_tx_pause_q_depth(soc,
						adapter->sessionId,
						hdd_ctx->config->
						TxHbwFlowMaxQueueDepth);
					cds_info("SCC: MODE %s(%d), CH %d, LWM %d, HWM %d, TXQDEP %d",
					       hdd_device_mode_to_string(
							adapter->device_mode),
					       adapter->device_mode,
					       targetChannel,
					       adapter->tx_flow_low_watermark,
					       adapter->tx_flow_low_watermark +
					       adapter->
					       tx_flow_high_watermark_offset,
					       hdd_ctx->config->
					       TxHbwFlowMaxQueueDepth);

					if (!preAdapterContext) {
						cds_err("SCC: Previous adapter context NULL");
						continue;
					}

					/* Previous adapter */
					preAdapterContext->
					tx_flow_low_watermark = 0;
					preAdapterContext->
					tx_flow_high_watermark_offset = 0;
					cdp_fc_ll_set_tx_pause_q_depth(soc,
						preAdapterContext->sessionId,
						hdd_ctx->config->
						TxHbwFlowMaxQueueDepth);
					cds_info("SCC: MODE %s(%d), CH %d, LWM %d, HWM %d, TXQDEP %d",
					       hdd_device_mode_to_string(
						preAdapterContext->device_mode
							  ),
					       preAdapterContext->device_mode,
					       targetChannel,
					       preAdapterContext->
					       tx_flow_low_watermark,
					       preAdapterContext->
					       tx_flow_low_watermark +
					       preAdapterContext->
					       tx_flow_high_watermark_offset,
					       hdd_ctx->config->
					       TxHbwFlowMaxQueueDepth);
				}
				/*
				 * MCC, each adapter will have dedicated
				 * resource
				 */
				else {
					/* current channel is 2.4 */
					if (targetChannel <=
				     WLAN_HDD_TX_FLOW_CONTROL_MAX_24BAND_CH) {
						channel24 = targetChannel;
						channel5 = preAdapterChannel;
						adapter2_4 = adapter;
						adapter5 = preAdapterContext;
					} else {
						/* Current channel is 5 */
						channel24 = preAdapterChannel;
						channel5 = targetChannel;
						adapter2_4 = preAdapterContext;
						adapter5 = adapter;
					}

					if (!adapter5) {
						cds_err("MCC: 5GHz adapter context NULL");
						continue;
					}
					adapter5->tx_flow_low_watermark =
						hdd_ctx->config->
						TxHbwFlowLowWaterMark;
					adapter5->
					tx_flow_high_watermark_offset =
						hdd_ctx->config->
						TxHbwFlowHighWaterMarkOffset;
					cdp_fc_ll_set_tx_pause_q_depth(soc,
						adapter5->sessionId,
						hdd_ctx->config->
						TxHbwFlowMaxQueueDepth);
					cds_info("MCC: MODE %s(%d), CH %d, LWM %d, HWM %d, TXQDEP %d",
					    hdd_device_mode_to_string(
						    adapter5->device_mode),
					    adapter5->device_mode,
					    channel5,
					    adapter5->tx_flow_low_watermark,
					    adapter5->
					    tx_flow_low_watermark +
					    adapter5->
					    tx_flow_high_watermark_offset,
					    hdd_ctx->config->
					    TxHbwFlowMaxQueueDepth);

					if (!adapter2_4) {
						cds_err("MCC: 2.4GHz adapter context NULL");
						continue;
					}
					adapter2_4->tx_flow_low_watermark =
						hdd_ctx->config->
						TxLbwFlowLowWaterMark;
					adapter2_4->
					tx_flow_high_watermark_offset =
						hdd_ctx->config->
						TxLbwFlowHighWaterMarkOffset;
					cdp_fc_ll_set_tx_pause_q_depth(soc,
						adapter2_4->sessionId,
						hdd_ctx->config->
						TxLbwFlowMaxQueueDepth);
					cds_info("MCC: MODE %s(%d), CH %d, LWM %d, HWM %d, TXQDEP %d",
						hdd_device_mode_to_string(
						    adapter2_4->device_mode),
						adapter2_4->device_mode,
						channel24,
						adapter2_4->
						tx_flow_low_watermark,
						adapter2_4->
						tx_flow_low_watermark +
						adapter2_4->
						tx_flow_high_watermark_offset,
						hdd_ctx->config->
						TxLbwFlowMaxQueueDepth);

				}
			}
		}
		targetChannel = 0;
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL */
		status = hdd_get_next_adapter(hdd_ctx, adapterNode, &pNext);
		adapterNode = pNext;
	}
	qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
	hdd_ctx->mcc_mode = cds_current_concurrency_is_mcc();
	qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
}

#ifdef FEATURE_WLAN_TDLS
/*
 * cds_check_is_tdls_allowed() - check is tdls allowed or not
 * @adapter: pointer to adapter
 *
 * Function determines the whether TDLS allowed in the system
 *
 * Return: true or false
 */
bool cds_check_is_tdls_allowed(enum tQDF_ADAPTER_MODE device_mode)
{
	bool state = false;
	uint32_t count;

	count = cds_get_connection_count();

	if (count > 1)
		state = false;
	else if (device_mode == QDF_STA_MODE ||
		 device_mode == QDF_P2P_CLIENT_MODE)
		state = true;

	/* If any concurrency is detected */
	if (!state)
		cds_dump_concurrency_info();

	return state;
}

/**
 * cds_set_tdls_ct_mode() - Set the tdls connection tracker mode
 * @hdd_ctx: hdd context
 *
 * This routine is called to set the tdls connection tracker operation status
 *
 * Return: NONE
 */
void cds_set_tdls_ct_mode(hdd_context_t *hdd_ctx)
{
	bool state = false;

	/* If any concurrency is detected, skip tdls pkt tracker */
	if (cds_get_connection_count() > 1) {
		state = false;
		goto set_state;
	}

	if (eTDLS_SUPPORT_DISABLED == hdd_ctx->tdls_mode ||
	    eTDLS_SUPPORT_NOT_ENABLED == hdd_ctx->tdls_mode ||
	    (!hdd_ctx->config->fEnableTDLSImplicitTrigger)) {
		state = false;
		goto set_state;
	} else if (cds_mode_specific_connection_count(QDF_STA_MODE,
						      NULL) == 1) {
		state = true;
	} else if (cds_mode_specific_connection_count(QDF_P2P_CLIENT_MODE,
						      NULL) == 1){
		state = true;
	} else {
		state = false;
		goto set_state;
	}

	/* In case of TDLS external control, peer should be added
	 * by the user space to start connection tracker.
	 */
	if (hdd_ctx->config->fTDLSExternalControl) {
		if (hdd_ctx->tdls_external_peer_count)
			state = true;
		else
			state = false;
	}

set_state:
	mutex_lock(&hdd_ctx->tdls_lock);
	hdd_ctx->enable_tdls_connection_tracker = state;
	mutex_unlock(&hdd_ctx->tdls_lock);

	cds_info("enable_tdls_connection_tracker %d",
		 hdd_ctx->enable_tdls_connection_tracker);
}
#endif

/**
 * cds_set_concurrency_mode() - To set concurrency mode
 * @mode: adapter mode
 *
 * This routine is called to set the concurrency mode
 *
 * Return: NONE
 */
void cds_set_concurrency_mode(enum tQDF_ADAPTER_MODE mode)
{
	hdd_context_t *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return;
	}

	switch (mode) {
	case QDF_STA_MODE:
	case QDF_P2P_CLIENT_MODE:
	case QDF_P2P_GO_MODE:
	case QDF_SAP_MODE:
	case QDF_IBSS_MODE:
	case QDF_MONITOR_MODE:
		hdd_ctx->concurrency_mode |= (1 << mode);
		hdd_ctx->no_of_open_sessions[mode]++;
		break;
	default:
		break;
	}

	/* set tdls connection tracker state */
	cds_set_tdls_ct_mode(hdd_ctx);

	cds_info("concurrency_mode = 0x%x Number of open sessions for mode %d = %d",
		hdd_ctx->concurrency_mode, mode,
		hdd_ctx->no_of_open_sessions[mode]);

	hdd_green_ap_start_bss(hdd_ctx);
}

/**
 * cds_clear_concurrency_mode() - To clear concurrency mode
 * @mode: adapter mode
 *
 * This routine is called to clear the concurrency mode
 *
 * Return: NONE
 */
void cds_clear_concurrency_mode(enum tQDF_ADAPTER_MODE mode)
{
	hdd_context_t *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return;
	}

	switch (mode) {
	case QDF_STA_MODE:
	case QDF_P2P_CLIENT_MODE:
	case QDF_P2P_GO_MODE:
	case QDF_SAP_MODE:
	case QDF_MONITOR_MODE:
		hdd_ctx->no_of_open_sessions[mode]--;
		if (!(hdd_ctx->no_of_open_sessions[mode]))
			hdd_ctx->concurrency_mode &= (~(1 << mode));
		break;
	default:
		break;
	}

	/* set tdls connection tracker state */
	cds_set_tdls_ct_mode(hdd_ctx);

	cds_info("concurrency_mode = 0x%x Number of open sessions for mode %d = %d",
		hdd_ctx->concurrency_mode, mode,
		hdd_ctx->no_of_open_sessions[mode]);

	hdd_green_ap_start_bss(hdd_ctx);
}

/**
 * cds_pdev_set_pcl() - Sets PCL to FW
 * @mode: adapter mode
 *
 * Fetches the PCL and sends the PCL to SME
 * module which in turn will send the WMI
 * command WMI_PDEV_SET_PCL_CMDID to the fw
 *
 * Return: None
 */
static void cds_pdev_set_pcl(enum tQDF_ADAPTER_MODE mode)
{
	QDF_STATUS status;
	enum cds_con_mode con_mode;
	struct sir_pcl_list pcl;
	hdd_context_t *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return;
	}
	pcl.pcl_len = 0;

	switch (mode) {
	case QDF_STA_MODE:
		con_mode = CDS_STA_MODE;
		break;
	case QDF_P2P_CLIENT_MODE:
		con_mode = CDS_P2P_CLIENT_MODE;
		break;
	case QDF_P2P_GO_MODE:
		con_mode = CDS_P2P_GO_MODE;
		break;
	case QDF_SAP_MODE:
		con_mode = CDS_SAP_MODE;
		break;
	case QDF_IBSS_MODE:
		con_mode = CDS_IBSS_MODE;
		break;
	default:
		cds_err("Unable to set PCL to FW: %d", mode);
		return;
	}

	cds_debug("get pcl to set it to the FW");

	status = cds_get_pcl(con_mode,
			pcl.pcl_list, &pcl.pcl_len,
			pcl.weight_list, QDF_ARRAY_SIZE(pcl.weight_list));
	if (status != QDF_STATUS_SUCCESS) {
		cds_err("Unable to set PCL to FW, Get PCL failed");
		return;
	}

	status = sme_pdev_set_pcl(hdd_ctx->hHal, pcl);
	if (status != QDF_STATUS_SUCCESS)
		cds_err("Send soc set PCL to SME failed");
	else
		cds_info("Set PCL to FW for mode:%d", mode);
}


/**
 * cds_set_pcl_for_existing_combo() - Set PCL for existing connection
 * @mode: Connection mode of type 'cds_con_mode'
 *
 * Set the PCL for an existing connection
 *
 * Return: None
 */
static void cds_set_pcl_for_existing_combo(enum cds_con_mode mode)
{
	struct cds_conc_connection_info info;
	enum tQDF_ADAPTER_MODE pcl_mode;

	switch (mode) {
	case CDS_STA_MODE:
		pcl_mode = QDF_STA_MODE;
		break;
	case CDS_SAP_MODE:
		pcl_mode = QDF_SAP_MODE;
		break;
	case CDS_P2P_CLIENT_MODE:
		pcl_mode = QDF_P2P_CLIENT_MODE;
		break;
	case CDS_P2P_GO_MODE:
		pcl_mode = QDF_P2P_GO_MODE;
		break;
	case CDS_IBSS_MODE:
		pcl_mode = QDF_IBSS_MODE;
		break;
	default:
		cds_err("Invalid mode to set PCL");
		return;
	};

	if (cds_mode_specific_connection_count(mode, NULL) > 0) {
		/* Check, store and temp delete the mode's parameter */
		cds_store_and_del_conn_info(mode, &info);
		/* Set the PCL to the FW since connection got updated */
		cds_pdev_set_pcl(pcl_mode);
		cds_info("Set PCL to FW for mode:%d", mode);
		/* Restore the connection info */
		cds_restore_deleted_conn_info(&info);
	}
}

/**
 * cds_incr_active_session() - increments the number of active sessions
 * @mode:	Adapter mode
 * @session_id: session ID for the connection session
 *
 * This function increments the number of active sessions maintained per device
 * mode. In the case of STA/P2P CLI/IBSS upon connection indication it is
 * incremented; In the case of SAP/P2P GO upon bss start it is incremented
 *
 * Return: None
 */
void cds_incr_active_session(enum tQDF_ADAPTER_MODE mode,
				  uint8_t session_id)
{
	hdd_context_t *hdd_ctx;
	cds_context_type *cds_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return;
	}

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return;
	}

	/*
	 * Need to aquire mutex as entire functionality in this function
	 * is in critical section
	 */
	qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
	switch (mode) {
	case QDF_STA_MODE:
	case QDF_P2P_CLIENT_MODE:
	case QDF_P2P_GO_MODE:
	case QDF_SAP_MODE:
	case QDF_IBSS_MODE:
		hdd_ctx->no_of_active_sessions[mode]++;
		break;
	default:
		break;
	}


	cds_info("No.# of active sessions for mode %d = %d",
		mode, hdd_ctx->no_of_active_sessions[mode]);
	/*
	 * Get PCL logic makes use of the connection info structure.
	 * Let us set the PCL to the FW before updating the connection
	 * info structure about the new connection.
	 */
	if (mode == QDF_STA_MODE) {
		qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
		/* Set PCL of STA to the FW */
		cds_pdev_set_pcl(mode);
		qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
		cds_info("Set PCL of STA to FW");
	}
	cds_incr_connection_count(session_id);
	if ((cds_mode_specific_connection_count(CDS_STA_MODE, NULL) > 0) &&
		(mode != QDF_STA_MODE)) {
		qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
		cds_set_pcl_for_existing_combo(CDS_STA_MODE);
		qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
	}

	/* set tdls connection tracker state */
	cds_set_tdls_ct_mode(hdd_ctx);
	cds_dump_current_concurrency();

	qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
}

/**
 * cds_need_opportunistic_upgrade() - Tells us if we really
 * need an upgrade to 2x2
 *
 * This function returns if updrade to 2x2 is needed
 *
 * Return: CDS_NOP = upgrade is not needed, otherwise upgrade is
 * needed
 */
enum cds_conc_next_action cds_need_opportunistic_upgrade(void)
{
	uint32_t conn_index;
	enum cds_conc_next_action upgrade = CDS_NOP;
	uint8_t mac = 0;
	struct sir_hw_mode_params hw_mode;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	cds_context_type *cds_ctx;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		goto done;
	}

	if (wma_is_hw_dbs_capable() == false) {
		cds_err("driver isn't dbs capable, no further action needed");
		goto done;
	}

	status = wma_get_current_hw_mode(&hw_mode);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		cds_err("wma_get_current_hw_mode failed");
		goto done;
	}
	if (!hw_mode.dbs_cap) {
		cds_info("current HW mode is non-DBS capable");
		goto done;
	}

	qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
	/* Are both mac's still in use */
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
		conn_index++) {
		cds_debug("index:%d mac:%d in_use:%d chan:%d org_nss:%d",
			conn_index,
			conc_connection_list[conn_index].mac,
			conc_connection_list[conn_index].in_use,
			conc_connection_list[conn_index].chan,
			conc_connection_list[conn_index].original_nss);
		if ((conc_connection_list[conn_index].mac == 0) &&
			conc_connection_list[conn_index].in_use) {
			mac |= CDS_MAC0;
			if (CDS_MAC0_AND_MAC1 == mac) {
				qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
				goto done;
			}
		} else if ((conc_connection_list[conn_index].mac == 1) &&
			conc_connection_list[conn_index].in_use) {
			mac |= CDS_MAC1;
			if (CDS_MAC0_AND_MAC1 == mac) {
				qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
				goto done;
			}
		}
	}
	/* Let's request for single MAC mode */
	upgrade = CDS_SINGLE_MAC;
	/* Is there any connection had an initial connection with 2x2 */
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
		conn_index++) {
		if ((conc_connection_list[conn_index].original_nss == 2) &&
			conc_connection_list[conn_index].in_use) {
			upgrade = CDS_SINGLE_MAC_UPGRADE;
			qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
			goto done;
		}
	}
	qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);

done:
	return upgrade;
}

/**
 * cds_get_pcl_for_existing_conn() - Get PCL for existing connection
 * @mode: Connection mode of type 'cds_con_mode'
 * @pcl_ch: Pointer to the PCL
 * @len: Pointer to the length of the PCL
 * @pcl_weight: Pointer to the weights of the PCL
 * @weight_len: Max length of the weights list
 *
 * Get the PCL for an existing connection
 *
 * Return: None
 */
QDF_STATUS cds_get_pcl_for_existing_conn(enum cds_con_mode mode,
			uint8_t *pcl_ch, uint32_t *len,
			uint8_t *pcl_weight, uint32_t weight_len)
{
	struct cds_conc_connection_info info;

	cds_context_type *cds_ctx;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return QDF_STATUS_E_INVAL;
	}

	cds_info("get pcl for existing conn:%d", mode);

	if (cds_mode_specific_connection_count(mode, NULL) > 0) {
		/* Check, store and temp delete the mode's parameter */
		cds_store_and_del_conn_info(mode, &info);
		/* Get the PCL */
		status = cds_get_pcl(mode, pcl_ch, len, pcl_weight, weight_len);
		cds_info("Get PCL to FW for mode:%d", mode);
		/* Restore the connection info */
		cds_restore_deleted_conn_info(&info);
	}
	return status;
}

/**
 * cds_decr_session_set_pcl() - Decrement session count and set PCL
 * @mode: Adapter mode
 * @session_id: Session id
 *
 * Decrements the active session count and sets the PCL if a STA connection
 * exists
 *
 * Return: None
 */
void cds_decr_session_set_pcl(enum tQDF_ADAPTER_MODE mode,
						uint8_t session_id)
{
	QDF_STATUS qdf_status;
	cds_context_type *cds_ctx;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return;
	}

	cds_decr_active_session(mode, session_id);
	/*
	 * After the removal of this connection, we need to check if
	 * a STA connection still exists. The reason for this is that
	 * if one or more STA exists, we need to provide the updated
	 * PCL to the FW for cases like LFR.
	 *
	 * Since cds_get_pcl provides PCL list based on the new
	 * connection that is going to come up, we will find the
	 * existing STA entry, save it and delete it temporarily.
	 * After this we will get PCL as though as new STA connection
	 * is coming up. This will give the exact PCL that needs to be
	 * given to the FW. After setting the PCL, we need to restore
	 * the entry that we have saved before.
	 */
	cds_set_pcl_for_existing_combo(CDS_STA_MODE);
	/* do we need to change the HW mode */
	if (cds_need_opportunistic_upgrade()) {
		/* let's start the timer */
		qdf_mc_timer_stop(&cds_ctx->dbs_opportunistic_timer);
		qdf_status = qdf_mc_timer_start(
					&cds_ctx->dbs_opportunistic_timer,
					DBS_OPPORTUNISTIC_TIME *
						1000);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status))
			cds_err("Failed to start dbs opportunistic timer");
	}

	return;
}


/**
 * cds_decr_active_session() - decrements the number of active sessions
 * @mode: Adapter mode
 * @session_id: session ID for the connection session
 *
 * This function decrements the number of active sessions maintained per device
 * mode. In the case of STA/P2P CLI/IBSS upon disconnection it is decremented
 * In the case of SAP/P2P GO upon bss stop it is decremented
 *
 * Return: None
 */
void cds_decr_active_session(enum tQDF_ADAPTER_MODE mode,
				  uint8_t session_id)
{
	hdd_context_t *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return;
	}

	switch (mode) {
	case QDF_STA_MODE:
	case QDF_P2P_CLIENT_MODE:
	case QDF_P2P_GO_MODE:
	case QDF_SAP_MODE:
	case QDF_IBSS_MODE:
		if (hdd_ctx->no_of_active_sessions[mode])
			hdd_ctx->no_of_active_sessions[mode]--;
		break;
	default:
		break;
	}

	cds_info("No.# of active sessions for mode %d = %d",
		mode, hdd_ctx->no_of_active_sessions[mode]);

	cds_decr_connection_count(session_id);

	/* set tdls connection tracker state */
	cds_set_tdls_ct_mode(hdd_ctx);

	cds_dump_current_concurrency();

}

/**
 * cds_dbs_opportunistic_timer_handler() - handler of
 * dbs_opportunistic_timer
 * @data: CDS context
 *
 * handler for dbs_opportunistic_timer
 *
 * Return: None
 */
static void cds_dbs_opportunistic_timer_handler(void *data)
{
	enum cds_conc_next_action action = CDS_NOP;
	cds_context_type *cds_ctx = (cds_context_type *)data;

	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return;
	}

	/* if we still need it */
	action = cds_need_opportunistic_upgrade();
	cds_info("action:%d", action);
	if (action) {
		/* lets call for action */
		/* session id is being used only
		 * in hidden ssid case for now.
		 * So, session id 0 is ok here.
		 */
		cds_next_actions(0, action,
				SIR_UPDATE_REASON_OPPORTUNISTIC);
	}

}

/**
 * cds_deinit_policy_mgr() - Deinitialize the policy manager
 * related data structures
 *
 * Deinitialize the policy manager related data structures
 *
 * Return: Success if the policy manager is deinitialized completely
 */
QDF_STATUS cds_deinit_policy_mgr(void)
{
	cds_context_type *cds_ctx;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return QDF_STATUS_E_FAILURE;
	}

	if (!QDF_IS_STATUS_SUCCESS(qdf_event_destroy
				  (&cds_ctx->connection_update_done_evt))) {
		cds_err("Failed to destroy connection_update_done_evt");
		status = QDF_STATUS_E_FAILURE;
		QDF_ASSERT(0);
	}

	if (QDF_TIMER_STATE_RUNNING ==
			qdf_mc_timer_get_current_state(
				&cds_ctx->dbs_opportunistic_timer)) {
		qdf_mc_timer_stop(&cds_ctx->dbs_opportunistic_timer);
	}

	if (!QDF_IS_STATUS_SUCCESS(qdf_mc_timer_destroy(
				      &cds_ctx->dbs_opportunistic_timer))) {
		cds_err("Cannot deallocate dbs opportunistic timer");
		status = QDF_STATUS_E_FAILURE;
		QDF_ASSERT(0);
	}

	cds_ctx->sme_get_valid_channels = NULL;
	cds_ctx->sme_get_nss_for_vdev = NULL;

	if (QDF_IS_STATUS_ERROR(cds_reset_sap_mandatory_channels())) {
		cds_err("failed to reset sap mandatory channels");
		status = QDF_STATUS_E_FAILURE;
		QDF_ASSERT(0);
	}

	return status;
}

/**
 * cds_init_policy_mgr() - Initialize the policy manager
 * related data structures
 *
 * Initialize the policy manager related data structures
 *
 * Return: Success if the policy manager is initialized completely
 */
QDF_STATUS cds_init_policy_mgr(struct cds_sme_cbacks *sme_cbacks)
{
	QDF_STATUS status;
	hdd_context_t *hdd_ctx;
	cds_context_type *cds_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return QDF_STATUS_E_FAILURE;
	}

	cds_debug("Initializing the policy manager");

	/* init conc_connection_list */
	qdf_mem_zero(conc_connection_list, sizeof(conc_connection_list));

	sme_register_hw_mode_trans_cb(hdd_ctx->hHal,
				cds_hw_mode_transition_cb);
	status = qdf_mc_timer_init(&cds_ctx->dbs_opportunistic_timer,
				   QDF_TIMER_TYPE_SW,
				   cds_dbs_opportunistic_timer_handler,
				   (void *)cds_ctx);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		cds_err("Failed to init DBS opportunistic timer");
		return status;
	}

	status = qdf_init_connection_update();
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		cds_err("connection_update_done_evt init failed");
		return status;
	}

	cds_ctx->do_hw_mode_change = false;
	cds_ctx->sme_get_valid_channels = sme_cbacks->sme_get_valid_channels;
	cds_ctx->sme_get_nss_for_vdev = sme_cbacks->sme_get_nss_for_vdev;

	status = cds_reset_sap_mandatory_channels();
	if (QDF_IS_STATUS_ERROR(status)) {
		cds_err("failed to reset mandatory channels");
		return status;
	}

	if (wma_is_hw_dbs_2x2_capable())
		cds_get_current_pref_hw_mode_ptr =
		cds_get_current_pref_hw_mode_dbs_2x2;
	else
		cds_get_current_pref_hw_mode_ptr =
		cds_get_current_pref_hw_mode_dbs_1x1;

	if (wma_is_hw_dbs_2x2_capable())
		second_connection_pcl_dbs_table =
		&second_connection_pcl_dbs_2x2_table;
	else
		second_connection_pcl_dbs_table =
		&second_connection_pcl_dbs_1x1_table;

	if (wma_is_hw_dbs_2x2_capable())
		third_connection_pcl_dbs_table =
		&third_connection_pcl_dbs_2x2_table;
	else
		third_connection_pcl_dbs_table =
		&third_connection_pcl_dbs_1x1_table;

	if (wma_is_hw_dbs_2x2_capable())
		next_action_two_connection_table =
		&next_action_two_connection_dbs_2x2_table;
	else
		next_action_two_connection_table =
		&next_action_two_connection_dbs_1x1_table;

	if (wma_is_hw_dbs_2x2_capable())
		next_action_three_connection_table =
		&next_action_three_connection_dbs_2x2_table;
	else
		next_action_three_connection_table =
		&next_action_three_connection_dbs_1x1_table;

	return QDF_STATUS_SUCCESS;
}

/**
 * cds_get_connection_for_vdev_id() - provides the
 * perticular connection with the requested vdev id
 * @vdev_id: vdev id of the connection
 *
 * This function provides the specific connection with the
 * requested vdev id
 *
 * Return: index in the connection table
 */
static uint32_t cds_get_connection_for_vdev_id(uint32_t vdev_id)
{
	uint32_t conn_index = 0;
	cds_context_type *cds_ctx;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return conn_index;
	}
	qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
		 conn_index++) {
		if ((conc_connection_list[conn_index].vdev_id == vdev_id) &&
			conc_connection_list[conn_index].in_use) {
			break;
		}
	}
	qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
	return conn_index;
}

/**
 * cds_get_mode() - Get mode from type and subtype
 * @type: type
 * @subtype: subtype
 *
 * Get the concurrency mode from the type and subtype
 * of the interface
 *
 * Return: cds_con_mode
 */
static enum cds_con_mode cds_get_mode(uint8_t type, uint8_t subtype)
{
	enum cds_con_mode mode = CDS_MAX_NUM_OF_MODE;

	if (type == WMI_VDEV_TYPE_AP) {
		switch (subtype) {
		case 0:
			mode = CDS_SAP_MODE;
			break;
		case WMI_UNIFIED_VDEV_SUBTYPE_P2P_GO:
			mode = CDS_P2P_GO_MODE;
			break;
		default:
			cds_err("Unknown subtype %d for type %d",
				subtype, type);
			break;
		}
	} else if (type == WMI_VDEV_TYPE_STA) {
		switch (subtype) {
		case 0:
			mode = CDS_STA_MODE;
			break;
		case WMI_UNIFIED_VDEV_SUBTYPE_P2P_CLIENT:
			mode = CDS_P2P_CLIENT_MODE;
			break;
		default:
			cds_err("Unknown subtype %d for type %d",
				subtype, type);
			break;
		}
	} else if (type == WMI_VDEV_TYPE_IBSS) {
		mode = CDS_IBSS_MODE;
	} else {
		cds_err("Unknown type %d", type);
	}

	return mode;
}

/**
 * cds_get_bw() - Get channel bandwidth type used by WMI
 * @chan_width: channel bandwidth type defined by host
 *
 * Get the channel bandwidth type used by WMI
 *
 * Return: hw_mode_bandwidth
 */
static enum hw_mode_bandwidth cds_get_bw(enum phy_ch_width chan_width)
{
	enum hw_mode_bandwidth bw = HW_MODE_BW_NONE;

	switch (chan_width) {
	case CH_WIDTH_20MHZ:
		bw = HW_MODE_20_MHZ;
		break;
	case CH_WIDTH_40MHZ:
		bw = HW_MODE_40_MHZ;
		break;
	case CH_WIDTH_80MHZ:
		bw = HW_MODE_80_MHZ;
		break;
	case CH_WIDTH_160MHZ:
		bw = HW_MODE_160_MHZ;
		break;
	case CH_WIDTH_80P80MHZ:
		bw = HW_MODE_80_PLUS_80_MHZ;
		break;
	case CH_WIDTH_5MHZ:
		bw = HW_MODE_5_MHZ;
		break;
	case CH_WIDTH_10MHZ:
		bw = HW_MODE_10_MHZ;
		break;
	default:
		cds_err("Unknown channel BW type %d", chan_width);
		break;
	}

	return bw;
}

/**
 * cds_incr_connection_count() - adds the new connection to
 * the current connections list
 * @vdev_id: vdev id
 *
 *
 * This function adds the new connection to the current
 * connections list
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cds_incr_connection_count(uint32_t vdev_id)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint32_t conn_index;
	struct wma_txrx_node *wma_conn_table_entry;
	hdd_context_t *hdd_ctx;
	cds_context_type *cds_ctx;
	enum cds_chain_mode chain_mask = CDS_ONE_ONE;
	uint8_t nss_2g, nss_5g;
	enum cds_con_mode mode;
	uint8_t chan;
	uint32_t nss = 0;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return status;
	}

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return status;
	}

	conn_index = cds_get_connection_count();
	if (hdd_ctx->config->gMaxConcurrentActiveSessions < conn_index) {
		cds_err("exceeded max connection limit %d",
			hdd_ctx->config->gMaxConcurrentActiveSessions);
		return status;
	}

	wma_conn_table_entry = wma_get_interface_by_vdev_id(vdev_id);

	if (NULL == wma_conn_table_entry) {
		cds_err("can't find vdev_id %d in WMA table", vdev_id);
		return status;
	}
	mode = cds_get_mode(wma_conn_table_entry->type,
					wma_conn_table_entry->sub_type);
	chan = cds_freq_to_chan(wma_conn_table_entry->mhz);
	status = cds_get_nss_for_vdev(mode, &nss_2g, &nss_5g);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		if ((CDS_IS_CHANNEL_24GHZ(chan) && (nss_2g > 1)) ||
			(CDS_IS_CHANNEL_5GHZ(chan) && (nss_5g > 1)))
			chain_mask = CDS_TWO_TWO;
		else
			chain_mask = CDS_ONE_ONE;
		nss = (CDS_IS_CHANNEL_24GHZ(chan)) ? nss_2g : nss_5g;
	} else {
		cds_err("Error in getting nss");
	}


	/* add the entry */
	cds_update_conc_list(conn_index,
			mode,
			chan,
			cds_get_bw(wma_conn_table_entry->chan_width),
			wma_conn_table_entry->mac_id,
			chain_mask,
			nss, vdev_id, true);
	cds_info("Add at idx:%d vdev %d mac=%d",
		conn_index, vdev_id,
		wma_conn_table_entry->mac_id);

	return QDF_STATUS_SUCCESS;
}

/**
 * cds_update_connection_info() - updates the existing
 * connection in the current connections list
 * @vdev_id: vdev id
 *
 *
 * This function adds the new connection to the current
 * connections list
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cds_update_connection_info(uint32_t vdev_id)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint32_t conn_index = 0;
	bool found = false;
	struct wma_txrx_node *wma_conn_table_entry;
	cds_context_type *cds_ctx;
	enum cds_chain_mode chain_mask = CDS_ONE_ONE;
	uint8_t nss_2g, nss_5g;
	enum cds_con_mode mode;
	uint8_t chan;
	uint32_t nss = 0;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return status;
	}

	qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
	while (CONC_CONNECTION_LIST_VALID_INDEX(conn_index)) {
		if (vdev_id == conc_connection_list[conn_index].vdev_id) {
			/* debug msg */
			found = true;
			break;
		}
		conn_index++;
	}
	qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
	if (!found) {
		/* err msg */
		cds_err("can't find vdev_id %d in conc_connection_list",
			vdev_id);
		return status;
	}

	wma_conn_table_entry = wma_get_interface_by_vdev_id(vdev_id);

	if (NULL == wma_conn_table_entry) {
		/* err msg*/
		cds_err("can't find vdev_id %d in WMA table", vdev_id);
		return status;
	}
	mode = cds_get_mode(wma_conn_table_entry->type,
					wma_conn_table_entry->sub_type);
	chan = cds_freq_to_chan(wma_conn_table_entry->mhz);
	status = cds_get_nss_for_vdev(mode, &nss_2g, &nss_5g);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		if ((CDS_IS_CHANNEL_24GHZ(chan) && (nss_2g > 1)) ||
			(CDS_IS_CHANNEL_5GHZ(chan) && (nss_5g > 1)))
			chain_mask = CDS_TWO_TWO;
		else
			chain_mask = CDS_ONE_ONE;
		nss = (CDS_IS_CHANNEL_24GHZ(chan)) ? nss_2g : nss_5g;
	} else {
		cds_err("Error in getting nss");
	}

	cds_debug("update PM connection table for vdev:%d", vdev_id);

	/* add the entry */
	cds_update_conc_list(conn_index,
			mode,
			chan,
			cds_get_bw(wma_conn_table_entry->chan_width),
			wma_conn_table_entry->mac_id,
			chain_mask,
			nss, vdev_id, true);
	return QDF_STATUS_SUCCESS;
}

/**
 * cds_decr_connection_count() - remove the old connection
 * from the current connections list
 * @vdev_id: vdev id of the old connection
 *
 *
 * This function removes the old connection from the current
 * connections list
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cds_decr_connection_count(uint32_t vdev_id)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint32_t conn_index = 0, next_conn_index = 0;
	bool found = false;
	cds_context_type *cds_ctx;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return status;
	}

	qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
	while (CONC_CONNECTION_LIST_VALID_INDEX(conn_index)) {
		if (vdev_id == conc_connection_list[conn_index].vdev_id) {
			/* debug msg */
			found = true;
			break;
		}
		conn_index++;
	}
	if (!found) {
		cds_err("can't find vdev_id %d in conc_connection_list",
			vdev_id);
		qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
		return status;
	}
	next_conn_index = conn_index + 1;
	while (CONC_CONNECTION_LIST_VALID_INDEX(next_conn_index)) {
		conc_connection_list[conn_index].vdev_id =
			conc_connection_list[next_conn_index].vdev_id;
		conc_connection_list[conn_index].mode =
			conc_connection_list[next_conn_index].mode;
		conc_connection_list[conn_index].mac =
			conc_connection_list[next_conn_index].mac;
		conc_connection_list[conn_index].chan =
			conc_connection_list[next_conn_index].chan;
		conc_connection_list[conn_index].bw =
			conc_connection_list[next_conn_index].bw;
		conc_connection_list[conn_index].chain_mask =
			conc_connection_list[next_conn_index].chain_mask;
		conc_connection_list[conn_index].original_nss =
			conc_connection_list[next_conn_index].original_nss;
		conc_connection_list[conn_index].in_use =
			conc_connection_list[next_conn_index].in_use;
		conn_index++;
		next_conn_index++;
	}

	/* clean up the entry */
	qdf_mem_zero(&conc_connection_list[next_conn_index - 1],
		sizeof(*conc_connection_list));
	qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
	return QDF_STATUS_SUCCESS;
}

/**
 * cds_get_sbs_channels() - provides the sbs channel(s)
 * with respect to current connection(s)
 * @channels:	the channel(s) on which current connection(s) is
 * @len:	Number of channels
 * @pcl_weight: Pointer to the weights of PCL
 * @weight_len: Max length of the weight list
 * @index: Index from which the weight list needs to be populated
 * @group_id: Next available groups for weight assignment
 * @available_5g_channels: List of available 5g channels
 * @available_5g_channels_len: Length of the 5g channels list
 * @add_5g_channels: If this flag is true append 5G channel list as well
 *
 * This function provides the channel(s) on which current
 * connection(s) is/are
 *
 * Return: QDF_STATUS
 */

static QDF_STATUS cds_get_sbs_channels(uint8_t *channels,
		uint32_t *len, uint8_t *pcl_weight, uint32_t weight_len,
		uint32_t *index, enum cds_pcl_group_id group_id,
		uint8_t *available_5g_channels,
		uint32_t available_5g_channels_len,
		bool add_5g_channels)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t conn_index = 0, num_channels = 0;
	uint32_t num_5g_channels = 0, cur_5g_channel = 0;
	uint8_t remaining_5g_Channels[QDF_MAX_NUM_CHAN] = {};
	uint32_t remaining_channel_index = 0;
	uint32_t j = 0, i = 0, weight1, weight2;

	if ((NULL == channels) || (NULL == len)) {
		cds_err("channels or len is NULL");
		status = QDF_STATUS_E_FAILURE;
		return status;
	}

	if (group_id == CDS_PCL_GROUP_ID1_ID2) {
		weight1 = WEIGHT_OF_GROUP1_PCL_CHANNELS;
		weight2 = WEIGHT_OF_GROUP2_PCL_CHANNELS;
	} else if (group_id == CDS_PCL_GROUP_ID2_ID3) {
		weight1 = WEIGHT_OF_GROUP2_PCL_CHANNELS;
		weight2 = WEIGHT_OF_GROUP3_PCL_CHANNELS;
	} else {
		weight1 = WEIGHT_OF_GROUP3_PCL_CHANNELS;
		weight2 = WEIGHT_OF_GROUP4_PCL_CHANNELS;
	}

	cds_debug("weight1=%d weight2=%d index=%d ", weight1, weight2, *index);

	while (CONC_CONNECTION_LIST_VALID_INDEX(conn_index)) {
		if ((CDS_IS_CHANNEL_5GHZ(
			conc_connection_list[conn_index].chan))
			&& (conc_connection_list[conn_index].in_use)) {
			num_5g_channels++;
			cur_5g_channel = conc_connection_list[conn_index].chan;
		}
		conn_index++;
	}

	conn_index = 0;
	if (num_5g_channels > 1) {
		/* This case we are already in SBS so return the channels */
		while (CONC_CONNECTION_LIST_VALID_INDEX(conn_index)) {
			channels[num_channels++] =
				conc_connection_list[conn_index++].chan;
			if (*index < weight_len)
				pcl_weight[(*index)++] = weight1;
		}
		*len = num_channels;
		/* fix duplicate issue later */
		if (add_5g_channels)
			for (j = 0; j < available_5g_channels_len; j++)
				remaining_5g_Channels[
				remaining_channel_index++] =
				available_5g_channels[j];
	} else {
		/* Get list of valid sbs channels for the current
		 * connected channel
		 */
		for (j = 0; j < available_5g_channels_len; j++) {
			if (CDS_IS_CHANNEL_VALID_5G_SBS(
			cur_5g_channel, available_5g_channels[j])) {
				channels[num_channels++] =
					available_5g_channels[j];
			} else {
				remaining_5g_Channels[
				remaining_channel_index++] =
				available_5g_channels[j];
				continue;
			}
			if (*index < weight_len)
				pcl_weight[(*index)++] = weight1;
		}
		*len = num_channels;
	}

	if (add_5g_channels) {
		qdf_mem_copy(channels+num_channels, remaining_5g_Channels,
			remaining_channel_index);
		*len += remaining_channel_index;
		for (i = 0; ((i < remaining_channel_index)
					&& (i < weight_len)); i++)
			pcl_weight[i] = weight2;
	}
	return status;
}


/**
 * cds_get_connection_channels() - provides the channel(s)
 * on which current connection(s) is
 * @channels:	the channel(s) on which current connection(s) is
 * @len:	Number of channels
 * @order:	no order OR 2.4 Ghz channel followed by 5 Ghz
 *	channel OR 5 Ghz channel followed by 2.4 Ghz channel
 * @skip_dfs_channel: if this flag is true then skip the dfs channel
 * @pcl_weight: Pointer to the weights of PCL
 * @weight_len: Max length of the weight list
 * @index: Index from which the weight list needs to be populated
 * @group_id: Next available groups for weight assignment
 *
 *
 * This function provides the channel(s) on which current
 * connection(s) is/are
 *
 * Return: QDF_STATUS
 */
static
QDF_STATUS cds_get_connection_channels(uint8_t *channels,
			uint32_t *len, enum cds_pcl_channel_order order,
			bool skip_dfs_channel,
			uint8_t *pcl_weight, uint32_t weight_len,
			uint32_t *index, enum cds_pcl_group_id group_id)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t conn_index = 0, num_channels = 0;
	uint32_t weight1, weight2;
	cds_context_type *cds_ctx;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return status;
	}

	if ((NULL == channels) || (NULL == len)) {
		cds_err("channels or len is NULL");
		status = QDF_STATUS_E_FAILURE;
		return status;
	}

	/* CDS_PCL_GROUP_ID1_ID2 indicates that all three weights are
	 * available for assignment. i.e., WEIGHT_OF_GROUP1_PCL_CHANNELS,
	 * WEIGHT_OF_GROUP2_PCL_CHANNELS and WEIGHT_OF_GROUP3_PCL_CHANNELS
	 * are all available. Since in this function only two weights are
	 * assigned at max, only group1 and group2 weights are considered.
	 *
	 * The other possible group id CDS_PCL_GROUP_ID2_ID3 indicates that
	 * group1 was assigned the weight WEIGHT_OF_GROUP1_PCL_CHANNELS and
	 * only weights WEIGHT_OF_GROUP2_PCL_CHANNELS and
	 * WEIGHT_OF_GROUP3_PCL_CHANNELS are available for further weight
	 * assignments.
	 *
	 * e.g., when order is CDS_PCL_ORDER_24G_THEN_5G and group id is
	 * CDS_PCL_GROUP_ID2_ID3, WEIGHT_OF_GROUP2_PCL_CHANNELS is assigned to
	 * 2.4GHz channels and the weight WEIGHT_OF_GROUP3_PCL_CHANNELS is
	 * assigned to the 5GHz channels.
	 */
	if (group_id == CDS_PCL_GROUP_ID1_ID2) {
		weight1 = WEIGHT_OF_GROUP1_PCL_CHANNELS;
		weight2 = WEIGHT_OF_GROUP2_PCL_CHANNELS;
	} else {
		weight1 = WEIGHT_OF_GROUP2_PCL_CHANNELS;
		weight2 = WEIGHT_OF_GROUP3_PCL_CHANNELS;
	}

	qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
	if (CDS_PCL_ORDER_NONE == order) {
		while (CONC_CONNECTION_LIST_VALID_INDEX(conn_index)) {
			if (skip_dfs_channel && CDS_IS_DFS_CH(
				    conc_connection_list[conn_index].chan)) {
				conn_index++;
			} else if (*index < weight_len) {
				channels[num_channels++] =
					conc_connection_list[conn_index++].chan;
				pcl_weight[(*index)++] = weight1;
			} else {
				conn_index++;
			}
		}
		*len = num_channels;
	} else if (CDS_PCL_ORDER_24G_THEN_5G == order) {
		while (CONC_CONNECTION_LIST_VALID_INDEX(conn_index)) {
			if (CDS_IS_CHANNEL_24GHZ(
				    conc_connection_list[conn_index].chan)
				&& (*index < weight_len)) {
				channels[num_channels++] =
					conc_connection_list[conn_index++].chan;
				pcl_weight[(*index)++] = weight1;
			} else {
				conn_index++;
			}
		}
		conn_index = 0;
		while (CONC_CONNECTION_LIST_VALID_INDEX(conn_index)) {
			if (skip_dfs_channel && CDS_IS_DFS_CH(
				    conc_connection_list[conn_index].chan)) {
				conn_index++;
			} else if (CDS_IS_CHANNEL_5GHZ(
				    conc_connection_list[conn_index].chan)
				&& (*index < weight_len)) {
				channels[num_channels++] =
					conc_connection_list[conn_index++].chan;
				pcl_weight[(*index)++] = weight2;
			} else {
				conn_index++;
			}
		}
		*len = num_channels;
	} else if (CDS_PCL_ORDER_5G_THEN_2G == order) {
		while (CONC_CONNECTION_LIST_VALID_INDEX(conn_index)) {
			if (skip_dfs_channel && CDS_IS_DFS_CH(
				conc_connection_list[conn_index].chan)) {
				conn_index++;
			} else if (CDS_IS_CHANNEL_5GHZ(
				    conc_connection_list[conn_index].chan)
				&& (*index < weight_len)) {
				channels[num_channels++] =
					conc_connection_list[conn_index++].chan;
				pcl_weight[(*index)++] = weight1;
			} else {
				conn_index++;
			}
		}
		conn_index = 0;
		while (CONC_CONNECTION_LIST_VALID_INDEX(conn_index)) {
			if (CDS_IS_CHANNEL_24GHZ(
				    conc_connection_list[conn_index].chan)
				&& (*index < weight_len)) {
				channels[num_channels++] =
					conc_connection_list[conn_index++].chan;
				pcl_weight[(*index)++] = weight2;

			} else {
				conn_index++;
			}
		}
		*len = num_channels;
	} else {
		cds_err("unknown order %d", order);
		status = QDF_STATUS_E_FAILURE;
	}
	qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);

	return status;
}

/**
 * cds_update_with_safe_channel_list() - provides the safe
 * channel list
 * @pcl_channels: channel list
 * @len: length of the list
 * @weight_list: Weights of the PCL
 * @weight_len: Max length of the weights list
 *
 * This function provides the safe channel list from the list
 * provided after consulting the channel avoidance list
 *
 * Return: None
 */
void cds_update_with_safe_channel_list(uint8_t *pcl_channels, uint32_t *len,
				uint8_t *weight_list, uint32_t weight_len)
{
	uint16_t unsafe_channel_list[QDF_MAX_NUM_CHAN];
	uint8_t current_channel_list[QDF_MAX_NUM_CHAN];
	uint8_t org_weight_list[QDF_MAX_NUM_CHAN];
	uint16_t unsafe_channel_count = 0;
	uint8_t is_unsafe = 1;
	uint8_t i, j;
	uint32_t safe_channel_count = 0, current_channel_count = 0;
	qdf_device_t qdf_ctx = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);

	if (!qdf_ctx) {
		cds_err("qdf_ctx is NULL");
		return;
	}

	if (len) {
		current_channel_count = QDF_MIN(*len, QDF_MAX_NUM_CHAN);
	} else {
		cds_err("invalid number of channel length");
		return;
	}

	pld_get_wlan_unsafe_channel(qdf_ctx->dev,
				    unsafe_channel_list,
				     &unsafe_channel_count,
				     sizeof(unsafe_channel_list));

	if (unsafe_channel_count == 0)
		cds_notice("There are no unsafe channels");

	if (unsafe_channel_count) {
		qdf_mem_copy(current_channel_list, pcl_channels,
			current_channel_count);
		qdf_mem_zero(pcl_channels,
			sizeof(*pcl_channels)*current_channel_count);

		qdf_mem_copy(org_weight_list, weight_list, QDF_MAX_NUM_CHAN);
		qdf_mem_zero(weight_list, weight_len);

		for (i = 0; i < current_channel_count; i++) {
			is_unsafe = 0;
			for (j = 0; j < unsafe_channel_count; j++) {
				if (current_channel_list[i] ==
					unsafe_channel_list[j]) {
					/* Found unsafe channel, update it */
					is_unsafe = 1;
					cds_warn("CH %d is not safe",
						current_channel_list[i]);
					break;
				}
			}
			if (!is_unsafe) {
				pcl_channels[safe_channel_count] =
					current_channel_list[i];
				if (safe_channel_count < weight_len)
					weight_list[safe_channel_count] =
						org_weight_list[i];
				safe_channel_count++;
			}
		}
		*len = safe_channel_count;
	}
	return;
}

/**
 * cds_get_channel_list() - provides the channel list
 * suggestion for new connection
 * @pcl:	The preferred channel list enum
 * @pcl_channels: PCL channels
 * @len: length of the PCL
 * @mode: concurrency mode for which channel list is requested
 * @pcl_weights: Weights of the PCL
 * @weight_len: Max length of the weight list
 *
 * This function provides the actual channel list based on the
 * current regulatory domain derived using preferred channel
 * list enum obtained from one of the pcl_table
 *
 * Return: Channel List
 */
static QDF_STATUS cds_get_channel_list(enum cds_pcl_type pcl,
			uint8_t *pcl_channels, uint32_t *len,
			enum cds_con_mode mode,
			uint8_t *pcl_weights, uint32_t weight_len)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint32_t num_channels = 0;
	uint32_t sbs_num_channels = 0;
	uint32_t chan_index = 0, chan_index_24 = 0, chan_index_5 = 0;
	uint8_t channel_list[QDF_MAX_NUM_CHAN] = {0};
	uint8_t channel_list_24[QDF_MAX_NUM_CHAN] = {0};
	uint8_t channel_list_5[QDF_MAX_NUM_CHAN] = {0};
	uint8_t sbs_channel_list[QDF_MAX_NUM_CHAN] = {0};
	bool skip_dfs_channel = false;
	hdd_context_t *hdd_ctx;
	uint32_t i = 0, j = 0;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return status;
	}

	if ((NULL == pcl_channels) || (NULL == len)) {
		cds_err("pcl_channels or len is NULL");
		return status;
	}

	if (CDS_MAX_PCL_TYPE == pcl) {
		/* msg */
		cds_err("pcl is invalid");
		return status;
	}

	if (CDS_NONE == pcl) {
		/* msg */
		cds_info("pcl is 0");
		return QDF_STATUS_SUCCESS;
	}
	/* get the channel list for current domain */
	status = cds_get_valid_chans(channel_list, &num_channels);
	if (QDF_IS_STATUS_ERROR(status)) {
		cds_err("Error in getting valid channels");
		return status;
	}

	/*
	 * if you have atleast one STA connection then don't fill DFS channels
	 * in the preferred channel list
	 */
	if (((mode == CDS_SAP_MODE) || (mode == CDS_P2P_GO_MODE)) &&
	    (cds_mode_specific_connection_count(CDS_STA_MODE, NULL) > 0)) {
		cds_info("STA present, skip DFS channels from pcl for SAP/Go");
		skip_dfs_channel = true;
	}

	/* Let's divide the list in 2.4 & 5 Ghz lists */
	while ((chan_index < QDF_MAX_NUM_CHAN) &&
		(channel_list[chan_index] <= 11) &&
		(chan_index_24 < QDF_MAX_NUM_CHAN))
		channel_list_24[chan_index_24++] = channel_list[chan_index++];
	if ((chan_index < QDF_MAX_NUM_CHAN) &&
		(channel_list[chan_index] == 12) &&
		(chan_index_24 < QDF_MAX_NUM_CHAN)) {
		channel_list_24[chan_index_24++] = channel_list[chan_index++];
		if ((chan_index < QDF_MAX_NUM_CHAN) &&
			(channel_list[chan_index] == 13) &&
			(chan_index_24 < QDF_MAX_NUM_CHAN)) {
			channel_list_24[chan_index_24++] =
				channel_list[chan_index++];
			if ((chan_index < QDF_MAX_NUM_CHAN) &&
				(channel_list[chan_index] == 14) &&
				(chan_index_24 < QDF_MAX_NUM_CHAN))
				channel_list_24[chan_index_24++] =
					channel_list[chan_index++];
		}
	}

	while ((chan_index < num_channels) &&
		(chan_index_5 < QDF_MAX_NUM_CHAN)) {
		if ((true == skip_dfs_channel) &&
		    CDS_IS_DFS_CH(channel_list[chan_index])) {
			chan_index++;
			continue;
		}
		channel_list_5[chan_index_5++] = channel_list[chan_index++];
	}

	num_channels = 0;
	sbs_num_channels = 0;
	/* In the below switch case, the channel list is populated based on the
	 * pcl. e.g., if the pcl is CDS_SCC_CH_24G, the SCC channel group is
	 * populated first followed by the 2.4GHz channel group. Along with
	 * this, the weights are also populated in the same order for each of
	 * these groups. There are three weight groups:
	 * WEIGHT_OF_GROUP1_PCL_CHANNELS, WEIGHT_OF_GROUP2_PCL_CHANNELS and
	 * WEIGHT_OF_GROUP3_PCL_CHANNELS.
	 *
	 * e.g., if pcl is CDS_SCC_ON_5_SCC_ON_24_24G: scc on 5GHz (group1)
	 * channels take the weight WEIGHT_OF_GROUP1_PCL_CHANNELS, scc on 2.4GHz
	 * (group2) channels take the weight WEIGHT_OF_GROUP2_PCL_CHANNELS and
	 * 2.4GHz (group3) channels take the weight
	 * WEIGHT_OF_GROUP3_PCL_CHANNELS.
	 *
	 * When the weight to be assigned to the group is known along with the
	 * number of channels, the weights are directly assigned to the
	 * pcl_weights list. But, the channel list is populated using
	 * cds_get_connection_channels(), the order of weights to be used is
	 * passed as an argument to the function cds_get_connection_channels()
	 * using 'enum cds_pcl_group_id' which indicates the next available
	 * weights to be used and cds_get_connection_channels() will take care
	 * of the weight assignments.
	 *
	 * e.g., 'enum cds_pcl_group_id' value of CDS_PCL_GROUP_ID2_ID3
	 * indicates that the next available groups for weight assignment are
	 * WEIGHT_OF_GROUP2_PCL_CHANNELS and WEIGHT_OF_GROUP3_PCL_CHANNELS and
	 * that the weight WEIGHT_OF_GROUP1_PCL_CHANNELS was already allocated.
	 * So, in the same example, when order is CDS_PCL_ORDER_24G_THEN_5G,
	 * cds_get_connection_channels() will assign the weight
	 * WEIGHT_OF_GROUP2_PCL_CHANNELS to 2.4GHz channels and assign the
	 * weight WEIGHT_OF_GROUP3_PCL_CHANNELS to 5GHz channels.
	 */
	switch (pcl) {
	case CDS_24G:
		chan_index_24 = QDF_MIN(chan_index_24, weight_len);
		qdf_mem_copy(pcl_channels, channel_list_24,
			chan_index_24);
		*len = chan_index_24;
		for (i = 0; i < *len; i++)
			pcl_weights[i] = WEIGHT_OF_GROUP1_PCL_CHANNELS;
		status = QDF_STATUS_SUCCESS;
		break;
	case CDS_5G:
		chan_index_5 = QDF_MIN(chan_index_5, weight_len);
		qdf_mem_copy(pcl_channels, channel_list_5,
			chan_index_5);
		*len = chan_index_5;
		for (i = 0; i < *len; i++)
			pcl_weights[i] = WEIGHT_OF_GROUP1_PCL_CHANNELS;
		status = QDF_STATUS_SUCCESS;
		break;
	case CDS_SCC_CH:
	case CDS_MCC_CH:
		cds_get_connection_channels(
			channel_list, &num_channels, CDS_PCL_ORDER_NONE,
			skip_dfs_channel, pcl_weights, weight_len, &i,
			CDS_PCL_GROUP_ID1_ID2);
		qdf_mem_copy(pcl_channels, channel_list, num_channels);
		*len = num_channels;
		status = QDF_STATUS_SUCCESS;
		break;
	case CDS_SCC_CH_24G:
	case CDS_MCC_CH_24G:
		cds_get_connection_channels(
			channel_list, &num_channels, CDS_PCL_ORDER_NONE,
			skip_dfs_channel, pcl_weights, weight_len, &i,
			CDS_PCL_GROUP_ID1_ID2);
		qdf_mem_copy(pcl_channels, channel_list, num_channels);
		*len = num_channels;
		chan_index_24 = QDF_MIN((num_channels + chan_index_24),
					weight_len) - num_channels;
		qdf_mem_copy(&pcl_channels[num_channels],
			channel_list_24, chan_index_24);
		*len += chan_index_24;
		for (j = 0; j < chan_index_24; i++, j++)
			pcl_weights[i] = WEIGHT_OF_GROUP2_PCL_CHANNELS;

		status = QDF_STATUS_SUCCESS;
		break;
	case CDS_SCC_CH_5G:
	case CDS_MCC_CH_5G:
		cds_get_connection_channels(
			channel_list, &num_channels, CDS_PCL_ORDER_NONE,
			skip_dfs_channel, pcl_weights, weight_len, &i,
			CDS_PCL_GROUP_ID1_ID2);
		qdf_mem_copy(pcl_channels, channel_list,
			num_channels);
		*len = num_channels;
		chan_index_5 = QDF_MIN((num_channels + chan_index_5),
					weight_len) - num_channels;
		qdf_mem_copy(&pcl_channels[num_channels],
			channel_list_5, chan_index_5);
		*len += chan_index_5;
		for (j = 0; j < chan_index_5; i++, j++)
			pcl_weights[i] = WEIGHT_OF_GROUP2_PCL_CHANNELS;
		status = QDF_STATUS_SUCCESS;
		break;
	case CDS_24G_SCC_CH:
	case CDS_24G_MCC_CH:
		chan_index_24 = QDF_MIN(chan_index_24, weight_len);
		qdf_mem_copy(pcl_channels, channel_list_24,
			chan_index_24);
		*len = chan_index_24;
		for (i = 0; i < chan_index_24; i++)
			pcl_weights[i] = WEIGHT_OF_GROUP1_PCL_CHANNELS;
		cds_get_connection_channels(
			channel_list, &num_channels, CDS_PCL_ORDER_NONE,
			skip_dfs_channel, pcl_weights, weight_len, &i,
			CDS_PCL_GROUP_ID2_ID3);
		qdf_mem_copy(&pcl_channels[chan_index_24],
			channel_list, num_channels);
		*len += num_channels;
		status = QDF_STATUS_SUCCESS;
		break;
	case CDS_5G_SCC_CH:
	case CDS_5G_MCC_CH:
		chan_index_5 = QDF_MIN(chan_index_5, weight_len);
		qdf_mem_copy(pcl_channels, channel_list_5,
			chan_index_5);
		*len = chan_index_5;
		for (i = 0; i < chan_index_5; i++)
			pcl_weights[i] = WEIGHT_OF_GROUP1_PCL_CHANNELS;
		cds_get_connection_channels(
			channel_list, &num_channels, CDS_PCL_ORDER_NONE,
			skip_dfs_channel, pcl_weights, weight_len, &i,
			CDS_PCL_GROUP_ID2_ID3);
		qdf_mem_copy(&pcl_channels[chan_index_5],
			channel_list, num_channels);
		*len += num_channels;
		status = QDF_STATUS_SUCCESS;
		break;
	case CDS_SCC_ON_24_SCC_ON_5:
		cds_get_connection_channels(
			channel_list, &num_channels, CDS_PCL_ORDER_24G_THEN_5G,
			skip_dfs_channel, pcl_weights, weight_len, &i,
			CDS_PCL_GROUP_ID1_ID2);
		qdf_mem_copy(pcl_channels, channel_list,
			num_channels);
		*len = num_channels;
		status = QDF_STATUS_SUCCESS;
		break;
	case CDS_SCC_ON_5_SCC_ON_24:
		cds_get_connection_channels(
			channel_list, &num_channels, CDS_PCL_ORDER_5G_THEN_2G,
			skip_dfs_channel, pcl_weights, weight_len, &i,
			CDS_PCL_GROUP_ID1_ID2);
		qdf_mem_copy(pcl_channels, channel_list, num_channels);
		*len = num_channels;
		status = QDF_STATUS_SUCCESS;
		break;
	case CDS_SCC_ON_24_SCC_ON_5_24G:
		cds_get_connection_channels(
			channel_list, &num_channels, CDS_PCL_ORDER_24G_THEN_5G,
			skip_dfs_channel, pcl_weights, weight_len, &i,
			CDS_PCL_GROUP_ID1_ID2);
		qdf_mem_copy(pcl_channels, channel_list, num_channels);
		*len = num_channels;
		chan_index_24 = QDF_MIN((num_channels + chan_index_24),
					weight_len) - num_channels;
		qdf_mem_copy(&pcl_channels[num_channels],
			channel_list_24, chan_index_24);
		*len += chan_index_24;
		for (j = 0; j < chan_index_24; i++, j++)
			pcl_weights[i] = WEIGHT_OF_GROUP3_PCL_CHANNELS;
		status = QDF_STATUS_SUCCESS;
		break;
	case CDS_SCC_ON_24_SCC_ON_5_5G:
		cds_get_connection_channels(
			channel_list, &num_channels, CDS_PCL_ORDER_24G_THEN_5G,
			skip_dfs_channel, pcl_weights, weight_len, &i,
			CDS_PCL_GROUP_ID1_ID2);
		qdf_mem_copy(pcl_channels, channel_list, num_channels);
		*len = num_channels;
		chan_index_5 = QDF_MIN((num_channels + chan_index_5),
					weight_len) - num_channels;
		qdf_mem_copy(&pcl_channels[num_channels],
			channel_list_5, chan_index_5);
		*len += chan_index_5;
		for (j = 0; j < chan_index_5; i++, j++)
			pcl_weights[i] = WEIGHT_OF_GROUP3_PCL_CHANNELS;
		status = QDF_STATUS_SUCCESS;
		break;
	case CDS_SCC_ON_5_SCC_ON_24_24G:
		cds_get_connection_channels(
			channel_list, &num_channels, CDS_PCL_ORDER_5G_THEN_2G,
			skip_dfs_channel, pcl_weights, weight_len, &i,
			CDS_PCL_GROUP_ID1_ID2);
		qdf_mem_copy(pcl_channels, channel_list, num_channels);
		*len = num_channels;
		chan_index_24 = QDF_MIN((num_channels + chan_index_24),
					weight_len) - num_channels;
		qdf_mem_copy(&pcl_channels[num_channels],
			channel_list_24, chan_index_24);
		*len += chan_index_24;
		for (j = 0; j < chan_index_24; i++, j++)
			pcl_weights[i] = WEIGHT_OF_GROUP3_PCL_CHANNELS;
		status = QDF_STATUS_SUCCESS;
		break;
	case CDS_SCC_ON_5_SCC_ON_24_5G:
		cds_get_connection_channels(
			channel_list, &num_channels, CDS_PCL_ORDER_5G_THEN_2G,
			skip_dfs_channel, pcl_weights, weight_len, &i,
			CDS_PCL_GROUP_ID1_ID2);
		qdf_mem_copy(pcl_channels, channel_list, num_channels);
		*len = num_channels;
		chan_index_5 = QDF_MIN((num_channels + chan_index_5),
					weight_len) - num_channels;
		qdf_mem_copy(&pcl_channels[num_channels],
			channel_list_5, chan_index_5);
		*len += chan_index_5;
		for (j = 0; j < chan_index_5; i++, j++)
			pcl_weights[i] = WEIGHT_OF_GROUP3_PCL_CHANNELS;
		status = QDF_STATUS_SUCCESS;
		break;
	case CDS_24G_SCC_CH_SBS_CH:
		qdf_mem_copy(pcl_channels, channel_list_24,
			chan_index_24);
		*len = chan_index_24;
		for (i = 0; ((i < chan_index_24) && (i < weight_len)); i++)
			pcl_weights[i] = WEIGHT_OF_GROUP1_PCL_CHANNELS;
		cds_get_connection_channels(
			channel_list, &num_channels, CDS_PCL_ORDER_NONE,
			skip_dfs_channel, pcl_weights, weight_len, &i,
			CDS_PCL_GROUP_ID2_ID3);
		qdf_mem_copy(&pcl_channels[chan_index_24],
			channel_list, num_channels);
		*len += num_channels;
		if (wma_is_hw_sbs_capable()) {
			cds_get_sbs_channels(
			sbs_channel_list, &sbs_num_channels, pcl_weights,
			weight_len, &i, CDS_PCL_GROUP_ID3_ID4,
			channel_list_5, chan_index_5, false);
			qdf_mem_copy(
				&pcl_channels[chan_index_24 + num_channels],
				sbs_channel_list, sbs_num_channels);
			*len += sbs_num_channels;
		}
		status = QDF_STATUS_SUCCESS;
		break;
	case CDS_24G_SCC_CH_SBS_CH_5G:
		qdf_mem_copy(pcl_channels, channel_list_24,
			chan_index_24);
		*len = chan_index_24;
		for (i = 0; ((i < chan_index_24) && (i < weight_len)); i++)
			pcl_weights[i] = WEIGHT_OF_GROUP1_PCL_CHANNELS;
		cds_get_connection_channels(
			channel_list, &num_channels, CDS_PCL_ORDER_NONE,
			skip_dfs_channel, pcl_weights, weight_len, &i,
			CDS_PCL_GROUP_ID2_ID3);
		qdf_mem_copy(&pcl_channels[chan_index_24],
			channel_list, num_channels);
		*len += num_channels;
		if (wma_is_hw_sbs_capable()) {
			cds_get_sbs_channels(
			sbs_channel_list, &sbs_num_channels, pcl_weights,
			weight_len, &i, CDS_PCL_GROUP_ID3_ID4,
			channel_list_5, chan_index_5, true);
			qdf_mem_copy(
				&pcl_channels[chan_index_24 + num_channels],
				sbs_channel_list, sbs_num_channels);
			*len += sbs_num_channels;
		} else {
			qdf_mem_copy(
				&pcl_channels[chan_index_24 + num_channels],
				channel_list_5, chan_index_5);
			*len += chan_index_5;
			for (i = chan_index_24 + num_channels;
				((i < *len) && (i < weight_len)); i++)
				pcl_weights[i] = WEIGHT_OF_GROUP3_PCL_CHANNELS;
		}
		status = QDF_STATUS_SUCCESS;
		break;
	case CDS_24G_SBS_CH_MCC_CH:
		qdf_mem_copy(pcl_channels, channel_list_24,
			chan_index_24);
		*len = chan_index_24;
		for (i = 0; ((i < chan_index_24) && (i < weight_len)); i++)
			pcl_weights[i] = WEIGHT_OF_GROUP1_PCL_CHANNELS;
		if (wma_is_hw_sbs_capable()) {
			cds_get_sbs_channels(
			sbs_channel_list, &sbs_num_channels, pcl_weights,
			weight_len, &i, CDS_PCL_GROUP_ID2_ID3,
			channel_list_5, chan_index_5, false);
			qdf_mem_copy(&pcl_channels[num_channels],
			sbs_channel_list, sbs_num_channels);
			*len += sbs_num_channels;
		}
		cds_get_connection_channels(
			channel_list, &num_channels, CDS_PCL_ORDER_NONE,
			skip_dfs_channel, pcl_weights, weight_len, &i,
			CDS_PCL_GROUP_ID2_ID3);
		qdf_mem_copy(&pcl_channels[chan_index_24],
			channel_list, num_channels);
		*len += num_channels;
		status = QDF_STATUS_SUCCESS;
		break;
	case CDS_SBS_CH_5G:
		if (wma_is_hw_sbs_capable()) {
			cds_get_sbs_channels(
			sbs_channel_list, &sbs_num_channels, pcl_weights,
			weight_len, &i, CDS_PCL_GROUP_ID1_ID2,
			channel_list_5, chan_index_5, true);
			qdf_mem_copy(&pcl_channels[num_channels],
			sbs_channel_list, sbs_num_channels);
			*len += sbs_num_channels;
		} else {
			qdf_mem_copy(pcl_channels, channel_list_5,
			chan_index_5);
			*len = chan_index_5;
			for (i = 0; ((i < *len) && (i < weight_len)); i++)
				pcl_weights[i] = WEIGHT_OF_GROUP1_PCL_CHANNELS;
		}
		status = QDF_STATUS_SUCCESS;
		break;
	default:
		cds_err("unknown pcl value %d", pcl);
		break;
	}

	if ((*len != 0) && (*len != i))
		cds_info("pcl len (%d) and weight list len mismatch (%d)",
			*len, i);

	/* check the channel avoidance list */
	cds_update_with_safe_channel_list(pcl_channels, len,
				pcl_weights, weight_len);

	return status;
}

/**
 * cds_map_concurrency_mode() - to map concurrency mode between sme and hdd
 * @old_mode: sme provided adapter mode
 * @new_mode: hdd provided concurrency mode
 *
 * This routine will map concurrency mode between sme and hdd
 *
 * Return: true or false
 */
bool cds_map_concurrency_mode(enum tQDF_ADAPTER_MODE *old_mode,
	enum cds_con_mode *new_mode)
{
	bool status = true;

	switch (*old_mode) {

	case QDF_STA_MODE:
		*new_mode = CDS_STA_MODE;
		break;
	case QDF_SAP_MODE:
		*new_mode = CDS_SAP_MODE;
		break;
	case QDF_P2P_CLIENT_MODE:
		*new_mode = CDS_P2P_CLIENT_MODE;
		break;
	case QDF_P2P_GO_MODE:
		*new_mode = CDS_P2P_GO_MODE;
		break;
	case QDF_IBSS_MODE:
		*new_mode = CDS_IBSS_MODE;
		break;
	default:
		*new_mode = CDS_MAX_NUM_OF_MODE;
		status = false;
		break;
	}
	return status;
}

/**
 * cds_get_channel() - provide channel number of given mode and vdevid
 * @mode: given CDS mode
 * @vdev_id: pointer to vdev_id
 *
 * This API will provide channel number of matching mode and vdevid.
 * If vdev_id is NULL then it will match only mode
 * If vdev_id is not NULL the it will match both mode and vdev_id
 *
 * Return: channel number
 */
uint8_t cds_get_channel(enum cds_con_mode mode, uint32_t *vdev_id)
{
	uint32_t idx = 0;
	uint8_t chan;
	cds_context_type *cds_ctx;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return 0;
	}

	if (mode >= CDS_MAX_NUM_OF_MODE) {
		cds_err("incorrect mode");
		return 0;
	}

	for (idx = 0; idx < MAX_NUMBER_OF_CONC_CONNECTIONS; idx++) {
		qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
		if ((conc_connection_list[idx].mode == mode) &&
				(!vdev_id || (*vdev_id ==
					conc_connection_list[idx].vdev_id))
				&& conc_connection_list[idx].in_use) {
			chan =  conc_connection_list[idx].chan;
			qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
			return chan;
		}
		qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
	}
	return 0;
}

/**
 * cds_get_pcl() - provides the preferred channel list for
 * new connection
 * @mode:	Device mode
 * @pcl_channels: PCL channels
 * @len: lenght of the PCL
 * @pcl_weight: Weights of the PCL
 * @weight_len: Max length of the weights list
 *
 * This function provides the preferred channel list on which
 * policy manager wants the new connection to come up. Various
 * connection decision making entities will using this function
 * to query the PCL info
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cds_get_pcl(enum cds_con_mode mode,
			uint8_t *pcl_channels, uint32_t *len,
			uint8_t *pcl_weight, uint32_t weight_len)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint32_t num_connections = 0, i;
	enum cds_conc_priority_mode first_index = 0;
	enum cds_one_connection_mode second_index = 0;
	enum cds_two_connection_mode third_index = 0;
	enum cds_pcl_type pcl = CDS_NONE;
	enum cds_conc_priority_mode conc_system_pref = 0;
	hdd_context_t *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return status;
	}

	/* find the current connection state from conc_connection_list*/
	num_connections = cds_get_connection_count();
	cds_debug("connections:%d pref:%d requested mode:%d",
		num_connections, hdd_ctx->config->conc_system_pref, mode);

	switch (hdd_ctx->config->conc_system_pref) {
	case 0:
		conc_system_pref = CDS_THROUGHPUT;
		break;
	case 1:
		conc_system_pref = CDS_POWERSAVE;
		break;
	case 2:
		conc_system_pref = CDS_LATENCY;
		break;
	default:
		cds_err("unknown conc_system_pref value %d",
			hdd_ctx->config->conc_system_pref);
		break;
	}

	switch (num_connections) {
	case 0:
		first_index =
			cds_get_first_connection_pcl_table_index();
		pcl = first_connection_pcl_table[mode][first_index];
		break;
	case 1:
		second_index =
			cds_get_second_connection_pcl_table_index();
		if (CDS_MAX_ONE_CONNECTION_MODE == second_index) {
			cds_err("couldn't find index for 2nd connection pcl table");
			return status;
		}
		if (wma_is_hw_dbs_capable() == true) {
			pcl = (*second_connection_pcl_dbs_table)
				[second_index][mode][conc_system_pref];
		} else {
			pcl = second_connection_pcl_nodbs_table
				[second_index][mode][conc_system_pref];
		}

		break;
	case 2:
		third_index =
			cds_get_third_connection_pcl_table_index();
		if (CDS_MAX_TWO_CONNECTION_MODE == third_index) {
			cds_err("couldn't find index for 3rd connection pcl table");
			return status;
		}
		if (wma_is_hw_dbs_capable() == true) {
			pcl = (*third_connection_pcl_dbs_table)
				[third_index][mode][conc_system_pref];
		} else {
			pcl = third_connection_pcl_nodbs_table
				[third_index][mode][conc_system_pref];
		}
		break;
	default:
		cds_err("unexpected num_connections value %d",
			num_connections);
		break;
	}

	cds_debug("index1:%d index2:%d index3:%d pcl:%d dbs:%d",
		first_index, second_index, third_index,
		pcl, wma_is_hw_dbs_capable());

	/* once the PCL enum is obtained find out the exact channel list with
	 * help from sme_get_cfg_valid_channels
	 */
	status = cds_get_channel_list(pcl, pcl_channels, len, mode,
					pcl_weight, weight_len);
	if (QDF_IS_STATUS_ERROR(status)) {
		cds_err("failed to get channel list:%d", status);
		return status;
	}

	cds_debug("pcl len:%d", *len);
	for (i = 0; i < *len; i++) {
		cds_debug("chan:%d weight:%d",
				pcl_channels[i], pcl_weight[i]);
	}

	if ((mode == CDS_SAP_MODE) && cds_is_sap_mandatory_channel_set()) {
		status = cds_modify_sap_pcl_based_on_mandatory_channel(
				pcl_channels, pcl_weight, len);
		if (QDF_IS_STATUS_ERROR(status)) {
			cds_err("failed to get modified pcl for SAP");
			return status;
		}
		cds_debug("modified pcl len:%d", *len);
		for (i = 0; i < *len; i++)
			cds_debug("chan:%d weight:%d",
					pcl_channels[i], pcl_weight[i]);

	}

	return QDF_STATUS_SUCCESS;
}

/**
 * cds_disallow_mcc() - Check for mcc
 *
 * @channel: channel on which new connection is coming up
 *
 * When a new connection is about to come up check if current
 * concurrency combination including the new connection is
 * causing MCC
 *
 * Return: True/False
 */
static bool cds_disallow_mcc(uint8_t channel)
{
	uint32_t index = 0;
	bool match = false;
	cds_context_type *cds_ctx;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return match;
	}
	qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
	while (CONC_CONNECTION_LIST_VALID_INDEX(index)) {
		if (wma_is_hw_dbs_capable() == false) {
			if (conc_connection_list[index].chan !=
				channel) {
				match = true;
				break;
			}
		} else if (CDS_IS_CHANNEL_5GHZ
			(conc_connection_list[index].chan)) {
			if (conc_connection_list[index].chan != channel) {
				match = true;
				break;
			}
		}
		index++;
	}
	qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
	return match;
}

/**
 * cds_allow_new_home_channel() - Check for allowed number of
 * home channels
 * @channel: channel on which new connection is coming up
 * @num_connections: number of current connections
 *
 * When a new connection is about to come up check if current
 * concurrency combination including the new connection is
 * allowed or not based on the HW capability
 *
 * Return: True/False
 */
static bool cds_allow_new_home_channel(uint8_t channel, uint32_t num_connections)
{
	bool status = true;
	cds_context_type *cds_ctx;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return false;
	}

	qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
	if ((num_connections == 2) &&
		(conc_connection_list[0].chan != conc_connection_list[1].chan)
		&&
		(conc_connection_list[0].mac == conc_connection_list[1].mac)) {
		if (wma_is_hw_dbs_capable() == false) {
			if ((channel != conc_connection_list[0].chan) &&
				(channel != conc_connection_list[1].chan)) {
				cds_err("don't allow 3rd home channel on same MAC");
				status = false;
			}
		} else if (((CDS_IS_CHANNEL_24GHZ(channel)) &&
				(CDS_IS_CHANNEL_24GHZ
				(conc_connection_list[0].chan)) &&
				(CDS_IS_CHANNEL_24GHZ
				(conc_connection_list[1].chan))) ||
				   ((CDS_IS_CHANNEL_5GHZ(channel)) &&
				(CDS_IS_CHANNEL_5GHZ
				(conc_connection_list[0].chan)) &&
				(CDS_IS_CHANNEL_5GHZ
				(conc_connection_list[1].chan)))) {
			cds_err("don't allow 3rd home channel on same MAC");
			status = false;
		}
	}
	qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
	return status;
}

/**
 * cds_is_ibss_conn_exist() - to check if IBSS connection already present
 * @hdd_ctx: pointer to hdd context
 * @ibss_channel: pointer to ibss channel which needs to be filled
 *
 * this routine will check if IBSS connection already exist or no. If it
 * exist then this routine will return true and fill the ibss_channel value.
 *
 * Return: true if ibss connection exist else false
 */
bool cds_is_ibss_conn_exist(uint8_t *ibss_channel)
{
	uint32_t count = 0, index = 0;
	uint32_t list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	bool status = false;

	cds_context_type *cds_ctx;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return status;
	}
	if (NULL == ibss_channel) {
		cds_err("Null pointer error");
		return false;
	}
	count = cds_mode_specific_connection_count(CDS_IBSS_MODE, list);
	qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
	if (count == 0) {
		/* No IBSS connection */
		status = false;
	} else if (count == 1) {
		*ibss_channel = conc_connection_list[list[index]].chan;
		status = true;
	} else {
		*ibss_channel = conc_connection_list[list[index]].chan;
		cds_notice("Multiple IBSS connections, picking first one");
		status = true;
	}
	qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
	return status;
}

/**
 * cds_vht160_conn_exist() - to check if we have a connection
 * already using vht160 or vht80+80
 *
 * This routine will check if vht160 connection already exist or
 * no. If it exist then this routine will return true.
 *
 * Return: true if vht160 connection exist else false
 */
static bool cds_vht160_conn_exist(void)
{
	uint32_t conn_index;
	bool status = false;
	cds_context_type *cds_ctx;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return status;
	}

	qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
		conn_index++) {
		if (conc_connection_list[conn_index].in_use &&
			((conc_connection_list[conn_index].bw ==
			HW_MODE_80_PLUS_80_MHZ) ||
			(conc_connection_list[conn_index].bw ==
			HW_MODE_160_MHZ))) {
			 status = true;
			 break;
		}
	}
	qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);

	return status;
}

/**
 * cds_is_5g_channel_allowed() - check if 5g channel is allowed
 * @channel: channel number which needs to be validated
 * @list: list of existing connections.
 * @mode: mode against which channel needs to be validated
 *
 * This API takes the channel as input and compares with existing
 * connection channels. If existing connection's channel is DFS channel
 * and provided channel is 5G channel then don't allow concurrency to
 * happen as MCC with DFS channel is not yet supported
 *
 * Return: true if 5G channel is allowed, false if not allowed
 *
 */
static bool cds_is_5g_channel_allowed(uint8_t channel, uint32_t *list,
				      enum cds_con_mode mode)
{
	uint32_t index = 0, count = 0;
	cds_context_type *cds_ctx;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return false;
	}

	count = cds_mode_specific_connection_count(mode, list);
	qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
	while (index < count) {
		if (CDS_IS_DFS_CH(conc_connection_list[list[index]].chan) &&
		    CDS_IS_CHANNEL_5GHZ(channel) &&
		    (channel != conc_connection_list[list[index]].chan)) {
			qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
			cds_err("don't allow MCC if SAP/GO on DFS channel");
			return false;
		}
		index++;
	}
	qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
	return true;

}

/**
 * cds_allow_concurrency() - Check for allowed concurrency
 * combination
 * @mode:	new connection mode
 * @channel: channel on which new connection is coming up
 * @bw: Bandwidth requested by the connection (optional)
 *
 * When a new connection is about to come up check if current
 * concurrency combination including the new connection is
 * allowed or not based on the HW capability
 *
 * Return: True/False
 */
bool cds_allow_concurrency(enum cds_con_mode mode,
				uint8_t channel, enum hw_mode_bandwidth bw)
{
	uint32_t num_connections = 0, count = 0, index = 0;
	bool status = false, match = false;
	uint32_t list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	hdd_context_t *hdd_ctx;
	cds_context_type *cds_ctx;
	QDF_STATUS ret;
	struct sir_pcl_list pcl;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return status;
	}

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return status;
	}


	qdf_mem_zero(&pcl, sizeof(pcl));
	ret = cds_get_pcl(mode, pcl.pcl_list, &pcl.pcl_len,
			pcl.weight_list, QDF_ARRAY_SIZE(pcl.weight_list));
	if (QDF_IS_STATUS_ERROR(ret)) {
		cds_err("disallow connection:%d", ret);
		goto done;
	}

	/* find the current connection state from conc_connection_list*/
	num_connections = cds_get_connection_count();

	if (num_connections && cds_is_sub_20_mhz_enabled()) {
		/* dont allow concurrency if Sub 20 MHz is enabled */
		status = false;
		goto done;
	}

	if (cds_max_concurrent_connections_reached()) {
		cds_err("Reached max concurrent connections: %d",
			hdd_ctx->config->gMaxConcurrentActiveSessions);
		goto done;
	}

	if (channel) {
		/* don't allow 3rd home channel on same MAC */
		if (!cds_allow_new_home_channel(channel, num_connections))
			goto done;

		/*
		 * 1) DFS MCC is not yet supported
		 * 2) If you already have STA connection on 5G channel then
		 *    don't allow any other persona to make connection on DFS
		 *    channel because STA 5G + DFS MCC is not allowed.
		 * 3) If STA is on 2G channel and SAP is coming up on
		 *    DFS channel then allow concurrency but make sure it is
		 *    going to DBS and send PCL to firmware indicating that
		 *    don't allow STA to roam to 5G channels.
		 */
		if (!cds_is_5g_channel_allowed(channel, list, CDS_P2P_GO_MODE))
			goto done;
		if (!cds_is_5g_channel_allowed(channel, list, CDS_SAP_MODE))
			goto done;

		if ((CDS_P2P_GO_MODE == mode) || (CDS_SAP_MODE == mode)) {
			if (CDS_IS_DFS_CH(channel))
				match = cds_disallow_mcc(channel);
		}
		if (true == match) {
			cds_err("No MCC, SAP/GO about to come up on DFS channel");
			goto done;
		}
	}

	/*
	 * Check all IBSS+STA concurrencies
	 *
	 * don't allow IBSS + STA MCC
	 * don't allow IBSS + STA SCC if IBSS is on DFS channel
	 */
	count = cds_mode_specific_connection_count(CDS_STA_MODE, list);
	if ((CDS_IBSS_MODE == mode) &&
		(cds_mode_specific_connection_count(
		CDS_IBSS_MODE, list)) && count) {
		cds_err("No 2nd IBSS, we already have STA + IBSS");
		goto done;
	}
	if ((CDS_IBSS_MODE == mode) &&
		(CDS_IS_DFS_CH(channel)) && count) {
		cds_err("No IBSS + STA SCC/MCC, IBSS is on DFS channel");
		goto done;
	}
	if (CDS_IBSS_MODE == mode) {
		if (wma_is_hw_dbs_capable() == true) {
			if (num_connections > 1) {
				cds_err("No IBSS, we have concurrent connections already");
				goto done;
			}
			qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
			if (CDS_STA_MODE != conc_connection_list[0].mode) {
				cds_err("No IBSS, we've a non-STA connection");
				qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
				goto done;
			}
			/*
			 * This logic protects STA and IBSS to come up on same
			 * band. If requirement changes then this condition
			 * needs to be removed
			 */
			if (channel &&
				(conc_connection_list[0].chan != channel) &&
				CDS_IS_SAME_BAND_CHANNELS(
				conc_connection_list[0].chan, channel)) {
				qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
				cds_err("No IBSS + STA MCC");
				goto done;
			}
			qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
		} else if (num_connections) {
			cds_err("No IBSS, we have one connection already");
			goto done;
		}
	}

	if ((CDS_STA_MODE == mode) &&
		(cds_mode_specific_connection_count(
		CDS_IBSS_MODE, list)) && count) {
		cds_err("No 2nd STA, we already have STA + IBSS");
		goto done;
	}

	if ((CDS_STA_MODE == mode) &&
		(cds_mode_specific_connection_count(CDS_IBSS_MODE, list))) {
		if (wma_is_hw_dbs_capable() == true) {
			if (num_connections > 1) {
				cds_err("No 2nd STA, we already have IBSS concurrency");
				goto done;
			}
			qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
			if (channel &&
				(CDS_IS_DFS_CH(conc_connection_list[0].chan))
				&& (CDS_IS_CHANNEL_5GHZ(channel))) {
				qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
				cds_err("No IBSS + STA SCC/MCC, IBSS is on DFS channel");
				goto done;
			}
			/*
			 * This logic protects STA and IBSS to come up on same
			 * band. If requirement changes then this condition
			 * needs to be removed
			 */
			if ((conc_connection_list[0].chan != channel) &&
				CDS_IS_SAME_BAND_CHANNELS(
				conc_connection_list[0].chan, channel)) {
				cds_err("No IBSS + STA MCC");
				qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
				goto done;
			}
			qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
		} else {
			cds_err("No STA, we have IBSS connection already");
			goto done;
		}
	}

	/* don't allow two P2P GO on same band */
	if (channel && (mode == CDS_P2P_GO_MODE) && num_connections) {
		index = 0;
		count = cds_mode_specific_connection_count(
						CDS_P2P_GO_MODE, list);
		qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
		while (index < count) {
			if (CDS_IS_SAME_BAND_CHANNELS(channel,
				conc_connection_list[list[index]].chan)) {
				cds_err("Don't allow P2P GO on same band");
				qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
				goto done;
			}
			index++;
		}
		qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
	}

	/* don't allow concurrency on vht160 or vht 80+80 */
	if (num_connections &&
			((bw == HW_MODE_80_PLUS_80_MHZ) ||
				(bw == HW_MODE_160_MHZ))) {
		cds_err("No VHT160, we have one connection already");
		goto done;
	}

	if (cds_vht160_conn_exist()) {
		cds_err("VHT160/80+80 connection exists, no concurrency");
		goto done;
	}


	status = true;

done:
	return status;
}

/**
 * cds_get_first_connection_pcl_table_index() - provides the
 * row index to firstConnectionPclTable to get to the correct
 * pcl
 *
 * This function provides the row index to
 * firstConnectionPclTable. The index is the preference config.
 *
 * Return: table index
 */
enum cds_conc_priority_mode cds_get_first_connection_pcl_table_index(void)
{
	hdd_context_t *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return CDS_THROUGHPUT;
	}

	if (hdd_ctx->config->conc_system_pref >= CDS_MAX_CONC_PRIORITY_MODE)
		return CDS_THROUGHPUT;
	return hdd_ctx->config->conc_system_pref;
}

/**
 * cds_get_second_connection_pcl_table_index() - provides the
 * row index to secondConnectionPclTable to get to the correct
 * pcl
 *
 * This function provides the row index to
 * secondConnectionPclTable. The index is derived based on
 * current connection, band on which it is on & chain mask it is
 * using, as obtained from conc_connection_list.
 *
 * Return: table index
 */
enum cds_one_connection_mode cds_get_second_connection_pcl_table_index(void)
{
	enum cds_one_connection_mode index = CDS_MAX_ONE_CONNECTION_MODE;
	cds_context_type *cds_ctx;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return index;
	}

	qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
	if (CDS_STA_MODE == conc_connection_list[0].mode) {
		if (CDS_IS_CHANNEL_24GHZ(conc_connection_list[0].chan)) {
			if (CDS_ONE_ONE == conc_connection_list[0].chain_mask)
				index = CDS_STA_24_1x1;
			else
				index = CDS_STA_24_2x2;
		} else {
			if (CDS_ONE_ONE == conc_connection_list[0].chain_mask)
				index = CDS_STA_5_1x1;
			else
				index = CDS_STA_5_2x2;
		}
	} else if (CDS_SAP_MODE == conc_connection_list[0].mode) {
		if (CDS_IS_CHANNEL_24GHZ(conc_connection_list[0].chan)) {
			if (CDS_ONE_ONE == conc_connection_list[0].chain_mask)
				index = CDS_SAP_24_1x1;
			else
				index = CDS_SAP_24_2x2;
		} else {
			if (CDS_ONE_ONE == conc_connection_list[0].chain_mask)
				index = CDS_SAP_5_1x1;
			else
				index = CDS_SAP_5_2x2;
		}
	} else if (CDS_P2P_CLIENT_MODE == conc_connection_list[0].mode) {
		if (CDS_IS_CHANNEL_24GHZ(conc_connection_list[0].chan)) {
			if (CDS_ONE_ONE == conc_connection_list[0].chain_mask)
				index = CDS_P2P_CLI_24_1x1;
			else
				index = CDS_P2P_CLI_24_2x2;
		} else {
			if (CDS_ONE_ONE == conc_connection_list[0].chain_mask)
				index = CDS_P2P_CLI_5_1x1;
			else
				index = CDS_P2P_CLI_5_2x2;
		}
	} else if (CDS_P2P_GO_MODE == conc_connection_list[0].mode) {
		if (CDS_IS_CHANNEL_24GHZ(conc_connection_list[0].chan)) {
			if (CDS_ONE_ONE == conc_connection_list[0].chain_mask)
				index = CDS_P2P_GO_24_1x1;
			else
				index = CDS_P2P_GO_24_2x2;
		} else {
			if (CDS_ONE_ONE == conc_connection_list[0].chain_mask)
				index = CDS_P2P_GO_5_1x1;
			else
				index = CDS_P2P_GO_5_2x2;
		}
	} else if (CDS_IBSS_MODE == conc_connection_list[0].mode) {
		if (CDS_IS_CHANNEL_24GHZ(conc_connection_list[0].chan)) {
			if (CDS_ONE_ONE == conc_connection_list[0].chain_mask)
				index = CDS_IBSS_24_1x1;
			else
				index = CDS_IBSS_24_2x2;
		} else {
			if (CDS_ONE_ONE == conc_connection_list[0].chain_mask)
				index = CDS_IBSS_5_1x1;
			else
				index = CDS_IBSS_5_2x2;
		}
	}

	cds_debug("mode:%d chan:%d chain:%d index:%d",
		conc_connection_list[0].mode, conc_connection_list[0].chan,
		conc_connection_list[0].chain_mask, index);

	qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
	return index;
}

/**
 * cds_get_third_connection_pcl_table_index() - provides the
 * row index to thirdConnectionPclTable to get to the correct
 * pcl
 *
 * This function provides the row index to
 * thirdConnectionPclTable. The index is derived based on
 * current connection, band on which it is on & chain mask it is
 * using, as obtained from conc_connection_list.
 *
 * Return: table index
 */
enum cds_two_connection_mode cds_get_third_connection_pcl_table_index(void)
{
	enum cds_one_connection_mode index = CDS_MAX_TWO_CONNECTION_MODE;
	cds_context_type *cds_ctx;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return index;
	}

	qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
	/* P2P Client + SAP */
	if (((CDS_P2P_CLIENT_MODE == conc_connection_list[0].mode) &&
		(CDS_SAP_MODE == conc_connection_list[1].mode)) ||
		((CDS_SAP_MODE == conc_connection_list[0].mode) &&
		(CDS_P2P_CLIENT_MODE == conc_connection_list[1].mode))) {
		/* SCC */
		if (conc_connection_list[0].chan ==
			conc_connection_list[1].chan) {
			if (CDS_IS_CHANNEL_24GHZ(
				conc_connection_list[0].chan)) {
				if (CDS_ONE_ONE ==
					conc_connection_list[0].chain_mask)
					index = CDS_P2P_CLI_SAP_SCC_24_1x1;
				else
					index = CDS_P2P_CLI_SAP_SCC_24_2x2;
			} else {
				if (CDS_ONE_ONE ==
					conc_connection_list[0].chain_mask)
					index = CDS_P2P_CLI_SAP_SCC_5_1x1;
				else
					index = CDS_P2P_CLI_SAP_SCC_5_2x2;
			}
		/* MCC */
		} else if (conc_connection_list[0].mac ==
				conc_connection_list[1].mac) {
			if ((CDS_IS_CHANNEL_24GHZ
				(conc_connection_list[0].chan)) &&
				(CDS_IS_CHANNEL_24GHZ
				(conc_connection_list[1].chan))) {
				if (CDS_ONE_ONE ==
				conc_connection_list[0].chain_mask)
					index = CDS_P2P_CLI_SAP_MCC_24_1x1;
				else
					index = CDS_P2P_CLI_SAP_MCC_24_2x2;
			} else if ((CDS_IS_CHANNEL_5GHZ(
				conc_connection_list[0].chan)) &&
				(CDS_IS_CHANNEL_5GHZ(
				conc_connection_list[1].chan))) {
				if (CDS_ONE_ONE ==
				conc_connection_list[0].chain_mask)
					index = CDS_P2P_CLI_SAP_MCC_5_1x1;
				else
					index = CDS_P2P_CLI_SAP_MCC_5_2x2;
			} else {
				if (CDS_ONE_ONE ==
				conc_connection_list[0].chain_mask)
					index = CDS_P2P_CLI_SAP_MCC_24_5_1x1;
				else
					index = CDS_P2P_CLI_SAP_MCC_24_5_2x2;
			}
			/* SBS */
		} else if (conc_connection_list[0].mac !=
				conc_connection_list[1].mac) {
			if ((CDS_IS_CHANNEL_5GHZ(
				conc_connection_list[0].chan)) &&
				(CDS_IS_CHANNEL_5GHZ(
				conc_connection_list[1].chan))) {
				if (CDS_ONE_ONE ==
					conc_connection_list[0].chain_mask)
					index = CDS_P2P_CLI_SAP_SBS_5_1x1;
			}
		/* DBS */
		} else {
			if (CDS_ONE_ONE ==
				conc_connection_list[0].chain_mask) {
				index = CDS_P2P_CLI_SAP_DBS_1x1;
			} else {
				index = CDS_P2P_CLI_SAP_DBS_2x2;
			}
		}
	} else /* STA + SAP */
	if (((CDS_STA_MODE == conc_connection_list[0].mode) &&
		(CDS_SAP_MODE == conc_connection_list[1].mode)) ||
		((CDS_SAP_MODE == conc_connection_list[0].mode) &&
		(CDS_STA_MODE == conc_connection_list[1].mode))) {
		/* SCC */
		if (conc_connection_list[0].chan ==
			conc_connection_list[1].chan) {
			if (CDS_IS_CHANNEL_24GHZ(
				conc_connection_list[0].chan)) {
				if (CDS_ONE_ONE ==
					conc_connection_list[0].chain_mask)
					index = CDS_STA_SAP_SCC_24_1x1;
				else
					index = CDS_STA_SAP_SCC_24_2x2;
			} else {
				if (CDS_ONE_ONE ==
					conc_connection_list[0].chain_mask)
					index = CDS_STA_SAP_SCC_5_1x1;
				else
					index = CDS_STA_SAP_SCC_5_2x2;
			}
		/* MCC */
		} else if (conc_connection_list[0].mac ==
				conc_connection_list[1].mac) {
			if ((CDS_IS_CHANNEL_24GHZ
				(conc_connection_list[0].chan)) &&
				(CDS_IS_CHANNEL_24GHZ
				(conc_connection_list[1].chan))) {
				if (CDS_ONE_ONE ==
				conc_connection_list[0].chain_mask)
					index = CDS_STA_SAP_MCC_24_1x1;
				else
					index = CDS_STA_SAP_MCC_24_2x2;
			} else if ((CDS_IS_CHANNEL_5GHZ(
				conc_connection_list[0].chan)) &&
				(CDS_IS_CHANNEL_5GHZ(
				conc_connection_list[1].chan))) {
				if (CDS_ONE_ONE ==
				conc_connection_list[0].chain_mask)
					index = CDS_STA_SAP_MCC_5_1x1;
				else
					index = CDS_STA_SAP_MCC_5_2x2;
			} else {
				if (CDS_ONE_ONE ==
				conc_connection_list[0].chain_mask)
					index = CDS_STA_SAP_MCC_24_5_1x1;
				else
					index = CDS_STA_SAP_MCC_24_5_2x2;
			}
			/* SBS */
		} else if (conc_connection_list[0].mac !=
				conc_connection_list[1].mac) {
			if ((CDS_IS_CHANNEL_5GHZ(
				conc_connection_list[0].chan)) &&
				(CDS_IS_CHANNEL_5GHZ(
				conc_connection_list[1].chan))) {
				if (CDS_ONE_ONE ==
					conc_connection_list[0].chain_mask)
					index = CDS_STA_SAP_SBS_5_1x1;
			}
		/* DBS */
		} else {
			if (CDS_ONE_ONE ==
				conc_connection_list[0].chain_mask) {
				index = CDS_STA_SAP_DBS_1x1;
			} else {
				index = CDS_STA_SAP_DBS_2x2;
			}
		}
	} else /* SAP + SAP */
	if ((CDS_SAP_MODE == conc_connection_list[0].mode) &&
		(CDS_SAP_MODE == conc_connection_list[1].mode)) {
		/* SCC */
		if (conc_connection_list[0].chan ==
			conc_connection_list[1].chan) {
			if (CDS_IS_CHANNEL_24GHZ(
				conc_connection_list[0].chan)) {
				if (CDS_ONE_ONE ==
					conc_connection_list[0].chain_mask)
					index = CDS_SAP_SAP_SCC_24_1x1;
				else
					index = CDS_SAP_SAP_SCC_24_2x2;
			} else {
				if (CDS_ONE_ONE ==
					conc_connection_list[0].chain_mask)
					index = CDS_SAP_SAP_SCC_5_1x1;
				else
					index = CDS_SAP_SAP_SCC_5_2x2;
			}
		/* MCC */
		} else if (conc_connection_list[0].mac ==
				conc_connection_list[1].mac) {
			if ((CDS_IS_CHANNEL_24GHZ
				(conc_connection_list[0].chan)) &&
				(CDS_IS_CHANNEL_24GHZ
				(conc_connection_list[1].chan))) {
				if (CDS_ONE_ONE ==
				conc_connection_list[0].chain_mask)
					index = CDS_SAP_SAP_MCC_24_1x1;
				else
					index = CDS_SAP_SAP_MCC_24_2x2;
			} else if ((CDS_IS_CHANNEL_5GHZ(
				conc_connection_list[0].chan)) &&
				(CDS_IS_CHANNEL_5GHZ(
				conc_connection_list[1].chan))) {
				if (CDS_ONE_ONE ==
				conc_connection_list[0].chain_mask)
					index = CDS_SAP_SAP_MCC_5_1x1;
				else
					index = CDS_SAP_SAP_MCC_5_2x2;
			} else {
				if (CDS_ONE_ONE ==
				conc_connection_list[0].chain_mask)
					index = CDS_SAP_SAP_MCC_24_5_1x1;
				else
					index = CDS_SAP_SAP_MCC_24_5_2x2;
			}
			/* SBS */
		} else if (conc_connection_list[0].mac !=
				conc_connection_list[1].mac) {
			if ((CDS_IS_CHANNEL_5GHZ(
				conc_connection_list[0].chan)) &&
				(CDS_IS_CHANNEL_5GHZ(
				conc_connection_list[1].chan))) {
				if (CDS_ONE_ONE ==
					conc_connection_list[0].chain_mask)
					index = CDS_SAP_SAP_SBS_5_1x1;
			}
		/* DBS */
		} else {
			if (CDS_ONE_ONE ==
				conc_connection_list[0].chain_mask) {
				index = CDS_SAP_SAP_DBS_1x1;
			} else {
				index = CDS_SAP_SAP_DBS_2x2;
			}
		}
	} else    /* STA + P2P GO */
	if (((CDS_STA_MODE == conc_connection_list[0].mode) &&
		(CDS_P2P_GO_MODE == conc_connection_list[1].mode)) ||
		((CDS_P2P_GO_MODE == conc_connection_list[0].mode) &&
		(CDS_STA_MODE == conc_connection_list[1].mode))) {
		/* SCC */
		if (conc_connection_list[0].chan ==
		conc_connection_list[1].chan) {
			if (CDS_IS_CHANNEL_24GHZ
				(conc_connection_list[0].chan)) {
				if (CDS_ONE_ONE ==
				conc_connection_list[0].chain_mask)
					index = CDS_STA_P2P_GO_SCC_24_1x1;
				else
					index = CDS_STA_P2P_GO_SCC_24_2x2;
			} else {
				if (CDS_ONE_ONE ==
				conc_connection_list[0].chain_mask)
					index = CDS_STA_P2P_GO_SCC_5_1x1;
				else
					index = CDS_STA_P2P_GO_SCC_5_2x2;
			}
		/* MCC */
		} else if (conc_connection_list[0].mac ==
			conc_connection_list[1].mac) {
			if ((CDS_IS_CHANNEL_24GHZ(
				conc_connection_list[0].chan)) &&
				(CDS_IS_CHANNEL_24GHZ
				(conc_connection_list[1].chan))) {
				if (CDS_ONE_ONE ==
					conc_connection_list[0].chain_mask)
					index = CDS_STA_P2P_GO_MCC_24_1x1;
				else
					index = CDS_STA_P2P_GO_MCC_24_2x2;
			} else if ((CDS_IS_CHANNEL_5GHZ(
				conc_connection_list[0].chan)) &&
				(CDS_IS_CHANNEL_5GHZ(
				conc_connection_list[1].chan))) {
				if (CDS_ONE_ONE ==
					conc_connection_list[0].chain_mask)
					index = CDS_STA_P2P_GO_MCC_5_1x1;
				else
					index = CDS_STA_P2P_GO_MCC_5_2x2;
			} else {
				if (CDS_ONE_ONE ==
				conc_connection_list[0].chain_mask)
					index = CDS_STA_P2P_GO_MCC_24_5_1x1;
				else
					index = CDS_STA_P2P_GO_MCC_24_5_2x2;
			}
			/* SBS */
		} else if (conc_connection_list[0].mac !=
				conc_connection_list[1].mac) {
			if ((CDS_IS_CHANNEL_5GHZ(
				conc_connection_list[0].chan)) &&
				(CDS_IS_CHANNEL_5GHZ(
				conc_connection_list[1].chan))) {
				if (CDS_ONE_ONE ==
					conc_connection_list[0].chain_mask)
					index = CDS_STA_P2P_GO_SBS_5_1x1;
			}
		/* DBS */
		} else {
			if (CDS_ONE_ONE ==
				conc_connection_list[0].chain_mask) {
				index = CDS_STA_P2P_GO_DBS_1x1;
			} else {
				index = CDS_STA_P2P_GO_DBS_2x2;
			}
		}
	} else    /* STA + P2P CLI */
	if (((CDS_STA_MODE == conc_connection_list[0].mode) &&
		(CDS_P2P_CLIENT_MODE == conc_connection_list[1].mode)) ||
		((CDS_P2P_CLIENT_MODE == conc_connection_list[0].mode) &&
		(CDS_STA_MODE == conc_connection_list[1].mode))) {
		/* SCC */
		if (conc_connection_list[0].chan ==
		conc_connection_list[1].chan) {
			if (CDS_IS_CHANNEL_24GHZ
				(conc_connection_list[0].chan)) {
				if (CDS_ONE_ONE ==
				conc_connection_list[0].chain_mask)
					index = CDS_STA_P2P_CLI_SCC_24_1x1;
				else
					index = CDS_STA_P2P_CLI_SCC_24_2x2;
			} else {
				if (CDS_ONE_ONE ==
				conc_connection_list[0].chain_mask)
					index = CDS_STA_P2P_CLI_SCC_5_1x1;
				else
					index = CDS_STA_P2P_CLI_SCC_5_2x2;
			}
		/* MCC */
		} else if (conc_connection_list[0].mac ==
			conc_connection_list[1].mac) {
			if ((CDS_IS_CHANNEL_24GHZ(
				conc_connection_list[0].chan)) &&
				(CDS_IS_CHANNEL_24GHZ(
				conc_connection_list[1].chan))) {
				if (CDS_ONE_ONE ==
					conc_connection_list[0].chain_mask)
					index = CDS_STA_P2P_CLI_MCC_24_1x1;
				else
					index = CDS_STA_P2P_CLI_MCC_24_2x2;
			} else if ((CDS_IS_CHANNEL_5GHZ(
				conc_connection_list[0].chan)) &&
				(CDS_IS_CHANNEL_5GHZ(
				conc_connection_list[1].chan))) {
				if (CDS_ONE_ONE ==
					conc_connection_list[0].chain_mask)
					index = CDS_STA_P2P_CLI_MCC_5_1x1;
				else
					index = CDS_STA_P2P_CLI_MCC_5_2x2;
			} else {
				if (CDS_ONE_ONE ==
					conc_connection_list[0].chain_mask)
					index = CDS_STA_P2P_CLI_MCC_24_5_1x1;
				else
					index = CDS_STA_P2P_CLI_MCC_24_5_2x2;
			}
			/* SBS */
		} else if (conc_connection_list[0].mac !=
				conc_connection_list[1].mac) {
			if ((CDS_IS_CHANNEL_5GHZ(
				conc_connection_list[0].chan)) &&
				(CDS_IS_CHANNEL_5GHZ(
				conc_connection_list[1].chan))) {
				if (CDS_ONE_ONE ==
					conc_connection_list[0].chain_mask)
					index = CDS_STA_P2P_CLI_SBS_5_1x1;
			}
		/* DBS */
		} else {
			if (CDS_ONE_ONE ==
				conc_connection_list[0].chain_mask) {
				index = CDS_STA_P2P_CLI_DBS_1x1;
			} else {
				index = CDS_STA_P2P_CLI_DBS_2x2;
			}
		}
	} else    /* P2P GO + P2P CLI */
	if (((CDS_P2P_GO_MODE == conc_connection_list[0].mode) &&
		(CDS_P2P_CLIENT_MODE == conc_connection_list[1].mode)) ||
		((CDS_P2P_CLIENT_MODE == conc_connection_list[0].mode) &&
		(CDS_P2P_GO_MODE == conc_connection_list[1].mode))) {
		/* SCC */
		if (conc_connection_list[0].chan ==
			conc_connection_list[1].chan) {
			if (CDS_IS_CHANNEL_24GHZ(
				conc_connection_list[0].chan)) {
				if (CDS_ONE_ONE ==
					conc_connection_list[0].chain_mask)
					index = CDS_P2P_GO_P2P_CLI_SCC_24_1x1;
				else
					index = CDS_P2P_GO_P2P_CLI_SCC_24_2x2;
			} else {
				if (CDS_ONE_ONE ==
					conc_connection_list[0].chain_mask)
					index = CDS_P2P_GO_P2P_CLI_SCC_5_1x1;
				else
					index = CDS_P2P_GO_P2P_CLI_SCC_5_2x2;
			}
		/* MCC */
		} else if (conc_connection_list[0].mac ==
			conc_connection_list[1].mac) {
			if ((CDS_IS_CHANNEL_24GHZ(
			conc_connection_list[0].chan)) &&
			(CDS_IS_CHANNEL_24GHZ(
			conc_connection_list[1].chan))) {
				if (CDS_ONE_ONE ==
					conc_connection_list[0].chain_mask)
					index = CDS_P2P_GO_P2P_CLI_MCC_24_1x1;
				else
					index = CDS_P2P_GO_P2P_CLI_MCC_24_2x2;
			} else if ((CDS_IS_CHANNEL_5GHZ(
				conc_connection_list[0].chan)) &&
				(CDS_IS_CHANNEL_5GHZ(
				conc_connection_list[1].chan))) {
				if (CDS_ONE_ONE ==
					conc_connection_list[0].chain_mask)
					index = CDS_P2P_GO_P2P_CLI_MCC_5_1x1;
				else
					index = CDS_P2P_GO_P2P_CLI_MCC_5_2x2;
			} else {
				if (CDS_ONE_ONE ==
					conc_connection_list[0].chain_mask)
					index = CDS_P2P_GO_P2P_CLI_MCC_24_5_1x1;
				else
					index = CDS_P2P_GO_P2P_CLI_MCC_24_5_2x2;
			}
			/* SBS */
		} else if (conc_connection_list[0].mac !=
				conc_connection_list[1].mac) {
			if ((CDS_IS_CHANNEL_5GHZ(
				conc_connection_list[0].chan)) &&
				(CDS_IS_CHANNEL_5GHZ(
				conc_connection_list[1].chan))) {
				if (CDS_ONE_ONE ==
					conc_connection_list[0].chain_mask)
					index = CDS_P2P_GO_P2P_CLI_SBS_5_1x1;
			}
		/* DBS */
		} else {
			if (CDS_ONE_ONE ==
				conc_connection_list[0].chain_mask) {
				index = CDS_P2P_GO_P2P_CLI_DBS_1x1;
			} else {
				index = CDS_P2P_GO_P2P_CLI_DBS_2x2;
			}
		}
	} else    /* P2P GO + SAP */
	if (((CDS_SAP_MODE == conc_connection_list[0].mode) &&
		(CDS_P2P_GO_MODE == conc_connection_list[1].mode)) ||
		((CDS_P2P_GO_MODE == conc_connection_list[0].mode) &&
		(CDS_SAP_MODE == conc_connection_list[1].mode))) {
		/* SCC */
		if (conc_connection_list[0].chan ==
			conc_connection_list[1].chan) {
			if (CDS_IS_CHANNEL_24GHZ(
				conc_connection_list[0].chan)) {
				if (CDS_ONE_ONE ==
					conc_connection_list[0].chain_mask)
					index = CDS_P2P_GO_SAP_SCC_24_1x1;
				else
					index = CDS_P2P_GO_SAP_SCC_24_2x2;
			} else {
				if (CDS_ONE_ONE ==
					conc_connection_list[0].chain_mask)
					index = CDS_P2P_GO_SAP_SCC_5_1x1;
				else
					index = CDS_P2P_GO_SAP_SCC_5_2x2;
			}
		/* MCC */
		} else if (conc_connection_list[0].mac ==
			conc_connection_list[1].mac) {
			if ((CDS_IS_CHANNEL_24GHZ(
				conc_connection_list[0].chan)) &&
				(CDS_IS_CHANNEL_24GHZ(
				conc_connection_list[1].chan))) {
				if (CDS_ONE_ONE ==
					conc_connection_list[0].chain_mask)
					index = CDS_P2P_GO_SAP_MCC_24_1x1;
				else
					index = CDS_P2P_GO_SAP_MCC_24_2x2;
			} else if ((CDS_IS_CHANNEL_5GHZ(
				conc_connection_list[0].chan)) &&
				(CDS_IS_CHANNEL_5GHZ(
				conc_connection_list[1].chan))) {
				if (CDS_ONE_ONE ==
					conc_connection_list[0].chain_mask)
					index = CDS_P2P_GO_SAP_MCC_5_1x1;
				else
					index = CDS_P2P_GO_SAP_MCC_5_2x2;
			} else {
				if (CDS_ONE_ONE ==
					conc_connection_list[0].chain_mask)
					index = CDS_P2P_GO_SAP_MCC_24_5_1x1;
				else
					index = CDS_P2P_GO_SAP_MCC_24_5_2x2;
			}
			/* SBS */
		} else if (conc_connection_list[0].mac !=
				conc_connection_list[1].mac) {
			if ((CDS_IS_CHANNEL_5GHZ(
				conc_connection_list[0].chan)) &&
				(CDS_IS_CHANNEL_5GHZ(
				conc_connection_list[1].chan))) {
				if (CDS_ONE_ONE ==
					conc_connection_list[0].chain_mask)
					index = CDS_P2P_GO_SAP_SBS_5_1x1;
			}
		/* DBS */
		} else {
			if (CDS_ONE_ONE ==
				conc_connection_list[0].chain_mask) {
				index = CDS_P2P_GO_SAP_DBS_1x1;
			} else {
				index = CDS_P2P_GO_SAP_DBS_2x2;
			}
		}
	} else    /* STA + STA */
	if (((CDS_STA_MODE == conc_connection_list[0].mode) &&
		(CDS_STA_MODE == conc_connection_list[1].mode)) ||
		((CDS_STA_MODE == conc_connection_list[0].mode) &&
		(CDS_STA_MODE == conc_connection_list[1].mode))) {
		/* SCC */
		if (conc_connection_list[0].chan ==
		conc_connection_list[1].chan) {
			if (CDS_IS_CHANNEL_24GHZ
				(conc_connection_list[0].chan)) {
				if (CDS_ONE_ONE ==
				conc_connection_list[0].chain_mask)
					index = CDS_STA_STA_SCC_24_1x1;
				else
					index = CDS_STA_STA_SCC_24_2x2;
			} else {
				if (CDS_ONE_ONE ==
				conc_connection_list[0].chain_mask)
					index = CDS_STA_STA_SCC_5_1x1;
				else
					index = CDS_STA_STA_SCC_5_2x2;
			}
		/* MCC */
		} else if (conc_connection_list[0].mac ==
			conc_connection_list[1].mac) {
			if ((CDS_IS_CHANNEL_24GHZ(
				conc_connection_list[0].chan)) &&
				(CDS_IS_CHANNEL_24GHZ(
				conc_connection_list[1].chan))) {
				if (CDS_ONE_ONE ==
					conc_connection_list[0].chain_mask)
					index = CDS_STA_STA_MCC_24_1x1;
				else
					index = CDS_STA_STA_MCC_24_2x2;
			} else if ((CDS_IS_CHANNEL_5GHZ(
				conc_connection_list[0].chan)) &&
				(CDS_IS_CHANNEL_5GHZ(
				conc_connection_list[1].chan))) {
				if (CDS_ONE_ONE ==
					conc_connection_list[0].chain_mask)
					index = CDS_STA_STA_MCC_5_1x1;
				else
					index = CDS_STA_STA_MCC_5_2x2;
			} else {
				if (CDS_ONE_ONE ==
					conc_connection_list[0].chain_mask)
					index = CDS_STA_STA_MCC_24_5_1x1;
				else
					index = CDS_STA_STA_MCC_24_5_2x2;
			}
			/* SBS */
		} else if (conc_connection_list[0].mac !=
				conc_connection_list[1].mac) {
			if ((CDS_IS_CHANNEL_5GHZ(
				conc_connection_list[0].chan)) &&
				(CDS_IS_CHANNEL_5GHZ(
				conc_connection_list[1].chan))) {
				if (CDS_ONE_ONE ==
					conc_connection_list[0].chain_mask)
					index = CDS_STA_STA_SBS_5_1x1;
			}
		/* DBS */
		} else {
			if (CDS_ONE_ONE ==
				conc_connection_list[0].chain_mask) {
				index = CDS_STA_STA_DBS_1x1;
			} else {
				index = CDS_STA_STA_DBS_2x2;
			}
		}
	}

	cds_debug("mode0:%d mode1:%d chan0:%d chan1:%d chain:%d index:%d",
		conc_connection_list[0].mode, conc_connection_list[1].mode,
		conc_connection_list[0].chan, conc_connection_list[1].chan,
		conc_connection_list[0].chain_mask, index);

	qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
	return index;
}

/**
 * cds_update_and_wait_for_connection_update() - Update and wait for
 * connection update
 * @session_id: Session id
 * @channel: Channel number
 * @reason: Reason for connection update
 *
 * Update the connection to either single MAC or dual MAC and wait for the
 * update to complete
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cds_update_and_wait_for_connection_update(uint8_t session_id,
					uint8_t channel,
					enum sir_conn_update_reason reason)
{
	QDF_STATUS status;

	cds_debug("session:%d channel:%d reason:%d",
		session_id, channel, reason);

	status = qdf_reset_connection_update();
	if (QDF_IS_STATUS_ERROR(status))
		cds_err("clearing event failed");

	status = cds_current_connections_update(session_id, channel, reason);
	if (QDF_STATUS_E_FAILURE == status) {
		cds_err("connections update failed");
		return QDF_STATUS_E_FAILURE;
	}

	/* Wait only when status is success */
	if (QDF_IS_STATUS_SUCCESS(status)) {
		status = qdf_wait_for_connection_update();
		if (QDF_IS_STATUS_ERROR(status)) {
			cds_err("qdf wait for event failed");
			return QDF_STATUS_E_FAILURE;
		}
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * cds_current_connections_update() - initiates actions
 * needed on current connections once channel has been decided
 * for the new connection
 * @session_id: Session id
 * @channel: Channel on which new connection will be
 * @reason: Reason for which connection update is required
 *
 * This function initiates initiates actions
 * needed on current connections once channel has been decided
 * for the new connection. Notifies UMAC & FW as well
 *
 * Return: QDF_STATUS enum
 */
QDF_STATUS cds_current_connections_update(uint32_t session_id,
				uint8_t channel,
				enum sir_conn_update_reason reason)
{
	enum cds_conc_next_action next_action = CDS_NOP;
	uint32_t num_connections = 0;
	enum cds_one_connection_mode second_index = 0;
	enum cds_two_connection_mode third_index = 0;
	enum cds_band band;
	cds_context_type *cds_ctx;
	hdd_context_t *hdd_ctx;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return status;
	}

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("Invalid HDD context");
		return QDF_STATUS_E_FAILURE;
	}

	if (wma_is_hw_dbs_capable() == false) {
		cds_err("driver isn't dbs capable, no further action needed");
		return QDF_STATUS_E_NOSUPPORT;
	}
	if (CDS_IS_CHANNEL_24GHZ(channel))
		band = CDS_BAND_24;
	else
		band = CDS_BAND_5;

	num_connections = cds_get_connection_count();

	cds_debug("num_connections=%d channel=%d",
		num_connections, channel);

	switch (num_connections) {
	case 0:
		if (band == CDS_BAND_24)
			if (wma_is_hw_dbs_2x2_capable())
				next_action = CDS_DBS;
			else
				next_action = CDS_NOP;
		else
			next_action = CDS_NOP;
		break;
	case 1:
		second_index =
			cds_get_second_connection_pcl_table_index();
		if (CDS_MAX_ONE_CONNECTION_MODE == second_index) {
			cds_err("couldn't find index for 2nd connection next action table");
			goto done;
		}
		next_action =
			(*next_action_two_connection_table)[second_index][band];
		break;
	case 2:
		third_index =
			cds_get_third_connection_pcl_table_index();
		if (CDS_MAX_TWO_CONNECTION_MODE == third_index) {
			cds_err("couldn't find index for 3rd connection next action table");
			goto done;
		}
		next_action = (*next_action_three_connection_table)
							[third_index][band];
		break;
	default:
		cds_err("unexpected num_connections value %d", num_connections);
		break;
	}

	if (CDS_NOP != next_action)
		status = cds_next_actions(session_id,
						next_action, reason);
	else
		status = QDF_STATUS_E_NOSUPPORT;

	cds_debug("index2=%d index3=%d next_action=%d, band=%d status=%d reason=%d session_id=%d",
		second_index, third_index, next_action, band, status,
		reason, session_id);

done:
	return status;
}

/**
 * cds_nss_update_cb() - callback from SME confirming nss
 * update
 * @hdd_ctx:	HDD Context
 * @tx_status: tx completion status for updated beacon with new
 *		nss value
 * @vdev_id: vdev id for the specific connection
 * @next_action: next action to happen at policy mgr after
 *		beacon update
 * @reason: Reason for nss update
 *
 * This function is the callback registered with SME at nss
 * update request time
 *
 * Return: None
 */
static void cds_nss_update_cb(void *context, uint8_t tx_status, uint8_t vdev_id,
				uint8_t next_action,
				enum sir_conn_update_reason reason)
{
	cds_context_type *cds_ctx;
	uint32_t conn_index = 0;

	if (QDF_STATUS_SUCCESS != tx_status)
		cds_err("nss update failed(%d) for vdev %d", tx_status, vdev_id);

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return;
	}

	/*
	 * Check if we are ok to request for HW mode change now
	 */
	conn_index = cds_get_connection_for_vdev_id(vdev_id);
	if (MAX_NUMBER_OF_CONC_CONNECTIONS == conn_index) {
		cds_err("connection not found for vdev %d", vdev_id);
		return;
	}

	cds_debug("nss update successful for vdev:%d", vdev_id);
	cds_next_actions(vdev_id, next_action, reason);
	return;
}

/**
 * cds_complete_action() - initiates actions needed on
 * current connections once channel has been decided for the new
 * connection
 * @new_nss: the new nss value
 * @next_action: next action to happen at policy mgr after
 *		beacon update
 * @reason: Reason for connection update
 * @session_id: Session id
 *
 * This function initiates initiates actions
 * needed on current connections once channel has been decided
 * for the new connection. Notifies UMAC & FW as well
 *
 * Return: QDF_STATUS enum
 */
static QDF_STATUS cds_complete_action(uint8_t  new_nss, uint8_t next_action,
				enum sir_conn_update_reason reason,
				uint32_t session_id)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint32_t index, count;
	uint32_t list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint32_t conn_index = 0;
	hdd_context_t *hdd_ctx;
	uint32_t vdev_id;
	uint32_t original_nss;
	cds_context_type *cds_ctx;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return status;
	}

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return status;
	}

	if (wma_is_hw_dbs_capable() == false) {
		cds_err("driver isn't dbs capable, no further action needed");
		return QDF_STATUS_E_NOSUPPORT;
	}

	/* cds_complete_action() is called by cds_next_actions().
	 * All other callers of cds_next_actions() have taken mutex
	 * protection. So, not taking any lock inside cds_complete_action()
	 * during conc_connection_list access.
	 */
	count = cds_mode_specific_connection_count(
			CDS_P2P_GO_MODE, list);
	for (index = 0; index < count; index++) {
		qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
		vdev_id = conc_connection_list[list[index]].vdev_id;
		original_nss = conc_connection_list[list[index]].original_nss;
		qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
		conn_index = cds_get_connection_for_vdev_id(vdev_id);
		if (MAX_NUMBER_OF_CONC_CONNECTIONS == conn_index) {
			cds_err("connection not found for vdev %d",
				vdev_id);
			continue;
		}

		if (2 == original_nss) {
			status = sme_nss_update_request(hdd_ctx->hHal,
					vdev_id, new_nss,
					cds_nss_update_cb,
					next_action, hdd_ctx, reason);
			if (!QDF_IS_STATUS_SUCCESS(status)) {
				cds_err("sme_nss_update_request() failed for vdev %d",
				vdev_id);
			}
		}
	}

	count = cds_mode_specific_connection_count(
			CDS_SAP_MODE, list);
	for (index = 0; index < count; index++) {
		qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
		vdev_id = conc_connection_list[list[index]].vdev_id;
		original_nss = conc_connection_list[list[index]].original_nss;
		qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
		conn_index = cds_get_connection_for_vdev_id(vdev_id);
		if (MAX_NUMBER_OF_CONC_CONNECTIONS == conn_index) {
			cds_err("connection not found for vdev %d",
				vdev_id);
			continue;
		}
		if (2 == original_nss) {
			status = sme_nss_update_request(hdd_ctx->hHal,
					vdev_id, new_nss,
					cds_nss_update_cb,
					next_action, hdd_ctx, reason);
			if (!QDF_IS_STATUS_SUCCESS(status)) {
				cds_err("sme_nss_update_request() failed for vdev %d",
				vdev_id);
			}
		}
	}
	if (!QDF_IS_STATUS_SUCCESS(status))
		status = cds_next_actions(session_id,
						next_action, reason);

	return status;
}

/**
 * cds_next_actions() - initiates actions needed on current
 * connections once channel has been decided for the new
 * connection
 * @session_id: Session id
 * @action: action to be executed
 * @reason: Reason for connection update
 *
 * This function initiates initiates actions
 * needed on current connections once channel has been decided
 * for the new connection. Notifies UMAC & FW as well
 *
 * Return: QDF_STATUS enum
 */
QDF_STATUS cds_next_actions(uint32_t session_id,
				enum cds_conc_next_action action,
				enum sir_conn_update_reason reason)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct sir_hw_mode_params hw_mode;

	if (wma_is_hw_dbs_capable() == false) {
		cds_err("driver isn't dbs capable, no further action needed");
		return QDF_STATUS_E_NOSUPPORT;
	}

	/* check for the current HW index to see if really need any action */
	status = wma_get_current_hw_mode(&hw_mode);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		cds_err("wma_get_current_hw_mode failed");
		return status;
	}
	/**
	 *  if already in DBS no need to request DBS. Might be needed
	 *  to extend the logic when multiple dbs HW mode is available
	 */
	if ((((CDS_DBS_DOWNGRADE == action) || (CDS_DBS == action) ||
		(CDS_DBS_UPGRADE == action))
		&& hw_mode.dbs_cap)) {
		cds_err("driver is already in %s mode, no further action needed",
				(hw_mode.dbs_cap) ? "dbs" : "non dbs");
		return QDF_STATUS_E_ALREADY;
	}

	if ((CDS_SBS == action) || (action == CDS_SBS_DOWNGRADE)) {
		if (!wma_is_hw_sbs_capable()) {
			/* No action */
			cds_info("firmware is not sbs capable");
			return QDF_STATUS_SUCCESS;
		}
		/* check if current mode is already SBS nothing to be
		 * done
		 */

	}

	switch (action) {
	case CDS_DBS_DOWNGRADE:
		/*
		* check if we have a beaconing entity that is using 2x2. If yes,
		* update the beacon template & notify FW. Once FW confirms
		*  beacon updated, send down the HW mode change req
		*/
		status = cds_complete_action(CDS_RX_NSS_1, CDS_DBS, reason,
						session_id);
		break;
	case CDS_DBS:
		if (wma_is_hw_dbs_2x2_capable())
			status = cds_pdev_set_hw_mode(session_id,
						HW_MODE_SS_2x2,
						HW_MODE_80_MHZ,
						HW_MODE_SS_2x2, HW_MODE_40_MHZ,
						HW_MODE_DBS,
						HW_MODE_AGILE_DFS_NONE,
						HW_MODE_SBS_NONE,
						reason);
		else
			status = cds_pdev_set_hw_mode(session_id,
						HW_MODE_SS_1x1,
						HW_MODE_80_MHZ,
						HW_MODE_SS_1x1, HW_MODE_40_MHZ,
						HW_MODE_DBS,
						HW_MODE_AGILE_DFS_NONE,
						HW_MODE_SBS_NONE,
						reason);
		break;
	case CDS_SINGLE_MAC_UPGRADE:
		/*
		 * change the HW mode first before the NSS upgrade
		 */
		status = cds_pdev_set_hw_mode(session_id,
						HW_MODE_SS_2x2,
						HW_MODE_80_MHZ,
						HW_MODE_SS_0x0, HW_MODE_BW_NONE,
						HW_MODE_DBS_NONE,
						HW_MODE_AGILE_DFS_NONE,
						HW_MODE_SBS_NONE,
						reason);
		/*
		* check if we have a beaconing entity that advertised 2x2
		* intially. If yes, update the beacon template & notify FW.
		* Once FW confirms beacon updated, send the HW mode change req
		*/
		status = cds_complete_action(CDS_RX_NSS_2, CDS_SINGLE_MAC,
						reason, session_id);
		break;
	case CDS_SINGLE_MAC:
		status = cds_pdev_set_hw_mode(session_id,
						HW_MODE_SS_2x2,
						HW_MODE_80_MHZ,
						HW_MODE_SS_0x0, HW_MODE_BW_NONE,
						HW_MODE_DBS_NONE,
						HW_MODE_AGILE_DFS_NONE,
						HW_MODE_SBS_NONE,
						reason);
		break;
	case CDS_DBS_UPGRADE:
		status = cds_pdev_set_hw_mode(session_id,
						HW_MODE_SS_2x2,
						HW_MODE_80_MHZ,
						HW_MODE_SS_2x2, HW_MODE_80_MHZ,
						HW_MODE_DBS,
						HW_MODE_AGILE_DFS_NONE,
						HW_MODE_SBS_NONE,
						reason);

		status = cds_complete_action(CDS_RX_NSS_2, CDS_DBS, reason,
						session_id);
		break;
	case CDS_SBS_DOWNGRADE:
		status = cds_complete_action(CDS_RX_NSS_1, CDS_SBS, reason,
						session_id);
		break;
	case CDS_SBS:
		status = cds_pdev_set_hw_mode(session_id,
						HW_MODE_SS_1x1,
						HW_MODE_80_MHZ,
						HW_MODE_SS_1x1, HW_MODE_80_MHZ,
						HW_MODE_DBS,
						HW_MODE_AGILE_DFS_NONE,
						HW_MODE_SBS,
						reason);
		break;
	default:
		cds_err("unexpected action value %d", action);
		status = QDF_STATUS_E_FAILURE;
		break;
	}

	return status;
}

/**
 * cds_get_concurrency_mode() - return concurrency mode
 *
 * This routine is used to retrieve concurrency mode
 *
 * Return: uint32_t value of concurrency mask
 */
uint32_t cds_get_concurrency_mode(void)
{
	hdd_context_t *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (NULL != hdd_ctx) {
		cds_info("concurrency_mode = 0x%x",
			hdd_ctx->concurrency_mode);
		return hdd_ctx->concurrency_mode;
	}

	/* we are in an invalid state :( */
	cds_err("Invalid context");
	return QDF_STA_MASK;
}

/**
 * cds_sap_restart_handle() - to handle restarting of SAP
 * @work: name of the work
 *
 * Purpose of this function is to trigger sap start. this function
 * will be called from workqueue.
 *
 * Return: void.
 */
static void cds_sap_restart_handle(struct work_struct *work)
{
	hdd_adapter_t *sap_adapter;
	hdd_context_t *hdd_ctx = container_of(work, hdd_context_t,
					sap_start_work);
	cds_ssr_protect(__func__);
	if (0 != wlan_hdd_validate_context(hdd_ctx)) {
		cds_ssr_unprotect(__func__);
		return;
	}
	sap_adapter = hdd_get_adapter(hdd_ctx, QDF_SAP_MODE);
	if (sap_adapter == NULL) {
		cds_err("sap_adapter is NULL");
		cds_ssr_unprotect(__func__);
		return;
	}
	wlan_hdd_start_sap(sap_adapter);

	cds_change_sap_restart_required_status(false);
	cds_ssr_unprotect(__func__);
}

/**
 * cds_check_and_restart_sap() - Check and restart sap if required
 * @roam_result: Roam result
 * @hdd_sta_ctx: HDD station context
 *
 * This routine will restart the SAP if restart is pending
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cds_check_and_restart_sap(eCsrRoamResult roam_result,
			hdd_station_ctx_t *hdd_sta_ctx)
{
	hdd_adapter_t *sap_adapter = NULL;
	hdd_ap_ctx_t *hdd_ap_ctx = NULL;
	uint8_t default_sap_channel = 6;
	hdd_context_t *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (!(hdd_ctx->config->conc_custom_rule1 &&
			(true == cds_is_sap_restart_required())))
		return QDF_STATUS_SUCCESS;

	sap_adapter = hdd_get_adapter(hdd_ctx, QDF_SAP_MODE);
	if (sap_adapter == NULL) {
		cds_err("sap_adapter is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (test_bit(SOFTAP_BSS_STARTED, &sap_adapter->event_flags)) {
		cds_err("SAP is already in started state");
		return QDF_STATUS_E_FAILURE;
	}

	hdd_ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(sap_adapter);
	if (hdd_ap_ctx == NULL) {
		cds_err("HDD sap context is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	if ((eCSR_ROAM_RESULT_ASSOCIATED == roam_result) &&
			hdd_sta_ctx->conn_info.operationChannel <
			SIR_11A_CHANNEL_BEGIN) {
		cds_err("Starting SAP on chnl [%d] after STA assoc complete",
			hdd_sta_ctx->conn_info.operationChannel);
		hdd_ap_ctx->operatingChannel =
			hdd_sta_ctx->conn_info.operationChannel;
	} else {
		/* start on default SAP channel */
		hdd_ap_ctx->operatingChannel =
			default_sap_channel;
		cds_err("Starting SAP on channel [%d] after STA assoc failed",
			default_sap_channel);
	}
	hdd_ap_ctx->sapConfig.ch_params.ch_width =
		hdd_ap_ctx->sapConfig.ch_width_orig;

	cds_set_channel_params(hdd_ap_ctx->operatingChannel,
		hdd_ap_ctx->sapConfig.sec_ch,
		&hdd_ap_ctx->sapConfig.ch_params);
	/*
	 * Create a workqueue and let the workqueue handle the restart
	 * of sap task. if we directly call sap restart function without
	 * creating workqueue then our main thread might go to sleep
	 * which is not acceptable.
	 */
	INIT_WORK(&hdd_ctx->sap_start_work,
			cds_sap_restart_handle);
	schedule_work(&hdd_ctx->sap_start_work);
	return QDF_STATUS_SUCCESS;
}

/**
 * cds_sta_sap_concur_handle() - This function will handle Station and sap
 * concurrency.
 * @sta_adapter: pointer to station adapter.
 * @roam_profile: pointer to station's roam profile.
 *
 * This function will find the AP to which station is likely to make the
 * the connection, if that AP's channel happens to be different than
 * SAP's channel then this function will stop the SAP.
 *
 * Return: true or false based on function's overall success.
 */
static bool cds_sta_sap_concur_handle(hdd_adapter_t *sta_adapter,
		tCsrRoamProfile *roam_profile)
{
	hdd_adapter_t *ap_adapter;
	bool are_cc_channels_same = false;
	tScanResultHandle scan_cache = NULL;
	QDF_STATUS status;
	hdd_context_t *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return are_cc_channels_same;
	}

	ap_adapter = hdd_get_adapter(hdd_ctx, QDF_SAP_MODE);
	if ((ap_adapter != NULL) &&
		test_bit(SOFTAP_BSS_STARTED, &ap_adapter->event_flags)) {
		status =
			wlan_hdd_check_custom_con_channel_rules(sta_adapter,
					ap_adapter, roam_profile, &scan_cache,
					&are_cc_channels_same);
		if (QDF_STATUS_SUCCESS != status) {
			cds_err("wlan_hdd_check_custom_con_channel_rules failed!");
			/* Not returning */
		}
		status = sme_scan_result_purge(
				WLAN_HDD_GET_HAL_CTX(sta_adapter),
				scan_cache);
		if (QDF_STATUS_SUCCESS != status) {
			cds_err("sme_scan_result_purge failed!");
			/* Not returning */
		}
		/*
		 * are_cc_channels_same will be false incase if SAP and STA
		 * channel is different or STA channel is zero.
		 * incase if STA channel is zero then lets stop the AP and
		 * restart flag set, so later whenever STA channel is defined
		 * we can restart our SAP in that channel.
		 */
		if (false == are_cc_channels_same) {
			cds_info("Stop AP due to mismatch with STA channel");
			wlan_hdd_stop_sap(ap_adapter);
			cds_change_sap_restart_required_status(true);
			return false;
		} else {
			cds_info("sap channels are same");
		}
	}
	return true;
}

#ifdef FEATURE_WLAN_CH_AVOID
/**
 * cds_sta_p2pgo_concur_handle() - This function will handle Station and GO
 * concurrency.
 * @sta_adapter: pointer to station adapter.
 * @roam_profile: pointer to station's roam profile.
 * @roam_id: reference to roam_id variable being passed.
 *
 * This function will find the AP to which station is likely to make the
 * the connection, if that AP's channel happens to be different than our
 * P2PGO's channel then this function will send avoid frequency event to
 * framework to make P2PGO stop and also caches station's connect request.
 *
 * Return: true or false based on function's overall success.
 */
static bool cds_sta_p2pgo_concur_handle(hdd_adapter_t *sta_adapter,
		tCsrRoamProfile *roam_profile,
		uint32_t *roam_id)
{
	hdd_adapter_t *p2pgo_adapter;
	bool are_cc_channels_same = false;
	tScanResultHandle scan_cache = NULL;
	uint32_t p2pgo_channel_num, freq;
	tHddAvoidFreqList hdd_avoid_freq_list;
	QDF_STATUS status;
	bool ret;
	hdd_context_t *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return are_cc_channels_same;
	}
	p2pgo_adapter = hdd_get_adapter(hdd_ctx, QDF_P2P_GO_MODE);
	if ((p2pgo_adapter != NULL) &&
		test_bit(SOFTAP_BSS_STARTED, &p2pgo_adapter->event_flags)) {
		status =
			wlan_hdd_check_custom_con_channel_rules(sta_adapter,
					p2pgo_adapter, roam_profile,
					&scan_cache, &are_cc_channels_same);
		if (QDF_STATUS_SUCCESS != status) {
			cds_err("wlan_hdd_check_custom_con_channel_rules failed");
			/* Not returning */
		}
		/*
		 * are_cc_channels_same will be false incase if P2PGO and STA
		 * channel is different or STA channel is zero.
		 */
		if (false == are_cc_channels_same) {
			if (true == cds_is_sta_connection_pending()) {
				MTRACE(qdf_trace(QDF_MODULE_ID_HDD,
					TRACE_CODE_HDD_CLEAR_JOIN_REQ,
					sta_adapter->sessionId, *roam_id));
				ret = sme_clear_joinreq_param(
					WLAN_HDD_GET_HAL_CTX(sta_adapter),
					sta_adapter->sessionId);
				if (true != ret) {
					cds_err("sme_clear_joinreq_param failed");
					/* Not returning */
				}
				cds_change_sta_conn_pending_status(false);
				cds_info("===>Clear pending join req");
			}
			MTRACE(qdf_trace(QDF_MODULE_ID_HDD,
					TRACE_CODE_HDD_STORE_JOIN_REQ,
					sta_adapter->sessionId, *roam_id));
			/* store the scan cache here */
			ret = sme_store_joinreq_param(
					WLAN_HDD_GET_HAL_CTX(sta_adapter),
					roam_profile,
					scan_cache,
					roam_id,
					sta_adapter->sessionId);
			if (true != ret) {
				cds_err("sme_store_joinreq_param failed");
				/* Not returning */
			}
			cds_change_sta_conn_pending_status(true);
			/*
			 * fill frequency avoidance event and send it up, so
			 * p2pgo stop event should get trigger from upper layer
			 */
			p2pgo_channel_num =
				WLAN_HDD_GET_AP_CTX_PTR(p2pgo_adapter)->
				operatingChannel;
			if (p2pgo_channel_num <= 14) {
				freq = ieee80211_channel_to_frequency(
						p2pgo_channel_num,
						NL80211_BAND_2GHZ);
			} else {
				freq = ieee80211_channel_to_frequency(
						p2pgo_channel_num,
						NL80211_BAND_5GHZ);
			}
			qdf_mem_zero(&hdd_avoid_freq_list,
					sizeof(hdd_avoid_freq_list));
			hdd_avoid_freq_list.avoidFreqRangeCount = 1;
			hdd_avoid_freq_list.avoidFreqRange[0].startFreq = freq;
			hdd_avoid_freq_list.avoidFreqRange[0].endFreq = freq;
			wlan_hdd_send_avoid_freq_event(hdd_ctx,
					&hdd_avoid_freq_list);
			cds_info("===>Sending chnl_avoid ch[%d] freq[%d]",
				p2pgo_channel_num, freq);
			cds_info("=>Stop GO due to mismatch with STA channel");
			return false;
		} else {
			cds_info("===>p2pgo channels are same");
			status = sme_scan_result_purge(
					WLAN_HDD_GET_HAL_CTX(sta_adapter),
					scan_cache);
			if (QDF_STATUS_SUCCESS != status) {
				cds_err("sme_scan_result_purge failed");
				/* Not returning */
			}
		}
	}
	return true;
}
#endif

/**
 * cds_handle_conc_rule1() - Check if concurrency rule1 is enabled
 * @adapter: HDD adpater
 * @roam_profile: Profile for connection
 *
 * Check if concurrency rule1 is enabled. As per rule1, if station is trying to
 * connect to some AP in 2.4Ghz and SAP is already in started state then SAP
 * should restart in station's
 *
 * Return: None
 */
void cds_handle_conc_rule1(hdd_adapter_t *adapter,
		tCsrRoamProfile *roam_profile)
{
	bool ret;
	hdd_context_t *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return;
	}

	/*
	 * Custom concurrency rule1: As per this rule if station is
	 * trying to connect to some AP in 2.4Ghz and SAP is already
	 * in started state then SAP should restart in station's
	 * channel.
	 */
	if (hdd_ctx->config->conc_custom_rule1 &&
			(QDF_STA_MODE == adapter->device_mode)) {
		ret = cds_sta_sap_concur_handle(adapter,
				roam_profile);
		if (true != ret) {
			cds_err("cds_sta_sap_concur_handle failed");
			/* Nothing to do for now */
		}
	}
}

#ifdef FEATURE_WLAN_CH_AVOID
/**
 * cds_handle_conc_rule2() - Check if concurrency rule2 is enabled
 * @adapter: HDD adpater
 * @roam_profile: Profile for connection
 *
 * Check if concurrency rule1 is enabled. As per rule1, if station is trying to
 * connect to some AP in 5Ghz and P2PGO is already in started state then P2PGO
 * should restart in station's channel
 *
 * Return: None
 */
bool cds_handle_conc_rule2(hdd_adapter_t *adapter,
		tCsrRoamProfile *roam_profile,
		uint32_t *roam_id)
{
	hdd_context_t *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return false;
	}

	/*
	 * Custom concurrency rule2: As per this rule if station is
	 * trying to connect to some AP in 5Ghz and P2PGO is already in
	 * started state then P2PGO should restart in station's channel.
	 */
	if (hdd_ctx->config->conc_custom_rule2 &&
		(QDF_STA_MODE == adapter->device_mode)) {
		if (false == cds_sta_p2pgo_concur_handle(
					adapter, roam_profile, roam_id)) {
			cds_err("P2PGO-STA chnl diff, cache join req");
			return false;
		}
	}
	return true;
}
#endif

/**
 * cds_get_channel_from_scan_result() - to get channel from scan result
 * @adapter: station adapter
 * @roam_profile: pointer to roam profile
 * @channel: channel to be filled
 *
 * This routine gets channel which most likely a candidate to which STA
 * will make connection.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cds_get_channel_from_scan_result(hdd_adapter_t *adapter,
			tCsrRoamProfile *roam_profile, uint8_t *channel)
{
	QDF_STATUS status;
	tScanResultHandle scan_cache = NULL;

	status = sme_get_ap_channel_from_scan_cache(
				WLAN_HDD_GET_HAL_CTX(adapter),
				roam_profile, &scan_cache,
				channel);
	sme_scan_result_purge(WLAN_HDD_GET_HAL_CTX(adapter), scan_cache);
	return status;
}

/**
 * cds_search_and_check_for_session_conc() - Checks if concurrecy is allowed
 * @session_id: Session id
 * @roam_profile: Pointer to the roam profile
 *
 * Searches and gets the channel number from the scan results and checks if
 * concurrency is allowed for the given session ID
 *
 * Non zero channel number if concurrency is allowed, zero otherwise
 */
uint8_t cds_search_and_check_for_session_conc(uint8_t session_id,
		tCsrRoamProfile *roam_profile)
{
	uint8_t channel = 0;
	QDF_STATUS status;
	hdd_context_t *hdd_ctx;
	hdd_adapter_t *adapter;
	bool ret;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("Invalid HDD context");
		return channel;
	}

	adapter = hdd_get_adapter_by_vdev(hdd_ctx, session_id);
	if (!adapter) {
		cds_err("Invalid HDD adapter");
		return channel;
	}

	status = cds_get_channel_from_scan_result(adapter,
			roam_profile, &channel);
	if ((QDF_STATUS_SUCCESS != status) || (channel == 0)) {
		cds_err("%s error %d %d",
			__func__, status, channel);
		return 0;
	}

	/* Take care of 160MHz and 80+80Mhz later */
	ret = cds_allow_concurrency(
		cds_convert_device_mode_to_qdf_type(
			adapter->device_mode),
		channel, HW_MODE_20_MHZ);
	if (false == ret) {
		cds_err("Connection failed due to conc check fail");
		return 0;
	}

	return channel;
}

/**
 * cds_check_for_session_conc() - Check if concurrency is allowed for a session
 * @session_id: Session ID
 * @channel: Channel number
 *
 * Checks if connection is allowed for a given session_id
 *
 * True if the concurrency is allowed, false otherwise
 */
bool cds_check_for_session_conc(uint8_t session_id, uint8_t channel)
{
	hdd_context_t *hdd_ctx;
	hdd_adapter_t *adapter;
	bool ret;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("Invalid HDD context");
		return false;
	}

	adapter = hdd_get_adapter_by_vdev(hdd_ctx, session_id);
	if (!adapter) {
		cds_err("Invalid HDD adapter");
		return false;
	}

	if (channel == 0) {
		cds_err("Invalid channel number 0");
		return false;
	}

	/* Take care of 160MHz and 80+80Mhz later */
	ret = cds_allow_concurrency(
		cds_convert_device_mode_to_qdf_type(
			adapter->device_mode),
		channel, HW_MODE_20_MHZ);
	if (false == ret) {
		cds_err("Connection failed due to conc check fail");
		return 0;
	}

	return true;
}

/**
 * cds_handle_conc_multiport() - to handle multiport concurrency
 * @session_id: Session ID
 * @channel: Channel number
 *
 * This routine will handle STA side concurrency when policy manager
 * is enabled.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cds_handle_conc_multiport(uint8_t session_id, uint8_t channel)
{
	QDF_STATUS status;

	if (!cds_check_for_session_conc(session_id, channel)) {
		cds_err("Conc not allowed for the session %d", session_id);
		return QDF_STATUS_E_FAILURE;
	}

	status = qdf_reset_connection_update();
	if (!QDF_IS_STATUS_SUCCESS(status))
		cds_err("clearing event failed");

	status = cds_current_connections_update(session_id,
			channel,
			SIR_UPDATE_REASON_NORMAL_STA);
	if (QDF_STATUS_E_FAILURE == status) {
		cds_err("connections update failed");
		return status;
	}

	return status;
}

#ifdef FEATURE_WLAN_FORCE_SAP_SCC
/**
 * cds_restart_softap() - restart SAP on STA channel to support
 * STA + SAP concurrency.
 *
 * @pHostapdAdapter: pointer to hdd adapter
 *
 * Return: None
 */
void cds_restart_softap(hdd_adapter_t *pHostapdAdapter)
{
	tHddAvoidFreqList hdd_avoid_freq_list;
	hdd_context_t *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return;
	}

	/* generate vendor specific event */
	qdf_mem_zero((void *)&hdd_avoid_freq_list, sizeof(tHddAvoidFreqList));
	hdd_avoid_freq_list.avoidFreqRange[0].startFreq =
		cds_chan_to_freq(pHostapdAdapter->sessionCtx.ap.
				operatingChannel);
	hdd_avoid_freq_list.avoidFreqRange[0].endFreq =
		cds_chan_to_freq(pHostapdAdapter->sessionCtx.ap.
				operatingChannel);
	hdd_avoid_freq_list.avoidFreqRangeCount = 1;
	wlan_hdd_send_avoid_freq_event(hdd_ctx, &hdd_avoid_freq_list);
}

/**
 * cds_force_sap_on_scc() - Force SAP on SCC
 * @roam_result: Roam result
 * @channel_id: STA channel id
 *
 * Restarts SAP on SCC if its operating channel is different from that of the
 * STA-AP interface
 *
 * Return: None
 */
void cds_force_sap_on_scc(eCsrRoamResult roam_result,
			 uint8_t channel_id)
{
	hdd_adapter_t *hostapd_adapter;
	hdd_context_t *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return;
	}

	if (!(eCSR_ROAM_RESULT_ASSOCIATED == roam_result &&
		hdd_ctx->config->SapSccChanAvoidance)) {
		cds_err("Not able to force SAP on SCC");
		return;
	}
	hostapd_adapter = hdd_get_adapter(hdd_ctx, QDF_SAP_MODE);
	if (hostapd_adapter != NULL) {
		/* Restart SAP if its operating channel is different
		 * from AP channel.
		 */
		if (hostapd_adapter->sessionCtx.ap.operatingChannel !=
				channel_id) {
			cds_err("Restart SAP: SAP channel-%d, STA channel-%d",
				hostapd_adapter->sessionCtx.ap.operatingChannel,
				channel_id);
			cds_restart_softap(hostapd_adapter);
		}
	}
}
#endif /* FEATURE_WLAN_FORCE_SAP_SCC */

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
/**
 * cds_check_sta_ap_concurrent_ch_intf() - Restart SAP in STA-AP case
 * @data: Pointer to STA adapter
 *
 * Restarts the SAP interface in STA-AP concurrency scenario
 *
 * Restart: None
 */
static void cds_check_sta_ap_concurrent_ch_intf(void *data)
{
	hdd_adapter_t *ap_adapter = NULL, *sta_adapter = (hdd_adapter_t *) data;
	hdd_context_t *hdd_ctx = WLAN_HDD_GET_CTX(sta_adapter);
	tHalHandle *hal_handle;
	hdd_ap_ctx_t *hdd_ap_ctx;
	uint16_t intf_ch = 0;
	p_cds_contextType cds_ctx;

	cds_ctx = cds_get_global_context();
	if (!cds_ctx) {
		cds_err("Invalid CDS context");
		return;
	}

	cds_info("cds_concurrent_open_sessions_running: %d",
		cds_concurrent_open_sessions_running());

	if ((hdd_ctx->config->WlanMccToSccSwitchMode ==
				QDF_MCC_TO_SCC_SWITCH_DISABLE)
			|| !(cds_concurrent_open_sessions_running()
			    || !(cds_get_concurrency_mode() ==
					(QDF_STA_MASK | QDF_SAP_MASK))))
		return;

	ap_adapter = hdd_get_adapter(hdd_ctx, QDF_SAP_MODE);
	if (ap_adapter == NULL)
		return;

	if (!test_bit(SOFTAP_BSS_STARTED, &ap_adapter->event_flags))
		return;

	hdd_ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(ap_adapter);
	hal_handle = WLAN_HDD_GET_HAL_CTX(ap_adapter);

	if (hal_handle == NULL)
		return;

	intf_ch = wlansap_check_cc_intf(hdd_ap_ctx->sapContext);
	cds_info("intf_ch:%d", intf_ch);

	if (intf_ch == 0)
		return;

	hdd_ap_ctx->sapConfig.channel = intf_ch;
	hdd_ap_ctx->sapConfig.ch_params.ch_width =
		hdd_ap_ctx->sapConfig.ch_width_orig;
	cds_set_channel_params(hdd_ap_ctx->sapConfig.channel,
			hdd_ap_ctx->sapConfig.sec_ch,
			&hdd_ap_ctx->sapConfig.ch_params);

	if (((hdd_ctx->config->WlanMccToSccSwitchMode ==
		QDF_MCC_TO_SCC_SWITCH_FORCE_WITHOUT_DISCONNECTION) ||
		(hdd_ctx->config->WlanMccToSccSwitchMode ==
		QDF_MCC_TO_SCC_SWITCH_WITH_FAVORITE_CHANNEL)) &&
		(cds_ctx->sap_restart_chan_switch_cb)) {
		cds_info("SAP chan change without restart");
		cds_ctx->sap_restart_chan_switch_cb(ap_adapter,
				hdd_ap_ctx->sapConfig.channel,
				hdd_ap_ctx->sapConfig.ch_params.ch_width);
	} else {
		cds_restart_sap(ap_adapter);
	}

}

/**
 * cds_check_concurrent_intf_and_restart_sap() - Check concurrent change intf
 * @adapter: Pointer to HDD adapter
 *
 * Checks the concurrent change interface and restarts SAP
 * Return: None
 */
void cds_check_concurrent_intf_and_restart_sap(hdd_adapter_t *adapter)
{
	hdd_context_t *hdd_ctx;
	hdd_station_ctx_t *hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return;
	}

	cds_info("mode:%d rule1:%d rule2:%d chan:%d",
		hdd_ctx->config->WlanMccToSccSwitchMode,
		hdd_ctx->config->conc_custom_rule1,
		hdd_ctx->config->conc_custom_rule2,
		hdd_sta_ctx->conn_info.operationChannel);

	if ((hdd_ctx->config->WlanMccToSccSwitchMode
				!= QDF_MCC_TO_SCC_SWITCH_DISABLE) &&
			((0 == hdd_ctx->config->conc_custom_rule1) &&
			 (0 == hdd_ctx->config->conc_custom_rule2))
#ifdef FEATURE_WLAN_STA_AP_MODE_DFS_DISABLE
			&& !CDS_IS_DFS_CH(hdd_sta_ctx->conn_info.
				operationChannel)
#endif
	   ) {
		qdf_create_work(0, &hdd_ctx->sta_ap_intf_check_work,
				cds_check_sta_ap_concurrent_ch_intf,
				(void *)adapter);
		qdf_sched_work(0, &hdd_ctx->sta_ap_intf_check_work);
		cds_info("Checking for Concurrent Change interference");
	}
}
#endif /* FEATURE_WLAN_MCC_TO_SCC_SWITCH */

/**
 * cds_is_mcc_in_24G() - Function to check for MCC in 2.4GHz
 *
 * This function is used to check for MCC operation in 2.4GHz band.
 * STA, P2P and SAP adapters are only considered.
 *
 * Return: Non zero value if MCC is detected in 2.4GHz band
 *
 */
uint8_t cds_is_mcc_in_24G(void)
{
	QDF_STATUS status;
	hdd_adapter_t *hdd_adapter = NULL;
	hdd_adapter_list_node_t *adapter_node = NULL, *next = NULL;
	uint8_t ret = 0;
	hdd_station_ctx_t *sta_ctx;
	hdd_ap_ctx_t *ap_ctx;
	uint8_t ch1 = 0, ch2 = 0;
	uint8_t channel = 0;
	hdd_hostapd_state_t *hostapd_state;
	hdd_context_t *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return 1;
	}

	status =  hdd_get_front_adapter(hdd_ctx, &adapter_node);

	/* loop through all adapters and check MCC for STA,P2P,SAP adapters */
	while (NULL != adapter_node && QDF_STATUS_SUCCESS == status) {
		hdd_adapter = adapter_node->pAdapter;

		if (!((hdd_adapter->device_mode >= QDF_STA_MODE)
					|| (hdd_adapter->device_mode
						<= QDF_P2P_GO_MODE))) {
			/* skip for other adapters */
			status = hdd_get_next_adapter(hdd_ctx,
					adapter_node, &next);
			adapter_node = next;
			continue;
		}
		if (QDF_STA_MODE ==
				hdd_adapter->device_mode ||
				QDF_P2P_CLIENT_MODE ==
				hdd_adapter->device_mode) {
			sta_ctx =
				WLAN_HDD_GET_STATION_CTX_PTR(
						hdd_adapter);
			if (eConnectionState_Associated ==
					sta_ctx->conn_info.connState)
				channel =
					sta_ctx->conn_info.
					operationChannel;
		} else if (QDF_P2P_GO_MODE ==
				hdd_adapter->device_mode ||
				QDF_SAP_MODE ==
				hdd_adapter->device_mode) {
			ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(hdd_adapter);
			hostapd_state =
				WLAN_HDD_GET_HOSTAP_STATE_PTR(
						hdd_adapter);
			if (hostapd_state->bssState == BSS_START &&
					hostapd_state->qdf_status ==
					QDF_STATUS_SUCCESS)
				channel = ap_ctx->operatingChannel;
		}

		if ((ch1 == 0) ||
				((ch2 != 0) && (ch2 != channel))) {
			ch1 = channel;
		} else if ((ch2 == 0) ||
				((ch1 != 0) && (ch1 != channel))) {
			ch2 = channel;
		}

		if ((ch1 != 0 && ch2 != 0) && (ch1 != ch2) &&
				((ch1 <= SIR_11B_CHANNEL_END) &&
				 (ch2 <= SIR_11B_CHANNEL_END))) {
			cds_err("MCC in 2.4Ghz on channels %d and %d",
				ch1, ch2);
			return 1;
		}
		status = hdd_get_next_adapter(hdd_ctx,
				adapter_node, &next);
		adapter_node = next;
	}
	return ret;
}

/**
 * cds_set_mas() - Function to set MAS value to UMAC
 * @adapter:            Pointer to HDD adapter
 * @mas_value:          0-Disable, 1-Enable MAS
 *
 * This function passes down the value of MAS to UMAC
 *
 * Return: Configuration message posting status, SUCCESS or Fail
 *
 */
int32_t cds_set_mas(hdd_adapter_t *adapter, uint8_t mas_value)
{
	hdd_context_t *hdd_ctx = NULL;
	QDF_STATUS ret_status;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (!hdd_ctx)
		return -EFAULT;

	if (mas_value) {
		/* Miracast is ON. Disable MAS and configure P2P quota */
		if (hdd_ctx->config->enableMCCAdaptiveScheduler) {
			if (cfg_set_int(hdd_ctx->hHal,
					WNI_CFG_ENABLE_MCC_ADAPTIVE_SCHED, 0)
					!= eSIR_SUCCESS) {
				cds_err("Could not pass on WNI_CFG_ENABLE_MCC_ADAPTIVE_SCHED to CCM");
			}
			ret_status = sme_set_mas(false);
			if (QDF_STATUS_SUCCESS != ret_status) {
				cds_err("Failed to disable MAS");
				return -EBUSY;
			}
		}

		/* Config p2p quota */
		if (adapter->device_mode == QDF_STA_MODE)
			cds_set_mcc_p2p_quota(adapter,
					100 - HDD_DEFAULT_MCC_P2P_QUOTA);
		else if (adapter->device_mode == QDF_P2P_GO_MODE)
			cds_go_set_mcc_p2p_quota(adapter,
					HDD_DEFAULT_MCC_P2P_QUOTA);
		else
			cds_set_mcc_p2p_quota(adapter,
					HDD_DEFAULT_MCC_P2P_QUOTA);
	} else {
		/* Reset p2p quota */
		if (adapter->device_mode == QDF_P2P_GO_MODE)
			cds_go_set_mcc_p2p_quota(adapter,
					HDD_RESET_MCC_P2P_QUOTA);
		else
			cds_set_mcc_p2p_quota(adapter,
					HDD_RESET_MCC_P2P_QUOTA);

		/* Miracast is OFF. Enable MAS and reset P2P quota */
		if (hdd_ctx->config->enableMCCAdaptiveScheduler) {
			if (cfg_set_int(hdd_ctx->hHal,
					WNI_CFG_ENABLE_MCC_ADAPTIVE_SCHED, 1)
					!= eSIR_SUCCESS) {
				cds_err("Could not pass on WNI_CFG_ENABLE_MCC_ADAPTIVE_SCHED to CCM");
			}

			/* Enable MAS */
			ret_status = sme_set_mas(true);
			if (QDF_STATUS_SUCCESS != ret_status) {
				cds_err("Unable to enable MAS");
				return -EBUSY;
			}
		}
	}

	return 0;
}

/**
 * cds_set_mcc_p2p_quota() - Function to set quota for P2P
 * @hostapd_adapter:    Pointer to HDD adapter
 * @set_value:          Qouta value for the interface
 *
 * This function is used to set the quota for P2P cases
 *
 * Return: Configuration message posting status, SUCCESS or Fail
 *
 */
int32_t cds_set_mcc_p2p_quota(hdd_adapter_t *hostapd_adapater,
		uint32_t set_value)
{
	uint8_t first_adapter_operating_channel = 0;
	uint8_t second_adapter_operating_channel = 0;
	int32_t ret = 0; /* success */

	uint32_t concurrent_state = cds_get_concurrency_mode();

	/*
	 * Check if concurrency mode is active.
	 * Need to modify this code to support MCC modes other than STA/P2P
	 */
	if ((concurrent_state == (QDF_STA_MASK | QDF_P2P_CLIENT_MASK)) ||
		(concurrent_state == (QDF_STA_MASK | QDF_P2P_GO_MASK))) {
		cds_info("STA & P2P are both enabled");
		/*
		 * The channel numbers for both adapters and the time
		 * quota for the 1st adapter, i.e., one specified in cmd
		 * are formatted as a bit vector then passed on to WMA
		 * +***********************************************************+
		 * |bit 31-24  | bit 23-16  |   bits 15-8   |   bits 7-0       |
		 * |  Unused   | Quota for  | chan. # for   |   chan. # for    |
		 * |           | 1st chan.  | 1st chan.     |   2nd chan.      |
		 * +***********************************************************+
		 */
		/* Get the operating channel of the specified vdev */
		first_adapter_operating_channel =
			hdd_get_operating_channel
			(
			 hostapd_adapater->pHddCtx,
			 hostapd_adapater->device_mode
			);
		cds_info("1st channel No.:%d and quota:%dms",
			first_adapter_operating_channel, set_value);
		/* Move the time quota for first channel to bits 15-8 */
		set_value = set_value << 8;
		/*
		 * Store the channel number of 1st channel at bits 7-0
		 * of the bit vector
		 */
		set_value = set_value | first_adapter_operating_channel;
		/* Find out the 2nd MCC adapter and its operating channel */
		second_adapter_operating_channel =
			cds_get_mcc_operating_channel(
					hostapd_adapater->sessionId);

		cds_info("2nd vdev channel No. is:%d",
			 second_adapter_operating_channel);

		if (second_adapter_operating_channel == 0 ||
		    first_adapter_operating_channel == 0) {
			cds_err("Invalid channel");
			return -EINVAL;
		}
		/*
		 * Now move the time quota and channel number of the
		 * 1st adapter to bits 23-16 and bits 15-8 of the bit
		 * vector, respectively.
		 */
		set_value = set_value << 8;
		/*
		 * Store the channel number for 2nd MCC vdev at bits
		 * 7-0 of set_value
		 */
		set_value = set_value | second_adapter_operating_channel;
		ret = wma_cli_set_command(hostapd_adapater->sessionId,
				WMA_VDEV_MCC_SET_TIME_QUOTA,
				set_value, VDEV_CMD);
	} else {
		cds_info("MCC is not active. Exit w/o setting latency");
	}
	return ret;
}

/**
 * cds_change_mcc_go_beacon_interval() - Change MCC beacon interval
 * @pHostapdAdapter: HDD adapter
 *
 * Updates the beacon parameters of the GO in MCC scenario
 *
 * Return: Success or Failure depending on the overall function behavior
 */
QDF_STATUS cds_change_mcc_go_beacon_interval(hdd_adapter_t *pHostapdAdapter)
{
	QDF_STATUS qdf_ret_status = QDF_STATUS_E_FAILURE;
	void *hHal;

	cds_info("UPDATE Beacon Params");

	if (QDF_SAP_MODE == pHostapdAdapter->device_mode) {
		hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);
		if (NULL == hHal) {
			cds_err("Hal ctx is null");
			return QDF_STATUS_E_FAULT;
		}
		qdf_ret_status =
			sme_change_mcc_beacon_interval(hHal,
					pHostapdAdapter->
					sessionId);
		if (qdf_ret_status == QDF_STATUS_E_FAILURE) {
			cds_err("Failed to update Beacon Params");
			return QDF_STATUS_E_FAILURE;
		}
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * cds_go_set_mcc_p2p_quota() - Function to set quota for P2P GO
 * @hostapd_adapter:    Pointer to HDD adapter
 * @set_value:          Qouta value for the interface
 *
 * This function is used to set the quota for P2P GO cases
 *
 * Return: Configuration message posting status, SUCCESS or Fail
 *
 */
int32_t cds_go_set_mcc_p2p_quota(hdd_adapter_t *hostapd_adapter,
		uint32_t set_value)
{
	uint8_t first_adapter_operating_channel = 0;
	uint8_t second_adapter_operating_channel = 0;
	uint32_t concurrent_state = 0;
	int32_t ret = 0; /* success */

	/*
	 * Check if concurrency mode is active.
	 * Need to modify this code to support MCC modes other than
	 * STA/P2P GO
	 */

	concurrent_state = cds_get_concurrency_mode();
	if (concurrent_state == (QDF_STA_MASK | QDF_P2P_GO_MASK)) {
		cds_info("STA & P2P are both enabled");

		/*
		 * The channel numbers for both adapters and the time
		 * quota for the 1st adapter, i.e., one specified in cmd
		 * are formatted as a bit vector then passed on to WMA
		 * +************************************************+
		 * |bit 31-24 |bit 23-16  |  bits 15-8  |bits 7-0   |
		 * |  Unused  |  Quota for| chan. # for |chan. # for|
		 * |          |  1st chan.| 1st chan.   |2nd chan.  |
		 * +************************************************+
		 */

		/* Get the operating channel of the specified vdev */
		first_adapter_operating_channel =
			hdd_get_operating_channel(hostapd_adapter->pHddCtx,
					hostapd_adapter->device_mode);

		cds_info("1st channel No.:%d and quota:%dms",
			first_adapter_operating_channel, set_value);

		/* Move the time quota for first adapter to bits 15-8 */
		set_value = set_value << 8;
		/*
		 * Store the operating channel number of 1st adapter at
		 * the lower 8-bits of bit vector.
		 */
		set_value = set_value | first_adapter_operating_channel;

		/* Find out the 2nd MCC adapter and its operating channel */
		second_adapter_operating_channel =
			cds_get_mcc_operating_channel(
					hostapd_adapter->sessionId);

		cds_info("2nd vdev channel No. is:%d",
			 second_adapter_operating_channel);

		if (second_adapter_operating_channel == 0 ||
		    first_adapter_operating_channel == 0) {
			cds_err("Invalid channel");
			return -EINVAL;
		}

		/*
		 * Move the time quota and operating channel number
		 * for the first adapter to bits 23-16 & bits 15-8
		 * of set_value vector, respectively.
		 */
		set_value = set_value << 8;
		/*
		 * Store the channel number for 2nd MCC vdev at bits
		 * 7-0 of set_value vector as per the bit format above.
		 */
		set_value = set_value |
			second_adapter_operating_channel;
		ret = wma_cli_set_command(hostapd_adapter->sessionId,
				WMA_VDEV_MCC_SET_TIME_QUOTA,
				set_value, VDEV_CMD);
	} else {
		cds_info("MCC is not active. Exit w/o setting latency");
	}
	return ret;
}

/**
 * cds_set_mcc_latency() - Set MCC latency
 * @adapter: Pointer to HDD adapter
 * @set_value: Latency value
 *
 * Sets the MCC latency value during STA-P2P concurrency
 *
 * Return: None
 */
void cds_set_mcc_latency(hdd_adapter_t *adapter, int set_value)
{
	uint32_t concurrent_state = 0;
	uint8_t first_adapter_operating_channel = 0;
	int ret = 0;            /* success */

	cds_info("iwpriv cmd to set MCC latency with val %dms",
		set_value);
	/**
	 * Check if concurrency mode is active.
	 * Need to modify this code to support MCC modes other than STA/P2P
	 */
	concurrent_state = cds_get_concurrency_mode();
	if ((concurrent_state == (QDF_STA_MASK | QDF_P2P_CLIENT_MASK)) ||
		(concurrent_state == (QDF_STA_MASK | QDF_P2P_GO_MASK))) {
		cds_info("STA & P2P are both enabled");
		/*
		 * The channel number and latency are formatted in
		 * a bit vector then passed on to WMA layer.
		 * +**********************************************+
		 * |bits 31-16 |      bits 15-8    |  bits 7-0    |
		 * |  Unused   | latency - Chan. 1 |  channel no. |
		 * +**********************************************+
		 */
		/* Get the operating channel of the designated vdev */
		first_adapter_operating_channel =
			hdd_get_operating_channel
			(adapter->pHddCtx, adapter->device_mode);
		/* Move the time latency for the adapter to bits 15-8 */
		set_value = set_value << 8;
		/* Store the channel number at bits 7-0 of the bit vector */
		set_value =
			set_value | first_adapter_operating_channel;
		/* Send command to WMA */
		ret = wma_cli_set_command(adapter->sessionId,
				WMA_VDEV_MCC_SET_TIME_LATENCY,
				set_value, VDEV_CMD);
	} else {
		cds_info("%s: MCC is not active. Exit w/o setting latency",
			__func__);
	}
}

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
/**
 * cds_change_sap_channel_with_csa() - Move SAP channel using (E)CSA
 * @adapter: AP adapter
 * @hdd_ap_ctx: AP context
 *
 * Invoke the callback function to change SAP channel using (E)CSA
 *
 * Return: None
 */
void cds_change_sap_channel_with_csa(hdd_adapter_t *adapter,
					hdd_ap_ctx_t *hdd_ap_ctx)
{
	p_cds_contextType cds_ctx;

	cds_ctx = cds_get_global_context();
	if (!cds_ctx) {
		cds_err("Invalid CDS context");
		return;
	}

	if (cds_ctx->sap_restart_chan_switch_cb) {
		cds_info("SAP change change without restart");
		cds_ctx->sap_restart_chan_switch_cb(adapter,
				hdd_ap_ctx->sapConfig.channel,
				hdd_ap_ctx->sapConfig.ch_params.ch_width);
	}
}
#endif

#if defined(FEATURE_WLAN_MCC_TO_SCC_SWITCH) || \
			defined(FEATURE_WLAN_STA_AP_MODE_DFS_DISABLE)
/**
 * cds_restart_sap() - to restart SAP in driver internally
 * @ap_adapter: Pointer to SAP hdd_adapter_t structure
 *
 * Return: None
 */
void cds_restart_sap(hdd_adapter_t *ap_adapter)
{
	hdd_ap_ctx_t *hdd_ap_ctx;
	hdd_hostapd_state_t *hostapd_state;
	QDF_STATUS qdf_status;
	hdd_context_t *hdd_ctx = WLAN_HDD_GET_CTX(ap_adapter);
	tsap_Config_t *sap_config;
	void *sap_ctx;

	hdd_ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(ap_adapter);
	sap_config = &hdd_ap_ctx->sapConfig;
	sap_ctx = hdd_ap_ctx->sapContext;

	mutex_lock(&hdd_ctx->sap_lock);
	if (test_bit(SOFTAP_BSS_STARTED, &ap_adapter->event_flags)) {
		wlan_hdd_del_station(ap_adapter);
		hdd_cleanup_actionframe(hdd_ctx, ap_adapter);
		hostapd_state = WLAN_HDD_GET_HOSTAP_STATE_PTR(ap_adapter);
		qdf_event_reset(&hostapd_state->qdf_stop_bss_event);
		if (QDF_STATUS_SUCCESS == wlansap_stop_bss(sap_ctx)) {
			qdf_status =
				qdf_wait_single_event(&hostapd_state->
					qdf_stop_bss_event,
					SME_CMD_TIMEOUT_VALUE);

			if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
				cds_err("SAP Stop Failed");
				goto end;
			}
		}
		clear_bit(SOFTAP_BSS_STARTED, &ap_adapter->event_flags);
		cds_decr_session_set_pcl(
			ap_adapter->device_mode, ap_adapter->sessionId);
		cds_err("SAP Stop Success");

		if (0 != wlan_hdd_cfg80211_update_apies(ap_adapter)) {
			cds_err("SAP Not able to set AP IEs");
			wlansap_reset_sap_config_add_ie(sap_config,
					eUPDATE_IE_ALL);
			goto end;
		}

		qdf_event_reset(&hostapd_state->qdf_event);
		if (wlansap_start_bss(sap_ctx, hdd_hostapd_sap_event_cb,
				      sap_config,
				      ap_adapter->dev) != QDF_STATUS_SUCCESS) {
			cds_err("SAP Start Bss fail");
			wlansap_reset_sap_config_add_ie(sap_config,
					eUPDATE_IE_ALL);
			goto end;
		}

		cds_info("Waiting for SAP to start");
		qdf_status =
			qdf_wait_single_event(&hostapd_state->qdf_event,
					SME_CMD_TIMEOUT_VALUE);
		wlansap_reset_sap_config_add_ie(sap_config,
				eUPDATE_IE_ALL);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			cds_err("SAP Start failed");
			goto end;
		}
		cds_err("SAP Start Success");
		set_bit(SOFTAP_BSS_STARTED, &ap_adapter->event_flags);
		if (hostapd_state->bssState == BSS_START)
			cds_incr_active_session(ap_adapter->device_mode,
						ap_adapter->sessionId);
		hostapd_state->bCommit = true;
	}
end:
	mutex_unlock(&hdd_ctx->sap_lock);
	return;
}
#endif

#ifdef FEATURE_WLAN_STA_AP_MODE_DFS_DISABLE
/**
 * cds_check_and_restart_sap_with_non_dfs_acs() - Restart SAP with non dfs acs
 *
 * Restarts SAP in non-DFS ACS mode when STA-AP mode DFS is not supported
 *
 * Return: None
 */
void cds_check_and_restart_sap_with_non_dfs_acs(void)
{
	hdd_adapter_t *ap_adapter;
	hdd_context_t *hdd_ctx;
	cds_context_type *cds_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return;
	}

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return;
	}

	if (cds_get_concurrency_mode() != (QDF_STA_MASK | QDF_SAP_MASK)) {
		cds_info("Concurrency mode is not SAP");
		return;
	}

	ap_adapter = hdd_get_adapter(hdd_ctx, QDF_SAP_MODE);
	if (ap_adapter != NULL &&
			test_bit(SOFTAP_BSS_STARTED,
				&ap_adapter->event_flags)
			&& CDS_IS_DFS_CH(ap_adapter->sessionCtx.ap.
				operatingChannel)) {

		cds_warn("STA-AP Mode DFS not supported. Restart SAP with Non DFS ACS");
		ap_adapter->sessionCtx.ap.sapConfig.channel =
			AUTO_CHANNEL_SELECT;
		ap_adapter->sessionCtx.ap.sapConfig.
			acs_cfg.acs_mode = true;

		cds_restart_sap(ap_adapter);
	}
}
#endif

struct cds_conc_connection_info *cds_get_conn_info(uint32_t *len)
{
	struct cds_conc_connection_info *conn_ptr = &conc_connection_list[0];
	*len = MAX_NUMBER_OF_CONC_CONNECTIONS;

	return conn_ptr;
}

#ifdef MPC_UT_FRAMEWORK
QDF_STATUS cds_update_connection_info_utfw(
		uint32_t vdev_id, uint32_t tx_streams, uint32_t rx_streams,
		uint32_t chain_mask, uint32_t type, uint32_t sub_type,
		uint32_t channelid, uint32_t mac_id)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint32_t conn_index = 0, found = 0;
	cds_context_type *cds_ctx;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return status;
	}

	qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
	while (CONC_CONNECTION_LIST_VALID_INDEX(conn_index)) {
		if (vdev_id == conc_connection_list[conn_index].vdev_id) {
			/* debug msg */
			found = 1;
			break;
		}
		conn_index++;
	}
	qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
	if (!found) {
		/* err msg */
		cds_err("can't find vdev_id %d in conc_connection_list",
			vdev_id);
		return status;
	}
	cds_info("--> updating entry at index[%d]", conn_index);

	cds_update_conc_list(conn_index,
			cds_get_mode(type, sub_type),
			channelid, HW_MODE_20_MHZ,
			mac_id, chain_mask, 0, vdev_id, true);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS cds_incr_connection_count_utfw(
		uint32_t vdev_id, uint32_t tx_streams, uint32_t rx_streams,
		uint32_t chain_mask, uint32_t type, uint32_t sub_type,
		uint32_t channelid, uint32_t mac_id)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint32_t conn_index = 0;
	hdd_context_t *hdd_ctx;
	cds_context_type *cds_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return status;
	}

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return status;
	}

	conn_index = cds_get_connection_count();
	if (MAX_NUMBER_OF_CONC_CONNECTIONS <= conn_index) {
		/* err msg */
		cds_err("exceeded max connection limit %d",
			MAX_NUMBER_OF_CONC_CONNECTIONS);
		return status;
	}
	cds_info("--> filling entry at index[%d]", conn_index);

	cds_update_conc_list(conn_index,
				cds_get_mode(type, sub_type),
				channelid, HW_MODE_20_MHZ,
				mac_id, chain_mask, 0, vdev_id, true);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS cds_decr_connection_count_utfw(uint32_t del_all,
	uint32_t vdev_id)
{
	QDF_STATUS status;
	cds_context_type *cds_ctx;
	struct cds_sme_cbacks sme_cbacks;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return QDF_STATUS_E_FAILURE;
	}

	sme_cbacks.sme_get_valid_channels = sme_get_cfg_valid_channels;
	sme_cbacks.sme_get_nss_for_vdev = sme_get_vdev_type_nss;
	if (del_all) {
		status = cds_init_policy_mgr(&sme_cbacks);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			cds_err("Policy manager initialization failed");
			return QDF_STATUS_E_FAILURE;
		}
	} else {
		cds_decr_connection_count(vdev_id);
	}

	return QDF_STATUS_SUCCESS;
}

enum cds_pcl_type get_pcl_from_first_conn_table(enum cds_con_mode type,
		enum cds_conc_priority_mode sys_pref)
{
	if ((sys_pref >= CDS_MAX_CONC_PRIORITY_MODE) ||
		(type >= CDS_MAX_NUM_OF_MODE))
		return CDS_MAX_PCL_TYPE;
	return first_connection_pcl_table[type][sys_pref];
}

enum cds_pcl_type get_pcl_from_second_conn_table(
	enum cds_one_connection_mode idx, enum cds_con_mode type,
	enum cds_conc_priority_mode sys_pref, uint8_t dbs_capable)
{
	if ((idx >= CDS_MAX_ONE_CONNECTION_MODE) ||
		(sys_pref >= CDS_MAX_CONC_PRIORITY_MODE) ||
		(type >= CDS_MAX_NUM_OF_MODE))
		return CDS_MAX_PCL_TYPE;
	if (dbs_capable)
		return (*second_connection_pcl_dbs_table)[idx][type][sys_pref];
	else
		return second_connection_pcl_nodbs_table[idx][type][sys_pref];
}

enum cds_pcl_type get_pcl_from_third_conn_table(
	enum cds_two_connection_mode idx, enum cds_con_mode type,
	enum cds_conc_priority_mode sys_pref, uint8_t dbs_capable)
{
	if ((idx >= CDS_MAX_TWO_CONNECTION_MODE) ||
		(sys_pref >= CDS_MAX_CONC_PRIORITY_MODE) ||
		(type >= CDS_MAX_NUM_OF_MODE))
		return CDS_MAX_PCL_TYPE;
	if (dbs_capable)
		return (*third_connection_pcl_dbs_table)[idx][type][sys_pref];
	else
		return third_connection_pcl_nodbs_table[idx][type][sys_pref];
}
#endif

/**
 * cds_convert_device_mode_to_qdf_type() - provides the
 * type translation from HDD to policy manager type
 * @device_mode: Generic connection mode type
 *
 *
 * This function provides the type translation
 *
 * Return: cds_con_mode enum
 */
enum cds_con_mode cds_convert_device_mode_to_qdf_type(
			enum tQDF_ADAPTER_MODE device_mode)
{
	enum cds_con_mode mode = CDS_MAX_NUM_OF_MODE;
	switch (device_mode) {
	case QDF_STA_MODE:
		mode = CDS_STA_MODE;
		break;
	case QDF_P2P_CLIENT_MODE:
		mode = CDS_P2P_CLIENT_MODE;
		break;
	case QDF_P2P_GO_MODE:
		mode = CDS_P2P_GO_MODE;
		break;
	case QDF_SAP_MODE:
		mode = CDS_SAP_MODE;
		break;
	case QDF_IBSS_MODE:
		mode = CDS_IBSS_MODE;
		break;
	default:
		cds_err("Unsupported mode (%d)",
			device_mode);
	}
	return mode;
}

/**
 * cds_get_conparam() - Get the connection mode parameters
 *
 * Return the connection mode parameter set by insmod or set during statically
 * linked driver
 *
 * Return: enum tQDF_GLOBAL_CON_MODE
 */
enum tQDF_GLOBAL_CON_MODE cds_get_conparam(void)
{
	enum tQDF_GLOBAL_CON_MODE con_mode;
	con_mode = hdd_get_conparam();
	return con_mode;
}

/**
 * cds_concurrent_open_sessions_running() - Checks for concurrent open session
 *
 * Checks if more than one open session is running for all the allowed modes
 * in the driver
 *
 * Return: True if more than one open session exists, False otherwise
 */
bool cds_concurrent_open_sessions_running(void)
{
	uint8_t i = 0;
	uint8_t j = 0;
	hdd_context_t *pHddCtx;

	pHddCtx = cds_get_context(QDF_MODULE_ID_HDD);
	if (NULL != pHddCtx) {
		for (i = 0; i < QDF_MAX_NO_OF_MODE; i++)
			j += pHddCtx->no_of_open_sessions[i];
	}

	return j > 1;
}

/**
 * cds_concurrent_beaconing_sessions_running() - Checks for concurrent beaconing
 * entities
 *
 * Checks if multiple beaconing sessions are running i.e., if SAP or GO or IBSS
 * are beaconing together
 *
 * Return: True if multiple entities are beaconing together, False otherwise
 */
bool cds_concurrent_beaconing_sessions_running(void)
{
	uint8_t i = 0;
	hdd_context_t *pHddCtx;

	pHddCtx = cds_get_context(QDF_MODULE_ID_HDD);
	if (NULL != pHddCtx) {
		i = pHddCtx->no_of_open_sessions[QDF_SAP_MODE] +
			pHddCtx->no_of_open_sessions[QDF_P2P_GO_MODE] +
			pHddCtx->no_of_open_sessions[QDF_IBSS_MODE];
	}
	return i > 1;
}


/**
 * cds_max_concurrent_connections_reached() - Check if max conccurrency is
 * reached
 *
 * Checks for presence of concurrency where more than one connection exists
 *
 * Return: True if the max concurrency is reached, False otherwise
 *
 * Example:
 *    STA + STA (wlan0 and wlan1 are connected) - returns true
 *    STA + STA (wlan0 connected and wlan1 disconnected) - returns false
 *    DUT with P2P-GO + P2P-CLIENT connection) - returns true
 *
 */
bool cds_max_concurrent_connections_reached(void)
{
	uint8_t i = 0, j = 0;
	hdd_context_t *pHddCtx;

	pHddCtx = cds_get_context(QDF_MODULE_ID_HDD);
	if (NULL != pHddCtx) {
		for (i = 0; i < QDF_MAX_NO_OF_MODE; i++)
			j += pHddCtx->no_of_active_sessions[i];
		return j >
			(pHddCtx->config->
			 gMaxConcurrentActiveSessions - 1);
	}

	return false;
}

/**
 * cds_clear_concurrent_session_count() - Clear active session count
 *
 * Clears the active session count for all modes
 *
 * Return: None
 */
void cds_clear_concurrent_session_count(void)
{
	uint8_t i = 0;
	hdd_context_t *pHddCtx;

	pHddCtx = cds_get_context(QDF_MODULE_ID_HDD);
	if (NULL != pHddCtx) {
		for (i = 0; i < QDF_MAX_NO_OF_MODE; i++)
			pHddCtx->no_of_active_sessions[i] = 0;
	}
}

/**
 * cds_is_multiple_active_sta_sessions() - Check for multiple STA connections
 *
 * Checks if multiple active STA connection are in the driver
 *
 * Return: True if multiple STA sessions are present, False otherwise
 *
 */
bool cds_is_multiple_active_sta_sessions(void)
{
	hdd_context_t *pHddCtx;
	uint8_t j = 0;

	pHddCtx = cds_get_context(QDF_MODULE_ID_HDD);
	if (NULL != pHddCtx)
		j = pHddCtx->no_of_active_sessions[QDF_STA_MODE];

	return j > 1;
}

/**
 * cds_is_sta_active_connection_exists() - Check if a STA connection is active
 *
 * Checks if there is atleast one active STA connection in the driver
 *
 * Return: True if an active STA session is present, False otherwise
 */
bool cds_is_sta_active_connection_exists(void)
{
	hdd_context_t *pHddCtx;
	uint8_t j = 0;

	pHddCtx = cds_get_context(QDF_MODULE_ID_HDD);
	if (NULL != pHddCtx)
		j = pHddCtx->no_of_active_sessions[QDF_STA_MODE];

	return j ? true : false;
}

/**
 * qdf_wait_for_connection_update() - Wait for hw mode command to get processed
 *
 * Waits for CONNECTION_UPDATE_TIMEOUT duration until the set hw mode
 * response sets the event connection_update_done_evt
 *
 * Return: QDF_STATUS
 */
QDF_STATUS qdf_wait_for_connection_update(void)
{
	QDF_STATUS status;
	p_cds_contextType cds_context;

	cds_context = cds_get_global_context();
	if (!cds_context) {
		cds_err("Invalid CDS context");
		return QDF_STATUS_E_FAILURE;
	}

	status = qdf_wait_single_event(
			&cds_context->connection_update_done_evt,
			CONNECTION_UPDATE_TIMEOUT);

	if (!QDF_IS_STATUS_SUCCESS(status)) {
		cds_err("wait for event failed");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * qdf_reset_connection_update() - Reset connection update event
 *
 * Resets the concurrent connection update event
 *
 * Return: QDF_STATUS
 */
QDF_STATUS qdf_reset_connection_update(void)
{
	QDF_STATUS status;
	p_cds_contextType cds_context;

	cds_context = cds_get_global_context();
	if (!cds_context) {
		cds_err("Invalid CDS context");
		return QDF_STATUS_E_FAILURE;
	}

	status = qdf_event_reset(&cds_context->connection_update_done_evt);

	if (!QDF_IS_STATUS_SUCCESS(status)) {
		cds_err("clear event failed");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * qdf_set_connection_update() - Set connection update event
 *
 * Sets the concurrent connection update event
 *
 * Return: QDF_STATUS
 */
QDF_STATUS qdf_set_connection_update(void)
{
	QDF_STATUS status;
	p_cds_contextType cds_context;

	cds_context = cds_get_global_context();
	if (!cds_context) {
		cds_err("Invalid CDS context");
		return QDF_STATUS_E_FAILURE;
	}

	status = qdf_event_set(&cds_context->connection_update_done_evt);

	if (!QDF_IS_STATUS_SUCCESS(status)) {
		cds_err("set event failed");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * qdf_init_connection_update() - Initialize connection update event
 *
 * Initializes the concurrent connection update event
 *
 * Return: QDF_STATUS
 */
QDF_STATUS qdf_init_connection_update(void)
{
	QDF_STATUS qdf_status;
	p_cds_contextType cds_context;

	cds_context = cds_get_global_context();
	if (!cds_context) {
		cds_err("Invalid CDS context");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_status = qdf_event_create(&cds_context->connection_update_done_evt);

	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		cds_err("init event failed");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * cds_get_current_pref_hw_mode_dbs_2x2() - Get the current preferred hw mode
 *
 * Get the preferred hw mode based on the current connection combinations
 *
 * Return: No change (CDS_NOP), MCC (CDS_SINGLE_MAC),
 *         DBS (CDS_DBS), SBS (CDS_SBS)
 */
static enum cds_conc_next_action cds_get_current_pref_hw_mode_dbs_2x2(void)
{
	uint32_t num_connections;
	uint8_t band1, band2, band3;
	struct sir_hw_mode_params hw_mode;
	QDF_STATUS status;
	hdd_context_t *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return CDS_NOP;
	}

	status = wma_get_current_hw_mode(&hw_mode);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		cds_err("wma_get_current_hw_mode failed");
		return CDS_NOP;
	}

	num_connections = cds_get_connection_count();

	cds_debug("chan[0]:%d chan[1]:%d chan[2]:%d num_connections:%d dbs:%d",
		conc_connection_list[0].chan, conc_connection_list[1].chan,
		conc_connection_list[2].chan, num_connections, hw_mode.dbs_cap);

	/* If the band of operation of both the MACs is the same,
	 * single MAC is preferred, otherwise DBS is preferred.
	 */
	switch (num_connections) {
	case 1:
		band1 = cds_chan_to_band(conc_connection_list[0].chan);
		if (band1 == CDS_BAND_2GHZ)
			return CDS_DBS;
		else
			return CDS_NOP;
	case 2:
		band1 = cds_chan_to_band(conc_connection_list[0].chan);
		band2 = cds_chan_to_band(conc_connection_list[1].chan);
		if ((band1 == CDS_BAND_2GHZ) || (band2 == CDS_BAND_2GHZ)) {
			if (!hw_mode.dbs_cap)
				return CDS_DBS;
			else
				return CDS_NOP;
		} else if ((band1 == CDS_BAND_5GHZ) &&
				(band2 == CDS_BAND_5GHZ)) {
			if (CDS_IS_CHANNEL_VALID_5G_SBS(
				conc_connection_list[0].chan,
				conc_connection_list[1].chan)) {
				if (!hw_mode.sbs_cap)
					return CDS_SBS;
				else
					return CDS_NOP;
			} else {
				if (hw_mode.sbs_cap || hw_mode.dbs_cap)
					return CDS_SINGLE_MAC;
				else
					return CDS_NOP;
			}
		} else
			return CDS_NOP;
	case 3:
		band1 = cds_chan_to_band(conc_connection_list[0].chan);
		band2 = cds_chan_to_band(conc_connection_list[1].chan);
		band3 = cds_chan_to_band(conc_connection_list[2].chan);
		if ((band1 == CDS_BAND_2GHZ) || (band2 == CDS_BAND_2GHZ) ||
			(band3 == CDS_BAND_2GHZ)) {
			if (!hw_mode.dbs_cap)
				return CDS_DBS;
			else
				return CDS_NOP;
		} else if ((band1 == CDS_BAND_5GHZ) &&
				(band2 == CDS_BAND_5GHZ) &&
					(band3 == CDS_BAND_5GHZ)) {
			if (CDS_IS_CHANNEL_VALID_5G_SBS(
				conc_connection_list[0].chan,
				conc_connection_list[2].chan) &&
				CDS_IS_CHANNEL_VALID_5G_SBS(
				conc_connection_list[1].chan,
				conc_connection_list[2].chan) &&
				CDS_IS_CHANNEL_VALID_5G_SBS(
				conc_connection_list[0].chan,
				conc_connection_list[1].chan)) {
				if (!hw_mode.sbs_cap)
					return CDS_SBS;
				else
					return CDS_NOP;
			} else {
				if (hw_mode.sbs_cap || hw_mode.dbs_cap)
					return CDS_SINGLE_MAC;
				else
					return CDS_NOP;
			}
		} else
			return CDS_NOP;
	default:
		cds_err("unexpected num_connections value %d",
				num_connections);
		return CDS_NOP;
	}
}

/**
 * cds_get_current_pref_hw_mode_dbs_1x1() - Get the current preferred hw mode
 *
 * Get the preferred hw mode based on the current connection combinations
 *
 * Return: No change (CDS_NOP), MCC (CDS_SINGLE_MAC_UPGRADE),
 *         DBS (CDS_DBS_DOWNGRADE)
 */
static enum cds_conc_next_action cds_get_current_pref_hw_mode_dbs_1x1(void)
{
	uint32_t num_connections;
	uint8_t band1, band2, band3;
	struct sir_hw_mode_params hw_mode;
	QDF_STATUS status;
	hdd_context_t *hdd_ctx;
	enum cds_conc_next_action next_action;
	cds_context_type *cds_ctx;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return CDS_NOP;
	}

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return CDS_NOP;
	}

	status = wma_get_current_hw_mode(&hw_mode);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		cds_err("wma_get_current_hw_mode failed");
		return CDS_NOP;
	}

	num_connections = cds_get_connection_count();

	qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
	cds_debug("chan[0]:%d chan[1]:%d chan[2]:%d num_connections:%d dbs:%d",
		conc_connection_list[0].chan, conc_connection_list[1].chan,
		conc_connection_list[2].chan, num_connections, hw_mode.dbs_cap);

	/* If the band of operation of both the MACs is the same,
	 * single MAC is preferred, otherwise DBS is preferred.
	 */
	switch (num_connections) {
	case 1:
		/* The driver would already be in the required hw mode */
		next_action = CDS_NOP;
		break;
	case 2:
		band1 = cds_chan_to_band(conc_connection_list[0].chan);
		band2 = cds_chan_to_band(conc_connection_list[1].chan);
		if ((band1 == band2) && (hw_mode.dbs_cap))
			next_action = CDS_SINGLE_MAC_UPGRADE;
		else if ((band1 != band2) && (!hw_mode.dbs_cap))
			next_action = CDS_DBS_DOWNGRADE;
		else
			next_action = CDS_NOP;

		break;

	case 3:
		band1 = cds_chan_to_band(conc_connection_list[0].chan);
		band2 = cds_chan_to_band(conc_connection_list[1].chan);
		band3 = cds_chan_to_band(conc_connection_list[2].chan);
		if (((band1 == band2) && (band2 == band3)) &&
				(hw_mode.dbs_cap)) {
			next_action = CDS_SINGLE_MAC_UPGRADE;
		} else if (((band1 != band2) || (band2 != band3) ||
					(band1 != band3)) &&
					(!hw_mode.dbs_cap)) {
			next_action = CDS_DBS_DOWNGRADE;
		} else {
			next_action = CDS_NOP;
		}
		break;
	default:
		cds_err("unexpected num_connections value %d",
				num_connections);
		next_action = CDS_NOP;
		break;
	}

	qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
	return next_action;

}

/**
 * cds_restart_opportunistic_timer() - Restarts opportunistic timer
 * @check_state: check timer state if this flag is set, else restart
 *               irrespective of state
 *
 * Restarts opportunistic timer for DBS_OPPORTUNISTIC_TIME seconds.
 * Check if current state is RUNNING if check_state is set, else
 * restart the timer irrespective of state.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cds_restart_opportunistic_timer(bool check_state)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	p_cds_contextType cds_ctx;

	cds_ctx = cds_get_global_context();
	if (!cds_ctx) {
		cds_err("Invalid CDS context");
		return status;
	}

	if (check_state &&
			QDF_TIMER_STATE_RUNNING !=
			cds_ctx->dbs_opportunistic_timer.state)
		return status;

	qdf_mc_timer_stop(&cds_ctx->dbs_opportunistic_timer);

	status = qdf_mc_timer_start(
			&cds_ctx->dbs_opportunistic_timer,
			DBS_OPPORTUNISTIC_TIME * 1000);

	if (!QDF_IS_STATUS_SUCCESS(status)) {
		cds_err("failed to start opportunistic timer");
		return status;
	}

	return status;
}

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
/**
 * cds_register_sap_restart_channel_switch_cb() - Register callback for SAP
 * channel switch without restart
 * @sap_restart_chan_switch_cb: Callback to perform channel switch
 *
 * Registers callback to perform channel switch without having to actually
 * restart the beaconing entity
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cds_register_sap_restart_channel_switch_cb(
		void (*sap_restart_chan_switch_cb)(void *, uint32_t, uint32_t))
{
	p_cds_contextType cds_ctx;

	cds_ctx = cds_get_global_context();
	if (!cds_ctx) {
		cds_err("Invalid CDS context");
		return QDF_STATUS_E_FAILURE;
	}

	cds_ctx->sap_restart_chan_switch_cb = sap_restart_chan_switch_cb;
	return QDF_STATUS_SUCCESS;
}

/**
 * cds_deregister_sap_restart_channel_switch_cb() - De-Register callback for SAP
 * channel switch without restart
 *
 * De Registers callback to perform channel switch
 *
 * Return: QDF_STATUS Enumeration
 */
QDF_STATUS cds_deregister_sap_restart_channel_switch_cb(void)
{
	p_cds_contextType cds_ctx;

	cds_ctx = cds_get_global_context();
	if (!cds_ctx) {
		cds_err("Invalid CDS context");
		return QDF_STATUS_E_FAILURE;
	}

	cds_ctx->sap_restart_chan_switch_cb = NULL;
	return QDF_STATUS_SUCCESS;
}

#endif



/**
 * cds_get_nondfs_preferred_channel() - to get non-dfs preferred channel
 *                                           for given mode
 * @mode: mode for which preferred non-dfs channel is requested
 * @for_existing_conn: flag to indicate if preferred channel is requested
 *                     for existing connection
 *
 * this routine will return non-dfs channel
 * 1) for getting non-dfs preferred channel, first we check if there are any
 *    other connection exist whose channel is non-dfs. if yes then return that
 *    channel so that we can accommodate upto 3 mode concurrency.
 * 2) if there no any other connection present then query concurrency module
 *    to give preferred channel list. once we get preferred channel list, loop
 *    through list to find first non-dfs channel from ascending order.
 *
 * Return: uint8_t non-dfs channel
 */
uint8_t
cds_get_nondfs_preferred_channel(enum cds_con_mode mode,
		bool for_existing_conn)
{
	uint8_t pcl_channels[QDF_MAX_NUM_CHAN];
	uint8_t pcl_weight[QDF_MAX_NUM_CHAN];

	/*
	 * in worst case if we can't find any channel at all
	 * then return 2.4G channel, so atleast we won't fall
	 * under 5G MCC scenario
	 */
	uint8_t channel = CDS_24_GHZ_CHANNEL_6;
	uint32_t i, pcl_len;

	if (true == for_existing_conn) {
		/*
		 * First try to see if there is any non-dfs channel already
		 * present in current connection table. If yes then return
		 * that channel
		 */
		if (true == cds_is_any_nondfs_chnl_present(&channel))
			return channel;

		if (QDF_STATUS_SUCCESS != cds_get_pcl_for_existing_conn(mode,
					&pcl_channels[0], &pcl_len,
					pcl_weight, QDF_ARRAY_SIZE(pcl_weight)))
			return channel;
	} else {
		if (QDF_STATUS_SUCCESS != cds_get_pcl(mode,
					&pcl_channels[0], &pcl_len,
					pcl_weight, QDF_ARRAY_SIZE(pcl_weight)))
			return channel;
	}

	for (i = 0; i < pcl_len; i++) {
		if (CDS_IS_DFS_CH(pcl_channels[i])) {
			continue;
		} else {
			channel = pcl_channels[i];
			break;
		}
	}
	return channel;
}


/**
 * cds_is_any_nondfs_chnl_present() - Find any non-dfs channel from conc table
 * @channel: pointer to channel which needs to be filled
 *
 * In-case if any connection is already present whose channel is none dfs then
 * return that channel
 *
 * Return: true up-on finding non-dfs channel else false
 */
bool cds_is_any_nondfs_chnl_present(uint8_t *channel)
{
	cds_context_type *cds_ctx;
	bool status = false;
	uint32_t conn_index = 0;
	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);

	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return false;
	}
	qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
			conn_index++) {
		if (conc_connection_list[conn_index].in_use &&
		    !CDS_IS_DFS_CH(conc_connection_list[conn_index].chan)) {
			*channel = conc_connection_list[conn_index].chan;
			status = true;
		}
	}
	qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
	return status;
}

/**
 * cds_is_any_dfs_beaconing_session_present() - to find if any DFS session
 * @channel: pointer to channel number that needs to filled
 *
 * If any beaconing session such as SAP or GO present and it is on DFS channel
 * then this function will return true
 *
 * Return: true if session is on DFS or false if session is on non-dfs channel
 */
bool cds_is_any_dfs_beaconing_session_present(uint8_t *channel)
{
	cds_context_type *cds_ctx;
	struct cds_conc_connection_info *conn_info;
	bool status = false;
	uint32_t conn_index = 0;
	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);

	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return false;
	}
	qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
			conn_index++) {
		conn_info = &conc_connection_list[conn_index];
		if (conn_info->in_use && CDS_IS_DFS_CH(conn_info->chan) &&
		    (CDS_SAP_MODE == conn_info->mode ||
		     CDS_P2P_GO_MODE == conn_info->mode)) {
			*channel = conc_connection_list[conn_index].chan;
			status = true;
		}
	}
	qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
	return status;
}

/**
 * cds_get_valid_chans() - Get the valid channel list
 * @chan_list: Pointer to the valid channel list
 * @list_len: Pointer to the length of the valid channel list
 *
 * Gets the valid channel list filtered by band
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cds_get_valid_chans(uint8_t *chan_list, uint32_t *list_len)
{
	QDF_STATUS status;
	hdd_context_t *hdd_ctx;
	cds_context_type *cds_ctx;

	*list_len = 0;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return QDF_STATUS_E_FAILURE;
	}

	if (!cds_ctx->sme_get_valid_channels) {
		cds_err("sme_get_valid_chans callback is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	*list_len = QDF_MAX_NUM_CHAN;
	status = cds_ctx->sme_get_valid_channels(hdd_ctx->hHal,
					chan_list, list_len);
	if (QDF_IS_STATUS_ERROR(status)) {
		cds_err("Error in getting valid channels");
		*list_len = 0;
		return status;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * cds_get_nss_for_vdev() - Get the allowed nss value for the
 * vdev
 * @dev_mode: connection type.
 * @nss2g: Pointer to the 2G Nss parameter.
 * @nss5g: Pointer to the 5G Nss parameter.
 *
 * Fills the 2G and 5G Nss values based on connection type.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cds_get_nss_for_vdev(enum cds_con_mode mode,
		uint8_t *nss_2g, uint8_t *nss_5g)
{
	hdd_context_t *hdd_ctx;
	cds_context_type *cds_ctx;
	enum tQDF_ADAPTER_MODE dev_mode;

	switch (mode) {
	case CDS_STA_MODE:
		dev_mode = QDF_STA_MODE;
		break;
	case CDS_SAP_MODE:
		dev_mode = QDF_SAP_MODE;
		break;
	case CDS_P2P_CLIENT_MODE:
		dev_mode = QDF_P2P_CLIENT_MODE;
		break;
	case CDS_P2P_GO_MODE:
		dev_mode = QDF_P2P_GO_MODE;
		break;
	case CDS_IBSS_MODE:
		dev_mode = QDF_IBSS_MODE;
		break;
	default:
		cds_err("Invalid mode to get allowed NSS value");
		return QDF_STATUS_E_FAILURE;
	};

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return QDF_STATUS_E_FAILURE;
	}

	if (!cds_ctx->sme_get_nss_for_vdev) {
		cds_err("sme_get_nss_for_vdev callback is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	cds_ctx->sme_get_nss_for_vdev(hdd_ctx->hHal,
					dev_mode, nss_2g, nss_5g);

	return QDF_STATUS_SUCCESS;
}

/**
 * cds_list_has_24GHz_channel() - Check if list contains 2.4GHz channels
 * @channel_list: Channel list
 * @list_len: Length of the channel list
 *
 * Checks if the channel list contains atleast one 2.4GHz channel
 *
 * Return: True if 2.4GHz channel is present, false otherwise
 */
bool cds_list_has_24GHz_channel(uint8_t *channel_list,
					uint32_t list_len)
{
	uint32_t i;

	for (i = 0; i < list_len; i++) {
		if (CDS_IS_CHANNEL_24GHZ(channel_list[i]))
			return true;
	}

	return false;
}

/**
 * cds_set_sap_mandatory_channels() - Set the mandatory channel for SAP
 * @channels: Channel list to be set
 * @len: Length of the channel list
 *
 * Sets the channels for the mandatory channel list along with the length of
 * of the channel list.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cds_set_sap_mandatory_channels(uint8_t *channels, uint32_t len)
{
	cds_context_type *cds_ctx;
	uint32_t i;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return QDF_STATUS_E_FAILURE;
	}

	if (!len) {
		cds_err("No mandatory freq/chan configured");
		return QDF_STATUS_E_FAILURE;
	}

	if (!cds_list_has_24GHz_channel(channels, len)) {
		cds_err("2.4GHz channels missing, this is not expected");
		return QDF_STATUS_E_FAILURE;
	}

	cds_debug("mandatory chan length:%d",
			cds_ctx->sap_mandatory_channels_len);

	for (i = 0; i < len; i++) {
		cds_ctx->sap_mandatory_channels[i] = channels[i];
		cds_debug("chan:%d", cds_ctx->sap_mandatory_channels[i]);
	}

	cds_ctx->sap_mandatory_channels_len = len;

	return QDF_STATUS_SUCCESS;
}

/**
 * cds_reset_sap_mandatory_channels() - Reset the SAP mandatory channels
 *
 * Resets the SAP mandatory channel list and the length of the list
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cds_reset_sap_mandatory_channels(void)
{
	cds_context_type *cds_ctx;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return QDF_STATUS_E_FAILURE;
	}

	cds_ctx->sap_mandatory_channels_len = 0;
	qdf_mem_zero(cds_ctx->sap_mandatory_channels,
		QDF_ARRAY_SIZE(cds_ctx->sap_mandatory_channels));

	return QDF_STATUS_SUCCESS;
}

/**
 * cds_is_sap_mandatory_channel_set() - Checks if SAP mandatory channel is set
 *
 * Checks if any mandatory channel is set for SAP operation
 *
 * Return: True if mandatory channel is set, false otherwise
 */
bool cds_is_sap_mandatory_channel_set(void)
{
	cds_context_type *cds_ctx;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return false;
	}

	if (cds_ctx->sap_mandatory_channels_len)
		return true;
	else
		return false;
}

/**
 * cds_modify_sap_pcl_based_on_mandatory_channel() - Modify SAPs PCL based on
 * mandatory channel list
 * @pcl_list_org: Pointer to the preferred channel list to be trimmed
 * @weight_list_org: Pointer to the weights of the preferred channel list
 * @pcl_len_org: Pointer to the length of the preferred chanel list
 *
 * Modifies the preferred channel list of SAP based on the mandatory channel
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cds_modify_sap_pcl_based_on_mandatory_channel(uint8_t *pcl_list_org,
						uint8_t *weight_list_org,
						uint32_t *pcl_len_org)
{
	cds_context_type *cds_ctx;
	uint32_t i, j, pcl_len = 0;
	uint8_t pcl_list[QDF_MAX_NUM_CHAN];
	uint8_t weight_list[QDF_MAX_NUM_CHAN];
	bool found;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return QDF_STATUS_E_FAILURE;
	}

	if (!cds_ctx->sap_mandatory_channels_len)
		return QDF_STATUS_SUCCESS;

	if (!cds_list_has_24GHz_channel(cds_ctx->sap_mandatory_channels,
			cds_ctx->sap_mandatory_channels_len)) {
		cds_err("fav channel list is missing 2.4GHz channels");
		return QDF_STATUS_E_FAILURE;
	}

	for (i = 0; i < cds_ctx->sap_mandatory_channels_len; i++)
		cds_debug("fav chan:%d", cds_ctx->sap_mandatory_channels[i]);

	for (i = 0; i < *pcl_len_org; i++) {
		found = false;
		for (j = 0; j < cds_ctx->sap_mandatory_channels_len; j++) {
			if (pcl_list_org[i] ==
			    cds_ctx->sap_mandatory_channels[j]) {
				found = true;
				break;
			}
		}
		if (found) {
			pcl_list[pcl_len] = pcl_list_org[i];
			weight_list[pcl_len++] = weight_list_org[i];
		}
	}

	qdf_mem_zero(pcl_list_org, QDF_ARRAY_SIZE(pcl_list_org));
	qdf_mem_zero(weight_list_org, QDF_ARRAY_SIZE(weight_list_org));
	qdf_mem_copy(pcl_list_org, pcl_list, pcl_len);
	qdf_mem_copy(weight_list_org, weight_list, pcl_len);
	*pcl_len_org = pcl_len;

	return QDF_STATUS_SUCCESS;
}

/**
 * cds_get_sap_mandatory_channel() - Get the mandatory channel for SAP
 * @chan: Pointer to the SAP mandatory channel
 *
 * Gets the mandatory channel for SAP operation
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cds_get_sap_mandatory_channel(uint32_t *chan)
{
	QDF_STATUS status;
	struct sir_pcl_list pcl;

	qdf_mem_zero(&pcl, sizeof(pcl));

	status = cds_get_pcl_for_existing_conn(CDS_SAP_MODE,
			pcl.pcl_list, &pcl.pcl_len,
			pcl.weight_list, QDF_ARRAY_SIZE(pcl.weight_list));
	if (QDF_IS_STATUS_ERROR(status)) {
		cds_err("Unable to get PCL for SAP");
		return status;
	}

	/* No existing SAP connection and hence a new SAP connection might be
	 * coming up.
	 */
	if (!pcl.pcl_len) {
		cds_info("cds_get_pcl_for_existing_conn returned no pcl");
		status = cds_get_pcl(CDS_SAP_MODE,
				pcl.pcl_list, &pcl.pcl_len,
				pcl.weight_list,
				QDF_ARRAY_SIZE(pcl.weight_list));
		if (QDF_IS_STATUS_ERROR(status)) {
			cds_err("Unable to get PCL for SAP: cds_get_pcl");
			return status;
		}
	}

	status = cds_modify_sap_pcl_based_on_mandatory_channel(pcl.pcl_list,
							pcl.weight_list,
							&pcl.pcl_len);
	if (QDF_IS_STATUS_ERROR(status)) {
		cds_err("Unable to modify SAP PCL");
		return status;
	}

	*chan = pcl.pcl_list[0];
	cds_info("mandatory channel:%d", *chan);
	return QDF_STATUS_SUCCESS;
}

/**
 * cds_get_valid_chan_weights() - Get the weightage for all valid channels
 * @weight: Pointer to the structure containing pcl, saved channel list and
 * weighed channel list
 *
 * Provides the weightage for all valid channels. This compares the PCL list
 * with the valid channel list. The channels present in the PCL get their
 * corresponding weightage and the non-PCL channels get the default weightage
 * of WEIGHT_OF_NON_PCL_CHANNELS.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cds_get_valid_chan_weights(struct sir_pcl_chan_weights *weight)
{
	uint32_t i, j;
	cds_context_type *cds_ctx;
	struct cds_conc_connection_info info;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return QDF_STATUS_E_FAILURE;
	}

	if (!weight->pcl_list) {
		cds_err("Invalid pcl");
		return QDF_STATUS_E_FAILURE;
	}

	if (!weight->saved_chan_list) {
		cds_err("Invalid valid channel list");
		return QDF_STATUS_E_FAILURE;
	}

	if (!weight->weighed_valid_list) {
		cds_err("Invalid weighed valid channel list");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mem_set(weight->weighed_valid_list, QDF_MAX_NUM_CHAN,
		    WEIGHT_OF_DISALLOWED_CHANNELS);

	if (cds_mode_specific_connection_count(CDS_STA_MODE, NULL) > 0) {
		/*
		 * Store the STA mode's parameter and temporarily delete it
		 * from the concurrency table. This way the allow concurrency
		 * check can be used as though a new connection is coming up,
		 * allowing to detect the disallowed channels.
		 */
		cds_store_and_del_conn_info(CDS_STA_MODE, &info);
		/*
		 * There is a small window between releasing the above lock
		 * and acquiring the same in cds_allow_concurrency, below!
		 */
		for (i = 0; i < weight->saved_num_chan; i++) {
			if (cds_allow_concurrency(CDS_STA_MODE,
						  weight->saved_chan_list[i],
						  HW_MODE_20_MHZ)) {
				weight->weighed_valid_list[i] =
					WEIGHT_OF_NON_PCL_CHANNELS;
			}
		}

		/* Restore the connection info */
		cds_restore_deleted_conn_info(&info);
	}

	for (i = 0; i < weight->saved_num_chan; i++) {
		for (j = 0; j < weight->pcl_len; j++) {
			if (weight->saved_chan_list[i] == weight->pcl_list[j]) {
				weight->weighed_valid_list[i] =
					weight->weight_list[j];
				break;
			}
		}
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * cds_set_hw_mode_on_channel_switch() - Set hw mode after channel switch
 * @session_id: Session ID
 *
 * Sets hw mode after doing a channel switch
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cds_set_hw_mode_on_channel_switch(uint8_t session_id)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE, qdf_status;
	cds_context_type *cds_ctx;
	enum cds_conc_next_action action;
	hdd_context_t *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return status;
	}

	if (!wma_is_hw_dbs_capable()) {
		cds_err("PM/DBS is disabled");
		return status;
	}

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_ERROR,
				FL("Invalid CDS Context"));
		return status;
	}

	action = (*cds_get_current_pref_hw_mode_ptr)();
	if ((action != CDS_DBS_DOWNGRADE) &&
	    (action != CDS_SINGLE_MAC_UPGRADE)) {
		QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_ERROR,
				FL("Invalid action: %d"), action);
		status = QDF_STATUS_SUCCESS;
		goto done;
	}

	QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_INFO,
			FL("action:%d session id:%d"),
			action, session_id);

	/* Opportunistic timer is started, PM will check if MCC upgrade can be
	 * done on timer expiry. This avoids any possible ping pong effect
	 * as well.
	 */
	if (action == CDS_SINGLE_MAC_UPGRADE) {
		qdf_status = cds_restart_opportunistic_timer(false);
		if (QDF_IS_STATUS_SUCCESS(qdf_status))
			cds_info("opportunistic timer for MCC upgrade");
		goto done;
	}

	/* For DBS, we want to move right away to DBS mode */
	status = cds_next_actions(session_id, action,
			SIR_UPDATE_REASON_CHANNEL_SWITCH);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_ERROR,
				FL("no set hw mode command was issued"));
		goto done;
	}
done:
	/* success must be returned only when a set hw mode was done */
	return status;
}

/**
 * cds_dump_connection_status_info() - Dump the concurrency information
 *
 * Prints the concurrency information such as tx/rx spatial stream, chainmask,
 * etc.
 *
 * Return: None
 */
void cds_dump_connection_status_info(void)
{
	cds_context_type *cds_ctx;
	uint32_t i;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return;
	}

	qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		cds_debug("%d: use:%d vdev:%d mode:%d mac:%d chan:%d orig chainmask:%d orig nss:%d bw:%d",
				i, conc_connection_list[i].in_use,
				conc_connection_list[i].vdev_id,
				conc_connection_list[i].mode,
				conc_connection_list[i].mac,
				conc_connection_list[i].chan,
				conc_connection_list[i].chain_mask,
				conc_connection_list[i].original_nss,
				conc_connection_list[i].bw);
	}
	qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
}

/**
 * cds_is_any_mode_active_on_band_along_with_session() - Check if any connection
 * mode is active on a band along with the given session
 * @session_id: Session along which active sessions are looked for
 * @band: Operating frequency band of the connection
 * CDS_BAND_24: Looks for active connection on 2.4 GHz only
 * CDS_BAND_5: Looks for active connection on 5 GHz only
 *
 * Checks if any of the connection mode is active on a given frequency band
 *
 * Return: True if any connection is active on a given band, false otherwise
 */
bool cds_is_any_mode_active_on_band_along_with_session(uint8_t session_id,
						       enum cds_band band)
{
	cds_context_type *cds_ctx;
	uint32_t i;
	bool status = false;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		status = false;
		goto send_status;
	}

	qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		switch (band) {
		case CDS_BAND_24:
			if ((conc_connection_list[i].vdev_id != session_id) &&
			    (conc_connection_list[i].in_use) &&
			    (CDS_IS_CHANNEL_24GHZ(
			    conc_connection_list[i].chan))) {
				status = true;
				goto release_mutex_and_send_status;
			}
			break;
		case CDS_BAND_5:
			if ((conc_connection_list[i].vdev_id != session_id) &&
			    (conc_connection_list[i].in_use) &&
			    (CDS_IS_CHANNEL_5GHZ(
			    conc_connection_list[i].chan))) {
				status = true;
				goto release_mutex_and_send_status;
			}
			break;
		default:
			cds_err("Invalid band option:%d", band);
			status = false;
			goto release_mutex_and_send_status;
		}
	}
release_mutex_and_send_status:
	qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
send_status:
	return status;
}

/**
 * cds_get_mac_id_by_session_id() - Get MAC ID for a given session ID
 * @session_id: Session ID
 * @mac_id: Pointer to the MAC ID
 *
 * Gets the MAC ID for a given session ID
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cds_get_mac_id_by_session_id(uint8_t session_id, uint8_t *mac_id)
{
	cds_context_type *cds_ctx;
	uint32_t i;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		if ((conc_connection_list[i].vdev_id == session_id) &&
		    (conc_connection_list[i].in_use)) {
			*mac_id = conc_connection_list[i].mac;
			qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
			return QDF_STATUS_SUCCESS;
		}
	}
	qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
	return QDF_STATUS_E_FAILURE;
}

/**
 * cds_get_mcc_session_id_on_mac() - Get MCC session's ID
 * @mac_id: MAC ID on which MCC session needs to be found
 * @session_id: Session with which MCC combination needs to be found
 * @mcc_session_id: Pointer to the MCC session ID
 *
 * Get the session ID of the MCC interface
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cds_get_mcc_session_id_on_mac(uint8_t mac_id, uint8_t session_id,
					uint8_t *mcc_session_id)
{
	cds_context_type *cds_ctx;
	uint32_t i;
	uint8_t chan = conc_connection_list[session_id].chan;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		if (conc_connection_list[i].mac != mac_id)
			continue;
		if (conc_connection_list[i].vdev_id == session_id)
			continue;
		/* Inter band or intra band MCC */
		if ((conc_connection_list[i].chan != chan) &&
		    (conc_connection_list[i].in_use)) {
			*mcc_session_id = conc_connection_list[i].vdev_id;
			qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
			return QDF_STATUS_SUCCESS;
		}
	}
	qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
	return QDF_STATUS_E_FAILURE;
}

/**
 * cds_get_mcc_operating_channel() - Get the MCC channel
 * @session_id: Session ID with which MCC is being done
 *
 * Gets the MCC channel for a given session ID.
 *
 * Return: '0' (INVALID_CHANNEL_ID) or valid channel number
 */
uint8_t cds_get_mcc_operating_channel(uint8_t session_id)
{
	uint8_t mac_id, mcc_session_id;
	QDF_STATUS status;
	uint8_t chan;
	cds_context_type *cds_ctx;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return INVALID_CHANNEL_ID;
	}

	status = cds_get_mac_id_by_session_id(session_id, &mac_id);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("failed to get MAC ID");
		return INVALID_CHANNEL_ID;
	}

	status = cds_get_mcc_session_id_on_mac(mac_id, session_id,
			&mcc_session_id);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("failed to get MCC session ID");
		return INVALID_CHANNEL_ID;
	}

	qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
	chan = conc_connection_list[mcc_session_id].chan;
	qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);

	return chan;
}

/**
 * cds_checkn_update_hw_mode_single_mac_mode() - Set hw_mode to SMM
 * if required
 * @channel: channel number for the new STA connection
 *
 * After the STA disconnection, if the hw_mode is in DBS and the new STA
 * connection is coming in the band in which existing connections are
 * present, then this function stops the dbs opportunistic timer and sets
 * the hw_mode to Single MAC mode (SMM).
 *
 * Return: None
 */
void cds_checkn_update_hw_mode_single_mac_mode(uint8_t channel)
{
	uint8_t i;
	cds_context_type *cds_ctx;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return;
	}

	qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		if (conc_connection_list[i].in_use)
			if (!CDS_IS_SAME_BAND_CHANNELS(channel,
				conc_connection_list[i].chan)) {
				qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);
				cds_info("DBS required");
				return;
			}
	}
	qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);

	if (QDF_TIMER_STATE_RUNNING ==
		cds_ctx->dbs_opportunistic_timer.state)
		qdf_mc_timer_stop(&cds_ctx->dbs_opportunistic_timer);

	cds_dbs_opportunistic_timer_handler((void *)cds_ctx);
}

/**
 * cds_set_do_hw_mode_change_flag() - Set flag to indicate hw mode change
 * @flag: Indicate if hw mode change is required or not
 *
 * Set the flag to indicate whether a hw mode change is required after a
 * vdev up or not. Flag value of true indicates that a hw mode change is
 * required after vdev up.
 *
 * Return: None
 */
void cds_set_do_hw_mode_change_flag(bool flag)
{
	cds_context_type *cds_ctx;
	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);

	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return;
	}

	qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
	cds_ctx->do_hw_mode_change = flag;
	qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);

	cds_debug("hw_mode_change_channel:%d", flag);
}

/**
 * cds_is_hw_mode_change_after_vdev_up() - Check if hw mode change is needed
 *
 * Returns the flag which indicates if a hw mode change is required after
 * vdev up.
 *
 * Return: True if hw mode change is required, false otherwise
 */
bool cds_is_hw_mode_change_after_vdev_up(void)
{
	cds_context_type *cds_ctx;
	bool flag;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);

	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return INVALID_CHANNEL_ID;
	}

	qdf_mutex_acquire(&cds_ctx->qdf_conc_list_lock);
	flag = cds_ctx->do_hw_mode_change;
	qdf_mutex_release(&cds_ctx->qdf_conc_list_lock);

	return flag;
}
