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
 * all file operation functions
 */

#include <adlink_common.h>
#ifndef AUTOCONF_INCLUDED
#include <linux/autoconf.h>
#endif
#include <linux/module.h>

#include <linux/kernel.h>       /* DPRINTK() */
#include <linux/slab.h>         /* kmalloc() */
#include <linux/fs.h>           /* everything... */
#include <linux/errno.h>        /* error codes */
#include <linux/types.h>        /* size_t */
#include <linux/proc_fs.h>      /* proc */
#include <linux/fcntl.h>        /* O_ACCMODE */
#include <linux/pci.h>          /* all about pci */
#include <linux/capability.h>   /* all about restrictions */
#include <asm/system.h>         /* cli(), *_flags */
#include <asm/uaccess.h>        /* copy_... */
#include <linux/delay.h>        /* mdelay() */
#include <linux/poll.h>         /* poll() and select() */

#include <linux/moduleparam.h>

#include <adlink_main.h>
#include <adlink_pci.h>
#include <adlink_sja1000.h>
#include <adlink_fifo.h>
#include <adlink_fops.h>
#include <adlink_parse.h>
#include <adlink_filter.h>
//#include <adlink_fops_rt.c>

#if defined(module_param_array)
extern char *type[8];
extern ushort io[8];
extern char irq[8];
extern ushort bitrate;
extern char *assign;

module_param (bitrate, ushort, 0444);
#else
MODULE_PARM (bitrate, "h");
MODULE_PARM (assign, "s");
#endif

MODULE_PARM_DESC (bitrate, "The initial bitrate (BTR0BTR1) for all channels");


/* wait this time in msec at max after releasing the device - give fifo a chance to flush */
#define MAX_WAIT_UNTIL_CLOSE 1000
#define minor(x) MINOR(x)

#include <adlink_fops_rt.c>

/**
 * is called by pcan_open() and pcan_netdev_open()
 */
int
pcan_open_path (PCAN_OPEN_PATH_ARGS)
{
    int err = 0;

    DPRINTK (KERN_DEBUG "%s: pcan_open_path, minor = %d, path = %d.\n",
             DEVICE_NAME, dev->nMinor, dev->nOpenPaths);

    /* only the first open to this device makes a default init on this device */
    if (!dev->nOpenPaths)
    {
        /* empty all FIFOs */
        err = pcan_fifo_reset (&dev->writeFifo);
        if (err)
            return err;
        err = pcan_fifo_reset (&dev->readFifo);
        if (err)
            return err;

        /* open the interface special parts */
        err = dev->open (dev);
        if (err)
        {
            DPRINTK (KERN_DEBUG "%s: can't open interface special parts!\n", DEVICE_NAME);
            return err;
        }

        /* install irq */
        err = dev->req_irq (REQ_IRQ_ARG);

        if (err)
        {
            DPRINTK (KERN_DEBUG "%s: can't request irq from device!\n", DEVICE_NAME);
            return err;
        }

        /* open the device itself */
        err = dev->device_open (dev, dev->wBTR0BTR1, dev->ucCANMsgType, dev->ucListenOnly);
        if (err)
        {
            DPRINTK (KERN_DEBUG "%s: can't open device hardware itself!\n", DEVICE_NAME);
            return err;
        }
    }

    dev->nOpenPaths++;

    DPRINTK (KERN_DEBUG "%s: pcan_open_path() is OK\n", DEVICE_NAME);

    return 0;
}

/**
 * find the pcandev according to given major,minor numbers
 * returns NULL pointer in the case of no success
 */
struct pcandev *
pcan_search_dev (int major, int minor)
{
    struct pcandev *dev = (struct pcandev *) NULL;
    struct list_head *ptr;

    DPRINTK (KERN_DEBUG "%s: pcan_search_dev(), major,minor = %d,%d.\n", DEVICE_NAME, major, minor);

    if (list_empty (&pcan_drv.devices))
    {
        DPRINTK (KERN_DEBUG "%s: no devices to select from!\n", DEVICE_NAME);
        return NULL;
    }

    /* loop through my devices */
    for (ptr = pcan_drv.devices.next; ptr != &pcan_drv.devices; ptr = ptr->next)
    {
        dev = (struct pcandev *) ptr;

        if (dev->nMajor == major)
            if (dev->nMinor == minor)
                break;
    }

    if (ptr == &pcan_drv.devices)
    {
        printk (KERN_DEBUG "%s: didn't find any pcan devices (%d,%d)\n", DEVICE_NAME, major, minor);
        return NULL;
    }

    return dev;
}

/**
 * is called by pcan_release() and pcan_netdev_close()
 */
void
pcan_release_path (PCAN_RELEASE_PATH_ARGS)
{
    DPRINTK (KERN_DEBUG "%s: pcan_release_path, minor = %d, path = %d.\n",
             DEVICE_NAME, dev->nMinor, dev->nOpenPaths);

    /* if it's the last release: init the chip for non-intrusive operation */
    if (dev->nOpenPaths > 1)
        dev->nOpenPaths--;
    else
    {
        WAIT_UNTIL_FIFO_EMPTY ();
        /* release the device itself */
        dev->device_release (dev);

        dev->release (dev);
        dev->nOpenPaths = 0;

        /* release the interface depended irq, after this 'dev' is not valid */
        dev->free_irq (dev);
    }
}

/**
 * is called at user ioctl() with cmd = PCAN_GET_EXT_STATUS
 */
TPEXTENDEDSTATUS
pcan_ioctl_extended_status_common (struct pcandev *dev)
{
    TPEXTENDEDSTATUS local;

    local.wErrorFlag = dev->wCANStatus;

    local.nPendingReads = dev->readFifo.nStored;

    /* get infos for friends of polling operation */
    if (pcan_fifo_empty (&dev->readFifo))
        local.wErrorFlag |= CAN_ERR_QRCVEMPTY;

    local.nPendingWrites = (dev->writeFifo.nStored + ((atomic_read (&dev->DataSendReady)) ? 0 : 1));

    if (!pcan_fifo_not_full (&dev->writeFifo))
        local.wErrorFlag |= CAN_ERR_QXMTFULL;

    local.nLastError = dev->nLastError;

    return local;
}

/**
 * is called at user ioctl() with cmd = PCAN_GET_STATUS
 */
TPSTATUS
pcan_ioctl_status_common (struct pcandev * dev)
{
    TPSTATUS local;

    local.wErrorFlag = dev->wCANStatus;

    /* get infos for friends of polling operation */
    if (pcan_fifo_empty (&dev->readFifo))
        local.wErrorFlag |= CAN_ERR_QRCVEMPTY;

    if (!pcan_fifo_not_full (&dev->writeFifo))
        local.wErrorFlag |= CAN_ERR_QXMTFULL;

    local.nLastError = dev->nLastError;

    return local;
}

/**
 * is called at user ioctl() with cmd = PCAN_DIAG
 */
TPDIAG
pcan_ioctl_diag_common (struct pcandev * dev)
{
    TPDIAG local;

    local.wType = dev->wType;
    switch (dev->wType)
    {
    case HW_PCI:
        local.dwBase = dev->port.pci.dwPort;
        local.wIrqLevel = dev->port.pci.wIrq;
        break;
    }
    local.dwReadCounter = dev->readFifo.dwTotal;
    local.dwWriteCounter = dev->writeFifo.dwTotal;
    local.dwIRQcounter = dev->dwInterruptCounter;
    local.dwErrorCounter = dev->dwErrorCounter;
    local.wErrorFlag = dev->wCANStatus;

    /* get infos for friends of polling operation */
    if (pcan_fifo_empty (&dev->readFifo))
        local.wErrorFlag |= CAN_ERR_QRCVEMPTY;

    if (!pcan_fifo_not_full (&dev->writeFifo))
        local.wErrorFlag |= CAN_ERR_QXMTFULL;

    local.nLastError = dev->nLastError;
    local.nOpenPaths = dev->nOpenPaths;

    /*   strncpy(local.szVersionString, pcan_drv.szVersionString, VERSIONSTRING_LEN);
     */

    return local;
}
