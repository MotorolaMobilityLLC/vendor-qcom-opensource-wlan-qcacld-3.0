/*
 * Copyright (c) 2013-2017 The Linux Foundation. All rights reserved.
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

#ifndef WLAN_HDD_FTM_H
#define WLAN_HDD_FTM_H

/**
 * DOC: wlan_hdd_ftm.h
 *
 * WLAN Host Device Driver Factory Test Mode header file
 */

#include "qdf_status.h"
#include "cds_mq.h"
#include "cds_api.h"
#include "qdf_types.h"
#include <wlan_ptt_sock_svc.h>

//A TLV stream contains a 28-byte stream header, and its payload. It represents
//a command from host or a response from target.
#define WLAN_FTM_OPCODE_TX_ON 28
#define WLAN_FTM_OPCODE_PHY   40
#define WLAN_FTM_OPCODE_DATA  52

int hdd_update_cds_config_ftm(hdd_context_t *hdd_ctx);
void hdd_ftm_mc_process_msg(void *message);
void getHexDump(char *s0, char *s1, int len);
#if  defined(QCA_WIFI_FTM)
QDF_STATUS vos_is_tcmd_data_white_listed(u_int8_t *data, int len);
QDF_STATUS wlan_hdd_ftm_testmode_cmd(void *data, int len);
int wlan_hdd_qcmbr_unified_ioctl(hdd_adapter_t *adapter, struct ifreq *ifr);
#endif

#endif
