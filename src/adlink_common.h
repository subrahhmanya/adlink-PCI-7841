#ifndef __ADLINK_COMMON_H__
#define __ADLINK_COMMON_H__
/* 
 * Driver for dual-port isolated CAN interface card
 * Copyright (C) 2011  Peter Kotvan <peter.kotvan@gmail.com>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * global defines
 */

/* 
 * eventually look at /boot/vmlinuz.version.h
 */
#include <linux/version.h>

#include <linux/stringify.h>

/* #ifndef AUTOCONF_INCLUDED
 * #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38)
 * #include <generated/autoconf.h>
 * #else
 * #include <linux/autoconf.h>
 * #endif
 * #define AUTOCONF_INCLUDED
 * #endif
 */
#include <generated/autoconf.h>

/**
 * name of the module and proc entry
 */
#define DEVICE_NAME "adlink"

#ifdef DEBUG
/* #define DPRINTK printk */
#define DPRINTK(msg, args...) printk(KERN_DEBUG "[adlink]: " msg, ##args)
#else
#define DPRINTK(msg, args...)
#endif

/**
 * RTDM
 */
#include <rtdm/rtdm_driver.h>
#include <asm/div64.h>
#if RTDM_API_VER < 5
typedef nanosecs_abs_t uint64_t;
#endif

static inline void
rt_gettimeofday (struct timeval *tv)
{
    nanosecs_abs_t current_time = rtdm_clock_read ();
    tv->tv_usec = (do_div (current_time, 1000000000) / 1000);
    tv->tv_sec = current_time;

}

#define DO_GETTIMEOFDAY(tv) rt_gettimeofday(&tv)

#define PCAN_IRQ_RETVAL(x) x

#define INIT_LOCK(lock)
#define DECLARE_SPIN_LOCK_IRQSAVE_FLAGS
#define SPIN_LOCK_IRQSAVE(lock)
#define SPIN_UNLOCK_IRQRESTORE(lock)

#endif /* __ADLINK_COMMON_H__ */
