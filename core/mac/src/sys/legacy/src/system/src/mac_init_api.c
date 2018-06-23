/*
 * Copyright (c) 2011-2018 The Linux Foundation. All rights reserved.
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
 *
 * mac_init_api.c - This file has all the mac level init functions
 *                   for all the defined threads at system level.
 * Author:    Dinesh Upadhyay
 * Date:      04/23/2007
 * History:-
 * Date: 04/08/2008       Modified by: Santosh Mandiganal
 * Modification Information: Code to allocate and free the  memory for DumpTable entry.
 * --------------------------------------------------------------------------
 *
 */
/* Standard include files */
#include "cfg_api.h"             /* cfg_cleanup */
#include "lim_api.h"             /* lim_cleanup */
#include "sir_types.h"
#include "sys_entry_func.h"
#include "mac_init_api.h"

#ifdef TRACE_RECORD
#include "mac_trace.h"
#endif

static tAniSirGlobal global_mac_context;

QDF_STATUS mac_start(tHalHandle hHal, void *pHalMacStartParams)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tpAniSirGlobal pMac = (tpAniSirGlobal) hHal;

	if (NULL == pMac) {
		QDF_ASSERT(0);
		status = QDF_STATUS_E_FAILURE;
		return status;
	}

	pMac->gDriverType =
		((tHalMacStartParameters *) pHalMacStartParams)->driverType;

	if (ANI_DRIVER_TYPE(pMac) != QDF_DRIVER_TYPE_MFG) {
		status = pe_start(pMac);
	}

	return status;
}

/** -------------------------------------------------------------
   \fn mac_stop
   \brief this function will be called from HDD to stop MAC. This function will stop all the mac modules.
 \       memory with global context will only be initialized not freed here.
   \param   tHalHandle hHal
   \param tHalStopType
   \return QDF_STATUS
   -------------------------------------------------------------*/

QDF_STATUS mac_stop(tHalHandle hHal, tHalStopType stopType)
{
	tpAniSirGlobal pMac = (tpAniSirGlobal) hHal;

	pe_stop(pMac);
	cfg_cleanup(pMac);

	return QDF_STATUS_SUCCESS;
}

/** -------------------------------------------------------------
   \fn mac_open
   \brief this function will be called during init. This function is suppose to allocate all the
 \       memory with the global context will be allocated here.
   \param   tHalHandle pHalHandle
   \param   hdd_handle_t hHdd
   \param   tHalOpenParameters* pHalOpenParams
   \return QDF_STATUS
   -------------------------------------------------------------*/

QDF_STATUS mac_open(struct wlan_objmgr_psoc *psoc, tHalHandle *pHalHandle,
		    hdd_handle_t hHdd, struct cds_config_info *cds_cfg)
{
	tpAniSirGlobal p_mac = &global_mac_context;
	QDF_STATUS status;

	if (pHalHandle == NULL)
		return QDF_STATUS_E_FAILURE;

	/*
	 * Set various global fields of p_mac here
	 * (Could be platform dependent as some variables in p_mac are platform
	 * dependent)
	 */
	p_mac->hHdd = hHdd;

	status = wlan_objmgr_psoc_try_get_ref(psoc, WLAN_LEGACY_MAC_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		pe_err("PSOC get ref failure");
		return QDF_STATUS_E_FAILURE;
	}

	p_mac->psoc = psoc;
	*pHalHandle = (tHalHandle) p_mac;

	{
		/*
		 * For Non-FTM cases this value will be reset during mac_start
		 */
		if (cds_cfg->driver_type)
			p_mac->gDriverType = QDF_DRIVER_TYPE_MFG;

		/* Call routine to initialize CFG data structures */
		if (QDF_STATUS_SUCCESS != cfg_init(p_mac))
			return QDF_STATUS_E_FAILURE;

		sys_init_globals(p_mac);
	}

	/* FW: 0 to 2047 and Host: 2048 to 4095 */
	p_mac->mgmtSeqNum = WLAN_HOST_SEQ_NUM_MIN - 1;
	p_mac->first_scan_done = false;
	p_mac->he_sgi_ltf_cfg_bit_mask = DEF_HE_AUTO_SGI_LTF;
	p_mac->is_usr_cfg_amsdu_enabled = true;

	status =  pe_open(p_mac, cds_cfg);
	if (QDF_STATUS_SUCCESS != status) {
		pe_err("pe_open() failure");
		cfg_de_init(p_mac);
	}

	return status;
}

/** -------------------------------------------------------------
   \fn mac_close
   \brief this function will be called in shutdown sequence from HDD. All the
 \       allocated memory with global context will be freed here.
   \param   tpAniSirGlobal pMac
   \return none
   -------------------------------------------------------------*/

QDF_STATUS mac_close(tHalHandle hHal)
{

	tpAniSirGlobal pMac = (tpAniSirGlobal) hHal;

	if (!pMac)
		return QDF_STATUS_E_FAILURE;

	pe_close(pMac);

	/* Call routine to free-up all CFG data structures */
	cfg_de_init(pMac);

	if (pMac->pdev) {
		wlan_objmgr_pdev_release_ref(pMac->pdev, WLAN_LEGACY_MAC_ID);
		pMac->pdev = NULL;
	}
	wlan_objmgr_psoc_release_ref(pMac->psoc, WLAN_LEGACY_MAC_ID);
	pMac->psoc = NULL;

	return QDF_STATUS_SUCCESS;
}
