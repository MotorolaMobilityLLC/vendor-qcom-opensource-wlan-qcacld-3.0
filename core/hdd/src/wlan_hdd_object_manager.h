/*
 * Copyright (c) 2017 The Linux Foundation. All rights reserved.
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
#if !defined(WLAN_HDD_OBJECT_MANAGER_H)
#define WLAN_HDD_OBJECT_MANAGER_H
/**
 * DOC: HDD object manager API file to create/destroy psoc, pdev, vdev
 * and peer objects by calling object manager APIs
 *
 * Common object model has 1 : N mapping between PSOC and PDEV but for MCL
 * PSOC and PDEV has 1 : 1 mapping.
 *
 * MCL object model view is:
 *
 *                          --------
 *                          | PSOC |
 *                          --------
 *                             |
 *                             |
 *                 --------------------------
 *                 |          PDEV          |
 *                 --------------------------
 *                 |                        |
 *                 |                        |
 *                 |                        |
 *             ----------             -------------
 *             | vdev 0 |             |   vdev n  |
 *             ----------             -------------
 *             |        |             |           |
 *        ----------   ----------    ----------  ----------
 *        | peer 1 |   | peer n |    | peer 1 |  | peer n |
 *        ----------   ----------    ----------  -----------
 *
 */
#include "wlan_hdd_main.h"
#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>

/**
 * hdd_create_and_store_psoc() - Create psoc object and store in hdd context
 * @hdd_ctx: Hdd context
 * @psoc_id: Psoc Id
 *
 * This API creates Psoc object with given @psoc_id and store the psoc reference
 * to hdd context
 *
 * Return: 0 for success, negative error code for failure
 */
int hdd_create_and_store_psoc(hdd_context_t *hdd_ctx, uint8_t psoc_id);

/**
 * hdd_release_and_destroy_psoc() - Deletes the psoc object
 * @hdd_ctx: Hdd context
 *
 * This API deletes psoc object and release its reference from hdd context
 *
 * Return: 0 for success, negative error code for failure
 */
int hdd_release_and_destroy_psoc(hdd_context_t *hdd_ctx);

/**
 * hdd_create_and_store_pdev() - Create pdev object and store in hdd context
 * @hdd_ctx: Hdd context
 *
 * This API creates the pdev object and store the pdev reference to hdd context
 *
 * Return: 0 for success, negative error code for failure
 */
int hdd_create_and_store_pdev(hdd_context_t *hdd_ctx);

/**
 * hdd_release_and_destroy_pdev() - Deletes the pdev object
 * @hdd_ctx: Hdd context
 *
 * This API deletes pdev object and release its reference from hdd context
 *
 * Return: 0 for success, negative error code for failure
 */
int hdd_release_and_destroy_pdev(hdd_context_t *hdd_ctx);

/**
 * hdd_create_and_store_vdev() - Create vdev object and store in hdd adapter
 * @pdev: pdev pointer
 * @adapter: hdd adapter
 *
 * This API creates the vdev object and store the vdev reference to the
 * given @adapter. Also, creates a self peer for the vdev. If the adapter
 * session id and vdev id of the new vdev object doesnot match, destroys the
 * created vdev object and returns failure
 *
 * Return: 0 for success, negative error code for failure
 */
int hdd_create_and_store_vdev(struct wlan_objmgr_pdev *pdev,
			      hdd_adapter_t *adapter);

/**
 * hdd_release_and_destroy_vdev() - Delete the vdev object
 * @hdd_ctx: Hdd context
 *
 * This API deletes vdev object and release its reference from hdd adapter
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_release_and_destroy_vdev(hdd_adapter_t *adapter);

/**
 * hdd_add_peer_object() - Create and add the peer object to the vdev
 * @vdev: vdev pointer
 * @adapter_mode: adapter mode
 * @mac_addr: Peer mac address
 *
 * This API creates and adds the peer object to the given @vdev. The peer type
 * (STA, AP or IBSS) is assigned based on adapter mode. For example, if adapter
 * mode is STA, peer is AP.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_add_peer_object(struct wlan_objmgr_vdev *vdev,
				enum tQDF_ADAPTER_MODE adapter_mode,
				uint8_t *mac_addr);

/**
 * hdd_remove_peer_object() - Delete and remove the peer from vdev
 * @vdev: vdev pointer
 * @mac_addr: Peer Mac address
 *
 * This API finds the peer object from given @mac_addr and deletes the same.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_remove_peer_object(struct wlan_objmgr_vdev *vdev,
						uint8_t *mac_addr);
#endif /* end #if !defined(WLAN_HDD_OBJECT_MANAGER_H) */
