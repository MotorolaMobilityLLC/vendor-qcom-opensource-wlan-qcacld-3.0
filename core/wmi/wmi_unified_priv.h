/*
 * Copyright (c) 2013-2016 The Linux Foundation. All rights reserved.
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

/*
 * This file contains the API definitions for the Unified Wireless Module Interface (WMI).
 */
#ifndef _WMI_UNIFIED_PRIV_H_
#define _WMI_UNIFIED_PRIV_H_
#include <osdep.h>
#include "a_types.h"
#include "wmi.h"
#include "wmi_unified.h"
#include "qdf_atomic.h"

#define WMI_UNIFIED_MAX_EVENT 0x100
#define WMI_MAX_CMDS  1024

typedef qdf_nbuf_t wmi_buf_t;

#ifdef WMI_INTERFACE_EVENT_LOGGING

#define WMI_EVENT_DEBUG_MAX_ENTRY (1024)

struct wmi_command_debug {
	uint32_t command;
	uint32_t data[4];       /*16 bytes of WMI cmd excluding TLV and WMI headers */
	uint64_t time;
};

struct wmi_event_debug {
	uint32_t event;
	uint32_t data[4];       /*16 bytes of WMI event data excluding TLV header */
	uint64_t time;
};

#endif /*WMI_INTERFACE_EVENT_LOGGING */

#ifdef WLAN_OPEN_SOURCE
struct fwdebug {
	struct sk_buff_head fwlog_queue;
	struct completion fwlog_completion;
	A_BOOL fwlog_open;
};
#endif /* WLAN_OPEN_SOURCE */

struct wmi_unified {
	ol_scn_t scn_handle;    /* handle to device */
	qdf_atomic_t pending_cmds;
	HTC_ENDPOINT_ID wmi_endpoint_id;
	uint16_t max_msg_len;
	WMI_EVT_ID event_id[WMI_UNIFIED_MAX_EVENT];
	wmi_unified_event_handler event_handler[WMI_UNIFIED_MAX_EVENT];
	uint32_t max_event_idx;
	void *htc_handle;
	qdf_spinlock_t eventq_lock;
	qdf_nbuf_queue_t event_queue;
	struct work_struct rx_event_work;
#ifdef WLAN_OPEN_SOURCE
	struct fwdebug dbglog;
	struct dentry *debugfs_phy;
#endif /* WLAN_OPEN_SOURCE */

#ifdef WMI_INTERFACE_EVENT_LOGGING
	qdf_spinlock_t wmi_record_lock;
#endif /*WMI_INTERFACE_EVENT_LOGGING */

	qdf_atomic_t is_target_suspended;

#ifdef FEATURE_RUNTIME_PM
	qdf_atomic_t runtime_pm_inprogress;
#endif

	int (*wma_process_fw_event_handler_cbk)(struct wmi_unified *wmi_handle,
						wmi_buf_t evt_buf);
};
#endif
