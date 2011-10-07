#ifndef __ADLINK_FOPS_H__
#define __ADLINK_FOPS_H__
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
#include <linux/kernel.h>      /* printk() */
#include <linux/fs.h>          /* everything... */

#include <rtdm/rtdm_driver.h>

int pcan_open_path(struct pcandev *dev, struct rtdm_dev_context *context);
void pcan_release_path(struct pcandev *dev, struct pcanctx_rt *ctx);

struct pcandev* pcan_search_dev(int major, int minor);

TPEXTENDEDSTATUS pcan_ioctl_extended_status_common(struct pcandev *dev);
TPSTATUS pcan_ioctl_status_common(struct pcandev *dev);
TPDIAG pcan_ioctl_diag_common(struct pcandev *dev);

extern struct rtdm_device pcandev_rt;

#endif                          /* _ADLINK_FOPS_H */
