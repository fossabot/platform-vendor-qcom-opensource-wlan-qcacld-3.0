/*
 * Copyright (c) 2011-2015 The Linux Foundation. All rights reserved.
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

/*=== includes ===*/
/* header files for OS primitives */
#include <osdep.h>              /* uint32_t, etc. */
#include <cdf_memory.h>         /* cdf_mem_malloc,free */
#include <cdf_types.h>          /* cdf_device_t, cdf_print */
#include <cdf_lock.h>           /* cdf_spinlock */
#include <cdf_atomic.h>         /* cdf_atomic_read */

/* Required for WLAN_FEATURE_FASTPATH */
#include <ce_api.h>
/* header files for utilities */
#include <cds_queue.h>          /* TAILQ */

/* header files for configuration API */
#include <ol_cfg.h>             /* ol_cfg_is_high_latency */
#include <ol_if_athvar.h>

/* header files for HTT API */
#include <ol_htt_api.h>
#include <ol_htt_tx_api.h>

/* header files for OS shim API */
#include <ol_osif_api.h>

/* header files for our own APIs */
#include <ol_txrx_api.h>
#include <ol_txrx_dbg.h>
#include <ol_txrx_ctrl_api.h>
#include <ol_txrx_osif_api.h>
/* header files for our internal definitions */
#include <ol_txrx_internal.h>   /* TXRX_ASSERT, etc. */
#include <wdi_event.h>          /* WDI events */
#include <ol_txrx_types.h>      /* ol_txrx_pdev_t, etc. */
#include <ol_ctrl_txrx_api.h>
#include <ol_tx.h>              /* ol_tx_ll */
#include <ol_rx.h>              /* ol_rx_deliver */
#include <ol_txrx_peer_find.h>  /* ol_txrx_peer_find_attach, etc. */
#include <ol_rx_pn.h>           /* ol_rx_pn_check, etc. */
#include <ol_rx_fwd.h>          /* ol_rx_fwd_check, etc. */
#include <ol_rx_reorder_timeout.h>      /* OL_RX_REORDER_TIMEOUT_INIT, etc. */
#include <ol_rx_reorder.h>
#include <ol_tx_send.h>         /* ol_tx_discard_target_frms */
#include <ol_tx_desc.h>         /* ol_tx_desc_frame_free */
#include <ol_tx_queue.h>
#include <ol_txrx.h>
#include "wma.h"



/*=== function definitions ===*/

/**
 * ol_tx_set_is_mgmt_over_wmi_enabled() - set flag to indicate that mgmt over
 *                                        wmi is enabled or not.
 * @value: 1 for enabled/ 0 for disable
 *
 * Return: None
 */
void ol_tx_set_is_mgmt_over_wmi_enabled(uint8_t value)
{
	struct ol_txrx_pdev_t *pdev = cds_get_context(CDF_MODULE_ID_TXRX);
	if (!pdev) {
		cdf_print("%s: pdev is NULL\n", __func__);
		return;
	}
	pdev->is_mgmt_over_wmi_enabled = value;
	return;
}

/**
 * ol_tx_get_is_mgmt_over_wmi_enabled() - get value of is_mgmt_over_wmi_enabled
 *
 * Return: is_mgmt_over_wmi_enabled
 */
uint8_t ol_tx_get_is_mgmt_over_wmi_enabled(void)
{
	struct ol_txrx_pdev_t *pdev = cds_get_context(CDF_MODULE_ID_TXRX);
	if (!pdev) {
		cdf_print("%s: pdev is NULL\n", __func__);
		return 0;
	}
	return pdev->is_mgmt_over_wmi_enabled;
}


#ifdef QCA_SUPPORT_TXRX_LOCAL_PEER_ID
ol_txrx_peer_handle
ol_txrx_find_peer_by_addr_and_vdev(ol_txrx_pdev_handle pdev,
				   ol_txrx_vdev_handle vdev,
				   uint8_t *peer_addr, uint8_t *peer_id)
{
	struct ol_txrx_peer_t *peer;

	peer = ol_txrx_peer_vdev_find_hash(pdev, vdev, peer_addr, 0, 1);
	if (!peer)
		return NULL;
	*peer_id = peer->local_id;
	cdf_atomic_dec(&peer->ref_cnt);
	return peer;
}

CDF_STATUS ol_txrx_get_vdevid(struct ol_txrx_peer_t *peer, uint8_t *vdev_id)
{
	if (!peer) {
		CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
				  "peer argument is null!!");
		return CDF_STATUS_E_FAILURE;
	}

	*vdev_id = peer->vdev->vdev_id;
	return CDF_STATUS_SUCCESS;
}

void *ol_txrx_get_vdev_by_sta_id(uint8_t sta_id)
{
	struct ol_txrx_peer_t *peer = NULL;
	ol_txrx_pdev_handle pdev = NULL;

	if (sta_id >= WLAN_MAX_STA_COUNT) {
		CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
			  "Invalid sta id passed");
		return NULL;
	}

	pdev = cds_get_context(CDF_MODULE_ID_TXRX);
	if (!pdev) {
		CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
			      "PDEV not found for sta_id [%d]", sta_id);
		return NULL;
	}

	peer = ol_txrx_peer_find_by_local_id(pdev, sta_id);
	if (!peer) {
		CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
			      "PEER [%d] not found", sta_id);
		return NULL;
	}

	return peer->vdev;
}

ol_txrx_peer_handle ol_txrx_find_peer_by_addr(ol_txrx_pdev_handle pdev,
					      uint8_t *peer_addr,
					      uint8_t *peer_id)
{
	struct ol_txrx_peer_t *peer;

	peer = ol_txrx_peer_find_hash_find(pdev, peer_addr, 0, 1);
	if (!peer)
		return NULL;
	*peer_id = peer->local_id;
	cdf_atomic_dec(&peer->ref_cnt);
	return peer;
}

uint16_t ol_txrx_local_peer_id(ol_txrx_peer_handle peer)
{
	return peer->local_id;
}

ol_txrx_peer_handle
ol_txrx_peer_find_by_local_id(struct ol_txrx_pdev_t *pdev,
			      uint8_t local_peer_id)
{
	struct ol_txrx_peer_t *peer;
	if ((local_peer_id == OL_TXRX_INVALID_LOCAL_PEER_ID) ||
	    (local_peer_id >= OL_TXRX_NUM_LOCAL_PEER_IDS)) {
		return NULL;
	}

	cdf_spin_lock_bh(&pdev->local_peer_ids.lock);
	peer = pdev->local_peer_ids.map[local_peer_id];
	cdf_spin_unlock_bh(&pdev->local_peer_ids.lock);
	return peer;
}

static void ol_txrx_local_peer_id_pool_init(struct ol_txrx_pdev_t *pdev)
{
	int i;

	/* point the freelist to the first ID */
	pdev->local_peer_ids.freelist = 0;

	/* link each ID to the next one */
	for (i = 0; i < OL_TXRX_NUM_LOCAL_PEER_IDS; i++) {
		pdev->local_peer_ids.pool[i] = i + 1;
		pdev->local_peer_ids.map[i] = NULL;
	}

	/* link the last ID to itself, to mark the end of the list */
	i = OL_TXRX_NUM_LOCAL_PEER_IDS;
	pdev->local_peer_ids.pool[i] = i;

	cdf_spinlock_init(&pdev->local_peer_ids.lock);
}

static void
ol_txrx_local_peer_id_alloc(struct ol_txrx_pdev_t *pdev,
			    struct ol_txrx_peer_t *peer)
{
	int i;

	cdf_spin_lock_bh(&pdev->local_peer_ids.lock);
	i = pdev->local_peer_ids.freelist;
	if (pdev->local_peer_ids.pool[i] == i) {
		/* the list is empty, except for the list-end marker */
		peer->local_id = OL_TXRX_INVALID_LOCAL_PEER_ID;
	} else {
		/* take the head ID and advance the freelist */
		peer->local_id = i;
		pdev->local_peer_ids.freelist = pdev->local_peer_ids.pool[i];
		pdev->local_peer_ids.map[i] = peer;
	}
	cdf_spin_unlock_bh(&pdev->local_peer_ids.lock);
}

static void
ol_txrx_local_peer_id_free(struct ol_txrx_pdev_t *pdev,
			   struct ol_txrx_peer_t *peer)
{
	int i = peer->local_id;
	if ((i == OL_TXRX_INVALID_LOCAL_PEER_ID) ||
	    (i >= OL_TXRX_NUM_LOCAL_PEER_IDS)) {
		return;
	}
	/* put this ID on the head of the freelist */
	cdf_spin_lock_bh(&pdev->local_peer_ids.lock);
	pdev->local_peer_ids.pool[i] = pdev->local_peer_ids.freelist;
	pdev->local_peer_ids.freelist = i;
	pdev->local_peer_ids.map[i] = NULL;
	cdf_spin_unlock_bh(&pdev->local_peer_ids.lock);
}

static void ol_txrx_local_peer_id_cleanup(struct ol_txrx_pdev_t *pdev)
{
	cdf_spinlock_destroy(&pdev->local_peer_ids.lock);
}

#else
#define ol_txrx_local_peer_id_pool_init(pdev)   /* no-op */
#define ol_txrx_local_peer_id_alloc(pdev, peer) /* no-op */
#define ol_txrx_local_peer_id_free(pdev, peer)  /* no-op */
#define ol_txrx_local_peer_id_cleanup(pdev)     /* no-op */
#endif

#ifdef WLAN_FEATURE_FASTPATH
/**
 * setup_fastpath_ce_handles() Update pdev with ce_handle for fastpath use.
 *
 * @osc: pointer to HIF context
 * @pdev: pointer to ol pdev
 *
 * Return: void
 */
static inline void
setup_fastpath_ce_handles(struct ol_softc *osc, struct ol_txrx_pdev_t *pdev)
{
	/*
	 * Before the HTT attach, set up the CE handles
	 * CE handles are (struct CE_state *)
	 * This is only required in the fast path
	 */
	pdev->ce_tx_hdl = (struct CE_handle *)
		osc->ce_id_to_state[CE_HTT_H2T_MSG];

}

#else  /* not WLAN_FEATURE_FASTPATH */
static inline void
setup_fastpath_ce_handles(struct ol_softc *osc, struct ol_txrx_pdev_t *pdev)
{
}
#endif /* WLAN_FEATURE_FASTPATH */

#ifdef QCA_LL_TX_FLOW_CONTROL_V2
/**
 * ol_tx_set_desc_global_pool_size() - set global pool size
 * @num_msdu_desc: total number of descriptors
 *
 * Return: none
 */
void ol_tx_set_desc_global_pool_size(uint32_t num_msdu_desc)
{
	struct ol_txrx_pdev_t *pdev = cds_get_context(CDF_MODULE_ID_TXRX);
	if (!pdev) {
		cdf_print("%s: pdev is NULL\n", __func__);
		return;
	}
	pdev->num_msdu_desc = num_msdu_desc + TX_FLOW_MGMT_POOL_SIZE;
	TXRX_PRINT(TXRX_PRINT_LEVEL_ERR, "Global pool size: %d = %d + %d\n",
		pdev->num_msdu_desc, num_msdu_desc, TX_FLOW_MGMT_POOL_SIZE);
	return;
}

/**
 * ol_tx_get_desc_global_pool_size() - get global pool size
 * @pdev: pdev handle
 *
 * Return: global pool size
 */
static inline
uint32_t ol_tx_get_desc_global_pool_size(struct ol_txrx_pdev_t *pdev)
{
	return pdev->num_msdu_desc;
}
#else
/**
 * ol_tx_get_desc_global_pool_size() - get global pool size
 * @pdev: pdev handle
 *
 * Return: global pool size
 */
static inline
uint32_t ol_tx_get_desc_global_pool_size(struct ol_txrx_pdev_t *pdev)
{
	return ol_cfg_target_tx_credit(pdev->ctrl_pdev);
}
#endif

/**
 * ol_txrx_pdev_alloc() - allocate txrx pdev
 * @ctrl_pdev: cfg pdev
 * @htc_pdev: HTC pdev
 * @osdev: os dev
 *
 * Return: txrx pdev handle
 *		  NULL for failure
 */
ol_txrx_pdev_handle
ol_txrx_pdev_alloc(ol_pdev_handle ctrl_pdev,
		    HTC_HANDLE htc_pdev, cdf_device_t osdev)
{
	struct ol_txrx_pdev_t *pdev;
	int i;

	pdev = cdf_mem_malloc(sizeof(*pdev));
	if (!pdev)
		goto fail0;
	cdf_mem_zero(pdev, sizeof(*pdev));

	pdev->cfg.default_tx_comp_req = !ol_cfg_tx_free_at_download(ctrl_pdev);

	/* store provided params */
	pdev->ctrl_pdev = ctrl_pdev;
	pdev->osdev = osdev;

	for (i = 0; i < htt_num_sec_types; i++)
		pdev->sec_types[i] = (enum ol_sec_type)i;

	TXRX_STATS_INIT(pdev);

	TAILQ_INIT(&pdev->vdev_list);

	/* do initial set up of the peer ID -> peer object lookup map */
	if (ol_txrx_peer_find_attach(pdev))
		goto fail1;

	pdev->htt_pdev =
		htt_pdev_alloc(pdev, ctrl_pdev, htc_pdev, osdev);
	if (!pdev->htt_pdev)
		goto fail2;

	return pdev;

fail2:
	ol_txrx_peer_find_detach(pdev);

fail1:
	cdf_mem_free(pdev);

fail0:
	return NULL;
}

/**
 * ol_txrx_pdev_attach() - attach txrx pdev
 * @pdev: txrx pdev
 *
 * Return: 0 for success
 */
int
ol_txrx_pdev_attach(ol_txrx_pdev_handle pdev)
{
	uint16_t i;
	uint16_t fail_idx = 0;
	int ret = 0;
	uint16_t desc_pool_size;
	struct ol_softc *osc =  cds_get_context(CDF_MODULE_ID_HIF);

	uint16_t desc_element_size = sizeof(union ol_tx_desc_list_elem_t);
	union ol_tx_desc_list_elem_t *c_element;
	unsigned int sig_bit;
	uint16_t desc_per_page;

	if (!osc) {
		ret = -EINVAL;
		goto ol_attach_fail;
	}

	/*
	 * For LL, limit the number of host's tx descriptors to match
	 * the number of target FW tx descriptors.
	 * This simplifies the FW, by ensuring the host will never
	 * download more tx descriptors than the target has space for.
	 * The FW will drop/free low-priority tx descriptors when it
	 * starts to run low, so that in theory the host should never
	 * run out of tx descriptors.
	 */

	/* initialize the counter of the target's tx buffer availability */
	cdf_atomic_init(&pdev->target_tx_credit);
	cdf_atomic_init(&pdev->orig_target_tx_credit);
	/*
	 * LL - initialize the target credit outselves.
	 * HL - wait for a HTT target credit initialization during htt_attach.
	 */

	cdf_atomic_add(ol_cfg_target_tx_credit(pdev->ctrl_pdev),
		   &pdev->target_tx_credit);

	desc_pool_size = ol_tx_get_desc_global_pool_size(pdev);

	setup_fastpath_ce_handles(osc, pdev);

	ret = htt_attach(pdev->htt_pdev, desc_pool_size);
	if (ret)
		goto ol_attach_fail;

	/* Update CE's pkt download length */
	ce_pkt_dl_len_set((void *)osc, htt_pkt_dl_len_get(pdev->htt_pdev));

	/* Attach micro controller data path offload resource */
	if (ol_cfg_ipa_uc_offload_enabled(pdev->ctrl_pdev))
		if (htt_ipa_uc_attach(pdev->htt_pdev))
			goto uc_attach_fail;

	/* Calculate single element reserved size power of 2 */
	pdev->tx_desc.desc_reserved_size = cdf_get_pwr2(desc_element_size);
	cdf_mem_multi_pages_alloc(pdev->osdev, &pdev->tx_desc.desc_pages,
		pdev->tx_desc.desc_reserved_size, desc_pool_size, 0, true);
	if ((0 == pdev->tx_desc.desc_pages.num_pages) ||
		(NULL == pdev->tx_desc.desc_pages.cacheable_pages)) {
		CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
			"Page alloc fail");
		goto page_alloc_fail;
	}
	desc_per_page = pdev->tx_desc.desc_pages.num_element_per_page;
	pdev->tx_desc.offset_filter = desc_per_page - 1;
	/* Calculate page divider to find page number */
	sig_bit = 0;
	while (desc_per_page) {
		sig_bit++;
		desc_per_page = desc_per_page >> 1;
	}
	pdev->tx_desc.page_divider = (sig_bit - 1);
	CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
		"page_divider 0x%x, offset_filter 0x%x num elem %d, ol desc num page %d, ol desc per page %d",
		pdev->tx_desc.page_divider, pdev->tx_desc.offset_filter,
		desc_pool_size, pdev->tx_desc.desc_pages.num_pages,
		pdev->tx_desc.desc_pages.num_element_per_page);

	/*
	 * Each SW tx desc (used only within the tx datapath SW) has a
	 * matching HTT tx desc (used for downloading tx meta-data to FW/HW).
	 * Go ahead and allocate the HTT tx desc and link it with the SW tx
	 * desc now, to avoid doing it during time-critical transmit.
	 */
	pdev->tx_desc.pool_size = desc_pool_size;
	pdev->tx_desc.freelist =
		(union ol_tx_desc_list_elem_t *)
		(*pdev->tx_desc.desc_pages.cacheable_pages);
	c_element = pdev->tx_desc.freelist;
	for (i = 0; i < desc_pool_size; i++) {
		void *htt_tx_desc;
		void *htt_frag_desc = NULL;
		uint32_t frag_paddr_lo = 0;
		uint32_t paddr_lo;

		if (i == (desc_pool_size - 1))
			c_element->next = NULL;
		else
			c_element->next = (union ol_tx_desc_list_elem_t *)
				ol_tx_desc_find(pdev, i + 1);

		htt_tx_desc = htt_tx_desc_alloc(pdev->htt_pdev, &paddr_lo, i);
		if (!htt_tx_desc) {
			CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_FATAL,
				  "%s: failed to alloc HTT tx desc (%d of %d)",
				__func__, i, desc_pool_size);
			fail_idx = i;
			goto desc_alloc_fail;
		}

		c_element->tx_desc.htt_tx_desc = htt_tx_desc;
		c_element->tx_desc.htt_tx_desc_paddr = paddr_lo;
		ret = htt_tx_frag_alloc(pdev->htt_pdev,
			i, &frag_paddr_lo, &htt_frag_desc);
		if (ret) {
			CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
				"%s: failed to alloc HTT frag dsc (%d/%d)",
				__func__, i, desc_pool_size);
			/* Is there a leak here, is this handling correct? */
			fail_idx = i;
			goto desc_alloc_fail;
		}
		if (!ret && htt_frag_desc) {
			/* Initialize the first 6 words (TSO flags)
			   of the frag descriptor */
			memset(htt_frag_desc, 0, 6 * sizeof(uint32_t));
			c_element->tx_desc.htt_frag_desc = htt_frag_desc;
			c_element->tx_desc.htt_frag_desc_paddr = frag_paddr_lo;
		}
		CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_INFO_LOW,
			"%s:%d - %d FRAG VA 0x%p FRAG PA 0x%x",
			__func__, __LINE__, i,
			c_element->tx_desc.htt_frag_desc,
			c_element->tx_desc.htt_frag_desc_paddr);
#ifdef QCA_SUPPORT_TXDESC_SANITY_CHECKS
		c_element->tx_desc.pkt_type = 0xff;
#ifdef QCA_COMPUTE_TX_DELAY
		c_element->tx_desc.entry_timestamp_ticks =
			0xffffffff;
#endif
#endif
		c_element->tx_desc.id = i;
		cdf_atomic_init(&c_element->tx_desc.ref_cnt);
		c_element = c_element->next;
		fail_idx = i;
	}

	/* link SW tx descs into a freelist */
	pdev->tx_desc.num_free = desc_pool_size;
	TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1,
		   "%s first tx_desc:0x%p Last tx desc:0x%p\n", __func__,
		   (uint32_t *) pdev->tx_desc.freelist,
		   (uint32_t *) (pdev->tx_desc.freelist + desc_pool_size));

	/* check what format of frames are expected to be delivered by the OS */
	pdev->frame_format = ol_cfg_frame_type(pdev->ctrl_pdev);
	if (pdev->frame_format == wlan_frm_fmt_native_wifi)
		pdev->htt_pkt_type = htt_pkt_type_native_wifi;
	else if (pdev->frame_format == wlan_frm_fmt_802_3) {
		if (ol_cfg_is_ce_classify_enabled(pdev->ctrl_pdev))
			pdev->htt_pkt_type = htt_pkt_type_eth2;
		else
			pdev->htt_pkt_type = htt_pkt_type_ethernet;
	} else {
		CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
			  "%s Invalid standard frame type: %d",
			  __func__, pdev->frame_format);
		goto control_init_fail;
	}

	/* setup the global rx defrag waitlist */
	TAILQ_INIT(&pdev->rx.defrag.waitlist);

	/* configure where defrag timeout and duplicate detection is handled */
	pdev->rx.flags.defrag_timeout_check =
		pdev->rx.flags.dup_check =
		ol_cfg_rx_host_defrag_timeout_duplicate_check(pdev->ctrl_pdev);

#ifdef QCA_SUPPORT_SW_TXRX_ENCAP
	/* Need to revisit this part. Currently,hardcode to riva's caps */
	pdev->target_tx_tran_caps = wlan_frm_tran_cap_raw;
	pdev->target_rx_tran_caps = wlan_frm_tran_cap_raw;
	/*
	 * The Riva HW de-aggregate doesn't have capability to generate 802.11
	 * header for non-first subframe of A-MSDU.
	 */
	pdev->sw_subfrm_hdr_recovery_enable = 1;
	/*
	 * The Riva HW doesn't have the capability to set Protected Frame bit
	 * in the MAC header for encrypted data frame.
	 */
	pdev->sw_pf_proc_enable = 1;

	if (pdev->frame_format == wlan_frm_fmt_802_3) {
		/* sw llc process is only needed in
		   802.3 to 802.11 transform case */
		pdev->sw_tx_llc_proc_enable = 1;
		pdev->sw_rx_llc_proc_enable = 1;
	} else {
		pdev->sw_tx_llc_proc_enable = 0;
		pdev->sw_rx_llc_proc_enable = 0;
	}

	switch (pdev->frame_format) {
	case wlan_frm_fmt_raw:
		pdev->sw_tx_encap =
			pdev->target_tx_tran_caps & wlan_frm_tran_cap_raw
			? 0 : 1;
		pdev->sw_rx_decap =
			pdev->target_rx_tran_caps & wlan_frm_tran_cap_raw
			? 0 : 1;
		break;
	case wlan_frm_fmt_native_wifi:
		pdev->sw_tx_encap =
			pdev->
			target_tx_tran_caps & wlan_frm_tran_cap_native_wifi
			? 0 : 1;
		pdev->sw_rx_decap =
			pdev->
			target_rx_tran_caps & wlan_frm_tran_cap_native_wifi
			? 0 : 1;
		break;
	case wlan_frm_fmt_802_3:
		pdev->sw_tx_encap =
			pdev->target_tx_tran_caps & wlan_frm_tran_cap_8023
			? 0 : 1;
		pdev->sw_rx_decap =
			pdev->target_rx_tran_caps & wlan_frm_tran_cap_8023
			? 0 : 1;
		break;
	default:
		CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
			  "Invalid std frame type; [en/de]cap: f:%x t:%x r:%x",
			  pdev->frame_format,
			  pdev->target_tx_tran_caps, pdev->target_rx_tran_caps);
		goto control_init_fail;
	}
#endif

	/*
	 * Determine what rx processing steps are done within the host.
	 * Possibilities:
	 * 1.  Nothing - rx->tx forwarding and rx PN entirely within target.
	 *     (This is unlikely; even if the target is doing rx->tx forwarding,
	 *     the host should be doing rx->tx forwarding too, as a back up for
	 *     the target's rx->tx forwarding, in case the target runs short on
	 *     memory, and can't store rx->tx frames that are waiting for
	 *     missing prior rx frames to arrive.)
	 * 2.  Just rx -> tx forwarding.
	 *     This is the typical configuration for HL, and a likely
	 *     configuration for LL STA or small APs (e.g. retail APs).
	 * 3.  Both PN check and rx -> tx forwarding.
	 *     This is the typical configuration for large LL APs.
	 * Host-side PN check without rx->tx forwarding is not a valid
	 * configuration, since the PN check needs to be done prior to
	 * the rx->tx forwarding.
	 */
	if (ol_cfg_is_full_reorder_offload(pdev->ctrl_pdev)) {
		/* PN check, rx-tx forwarding and rx reorder is done by
		   the target */
		if (ol_cfg_rx_fwd_disabled(pdev->ctrl_pdev))
			pdev->rx_opt_proc = ol_rx_in_order_deliver;
		else
			pdev->rx_opt_proc = ol_rx_fwd_check;
	} else {
		if (ol_cfg_rx_pn_check(pdev->ctrl_pdev)) {
			if (ol_cfg_rx_fwd_disabled(pdev->ctrl_pdev)) {
				/*
				 * PN check done on host,
				 * rx->tx forwarding not done at all.
				 */
				pdev->rx_opt_proc = ol_rx_pn_check_only;
			} else if (ol_cfg_rx_fwd_check(pdev->ctrl_pdev)) {
				/*
				 * Both PN check and rx->tx forwarding done
				 * on host.
				 */
				pdev->rx_opt_proc = ol_rx_pn_check;
			} else {
#define TRACESTR01 "invalid config: if rx PN check is on the host,"\
"rx->tx forwarding check needs to also be on the host"
				CDF_TRACE(CDF_MODULE_ID_TXRX,
					  CDF_TRACE_LEVEL_ERROR,
					  "%s: %s", __func__, TRACESTR01);
#undef TRACESTR01
				goto control_init_fail;
			}
		} else {
			/* PN check done on target */
			if ((!ol_cfg_rx_fwd_disabled(pdev->ctrl_pdev)) &&
			    ol_cfg_rx_fwd_check(pdev->ctrl_pdev)) {
				/*
				 * rx->tx forwarding done on host (possibly as
				 * back-up for target-side primary rx->tx
				 * forwarding)
				 */
				pdev->rx_opt_proc = ol_rx_fwd_check;
			} else {
				/* rx->tx forwarding either done in target,
				 * or not done at all */
				pdev->rx_opt_proc = ol_rx_deliver;
			}
		}
	}

	/* initialize mutexes for tx desc alloc and peer lookup */
	cdf_spinlock_init(&pdev->tx_mutex);
	cdf_spinlock_init(&pdev->peer_ref_mutex);
	cdf_spinlock_init(&pdev->rx.mutex);
	cdf_spinlock_init(&pdev->last_real_peer_mutex);
	OL_TXRX_PEER_STATS_MUTEX_INIT(pdev);

	if (OL_RX_REORDER_TRACE_ATTACH(pdev) != A_OK)
		goto reorder_trace_attach_fail;

	if (OL_RX_PN_TRACE_ATTACH(pdev) != A_OK)
		goto pn_trace_attach_fail;

#ifdef PERE_IP_HDR_ALIGNMENT_WAR
	pdev->host_80211_enable = ol_scn_host_80211_enable_get(pdev->ctrl_pdev);
#endif

	/*
	 * WDI event attach
	 */
	wdi_event_attach(pdev);

	/*
	 * Initialize rx PN check characteristics for different security types.
	 */
	cdf_mem_set(&pdev->rx_pn[0], sizeof(pdev->rx_pn), 0);

	/* TKIP: 48-bit TSC, CCMP: 48-bit PN */
	pdev->rx_pn[htt_sec_type_tkip].len =
		pdev->rx_pn[htt_sec_type_tkip_nomic].len =
			pdev->rx_pn[htt_sec_type_aes_ccmp].len = 48;
	pdev->rx_pn[htt_sec_type_tkip].cmp =
		pdev->rx_pn[htt_sec_type_tkip_nomic].cmp =
			pdev->rx_pn[htt_sec_type_aes_ccmp].cmp = ol_rx_pn_cmp48;

	/* WAPI: 128-bit PN */
	pdev->rx_pn[htt_sec_type_wapi].len = 128;
	pdev->rx_pn[htt_sec_type_wapi].cmp = ol_rx_pn_wapi_cmp;

	OL_RX_REORDER_TIMEOUT_INIT(pdev);

	TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "Created pdev %p\n", pdev);

	pdev->cfg.host_addba = ol_cfg_host_addba(pdev->ctrl_pdev);

#ifdef QCA_SUPPORT_PEER_DATA_RX_RSSI
#define OL_TXRX_RSSI_UPDATE_SHIFT_DEFAULT 3

/* #if 1 -- TODO: clean this up */
#define OL_TXRX_RSSI_NEW_WEIGHT_DEFAULT	\
	/* avg = 100% * new + 0% * old */ \
	(1 << OL_TXRX_RSSI_UPDATE_SHIFT_DEFAULT)
/*
#else
#define OL_TXRX_RSSI_NEW_WEIGHT_DEFAULT
	//avg = 25% * new + 25% * old
	(1 << (OL_TXRX_RSSI_UPDATE_SHIFT_DEFAULT-2))
#endif
*/
	pdev->rssi_update_shift = OL_TXRX_RSSI_UPDATE_SHIFT_DEFAULT;
	pdev->rssi_new_weight = OL_TXRX_RSSI_NEW_WEIGHT_DEFAULT;
#endif

	ol_txrx_local_peer_id_pool_init(pdev);

	pdev->cfg.ll_pause_txq_limit =
		ol_tx_cfg_max_tx_queue_depth_ll(pdev->ctrl_pdev);

#ifdef QCA_COMPUTE_TX_DELAY
	cdf_mem_zero(&pdev->tx_delay, sizeof(pdev->tx_delay));
	cdf_spinlock_init(&pdev->tx_delay.mutex);

	/* initialize compute interval with 5 seconds (ESE default) */
	pdev->tx_delay.avg_period_ticks = cdf_system_msecs_to_ticks(5000);
	{
		uint32_t bin_width_1000ticks;
		bin_width_1000ticks =
			cdf_system_msecs_to_ticks
				(QCA_TX_DELAY_HIST_INTERNAL_BIN_WIDTH_MS
				 * 1000);
		/*
		 * Compute a factor and shift that together are equal to the
		 * inverse of the bin_width time, so that rather than dividing
		 * by the bin width time, approximately the same result can be
		 * obtained much more efficiently by a multiply + shift.
		 * multiply_factor >> shift = 1 / bin_width_time, so
		 * multiply_factor = (1 << shift) / bin_width_time.
		 *
		 * Pick the shift semi-arbitrarily.
		 * If we knew statically what the bin_width would be, we could
		 * choose a shift that minimizes the error.
		 * Since the bin_width is determined dynamically, simply use a
		 * shift that is about half of the uint32_t size.  This should
		 * result in a relatively large multiplier value, which
		 * minimizes error from rounding the multiplier to an integer.
		 * The rounding error only becomes significant if the tick units
		 * are on the order of 1 microsecond.  In most systems, it is
		 * expected that the tick units will be relatively low-res,
		 * on the order of 1 millisecond.  In such systems the rounding
		 * error is negligible.
		 * It would be more accurate to dynamically try out different
		 * shifts and choose the one that results in the smallest
		 * rounding error, but that extra level of fidelity is
		 * not needed.
		 */
		pdev->tx_delay.hist_internal_bin_width_shift = 16;
		pdev->tx_delay.hist_internal_bin_width_mult =
			((1 << pdev->tx_delay.hist_internal_bin_width_shift) *
			 1000 + (bin_width_1000ticks >> 1)) /
			bin_width_1000ticks;
	}
#endif /* QCA_COMPUTE_TX_DELAY */

	/* Thermal Mitigation */
	ol_tx_throttle_init(pdev);
	ol_tso_seg_list_init(pdev, desc_pool_size);
	ol_tx_register_flow_control(pdev);

	return 0;            /* success */

pn_trace_attach_fail:
	OL_RX_REORDER_TRACE_DETACH(pdev);

reorder_trace_attach_fail:
	cdf_spinlock_destroy(&pdev->tx_mutex);
	cdf_spinlock_destroy(&pdev->peer_ref_mutex);
	cdf_spinlock_destroy(&pdev->rx.mutex);
	cdf_spinlock_destroy(&pdev->last_real_peer_mutex);
	OL_TXRX_PEER_STATS_MUTEX_DESTROY(pdev);

control_init_fail:
desc_alloc_fail:
	for (i = 0; i < fail_idx; i++)
		htt_tx_desc_free(pdev->htt_pdev,
			(ol_tx_desc_find(pdev, i))->htt_tx_desc);

	cdf_mem_multi_pages_free(pdev->osdev,
		&pdev->tx_desc.desc_pages, 0, true);

page_alloc_fail:
	if (ol_cfg_ipa_uc_offload_enabled(pdev->ctrl_pdev))
		htt_ipa_uc_detach(pdev->htt_pdev);
uc_attach_fail:
	htt_detach(pdev->htt_pdev);

ol_attach_fail:
	return ret;            /* fail */
}

A_STATUS ol_txrx_pdev_attach_target(ol_txrx_pdev_handle pdev)
{
	return htt_attach_target(pdev->htt_pdev);
}

void ol_txrx_pdev_detach(ol_txrx_pdev_handle pdev, int force)
{
	int i;

	/*checking to ensure txrx pdev structure is not NULL */
	if (!pdev) {
		TXRX_PRINT(TXRX_PRINT_LEVEL_ERR, "NULL pdev passed to %s\n", __func__);
		return;
	}
	/* preconditions */
	TXRX_ASSERT2(pdev);

	/* check that the pdev has no vdevs allocated */
	TXRX_ASSERT1(TAILQ_EMPTY(&pdev->vdev_list));

	OL_RX_REORDER_TIMEOUT_CLEANUP(pdev);

#ifdef QCA_SUPPORT_TX_THROTTLE
	/* Thermal Mitigation */
	cdf_softirq_timer_cancel(&pdev->tx_throttle.phase_timer);
	cdf_softirq_timer_free(&pdev->tx_throttle.phase_timer);
#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
	cdf_softirq_timer_cancel(&pdev->tx_throttle.tx_timer);
	cdf_softirq_timer_free(&pdev->tx_throttle.tx_timer);
#endif
#endif
	ol_tso_seg_list_deinit(pdev);
	ol_tx_deregister_flow_control(pdev);

	if (force) {
		/*
		 * The assertion above confirms that all vdevs within this pdev
		 * were detached.  However, they may not have actually been
		 * deleted.
		 * If the vdev had peers which never received a PEER_UNMAP msg
		 * from the target, then there are still zombie peer objects,
		 * and the vdev parents of the zombie peers are also zombies,
		 * hanging around until their final peer gets deleted.
		 * Go through the peer hash table and delete any peers left.
		 * As a side effect, this will complete the deletion of any
		 * vdevs that are waiting for their peers to finish deletion.
		 */
		TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "Force delete for pdev %p\n",
			   pdev);
		ol_txrx_peer_find_hash_erase(pdev);
	}

	/* Stop the communication between HTT and target at first */
	htt_detach_target(pdev->htt_pdev);

	for (i = 0; i < pdev->tx_desc.pool_size; i++) {
		void *htt_tx_desc;
		struct ol_tx_desc_t *tx_desc;

		tx_desc = ol_tx_desc_find(pdev, i);
		/*
		 * Confirm that each tx descriptor is "empty", i.e. it has
		 * no tx frame attached.
		 * In particular, check that there are no frames that have
		 * been given to the target to transmit, for which the
		 * target has never provided a response.
		 */
		if (cdf_atomic_read(&tx_desc->ref_cnt)) {
			TXRX_PRINT(TXRX_PRINT_LEVEL_WARN,
				   "Warning: freeing tx frame (no compltn)\n");
			ol_tx_desc_frame_free_nonstd(pdev,
						     tx_desc, 1);
		}
		htt_tx_desc = tx_desc->htt_tx_desc;
		htt_tx_desc_free(pdev->htt_pdev, htt_tx_desc);
	}

	cdf_mem_multi_pages_free(pdev->osdev,
		&pdev->tx_desc.desc_pages, 0, true);
	pdev->tx_desc.freelist = NULL;

	/* Detach micro controller data path offload resource */
	if (ol_cfg_ipa_uc_offload_enabled(pdev->ctrl_pdev))
		htt_ipa_uc_detach(pdev->htt_pdev);

	htt_detach(pdev->htt_pdev);
	htt_pdev_free(pdev->htt_pdev);

	ol_txrx_peer_find_detach(pdev);

	cdf_spinlock_destroy(&pdev->tx_mutex);
	cdf_spinlock_destroy(&pdev->peer_ref_mutex);
	cdf_spinlock_destroy(&pdev->last_real_peer_mutex);
	cdf_spinlock_destroy(&pdev->rx.mutex);
#ifdef QCA_SUPPORT_TX_THROTTLE
	/* Thermal Mitigation */
	cdf_spinlock_destroy(&pdev->tx_throttle.mutex);
#endif
	OL_TXRX_PEER_STATS_MUTEX_DESTROY(pdev);

	OL_RX_REORDER_TRACE_DETACH(pdev);
	OL_RX_PN_TRACE_DETACH(pdev);
	/*
	 * WDI event detach
	 */
	wdi_event_detach(pdev);
	ol_txrx_local_peer_id_cleanup(pdev);

#ifdef QCA_COMPUTE_TX_DELAY
	cdf_spinlock_destroy(&pdev->tx_delay.mutex);
#endif
}

ol_txrx_vdev_handle
ol_txrx_vdev_attach(ol_txrx_pdev_handle pdev,
		    uint8_t *vdev_mac_addr,
		    uint8_t vdev_id, enum wlan_op_mode op_mode)
{
	struct ol_txrx_vdev_t *vdev;

	/* preconditions */
	TXRX_ASSERT2(pdev);
	TXRX_ASSERT2(vdev_mac_addr);

	vdev = cdf_mem_malloc(sizeof(*vdev));
	if (!vdev)
		return NULL;    /* failure */

	/* store provided params */
	vdev->pdev = pdev;
	vdev->vdev_id = vdev_id;
	vdev->opmode = op_mode;

	vdev->delete.pending = 0;
	vdev->safemode = 0;
	vdev->drop_unenc = 1;
	vdev->num_filters = 0;

	cdf_mem_copy(&vdev->mac_addr.raw[0], vdev_mac_addr,
		     OL_TXRX_MAC_ADDR_LEN);

	TAILQ_INIT(&vdev->peer_list);
	vdev->last_real_peer = NULL;

#ifdef QCA_IBSS_SUPPORT
	vdev->ibss_peer_num = 0;
	vdev->ibss_peer_heart_beat_timer = 0;
#endif

	cdf_spinlock_init(&vdev->ll_pause.mutex);
	vdev->ll_pause.paused_reason = 0;
	vdev->ll_pause.txq.head = vdev->ll_pause.txq.tail = NULL;
	vdev->ll_pause.txq.depth = 0;
	cdf_softirq_timer_init(pdev->osdev,
			       &vdev->ll_pause.timer,
			       ol_tx_vdev_ll_pause_queue_send, vdev,
			       CDF_TIMER_TYPE_SW);
	cdf_atomic_init(&vdev->os_q_paused);
	cdf_atomic_set(&vdev->os_q_paused, 0);
	vdev->tx_fl_lwm = 0;
	vdev->tx_fl_hwm = 0;
	vdev->wait_on_peer_id = OL_TXRX_INVALID_LOCAL_PEER_ID;
	cdf_spinlock_init(&vdev->flow_control_lock);
	vdev->osif_flow_control_cb = NULL;
	vdev->osif_fc_ctx = NULL;

	/* Default MAX Q depth for every VDEV */
	vdev->ll_pause.max_q_depth =
		ol_tx_cfg_max_tx_queue_depth_ll(vdev->pdev->ctrl_pdev);
	/* add this vdev into the pdev's list */
	TAILQ_INSERT_TAIL(&pdev->vdev_list, vdev, vdev_list_elem);

	TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1,
		   "Created vdev %p (%02x:%02x:%02x:%02x:%02x:%02x)\n",
		   vdev,
		   vdev->mac_addr.raw[0], vdev->mac_addr.raw[1],
		   vdev->mac_addr.raw[2], vdev->mac_addr.raw[3],
		   vdev->mac_addr.raw[4], vdev->mac_addr.raw[5]);

	/*
	 * We've verified that htt_op_mode == wlan_op_mode,
	 * so no translation is needed.
	 */
	htt_vdev_attach(pdev->htt_pdev, vdev_id, op_mode);

	return vdev;
}

void ol_txrx_osif_vdev_register(ol_txrx_vdev_handle vdev,
				void *osif_vdev,
				struct ol_txrx_osif_ops *txrx_ops)
{
	vdev->osif_dev = osif_vdev;
	txrx_ops->tx.std = vdev->tx = OL_TX_LL;
	txrx_ops->tx.non_std = ol_tx_non_std_ll;
}

void ol_txrx_set_curchan(ol_txrx_pdev_handle pdev, uint32_t chan_mhz)
{
	return;
}

void ol_txrx_set_safemode(ol_txrx_vdev_handle vdev, uint32_t val)
{
	vdev->safemode = val;
}

void
ol_txrx_set_privacy_filters(ol_txrx_vdev_handle vdev,
			    void *filters, uint32_t num)
{
	cdf_mem_copy(vdev->privacy_filters, filters,
		     num * sizeof(struct privacy_exemption));
	vdev->num_filters = num;
}

void ol_txrx_set_drop_unenc(ol_txrx_vdev_handle vdev, uint32_t val)
{
	vdev->drop_unenc = val;
}

void
ol_txrx_vdev_detach(ol_txrx_vdev_handle vdev,
		    ol_txrx_vdev_delete_cb callback, void *context)
{
	struct ol_txrx_pdev_t *pdev = vdev->pdev;

	/* preconditions */
	TXRX_ASSERT2(vdev);

	cdf_spin_lock_bh(&vdev->ll_pause.mutex);
	cdf_softirq_timer_cancel(&vdev->ll_pause.timer);
	cdf_softirq_timer_free(&vdev->ll_pause.timer);
	vdev->ll_pause.is_q_timer_on = false;
	while (vdev->ll_pause.txq.head) {
		cdf_nbuf_t next = cdf_nbuf_next(vdev->ll_pause.txq.head);
		cdf_nbuf_set_next(vdev->ll_pause.txq.head, NULL);
		cdf_nbuf_unmap(pdev->osdev, vdev->ll_pause.txq.head,
			       CDF_DMA_TO_DEVICE);
		cdf_nbuf_tx_free(vdev->ll_pause.txq.head, NBUF_PKT_ERROR);
		vdev->ll_pause.txq.head = next;
	}
	cdf_spin_unlock_bh(&vdev->ll_pause.mutex);
	cdf_spinlock_destroy(&vdev->ll_pause.mutex);

	cdf_spin_lock_bh(&vdev->flow_control_lock);
	vdev->osif_flow_control_cb = NULL;
	vdev->osif_fc_ctx = NULL;
	cdf_spin_unlock_bh(&vdev->flow_control_lock);
	cdf_spinlock_destroy(&vdev->flow_control_lock);

	/* remove the vdev from its parent pdev's list */
	TAILQ_REMOVE(&pdev->vdev_list, vdev, vdev_list_elem);

	/*
	 * Use peer_ref_mutex while accessing peer_list, in case
	 * a peer is in the process of being removed from the list.
	 */
	cdf_spin_lock_bh(&pdev->peer_ref_mutex);
	/* check that the vdev has no peers allocated */
	if (!TAILQ_EMPTY(&vdev->peer_list)) {
		/* debug print - will be removed later */
		TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1,
			   "%s: not deleting vdev object %p (%02x:%02x:%02x:%02x:%02x:%02x)"
			   "until deletion finishes for all its peers\n",
			   __func__, vdev,
			   vdev->mac_addr.raw[0], vdev->mac_addr.raw[1],
			   vdev->mac_addr.raw[2], vdev->mac_addr.raw[3],
			   vdev->mac_addr.raw[4], vdev->mac_addr.raw[5]);
		/* indicate that the vdev needs to be deleted */
		vdev->delete.pending = 1;
		vdev->delete.callback = callback;
		vdev->delete.context = context;
		cdf_spin_unlock_bh(&pdev->peer_ref_mutex);
		return;
	}
	cdf_spin_unlock_bh(&pdev->peer_ref_mutex);

	TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1,
		   "%s: deleting vdev obj %p (%02x:%02x:%02x:%02x:%02x:%02x)\n",
		   __func__, vdev,
		   vdev->mac_addr.raw[0], vdev->mac_addr.raw[1],
		   vdev->mac_addr.raw[2], vdev->mac_addr.raw[3],
		   vdev->mac_addr.raw[4], vdev->mac_addr.raw[5]);

	htt_vdev_detach(pdev->htt_pdev, vdev->vdev_id);

	/*
	 * Doesn't matter if there are outstanding tx frames -
	 * they will be freed once the target sends a tx completion
	 * message for them.
	 */
	cdf_mem_free(vdev);
	if (callback)
		callback(context);
}

/**
 * ol_txrx_flush_rx_frames() - flush cached rx frames
 * @peer: peer
 * @drop: set flag to drop frames
 *
 * Return: None
 */
void ol_txrx_flush_rx_frames(struct ol_txrx_peer_t *peer,
				    bool drop)
{
	struct ol_rx_cached_buf *cache_buf;
	CDF_STATUS ret;
	ol_rx_callback_fp data_rx = NULL;
	void *cds_ctx = cds_get_global_context();

	if (cdf_atomic_inc_return(&peer->flush_in_progress) > 1) {
		cdf_atomic_dec(&peer->flush_in_progress);
		return;
	}

	cdf_assert(cds_ctx);
	cdf_spin_lock_bh(&peer->peer_info_lock);
	if (peer->state >= ol_txrx_peer_state_conn)
		data_rx = peer->osif_rx;
	else
		drop = true;
	cdf_spin_unlock_bh(&peer->peer_info_lock);

	cdf_spin_lock_bh(&peer->bufq_lock);
	cache_buf = list_entry((&peer->cached_bufq)->next,
				typeof(*cache_buf), list);
	while (!list_empty(&peer->cached_bufq)) {
		list_del(&cache_buf->list);
		cdf_spin_unlock_bh(&peer->bufq_lock);
		if (drop) {
			cdf_nbuf_free(cache_buf->buf);
		} else {
			/* Flush the cached frames to HDD */
			ret = data_rx(cds_ctx, cache_buf->buf, peer->local_id);
			if (ret != CDF_STATUS_SUCCESS)
				cdf_nbuf_free(cache_buf->buf);
		}
		cdf_mem_free(cache_buf);
		cdf_spin_lock_bh(&peer->bufq_lock);
		cache_buf = list_entry((&peer->cached_bufq)->next,
				typeof(*cache_buf), list);
	}
	cdf_spin_unlock_bh(&peer->bufq_lock);
	cdf_atomic_dec(&peer->flush_in_progress);
}

ol_txrx_peer_handle
ol_txrx_peer_attach(ol_txrx_pdev_handle pdev,
		    ol_txrx_vdev_handle vdev, uint8_t *peer_mac_addr)
{
	struct ol_txrx_peer_t *peer;
	struct ol_txrx_peer_t *temp_peer;
	uint8_t i;
	int differs;
	bool wait_on_deletion = false;
	unsigned long rc;

	/* preconditions */
	TXRX_ASSERT2(pdev);
	TXRX_ASSERT2(vdev);
	TXRX_ASSERT2(peer_mac_addr);

	cdf_spin_lock_bh(&pdev->peer_ref_mutex);
	/* check for duplicate exsisting peer */
	TAILQ_FOREACH(temp_peer, &vdev->peer_list, peer_list_elem) {
		if (!ol_txrx_peer_find_mac_addr_cmp(&temp_peer->mac_addr,
			(union ol_txrx_align_mac_addr_t *)peer_mac_addr)) {
			TXRX_PRINT(TXRX_PRINT_LEVEL_ERR,
				"vdev_id %d (%02x:%02x:%02x:%02x:%02x:%02x) already exsist.\n",
				vdev->vdev_id,
				peer_mac_addr[0], peer_mac_addr[1],
				peer_mac_addr[2], peer_mac_addr[3],
				peer_mac_addr[4], peer_mac_addr[5]);
			if (cdf_atomic_read(&temp_peer->delete_in_progress)) {
				vdev->wait_on_peer_id = temp_peer->local_id;
				cdf_event_init(&vdev->wait_delete_comp);
				wait_on_deletion = true;
			} else {
				cdf_spin_unlock_bh(&pdev->peer_ref_mutex);
				return NULL;
			}
		}
	}
	cdf_spin_unlock_bh(&pdev->peer_ref_mutex);

	if (wait_on_deletion) {
		/* wait for peer deletion */
		rc = cdf_wait_single_event(&vdev->wait_delete_comp,
			cdf_system_msecs_to_ticks(PEER_DELETION_TIMEOUT));
		if (!rc) {
			TXRX_PRINT(TXRX_PRINT_LEVEL_ERR,
				"timedout waiting for peer(%d) deletion\n",
				vdev->wait_on_peer_id);
			vdev->wait_on_peer_id = OL_TXRX_INVALID_LOCAL_PEER_ID;
			return NULL;
		}
	}

	peer = cdf_mem_malloc(sizeof(*peer));
	if (!peer)
		return NULL;    /* failure */
	cdf_mem_zero(peer, sizeof(*peer));

	/* store provided params */
	peer->vdev = vdev;
	cdf_mem_copy(&peer->mac_addr.raw[0], peer_mac_addr,
		     OL_TXRX_MAC_ADDR_LEN);

	INIT_LIST_HEAD(&peer->cached_bufq);
	cdf_spin_lock_bh(&pdev->peer_ref_mutex);
	/* add this peer into the vdev's list */
	TAILQ_INSERT_TAIL(&vdev->peer_list, peer, peer_list_elem);
	cdf_spin_unlock_bh(&pdev->peer_ref_mutex);
	/* check whether this is a real peer (peer mac addr != vdev mac addr) */
	if (ol_txrx_peer_find_mac_addr_cmp(&vdev->mac_addr, &peer->mac_addr))
		vdev->last_real_peer = peer;

	peer->rx_opt_proc = pdev->rx_opt_proc;

	ol_rx_peer_init(pdev, peer);

	/* initialize the peer_id */
	for (i = 0; i < MAX_NUM_PEER_ID_PER_PEER; i++)
		peer->peer_ids[i] = HTT_INVALID_PEER;


	peer->osif_rx = NULL;
	cdf_spinlock_init(&peer->peer_info_lock);
	cdf_spinlock_init(&peer->bufq_lock);

	cdf_atomic_init(&peer->delete_in_progress);
	cdf_atomic_init(&peer->flush_in_progress);

	cdf_atomic_init(&peer->ref_cnt);

	/* keep one reference for attach */
	cdf_atomic_inc(&peer->ref_cnt);

	/* keep one reference for ol_rx_peer_map_handler */
	cdf_atomic_inc(&peer->ref_cnt);

	peer->valid = 1;

	ol_txrx_peer_find_hash_add(pdev, peer);

	TXRX_PRINT(TXRX_PRINT_LEVEL_INFO2,
		   "vdev %p created peer %p (%02x:%02x:%02x:%02x:%02x:%02x)\n",
		   vdev, peer,
		   peer->mac_addr.raw[0], peer->mac_addr.raw[1],
		   peer->mac_addr.raw[2], peer->mac_addr.raw[3],
		   peer->mac_addr.raw[4], peer->mac_addr.raw[5]);
	/*
	 * For every peer MAp message search and set if bss_peer
	 */
	differs =
		cdf_mem_compare(peer->mac_addr.raw, vdev->mac_addr.raw,
				OL_TXRX_MAC_ADDR_LEN);
	if (!differs)
		peer->bss_peer = 1;

	/*
	 * The peer starts in the "disc" state while association is in progress.
	 * Once association completes, the peer will get updated to "auth" state
	 * by a call to ol_txrx_peer_state_update if the peer is in open mode,
	 * or else to the "conn" state. For non-open mode, the peer will
	 * progress to "auth" state once the authentication completes.
	 */
	peer->state = ol_txrx_peer_state_invalid;
	ol_txrx_peer_state_update(pdev, peer->mac_addr.raw,
				  ol_txrx_peer_state_disc);

#ifdef QCA_SUPPORT_PEER_DATA_RX_RSSI
	peer->rssi_dbm = HTT_RSSI_INVALID;
#endif

	ol_txrx_local_peer_id_alloc(pdev, peer);

	return peer;
}

/*
 * Discarding tx filter - removes all data frames (disconnected state)
 */
static A_STATUS ol_tx_filter_discard(struct ol_txrx_msdu_info_t *tx_msdu_info)
{
	return A_ERROR;
}

/*
 * Non-autentication tx filter - filters out data frames that are not
 * related to authentication, but allows EAPOL (PAE) or WAPI (WAI)
 * data frames (connected state)
 */
static A_STATUS ol_tx_filter_non_auth(struct ol_txrx_msdu_info_t *tx_msdu_info)
{
	return
		(tx_msdu_info->htt.info.ethertype == ETHERTYPE_PAE ||
		 tx_msdu_info->htt.info.ethertype ==
		 ETHERTYPE_WAI) ? A_OK : A_ERROR;
}

/*
 * Pass-through tx filter - lets all data frames through (authenticated state)
 */
static A_STATUS ol_tx_filter_pass_thru(struct ol_txrx_msdu_info_t *tx_msdu_info)
{
	return A_OK;
}

CDF_STATUS
ol_txrx_peer_state_update(struct ol_txrx_pdev_t *pdev, uint8_t *peer_mac,
			  enum ol_txrx_peer_state state)
{
	struct ol_txrx_peer_t *peer;

	if (cdf_unlikely(!pdev)) {
		TXRX_PRINT(TXRX_PRINT_LEVEL_ERR, "Pdev is NULL");
		cdf_assert(0);
		return CDF_STATUS_E_INVAL;
	}

	peer =  ol_txrx_peer_find_hash_find(pdev, peer_mac, 0, 1);
	if (NULL == peer) {
		TXRX_PRINT(TXRX_PRINT_LEVEL_INFO2, "%s: peer is null for peer_mac"
			" 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", __FUNCTION__,
			peer_mac[0], peer_mac[1], peer_mac[2], peer_mac[3],
			peer_mac[4], peer_mac[5]);
		return CDF_STATUS_E_INVAL;
	}

	/* TODO: Should we send WMI command of the connection state? */
	/* avoid multiple auth state change. */
	if (peer->state == state) {
#ifdef TXRX_PRINT_VERBOSE_ENABLE
		TXRX_PRINT(TXRX_PRINT_LEVEL_INFO3,
			   "%s: no state change, returns directly\n",
			   __func__);
#endif
		cdf_atomic_dec(&peer->ref_cnt);
		return CDF_STATUS_SUCCESS;
	}

	TXRX_PRINT(TXRX_PRINT_LEVEL_INFO2, "%s: change from %d to %d\n",
		   __func__, peer->state, state);

	peer->tx_filter = (state == ol_txrx_peer_state_auth)
		? ol_tx_filter_pass_thru
		: ((state == ol_txrx_peer_state_conn)
		   ? ol_tx_filter_non_auth
		   : ol_tx_filter_discard);

	if (peer->vdev->pdev->cfg.host_addba) {
		if (state == ol_txrx_peer_state_auth) {
			int tid;
			/*
			 * Pause all regular (non-extended) TID tx queues until
			 * data arrives and ADDBA negotiation has completed.
			 */
			TXRX_PRINT(TXRX_PRINT_LEVEL_INFO2,
				   "%s: pause peer and unpause mgmt/non-qos\n",
				   __func__);
			ol_txrx_peer_pause(peer); /* pause all tx queues */
			/* unpause mgmt and non-QoS tx queues */
			for (tid = OL_TX_NUM_QOS_TIDS;
			     tid < OL_TX_NUM_TIDS; tid++)
				ol_txrx_peer_tid_unpause(peer, tid);
		}
	}
	cdf_atomic_dec(&peer->ref_cnt);

	/* Set the state after the Pause to avoid the race condiction
	   with ADDBA check in tx path */
	peer->state = state;
	return CDF_STATUS_SUCCESS;
}

void
ol_txrx_peer_keyinstalled_state_update(struct ol_txrx_peer_t *peer, uint8_t val)
{
	peer->keyinstalled = val;
}

void
ol_txrx_peer_update(ol_txrx_vdev_handle vdev,
		    uint8_t *peer_mac,
		    union ol_txrx_peer_update_param_t *param,
		    enum ol_txrx_peer_update_select_t select)
{
	struct ol_txrx_peer_t *peer;

	peer = ol_txrx_peer_find_hash_find(vdev->pdev, peer_mac, 0, 1);
	if (!peer) {
		TXRX_PRINT(TXRX_PRINT_LEVEL_INFO2, "%s: peer is null",
			   __func__);
		return;
	}

	switch (select) {
	case ol_txrx_peer_update_qos_capable:
	{
		/* save qos_capable here txrx peer,
		 * when HTT_ISOC_T2H_MSG_TYPE_PEER_INFO comes then save.
		 */
		peer->qos_capable = param->qos_capable;
		/*
		 * The following function call assumes that the peer has a
		 * single ID. This is currently true, and
		 * is expected to remain true.
		 */
		htt_peer_qos_update(peer->vdev->pdev->htt_pdev,
				    peer->peer_ids[0],
				    peer->qos_capable);
		break;
	}
	case ol_txrx_peer_update_uapsdMask:
	{
		peer->uapsd_mask = param->uapsd_mask;
		htt_peer_uapsdmask_update(peer->vdev->pdev->htt_pdev,
					  peer->peer_ids[0],
					  peer->uapsd_mask);
		break;
	}
	case ol_txrx_peer_update_peer_security:
	{
		enum ol_sec_type sec_type = param->sec_type;
		enum htt_sec_type peer_sec_type = htt_sec_type_none;

		switch (sec_type) {
		case ol_sec_type_none:
			peer_sec_type = htt_sec_type_none;
			break;
		case ol_sec_type_wep128:
			peer_sec_type = htt_sec_type_wep128;
			break;
		case ol_sec_type_wep104:
			peer_sec_type = htt_sec_type_wep104;
			break;
		case ol_sec_type_wep40:
			peer_sec_type = htt_sec_type_wep40;
			break;
		case ol_sec_type_tkip:
			peer_sec_type = htt_sec_type_tkip;
			break;
		case ol_sec_type_tkip_nomic:
			peer_sec_type = htt_sec_type_tkip_nomic;
			break;
		case ol_sec_type_aes_ccmp:
			peer_sec_type = htt_sec_type_aes_ccmp;
			break;
		case ol_sec_type_wapi:
			peer_sec_type = htt_sec_type_wapi;
			break;
		default:
			peer_sec_type = htt_sec_type_none;
			break;
		}

		peer->security[txrx_sec_ucast].sec_type =
			peer->security[txrx_sec_mcast].sec_type =
				peer_sec_type;

		break;
	}
	default:
	{
		CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
			  "ERROR: unknown param %d in %s", select,
			  __func__);
		break;
	}
	}
	cdf_atomic_dec(&peer->ref_cnt);
}

uint8_t
ol_txrx_peer_uapsdmask_get(struct ol_txrx_pdev_t *txrx_pdev, uint16_t peer_id)
{

	struct ol_txrx_peer_t *peer;
	peer = ol_txrx_peer_find_by_id(txrx_pdev, peer_id);
	if (peer)
		return peer->uapsd_mask;
	return 0;
}

uint8_t
ol_txrx_peer_qoscapable_get(struct ol_txrx_pdev_t *txrx_pdev, uint16_t peer_id)
{

	struct ol_txrx_peer_t *peer_t =
		ol_txrx_peer_find_by_id(txrx_pdev, peer_id);
	if (peer_t != NULL)
		return peer_t->qos_capable;
	return 0;
}

void ol_txrx_peer_unref_delete(ol_txrx_peer_handle peer)
{
	struct ol_txrx_vdev_t *vdev;
	struct ol_txrx_pdev_t *pdev;
	int i;

	/* preconditions */
	TXRX_ASSERT2(peer);

	vdev = peer->vdev;
	if (NULL == vdev) {
		TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1,
			   "The vdev is not present anymore\n");
		return;
	}

	pdev = vdev->pdev;
	if (NULL == pdev) {
		TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1,
			   "The pdev is not present anymore\n");
		return;
	}

	/*
	 * Check for the reference count before deleting the peer
	 * as we noticed that sometimes we are re-entering this
	 * function again which is leading to dead-lock.
	 * (A double-free should never happen, so assert if it does.)
	 */

	if (0 == cdf_atomic_read(&(peer->ref_cnt))) {
		TXRX_PRINT(TXRX_PRINT_LEVEL_ERR,
			   "The Peer is not present anymore\n");
		cdf_assert(0);
		return;
	}

	/*
	 * Hold the lock all the way from checking if the peer ref count
	 * is zero until the peer references are removed from the hash
	 * table and vdev list (if the peer ref count is zero).
	 * This protects against a new HL tx operation starting to use the
	 * peer object just after this function concludes it's done being used.
	 * Furthermore, the lock needs to be held while checking whether the
	 * vdev's list of peers is empty, to make sure that list is not modified
	 * concurrently with the empty check.
	 */
	cdf_spin_lock_bh(&pdev->peer_ref_mutex);
	if (cdf_atomic_dec_and_test(&peer->ref_cnt)) {
		u_int16_t peer_id;

		TXRX_PRINT(TXRX_PRINT_LEVEL_ERR,
			   "Deleting peer %p (%02x:%02x:%02x:%02x:%02x:%02x)\n",
			   peer,
			   peer->mac_addr.raw[0], peer->mac_addr.raw[1],
			   peer->mac_addr.raw[2], peer->mac_addr.raw[3],
			   peer->mac_addr.raw[4], peer->mac_addr.raw[5]);

		peer_id = peer->local_id;
		/* remove the reference to the peer from the hash table */
		ol_txrx_peer_find_hash_remove(pdev, peer);

		/* remove the peer from its parent vdev's list */
		TAILQ_REMOVE(&peer->vdev->peer_list, peer, peer_list_elem);

		/* cleanup the Rx reorder queues for this peer */
		ol_rx_peer_cleanup(vdev, peer);

		/* peer is removed from peer_list */
		cdf_atomic_set(&peer->delete_in_progress, 0);

		/*
		 * Set wait_delete_comp event if the current peer id matches
		 * with registered peer id.
		 */
		if (peer_id == vdev->wait_on_peer_id) {
			cdf_event_set(&vdev->wait_delete_comp);
			vdev->wait_on_peer_id = OL_TXRX_INVALID_LOCAL_PEER_ID;
		}

		/* check whether the parent vdev has no peers left */
		if (TAILQ_EMPTY(&vdev->peer_list)) {
			/*
			 * Now that there are no references to the peer, we can
			 * release the peer reference lock.
			 */
			cdf_spin_unlock_bh(&pdev->peer_ref_mutex);
			/*
			 * Check if the parent vdev was waiting for its peers
			 * to be deleted, in order for it to be deleted too.
			 */
			if (vdev->delete.pending) {
				ol_txrx_vdev_delete_cb vdev_delete_cb =
					vdev->delete.callback;
				void *vdev_delete_context =
					vdev->delete.context;

				TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1,
					   "%s: deleting vdev object %p "
					   "(%02x:%02x:%02x:%02x:%02x:%02x)"
					   " - its last peer is done\n",
					   __func__, vdev,
					   vdev->mac_addr.raw[0],
					   vdev->mac_addr.raw[1],
					   vdev->mac_addr.raw[2],
					   vdev->mac_addr.raw[3],
					   vdev->mac_addr.raw[4],
					   vdev->mac_addr.raw[5]);
				/* all peers are gone, go ahead and delete it */
				cdf_mem_free(vdev);
				if (vdev_delete_cb)
					vdev_delete_cb(vdev_delete_context);
			}
		} else
			cdf_spin_unlock_bh(&pdev->peer_ref_mutex);

		/*
		 * 'array' is allocated in addba handler and is supposed to be
		 * freed in delba handler. There is the case (for example, in
		 * SSR) where delba handler is not called. Because array points
		 * to address of 'base' by default and is reallocated in addba
		 * handler later, only free the memory when the array does not
		 * point to base.
		 */
		for (i = 0; i < OL_TXRX_NUM_EXT_TIDS; i++) {
			if (peer->tids_rx_reorder[i].array !=
			    &peer->tids_rx_reorder[i].base) {
				TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1,
					   "%s, delete reorder arr, tid:%d\n",
					   __func__, i);
				cdf_mem_free(peer->tids_rx_reorder[i].array);
				ol_rx_reorder_init(&peer->tids_rx_reorder[i],
						   (uint8_t) i);
			}
		}

		cdf_mem_free(peer);
	} else {
		cdf_spin_unlock_bh(&pdev->peer_ref_mutex);
	}
}

void ol_txrx_peer_detach(ol_txrx_peer_handle peer)
{
	struct ol_txrx_vdev_t *vdev = peer->vdev;

	/* redirect peer's rx delivery function to point to a discard func */
	peer->rx_opt_proc = ol_rx_discard;

	peer->valid = 0;

	ol_txrx_local_peer_id_free(peer->vdev->pdev, peer);

	/* debug print to dump rx reorder state */
	/* htt_rx_reorder_log_print(vdev->pdev->htt_pdev); */

	TXRX_PRINT(TXRX_PRINT_LEVEL_ERR,
		   "%s:peer %p (%02x:%02x:%02x:%02x:%02x:%02x)\n",
		   __func__, peer,
		   peer->mac_addr.raw[0], peer->mac_addr.raw[1],
		   peer->mac_addr.raw[2], peer->mac_addr.raw[3],
		   peer->mac_addr.raw[4], peer->mac_addr.raw[5]);
	ol_txrx_flush_rx_frames(peer, 1);

	if (peer->vdev->last_real_peer == peer)
		peer->vdev->last_real_peer = NULL;

	cdf_spin_lock_bh(&vdev->pdev->last_real_peer_mutex);
	if (vdev->last_real_peer == peer)
		vdev->last_real_peer = NULL;
	cdf_spin_unlock_bh(&vdev->pdev->last_real_peer_mutex);
	htt_rx_reorder_log_print(peer->vdev->pdev->htt_pdev);

	cdf_spinlock_destroy(&peer->peer_info_lock);
	cdf_spinlock_destroy(&peer->bufq_lock);
	/* set delete_in_progress to identify that wma
	 * is waiting for unmap massage for this peer */
	cdf_atomic_set(&peer->delete_in_progress, 1);
	/*
	 * Remove the reference added during peer_attach.
	 * The peer will still be left allocated until the
	 * PEER_UNMAP message arrives to remove the other
	 * reference, added by the PEER_MAP message.
	 */
	ol_txrx_peer_unref_delete(peer);
}

ol_txrx_peer_handle
ol_txrx_peer_find_by_addr(struct ol_txrx_pdev_t *pdev, uint8_t *peer_mac_addr)
{
	struct ol_txrx_peer_t *peer;
	peer = ol_txrx_peer_find_hash_find(pdev, peer_mac_addr, 0, 0);
	if (peer) {
		TXRX_PRINT(TXRX_PRINT_LEVEL_ERR,
			   "%s: Delete extra reference %p\n", __func__, peer);
		/* release the extra reference */
		ol_txrx_peer_unref_delete(peer);
	}
	return peer;
}

/**
 * ol_txrx_dump_tx_desc() - dump tx desc total and free count
 * @txrx_pdev: Pointer to txrx pdev
 *
 * Return: none
 */
static void ol_txrx_dump_tx_desc(ol_txrx_pdev_handle pdev_handle)
{
	struct ol_txrx_pdev_t *pdev = (ol_txrx_pdev_handle) pdev_handle;
	uint32_t total;

	total = ol_tx_get_desc_global_pool_size(pdev);

	TXRX_PRINT(TXRX_PRINT_LEVEL_ERR,
		"total tx credit %d num_free %d",
		total, pdev->tx_desc.num_free);

	return;
}

/**
 * ol_txrx_wait_for_pending_tx() - wait for tx queue to be empty
 * @timeout: timeout in ms
 *
 * Wait for tx queue to be empty, return timeout error if
 * queue doesn't empty before timeout occurs.
 *
 * Return:
 *    CDF_STATUS_SUCCESS if the queue empties,
 *    CDF_STATUS_E_TIMEOUT in case of timeout,
 *    CDF_STATUS_E_FAULT in case of missing handle
 */
CDF_STATUS ol_txrx_wait_for_pending_tx(int timeout)
{
	ol_txrx_pdev_handle txrx_pdev = cds_get_context(CDF_MODULE_ID_TXRX);

	if (txrx_pdev == NULL) {
		TXRX_PRINT(TXRX_PRINT_LEVEL_ERR,
			   "%s: txrx context is null", __func__);
		return CDF_STATUS_E_FAULT;
	}

	while (ol_txrx_get_tx_pending(txrx_pdev)) {
		cdf_sleep(OL_ATH_TX_DRAIN_WAIT_DELAY);
		if (timeout <= 0) {
			TXRX_PRINT(TXRX_PRINT_LEVEL_ERR,
				"%s: tx frames are pending", __func__);
			ol_txrx_dump_tx_desc(txrx_pdev);
			return CDF_STATUS_E_TIMEOUT;
		}
		timeout = timeout - OL_ATH_TX_DRAIN_WAIT_DELAY;
	}
	return CDF_STATUS_SUCCESS;
}

#ifndef QCA_WIFI_3_0_EMU
#define SUSPEND_DRAIN_WAIT 500
#else
#define SUSPEND_DRAIN_WAIT 3000
#endif

/**
 * ol_txrx_bus_suspend() - bus suspend
 *
 * Ensure that ol_txrx is ready for bus suspend
 *
 * Return: CDF_STATUS
 */
CDF_STATUS ol_txrx_bus_suspend(void)
{
	return ol_txrx_wait_for_pending_tx(SUSPEND_DRAIN_WAIT);
}

/**
 * ol_txrx_bus_resume() - bus resume
 *
 * Dummy function for symetry
 *
 * Return: CDF_STATUS_SUCCESS
 */
CDF_STATUS ol_txrx_bus_resume(void)
{
	return CDF_STATUS_SUCCESS;
}

int ol_txrx_get_tx_pending(ol_txrx_pdev_handle pdev_handle)
{
	struct ol_txrx_pdev_t *pdev = (ol_txrx_pdev_handle) pdev_handle;
	uint32_t total;

	total = ol_tx_get_desc_global_pool_size(pdev);

	return total - pdev->tx_desc.num_free;
}

void ol_txrx_discard_tx_pending(ol_txrx_pdev_handle pdev_handle)
{
	ol_tx_desc_list tx_descs;
	/* First let hif do the cdf_atomic_dec_and_test(&tx_desc->ref_cnt)
	 * then let htt do the cdf_atomic_dec_and_test(&tx_desc->ref_cnt)
	 * which is tha same with normal data send complete path*/
	htt_tx_pending_discard(pdev_handle->htt_pdev);

	TAILQ_INIT(&tx_descs);
	ol_tx_queue_discard(pdev_handle, true, &tx_descs);
	/* Discard Frames in Discard List */
	ol_tx_desc_frame_list_free(pdev_handle, &tx_descs, 1 /* error */);

	ol_tx_discard_target_frms(pdev_handle);
}

/*--- debug features --------------------------------------------------------*/

unsigned g_txrx_print_level = TXRX_PRINT_LEVEL_ERR;     /* default */

void ol_txrx_print_level_set(unsigned level)
{
#ifndef TXRX_PRINT_ENABLE
	CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_FATAL,
		  "The driver is compiled without TXRX prints enabled.\n"
		  "To enable them, recompile with TXRX_PRINT_ENABLE defined");
#else
	CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_INFO,
		  "TXRX printout level changed from %d to %d",
		  g_txrx_print_level, level);
	g_txrx_print_level = level;
#endif
}

struct ol_txrx_stats_req_internal {
	struct ol_txrx_stats_req base;
	int serviced;           /* state of this request */
	int offset;
};

static inline
uint64_t ol_txrx_stats_ptr_to_u64(struct ol_txrx_stats_req_internal *req)
{
	return (uint64_t) ((size_t) req);
}

static inline
struct ol_txrx_stats_req_internal *ol_txrx_u64_to_stats_ptr(uint64_t cookie)
{
	return (struct ol_txrx_stats_req_internal *)((size_t) cookie);
}

#ifdef ATH_PERF_PWR_OFFLOAD
void
ol_txrx_fw_stats_cfg(ol_txrx_vdev_handle vdev,
		     uint8_t cfg_stats_type, uint32_t cfg_val)
{
	uint64_t dummy_cookie = 0;
	htt_h2t_dbg_stats_get(vdev->pdev->htt_pdev, 0 /* upload mask */,
			      0 /* reset mask */,
			      cfg_stats_type, cfg_val, dummy_cookie);
}

A_STATUS
ol_txrx_fw_stats_get(ol_txrx_vdev_handle vdev, struct ol_txrx_stats_req *req)
{
	struct ol_txrx_pdev_t *pdev = vdev->pdev;
	uint64_t cookie;
	struct ol_txrx_stats_req_internal *non_volatile_req;

	if (!pdev ||
	    req->stats_type_upload_mask >= 1 << HTT_DBG_NUM_STATS ||
	    req->stats_type_reset_mask >= 1 << HTT_DBG_NUM_STATS) {
		return A_ERROR;
	}

	/*
	 * Allocate a non-transient stats request object.
	 * (The one provided as an argument is likely allocated on the stack.)
	 */
	non_volatile_req = cdf_mem_malloc(sizeof(*non_volatile_req));
	if (!non_volatile_req)
		return A_NO_MEMORY;

	/* copy the caller's specifications */
	non_volatile_req->base = *req;
	non_volatile_req->serviced = 0;
	non_volatile_req->offset = 0;

	/* use the non-volatile request object's address as the cookie */
	cookie = ol_txrx_stats_ptr_to_u64(non_volatile_req);

	if (htt_h2t_dbg_stats_get(pdev->htt_pdev,
				  req->stats_type_upload_mask,
				  req->stats_type_reset_mask,
				  HTT_H2T_STATS_REQ_CFG_STAT_TYPE_INVALID, 0,
				  cookie)) {
		cdf_mem_free(non_volatile_req);
		return A_ERROR;
	}

	if (req->wait.blocking)
		while (cdf_semaphore_acquire(pdev->osdev, req->wait.sem_ptr))
			;

	return A_OK;
}
#endif
void
ol_txrx_fw_stats_handler(ol_txrx_pdev_handle pdev,
			 uint64_t cookie, uint8_t *stats_info_list)
{
	enum htt_dbg_stats_type type;
	enum htt_dbg_stats_status status;
	int length;
	uint8_t *stats_data;
	struct ol_txrx_stats_req_internal *req;
	int more = 0;

	req = ol_txrx_u64_to_stats_ptr(cookie);

	do {
		htt_t2h_dbg_stats_hdr_parse(stats_info_list, &type, &status,
					    &length, &stats_data);
		if (status == HTT_DBG_STATS_STATUS_SERIES_DONE)
			break;
		if (status == HTT_DBG_STATS_STATUS_PRESENT ||
		    status == HTT_DBG_STATS_STATUS_PARTIAL) {
			uint8_t *buf;
			int bytes = 0;

			if (status == HTT_DBG_STATS_STATUS_PARTIAL)
				more = 1;
			if (req->base.print.verbose || req->base.print.concise)
				/* provide the header along with the data */
				htt_t2h_stats_print(stats_info_list,
						    req->base.print.concise);

			switch (type) {
			case HTT_DBG_STATS_WAL_PDEV_TXRX:
				bytes = sizeof(struct wlan_dbg_stats);
				if (req->base.copy.buf) {
					int lmt;

					lmt = sizeof(struct wlan_dbg_stats);
					if (req->base.copy.byte_limit < lmt)
						lmt = req->base.copy.byte_limit;
					buf = req->base.copy.buf + req->offset;
					cdf_mem_copy(buf, stats_data, lmt);
				}
				break;
			case HTT_DBG_STATS_RX_REORDER:
				bytes = sizeof(struct rx_reorder_stats);
				if (req->base.copy.buf) {
					int lmt;

					lmt = sizeof(struct rx_reorder_stats);
					if (req->base.copy.byte_limit < lmt)
						lmt = req->base.copy.byte_limit;
					buf = req->base.copy.buf + req->offset;
					cdf_mem_copy(buf, stats_data, lmt);
				}
				break;
			case HTT_DBG_STATS_RX_RATE_INFO:
				bytes = sizeof(wlan_dbg_rx_rate_info_t);
				if (req->base.copy.buf) {
					int lmt;

					lmt = sizeof(wlan_dbg_rx_rate_info_t);
					if (req->base.copy.byte_limit < lmt)
						lmt = req->base.copy.byte_limit;
					buf = req->base.copy.buf + req->offset;
					cdf_mem_copy(buf, stats_data, lmt);
				}
				break;

			case HTT_DBG_STATS_TX_RATE_INFO:
				bytes = sizeof(wlan_dbg_tx_rate_info_t);
				if (req->base.copy.buf) {
					int lmt;

					lmt = sizeof(wlan_dbg_tx_rate_info_t);
					if (req->base.copy.byte_limit < lmt)
						lmt = req->base.copy.byte_limit;
					buf = req->base.copy.buf + req->offset;
					cdf_mem_copy(buf, stats_data, lmt);
				}
				break;

			case HTT_DBG_STATS_TX_PPDU_LOG:
				bytes = 0;
				/* TO DO: specify how many bytes are present */
				/* TO DO: add copying to the requestor's buf */

			case HTT_DBG_STATS_RX_REMOTE_RING_BUFFER_INFO:
				bytes = sizeof(struct rx_remote_buffer_mgmt_stats);
				if (req->base.copy.buf) {
					int limit;

					limit = sizeof(struct rx_remote_buffer_mgmt_stats);
					if (req->base.copy.byte_limit < limit) {
						limit = req->base.copy.byte_limit;
					}
					buf = req->base.copy.buf + req->offset;
					cdf_mem_copy(buf, stats_data, limit);
				}
				break;

			case HTT_DBG_STATS_TXBF_INFO:
				bytes = sizeof(struct wlan_dbg_txbf_data_stats);
				if (req->base.copy.buf) {
					int limit;

					limit = sizeof(struct wlan_dbg_txbf_data_stats);
					if (req->base.copy.byte_limit < limit)
						limit = req->base.copy.byte_limit;
					buf = req->base.copy.buf + req->offset;
					cdf_mem_copy(buf, stats_data, limit);
				}
				break;

			case HTT_DBG_STATS_SND_INFO:
				bytes = sizeof(struct wlan_dbg_txbf_snd_stats);
				if (req->base.copy.buf) {
					int limit;

					limit = sizeof(struct wlan_dbg_txbf_snd_stats);
					if (req->base.copy.byte_limit < limit)
						limit = req->base.copy.byte_limit;
					buf = req->base.copy.buf + req->offset;
					cdf_mem_copy(buf, stats_data, limit);
				}
				break;

			case HTT_DBG_STATS_TX_SELFGEN_INFO:
				bytes = sizeof(struct wlan_dbg_tx_selfgen_stats);
				if (req->base.copy.buf) {
					int limit;

					limit = sizeof(struct wlan_dbg_tx_selfgen_stats);
					if (req->base.copy.byte_limit < limit)
						limit = req->base.copy.byte_limit;
					buf = req->base.copy.buf + req->offset;
					cdf_mem_copy(buf, stats_data, limit);
				}
				break;

			case HTT_DBG_STATS_ERROR_INFO:
				bytes =
				  sizeof(struct wlan_dbg_wifi2_error_stats);
				if (req->base.copy.buf) {
					int limit;

					limit =
					sizeof(struct wlan_dbg_wifi2_error_stats);
					if (req->base.copy.byte_limit < limit)
						limit = req->base.copy.byte_limit;
					buf = req->base.copy.buf + req->offset;
					cdf_mem_copy(buf, stats_data, limit);
				}
				break;

			case HTT_DBG_STATS_TXBF_MUSU_NDPA_PKT:
				bytes =
				  sizeof(struct rx_txbf_musu_ndpa_pkts_stats);
				if (req->base.copy.buf) {
					int limit;

					limit = sizeof(struct
						rx_txbf_musu_ndpa_pkts_stats);
					if (req->base.copy.byte_limit <	limit)
						limit =
						req->base.copy.byte_limit;
					buf = req->base.copy.buf + req->offset;
					cdf_mem_copy(buf, stats_data, limit);
				}
				break;

			default:
				break;
			}
			buf = req->base.copy.buf
				? req->base.copy.buf
				: stats_data;
			if (req->base.callback.fp)
				req->base.callback.fp(req->base.callback.ctxt,
						      type, buf, bytes);
		}
		stats_info_list += length;
	} while (1);

	if (!more) {
		if (req->base.wait.blocking)
			cdf_semaphore_release(pdev->osdev,
					      req->base.wait.sem_ptr);
		cdf_mem_free(req);
	}
}

#ifndef ATH_PERF_PWR_OFFLOAD /*---------------------------------------------*/
int ol_txrx_debug(ol_txrx_vdev_handle vdev, int debug_specs)
{
	if (debug_specs & TXRX_DBG_MASK_OBJS) {
#if defined(TXRX_DEBUG_LEVEL) && TXRX_DEBUG_LEVEL > 5
		ol_txrx_pdev_display(vdev->pdev, 0);
#else
		CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_FATAL,
			  "The pdev,vdev,peer display functions are disabled.\n"
			  "To enable them, recompile with TXRX_DEBUG_LEVEL > 5");
#endif
	}
	if (debug_specs & TXRX_DBG_MASK_STATS) {
		ol_txrx_stats_display(vdev->pdev);
	}
	if (debug_specs & TXRX_DBG_MASK_PROT_ANALYZE) {
#if defined(ENABLE_TXRX_PROT_ANALYZE)
		ol_txrx_prot_ans_display(vdev->pdev);
#else
		CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_FATAL,
			  "txrx protocol analysis is disabled.\n"
			  "To enable it, recompile with "
			  "ENABLE_TXRX_PROT_ANALYZE defined");
#endif
	}
	if (debug_specs & TXRX_DBG_MASK_RX_REORDER_TRACE) {
#if defined(ENABLE_RX_REORDER_TRACE)
		ol_rx_reorder_trace_display(vdev->pdev, 0, 0);
#else
		CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_FATAL,
			  "rx reorder seq num trace is disabled.\n"
			  "To enable it, recompile with "
			  "ENABLE_RX_REORDER_TRACE defined");
#endif

	}
	return 0;
}
#endif

int ol_txrx_aggr_cfg(ol_txrx_vdev_handle vdev,
		     int max_subfrms_ampdu, int max_subfrms_amsdu)
{
	return htt_h2t_aggr_cfg_msg(vdev->pdev->htt_pdev,
				    max_subfrms_ampdu, max_subfrms_amsdu);
}

#if defined(TXRX_DEBUG_LEVEL) && TXRX_DEBUG_LEVEL > 5
void ol_txrx_pdev_display(ol_txrx_pdev_handle pdev, int indent)
{
	struct ol_txrx_vdev_t *vdev;

	CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_INFO_LOW,
		  "%*s%s:\n", indent, " ", "txrx pdev");
	CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_INFO_LOW,
		  "%*spdev object: %p", indent + 4, " ", pdev);
	CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_INFO_LOW,
		  "%*svdev list:", indent + 4, " ");
	TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
		ol_txrx_vdev_display(vdev, indent + 8);
	}
	ol_txrx_peer_find_display(pdev, indent + 4);
	CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_INFO_LOW,
		  "%*stx desc pool: %d elems @ %p", indent + 4, " ",
		  pdev->tx_desc.pool_size, pdev->tx_desc.array);
	CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_INFO_LOW, " ");
	htt_display(pdev->htt_pdev, indent);
}

void ol_txrx_vdev_display(ol_txrx_vdev_handle vdev, int indent)
{
	struct ol_txrx_peer_t *peer;

	CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_INFO_LOW,
		  "%*stxrx vdev: %p\n", indent, " ", vdev);
	CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_INFO_LOW,
		  "%*sID: %d\n", indent + 4, " ", vdev->vdev_id);
	CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_INFO_LOW,
		  "%*sMAC addr: %d:%d:%d:%d:%d:%d",
		  indent + 4, " ",
		  vdev->mac_addr.raw[0], vdev->mac_addr.raw[1],
		  vdev->mac_addr.raw[2], vdev->mac_addr.raw[3],
		  vdev->mac_addr.raw[4], vdev->mac_addr.raw[5]);
	CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_INFO_LOW,
		  "%*speer list:", indent + 4, " ");
	TAILQ_FOREACH(peer, &vdev->peer_list, peer_list_elem) {
		ol_txrx_peer_display(peer, indent + 8);
	}
}

void ol_txrx_peer_display(ol_txrx_peer_handle peer, int indent)
{
	int i;

	CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_INFO_LOW,
		  "%*stxrx peer: %p", indent, " ", peer);
	for (i = 0; i < MAX_NUM_PEER_ID_PER_PEER; i++) {
		if (peer->peer_ids[i] != HTT_INVALID_PEER) {
			CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_INFO_LOW,
				  "%*sID: %d", indent + 4, " ",
				  peer->peer_ids[i]);
		}
	}
}
#endif /* TXRX_DEBUG_LEVEL */

#if defined(FEATURE_TSO) && defined(FEATURE_TSO_DEBUG)
void ol_txrx_stats_display_tso(ol_txrx_pdev_handle pdev)
{
	int msdu_idx;
	int seg_idx;

	CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
		"TSO pkts %lld, bytes %lld\n",
		pdev->stats.pub.tx.tso.tso_pkts.pkts,
		pdev->stats.pub.tx.tso.tso_pkts.bytes);

	for (msdu_idx = 0; msdu_idx < NUM_MAX_TSO_MSDUS; msdu_idx++) {
		CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
			"curr msdu idx: %d curr seg idx: %d num segs %d\n",
			TXRX_STATS_TSO_MSDU_IDX(pdev),
			TXRX_STATS_TSO_SEG_IDX(pdev),
			TXRX_STATS_TSO_MSDU_NUM_SEG(pdev, msdu_idx));
		for (seg_idx = 0;
			 ((seg_idx < TXRX_STATS_TSO_MSDU_NUM_SEG(pdev, msdu_idx)) &&
			  (seg_idx < NUM_MAX_TSO_SEGS));
			 seg_idx++) {
			struct cdf_tso_seg_t tso_seg =
				 TXRX_STATS_TSO_SEG(pdev, msdu_idx, seg_idx);

			CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
				 "msdu idx: %d seg idx: %d\n",
				 msdu_idx, seg_idx);
			CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
				 "tso_enable: %d\n",
				 tso_seg.tso_flags.tso_enable);
			CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
				 "fin %d syn %d rst %d psh %d ack %d\n"
				 "urg %d ece %d cwr %d ns %d\n",
				 tso_seg.tso_flags.fin, tso_seg.tso_flags.syn,
				 tso_seg.tso_flags.rst, tso_seg.tso_flags.psh,
				 tso_seg.tso_flags.ack, tso_seg.tso_flags.urg,
				 tso_seg.tso_flags.ece, tso_seg.tso_flags.cwr,
				 tso_seg.tso_flags.ns);
			CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
				 "tcp_seq_num: 0x%x ip_id: %d\n",
				 tso_seg.tso_flags.tcp_seq_num,
				 tso_seg.tso_flags.ip_id);
		}
	}
}
#endif

/**
 * ol_txrx_stats() - update ol layer stats
 * @vdev_id: vdev_id
 * @buffer: pointer to buffer
 * @buf_len: length of the buffer
 *
 * Return: length of string
 */
int
ol_txrx_stats(uint8_t vdev_id, char *buffer, unsigned buf_len)
{
	uint32_t len = 0;

	ol_txrx_vdev_handle vdev = ol_txrx_get_vdev_from_vdev_id(vdev_id);
	if (!vdev) {
		CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
			"%s: vdev is NULL", __func__);
		snprintf(buffer, buf_len, "vdev not found");
		return len;
	}

	len = scnprintf(buffer, buf_len,
		"\nTXRX stats:\n"
		"\nllQueue State : %s"
		"\n pause %u unpause %u"
		"\n overflow %u"
		"\nllQueue timer state : %s\n",
		((vdev->ll_pause.is_q_paused == false) ? "UNPAUSED" : "PAUSED"),
		vdev->ll_pause.q_pause_cnt,
		vdev->ll_pause.q_unpause_cnt,
		vdev->ll_pause.q_overflow_cnt,
		((vdev->ll_pause.is_q_timer_on == false)
			? "NOT-RUNNING" : "RUNNING"));
	return len;
}

void ol_txrx_stats_display(ol_txrx_pdev_handle pdev)
{
	CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR, "txrx stats:");
	CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
		  "  tx: sent %lld msdus (%lld B), "
		  "      rejected %lld (%lld B), dropped %lld (%lld B)",
		  pdev->stats.pub.tx.delivered.pkts,
		  pdev->stats.pub.tx.delivered.bytes,
		  pdev->stats.pub.tx.dropped.host_reject.pkts,
		  pdev->stats.pub.tx.dropped.host_reject.bytes,
		  pdev->stats.pub.tx.dropped.download_fail.pkts
		  + pdev->stats.pub.tx.dropped.target_discard.pkts
		  + pdev->stats.pub.tx.dropped.no_ack.pkts,
		  pdev->stats.pub.tx.dropped.download_fail.bytes
		  + pdev->stats.pub.tx.dropped.target_discard.bytes
		  + pdev->stats.pub.tx.dropped.no_ack.bytes);
	CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
		  "    download fail: %lld (%lld B), "
		  "target discard: %lld (%lld B), "
		  "no ack: %lld (%lld B)",
		  pdev->stats.pub.tx.dropped.download_fail.pkts,
		  pdev->stats.pub.tx.dropped.download_fail.bytes,
		  pdev->stats.pub.tx.dropped.target_discard.pkts,
		  pdev->stats.pub.tx.dropped.target_discard.bytes,
		  pdev->stats.pub.tx.dropped.no_ack.pkts,
		  pdev->stats.pub.tx.dropped.no_ack.bytes);
	CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
		  "Tx completion per interrupt:\n"
		  "Single Packet  %d\n"
		  " 2-10 Packets  %d\n"
		  "11-20 Packets  %d\n"
		  "21-30 Packets  %d\n"
		  "31-40 Packets  %d\n"
		  "41-50 Packets  %d\n"
		  "51-60 Packets  %d\n"
		  "  60+ Packets  %d\n",
		  pdev->stats.pub.tx.comp_histogram.pkts_1,
		  pdev->stats.pub.tx.comp_histogram.pkts_2_10,
		  pdev->stats.pub.tx.comp_histogram.pkts_11_20,
		  pdev->stats.pub.tx.comp_histogram.pkts_21_30,
		  pdev->stats.pub.tx.comp_histogram.pkts_31_40,
		  pdev->stats.pub.tx.comp_histogram.pkts_41_50,
		  pdev->stats.pub.tx.comp_histogram.pkts_51_60,
		  pdev->stats.pub.tx.comp_histogram.pkts_61_plus);
	CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
		  "  rx: %lld ppdus, %lld mpdus, %lld msdus, %lld bytes, %lld errs",
		  pdev->stats.priv.rx.normal.ppdus,
		  pdev->stats.priv.rx.normal.mpdus,
		  pdev->stats.pub.rx.delivered.pkts,
		  pdev->stats.pub.rx.delivered.bytes,
		  pdev->stats.priv.rx.err.mpdu_bad);

	CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
		  "  fwd to stack %d, fwd to fw %d, fwd to stack & fw  %d\n",
		  pdev->stats.pub.rx.intra_bss_fwd.packets_stack,
		  pdev->stats.pub.rx.intra_bss_fwd.packets_fwd,
		  pdev->stats.pub.rx.intra_bss_fwd.packets_stack_n_fwd);
}

void ol_txrx_stats_clear(ol_txrx_pdev_handle pdev)
{
	cdf_mem_zero(&pdev->stats, sizeof(pdev->stats));
}

#if defined(ENABLE_TXRX_PROT_ANALYZE)

void ol_txrx_prot_ans_display(ol_txrx_pdev_handle pdev)
{
	ol_txrx_prot_an_display(pdev->prot_an_tx_sent);
	ol_txrx_prot_an_display(pdev->prot_an_rx_sent);
}

#endif /* ENABLE_TXRX_PROT_ANALYZE */

#ifdef QCA_SUPPORT_PEER_DATA_RX_RSSI
int16_t ol_txrx_peer_rssi(ol_txrx_peer_handle peer)
{
	return (peer->rssi_dbm == HTT_RSSI_INVALID) ?
	       OL_TXRX_RSSI_INVALID : peer->rssi_dbm;
}
#endif /* #ifdef QCA_SUPPORT_PEER_DATA_RX_RSSI */

#ifdef QCA_ENABLE_OL_TXRX_PEER_STATS
A_STATUS
ol_txrx_peer_stats_copy(ol_txrx_pdev_handle pdev,
			ol_txrx_peer_handle peer, ol_txrx_peer_stats_t *stats)
{
	cdf_assert(pdev && peer && stats);
	cdf_spin_lock_bh(&pdev->peer_stat_mutex);
	cdf_mem_copy(stats, &peer->stats, sizeof(*stats));
	cdf_spin_unlock_bh(&pdev->peer_stat_mutex);
	return A_OK;
}
#endif /* QCA_ENABLE_OL_TXRX_PEER_STATS */

void ol_vdev_rx_set_intrabss_fwd(ol_txrx_vdev_handle vdev, bool val)
{
	if (NULL == vdev)
		return;

	vdev->disable_intrabss_fwd = val;
}

#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL

/**
 * ol_txrx_get_vdev_from_sta_id() - get vdev from sta_id
 * @sta_id: sta_id
 *
 * Return: vdev handle
 *            NULL if not found.
 */
static ol_txrx_vdev_handle ol_txrx_get_vdev_from_sta_id(uint8_t sta_id)
{
	struct ol_txrx_peer_t *peer = NULL;
	ol_txrx_pdev_handle pdev = NULL;

	if (sta_id >= WLAN_MAX_STA_COUNT) {
		CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
			"Invalid sta id passed");
		return NULL;
	}

	pdev = cds_get_context(CDF_MODULE_ID_TXRX);
	if (!pdev) {
		CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
			"PDEV not found for sta_id [%d]", sta_id);
		return NULL;
	}

	peer = ol_txrx_peer_find_by_local_id(pdev, sta_id);

	if (!peer) {
		CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
			"PEER [%d] not found", sta_id);
		return NULL;
	}

	return peer->vdev;
}

/**
 * ol_txrx_register_tx_flow_control() - register tx flow control callback
 * @vdev_id: vdev_id
 * @flowControl: flow control callback
 * @osif_fc_ctx: callback context
 *
 * Return: 0 for sucess or error code
 */
int ol_txrx_register_tx_flow_control (uint8_t vdev_id,
	ol_txrx_tx_flow_control_fp flowControl,
	void *osif_fc_ctx)
{
	ol_txrx_vdev_handle vdev = ol_txrx_get_vdev_from_vdev_id(vdev_id);
	if (NULL == vdev) {
		CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
			"%s: Invalid vdev_id %d", __func__, vdev_id);
		return -EINVAL;
	}

	cdf_spin_lock_bh(&vdev->flow_control_lock);
	vdev->osif_flow_control_cb = flowControl;
	vdev->osif_fc_ctx = osif_fc_ctx;
	cdf_spin_unlock_bh(&vdev->flow_control_lock);
	return 0;
}

/**
 * ol_txrx_de_register_tx_flow_control_cb() - deregister tx flow control callback
 * @vdev_id: vdev_id
 *
 * Return: 0 for success or error code
 */
int ol_txrx_deregister_tx_flow_control_cb(uint8_t vdev_id)
{
	ol_txrx_vdev_handle vdev = ol_txrx_get_vdev_from_vdev_id(vdev_id);
	if (NULL == vdev) {
		CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
				"%s: Invalid vdev_id", __func__);
		return -EINVAL;
	}

	cdf_spin_lock_bh(&vdev->flow_control_lock);
	vdev->osif_flow_control_cb = NULL;
	vdev->osif_fc_ctx = NULL;
	cdf_spin_unlock_bh(&vdev->flow_control_lock);
	return 0;
}

/**
 * ol_txrx_get_tx_resource() - if tx resource less than low_watermark
 * @sta_id: sta id
 * @low_watermark: low watermark
 * @high_watermark_offset: high watermark offset value
 *
 * Return: true/false
 */
bool
ol_txrx_get_tx_resource(uint8_t sta_id,
			unsigned int low_watermark,
			unsigned int high_watermark_offset)
{
	ol_txrx_vdev_handle vdev = ol_txrx_get_vdev_from_sta_id(sta_id);
	if (NULL == vdev) {
		CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
			"%s: Invalid sta_id %d", __func__, sta_id);
		/* Return true so caller do not understand that resource
		 * is less than low_watermark.
		 * sta_id validation will be done in ol_tx_send_data_frame
		 * and if sta_id is not registered then host will drop
		 * packet.
		 */
		return true;
	}

	cdf_spin_lock_bh(&vdev->pdev->tx_mutex);
	if (vdev->pdev->tx_desc.num_free < (uint16_t) low_watermark) {
		vdev->tx_fl_lwm = (uint16_t) low_watermark;
		vdev->tx_fl_hwm =
			(uint16_t) (low_watermark + high_watermark_offset);
		/* Not enough free resource, stop TX OS Q */
		cdf_atomic_set(&vdev->os_q_paused, 1);
		cdf_spin_unlock_bh(&vdev->pdev->tx_mutex);
		return false;
	}
	cdf_spin_unlock_bh(&vdev->pdev->tx_mutex);
	return true;
}

/**
 * ol_txrx_ll_set_tx_pause_q_depth() - set pause queue depth
 * @vdev_id: vdev id
 * @pause_q_depth: pause queue depth
 *
 * Return: 0 for success or error code
 */
int
ol_txrx_ll_set_tx_pause_q_depth(uint8_t vdev_id, int pause_q_depth)
{
	ol_txrx_vdev_handle vdev = ol_txrx_get_vdev_from_vdev_id(vdev_id);
	if (NULL == vdev) {
		CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
			"%s: Invalid vdev_id %d", __func__, vdev_id);
		return -EINVAL;
	}

	cdf_spin_lock_bh(&vdev->ll_pause.mutex);
	vdev->ll_pause.max_q_depth = pause_q_depth;
	cdf_spin_unlock_bh(&vdev->ll_pause.mutex);

	return 0;
}

/**
 * ol_txrx_flow_control_cb() - call osif flow control callback
 * @vdev: vdev handle
 * @tx_resume: tx resume flag
 *
 * Return: none
 */
inline void ol_txrx_flow_control_cb(ol_txrx_vdev_handle vdev,
	bool tx_resume)
{
	cdf_spin_lock_bh(&vdev->flow_control_lock);
	if ((vdev->osif_flow_control_cb) && (vdev->osif_fc_ctx))
		vdev->osif_flow_control_cb(vdev->osif_fc_ctx, tx_resume);
	cdf_spin_unlock_bh(&vdev->flow_control_lock);

	return;
}
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL */

#ifdef IPA_OFFLOAD
/**
 * ol_txrx_ipa_uc_get_resource() - Client request resource information
 * @pdev: handle to the HTT instance
 * @ce_sr_base_paddr: copy engine source ring base physical address
 * @ce_sr_ring_size: copy engine source ring size
 * @ce_reg_paddr: copy engine register physical address
 * @tx_comp_ring_base_paddr: tx comp ring base physical address
 * @tx_comp_ring_size: tx comp ring size
 * @tx_num_alloc_buffer: number of allocated tx buffer
 * @rx_rdy_ring_base_paddr: rx ready ring base physical address
 * @rx_rdy_ring_size: rx ready ring size
 * @rx_proc_done_idx_paddr: rx process done index physical address
 * @rx_proc_done_idx_vaddr: rx process done index virtual address
 * @rx2_rdy_ring_base_paddr: rx done ring base physical address
 * @rx2_rdy_ring_size: rx done ring size
 * @rx2_proc_done_idx_paddr: rx done index physical address
 * @rx2_proc_done_idx_vaddr: rx done index virtual address
 *
 *  OL client will reuqest IPA UC related resource information
 *  Resource information will be distributted to IPA module
 *  All of the required resources should be pre-allocated
 *
 * Return: none
 */
void
ol_txrx_ipa_uc_get_resource(ol_txrx_pdev_handle pdev,
			    cdf_dma_addr_t *ce_sr_base_paddr,
			    uint32_t *ce_sr_ring_size,
			    cdf_dma_addr_t *ce_reg_paddr,
			    cdf_dma_addr_t *tx_comp_ring_base_paddr,
			    uint32_t *tx_comp_ring_size,
			    uint32_t *tx_num_alloc_buffer,
			    cdf_dma_addr_t *rx_rdy_ring_base_paddr,
			    uint32_t *rx_rdy_ring_size,
			    cdf_dma_addr_t *rx_proc_done_idx_paddr,
			    void **rx_proc_done_idx_vaddr,
			    cdf_dma_addr_t *rx2_rdy_ring_base_paddr,
			    uint32_t *rx2_rdy_ring_size,
			    cdf_dma_addr_t *rx2_proc_done_idx2_paddr,
			    void **rx2_proc_done_idx2_vaddr)
{
	htt_ipa_uc_get_resource(pdev->htt_pdev,
				ce_sr_base_paddr,
				ce_sr_ring_size,
				ce_reg_paddr,
				tx_comp_ring_base_paddr,
				tx_comp_ring_size,
				tx_num_alloc_buffer,
				rx_rdy_ring_base_paddr,
				rx_rdy_ring_size, rx_proc_done_idx_paddr,
				rx_proc_done_idx_vaddr,
				rx2_rdy_ring_base_paddr,
				rx2_rdy_ring_size, rx2_proc_done_idx2_paddr,
				rx2_proc_done_idx2_vaddr);
}

/**
 * ol_txrx_ipa_uc_set_doorbell_paddr() - Client set IPA UC doorbell register
 * @pdev: handle to the HTT instance
 * @ipa_uc_tx_doorbell_paddr: tx comp doorbell physical address
 * @ipa_uc_rx_doorbell_paddr: rx ready doorbell physical address
 *
 *  IPA UC let know doorbell register physical address
 *  WLAN firmware will use this physical address to notify IPA UC
 *
 * Return: none
 */
void
ol_txrx_ipa_uc_set_doorbell_paddr(ol_txrx_pdev_handle pdev,
				  cdf_dma_addr_t ipa_tx_uc_doorbell_paddr,
				  cdf_dma_addr_t ipa_rx_uc_doorbell_paddr)
{
	htt_ipa_uc_set_doorbell_paddr(pdev->htt_pdev,
				      ipa_tx_uc_doorbell_paddr,
				      ipa_rx_uc_doorbell_paddr);
}

/**
 * ol_txrx_ipa_uc_set_active() - Client notify IPA UC data path active or not
 * @pdev: handle to the HTT instance
 * @ipa_uc_tx_doorbell_paddr: tx comp doorbell physical address
 * @ipa_uc_rx_doorbell_paddr: rx ready doorbell physical address
 *
 *  IPA UC let know doorbell register physical address
 *  WLAN firmware will use this physical address to notify IPA UC
 *
 * Return: none
 */
void
ol_txrx_ipa_uc_set_active(ol_txrx_pdev_handle pdev, bool uc_active, bool is_tx)
{
	htt_h2t_ipa_uc_set_active(pdev->htt_pdev, uc_active, is_tx);
}

/**
 * ol_txrx_ipa_uc_fw_op_event_handler() - opcode event handler
 * @context: pdev context
 * @rxpkt: received packet
 * @staid: peer id
 *
 * Return: None
 */
void ol_txrx_ipa_uc_fw_op_event_handler(void *context,
					void *rxpkt,
					uint16_t staid)
{
	ol_txrx_pdev_handle pdev = (ol_txrx_pdev_handle)context;

	if (cdf_unlikely(!pdev)) {
		CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
			      "%s: Invalid context", __func__);
		return;
	}

	if (pdev->ipa_uc_op_cb)
		pdev->ipa_uc_op_cb(rxpkt, pdev->osif_dev);
	else
		CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
			      "%s: ipa_uc_op_cb NULL", __func__);
}

/**
 * ol_txrx_ipa_uc_op_response() - Handle OP command response from firmware
 * @pdev: handle to the HTT instance
 * @op_msg: op response message from firmware
 *
 * Return: none
 */
void ol_txrx_ipa_uc_op_response(ol_txrx_pdev_handle pdev, uint8_t *op_msg)
{
	p_cds_sched_context sched_ctx = get_cds_sched_ctxt();
	struct cds_ol_rx_pkt *pkt;

	if (cdf_unlikely(!sched_ctx))
		return;

	pkt = cds_alloc_ol_rx_pkt(sched_ctx);
	if (cdf_unlikely(!pkt)) {
		CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
			      "%s: Not able to allocate context", __func__);
		return;
	}

	pkt->callback = (cds_ol_rx_thread_cb) ol_txrx_ipa_uc_fw_op_event_handler;
	pkt->context = pdev;
	pkt->Rxpkt = (void *)op_msg;
	pkt->staId = 0;
	cds_indicate_rxpkt(sched_ctx, pkt);
}

/**
 * ol_txrx_ipa_uc_register_op_cb() - Register OP handler function
 * @pdev: handle to the HTT instance
 * @op_cb: handler function pointer
 * @osif_dev: register client context
 *
 * Return: none
 */
void ol_txrx_ipa_uc_register_op_cb(ol_txrx_pdev_handle pdev,
				   ipa_uc_op_cb_type op_cb, void *osif_dev)
{
	pdev->ipa_uc_op_cb = op_cb;
	pdev->osif_dev = osif_dev;
}

/**
 * ol_txrx_ipa_uc_get_stat() - Get firmware wdi status
 * @pdev: handle to the HTT instance
 *
 * Return: none
 */
void ol_txrx_ipa_uc_get_stat(ol_txrx_pdev_handle pdev)
{
	htt_h2t_ipa_uc_get_stats(pdev->htt_pdev);
}
#endif /* IPA_UC_OFFLOAD */

void ol_txrx_display_stats(uint16_t value)
{
	ol_txrx_pdev_handle pdev;

	pdev = cds_get_context(CDF_MODULE_ID_TXRX);
	if (!pdev) {
		CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
			"%s: pdev is NULL", __func__);
		return;
	}

	switch (value) {
	case WLAN_TXRX_STATS:
		ol_txrx_stats_display(pdev);
		break;
	case WLAN_TXRX_TSO_STATS:
#if defined(FEATURE_TSO) && defined(FEATURE_TSO_DEBUG)
		ol_txrx_stats_display_tso(pdev);
#else
		CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
					"%s: TSO not supported", __func__);
#endif
		break;
	case WLAN_DUMP_TX_FLOW_POOL_INFO:
		ol_tx_dump_flow_pool_info();
		break;
	case WLAN_TXRX_DESC_STATS:
		cdf_nbuf_tx_desc_count_display();
		break;
	default:
		CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
					"%s: Unknown value", __func__);
		break;
	}
}

void ol_txrx_clear_stats(uint16_t value)
{
	ol_txrx_pdev_handle pdev;

	pdev = cds_get_context(CDF_MODULE_ID_TXRX);
	if (!pdev) {
		CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
			"%s: pdev is NULL", __func__);
		return;
	}

	switch (value) {
	case WLAN_TXRX_STATS:
		ol_txrx_stats_clear(pdev);
		break;
	case WLAN_DUMP_TX_FLOW_POOL_INFO:
		ol_tx_clear_flow_pool_stats();
		break;
	case WLAN_TXRX_DESC_STATS:
		cdf_nbuf_tx_desc_count_clear();
		break;
	default:
		CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
					"%s: Unknown value", __func__);
		break;
	}
}

/**
 * ol_rx_data_cb() - data rx callback
 * @peer: peer
 * @buf_list: buffer list
 *
 * Return: None
 */
static void ol_rx_data_cb(struct ol_txrx_peer_t *peer,
			  cdf_nbuf_t buf_list)
{
	void *cds_ctx = cds_get_global_context();
	cdf_nbuf_t buf, next_buf;
	CDF_STATUS ret;
	ol_rx_callback_fp data_rx = NULL;

	if (cdf_unlikely(!cds_ctx))
		goto free_buf;

	cdf_spin_lock_bh(&peer->peer_info_lock);
	if (cdf_unlikely(!(peer->state >= ol_txrx_peer_state_conn))) {
		cdf_spin_unlock_bh(&peer->peer_info_lock);
		goto free_buf;
	}
	data_rx = peer->osif_rx;
	cdf_spin_unlock_bh(&peer->peer_info_lock);

	cdf_spin_lock_bh(&peer->bufq_lock);
	if (!list_empty(&peer->cached_bufq)) {
		cdf_spin_unlock_bh(&peer->bufq_lock);
		/* Flush the cached frames to HDD before passing new rx frame */
		ol_txrx_flush_rx_frames(peer, 0);
	} else
		cdf_spin_unlock_bh(&peer->bufq_lock);

	buf = buf_list;
	while (buf) {
		next_buf = cdf_nbuf_queue_next(buf);
		cdf_nbuf_set_next(buf, NULL);   /* Add NULL terminator */
		ret = data_rx(cds_ctx, buf, peer->local_id);
		if (ret != CDF_STATUS_SUCCESS) {
			TXRX_PRINT(TXRX_PRINT_LEVEL_ERR, "Frame Rx to HDD failed");
			cdf_nbuf_free(buf);
		}
		buf = next_buf;
	}
	return;

free_buf:
	TXRX_PRINT(TXRX_PRINT_LEVEL_WARN, "%s:Dropping frames", __func__);
	buf = buf_list;
	while (buf) {
		next_buf = cdf_nbuf_queue_next(buf);
		cdf_nbuf_free(buf);
		buf = next_buf;
	}
}

/**
 * ol_rx_data_process() - process rx frame
 * @peer: peer
 * @rx_buf_list: rx buffer list
 *
 * Return: None
 */
void ol_rx_data_process(struct ol_txrx_peer_t *peer,
			cdf_nbuf_t rx_buf_list)
{
	/* Firmware data path active response will use shim RX thread
	 * T2H MSG running on SIRQ context,
	 * IPA kernel module API should not be called on SIRQ CTXT */
	cdf_nbuf_t buf, next_buf;
	ol_rx_callback_fp data_rx = NULL;
	ol_txrx_pdev_handle pdev = cds_get_context(CDF_MODULE_ID_TXRX);

	if ((!peer) || (!pdev)) {
		TXRX_PRINT(TXRX_PRINT_LEVEL_ERR, "peer/pdev is NULL");
		goto drop_rx_buf;
	}

	cdf_spin_lock_bh(&peer->peer_info_lock);
	if (peer->state >= ol_txrx_peer_state_conn)
		data_rx = peer->osif_rx;
	cdf_spin_unlock_bh(&peer->peer_info_lock);

	/*
	 * If there is a data frame from peer before the peer is
	 * registered for data service, enqueue them on to pending queue
	 * which will be flushed to HDD once that station is registered.
	 */
	if (!data_rx) {
		struct ol_rx_cached_buf *cache_buf;
		buf = rx_buf_list;
		while (buf) {
			next_buf = cdf_nbuf_queue_next(buf);
			cache_buf = cdf_mem_malloc(sizeof(*cache_buf));
			if (!cache_buf) {
				TXRX_PRINT(TXRX_PRINT_LEVEL_ERR,
					"Failed to allocate buf to cache the rx frames");
				cdf_nbuf_free(buf);
			} else {
				/* Add NULL terminator */
				cdf_nbuf_set_next(buf, NULL);
				cache_buf->buf = buf;
				cdf_spin_lock_bh(&peer->bufq_lock);
				list_add_tail(&cache_buf->list,
					      &peer->cached_bufq);
				cdf_spin_unlock_bh(&peer->bufq_lock);
			}
			buf = next_buf;
		}
	} else {
#ifdef QCA_CONFIG_SMP
		/*
		 * If the kernel is SMP, schedule rx thread to
		 * better use multicores.
		 */
		if (!ol_cfg_is_rx_thread_enabled(pdev->ctrl_pdev)) {
			ol_rx_data_cb(peer, rx_buf_list);
		} else {
			p_cds_sched_context sched_ctx =
				get_cds_sched_ctxt();
			struct cds_ol_rx_pkt *pkt;

			if (unlikely(!sched_ctx))
				goto drop_rx_buf;

			pkt = cds_alloc_ol_rx_pkt(sched_ctx);
			if (!pkt) {
				TXRX_PRINT(TXRX_PRINT_LEVEL_ERR,
					"No available Rx message buffer");
				goto drop_rx_buf;
			}
			pkt->callback = (cds_ol_rx_thread_cb)
					ol_rx_data_cb;
			pkt->context = (void *)peer;
			pkt->Rxpkt = (void *)rx_buf_list;
			pkt->staId = peer->local_id;
			cds_indicate_rxpkt(sched_ctx, pkt);
		}
#else                           /* QCA_CONFIG_SMP */
		ol_rx_data_cb(peer, rx_buf_list, 0);
#endif /* QCA_CONFIG_SMP */
	}

	return;

drop_rx_buf:
	TXRX_PRINT(TXRX_PRINT_LEVEL_ERR, "Dropping rx packets");
	buf = rx_buf_list;
	while (buf) {
		next_buf = cdf_nbuf_queue_next(buf);
		cdf_nbuf_free(buf);
		buf = next_buf;
	}
}

/**
 * ol_txrx_register_peer() - register peer
 * @rxcb: rx callback
 * @sta_desc: sta descriptor
 *
 * Return: CDF Status
 */
CDF_STATUS ol_txrx_register_peer(ol_rx_callback_fp rxcb,
				 struct ol_txrx_desc_type *sta_desc)
{
	struct ol_txrx_peer_t *peer;
	struct ol_txrx_pdev_t *pdev = cds_get_context(CDF_MODULE_ID_TXRX);
	union ol_txrx_peer_update_param_t param;
	struct privacy_exemption privacy_filter;

	if (!pdev) {
		TXRX_PRINT(TXRX_PRINT_LEVEL_ERR, "Pdev is NULL");
		return CDF_STATUS_E_INVAL;
	}

	if (sta_desc->sta_id >= WLAN_MAX_STA_COUNT) {
		TXRX_PRINT(TXRX_PRINT_LEVEL_ERR, "Invalid sta id :%d",
			 sta_desc->sta_id);
		return CDF_STATUS_E_INVAL;
	}

	peer = ol_txrx_peer_find_by_local_id(pdev, sta_desc->sta_id);
	if (!peer)
		return CDF_STATUS_E_FAULT;

	cdf_spin_lock_bh(&peer->peer_info_lock);
	peer->osif_rx = rxcb;
	peer->state = ol_txrx_peer_state_conn;
	cdf_spin_unlock_bh(&peer->peer_info_lock);

	param.qos_capable = sta_desc->is_qos_enabled;
	ol_txrx_peer_update(peer->vdev, peer->mac_addr.raw, &param,
			    ol_txrx_peer_update_qos_capable);

	if (sta_desc->is_wapi_supported) {
		/*Privacy filter to accept unencrypted WAI frames */
		privacy_filter.ether_type = ETHERTYPE_WAI;
		privacy_filter.filter_type = PRIVACY_FILTER_ALWAYS;
		privacy_filter.packet_type = PRIVACY_FILTER_PACKET_BOTH;
		ol_txrx_set_privacy_filters(peer->vdev, &privacy_filter, 1);
	}

	ol_txrx_flush_rx_frames(peer, 0);
	return CDF_STATUS_SUCCESS;
}

/**
 * ol_txrx_clear_peer() - clear peer
 * @sta_id: sta id
 *
 * Return: CDF Status
 */
CDF_STATUS ol_txrx_clear_peer(uint8_t sta_id)
{
	struct ol_txrx_peer_t *peer;
	struct ol_txrx_pdev_t *pdev = cds_get_context(CDF_MODULE_ID_TXRX);

	if (!pdev) {
		TXRX_PRINT(TXRX_PRINT_LEVEL_ERR, "%s: Unable to find pdev!",
			   __func__);
		return CDF_STATUS_E_FAILURE;
	}

	if (sta_id >= WLAN_MAX_STA_COUNT) {
		TXRX_PRINT(TXRX_PRINT_LEVEL_ERR, "Invalid sta id %d", sta_id);
		return CDF_STATUS_E_INVAL;
	}

#ifdef QCA_CONFIG_SMP
	{
		p_cds_sched_context sched_ctx = get_cds_sched_ctxt();
		/* Drop pending Rx frames in CDS */
		if (sched_ctx)
			cds_drop_rxpkt_by_staid(sched_ctx, sta_id);
	}
#endif

	peer = ol_txrx_peer_find_by_local_id(pdev, sta_id);
	if (!peer)
		return CDF_STATUS_E_FAULT;

	/* Purge the cached rx frame queue */
	ol_txrx_flush_rx_frames(peer, 1);

	cdf_spin_lock_bh(&peer->peer_info_lock);
	peer->osif_rx = NULL;
	peer->state = ol_txrx_peer_state_disc;
	cdf_spin_unlock_bh(&peer->peer_info_lock);

	return CDF_STATUS_SUCCESS;
}

/**
 * ol_txrx_register_ocb_peer - Function to register the OCB peer
 * @cds_ctx: Pointer to the global OS context
 * @mac_addr: MAC address of the self peer
 * @peer_id: Pointer to the peer ID
 *
 * Return: CDF_STATUS_SUCCESS on success, CDF_STATUS_E_FAILURE on failure
 */
CDF_STATUS ol_txrx_register_ocb_peer(void *cds_ctx, uint8_t *mac_addr,
				     uint8_t *peer_id)
{
	ol_txrx_pdev_handle pdev;
	ol_txrx_peer_handle peer;

	if (!cds_ctx) {
		TXRX_PRINT(TXRX_PRINT_LEVEL_ERR, "%s: Invalid context",
			   __func__);
		return CDF_STATUS_E_FAILURE;
	}

	pdev = cds_get_context(CDF_MODULE_ID_TXRX);
	if (!pdev) {
		TXRX_PRINT(TXRX_PRINT_LEVEL_ERR, "%s: Unable to find pdev!",
			   __func__);
		return CDF_STATUS_E_FAILURE;
	}

	peer = ol_txrx_find_peer_by_addr(pdev, mac_addr, peer_id);
	if (!peer) {
		TXRX_PRINT(TXRX_PRINT_LEVEL_ERR, "%s: Unable to find OCB peer!",
			   __func__);
		return CDF_STATUS_E_FAILURE;
	}

	ol_txrx_set_ocb_peer(pdev, peer);

	/* Set peer state to connected */
	ol_txrx_peer_state_update(pdev, peer->mac_addr.raw,
				  ol_txrx_peer_state_auth);

	return CDF_STATUS_SUCCESS;
}

/**
 * ol_txrx_set_ocb_peer - Function to store the OCB peer
 * @pdev: Handle to the HTT instance
 * @peer: Pointer to the peer
 */
void ol_txrx_set_ocb_peer(struct ol_txrx_pdev_t *pdev,
			  struct ol_txrx_peer_t *peer)
{
	if (pdev == NULL)
		return;

	pdev->ocb_peer = peer;
	pdev->ocb_peer_valid = (NULL != peer);
}

/**
 * ol_txrx_get_ocb_peer - Function to retrieve the OCB peer
 * @pdev: Handle to the HTT instance
 * @peer: Pointer to the returned peer
 *
 * Return: true if the peer is valid, false if not
 */
bool ol_txrx_get_ocb_peer(struct ol_txrx_pdev_t *pdev,
			  struct ol_txrx_peer_t **peer)
{
	int rc;

	if ((pdev == NULL) || (peer == NULL)) {
		rc = false;
		goto exit;
	}

	if (pdev->ocb_peer_valid) {
		*peer = pdev->ocb_peer;
		rc = true;
	} else {
		rc = false;
	}

exit:
	return rc;
}

#ifdef QCA_LL_TX_FLOW_CONTROL_V2
/**
 * ol_txrx_register_pause_cb() - register pause callback
 * @pause_cb: pause callback
 *
 * Return: CDF status
 */
CDF_STATUS ol_txrx_register_pause_cb(ol_tx_pause_callback_fp pause_cb)
{
	struct ol_txrx_pdev_t *pdev = cds_get_context(CDF_MODULE_ID_TXRX);
	if (!pdev || !pause_cb) {
		TXRX_PRINT(TXRX_PRINT_LEVEL_ERR, "pdev or pause_cb is NULL");
		return CDF_STATUS_E_INVAL;
	}
	pdev->pause_cb = pause_cb;
	return CDF_STATUS_SUCCESS;
}
#endif

#if defined(FEATURE_LRO)
/**
 * ol_txrx_lro_flush_handler() - LRO flush handler
 * @context: dev handle
 * @rxpkt: rx data
 * @staid: station id
 *
 * This function handles an LRO flush indication.
 * If the rx thread is enabled, it will be invoked by the rx
 * thread else it will be called in the tasklet context
 *
 * Return: none
 */
void ol_txrx_lro_flush_handler(void *context,
	 void *rxpkt,
	 uint16_t staid)
{
	ol_txrx_pdev_handle pdev = (ol_txrx_pdev_handle)context;

	if (cdf_unlikely(!pdev)) {
		CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
			 "%s: Invalid context", __func__);
		cdf_assert(0);
		return;
	}

	if (pdev->lro_info.lro_flush_cb)
		pdev->lro_info.lro_flush_cb(pdev->lro_info.lro_data);
	else
		CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
			 "%s: lro_flush_cb NULL", __func__);
}

/**
 * ol_txrx_lro_flush() - LRO flush callback
 * @data: opaque data pointer
 *
 * This is the callback registered with CE to trigger
 * an LRO flush
 *
 * Return: none
 */
void ol_txrx_lro_flush(void *data)
{
	p_cds_sched_context sched_ctx = get_cds_sched_ctxt();
	struct cds_ol_rx_pkt *pkt;
	ol_txrx_pdev_handle pdev = (ol_txrx_pdev_handle)data;

	if (cdf_unlikely(!sched_ctx))
		return;

	if (!ol_cfg_is_rx_thread_enabled(pdev->ctrl_pdev)) {
		ol_txrx_lro_flush_handler((void *)pdev, NULL, 0);
	} else {
		pkt = cds_alloc_ol_rx_pkt(sched_ctx);
		if (cdf_unlikely(!pkt)) {
			CDF_TRACE(CDF_MODULE_ID_TXRX, CDF_TRACE_LEVEL_ERROR,
				 "%s: Not able to allocate context", __func__);
			return;
		}

		pkt->callback =
			 (cds_ol_rx_thread_cb) ol_txrx_lro_flush_handler;
		pkt->context = pdev;
		pkt->Rxpkt = NULL;
		pkt->staId = 0;
		cds_indicate_rxpkt(sched_ctx, pkt);
	}
}

/**
 * ol_register_lro_flush_cb() - register the LRO flush callback
 * @handler: callback function
 * @data: opaque data pointer to be passed back
 *
 * Store the LRO flush callback provided and in turn
 * register OL's LRO flush handler with CE
 *
 * Return: none
 */
void ol_register_lro_flush_cb(void (handler)(void *), void *data)
{
	struct ol_softc *hif_device =
		(struct ol_softc *)cds_get_context(CDF_MODULE_ID_HIF);
	struct ol_txrx_pdev_t *pdev = cds_get_context(CDF_MODULE_ID_TXRX);

	pdev->lro_info.lro_flush_cb = handler;
	pdev->lro_info.lro_data = data;

	ce_lro_flush_cb_register(hif_device, ol_txrx_lro_flush, pdev);
}

/**
 * ol_deregister_lro_flush_cb() - deregister the LRO flush
 * callback
 *
 * Remove the LRO flush callback provided and in turn
 * deregister OL's LRO flush handler with CE
 *
 * Return: none
 */
void ol_deregister_lro_flush_cb(void)
{
	struct ol_softc *hif_device =
		(struct ol_softc *)cds_get_context(CDF_MODULE_ID_HIF);
	struct ol_txrx_pdev_t *pdev = cds_get_context(CDF_MODULE_ID_TXRX);

	ce_lro_flush_cb_deregister(hif_device);

	pdev->lro_info.lro_flush_cb = NULL;
	pdev->lro_info.lro_data = NULL;
}
#endif /* FEATURE_LRO */
