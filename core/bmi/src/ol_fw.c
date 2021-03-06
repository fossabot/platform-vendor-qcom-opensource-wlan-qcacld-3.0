/*
 * Copyright (c) 2014-2015 The Linux Foundation. All rights reserved.
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

#include <linux/firmware.h>
#include "ol_if_athvar.h"
#include "targaddrs.h"
#include "ol_cfg.h"
#include "cds_api.h"
#include "wma_api.h"
#include "wma.h"
#include "bin_sig.h"
#include "i_ar6320v2_regtable.h"
#include "epping_main.h"
#include "ce_reg.h"
#if  defined(CONFIG_CNSS)
#include <net/cnss.h>
#endif

#include "i_bmi.h"
#include "qwlan_version.h"
#include "cds_concurrency.h"

#ifdef FEATURE_SECURE_FIRMWARE
static struct hash_fw fw_hash;
#endif

static uint32_t refclk_speed_to_hz[] = {
	48000000,               /* SOC_REFCLK_48_MHZ */
	19200000,               /* SOC_REFCLK_19_2_MHZ */
	24000000,               /* SOC_REFCLK_24_MHZ */
	26000000,               /* SOC_REFCLK_26_MHZ */
	37400000,               /* SOC_REFCLK_37_4_MHZ */
	38400000,               /* SOC_REFCLK_38_4_MHZ */
	40000000,               /* SOC_REFCLK_40_MHZ */
	52000000,               /* SOC_REFCLK_52_MHZ */
};

static int ol_target_coredump(void *inst, void *memory_block,
					uint32_t block_len);
#ifdef FEATURE_SECURE_FIRMWARE
static int ol_check_fw_hash(const u8 *data, u32 fw_size, ATH_BIN_FILE file)
{
	u8 *hash = NULL;
#ifdef CONFIG_CNSS
	u8 *fw_mem = NULL;
	u8 digest[SHA256_DIGEST_SIZE];
#endif
	u8 temp[SHA256_DIGEST_SIZE] = { };
	int ret = 0;

	switch (file) {
	case ATH_BOARD_DATA_FILE:
		hash = fw_hash.bdwlan;
		break;
	case ATH_OTP_FILE:
		hash = fw_hash.otp;
		break;
	case ATH_FIRMWARE_FILE:
#ifdef QCA_WIFI_FTM
		if (cds_get_conparam() == CDF_FTM_MODE) {
			hash = fw_hash.utf;
			break;
		}
#endif
		hash = fw_hash.qwlan;
	default:
		break;
	}

	if (!hash) {
		BMI_INFO("No entry for file:%d Download FW in non-secure mode",
									file);
		goto end;
	}

	if (!cdf_mem_compare(hash, temp, SHA256_DIGEST_SIZE)) {
		BMI_INFO("Download FW in non-secure mode:%d", file);
		goto end;
	}

#ifdef CONFIG_CNSS
	fw_mem = (u8 *)cnss_get_fw_ptr();
	if (!fw_mem || (fw_size > MAX_FIRMWARE_SIZE)) {
		BMI_ERR("No Memory to copy FW data");
		ret = -1;
		goto end;
	}
	cdf_mem_copy(fw_mem, data, fw_size);

	ret = cnss_get_sha_hash(fw_mem, fw_size, "sha256", digest);

	if (ret) {
		BMI_ERR("Sha256 Hash computation failed err:%d", ret);
		goto end;
	}

	if (cdf_mem_compare(hash, digest, SHA256_DIGEST_SIZE) != 0) {
		BMI_ERR("Hash Mismatch");
		cdf_trace_hex_dump(CDF_MODULE_ID_CDF, CDF_TRACE_LEVEL_FATAL,
				   digest, SHA256_DIGEST_SIZE);
		cdf_trace_hex_dump(CDF_MODULE_ID_CDF, CDF_TRACE_LEVEL_FATAL,
				   hash, SHA256_DIGEST_SIZE);
		ret = CDF_STATUS_E_FAILURE;
	}
#endif
end:
	return ret;
}
#endif

static int __ol_transfer_bin_file(struct ol_softc *scn, ATH_BIN_FILE file,
				  uint32_t address, bool compressed)
{
	int status = EOK;
	const char *filename = NULL;
	const struct firmware *fw_entry;
	uint32_t fw_entry_size;
	uint8_t *temp_eeprom;
	uint32_t board_data_size;
#ifdef QCA_SIGNED_SPLIT_BINARY_SUPPORT
	bool bin_sign = false;
	int bin_off, bin_len;
	SIGN_HEADER_T *sign_header;
#endif

	switch (file) {
	default:
		BMI_ERR("%s: Unknown file type", __func__);
		return -1;
	case ATH_OTP_FILE:
#if defined(CONFIG_CNSS)
		filename = scn->fw_files.otp_data;
#else
		filename = QCA_OTP_FILE;
#endif
#ifdef QCA_SIGNED_SPLIT_BINARY_SUPPORT
		bin_sign = true;
#endif
		break;
	case ATH_FIRMWARE_FILE:
		if (WLAN_IS_EPPING_ENABLED(cds_get_conparam())) {
#if defined(CONFIG_CNSS)
			filename = scn->fw_files.epping_file;
#else
			filename = QCA_FIRMWARE_EPPING_FILE;
#endif
			BMI_INFO("%s: Loading epping firmware file %s",
						__func__, filename);
			break;
		}
#ifdef QCA_WIFI_FTM
		if (cds_get_conparam() == CDF_FTM_MODE) {
#if defined(CONFIG_CNSS)
			filename = scn->fw_files.utf_file;
#else
			filename = QCA_UTF_FIRMWARE_FILE;
#endif
#ifdef QCA_SIGNED_SPLIT_BINARY_SUPPORT
			bin_sign = true;
#endif
			BMI_INFO("%s: Loading firmware file %s",
						__func__, filename);
			break;
		}
#endif
#if defined(CONFIG_CNSS)
		filename = scn->fw_files.image_file;
#else
		filename = QCA_FIRMWARE_FILE;
#endif
#ifdef QCA_SIGNED_SPLIT_BINARY_SUPPORT
		bin_sign = true;
#endif
		break;
	case ATH_PATCH_FILE:
		BMI_INFO("%s: no Patch file defined", __func__);
		return 0;
	case ATH_BOARD_DATA_FILE:
#ifdef QCA_WIFI_FTM
		if (cds_get_conparam() == CDF_FTM_MODE) {
#if defined(CONFIG_CNSS)
			filename = scn->fw_files.utf_board_data;
#else
			filename = QCA_BOARD_DATA_FILE;
#endif
#ifdef QCA_SIGNED_SPLIT_BINARY_SUPPORT
			bin_sign = true;
#endif
			BMI_INFO("%s: Loading board data file %s",
						__func__, filename);
			break;
		}
#endif /* QCA_WIFI_FTM */
#if defined(CONFIG_CNSS)
		filename = scn->fw_files.board_data;
#else
		filename = QCA_BOARD_DATA_FILE;
#endif
#ifdef QCA_SIGNED_SPLIT_BINARY_SUPPORT
		bin_sign = false;
#endif
		break;
	case ATH_SETUP_FILE:
		if (cds_get_conparam() != CDF_FTM_MODE &&
		    !WLAN_IS_EPPING_ENABLED(cds_get_conparam())) {
#ifdef CONFIG_CNSS
			BMI_INFO("%s: no Setup file defined", __func__);
			return -1;
#else
			filename = QCA_SETUP_FILE;
#ifdef QCA_SIGNED_SPLIT_BINARY_SUPPORT
			bin_sign = true;
#endif
			BMI_INFO("%s: Loading setup file %s",
			       __func__, filename);
#endif /* CONFIG_CNSS */
		} else {
			BMI_INFO("%s: no Setup file needed", __func__);
			return -1;
		}
		break;
	}

	if (request_firmware(&fw_entry, filename, scn->aps_osdev.device) != 0) {
		BMI_ERR("%s: Failed to get %s", __func__, filename);

		if (file == ATH_OTP_FILE)
			return -ENOENT;

#if defined(QCA_WIFI_FTM) && defined(CONFIG_CNSS)
		/* Try default board data file if FTM specific
		 * board data file is not present. */
		if (filename == scn->fw_files.utf_board_data) {
			filename = scn->fw_files.board_data;
			BMI_INFO("%s: Trying to load default %s",
			       __func__, filename);
			if (request_firmware(&fw_entry, filename,
					     scn->aps_osdev.device) != 0) {
				BMI_ERR("%s: Failed to get %s",
				       __func__, filename);
				return -1;
			}
		} else {
			return -1;
		}
#else
		return -1;
#endif
	}

	if (!fw_entry || !fw_entry->data) {
		BMI_ERR("Invalid fw_entries");
		return CDF_STATUS_E_FAILURE;
	}

	fw_entry_size = fw_entry->size;
	temp_eeprom = NULL;

#ifdef FEATURE_SECURE_FIRMWARE

	if (ol_check_fw_hash(fw_entry->data, fw_entry_size, file)) {
		BMI_ERR("Hash Check failed for file:%s", filename);
		status = CDF_STATUS_E_FAILURE;
		goto end;
	}
#endif

	if (file == ATH_BOARD_DATA_FILE) {
		uint32_t board_ext_address;
		int32_t board_ext_data_size;

		temp_eeprom = cdf_mem_malloc(fw_entry_size);
		if (!temp_eeprom) {
			BMI_ERR("%s: Memory allocation failed", __func__);
			release_firmware(fw_entry);
			return CDF_STATUS_E_NOMEM;
		}

		cdf_mem_copy(temp_eeprom, (uint8_t *) fw_entry->data,
			  fw_entry_size);

		switch (scn->target_type) {
		default:
			board_data_size = 0;
			board_ext_data_size = 0;
			break;
		case TARGET_TYPE_AR6004:
			board_data_size = AR6004_BOARD_DATA_SZ;
			board_ext_data_size = AR6004_BOARD_EXT_DATA_SZ;
		case TARGET_TYPE_AR9888:
			board_data_size = AR9888_BOARD_DATA_SZ;
			board_ext_data_size = AR9888_BOARD_EXT_DATA_SZ;
			break;
		}

		/* Determine where in Target RAM to write Board Data */
		bmi_read_memory(HOST_INTEREST_ITEM_ADDRESS(scn->target_type,
							   hi_board_ext_data),
				(uint8_t *) &board_ext_address, 4, scn);
		BMI_INFO("Board extended Data download address: 0x%x",
		       board_ext_address);

		/* Check whether the target has allocated memory for extended
		 * board data and file contains extended board data
		 */

		if ((board_ext_address)
		    && (fw_entry_size ==
			(board_data_size + board_ext_data_size))) {
			uint32_t param;

			status = bmi_write_memory(board_ext_address,
					(uint8_t *)(temp_eeprom +
					board_data_size),
					board_ext_data_size, scn);

			if (status != EOK)
				goto end;

			/* Record extended board Data initialized */
			param = (board_ext_data_size << 16) | 1;
			bmi_write_memory(
				HOST_INTEREST_ITEM_ADDRESS(scn->target_type,
					hi_board_ext_data_config),
					(uint8_t *)&param, 4, scn);

			fw_entry_size = board_data_size;
		}
	}
#ifdef QCA_SIGNED_SPLIT_BINARY_SUPPORT
	if (bin_sign) {
		uint32_t chip_id;

		if (fw_entry_size < sizeof(SIGN_HEADER_T)) {
			BMI_ERR("Invalid binary size %d", fw_entry_size);
			status = CDF_STATUS_E_FAILURE;
			goto end;
		}

		sign_header = (SIGN_HEADER_T *) fw_entry->data;
		chip_id = cpu_to_le32(sign_header->product_id);
		if (sign_header->magic_num == SIGN_HEADER_MAGIC
		    && (chip_id == AR6320_REV1_1_VERSION
			|| chip_id == AR6320_REV1_3_VERSION
			|| chip_id == AR6320_REV2_1_VERSION)) {

			status = bmi_sign_stream_start(address,
						(uint8_t *)fw_entry->data,
						sizeof(SIGN_HEADER_T), scn);
			if (status != EOK) {
				BMI_ERR("unable to start sign stream");
				status = CDF_STATUS_E_FAILURE;
				goto end;
			}

			bin_off = sizeof(SIGN_HEADER_T);
			bin_len = sign_header->rampatch_len
				  - sizeof(SIGN_HEADER_T);
		} else {
			bin_sign = false;
			bin_off = 0;
			bin_len = fw_entry_size;
		}
	} else {
		bin_len = fw_entry_size;
		bin_off = 0;
	}

	if (compressed) {
		status = bmi_fast_download(address,
					   (uint8_t *) fw_entry->data + bin_off,
					   bin_len, scn);
	} else {
		if (file == ATH_BOARD_DATA_FILE && fw_entry->data) {
			status = bmi_write_memory(address,
						  (uint8_t *) temp_eeprom,
						  fw_entry_size, scn);
		} else {
			status = bmi_write_memory(address,
						  (uint8_t *) fw_entry->data
						  + bin_off, bin_len, scn);
		}
	}

	if (bin_sign) {
		bin_off += bin_len;
		bin_len = sign_header->total_len - sign_header->rampatch_len;

		if (bin_len > 0) {
			status = bmi_sign_stream_start(0,
					(uint8_t *)fw_entry->data +
					bin_off, bin_len, scn);
			if (status != EOK)
				BMI_ERR("sign stream error");
		}
	}
#else
	if (compressed) {
		status = bmi_fast_download(address,
					   (uint8_t *) fw_entry->data,
					   fw_entry_size, scn);
	} else {
		if (file == ATH_BOARD_DATA_FILE && fw_entry->data) {
			status = bmi_write_memory(address,
						  (uint8_t *) temp_eeprom,
						  fw_entry_size, scn);
		} else {
			status = bmi_write_memory(address,
						  (uint8_t *) fw_entry->data,
						  fw_entry_size, scn);
		}
	}
#endif /* QCA_SIGNED_SPLIT_BINARY_SUPPORT */

end:
	if (temp_eeprom)
		cdf_mem_free(temp_eeprom);

	if (status != EOK) {
		BMI_ERR("%s, BMI operation failed: %d", __func__, __LINE__);
		release_firmware(fw_entry);
		return CDF_STATUS_E_FAILURE;
	}

	release_firmware(fw_entry);

	BMI_INFO("transferring file: %s size %d bytes done!",
		(filename != NULL) ? filename : " ", fw_entry_size);

	return status;
}

static int ol_transfer_bin_file(struct ol_softc *scn, ATH_BIN_FILE file,
				uint32_t address, bool compressed)
{
	int ret;

#ifdef CONFIG_CNSS
	/* Wait until suspend and resume are completed before loading FW */
	cnss_lock_pm_sem();
#endif

	ret = __ol_transfer_bin_file(scn, file, address, compressed);

#ifdef CONFIG_CNSS
	cnss_release_pm_sem();
#endif

	return ret;
}

int dump_ce_register(struct ol_softc *scn)
{
	uint32_t ce_reg_address = CE0_BASE_ADDRESS;
	uint32_t ce_reg_values[8][CE_USEFUL_SIZE >> 2];
	uint32_t ce_reg_word_size = CE_USEFUL_SIZE >> 2;
	uint16_t i, j;

	for (i = 0; i < 8; i++, ce_reg_address += CE_OFFSET) {
		if (hif_diag_read_mem(scn, ce_reg_address,
			(uint8_t *) &ce_reg_values[i][0], ce_reg_word_size *
			sizeof(uint32_t)) != CDF_STATUS_SUCCESS) {
			BMI_ERR("Dumping CE register failed!");
			return -EACCES;
		}
	}

	for (i = 0; i < 8; i++) {
		BMI_ERR("CE%d Registers:", i);
		for (j = 0; j < ce_reg_word_size; j++) {
			BMI_ERR("0x%08x ", ce_reg_values[i][j]);
			if (!((j + 1) % 5) || (ce_reg_word_size - 1) == j)
				BMI_ERR(" ");
		}
	}

	return 0;
}

#if  defined(CONFIG_CNSS)

static struct ol_softc *ramdump_scn;

int ol_copy_ramdump(struct ol_softc *scn)
{
	int ret;

	if (!scn->ramdump_base || !scn->ramdump_size) {
		BMI_ERR("%s:ramdump collection fail", __func__);
		ret = -EACCES;
		goto out;
	}

	ret = ol_target_coredump(scn, scn->ramdump_base, scn->ramdump_size);

out:
	return ret;
}

static void ramdump_work_handler(struct work_struct *ramdump)
{
	int ret;
	uint32_t host_interest_address;
	uint32_t dram_dump_values[4];

	if (!ramdump_scn) {
		BMI_ERR("%s:Ramdump_scn is null:", __func__);
		goto out_fail;
	}
#ifdef DEBUG
	ret = hif_check_soc_status(ramdump_scn);
	if (ret)
		goto out_fail;

	ret = dump_ce_register(ramdump_scn);
	if (ret)
		goto out_fail;

	dump_ce_debug_register(ramdump_scn);
#endif

	if (hif_diag_read_mem(ramdump_scn,
			hif_hia_item_address(ramdump_scn->target_type,
			offsetof(struct host_interest_s, hi_failure_state)),
			(uint8_t *)&host_interest_address,
			sizeof(uint32_t)) != CDF_STATUS_SUCCESS) {
		BMI_ERR("HifDiagReadiMem FW Dump Area Pointer failed!");
		ol_copy_ramdump(ramdump_scn);
		cnss_device_crashed();
		return;
	}

	BMI_ERR("Host interest item address: 0x%08x", host_interest_address);

	if (hif_diag_read_mem(ramdump_scn, host_interest_address,
			      (uint8_t *) &dram_dump_values[0],
			      4 * sizeof(uint32_t)) != CDF_STATUS_SUCCESS) {
		BMI_ERR("HifDiagReadiMem FW Dump Area failed!");
		goto out_fail;
	}
	BMI_ERR("FW Assertion at PC: 0x%08x BadVA: 0x%08x TargetID: 0x%08x",
	       dram_dump_values[2], dram_dump_values[3], dram_dump_values[0]);

	if (ol_copy_ramdump(ramdump_scn))
		goto out_fail;

	BMI_ERR("%s: RAM dump collecting completed!", __func__);
	/* notify SSR framework the target has crashed. */
	cnss_device_crashed();
	return;

out_fail:
	/* Silent SSR on dump failure */
#ifdef CNSS_SELF_RECOVERY
	cnss_device_self_recovery();
#else
	cnss_device_crashed();
#endif
	return;
}

static DECLARE_WORK(ramdump_work, ramdump_work_handler);

void ol_schedule_ramdump_work(struct ol_softc *scn)
{
	ramdump_scn = scn;
	schedule_work(&ramdump_work);
}

static void fw_indication_work_handler(struct work_struct *fw_indication)
{
	cnss_device_self_recovery();
}

static DECLARE_WORK(fw_indication_work, fw_indication_work_handler);

void ol_schedule_fw_indication_work(struct ol_softc *scn)
{
	schedule_work(&fw_indication_work);
}
#endif

void ol_target_failure(void *instance, CDF_STATUS status)
{
	struct ol_softc *scn = (struct ol_softc *)instance;
	tp_wma_handle wma = cds_get_context(CDF_MODULE_ID_WMA);
	int ret;

	cdf_event_set(&wma->recovery_event);

	if (OL_TRGET_STATUS_RESET == scn->target_status) {
		BMI_ERR("Target is already asserted, ignore!");
		return;
	}
	scn->target_status = OL_TRGET_STATUS_RESET;

	if (cds_is_logp_in_progress()) {
		BMI_ERR("%s: LOGP is in progress, ignore!\n", __func__);
		return;
	}

	if (cds_is_load_unload_in_progress()) {
		BMI_ERR("%s: Loading/Unloading is in progress, ignore!",
		       __func__);
		return;
	}
	cds_set_logp_in_progress(true);

#ifdef CONFIG_CNSS
	ret = hif_check_fw_reg(scn);
	if (0 == ret) {
		if (scn->enable_self_recovery) {
			ol_schedule_fw_indication_work(scn);
			return;
		}
	} else if (-1 == ret) {
		return;
	}
#endif

	BMI_ERR("XXX TARGET ASSERTED XXX");

#if  defined(CONFIG_CNSS)
	/* Collect the RAM dump through a workqueue */
	if (scn->enable_ramdump_collection)
		ol_schedule_ramdump_work(scn);
	else
		pr_debug("%s: athdiag read for target reg\n", __func__);
#endif

	return;
}

CDF_STATUS ol_configure_target(struct ol_softc *scn)
{
	uint32_t param;
#ifdef CONFIG_CNSS
	struct cnss_platform_cap cap;
	int ret;
#endif

	/* Tell target which HTC version it is used */
	param = HTC_PROTOCOL_VERSION;
	if (bmi_write_memory(
		hif_hia_item_address(scn->target_type,
		offsetof(struct host_interest_s, hi_app_host_interest)),
		(uint8_t *) &param, 4, scn) != CDF_STATUS_SUCCESS) {
		BMI_ERR("bmi_write_memory for htc version failed");
		return CDF_STATUS_E_FAILURE;
	}

	/* set the firmware mode to STA/IBSS/AP */
	{
		if (bmi_read_memory(hif_hia_item_address(scn->target_type,
			offsetof(struct host_interest_s, hi_option_flag)),
			(uint8_t *)&param, 4, scn) != CDF_STATUS_SUCCESS) {
			BMI_ERR("bmi_read_memory for setting fwmode failed");
			return CDF_STATUS_E_FAILURE;
		}

		/* TODO following parameters need to be re-visited. */
		param |= (1 << HI_OPTION_NUM_DEV_SHIFT); /* num_device */
		/* Firmware mode ?? */
		param |= (HI_OPTION_FW_MODE_AP << HI_OPTION_FW_MODE_SHIFT);
		/* mac_addr_method */
		param |= (1 << HI_OPTION_MAC_ADDR_METHOD_SHIFT);
		/* firmware_bridge */
		param |= (0 << HI_OPTION_FW_BRIDGE_SHIFT);
		/* fwsubmode */
		param |= (0 << HI_OPTION_FW_SUBMODE_SHIFT);

		BMI_INFO("NUM_DEV=%d FWMODE=0x%x FWSUBMODE=0x%x FWBR_BUF %d",
		       1, HI_OPTION_FW_MODE_AP, 0, 0);

		if (bmi_write_memory(
			hif_hia_item_address(scn->target_type,
			offsetof(struct host_interest_s, hi_option_flag)),
			(uint8_t *)&param, 4, scn) != CDF_STATUS_SUCCESS) {
			BMI_ERR("BMI WRITE for setting fwmode failed");
			return CDF_STATUS_E_FAILURE;
		}
	}

#if (CONFIG_DISABLE_CDC_MAX_PERF_WAR)
	{
		/* set the firmware to disable CDC max perf WAR */
		if (bmi_read_memory(hif_hia_item_address(scn->target_type,
			offsetof(struct host_interest_s, hi_option_flag2)),
			(uint8_t *) &param, 4, scn) != CDF_STATUS_SUCCESS) {
			BMI_ERR("BMI READ for setting cdc max perf failed");
			return CDF_STATUS_E_FAILURE;
		}

		param |= HI_OPTION_DISABLE_CDC_MAX_PERF_WAR;
		if (bmi_write_memory(
			hif_hia_item_address(scn->target_type,
			offsetof(struct host_interest_s, hi_option_flag2)),
			(uint8_t *)&param, 4, scn) != CDF_STATUS_SUCCESS) {
			BMI_ERR("setting cdc max perf failed");
			return CDF_STATUS_E_FAILURE;
		}
	}
#endif /* CONFIG_CDC_MAX_PERF_WAR */

#ifdef CONFIG_CNSS

	ret = cnss_get_platform_cap(&cap);
	if (ret)
		BMI_ERR("platform capability info from CNSS not available");

	if (!ret && cap.cap_flag & CNSS_HAS_EXTERNAL_SWREG) {
		if (bmi_read_memory(hif_hia_item_address(scn->target_type,
			offsetof(struct host_interest_s, hi_option_flag2)),
			(uint8_t *)&param, 4, scn) != CDF_STATUS_SUCCESS) {
			BMI_ERR("bmi_read_memory for setting"
				"external SWREG failed");
			return CDF_STATUS_E_FAILURE;
		}

		param |= HI_OPTION_USE_EXT_LDO;
		if (bmi_write_memory(
			hif_hia_item_address(scn->target_type,
			offsetof(struct host_interest_s, hi_option_flag2)),
			(uint8_t *)&param, 4, scn) != CDF_STATUS_SUCCESS) {
			BMI_ERR("BMI WRITE for setting external SWREG fail");
			return CDF_STATUS_E_FAILURE;
		}
	}
#endif

#ifdef WLAN_FEATURE_LPSS
	if (scn->enablelpasssupport) {
		if (bmi_read_memory(hif_hia_item_address(scn->target_type,
			offsetof(struct host_interest_s, hi_option_flag2)),
			(uint8_t *) &param, 4, scn) != CDF_STATUS_SUCCESS) {
			BMI_ERR("BMI READ:Setting LPASS Support failed");
			return CDF_STATUS_E_FAILURE;
		}

		param |= HI_OPTION_DBUART_SUPPORT;
		if (bmi_write_memory(
			hif_hia_item_address(scn->target_type,
			offsetof(struct host_interest_s, hi_option_flag2)),
			(uint8_t *)&param, 4, scn) != CDF_STATUS_SUCCESS) {
			BMI_ERR("BMI_READ for setting LPASS Support fail");
			return CDF_STATUS_E_FAILURE;
		}
	}
#endif

	/* If host is running on a BE CPU, set the host interest area */
	{
#ifdef BIG_ENDIAN_HOST
		param = 1;
#else
		param = 0;
#endif
		if (bmi_write_memory(
			hif_hia_item_address(scn->target_type,
			offsetof(struct host_interest_s, hi_be)),
			(uint8_t *) &param, 4, scn) != CDF_STATUS_SUCCESS) {
			BMI_ERR("setting host CPU BE mode failed");
			return CDF_STATUS_E_FAILURE;
		}
	}

	/* FW descriptor/Data swap flags */
	param = 0;
	if (bmi_write_memory(
		hif_hia_item_address(scn->target_type,
		offsetof(struct host_interest_s, hi_fw_swap)),
		(uint8_t *) &param, 4, scn) != CDF_STATUS_SUCCESS) {
		BMI_ERR("BMI WRITE failed setting FW data/desc swap flags");
		return CDF_STATUS_E_FAILURE;
	}

	return CDF_STATUS_SUCCESS;
}

static int ol_check_dataset_patch(struct ol_softc *scn, uint32_t *address)
{
	/* Check if patch file needed for this target type/version. */
	return 0;
}


CDF_STATUS ol_fw_populate_clk_settings(A_refclk_speed_t refclk,
				     struct cmnos_clock_s *clock_s)
{
	if (!clock_s)
		return CDF_STATUS_E_FAILURE;

	switch (refclk) {
	case SOC_REFCLK_48_MHZ:
		clock_s->wlan_pll.div = 0xE;
		clock_s->wlan_pll.rnfrac = 0x2AAA8;
		clock_s->pll_settling_time = 2400;
		break;
	case SOC_REFCLK_19_2_MHZ:
		clock_s->wlan_pll.div = 0x24;
		clock_s->wlan_pll.rnfrac = 0x2AAA8;
		clock_s->pll_settling_time = 960;
		break;
	case SOC_REFCLK_24_MHZ:
		clock_s->wlan_pll.div = 0x1D;
		clock_s->wlan_pll.rnfrac = 0x15551;
		clock_s->pll_settling_time = 1200;
		break;
	case SOC_REFCLK_26_MHZ:
		clock_s->wlan_pll.div = 0x1B;
		clock_s->wlan_pll.rnfrac = 0x4EC4;
		clock_s->pll_settling_time = 1300;
		break;
	case SOC_REFCLK_37_4_MHZ:
		clock_s->wlan_pll.div = 0x12;
		clock_s->wlan_pll.rnfrac = 0x34B49;
		clock_s->pll_settling_time = 1870;
		break;
	case SOC_REFCLK_38_4_MHZ:
		clock_s->wlan_pll.div = 0x12;
		clock_s->wlan_pll.rnfrac = 0x15551;
		clock_s->pll_settling_time = 1920;
		break;
	case SOC_REFCLK_40_MHZ:
		clock_s->wlan_pll.div = 0x11;
		clock_s->wlan_pll.rnfrac = 0x26665;
		clock_s->pll_settling_time = 2000;
		break;
	case SOC_REFCLK_52_MHZ:
		clock_s->wlan_pll.div = 0x1B;
		clock_s->wlan_pll.rnfrac = 0x4EC4;
		clock_s->pll_settling_time = 2600;
		break;
	case SOC_REFCLK_UNKNOWN:
		clock_s->wlan_pll.refdiv = 0;
		clock_s->wlan_pll.div = 0;
		clock_s->wlan_pll.rnfrac = 0;
		clock_s->wlan_pll.outdiv = 0;
		clock_s->pll_settling_time = 1024;
		clock_s->refclk_hz = 0;
	default:
		return CDF_STATUS_E_FAILURE;
	}

	clock_s->refclk_hz = refclk_speed_to_hz[refclk];
	clock_s->wlan_pll.refdiv = 0;
	clock_s->wlan_pll.outdiv = 1;

	return CDF_STATUS_SUCCESS;
}

CDF_STATUS ol_patch_pll_switch(struct ol_softc *scn)
{
	CDF_STATUS status = CDF_STATUS_SUCCESS;
	uint32_t addr = 0;
	uint32_t reg_val = 0;
	uint32_t mem_val = 0;
	struct cmnos_clock_s clock_s;
	uint32_t cmnos_core_clk_div_addr = 0;
	uint32_t cmnos_cpu_pll_init_done_addr = 0;
	uint32_t cmnos_cpu_speed_addr = 0;

	switch (scn->target_version) {
	case AR6320_REV1_1_VERSION:
		cmnos_core_clk_div_addr = AR6320_CORE_CLK_DIV_ADDR;
		cmnos_cpu_pll_init_done_addr = AR6320_CPU_PLL_INIT_DONE_ADDR;
		cmnos_cpu_speed_addr = AR6320_CPU_SPEED_ADDR;
		break;
	case AR6320_REV1_3_VERSION:
	case AR6320_REV2_1_VERSION:
		cmnos_core_clk_div_addr = AR6320V2_CORE_CLK_DIV_ADDR;
		cmnos_cpu_pll_init_done_addr = AR6320V2_CPU_PLL_INIT_DONE_ADDR;
		cmnos_cpu_speed_addr = AR6320V2_CPU_SPEED_ADDR;
		break;
	case AR6320_REV3_VERSION:
	case AR6320_REV3_2_VERSION:
		cmnos_core_clk_div_addr = AR6320V3_CORE_CLK_DIV_ADDR;
		cmnos_cpu_pll_init_done_addr = AR6320V3_CPU_PLL_INIT_DONE_ADDR;
		cmnos_cpu_speed_addr = AR6320V3_CPU_SPEED_ADDR;
		break;
	default:
		BMI_ERR("%s: Unsupported target version %x", __func__,
		       scn->target_version);
		goto end;
	}

	addr = (RTC_SOC_BASE_ADDRESS | EFUSE_OFFSET);
	status = bmi_read_soc_register(addr, &reg_val, scn);
	if (status != CDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to read EFUSE Addr");
		goto end;
	}

	status = ol_fw_populate_clk_settings(EFUSE_XTAL_SEL_GET(reg_val),
					     &clock_s);
	if (status != CDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to set clock settings");
		goto end;
	}
	BMI_DBG("crystal_freq: %dHz", clock_s.refclk_hz);

	/* ------Step 1---- */
	reg_val = 0;
	addr = (RTC_SOC_BASE_ADDRESS | BB_PLL_CONFIG_OFFSET);
	status = bmi_read_soc_register(addr, &reg_val, scn);
	if (status != CDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to read PLL_CONFIG Addr");
		goto end;
	}
	BMI_DBG("Step 1a: %8X", reg_val);

	reg_val &= ~(BB_PLL_CONFIG_FRAC_MASK | BB_PLL_CONFIG_OUTDIV_MASK);
	reg_val |= (BB_PLL_CONFIG_FRAC_SET(clock_s.wlan_pll.rnfrac) |
		    BB_PLL_CONFIG_OUTDIV_SET(clock_s.wlan_pll.outdiv));
	status = bmi_write_soc_register(addr, reg_val, scn);
	if (status != CDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to write PLL_CONFIG Addr");
		goto end;
	}

	reg_val = 0;
	status = bmi_read_soc_register(addr, &reg_val, scn);
	if (status != CDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to read back PLL_CONFIG Addr");
		goto end;
	}
	BMI_DBG("Step 1b: %8X", reg_val);

	/* ------Step 2---- */
	reg_val = 0;
	addr = (RTC_WMAC_BASE_ADDRESS | WLAN_PLL_SETTLE_OFFSET);
	status = bmi_read_soc_register(addr, &reg_val, scn);
	if (status != CDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to read PLL_SETTLE Addr");
		goto end;
	}
	BMI_DBG("Step 2a: %8X", reg_val);

	reg_val &= ~WLAN_PLL_SETTLE_TIME_MASK;
	reg_val |= WLAN_PLL_SETTLE_TIME_SET(clock_s.pll_settling_time);
	status = bmi_write_soc_register(addr, reg_val, scn);
	if (status != CDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to write PLL_SETTLE Addr");
		goto end;
	}

	reg_val = 0;
	status = bmi_read_soc_register(addr, &reg_val, scn);
	if (status != CDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to read back PLL_SETTLE Addr");
		goto end;
	}
	BMI_DBG("Step 2b: %8X", reg_val);

	/* ------Step 3---- */
	reg_val = 0;
	addr = (RTC_SOC_BASE_ADDRESS | SOC_CORE_CLK_CTRL_OFFSET);
	status = bmi_read_soc_register(addr, &reg_val, scn);
	if (status != CDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to read CLK_CTRL Addr");
		goto end;
	}
	BMI_DBG("Step 3a: %8X", reg_val);

	reg_val &= ~SOC_CORE_CLK_CTRL_DIV_MASK;
	reg_val |= SOC_CORE_CLK_CTRL_DIV_SET(1);
	status = bmi_write_soc_register(addr, reg_val, scn);
	if (status != CDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to write CLK_CTRL Addr");
		goto end;
	}

	reg_val = 0;
	status = bmi_read_soc_register(addr, &reg_val, scn);
	if (status != CDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to read back CLK_CTRL Addr");
		goto end;
	}
	BMI_DBG("Step 3b: %8X", reg_val);

	/* ------Step 4----- */
	mem_val = 1;
	status = bmi_write_memory(cmnos_core_clk_div_addr,
				  (uint8_t *) &mem_val, 4, scn);
	if (status != CDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to write CLK_DIV Addr");
		goto end;
	}

	/* ------Step 5----- */
	reg_val = 0;
	addr = (RTC_WMAC_BASE_ADDRESS | WLAN_PLL_CONTROL_OFFSET);
	status = bmi_read_soc_register(addr, &reg_val, scn);
	if (status != CDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to read PLL_CTRL Addr");
		goto end;
	}
	BMI_DBG("Step 5a: %8X", reg_val);

	reg_val &= ~(WLAN_PLL_CONTROL_REFDIV_MASK | WLAN_PLL_CONTROL_DIV_MASK |
		     WLAN_PLL_CONTROL_NOPWD_MASK);
	reg_val |= (WLAN_PLL_CONTROL_REFDIV_SET(clock_s.wlan_pll.refdiv) |
		    WLAN_PLL_CONTROL_DIV_SET(clock_s.wlan_pll.div) |
		    WLAN_PLL_CONTROL_NOPWD_SET(1));
	status = bmi_write_soc_register(addr, reg_val, scn);
	if (status != CDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to write PLL_CTRL Addr");
		goto end;
	}

	reg_val = 0;
	status = bmi_read_soc_register(addr, &reg_val, scn);
	if (status != CDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to read back PLL_CTRL Addr");
		goto end;
	}
	OS_DELAY(100);
	BMI_DBG("Step 5b: %8X", reg_val);

	/* ------Step 6------- */
	do {
		reg_val = 0;
		status = bmi_read_soc_register((RTC_WMAC_BASE_ADDRESS |
				RTC_SYNC_STATUS_OFFSET), &reg_val, scn);
		if (status != CDF_STATUS_SUCCESS) {
			BMI_ERR("Failed to read RTC_SYNC_STATUS Addr");
			goto end;
		}
	} while (RTC_SYNC_STATUS_PLL_CHANGING_GET(reg_val));

	/* ------Step 7------- */
	reg_val = 0;
	addr = (RTC_WMAC_BASE_ADDRESS | WLAN_PLL_CONTROL_OFFSET);
	status = bmi_read_soc_register(addr, &reg_val, scn);
	if (status != CDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to read PLL_CTRL Addr for CTRL_BYPASS");
		goto end;
	}
	BMI_DBG("Step 7a: %8X", reg_val);

	reg_val &= ~WLAN_PLL_CONTROL_BYPASS_MASK;
	reg_val |= WLAN_PLL_CONTROL_BYPASS_SET(0);
	status = bmi_write_soc_register(addr, reg_val, scn);
	if (status != CDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to write PLL_CTRL Addr for CTRL_BYPASS");
		goto end;
	}

	reg_val = 0;
	status = bmi_read_soc_register(addr, &reg_val, scn);
	if (status != CDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to read back PLL_CTRL Addr for CTRL_BYPASS");
		goto end;
	}
	BMI_DBG("Step 7b: %8X", reg_val);

	/* ------Step 8-------- */
	do {
		reg_val = 0;
		status = bmi_read_soc_register((RTC_WMAC_BASE_ADDRESS |
				RTC_SYNC_STATUS_OFFSET), &reg_val, scn);
		if (status != CDF_STATUS_SUCCESS) {
			BMI_ERR("Failed to read SYNC_STATUS Addr");
			goto end;
		}
	} while (RTC_SYNC_STATUS_PLL_CHANGING_GET(reg_val));

	/* ------Step 9-------- */
	reg_val = 0;
	addr = (RTC_SOC_BASE_ADDRESS | SOC_CPU_CLOCK_OFFSET);
	status = bmi_read_soc_register(addr, &reg_val, scn);
	if (status != CDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to read CPU_CLK Addr");
		goto end;
	}
	BMI_DBG("Step 9a: %8X", reg_val);

	reg_val &= ~SOC_CPU_CLOCK_STANDARD_MASK;
	reg_val |= SOC_CPU_CLOCK_STANDARD_SET(1);
	status = bmi_write_soc_register(addr, reg_val, scn);
	if (status != CDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to write CPU_CLK Addr");
		goto end;
	}

	reg_val = 0;
	status = bmi_read_soc_register(addr, &reg_val, scn);
	if (status != CDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to read back CPU_CLK Addr");
		goto end;
	}
	BMI_DBG("Step 9b: %8X", reg_val);

	/* ------Step 10------- */
	reg_val = 0;
	addr = (RTC_WMAC_BASE_ADDRESS | WLAN_PLL_CONTROL_OFFSET);
	status = bmi_read_soc_register(addr, &reg_val, scn);
	if (status != CDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to read PLL_CTRL Addr for NOPWD");
		goto end;
	}
	BMI_DBG("Step 10a: %8X", reg_val);

	reg_val &= ~WLAN_PLL_CONTROL_NOPWD_MASK;
	status = bmi_write_soc_register(addr, reg_val, scn);
	if (status != CDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to write PLL_CTRL Addr for NOPWD");
		goto end;
	}
	reg_val = 0;
	status = bmi_read_soc_register(addr, &reg_val, scn);
	if (status != CDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to read back PLL_CTRL Addr for NOPWD");
		goto end;
	}
	BMI_DBG("Step 10b: %8X", reg_val);

	/* ------Step 11------- */
	mem_val = 1;
	status = bmi_write_memory(cmnos_cpu_pll_init_done_addr,
				  (uint8_t *) &mem_val, 4, scn);
	if (status != CDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to write PLL_INIT Addr");
		goto end;
	}

	mem_val = TARGET_CPU_FREQ;
	status = bmi_write_memory(cmnos_cpu_speed_addr,
				  (uint8_t *) &mem_val, 4, scn);
	if (status != CDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to write CPU_SPEED Addr");
		goto end;
	}

end:
	return status;
}

#ifdef CONFIG_CNSS
/* AXI Start Address */
#define TARGET_ADDR (0xa0000)

void ol_transfer_codeswap_struct(struct ol_softc *scn)
{
	struct codeswap_codeseg_info wlan_codeswap;
	CDF_STATUS rv;

	if (!scn) {
		BMI_ERR("%s: ol_softc is null", __func__);
		return;
	}
	if (cnss_get_codeswap_struct(&wlan_codeswap)) {
		BMI_ERR("%s: failed to get codeswap structure", __func__);
		return;
	}

	rv = bmi_write_memory(TARGET_ADDR,
			      (uint8_t *) &wlan_codeswap, sizeof(wlan_codeswap),
			      scn);

	if (rv != CDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to Write 0xa0000 to Target");
		return;
	}
	BMI_INFO("codeswap structure is successfully downloaded");
}
#endif

CDF_STATUS ol_download_firmware(struct ol_softc *scn)
{
	uint32_t param, address = 0;
	int status = !EOK;
	CDF_STATUS ret;

#ifdef CONFIG_CNSS
	if (0 != cnss_get_fw_files_for_target(&scn->fw_files,
					      scn->target_type,
					      scn->target_version)) {
		BMI_ERR("%s: No FW files from CNSS driver", __func__);
		return CDF_STATUS_E_FAILURE;
	}
#endif
	/* Transfer Board Data from Target EEPROM to Target RAM */
	/* Determine where in Target RAM to write Board Data */
	bmi_read_memory(hif_hia_item_address(scn->target_type,
			offsetof(struct host_interest_s, hi_board_data)),
			(uint8_t *)&address, 4, scn);

	if (!address) {
		address = AR6004_REV5_BOARD_DATA_ADDRESS;
		BMI_DBG("%s: Target address not known! Using 0x%x",
						__func__, address);
	}
	ret = ol_patch_pll_switch(scn);
	if (ret != CDF_STATUS_SUCCESS) {
		BMI_ERR("pll switch failed. status %d", ret);
		return ret;
	}
	if (scn->cal_in_flash) {
		/* Write EEPROM or Flash data to Target RAM */
		status = ol_transfer_bin_file(scn, ATH_FLASH_FILE,
						address, false);
	}

	if (status == EOK) {
		/* Record the fact that Board Data is initialized */
		param = 1;
		bmi_write_memory(
			hif_hia_item_address(scn->target_type,
			offsetof(struct host_interest_s,
				hi_board_data_initialized)),
				(uint8_t *) &param, 4, scn);
	} else {
		/* Flash is either not available or invalid */
		if (ol_transfer_bin_file
			    (scn, ATH_BOARD_DATA_FILE, address, false) != EOK) {
			return -1;
		}

		/* Record the fact that Board Data is initialized */
		param = 1;
		bmi_write_memory(
			hif_hia_item_address(scn->target_type,
			offsetof(struct host_interest_s,
				hi_board_data_initialized)),
				(uint8_t *) &param, 4, scn);

		/* Transfer One Time Programmable data */
		address = BMI_SEGMENTED_WRITE_ADDR;
		BMI_INFO("%s: Using 0x%x for the remainder of init",
				__func__, address);

#ifdef CONFIG_CNSS
		ol_transfer_codeswap_struct(scn);
#endif
		status = ol_transfer_bin_file(scn, ATH_OTP_FILE,
						address, true);
		/* Execute the OTP code only if entry found and downloaded */
		if (status == EOK) {
			param = 0;
#ifndef FEATURE_BMI_2
			bmi_execute(address, &param, scn);
#endif
		} else if (status < 0) {
			return status;
		}
	}

	if (ol_transfer_bin_file(scn, ATH_SETUP_FILE,
		BMI_SEGMENTED_WRITE_ADDR, true) == EOK) {
		param = 0;
#ifndef FEATURE_BMI_2
		bmi_execute(address, &param, scn);
#endif
	}

	/* Download Target firmware
	 * TODO point to target specific files in runtime
	 */
	address = BMI_SEGMENTED_WRITE_ADDR;
	if (ol_transfer_bin_file(scn, ATH_FIRMWARE_FILE,
				address, true) != EOK) {
		return -1;
	}

	/* Apply the patches */
	if (ol_check_dataset_patch(scn, &address)) {
		if ((ol_transfer_bin_file(scn, ATH_PATCH_FILE, address, false))
		    != EOK) {
			return -1;
		}
		bmi_write_memory(hif_hia_item_address(scn->target_type,
			offsetof(struct host_interest_s, hi_dset_list_head)),
			(uint8_t *) &address, 4, scn);
	}

	if (scn->enableuartprint ||
	    (WLAN_IS_EPPING_ENABLED(cds_get_conparam()) &&
	     WLAN_IS_EPPING_FW_UART(cds_get_conparam()))) {
		switch (scn->target_version) {
		case AR6004_VERSION_REV1_3:
			param = 11;
			break;
		case AR6320_REV1_VERSION:
		case AR6320_REV2_VERSION:
		case AR6320_REV3_VERSION:
		case AR6320_REV3_2_VERSION:
		case AR6320_REV4_VERSION:
		case AR6320_DEV_VERSION:
			param = 6;
			break;
		default:
			/* Configure GPIO AR9888 UART */
			param = 7;
		}

		bmi_write_memory(hif_hia_item_address(scn->target_type,
			offsetof(struct host_interest_s, hi_dbg_uart_txpin)),
			(uint8_t *)&param, 4, scn);
		param = 1;
		bmi_write_memory(hif_hia_item_address(scn->target_type,
			offsetof(struct host_interest_s, hi_serial_enable)),
			(uint8_t *)&param, 4, scn);
	} else {
		/*
		 * Explicitly setting UART prints to zero as target turns it on
		 * based on scratch registers.
		 */
		param = 0;
		bmi_write_memory(hif_hia_item_address(scn->target_type,
			offsetof(struct host_interest_s, hi_serial_enable)),
			(uint8_t *)&param, 4, scn);
	}

	if (scn->enablefwlog) {
		bmi_read_memory(hif_hia_item_address(scn->target_type,
			offsetof(struct host_interest_s, hi_option_flag)),
			(uint8_t *)&param, 4, scn);

		param &= ~(HI_OPTION_DISABLE_DBGLOG);
		bmi_write_memory(hif_hia_item_address(scn->target_type,
			offsetof(struct host_interest_s, hi_option_flag)),
			(uint8_t *)&param, 4, scn);
	} else {
		/*
		 * Explicitly setting fwlog prints to zero as target turns it on
		 * based on scratch registers.
		 */
		bmi_read_memory(hif_hia_item_address(scn->target_type,
			offsetof(struct host_interest_s, hi_option_flag)),
			(uint8_t *)&param, 4, scn);

		param |= HI_OPTION_DISABLE_DBGLOG;
		bmi_write_memory(hif_hia_item_address(scn->target_type,
			offsetof(struct host_interest_s, hi_option_flag)),
			(uint8_t *) &param, 4, scn);
	}

	return status;
}

int ol_diag_read(struct ol_softc *scn, uint8_t *buffer,
		 uint32_t pos, size_t count)
{
	int result = 0;

	if ((4 == count) && ((pos & 3) == 0)) {
		result = hif_diag_read_access(scn, pos,
					      (uint32_t *) buffer);
	} else {
		size_t amount_read = 0;
		size_t readSize = PCIE_READ_LIMIT;
		size_t remainder = 0;
		if (count > PCIE_READ_LIMIT) {
			while ((amount_read < count) && (0 == result)) {
				result = hif_diag_read_mem(scn, pos,
							   buffer, readSize);
				if (0 == result) {
					buffer += readSize;
					pos += readSize;
					amount_read += readSize;
					remainder = count - amount_read;
					if (remainder < PCIE_READ_LIMIT)
						readSize = remainder;
				}
			}
		} else {
		result = hif_diag_read_mem(scn, pos,
					   buffer, count);
		}
	}

	if (!result)
		return count;
	else
		return -EIO;
}

static int ol_ath_get_reg_table(uint32_t target_version,
				tgt_reg_table *reg_table)
{
	int section_len = 0;

	if (!reg_table) {
		cdf_assert(0);
		return section_len;
	}

	switch (target_version) {
	case AR6320_REV2_1_VERSION:
		reg_table->section =
			(tgt_reg_section *) &ar6320v2_reg_table[0];
		reg_table->section_size = sizeof(ar6320v2_reg_table)
					  / sizeof(ar6320v2_reg_table[0]);
		section_len = AR6320_REV2_1_REG_SIZE;
		break;
	case AR6320_REV3_VERSION:
	case AR6320_REV3_2_VERSION:
		reg_table->section =
			(tgt_reg_section *) &ar6320v3_reg_table[0];
		reg_table->section_size = sizeof(ar6320v3_reg_table)
					  / sizeof(ar6320v3_reg_table[0]);
		section_len = AR6320_REV3_REG_SIZE;
		break;
	default:
		reg_table->section = (void *)NULL;
		reg_table->section_size = 0;
		section_len = 0;
	}

	return section_len;
}

static int ol_diag_read_reg_loc(struct ol_softc *scn, uint8_t *buffer,
				uint32_t buffer_len)
{
	int i, len, section_len, fill_len;
	int dump_len, result = 0;
	tgt_reg_table reg_table;
	tgt_reg_section *curr_sec, *next_sec;

	section_len = ol_ath_get_reg_table(scn->target_version, &reg_table);

	if (!reg_table.section || !reg_table.section_size || !section_len) {
		BMI_ERR("%s: failed to get reg table", __func__);
		result = -EIO;
		goto out;
	}

	curr_sec = reg_table.section;
	for (i = 0; i < reg_table.section_size; i++) {

		dump_len = curr_sec->end_addr - curr_sec->start_addr;

		if ((buffer_len - result) < dump_len) {
			BMI_ERR("Not enough memory to dump the registers:"
			       " %d: 0x%08x-0x%08x", i,
			       curr_sec->start_addr, curr_sec->end_addr);
			goto out;
		}

		len = ol_diag_read(scn, buffer, curr_sec->start_addr, dump_len);

		if (len != -EIO) {
			buffer += len;
			result += len;
		} else {
			BMI_ERR("%s: can't read reg 0x%08x len = %d",
				__func__, curr_sec->start_addr, dump_len);
			result = -EIO;
			goto out;
		}

		if (result < section_len) {
			next_sec = (tgt_reg_section *) ((uint8_t *) curr_sec
							+ sizeof(*curr_sec));
			fill_len = next_sec->start_addr - curr_sec->end_addr;
			if ((buffer_len - result) < fill_len) {
				BMI_ERR("Not enough memory to fill registers:"
				       " %d: 0x%08x-0x%08x", i,
				       curr_sec->end_addr,
				       next_sec->start_addr);
				goto out;
			}

			if (fill_len) {
				buffer += fill_len;
				result += fill_len;
			}
		}
		curr_sec++;
	}

out:
	return result;
}

void ol_dump_target_memory(struct ol_softc *scn, void *memory_block)
{
	char *buffer_loc = memory_block;
	u_int32_t section_count = 0;
	u_int32_t address = 0;
	u_int32_t size = 0;

	for (; section_count < 2; section_count++) {
		switch (section_count) {
		case 0:
			address = DRAM_LOCAL_BASE_ADDR;
			size = DRAM_SIZE;
			break;
		case 1:
			address = AXI_LOCATION;
			size = AXI_SIZE;
		default:
			break;
		}
		hif_dump_target_memory(scn, buffer_loc, address, size);
		buffer_loc += size;
	}
}

/**---------------------------------------------------------------------------
*   \brief  ol_target_coredump
*
*   Function to perform core dump for the target
*
*   \param:   scn - ol_softc handler
*             memory_block - non-NULL reserved memory location
*             block_len - size of the dump to collect
*
*   \return:  None
* --------------------------------------------------------------------------*/
static int ol_target_coredump(void *inst, void *memory_block,
					uint32_t block_len)
{
	struct ol_softc *scn = (struct ol_softc *)inst;
	int8_t *buffer_loc = memory_block;
	int result = 0;
	int ret = 0;
	uint32_t amount_read = 0;
	uint32_t section_count = 0;
	uint32_t pos = 0;
	uint32_t read_len = 0;

	/*
	 * SECTION = DRAM
	 * START   = 0x00400000
	 * LENGTH  = 0x000a8000
	 *
	 * SECTION = AXI
	 * START   = 0x000a0000
	 * LENGTH  = 0x00018000
	 *
	 * SECTION = REG
	 * START   = 0x00000800
	 * LENGTH  = 0x0007F820
	 */

	while ((section_count < 3) && (amount_read < block_len)) {
		switch (section_count) {
		case 0:
			/* DRAM SECTION */
			pos = DRAM_LOCATION;
			read_len = DRAM_SIZE;
			BMI_ERR("%s: Dumping DRAM section...", __func__);
			break;
		case 1:
			/* AXI SECTION */
			pos = AXI_LOCATION;
			read_len = AXI_SIZE;
			BMI_ERR("%s: Dumping AXI section...", __func__);
			break;
		case 2:
			/* REG SECTION */
			pos = REGISTER_LOCATION;
			/* ol_diag_read_reg_loc checks for buffer overrun */
			read_len = 0;
			BMI_ERR("%s: Dumping Register section...", __func__);
			break;
		}

		if ((block_len - amount_read) >= read_len) {
			if (pos == REGISTER_LOCATION)
				result = ol_diag_read_reg_loc(scn, buffer_loc,
							      block_len -
							      amount_read);
			else
				result = ol_diag_read(scn, buffer_loc,
					      pos, read_len);
			if (result != -EIO) {
				amount_read += result;
				buffer_loc += result;
				section_count++;
			} else {
				BMI_ERR("Could not read dump section!");
				dump_ce_register(scn);
				dump_ce_debug_register(scn);
				ol_dump_target_memory(scn, memory_block);
				ret = -EACCES;
				break;  /* Could not read the section */
			}
		} else {
			BMI_ERR("Insufficient room in dump buffer!");
			break;  /* Insufficient room in buffer */
		}
	}
	return ret;
}

#define MAX_SUPPORTED_PEERS_REV1_1 14
#define MAX_SUPPORTED_PEERS_REV1_3 32

uint8_t ol_get_number_of_peers_supported(struct ol_softc *scn)
{
	uint8_t max_no_of_peers = 0;

	switch (scn->target_version) {
	case AR6320_REV1_1_VERSION:
		if (scn->max_no_of_peers > MAX_SUPPORTED_PEERS_REV1_1)
			max_no_of_peers = MAX_SUPPORTED_PEERS_REV1_1;
		else
			max_no_of_peers = scn->max_no_of_peers;
		break;

	default:
		if (scn->max_no_of_peers > MAX_SUPPORTED_PEERS_REV1_3)
			max_no_of_peers = MAX_SUPPORTED_PEERS_REV1_3;
		else
			max_no_of_peers = scn->max_no_of_peers;
		break;

	}
	return max_no_of_peers;
}
