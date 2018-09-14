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

#if !defined(__I_CDF_MC_TIMER_H)
#define __I_CDF_MC_TIMER_H

/**
 * DOC: i_cdf_mc_timer.h
 *
 * Linux-specific definitions for CDF timers serialized to MC thread
 */

/* Include Files */
#include <cdf_mc_timer.h>
#include <cdf_types.h>
#include <linux/timer.h>
#include <linux/time.h>
#include <linux/jiffies.h>

/* Preprocessor definitions and constants */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
/* Type declarations */

typedef struct cdf_mc_timer_platform_s {
	struct timer_list Timer;
	int threadID;
	uint32_t cookie;
	spinlock_t spinlock;
} cdf_mc_timer_platform_t;

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* __I_CDF_MC_TIMER_H */