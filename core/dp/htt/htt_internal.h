/*
 * Copyright (c) 2011, 2014-2016 The Linux Foundation. All rights reserved.
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

#ifndef _HTT_INTERNAL__H_
#define _HTT_INTERNAL__H_

#include <athdefs.h>            /* A_STATUS */
#include <qdf_nbuf.h>           /* qdf_nbuf_t */
#include <qdf_util.h>           /* qdf_assert */
#include <htc_api.h>            /* HTC_PACKET */

#include <htt_types.h>

#ifndef offsetof
#define offsetof(type, field)   ((size_t)(&((type *)0)->field))
#endif

#undef MS
#define MS(_v, _f) (((_v) & _f ## _MASK) >> _f ## _LSB)
#undef SM
#define SM(_v, _f) (((_v) << _f ## _LSB) & _f ## _MASK)
#undef WO
#define WO(_f)      ((_f ## _OFFSET) >> 2)

#define GET_FIELD(_addr, _f) MS(*((A_UINT32 *)(_addr) + WO(_f)), _f)

#include <rx_desc.h>
#include <wal_rx_desc.h>        /* struct rx_attention, etc */

struct htt_host_fw_desc_base {
	union {
		struct fw_rx_desc_base val;
		A_UINT32 dummy_pad;     /* make sure it is DOWRD aligned */
	} u;
};

/*
 * This struct defines the basic descriptor information used by host,
 * which is written either by the 11ac HW MAC into the host Rx data
 * buffer ring directly or generated by FW and copied from Rx indication
 */
#define RX_HTT_HDR_STATUS_LEN 64
struct htt_host_rx_desc_base {
	struct htt_host_fw_desc_base fw_desc;
	struct rx_attention attention;
	struct rx_frag_info frag_info;
	struct rx_mpdu_start mpdu_start;
	struct rx_msdu_start msdu_start;
	struct rx_msdu_end msdu_end;
	struct rx_mpdu_end mpdu_end;
	struct rx_ppdu_start ppdu_start;
	struct rx_ppdu_end ppdu_end;
	char rx_hdr_status[RX_HTT_HDR_STATUS_LEN];
};

#define RX_STD_DESC_ATTN_OFFSET	\
	(offsetof(struct htt_host_rx_desc_base, attention))
#define RX_STD_DESC_FRAG_INFO_OFFSET \
	(offsetof(struct htt_host_rx_desc_base, frag_info))
#define RX_STD_DESC_MPDU_START_OFFSET \
	(offsetof(struct htt_host_rx_desc_base, mpdu_start))
#define RX_STD_DESC_MSDU_START_OFFSET \
	(offsetof(struct htt_host_rx_desc_base, msdu_start))
#define RX_STD_DESC_MSDU_END_OFFSET \
	(offsetof(struct htt_host_rx_desc_base, msdu_end))
#define RX_STD_DESC_MPDU_END_OFFSET \
	(offsetof(struct htt_host_rx_desc_base, mpdu_end))
#define RX_STD_DESC_PPDU_START_OFFSET \
	(offsetof(struct htt_host_rx_desc_base, ppdu_start))
#define RX_STD_DESC_PPDU_END_OFFSET \
	(offsetof(struct htt_host_rx_desc_base, ppdu_end))
#define RX_STD_DESC_HDR_STATUS_OFFSET \
	(offsetof(struct htt_host_rx_desc_base, rx_hdr_status))

#define RX_STD_DESC_FW_MSDU_OFFSET \
	(offsetof(struct htt_host_rx_desc_base, fw_desc))

#define RX_STD_DESC_SIZE (sizeof(struct htt_host_rx_desc_base))

#define RX_DESC_ATTN_OFFSET32       (RX_STD_DESC_ATTN_OFFSET >> 2)
#define RX_DESC_FRAG_INFO_OFFSET32  (RX_STD_DESC_FRAG_INFO_OFFSET >> 2)
#define RX_DESC_MPDU_START_OFFSET32 (RX_STD_DESC_MPDU_START_OFFSET >> 2)
#define RX_DESC_MSDU_START_OFFSET32 (RX_STD_DESC_MSDU_START_OFFSET >> 2)
#define RX_DESC_MSDU_END_OFFSET32   (RX_STD_DESC_MSDU_END_OFFSET >> 2)
#define RX_DESC_MPDU_END_OFFSET32   (RX_STD_DESC_MPDU_END_OFFSET >> 2)
#define RX_DESC_PPDU_START_OFFSET32 (RX_STD_DESC_PPDU_START_OFFSET >> 2)
#define RX_DESC_PPDU_END_OFFSET32   (RX_STD_DESC_PPDU_END_OFFSET >> 2)
#define RX_DESC_HDR_STATUS_OFFSET32 (RX_STD_DESC_HDR_STATUS_OFFSET >> 2)

#define RX_STD_DESC_SIZE_DWORD      (RX_STD_DESC_SIZE >> 2)

/*
 * Make sure there is a minimum headroom provided in the rx netbufs
 * for use by the OS shim and OS and rx data consumers.
 */
#define HTT_RX_BUF_OS_MIN_HEADROOM 32
#define HTT_RX_STD_DESC_RESERVATION  \
	((HTT_RX_BUF_OS_MIN_HEADROOM > RX_STD_DESC_SIZE) ? \
	 HTT_RX_BUF_OS_MIN_HEADROOM : RX_STD_DESC_SIZE)
#define HTT_RX_DESC_RESERVATION32 \
	(HTT_RX_STD_DESC_RESERVATION >> 2)

#define HTT_RX_DESC_ALIGN_MASK 7        /* 8-byte alignment */
#ifdef DEBUG_RX_RING_BUFFER
#define HTT_RX_RING_BUFF_DBG_LIST          1024
struct rx_buf_debug {
	uint32_t paddr;
	void     *vaddr;
	bool     in_use;
};
#endif
static inline struct htt_host_rx_desc_base *htt_rx_desc(qdf_nbuf_t msdu)
{
	return (struct htt_host_rx_desc_base *)
	       (((size_t) (qdf_nbuf_head(msdu) + HTT_RX_DESC_ALIGN_MASK)) &
		~HTT_RX_DESC_ALIGN_MASK);
}

#if defined(FEATURE_LRO)
/**
 * htt_print_rx_desc_lro() - print LRO information in the rx
 * descriptor
 * @rx_desc: HTT rx descriptor
 *
 * Prints the LRO related fields in the HTT rx descriptor
 *
 * Return: none
 */
static inline void htt_print_rx_desc_lro(struct htt_host_rx_desc_base *rx_desc)
{
	qdf_print
		("----------------------RX DESC LRO----------------------\n");
	qdf_print("msdu_end.lro_eligible:0x%x\n",
		 rx_desc->msdu_end.lro_eligible);
	qdf_print("msdu_start.tcp_only_ack:0x%x\n",
		 rx_desc->msdu_start.tcp_only_ack);
	qdf_print("msdu_end.tcp_udp_chksum:0x%x\n",
		 rx_desc->msdu_end.tcp_udp_chksum);
	qdf_print("msdu_end.tcp_seq_number:0x%x\n",
		 rx_desc->msdu_end.tcp_seq_number);
	qdf_print("msdu_end.tcp_ack_number:0x%x\n",
		 rx_desc->msdu_end.tcp_ack_number);
	qdf_print("msdu_start.tcp_proto:0x%x\n",
		 rx_desc->msdu_start.tcp_proto);
	qdf_print("msdu_start.ipv6_proto:0x%x\n",
		 rx_desc->msdu_start.ipv6_proto);
	qdf_print("msdu_start.ipv4_proto:0x%x\n",
		 rx_desc->msdu_start.ipv4_proto);
	qdf_print("msdu_start.l3_offset:0x%x\n",
		 rx_desc->msdu_start.l3_offset);
	qdf_print("msdu_start.l4_offset:0x%x\n",
		 rx_desc->msdu_start.l4_offset);
	qdf_print("msdu_start.flow_id_toeplitz:0x%x\n",
			   rx_desc->msdu_start.flow_id_toeplitz);
	qdf_print
		("---------------------------------------------------------\n");
}

/**
 * htt_print_rx_desc_lro() - extract LRO information from the rx
 * descriptor
 * @msdu: network buffer
 * @rx_desc: HTT rx descriptor
 *
 * Extracts the LRO related fields from the HTT rx descriptor
 * and stores them in the network buffer's control block
 *
 * Return: none
 */
static inline void htt_rx_extract_lro_info(qdf_nbuf_t msdu,
	 struct htt_host_rx_desc_base *rx_desc)
{
	QDF_NBUF_CB_RX_LRO_ELIGIBLE(msdu) = rx_desc->msdu_end.lro_eligible;
	if (rx_desc->msdu_end.lro_eligible) {
		QDF_NBUF_CB_RX_TCP_PURE_ACK(msdu) = rx_desc->msdu_start.tcp_only_ack;
		QDF_NBUF_CB_RX_TCP_CHKSUM(msdu) = rx_desc->msdu_end.tcp_udp_chksum;
		QDF_NBUF_CB_RX_TCP_SEQ_NUM(msdu) = rx_desc->msdu_end.tcp_seq_number;
		QDF_NBUF_CB_RX_TCP_ACK_NUM(msdu) = rx_desc->msdu_end.tcp_ack_number;
		QDF_NBUF_CB_RX_TCP_WIN(msdu) = rx_desc->msdu_end.window_size;
		QDF_NBUF_CB_RX_TCP_PROTO(msdu) = rx_desc->msdu_start.tcp_proto;
		QDF_NBUF_CB_RX_IPV6_PROTO(msdu) = rx_desc->msdu_start.ipv6_proto;
		QDF_NBUF_CB_RX_IP_OFFSET(msdu) = rx_desc->msdu_start.l3_offset;
		QDF_NBUF_CB_RX_TCP_OFFSET(msdu) = rx_desc->msdu_start.l4_offset;
		QDF_NBUF_CB_RX_FLOW_ID_TOEPLITZ(msdu) =
			 rx_desc->msdu_start.flow_id_toeplitz;
	}
}
#else
static inline void htt_print_rx_desc_lro(struct htt_host_rx_desc_base *rx_desc)
{}
static inline void htt_rx_extract_lro_info(qdf_nbuf_t msdu,
	 struct htt_host_rx_desc_base *rx_desc) {}
#endif /* FEATURE_LRO */

static inline void htt_print_rx_desc(struct htt_host_rx_desc_base *rx_desc)
{
	qdf_print
		("----------------------RX DESC----------------------------\n");
	qdf_print("attention: %#010x\n",
		  (unsigned int)(*(uint32_t *) &rx_desc->attention));
	qdf_print("frag_info: %#010x\n",
		  (unsigned int)(*(uint32_t *) &rx_desc->frag_info));
	qdf_print("mpdu_start: %#010x %#010x %#010x\n",
		  (unsigned int)(((uint32_t *) &rx_desc->mpdu_start)[0]),
		  (unsigned int)(((uint32_t *) &rx_desc->mpdu_start)[1]),
		  (unsigned int)(((uint32_t *) &rx_desc->mpdu_start)[2]));
	qdf_print("msdu_start: %#010x %#010x %#010x\n",
		  (unsigned int)(((uint32_t *) &rx_desc->msdu_start)[0]),
		  (unsigned int)(((uint32_t *) &rx_desc->msdu_start)[1]),
		  (unsigned int)(((uint32_t *) &rx_desc->msdu_start)[2]));
	qdf_print("msdu_end: %#010x %#010x %#010x %#010x %#010x\n",
		  (unsigned int)(((uint32_t *) &rx_desc->msdu_end)[0]),
		  (unsigned int)(((uint32_t *) &rx_desc->msdu_end)[1]),
		  (unsigned int)(((uint32_t *) &rx_desc->msdu_end)[2]),
		  (unsigned int)(((uint32_t *) &rx_desc->msdu_end)[3]),
		  (unsigned int)(((uint32_t *) &rx_desc->msdu_end)[4]));
	qdf_print("mpdu_end: %#010x\n",
		  (unsigned int)(*(uint32_t *) &rx_desc->mpdu_end));
	qdf_print("ppdu_start: " "%#010x %#010x %#010x %#010x %#010x\n"
		  "%#010x %#010x %#010x %#010x %#010x\n",
		  (unsigned int)(((uint32_t *) &rx_desc->ppdu_start)[0]),
		  (unsigned int)(((uint32_t *) &rx_desc->ppdu_start)[1]),
		  (unsigned int)(((uint32_t *) &rx_desc->ppdu_start)[2]),
		  (unsigned int)(((uint32_t *) &rx_desc->ppdu_start)[3]),
		  (unsigned int)(((uint32_t *) &rx_desc->ppdu_start)[4]),
		  (unsigned int)(((uint32_t *) &rx_desc->ppdu_start)[5]),
		  (unsigned int)(((uint32_t *) &rx_desc->ppdu_start)[6]),
		  (unsigned int)(((uint32_t *) &rx_desc->ppdu_start)[7]),
		  (unsigned int)(((uint32_t *) &rx_desc->ppdu_start)[8]),
		  (unsigned int)(((uint32_t *) &rx_desc->ppdu_start)[9]));
	qdf_print("ppdu_end:" "%#010x %#010x %#010x %#010x %#010x\n"
		  "%#010x %#010x %#010x %#010x %#010x\n"
		  "%#010x,%#010x %#010x %#010x %#010x\n"
		  "%#010x %#010x %#010x %#010x %#010x\n" "%#010x %#010x\n",
		  (unsigned int)(((uint32_t *) &rx_desc->ppdu_end)[0]),
		  (unsigned int)(((uint32_t *) &rx_desc->ppdu_end)[1]),
		  (unsigned int)(((uint32_t *) &rx_desc->ppdu_end)[2]),
		  (unsigned int)(((uint32_t *) &rx_desc->ppdu_end)[3]),
		  (unsigned int)(((uint32_t *) &rx_desc->ppdu_end)[4]),
		  (unsigned int)(((uint32_t *) &rx_desc->ppdu_end)[5]),
		  (unsigned int)(((uint32_t *) &rx_desc->ppdu_end)[6]),
		  (unsigned int)(((uint32_t *) &rx_desc->ppdu_end)[7]),
		  (unsigned int)(((uint32_t *) &rx_desc->ppdu_end)[8]),
		  (unsigned int)(((uint32_t *) &rx_desc->ppdu_end)[9]),
		  (unsigned int)(((uint32_t *) &rx_desc->ppdu_end)[10]),
		  (unsigned int)(((uint32_t *) &rx_desc->ppdu_end)[11]),
		  (unsigned int)(((uint32_t *) &rx_desc->ppdu_end)[12]),
		  (unsigned int)(((uint32_t *) &rx_desc->ppdu_end)[13]),
		  (unsigned int)(((uint32_t *) &rx_desc->ppdu_end)[14]),
		  (unsigned int)(((uint32_t *) &rx_desc->ppdu_end)[15]),
		  (unsigned int)(((uint32_t *) &rx_desc->ppdu_end)[16]),
		  (unsigned int)(((uint32_t *) &rx_desc->ppdu_end)[17]),
		  (unsigned int)(((uint32_t *) &rx_desc->ppdu_end)[18]),
		  (unsigned int)(((uint32_t *) &rx_desc->ppdu_end)[19]),
		  (unsigned int)(((uint32_t *) &rx_desc->ppdu_end)[20]),
		  (unsigned int)(((uint32_t *) &rx_desc->ppdu_end)[21]));
	qdf_print
		("---------------------------------------------------------\n");
}

#ifndef HTT_ASSERT_LEVEL
#define HTT_ASSERT_LEVEL 3
#endif

#define HTT_ASSERT_ALWAYS(condition) qdf_assert_always((condition))

#define HTT_ASSERT0(condition) qdf_assert((condition))
#if HTT_ASSERT_LEVEL > 0
#define HTT_ASSERT1(condition) qdf_assert((condition))
#else
#define HTT_ASSERT1(condition)
#endif

#if HTT_ASSERT_LEVEL > 1
#define HTT_ASSERT2(condition) qdf_assert((condition))
#else
#define HTT_ASSERT2(condition)
#endif

#if HTT_ASSERT_LEVEL > 2
#define HTT_ASSERT3(condition) qdf_assert((condition))
#else
#define HTT_ASSERT3(condition)
#endif

#define HTT_MAC_ADDR_LEN 6

/*
 * HTT_MAX_SEND_QUEUE_DEPTH -
 * How many packets HTC should allow to accumulate in a send queue
 * before calling the EpSendFull callback to see whether to retain
 * or drop packets.
 * This is not relevant for LL, where tx descriptors should be immediately
 * downloaded to the target.
 * This is not very relevant for HL either, since it is anticipated that
 * the HL tx download scheduler will not work this far in advance - rather,
 * it will make its decisions just-in-time, so it can be responsive to
 * changing conditions.
 * Hence, this queue depth threshold spec is mostly just a formality.
 */
#define HTT_MAX_SEND_QUEUE_DEPTH 64

#define IS_PWR2(value) (((value) ^ ((value)-1)) == ((value) << 1) - 1)

/* FIX THIS
 * Should be: sizeof(struct htt_host_rx_desc) + max rx MSDU size,
 * rounded up to a cache line size.
 */
#define HTT_RX_BUF_SIZE 1920
/*
 * DMA_MAP expects the buffer to be an integral number of cache lines.
 * Rather than checking the actual cache line size, this code makes a
 * conservative estimate of what the cache line size could be.
 */
#define HTT_LOG2_MAX_CACHE_LINE_SIZE 7  /* 2^7 = 128 */
#define HTT_MAX_CACHE_LINE_SIZE_MASK ((1 << HTT_LOG2_MAX_CACHE_LINE_SIZE) - 1)

#ifdef BIG_ENDIAN_HOST
/*
 * big-endian: bytes within a 4-byte "word" are swapped:
 * pre-swap  post-swap
 *  index     index
 *    0         3
 *    1         2
 *    2         1
 *    3         0
 *    4         7
 *    5         6
 * etc.
 * To compute the post-swap index from the pre-swap index, compute
 * the byte offset for the start of the word (index & ~0x3) and add
 * the swapped byte offset within the word (3 - (index & 0x3)).
 */
#define HTT_ENDIAN_BYTE_IDX_SWAP(idx) (((idx) & ~0x3) + (3 - ((idx) & 0x3)))
#else
/* little-endian: no adjustment needed */
#define HTT_ENDIAN_BYTE_IDX_SWAP(idx) idx
#endif

#define HTT_TX_MUTEX_INIT(_mutex)			\
	qdf_spinlock_create(_mutex)

#define HTT_TX_MUTEX_ACQUIRE(_mutex)			\
	qdf_spin_lock_bh(_mutex)

#define HTT_TX_MUTEX_RELEASE(_mutex)			\
	qdf_spin_unlock_bh(_mutex)

#define HTT_TX_MUTEX_DESTROY(_mutex)			\
	qdf_spinlock_destroy(_mutex)

#define HTT_TX_DESC_PADDR(_pdev, _tx_desc_vaddr)       \
	((_pdev)->tx_descs.pool_paddr +  (uint32_t)	  \
	 ((char *)(_tx_desc_vaddr) -			   \
	  (char *)((_pdev)->tx_descs.pool_vaddr)))

#ifdef ATH_11AC_TXCOMPACT

#define HTT_TX_NBUF_QUEUE_MUTEX_INIT(_pdev)		\
	qdf_spinlock_create(&_pdev->txnbufq_mutex)

#define HTT_TX_NBUF_QUEUE_MUTEX_DESTROY(_pdev)	       \
	HTT_TX_MUTEX_DESTROY(&_pdev->txnbufq_mutex)

#define HTT_TX_NBUF_QUEUE_REMOVE(_pdev, _msdu)	do {	\
	HTT_TX_MUTEX_ACQUIRE(&_pdev->txnbufq_mutex);	\
	_msdu =  qdf_nbuf_queue_remove(&_pdev->txnbufq);\
	HTT_TX_MUTEX_RELEASE(&_pdev->txnbufq_mutex);    \
	} while (0)

#define HTT_TX_NBUF_QUEUE_ADD(_pdev, _msdu) do {	\
	HTT_TX_MUTEX_ACQUIRE(&_pdev->txnbufq_mutex);	\
	qdf_nbuf_queue_add(&_pdev->txnbufq, _msdu);     \
	HTT_TX_MUTEX_RELEASE(&_pdev->txnbufq_mutex);    \
	} while (0)

#define HTT_TX_NBUF_QUEUE_INSERT_HEAD(_pdev, _msdu) do {   \
	HTT_TX_MUTEX_ACQUIRE(&_pdev->txnbufq_mutex);	   \
	qdf_nbuf_queue_insert_head(&_pdev->txnbufq, _msdu);\
	HTT_TX_MUTEX_RELEASE(&_pdev->txnbufq_mutex);       \
	} while (0)
#else

#define HTT_TX_NBUF_QUEUE_MUTEX_INIT(_pdev)
#define HTT_TX_NBUF_QUEUE_REMOVE(_pdev, _msdu)
#define HTT_TX_NBUF_QUEUE_ADD(_pdev, _msdu)
#define HTT_TX_NBUF_QUEUE_INSERT_HEAD(_pdev, _msdu)
#define HTT_TX_NBUF_QUEUE_MUTEX_DESTROY(_pdev)

#endif

void htt_tx_resume_handler(void *);
#ifdef ATH_11AC_TXCOMPACT
#define HTT_TX_SCHED htt_tx_sched
#else
#define HTT_TX_SCHED(pdev)      /* no-op */
#endif

int htt_tx_attach(struct htt_pdev_t *pdev, int desc_pool_elems);

void htt_tx_detach(struct htt_pdev_t *pdev);

int htt_rx_attach(struct htt_pdev_t *pdev);

void htt_rx_detach(struct htt_pdev_t *pdev);

int htt_htc_attach(struct htt_pdev_t *pdev);

void htt_t2h_msg_handler(void *context, HTC_PACKET *pkt);

void htt_h2t_send_complete(void *context, HTC_PACKET *pkt);

A_STATUS htt_h2t_ver_req_msg(struct htt_pdev_t *pdev);

#if defined(HELIUMPLUS_PADDR64)
A_STATUS
htt_h2t_frag_desc_bank_cfg_msg(struct htt_pdev_t *pdev);
#endif /* defined(HELIUMPLUS_PADDR64) */

extern A_STATUS htt_h2t_rx_ring_cfg_msg_ll(struct htt_pdev_t *pdev);
extern A_STATUS (*htt_h2t_rx_ring_cfg_msg)(struct htt_pdev_t *pdev);

HTC_SEND_FULL_ACTION htt_h2t_full(void *context, HTC_PACKET *pkt);

struct htt_htc_pkt *htt_htc_pkt_alloc(struct htt_pdev_t *pdev);

void htt_htc_pkt_free(struct htt_pdev_t *pdev, struct htt_htc_pkt *pkt);

void htt_htc_pkt_pool_free(struct htt_pdev_t *pdev);

#ifdef ATH_11AC_TXCOMPACT
void
htt_htc_misc_pkt_list_add(struct htt_pdev_t *pdev, struct htt_htc_pkt *pkt);

void htt_htc_misc_pkt_pool_free(struct htt_pdev_t *pdev);
#endif

int
htt_rx_hash_list_insert(struct htt_pdev_t *pdev, uint32_t paddr,
			qdf_nbuf_t netbuf);

qdf_nbuf_t htt_rx_hash_list_lookup(struct htt_pdev_t *pdev, uint32_t paddr);

#ifdef IPA_OFFLOAD
int
htt_tx_ipa_uc_attach(struct htt_pdev_t *pdev,
		     unsigned int uc_tx_buf_sz,
		     unsigned int uc_tx_buf_cnt,
		     unsigned int uc_tx_partition_base);

int
htt_rx_ipa_uc_attach(struct htt_pdev_t *pdev, unsigned int rx_ind_ring_size);

int htt_tx_ipa_uc_detach(struct htt_pdev_t *pdev);

int htt_rx_ipa_uc_detach(struct htt_pdev_t *pdev);
#else
/**
 * htt_tx_ipa_uc_attach() - attach htt ipa uc tx resource
 * @pdev: htt context
 * @uc_tx_buf_sz: single tx buffer size
 * @uc_tx_buf_cnt: total tx buffer count
 * @uc_tx_partition_base: tx buffer partition start
 *
 * Return: 0 success
 */
static inline int
htt_tx_ipa_uc_attach(struct htt_pdev_t *pdev,
		     unsigned int uc_tx_buf_sz,
		     unsigned int uc_tx_buf_cnt,
		     unsigned int uc_tx_partition_base)
{
	return 0;
}

/**
 * htt_rx_ipa_uc_attach() - attach htt ipa uc rx resource
 * @pdev: htt context
 * @rx_ind_ring_size: rx ring size
 *
 * Return: 0 success
 */
static inline int
htt_rx_ipa_uc_attach(struct htt_pdev_t *pdev, unsigned int rx_ind_ring_size)
{
	return 0;
}

static inline int htt_tx_ipa_uc_detach(struct htt_pdev_t *pdev)
{
	return 0;
}

static inline int htt_rx_ipa_uc_detach(struct htt_pdev_t *pdev)
{
	return 0;
}
#endif /* IPA_OFFLOAD */
#ifdef DEBUG_RX_RING_BUFFER
/**
 * htt_rx_dbg_rxbuf_init() - init debug rx buff list
 * @pdev: pdev handle
 *
 * Return: none
 */
static inline
void htt_rx_dbg_rxbuf_init(struct htt_pdev_t *pdev)
{
	pdev->rx_buff_list = qdf_mem_malloc(
				 HTT_RX_RING_BUFF_DBG_LIST *
				 sizeof(struct rx_buf_debug));
	if (!pdev->rx_buff_list) {
		qdf_print("HTT: debug RX buffer allocation failed\n");
		QDF_ASSERT(0);
	}
}
/**
 * htt_rx_dbg_rxbuf_set() - set element of rx buff list
 * @pdev: pdev handle
 * @paddr: physical address of netbuf
 * @rx_netbuf: received netbuf
 *
 * Return: none
 */
static inline
void htt_rx_dbg_rxbuf_set(struct htt_pdev_t *pdev,
				uint32_t paddr,
				qdf_nbuf_t rx_netbuf)
{
	if (pdev->rx_buff_list) {
		pdev->rx_buff_list[pdev->rx_buff_index].paddr =
					paddr;
		pdev->rx_buff_list[pdev->rx_buff_index].in_use =
					true;
		pdev->rx_buff_list[pdev->rx_buff_index].vaddr =
					rx_netbuf;
		NBUF_MAP_ID(rx_netbuf) = pdev->rx_buff_index;
		if (++pdev->rx_buff_index ==
				HTT_RX_RING_BUFF_DBG_LIST)
			pdev->rx_buff_index = 0;
	}
}
/**
 * htt_rx_dbg_rxbuf_set() - reset element of rx buff list
 * @pdev: pdev handle
 * @netbuf: rx sk_buff
 * Return: none
 */
static inline
void htt_rx_dbg_rxbuf_reset(struct htt_pdev_t *pdev,
				qdf_nbuf_t netbuf)
{
	uint32_t index;

	if (pdev->rx_buff_list) {
		index = NBUF_MAP_ID(netbuf);
		if (index < HTT_RX_RING_BUFF_DBG_LIST) {
			pdev->rx_buff_list[index].in_use =
						false;
			pdev->rx_buff_list[index].paddr = 0;
			pdev->rx_buff_list[index].vaddr = NULL;
		}
	}
}
/**
 * htt_rx_dbg_rxbuf_deinit() - deinit debug rx buff list
 * @pdev: pdev handle
 *
 * Return: none
 */
static inline
void htt_rx_dbg_rxbuf_deinit(struct htt_pdev_t *pdev)
{
	if (pdev->rx_buff_list)
		qdf_mem_free(pdev->rx_buff_list);
}
#else
static inline
void htt_rx_dbg_rxbuf_init(struct htt_pdev_t *pdev)
{
	return;
}
static inline
void htt_rx_dbg_rxbuf_set(struct htt_pdev_t *pdev,
				uint32_t paddr,
				qdf_nbuf_t rx_netbuf)
{
	return;
}
static inline
void htt_rx_dbg_rxbuf_reset(struct htt_pdev_t *pdev,
				qdf_nbuf_t netbuf)
{
	return;
}
static inline
void htt_rx_dbg_rxbuf_deinit(struct htt_pdev_t *pdev)
{
	return;
}
#endif
#endif /* _HTT_INTERNAL__H_ */
