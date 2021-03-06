/*
 * Copyright (c) 2012-2015 The Linux Foundation. All rights reserved.
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

#ifdef FEATURE_OEM_DATA_SUPPORT

/**
 *  DOC: wlan_hdd_oemdata.c
 *
 *  Support for generic OEM Data Request handling
 *
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/wireless.h>
#include <wlan_hdd_includes.h>
#include <net/arp.h>
#include "qwlan_version.h"
#include "cds_utils.h"
#include "wma.h"

static struct hdd_context_s *p_hdd_ctx;

/**
 * hdd_oem_data_req_callback() - OEM Data request callback handler
 * @hHal: MAC handle
 * @pContext: User context.  For this callback the net device was registered
 * @oemDataReqID: The ID of the request
 * @oemDataReqStatus: Status of the request
 *
 * This function reports the results of the request to user space
 *
 * Return: CDF_STATUS enumeration
 */
static CDF_STATUS hdd_oem_data_req_callback(tHalHandle hHal,
					    void *pContext,
					    uint32_t oemDataReqID,
					    eOemDataReqStatus oemDataReqStatus)
{
	CDF_STATUS status = CDF_STATUS_SUCCESS;
	struct net_device *dev = (struct net_device *)pContext;
	union iwreq_data wrqu;
	char buffer[IW_CUSTOM_MAX + 1];

	memset(&wrqu, '\0', sizeof(wrqu));
	memset(buffer, '\0', sizeof(buffer));

	if (oemDataReqStatus == eOEM_DATA_REQ_FAILURE) {
		snprintf(buffer, IW_CUSTOM_MAX, "QCOM: OEM-DATA-REQ-FAILED");
		hddLog(LOGW, "%s: oem data req %d failed", __func__,
		       oemDataReqID);
	} else if (oemDataReqStatus == eOEM_DATA_REQ_INVALID_MODE) {
		snprintf(buffer, IW_CUSTOM_MAX,
			 "QCOM: OEM-DATA-REQ-INVALID-MODE");
		hddLog(LOGW,
		       "%s: oem data req %d failed because the driver is in invalid mode (IBSS|AP)",
		       __func__, oemDataReqID);
	} else {
		snprintf(buffer, IW_CUSTOM_MAX, "QCOM: OEM-DATA-REQ-SUCCESS");
	}

	wrqu.data.pointer = buffer;
	wrqu.data.length = strlen(buffer);

	wireless_send_event(dev, IWEVCUSTOM, &wrqu, buffer);

	return status;
}

/**
 * iw_get_oem_data_cap() - Get OEM Data Capabilities
 * @dev: net device upon which the request was received
 * @info: ioctl request information
 * @wrqu: ioctl request data
 * @extra: ioctl data payload
 *
 * This function gets the capability information for OEM Data Request
 * and Response.
 *
 * Return: 0 for success, negative errno value on failure
 */
int iw_get_oem_data_cap(struct net_device *dev,
			struct iw_request_info *info,
			union iwreq_data *wrqu, char *extra)
{
	CDF_STATUS status;
	t_iw_oem_data_cap oemDataCap;
	t_iw_oem_data_cap *pHddOemDataCap;
	hdd_adapter_t *pAdapter = (netdev_priv(dev));
	hdd_context_t *pHddContext;
	struct hdd_config *pConfig;
	uint32_t numChannels;
	uint8_t chanList[OEM_CAP_MAX_NUM_CHANNELS];
	uint32_t i;
	int ret;

	ENTER();

	pHddContext = WLAN_HDD_GET_CTX(pAdapter);
	ret = wlan_hdd_validate_context(pHddContext);
	if (0 != ret)
		return ret;

	pConfig = pHddContext->config;
	if (!pConfig) {
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
			  "%s:HDD configuration is null", __func__);
		return -ENOENT;
	}

	do {
		cdf_mem_zero(&oemDataCap, sizeof(oemDataCap));
		strlcpy(oemDataCap.oem_target_signature, OEM_TARGET_SIGNATURE,
			OEM_TARGET_SIGNATURE_LEN);
		oemDataCap.oem_target_type = pHddContext->target_type;
		oemDataCap.oem_fw_version = pHddContext->target_fw_version;
		oemDataCap.driver_version.major = QWLAN_VERSION_MAJOR;
		oemDataCap.driver_version.minor = QWLAN_VERSION_MINOR;
		oemDataCap.driver_version.patch = QWLAN_VERSION_PATCH;
		oemDataCap.driver_version.build = QWLAN_VERSION_BUILD;
		oemDataCap.allowed_dwell_time_min =
			pConfig->nNeighborScanMinChanTime;
		oemDataCap.allowed_dwell_time_max =
			pConfig->nNeighborScanMaxChanTime;
		oemDataCap.curr_dwell_time_min =
			sme_get_neighbor_scan_min_chan_time(pHddContext->hHal,
							    pAdapter->sessionId);
		oemDataCap.curr_dwell_time_max =
			sme_get_neighbor_scan_max_chan_time(pHddContext->hHal,
							    pAdapter->sessionId);
		oemDataCap.supported_bands = pConfig->nBandCapability;

		/* request for max num of channels */
		numChannels = WNI_CFG_VALID_CHANNEL_LIST_LEN;
		status = sme_get_cfg_valid_channels(pHddContext->hHal,
						    &chanList[0], &numChannels);
		if (CDF_STATUS_SUCCESS != status) {
			CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
				  "%s:failed to get valid channel list",
				  __func__);
			return -ENOENT;
		} else {
			/* make sure num channels is not more than chan list array */
			if (numChannels > OEM_CAP_MAX_NUM_CHANNELS) {
				CDF_TRACE(CDF_MODULE_ID_HDD,
					  CDF_TRACE_LEVEL_ERROR,
					  "%s:Num of channels(%d) more than length(%d) of chanlist",
					  __func__, numChannels,
					  OEM_CAP_MAX_NUM_CHANNELS);
				return -ENOMEM;
			}

			oemDataCap.num_channels = numChannels;
			for (i = 0; i < numChannels; i++) {
				oemDataCap.channel_list[i] = chanList[i];
			}
		}

		pHddOemDataCap = (t_iw_oem_data_cap *) (extra);
		cdf_mem_copy(pHddOemDataCap, &oemDataCap,
			     sizeof(*pHddOemDataCap));
	} while (0);

	EXIT();
	return 0;
}

/**
 * send_oem_reg_rsp_nlink_msg() - send oem registration response
 *
 * This function sends oem message to registered application process
 *
 * Return:  none
 */
static void send_oem_reg_rsp_nlink_msg(void)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	tAniMsgHdr *aniHdr;
	uint8_t *buf;
	uint8_t *numInterfaces;
	uint8_t *deviceMode;
	uint8_t *vdevId;
	hdd_adapter_list_node_t *pAdapterNode = NULL;
	hdd_adapter_list_node_t *pNext = NULL;
	hdd_adapter_t *pAdapter = NULL;
	CDF_STATUS status = 0;

	/* OEM message is always to a specific process and cannot be a broadcast */
	if (p_hdd_ctx->oem_pid == 0) {
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
			  "%s: invalid dest pid", __func__);
		return;
	}

	skb = alloc_skb(NLMSG_SPACE(WLAN_NL_MAX_PAYLOAD), GFP_KERNEL);
	if (skb == NULL) {
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
			  "%s: alloc_skb failed", __func__);
		return;
	}

	nlh = (struct nlmsghdr *)skb->data;
	nlh->nlmsg_pid = 0;     /* from kernel */
	nlh->nlmsg_flags = 0;
	nlh->nlmsg_seq = 0;
	nlh->nlmsg_type = WLAN_NL_MSG_OEM;
	aniHdr = NLMSG_DATA(nlh);
	aniHdr->type = ANI_MSG_APP_REG_RSP;

	/* Fill message body:
	 *   First byte will be number of interfaces, followed by
	 *   two bytes for each interfaces
	 *     - one byte for device mode
	 *     - one byte for vdev id
	 */
	buf = (char *)((char *)aniHdr + sizeof(tAniMsgHdr));
	numInterfaces = buf++;
	*numInterfaces = 0;

	/* Iterate through each of the adapters and fill device mode and vdev id */
	status = hdd_get_front_adapter(p_hdd_ctx, &pAdapterNode);
	while ((CDF_STATUS_SUCCESS == status) && pAdapterNode) {
		pAdapter = pAdapterNode->pAdapter;
		if (pAdapter) {
			deviceMode = buf++;
			vdevId = buf++;
			*deviceMode = pAdapter->device_mode;
			*vdevId = pAdapter->sessionId;
			(*numInterfaces)++;
			CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_INFO,
				  "%s: numInterfaces: %d, deviceMode: %d, vdevId: %d",
				  __func__, *numInterfaces, *deviceMode,
				  *vdevId);
		}
		status = hdd_get_next_adapter(p_hdd_ctx, pAdapterNode, &pNext);
		pAdapterNode = pNext;
	}

	aniHdr->length =
		sizeof(uint8_t) + (*numInterfaces) * 2 * sizeof(uint8_t);
	nlh->nlmsg_len = NLMSG_LENGTH((sizeof(tAniMsgHdr) + aniHdr->length));

	skb_put(skb, NLMSG_SPACE((sizeof(tAniMsgHdr) + aniHdr->length)));

	CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_INFO,
		  "%s: sending App Reg Response length (%d) to process pid (%d)",
		  __func__, aniHdr->length, p_hdd_ctx->oem_pid);

	(void)nl_srv_ucast(skb, p_hdd_ctx->oem_pid, MSG_DONTWAIT);

	return;
}

/**
 * send_oem_err_rsp_nlink_msg() - send oem error response
 * @app_pid: PID of oem application process
 * @error_code: response error code
 *
 * This function sends error response to oem app
 *
 * Return: none
 */
static void send_oem_err_rsp_nlink_msg(int32_t app_pid, uint8_t error_code)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	tAniMsgHdr *aniHdr;
	uint8_t *buf;

	skb = alloc_skb(NLMSG_SPACE(WLAN_NL_MAX_PAYLOAD), GFP_KERNEL);
	if (skb == NULL) {
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
			  "%s: alloc_skb failed", __func__);
		return;
	}

	nlh = (struct nlmsghdr *)skb->data;
	nlh->nlmsg_pid = 0;     /* from kernel */
	nlh->nlmsg_flags = 0;
	nlh->nlmsg_seq = 0;
	nlh->nlmsg_type = WLAN_NL_MSG_OEM;
	aniHdr = NLMSG_DATA(nlh);
	aniHdr->type = ANI_MSG_OEM_ERROR;
	aniHdr->length = sizeof(uint8_t);
	nlh->nlmsg_len = NLMSG_LENGTH(sizeof(tAniMsgHdr) + aniHdr->length);

	/* message body will contain one byte of error code */
	buf = (char *)((char *)aniHdr + sizeof(tAniMsgHdr));
	*buf = error_code;

	skb_put(skb, NLMSG_SPACE(sizeof(tAniMsgHdr) + aniHdr->length));

	CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_INFO,
		  "%s: sending oem error response to process pid (%d)",
		  __func__, app_pid);

	(void)nl_srv_ucast(skb, app_pid, MSG_DONTWAIT);

	return;
}

/**
 * hdd_send_oem_data_rsp_msg() - send oem data response
 * @length: length of the OEM Data Response message
 * @oemDataRsp: the actual OEM Data Response message
 *
 * This function sends an OEM Data Response message to a registered
 * application process over the netlink socket.
 *
 * Return: 0 for success, non zero for failure
 */
void hdd_send_oem_data_rsp_msg(int length, uint8_t *oemDataRsp)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	tAniMsgHdr *aniHdr;
	uint8_t *oemData;

	/* OEM message is always to a specific process and cannot be a broadcast */
	if (p_hdd_ctx->oem_pid == 0) {
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
			  "%s: invalid dest pid", __func__);
		return;
	}

	if (length > OEM_DATA_RSP_SIZE) {
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
			  "%s: invalid length of Oem Data response", __func__);
		return;
	}

	skb = alloc_skb(NLMSG_SPACE(sizeof(tAniMsgHdr) + OEM_DATA_RSP_SIZE),
			GFP_KERNEL);
	if (skb == NULL) {
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
			  "%s: alloc_skb failed", __func__);
		return;
	}

	nlh = (struct nlmsghdr *)skb->data;
	nlh->nlmsg_pid = 0;     /* from kernel */
	nlh->nlmsg_flags = 0;
	nlh->nlmsg_seq = 0;
	nlh->nlmsg_type = WLAN_NL_MSG_OEM;
	aniHdr = NLMSG_DATA(nlh);
	aniHdr->type = ANI_MSG_OEM_DATA_RSP;

	aniHdr->length = length;
	nlh->nlmsg_len = NLMSG_LENGTH((sizeof(tAniMsgHdr) + aniHdr->length));
	oemData = (uint8_t *) ((char *)aniHdr + sizeof(tAniMsgHdr));
	cdf_mem_copy(oemData, oemDataRsp, length);

	skb_put(skb, NLMSG_SPACE((sizeof(tAniMsgHdr) + aniHdr->length)));

	CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_INFO,
		  "%s: sending Oem Data Response of len (%d) to process pid (%d)",
		  __func__, length, p_hdd_ctx->oem_pid);

	(void)nl_srv_ucast(skb, p_hdd_ctx->oem_pid, MSG_DONTWAIT);

	return;
}

/**
 * oem_process_data_req_msg() - process oem data request
 * @oemDataLen: Length to OEM Data buffer
 * @oemData: Pointer to OEM Data buffer
 *
 * This function sends oem message to SME
 *
 * Return: CDF_STATUS enumeration
 */
static CDF_STATUS oem_process_data_req_msg(int oemDataLen, char *oemData)
{
	hdd_adapter_t *pAdapter = NULL;
	tOemDataReqConfig oemDataReqConfig;
	uint32_t oemDataReqID = 0;
	CDF_STATUS status = CDF_STATUS_SUCCESS;

	/* for now, STA interface only */
	pAdapter = hdd_get_adapter(p_hdd_ctx, WLAN_HDD_INFRA_STATION);
	if (!pAdapter) {
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
			  "%s: No adapter for STA mode", __func__);
		return CDF_STATUS_E_FAILURE;
	}

	if (!oemData) {
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
			  "%s: oemData is null", __func__);
		return CDF_STATUS_E_FAILURE;
	}

	cdf_mem_zero(&oemDataReqConfig, sizeof(tOemDataReqConfig));

	cdf_mem_copy((&oemDataReqConfig)->oemDataReq, oemData, oemDataLen);

	CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_INFO,
		  "%s: calling sme_oem_data_req", __func__);

	status = sme_oem_data_req(p_hdd_ctx->hHal,
				  pAdapter->sessionId,
				  &oemDataReqConfig,
				  &oemDataReqID,
				  &hdd_oem_data_req_callback, pAdapter->dev);
	return status;
}

/**
 * oem_process_channel_info_req_msg() - process oem channel_info request
 * @numOfChannels: number of channels
 * @chanList: list of channel information
 *
 * This function responds with channel info to oem process
 *
 * Return: 0 for success, non zero for failure
 */
static int oem_process_channel_info_req_msg(int numOfChannels, char *chanList)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	tAniMsgHdr *aniHdr;
	tHddChannelInfo *pHddChanInfo;
	tHddChannelInfo hddChanInfo;
	uint8_t chanId;
	uint32_t reg_info_1;
	uint32_t reg_info_2;
	CDF_STATUS status = CDF_STATUS_E_FAILURE;
	int i;
	uint8_t *buf;

	/* OEM message is always to a specific process and cannot be a broadcast */
	if (p_hdd_ctx->oem_pid == 0) {
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
			  "%s: invalid dest pid", __func__);
		return -EPERM;
	}

	skb = alloc_skb(NLMSG_SPACE(sizeof(tAniMsgHdr) + sizeof(uint8_t) +
				    numOfChannels * sizeof(tHddChannelInfo)),
			GFP_KERNEL);
	if (skb == NULL) {
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
			  "%s: alloc_skb failed", __func__);
		return -ENOMEM;
	}

	nlh = (struct nlmsghdr *)skb->data;
	nlh->nlmsg_pid = 0;     /* from kernel */
	nlh->nlmsg_flags = 0;
	nlh->nlmsg_seq = 0;
	nlh->nlmsg_type = WLAN_NL_MSG_OEM;
	aniHdr = NLMSG_DATA(nlh);
	aniHdr->type = ANI_MSG_CHANNEL_INFO_RSP;

	aniHdr->length =
		sizeof(uint8_t) + numOfChannels * sizeof(tHddChannelInfo);
	nlh->nlmsg_len = NLMSG_LENGTH((sizeof(tAniMsgHdr) + aniHdr->length));

	/* First byte of message body will have num of channels */
	buf = (char *)((char *)aniHdr + sizeof(tAniMsgHdr));
	*buf++ = numOfChannels;

	/* Next follows channel info struct for each channel id.
	 * If chan id is wrong or SME returns failure for a channel
	 * then fill in 0 in channel info for that particular channel
	 */
	for (i = 0; i < numOfChannels; i++) {
		pHddChanInfo = (tHddChannelInfo *) ((char *)buf +
						    i *
						    sizeof(tHddChannelInfo));

		chanId = chanList[i];
		status = sme_get_reg_info(p_hdd_ctx->hHal, chanId,
					  &reg_info_1, &reg_info_2);
		if (CDF_STATUS_SUCCESS == status) {
			/* copy into hdd chan info struct */
			hddChanInfo.chan_id = chanId;
			hddChanInfo.reserved0 = 0;
			hddChanInfo.mhz = cds_chan_to_freq(chanId);
			hddChanInfo.band_center_freq1 = hddChanInfo.mhz;
			hddChanInfo.band_center_freq2 = 0;

			hddChanInfo.info = 0;
			if (CHANNEL_STATE_DFS ==
			    cds_get_channel_state(chanId))
				WMI_SET_CHANNEL_FLAG(&hddChanInfo,
						     WMI_CHAN_FLAG_DFS);
			hddChanInfo.reg_info_1 = reg_info_1;
			hddChanInfo.reg_info_2 = reg_info_2;
		} else {
			/* channel info is not returned, fill in zeros in channel
			 * info struct
			 */
			CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_INFO,
				  "%s: sme_get_reg_info failed for chan (%d), return info 0",
				  __func__, chanId);
			hddChanInfo.chan_id = chanId;
			hddChanInfo.reserved0 = 0;
			hddChanInfo.mhz = 0;
			hddChanInfo.band_center_freq1 = 0;
			hddChanInfo.band_center_freq2 = 0;
			hddChanInfo.info = 0;
			hddChanInfo.reg_info_1 = 0;
			hddChanInfo.reg_info_2 = 0;
		}
		cdf_mem_copy(pHddChanInfo, &hddChanInfo,
			     sizeof(tHddChannelInfo));
	}

	skb_put(skb, NLMSG_SPACE((sizeof(tAniMsgHdr) + aniHdr->length)));

	CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_INFO,
		  "%s: sending channel info resp for num channels (%d) to pid (%d)",
		  __func__, numOfChannels, p_hdd_ctx->oem_pid);

	(void)nl_srv_ucast(skb, p_hdd_ctx->oem_pid, MSG_DONTWAIT);

	return 0;
}

/**
 * hdd_send_peer_status_ind_to_oem_app() -
 *	Function to send peer status to a registered application
 * @peerMac: MAC address of peer
 * @peerStatus: ePeerConnected or ePeerDisconnected
 * @peerTimingMeasCap: 0: RTT/RTT2, 1: RTT3. Default is 0
 * @sessionId: SME session id, i.e. vdev_id
 * @chan_info: operating channel information
 *
 * Return: none
 */
void hdd_send_peer_status_ind_to_oem_app(struct cdf_mac_addr *peerMac,
					 uint8_t peerStatus,
					 uint8_t peerTimingMeasCap,
					 uint8_t sessionId,
					 tSirSmeChanInfo *chan_info)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	tAniMsgHdr *aniHdr;
	tPeerStatusInfo *pPeerInfo;

	if (!p_hdd_ctx || !p_hdd_ctx->hHal) {
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
			  "%s: Either HDD Ctx is null or Hal Ctx is null",
			  __func__);
		return;
	}

	/* check if oem app has registered and pid is valid */
	if ((!p_hdd_ctx->oem_app_registered) || (p_hdd_ctx->oem_pid == 0)) {
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_INFO_HIGH,
			  "%s: OEM app is not registered(%d) or pid is invalid(%d)",
			  __func__, p_hdd_ctx->oem_app_registered,
			  p_hdd_ctx->oem_pid);
		return;
	}

	skb = alloc_skb(NLMSG_SPACE(sizeof(tAniMsgHdr) +
				    sizeof(tPeerStatusInfo)),
			GFP_KERNEL);
	if (skb == NULL) {
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
			  "%s: alloc_skb failed", __func__);
		return;
	}

	nlh = (struct nlmsghdr *)skb->data;
	nlh->nlmsg_pid = 0;     /* from kernel */
	nlh->nlmsg_flags = 0;
	nlh->nlmsg_seq = 0;
	nlh->nlmsg_type = WLAN_NL_MSG_OEM;
	aniHdr = NLMSG_DATA(nlh);
	aniHdr->type = ANI_MSG_PEER_STATUS_IND;

	aniHdr->length = sizeof(tPeerStatusInfo);
	nlh->nlmsg_len = NLMSG_LENGTH((sizeof(tAniMsgHdr) + aniHdr->length));

	pPeerInfo = (tPeerStatusInfo *) ((char *)aniHdr + sizeof(tAniMsgHdr));

	cdf_mem_copy(pPeerInfo->peer_mac_addr, peerMac->bytes,
		     sizeof(peerMac->bytes));
	pPeerInfo->peer_status = peerStatus;
	pPeerInfo->vdev_id = sessionId;
	pPeerInfo->peer_capability = peerTimingMeasCap;
	pPeerInfo->reserved0 = 0;

	if (chan_info) {
		pPeerInfo->peer_chan_info.chan_id = chan_info->chan_id;
		pPeerInfo->peer_chan_info.reserved0 = 0;
		pPeerInfo->peer_chan_info.mhz = chan_info->mhz;
		pPeerInfo->peer_chan_info.band_center_freq1 =
			chan_info->band_center_freq1;
		pPeerInfo->peer_chan_info.band_center_freq2 =
			chan_info->band_center_freq2;
		pPeerInfo->peer_chan_info.info = chan_info->info;
		pPeerInfo->peer_chan_info.reg_info_1 = chan_info->reg_info_1;
		pPeerInfo->peer_chan_info.reg_info_2 = chan_info->reg_info_2;
	} else {
		pPeerInfo->peer_chan_info.chan_id = 0;
		pPeerInfo->peer_chan_info.reserved0 = 0;
		pPeerInfo->peer_chan_info.mhz = 0;
		pPeerInfo->peer_chan_info.band_center_freq1 = 0;
		pPeerInfo->peer_chan_info.band_center_freq2 = 0;
		pPeerInfo->peer_chan_info.info = 0;
		pPeerInfo->peer_chan_info.reg_info_1 = 0;
		pPeerInfo->peer_chan_info.reg_info_2 = 0;
	}
	skb_put(skb, NLMSG_SPACE((sizeof(tAniMsgHdr) + aniHdr->length)));

	CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_INFO_HIGH,
		  "%s: sending peer " MAC_ADDRESS_STR
		  " status(%d), peerTimingMeasCap(%d), vdevId(%d), chanId(%d)"
		  " to oem app pid(%d), center freq 1 (%d), center freq 2 (%d),"
		  " info (0x%x), frequency (%d),reg info 1 (0x%x),"
		  " reg info 2 (0x%x)", __func__,
		  MAC_ADDR_ARRAY(peerMac->bytes),
		  peerStatus, peerTimingMeasCap,
		  sessionId, pPeerInfo->peer_chan_info.chan_id,
		  p_hdd_ctx->oem_pid,
		  pPeerInfo->peer_chan_info.band_center_freq1,
		  pPeerInfo->peer_chan_info.band_center_freq2,
		  pPeerInfo->peer_chan_info.info,
		  pPeerInfo->peer_chan_info.mhz,
		  pPeerInfo->peer_chan_info.reg_info_1,
		  pPeerInfo->peer_chan_info.reg_info_2);

	(void)nl_srv_ucast(skb, p_hdd_ctx->oem_pid, MSG_DONTWAIT);

	return;
}

/*
 * Callback function invoked by Netlink service for all netlink
 * messages (from user space) addressed to WLAN_NL_MSG_OEM
 */

/**
 * oem_msg_callback() - callback invoked by netlink service
 * @skb:    skb with netlink message
 *
 * This function gets invoked by netlink service when a message
 * is received from user space addressed to WLAN_NL_MSG_OEM
 *
 * Return: zero on success
 *         On error, error number will be returned.
 */
static int oem_msg_callback(struct sk_buff *skb)
{
	struct nlmsghdr *nlh;
	tAniMsgHdr *msg_hdr;
	int ret;
	char *sign_str = NULL;
	nlh = (struct nlmsghdr *)skb->data;

	if (!nlh) {
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
			  "%s: Netlink header null", __func__);
		return -EPERM;
	}

	ret = wlan_hdd_validate_context(p_hdd_ctx);
	if (0 != ret) {
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
			  FL("HDD context is not valid"));
		return ret;
	}

	msg_hdr = NLMSG_DATA(nlh);

	if (!msg_hdr) {
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
			  "%s: Message header null", __func__);
		send_oem_err_rsp_nlink_msg(nlh->nlmsg_pid,
					   OEM_ERR_NULL_MESSAGE_HEADER);
		return -EPERM;
	}

	if (nlh->nlmsg_len <
	    NLMSG_LENGTH(sizeof(tAniMsgHdr) + msg_hdr->length)) {
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
			  "%s: Invalid nl msg len, nlh->nlmsg_len (%d), msg_hdr->len (%d)",
			  __func__, nlh->nlmsg_len, msg_hdr->length);
		send_oem_err_rsp_nlink_msg(nlh->nlmsg_pid,
					   OEM_ERR_INVALID_MESSAGE_LENGTH);
		return -EPERM;
	}

	switch (msg_hdr->type) {
	case ANI_MSG_APP_REG_REQ:
		/* Registration request is only allowed for Qualcomm Application */
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_INFO,
			  "%s: Received App Req Req from App process pid(%d), len(%d)",
			  __func__, nlh->nlmsg_pid, msg_hdr->length);

		sign_str = (char *)((char *)msg_hdr + sizeof(tAniMsgHdr));
		if ((OEM_APP_SIGNATURE_LEN == msg_hdr->length) &&
		    (0 == strncmp(sign_str, OEM_APP_SIGNATURE_STR,
				  OEM_APP_SIGNATURE_LEN))) {
			CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_INFO,
				  "%s: Valid App Req Req from oem app process pid(%d)",
				  __func__, nlh->nlmsg_pid);

			p_hdd_ctx->oem_app_registered = true;
			p_hdd_ctx->oem_pid = nlh->nlmsg_pid;
			send_oem_reg_rsp_nlink_msg();
		} else {
			CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
				  "%s: Invalid signature in App Reg Request from pid(%d)",
				  __func__, nlh->nlmsg_pid);
			send_oem_err_rsp_nlink_msg(nlh->nlmsg_pid,
						   OEM_ERR_INVALID_SIGNATURE);
			return -EPERM;
		}
		break;

	case ANI_MSG_OEM_DATA_REQ:
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_INFO,
			  "%s: Received Oem Data Request length(%d) from pid: %d",
			  __func__, msg_hdr->length, nlh->nlmsg_pid);

		if ((!p_hdd_ctx->oem_app_registered) ||
		    (nlh->nlmsg_pid != p_hdd_ctx->oem_pid)) {
			/* either oem app is not registered yet or pid is different */
			CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
				  "%s: OEM DataReq: app not registered(%d) or incorrect pid(%d)",
				  __func__, p_hdd_ctx->oem_app_registered,
				  nlh->nlmsg_pid);
			send_oem_err_rsp_nlink_msg(nlh->nlmsg_pid,
						   OEM_ERR_APP_NOT_REGISTERED);
			return -EPERM;
		}

		if ((!msg_hdr->length) || (OEM_DATA_REQ_SIZE < msg_hdr->length)) {
			CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
				  "%s: Invalid length (%d) in Oem Data Request",
				  __func__, msg_hdr->length);
			send_oem_err_rsp_nlink_msg(nlh->nlmsg_pid,
						   OEM_ERR_INVALID_MESSAGE_LENGTH);
			return -EPERM;
		}
		oem_process_data_req_msg(msg_hdr->length,
					 (char *)((char *)msg_hdr +
						  sizeof(tAniMsgHdr)));
		break;

	case ANI_MSG_CHANNEL_INFO_REQ:
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_INFO,
			  "%s: Received channel info request, num channel(%d) from pid: %d",
			  __func__, msg_hdr->length, nlh->nlmsg_pid);

		if ((!p_hdd_ctx->oem_app_registered) ||
		    (nlh->nlmsg_pid != p_hdd_ctx->oem_pid)) {
			/* either oem app is not registered yet or pid is different */
			CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
				  "%s: Chan InfoReq: app not registered(%d) or incorrect pid(%d)",
				  __func__, p_hdd_ctx->oem_app_registered,
				  nlh->nlmsg_pid);
			send_oem_err_rsp_nlink_msg(nlh->nlmsg_pid,
						   OEM_ERR_APP_NOT_REGISTERED);
			return -EPERM;
		}

		/* message length contains list of channel ids */
		if ((!msg_hdr->length) ||
		    (WNI_CFG_VALID_CHANNEL_LIST_LEN < msg_hdr->length)) {
			CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
				  "%s: Invalid length (%d) in channel info request",
				  __func__, msg_hdr->length);
			send_oem_err_rsp_nlink_msg(nlh->nlmsg_pid,
						   OEM_ERR_INVALID_MESSAGE_LENGTH);
			return -EPERM;
		}
		oem_process_channel_info_req_msg(msg_hdr->length,
						 (char *)((char *)msg_hdr +
							  sizeof(tAniMsgHdr)));
		break;

	default:
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
			  "%s: Received Invalid message type (%d), length (%d)",
			  __func__, msg_hdr->type, msg_hdr->length);
		send_oem_err_rsp_nlink_msg(nlh->nlmsg_pid,
					   OEM_ERR_INVALID_MESSAGE_TYPE);
		return -EPERM;
	}
	return 0;
}

static int __oem_msg_callback(struct sk_buff *skb)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = oem_msg_callback(skb);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * oem_activate_service() - Activate oem message handler
 * @hdd_ctx:   pointer to global HDD context
 *
 * This function registers a handler to receive netlink message from
 * an OEM application process.
 *
 * Return: zero on success
 *         On error, error number will be returned.
 */
int oem_activate_service(struct hdd_context_s *hdd_ctx)
{
	p_hdd_ctx = hdd_ctx;

	/* Register the msg handler for msgs addressed to WLAN_NL_MSG_OEM */
	return nl_srv_register(WLAN_NL_MSG_OEM, __oem_msg_callback);
}

#endif
