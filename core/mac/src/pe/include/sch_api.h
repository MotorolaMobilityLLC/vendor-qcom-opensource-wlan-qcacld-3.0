/*
 * Copyright (c) 2011-2015, 2017-2018 The Linux Foundation. All rights reserved.
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
 * Author:      Sandesh Goel
 * Date:        02/25/02
 * History:-
 * Date            Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#ifndef __SCH_API_H__
#define __SCH_API_H__

#include "sir_common.h"
#include "sir_mac_prot_def.h"

#include "ani_global.h"

/* update only the broadcast qos params */
extern void sch_qos_update_broadcast(tpAniSirGlobal pMac,
				     tpPESession psessionEntry);

/* fill in the default local edca parameter into gLimEdcaParams[] */
extern void sch_set_default_edca_params(tpAniSirGlobal pMac, tpPESession psessionE);

/* update only local qos params */
extern void sch_qos_update_local(tpAniSirGlobal pMac, tpPESession psessionEntry);

/* update the edca profile parameters */
extern void sch_edca_profile_update(tpAniSirGlobal pMac,
				    tpPESession psessionEntry);

/* / Set the fixed fields in a beacon frame */
extern tSirRetStatus sch_set_fixed_beacon_fields(tpAniSirGlobal pMac,
						 tpPESession psessionEntry);

/* / Process the scheduler messages */
extern void sch_process_message(tpAniSirGlobal pMac,
				struct scheduler_msg *pSchMsg);

/* / The beacon Indication handler function */
extern void sch_process_pre_beacon_ind(tpAniSirGlobal pMac,
				       struct scheduler_msg *limMsg);

/* / Post a message to the scheduler message queue */
extern tSirRetStatus sch_post_message(tpAniSirGlobal pMac,
				      struct scheduler_msg *pMsg);

extern void sch_beacon_process(tpAniSirGlobal pMac, uint8_t *pRxPacketInfo,
			       tpPESession psessionEntry);
extern tSirRetStatus sch_beacon_edca_process(tpAniSirGlobal pMac,
					     tSirMacEdcaParamSetIE *edca,
					     tpPESession psessionEntry);

void sch_generate_tim(tpAniSirGlobal, uint8_t **, uint16_t *, uint8_t);

void sch_set_beacon_interval(tpAniSirGlobal pMac, tpPESession psessionEntry);

tSirRetStatus sch_send_beacon_req(tpAniSirGlobal, uint8_t *, uint16_t,
				  tpPESession psessionEntry);

tSirRetStatus lim_update_probe_rsp_template_ie_bitmap_beacon1(tpAniSirGlobal,
							      tDot11fBeacon1 *,
							      tpPESession
							      psessionEntry);
void lim_update_probe_rsp_template_ie_bitmap_beacon2(tpAniSirGlobal, tDot11fBeacon2 *,
						     uint32_t *,
						     tDot11fProbeResponse *);
void set_probe_rsp_ie_bitmap(uint32_t *, uint32_t);
uint32_t lim_send_probe_rsp_template_to_hal(tpAniSirGlobal, tpPESession, uint32_t *);

int sch_gen_timing_advert_frame(tpAniSirGlobal pMac, tSirMacAddr self_addr,
	uint8_t **buf, uint32_t *timestamp_offset, uint32_t *time_value_offset);

/*
 * sch_beacon_process_for_ap() - process the beacon frame for AP sessions
 * @mac_ctx: pointer to the global mac_ctx
 * @rx_pkt_info: pointer to the frame Rx Meta
 * @bcn: pointer to the beacon struct
 *
 * Process the beacon in the context of any existing AP or BTAP
 * session. This takes cares of following two scenarios:
 *  - session = NULL:
 * e.g. beacon received from a neighboring BSS, you want to apply the
 * protection settings to BTAP/InfraAP beacons
 *  - session is non NULL:
 * e.g. beacon received is from the INFRA AP to which you are connected
 * on another concurrent link. In this case also, we want to apply the
 * protection settings(as advertised by Infra AP) to BTAP beacons
 *
 * Return: None
 */
void sch_beacon_process_for_ap(tpAniSirGlobal mac_ctx,
					uint8_t *rx_pkt_info,
					tSchBeaconStruct *bcn);

#endif
