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
 * adlink_main.c - init, cleanup, proc of driver
 */

#include <adlink_common.h>

#include <linux/module.h>
#include <linux/kernel.h>       /* DPRINTK() */
#include <linux/slab.h>         /* kmalloc() */
#include <linux/fs.h>           /* everything... */
#include <linux/errno.h>        /* error codes */
#include <linux/types.h>            /* size_t */
#include <linux/proc_fs.h>          /* proc */
#include <linux/fcntl.h>            /* O_ACCMODE */
#include <linux/capability.h>       /* all about restrictions */
#include <linux/param.h>            /* because of HZ */
#include <asm/system.h>         /* cli(), *_flags */
#include <asm/uaccess.h>            /* copy_... */

#include <adlink_main.h>
#include <adlink_pci.h>
#include <adlink_fops.h>
#include <adlink_fifo.h>
#include <adlink_filter.h>

/**
 * Default defines
 */
#define DEFAULT_BTR0BTR1    CAN_BAUD_500K  // defaults to 500 kbit/sec
#define DEFAULT_EXTENDED    1              // catch all frames
#define DEFAULT_LISTENONLY  0

/**
 * Globals
 */
/* filled by module initialization */
char *type[8] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
u16  io[8]    = {0, 0, 0, 0, 0, 0, 0, 0};
u8   irq[8]   = {0, 0, 0, 0, 0, 0, 0, 0};
u16  bitrate  = DEFAULT_BTR0BTR1;
char *assign  = NULL;

struct driverobj pcan_drv = {};

#include <linux/device.h>

void
cleanup_module (void)
{

}

int
init_module (void)
{

    return 0;
}

MODULE_AUTHOR("peter.kotvan@gmail.com");
MODULE_DESCRIPTION("Driver for dual-port isolated CAN interface card");
MODULE_SUPPORTED_DEVICE("adlink PCI-7841");
MODULE_LICENSE("GPL");
