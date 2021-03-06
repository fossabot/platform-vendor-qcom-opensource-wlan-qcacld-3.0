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

#if !defined(__SME_API_H)
#define __SME_API_H

/**
 * file  smeApi.h
 *
 * brief prototype for SME APIs
 */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include "csr_api.h"
#include "cds_mq.h"
#include "cdf_lock.h"
#include "cdf_types.h"
#include "sir_api.h"
#include "cds_reg_service.h"
#include "p2p_api.h"
#include "cds_regdomain.h"
#include "sme_internal.h"
#include "wma_tgt_cfg.h"

#ifdef FEATURE_OEM_DATA_SUPPORT
#include "oem_data_api.h"
#endif

#if defined WLAN_FEATURE_VOWIFI
#include "sme_rrm_internal.h"
#endif
#include "sir_types.h"
/*--------------------------------------------------------------------------
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/

#define SME_SUMMARY_STATS         1
#define SME_GLOBAL_CLASSA_STATS   2
#define SME_GLOBAL_CLASSB_STATS   4
#define SME_GLOBAL_CLASSC_STATS   8
#define SME_GLOBAL_CLASSD_STATS  16
#define SME_PER_STA_STATS        32

#define SME_SESSION_ID_ANY        50

#define SME_INVALID_COUNTRY_CODE "XX"

#define SME_2_4_GHZ_MAX_FREQ    3000

#define SME_SET_CHANNEL_REG_POWER(reg_info_1, val) do {	\
	reg_info_1 &= 0xff00ffff;	      \
	reg_info_1 |= ((val & 0xff) << 16);   \
} while (0)

#define SME_SET_CHANNEL_MAX_TX_POWER(reg_info_2, val) do { \
	reg_info_2 &= 0xffff00ff;	      \
	reg_info_2 |= ((val & 0xff) << 8);   \
} while (0)

#define SME_CONFIG_TO_ROAM_CONFIG 1
#define ROAM_CONFIG_TO_SME_CONFIG 2

/*--------------------------------------------------------------------------
  Type declarations
  ------------------------------------------------------------------------*/
typedef void (*hdd_ftm_msg_processor)(void *);
typedef struct _smeConfigParams {
	tCsrConfigParam csrConfig;
#if defined WLAN_FEATURE_VOWIFI
	struct rrm_config_param rrmConfig;
#endif
#if defined FEATURE_WLAN_LFR
	uint8_t isFastRoamIniFeatureEnabled;
	uint8_t MAWCEnabled;
#endif
#if defined FEATURE_WLAN_ESE
	uint8_t isEseIniFeatureEnabled;
#endif
#if  defined(WLAN_FEATURE_VOWIFI_11R) || defined(FEATURE_WLAN_ESE) || \
	defined(FEATURE_WLAN_LFR)
	uint8_t isFastTransitionEnabled;
	uint8_t RoamRssiDiff;
	bool isWESModeEnabled;
#endif
	uint8_t isAmsduSupportInAMPDU;
	bool pnoOffload;
	uint8_t fEnableDebugLog;
	uint8_t max_intf_count;
	bool enable5gEBT;
	bool enableSelfRecovery;
	uint32_t f_sta_miracast_mcc_rest_time_val;
#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
	bool sap_channel_avoidance;
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */
	uint8_t f_prefer_non_dfs_on_radar;
	bool is_ps_enabled;
	bool policy_manager_enabled;
	uint32_t fine_time_meas_cap;
	uint32_t dual_mac_feature_disable;
#ifdef FEATURE_WLAN_SCAN_PNO
	bool pno_channel_prediction;
	uint8_t top_k_num_of_channels;
	uint8_t stationary_thresh;
	uint32_t channel_prediction_full_scan;
#endif
	bool early_stop_scan_enable;
	int8_t early_stop_scan_min_threshold;
	int8_t early_stop_scan_max_threshold;
	int8_t first_scan_bucket_threshold;
} tSmeConfigParams, *tpSmeConfigParams;

#ifdef FEATURE_WLAN_TDLS
#define SME_TDLS_MAX_SUPP_CHANNELS       128
#define SME_TDLS_MAX_SUPP_OPER_CLASSES   32

typedef struct _smeTdlsPeerCapParams {
	uint8_t isPeerResponder;
	uint8_t peerUapsdQueue;
	uint8_t peerMaxSp;
	uint8_t peerBuffStaSupport;
	uint8_t peerOffChanSupport;
	uint8_t peerCurrOperClass;
	uint8_t selfCurrOperClass;
	uint8_t peerChanLen;
	uint8_t peerChan[SME_TDLS_MAX_SUPP_CHANNELS];
	uint8_t peerOperClassLen;
	uint8_t peerOperClass[SME_TDLS_MAX_SUPP_OPER_CLASSES];
	uint8_t prefOffChanNum;
	uint8_t prefOffChanBandwidth;
	uint8_t opClassForPrefOffChan;
} tSmeTdlsPeerCapParams;

typedef enum {
	eSME_TDLS_PEER_STATE_PEERING,
	eSME_TDLS_PEER_STATE_CONNECTED,
	eSME_TDLS_PEER_STATE_TEARDOWN
} eSmeTdlsPeerState;

typedef struct _smeTdlsPeerStateParams {
	uint32_t vdevId;
	tSirMacAddr peerMacAddr;
	uint32_t peerState;
	tSmeTdlsPeerCapParams peerCap;
} tSmeTdlsPeerStateParams;

#define ENABLE_CHANSWITCH  1
#define DISABLE_CHANSWITCH 2
#define BW_20_OFFSET_BIT   0
#define BW_40_OFFSET_BIT   1
#define BW_80_OFFSET_BIT   2
#define BW_160_OFFSET_BIT  3
typedef struct sme_tdls_chan_switch_params_struct {
	uint32_t vdev_id;
	tSirMacAddr peer_mac_addr;
	uint16_t tdls_off_ch_bw_offset;/* Target Off Channel Bandwidth offset */
	uint8_t tdls_off_channel;      /* Target Off Channel */
	uint8_t tdls_off_ch_mode;      /* TDLS Off Channel Mode */
	uint8_t is_responder;          /* is peer responder or initiator */
	uint8_t opclass;           /* tdls operating class */
} sme_tdls_chan_switch_params;
#endif /* FEATURE_WLAN_TDLS */

/* Thermal Mitigation*/
typedef struct {
	uint16_t smeMinTempThreshold;
	uint16_t smeMaxTempThreshold;
} tSmeThermalLevelInfo;

#define SME_MAX_THERMAL_LEVELS (4)
typedef struct {
	/* Array of thermal levels */
	tSmeThermalLevelInfo smeThermalLevels[SME_MAX_THERMAL_LEVELS];
	uint8_t smeThermalMgmtEnabled;
	uint32_t smeThrottlePeriod;
} tSmeThermalParams;

typedef enum {
	SME_AC_BK = 0,
	SME_AC_BE = 1,
	SME_AC_VI = 2,
	SME_AC_VO = 3
} sme_ac_enum_type;

/* TSPEC Direction Enum Type */
typedef enum {
	/* uplink */
	SME_TX_DIR = 0,
	/* downlink */
	SME_RX_DIR = 1,
	/* bidirectional */
	SME_BI_DIR = 2,
} sme_tspec_dir_type;

/*-------------------------------------------------------------------------
  Function declarations and documenation
  ------------------------------------------------------------------------*/
CDF_STATUS sme_open(tHalHandle hHal);
CDF_STATUS sme_init_chan_list(tHalHandle hal, uint8_t *alpha2,
		COUNTRY_CODE_SOURCE cc_src);
CDF_STATUS sme_close(tHalHandle hHal);
CDF_STATUS sme_start(tHalHandle hHal);
CDF_STATUS sme_stop(tHalHandle hHal, tHalStopType stopType);
CDF_STATUS sme_open_session(tHalHandle hHal, csr_roam_completeCallback callback,
		void *pContext, uint8_t *pSelfMacAddr,
		uint8_t *pbSessionId, uint32_t type,
		uint32_t subType);
void sme_set_curr_device_mode(tHalHandle hHal, tCDF_CON_MODE currDeviceMode);
CDF_STATUS sme_close_session(tHalHandle hHal, uint8_t sessionId,
		csr_roamSessionCloseCallback callback,
		void *pContext);
CDF_STATUS sme_update_roam_params(tHalHandle hHal, uint8_t session_id,
		struct roam_ext_params roam_params_src, int update_param);
#ifdef FEATURE_WLAN_SCAN_PNO
void sme_update_roam_pno_channel_prediction_config(
		tHalHandle hal, tpSmeConfigParams sme_config,
		uint8_t copy_from_to);
#else
static inline void sme_update_roam_pno_channel_prediction_config(
		tHalHandle hal, tpSmeConfigParams sme_config,
		uint8_t copy_from_to)
{}
#endif
CDF_STATUS sme_update_config(tHalHandle hHal,
		tpSmeConfigParams pSmeConfigParams);

#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
CDF_STATUS sme_set_plm_request(tHalHandle hHal, tpSirPlmReq pPlm);
#endif

CDF_STATUS sme_set11dinfo(tHalHandle hHal, tpSmeConfigParams pSmeConfigParams);
CDF_STATUS sme_get_soft_ap_domain(tHalHandle hHal,
		v_REGDOMAIN_t *domainIdSoftAp);
CDF_STATUS sme_set_reg_info(tHalHandle hHal, uint8_t *apCntryCode);
CDF_STATUS sme_change_config_params(tHalHandle hHal,
		tCsrUpdateConfigParam *pUpdateConfigParam);
CDF_STATUS sme_hdd_ready_ind(tHalHandle hHal);
CDF_STATUS sme_process_msg(tHalHandle hHal, cds_msg_t *pMsg);
void sme_free_msg(tHalHandle hHal, cds_msg_t *pMsg);
CDF_STATUS sme_scan_request(tHalHandle hHal, uint8_t sessionId,
		tCsrScanRequest *, csr_scan_completeCallback callback,
		void *pContext);
CDF_STATUS sme_scan_get_result(tHalHandle hHal, uint8_t sessionId,
		tCsrScanResultFilter *pFilter,
		tScanResultHandle *phResult);
CDF_STATUS sme_get_ap_channel_from_scan_cache(tHalHandle hHal,
		tCsrRoamProfile *profile,
		tScanResultHandle *scan_cache,
		uint8_t *ap_chnl_id);
bool sme_store_joinreq_param(tHalHandle hal_handle,
		tCsrRoamProfile *profile,
		tScanResultHandle scan_cache,
		uint32_t *roam_id,
		uint32_t session_id);
bool sme_clear_joinreq_param(tHalHandle hal_handle,
		uint32_t session_id);
CDF_STATUS sme_issue_stored_joinreq(tHalHandle hal_handle,
		uint32_t *roam_id,
		uint32_t session_id);
CDF_STATUS sme_scan_flush_result(tHalHandle hHal);
CDF_STATUS sme_filter_scan_results(tHalHandle hHal, uint8_t sessionId);
CDF_STATUS sme_scan_flush_p2p_result(tHalHandle hHal, uint8_t sessionId);
tCsrScanResultInfo *sme_scan_result_get_first(tHalHandle,
		tScanResultHandle hScanResult);
tCsrScanResultInfo *sme_scan_result_get_next(tHalHandle,
		tScanResultHandle hScanResult);
CDF_STATUS sme_scan_result_purge(tHalHandle hHal,
		tScanResultHandle hScanResult);
CDF_STATUS sme_scan_get_pmkid_candidate_list(tHalHandle hHal, uint8_t sessionId,
		tPmkidCandidateInfo *pPmkidList,
		uint32_t *pNumItems);
CDF_STATUS sme_roam_connect(tHalHandle hHal, uint8_t sessionId,
		tCsrRoamProfile *pProfile, uint32_t *pRoamId);
CDF_STATUS sme_roam_reassoc(tHalHandle hHal, uint8_t sessionId,
		tCsrRoamProfile *pProfile,
		tCsrRoamModifyProfileFields modProfileFields,
		uint32_t *pRoamId, bool fForce);
CDF_STATUS sme_roam_connect_to_last_profile(tHalHandle hHal, uint8_t sessionId);
CDF_STATUS sme_roam_disconnect(tHalHandle hHal, uint8_t sessionId,
		eCsrRoamDisconnectReason reason);
CDF_STATUS sme_roam_stop_bss(tHalHandle hHal, uint8_t sessionId);
CDF_STATUS sme_roam_get_associated_stas(tHalHandle hHal, uint8_t sessionId,
		CDF_MODULE_ID modId, void *pUsrContext,
		void *pfnSapEventCallback,
		uint8_t *pAssocStasBuf);
CDF_STATUS sme_roam_disconnect_sta(tHalHandle hHal, uint8_t sessionId,
		const uint8_t *pPeerMacAddr);
CDF_STATUS sme_roam_deauth_sta(tHalHandle hHal, uint8_t sessionId,
		struct tagCsrDelStaParams *pDelStaParams);
CDF_STATUS sme_roam_tkip_counter_measures(tHalHandle hHal, uint8_t sessionId,
		bool bEnable);
CDF_STATUS sme_roam_get_wps_session_overlap(tHalHandle hHal, uint8_t sessionId,
		void *pUsrContext,
		void *pfnSapEventCallback,
		struct cdf_mac_addr pRemoveMac);
CDF_STATUS sme_roam_get_connect_state(tHalHandle hHal, uint8_t sessionId,
		eCsrConnectState *pState);
CDF_STATUS sme_roam_get_connect_profile(tHalHandle hHal, uint8_t sessionId,
		tCsrRoamConnectedProfile *pProfile);
CDF_STATUS sme_roam_free_connect_profile(tHalHandle hHal,
		tCsrRoamConnectedProfile *pProfile);
CDF_STATUS sme_roam_set_pmkid_cache(tHalHandle hHal, uint8_t sessionId,
		tPmkidCacheInfo *pPMKIDCache,
		uint32_t numItems,
		bool update_entire_cache);

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
CDF_STATUS sme_roam_set_psk_pmk(tHalHandle hHal, uint8_t sessionId,
		uint8_t *pPSK_PMK, size_t pmk_len);
#endif
CDF_STATUS sme_roam_get_security_req_ie(tHalHandle hHal, uint8_t sessionId,
		uint32_t *pLen, uint8_t *pBuf,
		eCsrSecurityType secType);
CDF_STATUS sme_roam_get_security_rsp_ie(tHalHandle hHal, uint8_t sessionId,
		uint32_t *pLen, uint8_t *pBuf,
		eCsrSecurityType secType);
uint32_t sme_roam_get_num_pmkid_cache(tHalHandle hHal, uint8_t sessionId);
CDF_STATUS sme_roam_get_pmkid_cache(tHalHandle hHal, uint8_t sessionId,
		uint32_t *pNum,
		tPmkidCacheInfo *pPmkidCache);
CDF_STATUS sme_get_config_param(tHalHandle hHal, tSmeConfigParams *pParam);
CDF_STATUS sme_get_statistics(tHalHandle hHal,
		eCsrStatsRequesterType requesterId,
		uint32_t statsMask, tCsrStatsCallback callback,
		uint32_t periodicity, bool cache, uint8_t staId,
		void *pContext, uint8_t sessionId);
CDF_STATUS sme_get_rssi(tHalHandle hHal,
		tCsrRssiCallback callback,
		uint8_t staId, struct cdf_mac_addr bssId, int8_t lastRSSI,
		void *pContext, void *p_cds_context);
CDF_STATUS sme_get_snr(tHalHandle hHal,
		tCsrSnrCallback callback,
		uint8_t staId, struct cdf_mac_addr bssId, void *pContext);
#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
CDF_STATUS sme_get_tsm_stats(tHalHandle hHal,
		tCsrTsmStatsCallback callback,
		uint8_t staId, struct cdf_mac_addr bssId,
		void *pContext, void *p_cds_context, uint8_t tid);
CDF_STATUS sme_set_cckm_ie(tHalHandle hHal,
		uint8_t sessionId,
		uint8_t *pCckmIe, uint8_t cckmIeLen);
CDF_STATUS sme_set_ese_beacon_request(tHalHandle hHal, const uint8_t sessionId,
		const tCsrEseBeaconReq *pEseBcnReq);
#endif /*FEATURE_WLAN_ESE && FEATURE_WLAN_ESE_UPLOAD */
CDF_STATUS sme_cfg_set_int(tHalHandle hal, uint16_t cfg_id, uint32_t value);
CDF_STATUS sme_cfg_set_str(tHalHandle hal, uint16_t cfg_id, uint8_t *str,
		uint32_t length);
CDF_STATUS sme_cfg_get_int(tHalHandle hal, uint16_t cfg_id,
		uint32_t *cfg_value);
CDF_STATUS sme_cfg_get_str(tHalHandle hal, uint16_t cfg_id, uint8_t *str,
		uint32_t *length);
CDF_STATUS sme_get_modify_profile_fields(tHalHandle hHal, uint8_t sessionId,
					 tCsrRoamModifyProfileFields *
					 pModifyProfileFields);

extern CDF_STATUS sme_set_host_power_save(tHalHandle hHal, bool psMode);

void sme_set_dhcp_till_power_active_flag(tHalHandle hHal, uint8_t flag);
extern CDF_STATUS sme_register11d_scan_done_callback(tHalHandle hHal,
		csr_scan_completeCallback);
#ifdef FEATURE_OEM_DATA_SUPPORT
extern CDF_STATUS sme_register_oem_data_rsp_callback(tHalHandle h_hal,
		sme_send_oem_data_rsp_msg callback);
#endif

extern CDF_STATUS sme_wow_add_pattern(tHalHandle hHal,
		struct wow_add_pattern *pattern, uint8_t sessionId);
extern CDF_STATUS sme_wow_delete_pattern(tHalHandle hHal,
		struct wow_delete_pattern *pattern, uint8_t sessionId);

void sme_register_ftm_msg_processor(tHalHandle hal,
				    hdd_ftm_msg_processor callback);

extern CDF_STATUS sme_enter_wowl(tHalHandle hHal,
			 void (*enter_wowl_callback_routine)(void
						  *callbackContext,
						  CDF_STATUS  status),
			 void *enter_wowl_callback_context,
#ifdef WLAN_WAKEUP_EVENTS
			 void (*wake_reason_ind_cb)(void *callbackContext,
						 tpSirWakeReasonInd
						 wake_reason_ind),
			 void *wake_reason_ind_cb_ctx,
#endif /* WLAN_WAKEUP_EVENTS */
			 tpSirSmeWowlEnterParams wowl_enter_params,
			 uint8_t sessionId);

extern CDF_STATUS sme_exit_wowl(tHalHandle hHal,
		tpSirSmeWowlExitParams wowl_exit_params);
CDF_STATUS sme_roam_set_key(tHalHandle, uint8_t sessionId,
		tCsrRoamSetKey *pSetKey, uint32_t *pRoamId);
CDF_STATUS sme_get_country_code(tHalHandle hHal, uint8_t *pBuf, uint8_t *pbLen);


void sme_apply_channel_power_info_to_fw(tHalHandle hHal);

/* some support functions */
bool sme_is11d_supported(tHalHandle hHal);
bool sme_is11h_supported(tHalHandle hHal);
bool sme_is_wmm_supported(tHalHandle hHal);

typedef void (*tSmeChangeCountryCallback)(void *pContext);
CDF_STATUS sme_change_country_code(tHalHandle hHal,
		tSmeChangeCountryCallback callback,
		uint8_t *pCountry,
		void *pContext,
		void *p_cds_context,
		tAniBool countryFromUserSpace,
		tAniBool sendRegHint);
CDF_STATUS sme_generic_change_country_code(tHalHandle hHal,
		uint8_t *pCountry,
		v_REGDOMAIN_t reg_domain);

CDF_STATUS sme_dhcp_start_ind(tHalHandle hHal,
		uint8_t device_mode,
		uint8_t *macAddr, uint8_t sessionId);
CDF_STATUS sme_dhcp_stop_ind(tHalHandle hHal,
		uint8_t device_mode,
		uint8_t *macAddr, uint8_t sessionId);
void sme_set_cfg_privacy(tHalHandle hHal, tCsrRoamProfile *pProfile,
		bool fPrivacy);
void sme_get_recovery_stats(tHalHandle hHal);
#if defined WLAN_FEATURE_VOWIFI
CDF_STATUS sme_neighbor_report_request(tHalHandle hHal, uint8_t sessionId,
		tpRrmNeighborReq pRrmNeighborReq,
		tpRrmNeighborRspCallbackInfo callbackInfo);
#endif
CDF_STATUS sme_get_wcnss_wlan_compiled_version(tHalHandle hHal,
		tSirVersionType * pVersion);
CDF_STATUS sme_get_wcnss_wlan_reported_version(tHalHandle hHal,
		tSirVersionType *pVersion);
CDF_STATUS sme_get_wcnss_software_version(tHalHandle hHal,
		uint8_t *pVersion, uint32_t versionBufferSize);
CDF_STATUS sme_get_wcnss_hardware_version(tHalHandle hHal,
		uint8_t *pVersion, uint32_t versionBufferSize);
#ifdef FEATURE_WLAN_WAPI
CDF_STATUS sme_scan_get_bkid_candidate_list(tHalHandle hHal, uint32_t sessionId,
		tBkidCandidateInfo * pBkidList,
		uint32_t *pNumItems);
#endif /* FEATURE_WLAN_WAPI */
#ifdef FEATURE_OEM_DATA_SUPPORT
CDF_STATUS sme_oem_data_req(tHalHandle hHal,
		uint8_t sessionId,
		tOemDataReqConfig *,
		uint32_t *pOemDataReqID,
		oem_data_oem_data_reqCompleteCallback callback,
		void *pContext);
#endif /*FEATURE_OEM_DATA_SUPPORT */
CDF_STATUS sme_roam_update_apwpsie(tHalHandle, uint8_t sessionId,
		tSirAPWPSIEs * pAPWPSIES);
CDF_STATUS sme_roam_update_apwparsni_es(tHalHandle hHal, uint8_t sessionId,
		tSirRSNie *pAPSirRSNie);
CDF_STATUS sme_change_mcc_beacon_interval(tHalHandle hHal, uint8_t sessionId);
CDF_STATUS sme_set_host_offload(tHalHandle hHal, uint8_t sessionId,
		tpSirHostOffloadReq pRequest);
CDF_STATUS sme_set_keep_alive(tHalHandle hHal, uint8_t sessionId,
		tpSirKeepAliveReq pRequest);
CDF_STATUS sme_get_operation_channel(tHalHandle hHal, uint32_t *pChannel,
		uint8_t sessionId);
CDF_STATUS sme_register_mgmt_frame(tHalHandle hHal, uint8_t sessionId,
		uint16_t frameType, uint8_t *matchData,
		uint16_t matchLen);
CDF_STATUS sme_deregister_mgmt_frame(tHalHandle hHal, uint8_t sessionId,
		uint16_t frameType, uint8_t *matchData,
		uint16_t matchLen);
CDF_STATUS sme_configure_rxp_filter(tHalHandle hHal,
		tpSirWlanSetRxpFilters wlanRxpFilterParam);
CDF_STATUS sme_ConfigureAppsCpuWakeupState(tHalHandle hHal, bool isAppsAwake);
CDF_STATUS sme_configure_suspend_ind(tHalHandle hHal,
		tpSirWlanSuspendParam wlanSuspendParam,
		csr_readyToSuspendCallback,
		void *callbackContext);
CDF_STATUS sme_configure_resume_req(tHalHandle hHal,
		tpSirWlanResumeParam wlanResumeParam);
#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
CDF_STATUS sme_configure_ext_wow(tHalHandle hHal,
		tpSirExtWoWParams wlanExtParams,
		csr_readyToSuspendCallback callback,
		void *callbackContext);
CDF_STATUS sme_configure_app_type1_params(tHalHandle hHal,
		tpSirAppType1Params wlanAppType1Params);
CDF_STATUS sme_configure_app_type2_params(tHalHandle hHal,
		tpSirAppType2Params wlanAppType2Params);
#endif
int8_t sme_get_infra_session_id(tHalHandle hHal);
uint8_t sme_get_infra_operation_channel(tHalHandle hHal, uint8_t sessionId);
uint8_t sme_get_concurrent_operation_channel(tHalHandle hHal);
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
uint16_t sme_check_concurrent_channel_overlap(tHalHandle hHal, uint16_t sap_ch,
		eCsrPhyMode sapPhyMode,
		uint8_t cc_switch_mode);
#endif
CDF_STATUS sme_abort_mac_scan(tHalHandle hHal, uint8_t sessionId,
		eCsrAbortReason reason);
CDF_STATUS sme_get_cfg_valid_channels(tHalHandle hHal, uint8_t *aValidChannels,
		uint32_t *len);
#ifdef FEATURE_WLAN_SCAN_PNO
CDF_STATUS sme_set_preferred_network_list(tHalHandle hHal,
		tpSirPNOScanReq pRequest,
		uint8_t sessionId,
		preferred_network_found_ind_cb
		callbackRoutine, void *callbackContext);

CDF_STATUS sme_preferred_network_found_ind(tHalHandle hHal, void *pMsg);
#endif /* FEATURE_WLAN_SCAN_PNO */
#ifdef WLAN_FEATURE_PACKET_FILTERING
CDF_STATUS sme_8023_multicast_list(tHalHandle hHal, uint8_t sessionId,
		tpSirRcvFltMcAddrList pMulticastAddrs);
CDF_STATUS sme_receive_filter_set_filter(tHalHandle hHal,
		tpSirRcvPktFilterCfgType pRcvPktFilterCfg,
		uint8_t sessionId);
CDF_STATUS sme_receive_filter_clear_filter(tHalHandle hHal,
		tpSirRcvFltPktClearParam pRcvFltPktClearParam,
		uint8_t sessionId);
#endif /* WLAN_FEATURE_PACKET_FILTERING */
bool sme_is_channel_valid(tHalHandle hHal, uint8_t channel);
CDF_STATUS sme_set_freq_band(tHalHandle hHal, uint8_t sessionId,
		eCsrBand eBand);
CDF_STATUS sme_get_freq_band(tHalHandle hHal, eCsrBand *pBand);
#ifdef WLAN_FEATURE_GTK_OFFLOAD
CDF_STATUS sme_set_gtk_offload(tHalHandle hal_ctx,
		tpSirGtkOffloadParams request,
		uint8_t session_id);
CDF_STATUS sme_get_gtk_offload(tHalHandle hal_ctx,
		gtk_offload_get_info_callback callback_routine,
		void *callback_context, uint8_t session_id);
#endif /* WLAN_FEATURE_GTK_OFFLOAD */
uint16_t sme_chn_to_freq(uint8_t chanNum);
bool sme_is_channel_valid(tHalHandle hHal, uint8_t channel);
CDF_STATUS sme_set_max_tx_power(tHalHandle hHal, struct cdf_mac_addr pBssid,
		struct cdf_mac_addr pSelfMacAddress, int8_t dB);
CDF_STATUS sme_set_max_tx_power_per_band(eCsrBand band, int8_t db);
CDF_STATUS sme_set_tx_power(tHalHandle hHal, uint8_t sessionId,
		struct cdf_mac_addr bssid,
		tCDF_CON_MODE dev_mode, int power);
CDF_STATUS sme_set_custom_mac_addr(tSirMacAddr customMacAddr);
CDF_STATUS sme_hide_ssid(tHalHandle hHal, uint8_t sessionId,
		uint8_t ssidHidden);
CDF_STATUS sme_set_tm_level(tHalHandle hHal, uint16_t newTMLevel,
		uint16_t tmMode);
void sme_feature_caps_exchange(tHalHandle hHal);
void sme_disable_feature_capablity(uint8_t feature_index);
void sme_reset_power_values_for5_g(tHalHandle hHal);
#if  defined(WLAN_FEATURE_VOWIFI_11R) || defined(FEATURE_WLAN_ESE) || \
		defined(FEATURE_WLAN_LFR)
CDF_STATUS sme_update_roam_prefer5_g_hz(tHalHandle hHal, bool nRoamPrefer5GHz);
CDF_STATUS sme_set_roam_intra_band(tHalHandle hHal, const bool nRoamIntraBand);
CDF_STATUS sme_update_roam_scan_n_probes(tHalHandle hHal, uint8_t sessionId,
		const uint8_t nProbes);
CDF_STATUS sme_update_roam_scan_home_away_time(tHalHandle hHal,
		uint8_t sessionId,
		const uint16_t nRoamScanHomeAwayTime,
		const bool bSendOffloadCmd);

bool sme_get_roam_intra_band(tHalHandle hHal);
uint8_t sme_get_roam_scan_n_probes(tHalHandle hHal);
uint16_t sme_get_roam_scan_home_away_time(tHalHandle hHal);
CDF_STATUS sme_update_roam_rssi_diff(tHalHandle hHal, uint8_t sessionId,
		uint8_t RoamRssiDiff);
CDF_STATUS sme_update_fast_transition_enabled(tHalHandle hHal,
		bool isFastTransitionEnabled);
CDF_STATUS sme_update_wes_mode(tHalHandle hHal, bool isWESModeEnabled,
		uint8_t sessionId);
CDF_STATUS sme_set_roam_scan_control(tHalHandle hHal, uint8_t sessionId,
		bool roamScanControl);
#endif /* (WLAN_FEATURE_VOWIFI_11R)||(FEATURE_WLAN_ESE)||(FEATURE_WLAN_LFR) */

#ifdef FEATURE_WLAN_LFR
CDF_STATUS sme_update_is_fast_roam_ini_feature_enabled(tHalHandle hHal,
		uint8_t sessionId,
		const bool
		isFastRoamIniFeatureEnabled);
CDF_STATUS sme_update_is_mawc_ini_feature_enabled(tHalHandle hHal,
		const bool MAWCEnabled);
CDF_STATUS sme_stop_roaming(tHalHandle hHal, uint8_t sessionId, uint8_t reason);
CDF_STATUS sme_start_roaming(tHalHandle hHal, uint8_t sessionId,
		uint8_t reason);
CDF_STATUS sme_update_enable_fast_roam_in_concurrency(tHalHandle hHal,
		bool bFastRoamInConIniFeatureEnabled);
#endif /* FEATURE_WLAN_LFR */
#ifdef FEATURE_WLAN_ESE
CDF_STATUS sme_update_is_ese_feature_enabled(tHalHandle hHal, uint8_t sessionId,
		const bool isEseIniFeatureEnabled);
#endif /* FEATURE_WLAN_ESE */
CDF_STATUS sme_update_config_fw_rssi_monitoring(tHalHandle hHal,
		bool fEnableFwRssiMonitoring);
#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
CDF_STATUS sme_set_roam_rescan_rssi_diff(tHalHandle hHal,
		uint8_t sessionId,
		const uint8_t nRoamRescanRssiDiff);
uint8_t sme_get_roam_rescan_rssi_diff(tHalHandle hHal);

CDF_STATUS sme_set_roam_opportunistic_scan_threshold_diff(tHalHandle hHal,
		uint8_t sessionId,
		const uint8_t nOpportunisticThresholdDiff);
uint8_t sme_get_roam_opportunistic_scan_threshold_diff(tHalHandle hHal);
CDF_STATUS sme_set_neighbor_lookup_rssi_threshold(tHalHandle hHal,
		uint8_t sessionId, uint8_t neighborLookupRssiThreshold);
CDF_STATUS sme_set_delay_before_vdev_stop(tHalHandle hHal,
		uint8_t sessionId, uint8_t delay_before_vdev_stop);
uint8_t sme_get_neighbor_lookup_rssi_threshold(tHalHandle hHal);
CDF_STATUS sme_set_neighbor_scan_refresh_period(tHalHandle hHal,
		uint8_t sessionId, uint16_t neighborScanResultsRefreshPeriod);
uint16_t sme_get_neighbor_scan_refresh_period(tHalHandle hHal);
uint16_t sme_get_empty_scan_refresh_period(tHalHandle hHal);
CDF_STATUS sme_update_empty_scan_refresh_period(tHalHandle hHal,
		uint8_t sessionId, uint16_t nEmptyScanRefreshPeriod);
CDF_STATUS sme_set_neighbor_scan_min_chan_time(tHalHandle hHal,
		const uint16_t nNeighborScanMinChanTime,
		uint8_t sessionId);
CDF_STATUS sme_set_neighbor_scan_max_chan_time(tHalHandle hHal,
		uint8_t sessionId, const uint16_t nNeighborScanMaxChanTime);
uint16_t sme_get_neighbor_scan_min_chan_time(tHalHandle hHal,
		uint8_t sessionId);
uint32_t sme_get_neighbor_roam_state(tHalHandle hHal, uint8_t sessionId);
uint32_t sme_get_current_roam_state(tHalHandle hHal, uint8_t sessionId);
uint32_t sme_get_current_roam_sub_state(tHalHandle hHal, uint8_t sessionId);
uint32_t sme_get_lim_sme_state(tHalHandle hHal);
uint32_t sme_get_lim_mlm_state(tHalHandle hHal);
bool sme_is_lim_session_valid(tHalHandle hHal, uint8_t sessionId);
uint32_t sme_get_lim_sme_session_state(tHalHandle hHal, uint8_t sessionId);
uint32_t sme_get_lim_mlm_session_state(tHalHandle hHal, uint8_t sessionId);
uint16_t sme_get_neighbor_scan_max_chan_time(tHalHandle hHal,
		uint8_t sessionId);
CDF_STATUS sme_set_neighbor_scan_period(tHalHandle hHal, uint8_t sessionId,
		const uint16_t nNeighborScanPeriod);
uint16_t sme_get_neighbor_scan_period(tHalHandle hHal, uint8_t sessionId);
CDF_STATUS sme_set_roam_bmiss_first_bcnt(tHalHandle hHal,
		uint8_t sessionId, const uint8_t nRoamBmissFirstBcnt);
uint8_t sme_get_roam_bmiss_first_bcnt(tHalHandle hHal);
CDF_STATUS sme_set_roam_bmiss_final_bcnt(tHalHandle hHal, uint8_t sessionId,
		const uint8_t nRoamBmissFinalBcnt);
uint8_t sme_get_roam_bmiss_final_bcnt(tHalHandle hHal);
CDF_STATUS sme_set_roam_beacon_rssi_weight(tHalHandle hHal, uint8_t sessionId,
		const uint8_t nRoamBeaconRssiWeight);
uint8_t sme_get_roam_beacon_rssi_weight(tHalHandle hHal);
#endif
#if  defined(WLAN_FEATURE_VOWIFI_11R) || defined(FEATURE_WLAN_ESE) || \
		defined(FEATURE_WLAN_LFR)
uint8_t sme_get_roam_rssi_diff(tHalHandle hHal);
CDF_STATUS sme_change_roam_scan_channel_list(tHalHandle hHal, uint8_t sessionId,
		uint8_t *pChannelList,
		uint8_t numChannels);
#ifdef FEATURE_WLAN_ESE_UPLOAD
CDF_STATUS sme_set_ese_roam_scan_channel_list(tHalHandle hHal,
		uint8_t sessionId, uint8_t *pChannelList,
		uint8_t numChannels);
#endif
CDF_STATUS sme_get_roam_scan_channel_list(tHalHandle hHal,
		uint8_t *pChannelList, uint8_t *pNumChannels,
		uint8_t sessionId);
bool sme_get_is_ese_feature_enabled(tHalHandle hHal);
bool sme_get_wes_mode(tHalHandle hHal);
bool sme_get_roam_scan_control(tHalHandle hHal);
bool sme_get_is_lfr_feature_enabled(tHalHandle hHal);
bool sme_get_is_ft_feature_enabled(tHalHandle hHal);
#endif
CDF_STATUS sme_update_roam_scan_offload_enabled(tHalHandle hHal,
		bool nRoamScanOffloadEnabled);
uint8_t sme_is_feature_supported_by_fw(uint8_t featEnumValue);
#ifdef FEATURE_WLAN_TDLS
CDF_STATUS sme_send_tdls_link_establish_params(tHalHandle hHal,
		uint8_t sessionId,
		const tSirMacAddr peerMac,
		tCsrTdlsLinkEstablishParams *
		tdlsLinkEstablishParams);
CDF_STATUS sme_send_tdls_mgmt_frame(tHalHandle hHal, uint8_t sessionId,
		const tSirMacAddr peerMac, uint8_t frame_type,
		uint8_t dialog, uint16_t status,
		uint32_t peerCapability, uint8_t *buf,
		uint8_t len, uint8_t responder);
CDF_STATUS sme_change_tdls_peer_sta(tHalHandle hHal, uint8_t sessionId,
		const tSirMacAddr peerMac,
		tCsrStaParams *pstaParams);
CDF_STATUS sme_add_tdls_peer_sta(tHalHandle hHal, uint8_t sessionId,
		const tSirMacAddr peerMac);
CDF_STATUS sme_delete_tdls_peer_sta(tHalHandle hHal, uint8_t sessionId,
		const tSirMacAddr peerMac);
void sme_set_tdls_power_save_prohibited(tHalHandle hHal, uint32_t sessionId,
		bool val);
CDF_STATUS sme_send_tdls_chan_switch_req(
		tHalHandle hal,
		sme_tdls_chan_switch_params *ch_switch_params);
#endif

/*
 * SME API to enable/disable WLAN driver initiated SSR
 */
void sme_update_enable_ssr(tHalHandle hHal, bool enableSSR);
CDF_STATUS sme_set_phy_mode(tHalHandle hHal, eCsrPhyMode phyMode);
eCsrPhyMode sme_get_phy_mode(tHalHandle hHal);
/*
 * SME API to determine the channel bonding mode
 */
CDF_STATUS sme_set_ch_params(tHalHandle hHal, eCsrPhyMode eCsrPhyMode,
		uint8_t channel, uint8_t ht_sec_ch, chan_params_t *ch_params);
CDF_STATUS sme_handoff_request(tHalHandle hHal, uint8_t sessionId,
		tCsrHandoffRequest *pHandoffInfo);
CDF_STATUS sme_is_sta_p2p_client_connected(tHalHandle hHal);
#ifdef FEATURE_WLAN_LPHB
CDF_STATUS sme_lphb_config_req(tHalHandle hHal,
		tSirLPHBReq * lphdReq,
		void (*pCallbackfn)(void *pHddCtx,
			tSirLPHBInd * indParam));
#endif /* FEATURE_WLAN_LPHB */
CDF_STATUS sme_add_periodic_tx_ptrn(tHalHandle hHal, tSirAddPeriodicTxPtrn
		*addPeriodicTxPtrnParams);
CDF_STATUS sme_del_periodic_tx_ptrn(tHalHandle hHal, tSirDelPeriodicTxPtrn
		*delPeriodicTxPtrnParams);
void sme_enable_disable_split_scan(tHalHandle hHal, uint8_t nNumStaChan,
		uint8_t nNumP2PChan);
CDF_STATUS sme_send_rate_update_ind(tHalHandle hHal,
		tSirRateUpdateInd *rateUpdateParams);
CDF_STATUS sme_roam_del_pmkid_from_cache(tHalHandle hHal, uint8_t sessionId,
		const uint8_t *pBSSId, bool flush_cache);
void sme_get_command_q_status(tHalHandle hHal);

/*
 * SME API to enable/disable idle mode powersave
 * This should be called only if powersave offload
 * is enabled
 */
CDF_STATUS sme_set_idle_powersave_config(void *cds_context,
		tHalHandle hHal, uint32_t value);
CDF_STATUS sme_notify_modem_power_state(tHalHandle hHal, uint32_t value);

/*SME API to convert convert the ini value to the ENUM used in csr and MAC*/
ePhyChanBondState sme_get_cb_phy_state_from_cb_ini_value(uint32_t cb_ini_value);
int sme_update_ht_config(tHalHandle hHal, uint8_t sessionId, uint16_t htCapab,
		int value);
int16_t sme_get_ht_config(tHalHandle hHal, uint8_t session_id,
		uint16_t ht_capab);
#ifdef QCA_HT_2040_COEX
CDF_STATUS sme_notify_ht2040_mode(tHalHandle hHal, uint16_t staId,
		struct cdf_mac_addr macAddrSTA,
		uint8_t sessionId,
		uint8_t channel_type);
CDF_STATUS sme_set_ht2040_mode(tHalHandle hHal, uint8_t sessionId,
		uint8_t channel_type, bool obssEnabled);
#endif
CDF_STATUS sme_get_reg_info(tHalHandle hHal, uint8_t chanId,
		uint32_t *regInfo1, uint32_t *regInfo2);
#ifdef FEATURE_WLAN_TDLS
CDF_STATUS sme_update_fw_tdls_state(tHalHandle hHal, void *psmeTdlsParams,
		bool useSmeLock);
CDF_STATUS sme_update_tdls_peer_state(tHalHandle hHal,
		tSmeTdlsPeerStateParams *pPeerStateParams);
#endif /* FEATURE_WLAN_TDLS */
#ifdef FEATURE_WLAN_CH_AVOID
CDF_STATUS sme_add_ch_avoid_callback(tHalHandle hHal,
	void (*pCallbackfn)(void *hdd_context, void *indi_param));
CDF_STATUS sme_ch_avoid_update_req(tHalHandle hHal);
#else
static inline
CDF_STATUS sme_add_ch_avoid_callback(tHalHandle hHal,
	void (*pCallbackfn)(void *hdd_context, void *indi_param))
{
	return CDF_STATUS_E_NOSUPPORT;
}

static inline
CDF_STATUS sme_ch_avoid_update_req(tHalHandle hHal)
{
	return CDF_STATUS_E_NOSUPPORT;
}
#endif /* FEATURE_WLAN_CH_AVOID */
#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
CDF_STATUS sme_set_auto_shutdown_cb(tHalHandle hHal, void (*pCallbackfn)(void));
CDF_STATUS sme_set_auto_shutdown_timer(tHalHandle hHal, uint32_t timer_value);
#endif
CDF_STATUS sme_roam_channel_change_req(tHalHandle hHal,
		struct cdf_mac_addr bssid, uint32_t cb_mode,
		tCsrRoamProfile *profile);
CDF_STATUS sme_roam_start_beacon_req(tHalHandle hHal,
		struct cdf_mac_addr bssid, uint8_t dfsCacWaitStatus);
CDF_STATUS sme_roam_csa_ie_request(tHalHandle hHal, struct cdf_mac_addr bssid,
		uint8_t targetChannel, uint8_t csaIeReqd, uint8_t ch_bandwidth);
CDF_STATUS sme_init_thermal_info(tHalHandle hHal,
		tSmeThermalParams thermalParam);
CDF_STATUS sme_set_thermal_level(tHalHandle hHal, uint8_t level);
CDF_STATUS sme_txpower_limit(tHalHandle hHal, tSirTxPowerLimit *psmetx);
CDF_STATUS sme_get_link_speed(tHalHandle hHal, tSirLinkSpeedInfo *lsReq,
		void *plsContext,
		void (*pCallbackfn)(tSirLinkSpeedInfo *indParam,
		void *pContext));
CDF_STATUS sme_modify_add_ie(tHalHandle hHal,
		tSirModifyIE *pModifyIE, eUpdateIEsType updateType);
CDF_STATUS sme_update_add_ie(tHalHandle hHal,
		tSirUpdateIE *pUpdateIE, eUpdateIEsType updateType);
CDF_STATUS sme_update_connect_debug(tHalHandle hHal, uint32_t set_value);
CDF_STATUS sme_ap_disable_intra_bss_fwd(tHalHandle hHal, uint8_t sessionId,
		bool disablefwd);
uint32_t sme_get_channel_bonding_mode5_g(tHalHandle hHal);
uint32_t sme_get_channel_bonding_mode24_g(tHalHandle hHal);
#ifdef WLAN_FEATURE_STATS_EXT
typedef struct sStatsExtRequestReq {
	uint32_t request_data_len;
	uint8_t *request_data;
} tStatsExtRequestReq, *tpStatsExtRequestReq;
typedef void (*StatsExtCallback)(void *, tStatsExtEvent *);
void sme_stats_ext_register_callback(tHalHandle hHal,
		StatsExtCallback callback);
CDF_STATUS sme_stats_ext_request(uint8_t session_id,
		tpStatsExtRequestReq input);
CDF_STATUS sme_stats_ext_event(tHalHandle hHal, void *pMsg);
#endif
CDF_STATUS sme_update_dfs_scan_mode(tHalHandle hHal,
		uint8_t sessionId,
		uint8_t allowDFSChannelRoam);
uint8_t sme_get_dfs_scan_mode(tHalHandle hHal);
bool sme_sta_in_middle_of_roaming(tHalHandle hHal, uint8_t sessionId);

#ifdef FEATURE_WLAN_EXTSCAN
CDF_STATUS sme_get_valid_channels_by_band(tHalHandle hHal, uint8_t wifiBand,
		uint32_t *aValidChannels,
		uint8_t *pNumChannels);
CDF_STATUS sme_ext_scan_get_capabilities(tHalHandle hHal,
		tSirGetExtScanCapabilitiesReqParams *pReq);
CDF_STATUS sme_ext_scan_start(tHalHandle hHal,
		tSirWifiScanCmdReqParams *pStartCmd);
CDF_STATUS sme_ext_scan_stop(tHalHandle hHal,
		tSirExtScanStopReqParams *pStopReq);
CDF_STATUS sme_set_bss_hotlist(tHalHandle hHal,
		tSirExtScanSetBssidHotListReqParams *
		pSetHotListReq);
CDF_STATUS sme_reset_bss_hotlist(tHalHandle hHal,
		tSirExtScanResetBssidHotlistReqParams *
		pResetReq);
CDF_STATUS sme_set_significant_change(tHalHandle hHal,
		tSirExtScanSetSigChangeReqParams *
		pSetSignificantChangeReq);
CDF_STATUS sme_reset_significant_change(tHalHandle hHal,
		tSirExtScanResetSignificantChangeReqParams
		*pResetReq);
CDF_STATUS sme_get_cached_results(tHalHandle hHal,
		tSirExtScanGetCachedResultsReqParams *
		pCachedResultsReq);

CDF_STATUS sme_set_epno_list(tHalHandle hal,
			     struct wifi_epno_params *req_msg);
CDF_STATUS sme_set_passpoint_list(tHalHandle hal,
					struct wifi_passpoint_req *req_msg);
CDF_STATUS sme_reset_passpoint_list(tHalHandle hal,
					struct wifi_passpoint_req *req_msg);
CDF_STATUS
sme_set_ssid_hotlist(tHalHandle hal,
		     struct sir_set_ssid_hotlist_request *request);

CDF_STATUS sme_ext_scan_register_callback(tHalHandle hHal,
		void (*pExtScanIndCb)(void *, const uint16_t, void *));
#endif /* FEATURE_WLAN_EXTSCAN */
CDF_STATUS sme_abort_roam_scan(tHalHandle hHal, uint8_t sessionId);
#ifdef WLAN_FEATURE_LINK_LAYER_STATS
CDF_STATUS sme_ll_stats_clear_req(tHalHandle hHal,
		tSirLLStatsClearReq * pclearStatsReq);
CDF_STATUS sme_ll_stats_set_req(tHalHandle hHal,
		tSirLLStatsSetReq *psetStatsReq);
CDF_STATUS sme_ll_stats_get_req(tHalHandle hHal,
		tSirLLStatsGetReq *pgetStatsReq);
CDF_STATUS sme_set_link_layer_stats_ind_cb(tHalHandle hHal,
		void (*callbackRoutine)(void *callbackCtx,
				int indType, void *pRsp));
#endif /* WLAN_FEATURE_LINK_LAYER_STATS */

CDF_STATUS sme_fw_mem_dump(tHalHandle hHal, void *recvd_req);
CDF_STATUS sme_fw_mem_dump_register_cb(tHalHandle hHal,
		void (*callback_routine)(void *cb_context,
		struct fw_dump_rsp *rsp));
CDF_STATUS sme_fw_mem_dump_unregister_cb(tHalHandle hHal);

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
CDF_STATUS sme_update_roam_offload_enabled(tHalHandle hHal,
		bool nRoamOffloadEnabled);
CDF_STATUS sme_update_roam_key_mgmt_offload_enabled(tHalHandle hHal,
		uint8_t sessionId,
		bool nRoamKeyMgmtOffloadEnabled);
#endif
#ifdef WLAN_FEATURE_NAN
CDF_STATUS sme_nan_event(tHalHandle hHal, void *pMsg);
#endif /* WLAN_FEATURE_NAN */
CDF_STATUS sme_get_link_status(tHalHandle hHal,
		tCsrLinkStatusCallback callback,
		void *pContext, uint8_t sessionId);
CDF_STATUS sme_get_temperature(tHalHandle hHal,
		void *tempContext,
		void (*pCallbackfn)(int temperature,
			void *pContext));
CDF_STATUS sme_set_scanning_mac_oui(tHalHandle hHal,
		tSirScanMacOui *pScanMacOui);

#ifdef IPA_OFFLOAD
/* ---------------------------------------------------------------------------
    \fn sme_ipa_offload_enable_disable
    \brief  API to enable/disable IPA offload
    \param  hHal - The handle returned by macOpen.
    \param  sessionId - Session Identifier
    \param  pRequest -  Pointer to the offload request.
    \return eHalStatus
  ---------------------------------------------------------------------------*/
CDF_STATUS sme_ipa_offload_enable_disable(tHalHandle hal,
				uint8_t session_id,
				struct sir_ipa_offload_enable_disable *request);
#else
static inline CDF_STATUS sme_ipa_offload_enable_disable(tHalHandle hal,
				uint8_t session_id,
				struct sir_ipa_offload_enable_disable *request)
{
	return CDF_STATUS_SUCCESS;
}
#endif /* IPA_OFFLOAD */

#ifdef DHCP_SERVER_OFFLOAD
CDF_STATUS sme_set_dhcp_srv_offload(tHalHandle hHal,
		tSirDhcpSrvOffloadInfo * pDhcpSrvInfo);
#endif /* DHCP_SERVER_OFFLOAD */
#ifdef WLAN_FEATURE_GPIO_LED_FLASHING
CDF_STATUS sme_set_led_flashing(tHalHandle hHal, uint8_t type,
		uint32_t x0, uint32_t x1);
#endif
CDF_STATUS sme_handle_dfs_chan_scan(tHalHandle hHal, uint8_t dfs_flag);
CDF_STATUS sme_set_mas(uint32_t val);
CDF_STATUS sme_set_miracast(tHalHandle hal, uint8_t filter_type);
CDF_STATUS sme_ext_change_channel(tHalHandle hHal, uint32_t channel,
					  uint8_t session_id);

CDF_STATUS sme_configure_modulated_dtim(tHalHandle hal, uint8_t session_id,
				      uint32_t modulated_dtim);

CDF_STATUS sme_configure_stats_avg_factor(tHalHandle hal, uint8_t session_id,
					  uint16_t stats_avg_factor);

CDF_STATUS sme_configure_guard_time(tHalHandle hal, uint8_t session_id,
				    uint32_t guard_time);

CDF_STATUS sme_wifi_start_logger(tHalHandle hal,
		struct sir_wifi_start_log start_log);

bool sme_neighbor_middle_of_roaming(tHalHandle hHal,
						uint8_t sessionId);

CDF_STATUS sme_enable_uapsd_for_ac(void *cds_ctx, uint8_t sta_id,
				      sme_ac_enum_type ac, uint8_t tid,
				      uint8_t pri, uint32_t srvc_int,
				      uint32_t sus_int,
				      sme_tspec_dir_type dir,
				      uint8_t psb, uint32_t sessionId,
				      uint32_t delay_interval);

CDF_STATUS sme_disable_uapsd_for_ac(void *cds_ctx, uint8_t sta_id,
				       sme_ac_enum_type ac,
				       uint32_t sessionId);

CDF_STATUS sme_set_rssi_monitoring(tHalHandle hal,
					struct rssi_monitor_req *input);
CDF_STATUS sme_set_rssi_threshold_breached_cb(tHalHandle hal,
			void (*cb)(void *, struct rssi_breach_event *));

CDF_STATUS sme_update_nss(tHalHandle h_hal, uint8_t nss);

bool sme_is_any_session_in_connected_state(tHalHandle h_hal);

CDF_STATUS sme_soc_set_pcl(tHalHandle hal,
		struct sir_pcl_list msg);
CDF_STATUS sme_soc_set_hw_mode(tHalHandle hal,
		struct sir_hw_mode msg);
void sme_register_hw_mode_trans_cb(tHalHandle hal,
		hw_mode_transition_cb callback);
CDF_STATUS sme_nss_update_request(tHalHandle hHal, uint32_t vdev_id,
				uint8_t  new_nss, void *cback,
				uint8_t next_action, void *hdd_context);

typedef void (*sme_peer_authorized_fp) (uint32_t vdev_id);
CDF_STATUS sme_set_peer_authorized(uint8_t *peer_addr,
				   sme_peer_authorized_fp auth_fp,
				   uint32_t vdev_id);
CDF_STATUS sme_soc_set_dual_mac_config(tHalHandle hal,
		struct sir_dual_mac_config msg);

void sme_set_scan_disable(tHalHandle h_hal, int value);
void sme_setdef_dot11mode(tHalHandle hal);

CDF_STATUS sme_disable_non_fcc_channel(tHalHandle hHal,
				       bool fcc_constraint);

CDF_STATUS sme_update_roam_scan_hi_rssi_scan_params(tHalHandle hal_handle,
	uint8_t session_id,
	uint32_t notify_id,
	int32_t val);

void wlan_sap_enable_phy_error_logs(tHalHandle hal, bool enable_log);
void sme_set_dot11p_config(tHalHandle hal, bool enable_dot11p);

CDF_STATUS sme_ocb_set_config(tHalHandle hHal, void *context,
			      ocb_callback callback,
			      struct sir_ocb_config *config);

CDF_STATUS sme_ocb_set_utc_time(tHalHandle hHal, struct sir_ocb_utc *utc);

CDF_STATUS sme_ocb_start_timing_advert(tHalHandle hHal,
	struct sir_ocb_timing_advert *timing_advert);

CDF_STATUS sme_ocb_stop_timing_advert(tHalHandle hHal,
	struct sir_ocb_timing_advert *timing_advert);

int sme_ocb_gen_timing_advert_frame(tHalHandle hHal, tSirMacAddr self_addr,
				    uint8_t **buf, uint32_t *timestamp_offset,
				    uint32_t *time_value_offset);

CDF_STATUS sme_ocb_get_tsf_timer(tHalHandle hHal, void *context,
				 ocb_callback callback,
				 struct sir_ocb_get_tsf_timer *request);

CDF_STATUS sme_dcc_get_stats(tHalHandle hHal, void *context,
			     ocb_callback callback,
			     struct sir_dcc_get_stats *request);

CDF_STATUS sme_dcc_clear_stats(tHalHandle hHal, uint32_t vdev_id,
			       uint32_t dcc_stats_bitmap);

CDF_STATUS sme_dcc_update_ndl(tHalHandle hHal, void *context,
			      ocb_callback callback,
			      struct sir_dcc_update_ndl *request);

CDF_STATUS sme_register_for_dcc_stats_event(tHalHandle hHal, void *context,
					    ocb_callback callback);
void sme_add_set_thermal_level_callback(tHalHandle hal,
		sme_set_thermal_level_callback callback);

void sme_update_tgt_services(tHalHandle hal, struct wma_tgt_services *cfg);
bool sme_validate_sap_channel_switch(tHalHandle hal,
		uint16_t sap_ch, eCsrPhyMode sap_phy_mode,
		uint8_t cc_switch_mode, uint8_t session_id);

#ifdef FEATURE_WLAN_TDLS
void sme_get_opclass(tHalHandle hal, uint8_t channel, uint8_t bw_offset,
		uint8_t *opclass);
#else
static inline void
sme_get_opclass(tHalHandle hal, uint8_t channel, uint8_t bw_offset,
		uint8_t *opclass)
{
}
#endif

#endif /* #if !defined( __SME_API_H ) */
