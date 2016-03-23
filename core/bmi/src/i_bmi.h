/*
 * Copyright (c) 2014-2016 The Linux Foundation. All rights reserved.
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
/* ===================================================================
 * Internal BMI Header File
 */

#ifndef _I_BMI_H_
#define _I_BMI_H_

#ifdef CONFIG_CNSS
#include <net/cnss.h>
#endif

#include "hif.h"
#include "bmi_msg.h"
#include "bmi.h"
#include "ol_fw.h"

#define QCA_FIRMWARE_FILE            "athwlan.bin"
#define QCA_UTF_FIRMWARE_FILE        "utf.bin"
#define QCA_BOARD_DATA_FILE          "fakeboar.bin"
#define QCA_OTP_FILE                 "otp.bin"
#define QCA_SETUP_FILE               "athsetup.bin"
#define QCA_FIRMWARE_EPPING_FILE     "epping.bin"
/*
 * Note that not all the register locations are accessible.
 * A list of accessible target registers are specified with
 * their start and end addresses in a table for given target
 * version. We should NOT access other locations as either
 * they are invalid locations or host does not have read
 * access to it or the value of the particular register
 * read might change
 */
#define REGISTER_LOCATION       0x00000800

#define DRAM_LOCATION           0x00400000
#define DRAM_SIZE               0x000a8000
/* The local base addr is used to read the target dump using pcie I/O reads */
#define DRAM_LOCAL_BASE_ADDR    (0x100000)

#define IRAM_LOCATION           0x00980000
#define IRAM_SIZE               0x00038000

#define AXI_LOCATION            0x000a0000
#define AXI_SIZE                0x00018000

#define TOTAL_DUMP_SIZE         0x00200000
#define PCIE_READ_LIMIT         0x00005000

#define SHA256_DIGEST_SIZE      32

/* BMI LOGGING WRAPPERS */

#define BMI_LOG(level, args...) CDF_TRACE(CDF_MODULE_ID_BMI, \
					level, ##args)
#define BMI_ERR(args ...)	BMI_LOG(CDF_TRACE_LEVEL_ERROR, args)
#define BMI_DBG(args ...)	BMI_LOG(CDF_TRACE_LEVEL_DEBUG, args)
#define BMI_WARN(args ...)	BMI_LOG(CDF_TRACE_LEVEL_WARN, args)
#define BMI_INFO(args ...)	BMI_LOG(CDF_TRACE_LEVEL_INFO, args)
/* End of BMI Logging Wrappers */

/* BMI Assert Wrappers */
#define bmi_assert CDF_BUG
/*
 * Although we had envisioned BMI to run on top of HTC, this is not how the
 * final implementation ended up. On the Target side, BMI is a part of the BSP
 * and does not use the HTC protocol nor even DMA -- it is intentionally kept
 * very simple.
 */

#define MAX_BMI_CMDBUF_SZ (BMI_DATASZ_MAX + \
			sizeof(uint32_t) /* cmd */ + \
			sizeof(uint32_t) /* addr */ + \
			sizeof(uint32_t))    /* length */
#define BMI_COMMAND_FITS(sz) ((sz) <= MAX_BMI_CMDBUF_SZ)
#define BMI_EXCHANGE_TIMEOUT_MS  1000

struct hash_fw {
	u8 qwlan[SHA256_DIGEST_SIZE];
	u8 otp[SHA256_DIGEST_SIZE];
	u8 bdwlan[SHA256_DIGEST_SIZE];
	u8 utf[SHA256_DIGEST_SIZE];
};

typedef enum _ATH_BIN_FILE {
	ATH_OTP_FILE,
	ATH_FIRMWARE_FILE,
	ATH_PATCH_FILE,
	ATH_BOARD_DATA_FILE,
	ATH_FLASH_FILE,
	ATH_SETUP_FILE,
} ATH_BIN_FILE;

#if defined(QCA_WIFI_3_0_ADRASTEA)
#define NO_BMI 1
#else
#define NO_BMI 0
#endif

/**
 * struct ol_context - Structure to hold OL context
 * @cdf_dev: CDF Device
 * @scn: HIF Context
 * @ramdump_work: WorkQueue for Ramdump collection
 *
 * Structure to hold all ol BMI/Ramdump info
 */
struct ol_context {
	cdf_device_t cdf_dev;
	struct ol_softc *scn;
	cdf_work_t ramdump_work;
};

CDF_STATUS bmi_execute(uint32_t address, uint32_t *param,
				struct ol_context *ol_ctx);
CDF_STATUS bmi_init(struct ol_context *ol_ctx);
CDF_STATUS bmi_no_command(struct ol_context *ol_ctx);
CDF_STATUS bmi_read_memory(uint32_t address, uint8_t *buffer, uint32_t length,
					struct ol_context *ol_ctx);
CDF_STATUS bmi_write_memory(uint32_t address, uint8_t *buffer, uint32_t length,
					struct ol_context *ol_ctx);
CDF_STATUS bmi_fast_download(uint32_t address, uint8_t *buffer, uint32_t length,
					struct ol_context *ol_ctx);
CDF_STATUS bmi_read_soc_register(uint32_t address,
				uint32_t *param, struct ol_context *ol_ctx);
CDF_STATUS bmi_write_soc_register(uint32_t address, uint32_t param,
					struct ol_context *ol_ctx);
CDF_STATUS bmi_get_target_info(struct bmi_target_info *targ_info,
			       struct ol_context *ol_ctx);
CDF_STATUS bmi_firmware_download(struct ol_context *ol_ctx);
CDF_STATUS bmi_done_local(struct ol_context *ol_ctx);
CDF_STATUS ol_download_firmware(struct ol_context *ol_ctx);
CDF_STATUS ol_configure_target(struct ol_context *ol_ctx);
void ramdump_work_handler(void *arg);
#endif
