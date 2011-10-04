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
#include <linux/kernel.h>       // DPRINTK()
#include <linux/slab.h>         // kmalloc()
#include <linux/fs.h>           // everything...
#include <linux/errno.h>        // error codes
#include <linux/types.h>        // size_t
#include <linux/proc_fs.h>      // proc
#include <linux/fcntl.h>        // O_ACCMODE
#include <linux/capability.h>   // all about restrictions
#include <linux/param.h>        // because of HZ
#include <asm/system.h>         // cli(), *_flags
#include <asm/uaccess.h>        // copy_...

void
cleanup_module (void)
{

}

int
init_module (void)
{

    return 0;
}
