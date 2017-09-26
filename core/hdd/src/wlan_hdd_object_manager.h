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
 * hdd_objmgr_create_and_store_psoc() - Create psoc and store in hdd context
 * @hdd_ctx: Hdd context
 * @psoc_id: Psoc Id
 *
 * This API creates Psoc object with given @psoc_id and store the psoc reference
 * to hdd context
 *
 * Return: 0 for success, negative error code for failure
 */
int hdd_objmgr_create_and_store_psoc(struct hdd_context *hdd_ctx, uint8_t psoc_id);

/**
 * hdd_objmgr_release_and_destroy_psoc() - Deletes the psoc object
 * @hdd_ctx: Hdd context
 *
 * This API deletes psoc object and release its reference from hdd context
 *
 * Return: 0 for success, negative error code for failure
 */
int hdd_objmgr_release_and_destroy_psoc(struct hdd_context *hdd_ctx);

/**
 * hdd_objmgr_create_and_store_pdev() - Create pdev and store in hdd context
 * @hdd_ctx: Hdd context
 *
 * This API creates the pdev object and store the pdev reference to hdd context
 *
 * Return: 0 for success, negative error code for failure
 */
int hdd_objmgr_create_and_store_pdev(struct hdd_context *hdd_ctx);

/**
 * hdd_objmgr_release_and_destroy_pdev() - Deletes the pdev object
 * @hdd_ctx: Hdd context
 *
 * This API deletes pdev object and release its reference from hdd context
 *
 * Return: 0 for success, negative error code for failure
 */
int hdd_objmgr_release_and_destroy_pdev(struct hdd_context *hdd_ctx);

/**
 * hdd_objmgr_create_and_store_vdev() - Create vdev and store in hdd adapter
 * @pdev: pdev pointer
 * @adapter: hdd adapter
 *
 * This API creates the vdev object and store the vdev reference to the
 * given @adapter. Also, creates a self peer for the vdev.
 *
 * Return: 0 for success, negative error code for failure
 */
int hdd_objmgr_create_and_store_vdev(struct wlan_objmgr_pdev *pdev,
			      struct hdd_adapter *adapter);

/**
 * hdd_objmgr_destroy_vdev() - Delete vdev
 * @adapter: hdd adapter
 *
 * This function logically destroys the vdev in object manager. Physical
 * deletion is prevented until the vdev is released via a call to
 * hdd_objmgr_release_vdev(). E.g.
 *
 *	hdd_objmgr_destroy_vdev(...);
 *	sme_close_session(...);
 *	hdd_objmgr_release_vdev(...);
 *
 * Return: 0 for success, negative error code for failure
 */
int hdd_objmgr_destroy_vdev(struct hdd_adapter *adapter);

/**
 * hdd_objmgr_release_vdev() - releases the vdev from adapter
 * @adapter: hdd adapter
 *
 * See also hdd_objmgr_destroy_vdev()
 *
 * Return: 0 for success, negative error code for failure
 */
int hdd_objmgr_release_vdev(struct hdd_adapter *adapter);

/**
 * hdd_objmgr_release_and_destroy_vdev() - Delete vdev and remove from adapter
 * @adapter: hdd adapter
 *
 * This API deletes vdev object and release its reference from hdd adapter
 *
 * Return: 0 for success, negative error code for failure
 */
int hdd_objmgr_release_and_destroy_vdev(struct hdd_adapter *adapter);

/**
 * hdd_objmgr_add_peer_object() - Create and add the peer to the vdev
 * @vdev: vdev pointer
 * @adapter_mode: adapter mode
 * @mac_addr: Peer mac address
 * @is_p2p_type: TRUE if peer is p2p type
 *
 * This API creates and adds the peer object to the given @vdev. The peer type
 * (STA, AP, P2P or IBSS) is assigned based on adapter mode. For example, if
 * adapter mode is STA, peer is AP.
 *
 * Return: 0 for success, negative error code for failure
 */
int hdd_objmgr_add_peer_object(struct wlan_objmgr_vdev *vdev,
			       enum tQDF_ADAPTER_MODE adapter_mode,
			       uint8_t *mac_addr,
			       bool is_p2p_type);

/**
 * hdd_objmgr_remove_peer_object() - Delete and remove the peer from vdev
 * @vdev: vdev pointer
 * @mac_addr: Peer Mac address
 *
 * This API finds the peer object from given @mac_addr and deletes the same.
 *
 * Return: 0 for success, negative error code for failure
 */
int hdd_objmgr_remove_peer_object(struct wlan_objmgr_vdev *vdev,
				  uint8_t *mac_addr);

/**
 * hdd_objmgr_set_peer_mlme_auth_state() - set the peer mlme auth state
 * @vdev: vdev pointer
 * @is_authenticated: Peer mlme auth state true/false
 *
 * This API set the peer mlme auth state
 *
 * Return: 0 for success, negative error code for failure
 */
int hdd_objmgr_set_peer_mlme_auth_state(struct wlan_objmgr_vdev *vdev,
					bool is_authenticated);

/**
 * hdd_objmgr_set_peer_mlme_state() - set the peer mlme state
 * @vdev: vdev pointer
 * @peer_state: Peer mlme state
 *
 * This API set the peer mlme state
 *
 * Return: 0 for success, negative error code for failure
 */
int hdd_objmgr_set_peer_mlme_state(struct wlan_objmgr_vdev *vdev,
				   enum wlan_peer_state peer_state);

#endif /* end #if !defined(WLAN_HDD_OBJECT_MANAGER_H) */