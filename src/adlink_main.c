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
#include <linux/types.h>        /* size_t */
#include <linux/proc_fs.h>      /* proc */
#include <linux/fcntl.h>        /* O_ACCMODE */
#include <linux/capability.h>   /* all about restrictions */
#include <linux/param.h>        /* because of HZ */
#include <asm/system.h>         /* cli(), *_flags */
#include <asm/uaccess.h>        /* copy_... */

#include <adlink_main.h>
#include <adlink_pci.h>
#include <adlink_fops.h>
#include <adlink_fifo.h>
#include <adlink_filter.h>

/**
 * Default defines
 */
#define DEFAULT_BTR0BTR1    CAN_BAUD_500K       // defaults to 500 kbit/sec
#define DEFAULT_EXTENDED    1   // catch all frames
#define DEFAULT_LISTENONLY  0

/**
 * Globals
 */
/* filled by module initialization */
char *type[8] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
u16 io[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
u8 irq[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

u16 bitrate = DEFAULT_BTR0BTR1;
char *assign = NULL;

/* Global driver objects */
struct driverobj pcan_drv = { };

struct list_head device_list;

#include <linux/device.h>

/* for procfs output */
char config[] = "*----------------------------------------------------------------------------";

static int
rt_dev_register (void)
{
    struct list_head *ptr;
    struct pcandev *dev;
    struct rtdm_device *rtdmdev;
    struct rt_device *rt_dev;
    int result = 0;

    for (ptr = pcan_drv.devices.next; ptr != &pcan_drv.devices; ptr = ptr->next)
    {
        dev = (struct pcandev *) ptr;

        if ((rtdmdev =
             (struct rtdm_device *) kmalloc (sizeof (struct rtdm_device), GFP_KERNEL)) == NULL)
            return -ENOMEM;

        if ((rt_dev = (struct rt_device *) kmalloc (sizeof (struct rt_device), GFP_KERNEL)) == NULL)
        {
            kfree (rtdmdev);
            return -ENOMEM;
        }

        memcpy (rtdmdev, &pcandev_rt, sizeof (struct rtdm_device));
        rtdmdev->device_id = MKDEV (dev->nMajor, dev->nMinor);
        snprintf (rtdmdev->device_name, RTDM_MAX_DEVNAME_LEN, "pcan%d", dev->nMinor);
        rtdmdev->proc_name = rtdmdev->device_name;
        result = rtdm_dev_register (rtdmdev);

        if (!result)
        {
            rt_dev->device = rtdmdev;
            list_add_tail (&rt_dev->list, &device_list);
        }
        else
        {
            kfree (rtdmdev);
            kfree (rt_dev);
            return result;
        }

    }
    return result;
}

void
rt_dev_unregister (void)
{
    struct list_head *ptr = NULL;
    // unregister all registered devices
    for (ptr = device_list.next; ptr != &device_list; ptr = ptr->next)
    {
        rtdm_dev_unregister (((struct rt_device *) ptr)->device, 1000);
    }
}

void
rt_remove_dev_list (void)
{
    struct rt_device *rt_dev;

    while (!list_empty (&device_list))  /* cycle through the list of devices and remove them */
    {
        rt_dev = (struct rt_device *) device_list.prev; /* empty in reverse order */
        list_del (&rt_dev->list);
        /* free all device allocted memory */
        kfree (rt_dev->device);
        kfree (rt_dev);
    }
    remove_dev_list ();
}

int
pcan_chardev_rx (struct pcandev *dev, struct can_frame *cf, struct timeval *tv)
{
    int result = 0;

    /* filter out extended messages in non extended mode */
    if (dev->bExtended || !(cf->can_id & CAN_EFF_FLAG))
    {
        TPCANRdMsg msg;
        struct timeval tr;
        get_relative_time (tv, &tr);
        timeval2pcan (&tr, &msg.dwTime, &msg.wUsec);

        /* convert to old style FIFO message until FIFO supports new
         * struct can_frame and error frames */
        frame2msg (cf, &msg.Msg);

        /* step forward in fifo */
        result = pcan_fifo_put (&dev->readFifo, &msg);

        /* flag to higher layers that a message was put into fifo or an error occurred */
        result = (result) ? result : 1;
    }

    return result;
}


void
cleanup_module (void)
{

}

int
init_module (void)
{

    return 0;
}

MODULE_AUTHOR ("peter.kotvan@gmail.com");
MODULE_DESCRIPTION ("Driver for dual-port isolated CAN interface card");
MODULE_SUPPORTED_DEVICE ("adlink PCI-7841");
MODULE_LICENSE ("GPL");
