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

/* Real time (RTDM) functions */
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

        memcpy (rtdmdev, &adlinkdev_rt, sizeof (struct rtdm_device));
        rtdmdev->device_id = MKDEV (dev->nMajor, dev->nMinor);
        snprintf (rtdmdev->device_name, RTDM_MAX_DEVNAME_LEN, "adlink%d", dev->nMinor);
        rtdmdev->proc_name = rtdmdev->device_name;
        result = rtdm_dev_register (rtdmdev);
        DPRINTK("rtdm_dev_register return value: %i\n",result);

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

/* end of real time functions */

/* convert struct can_frame to struct TPCANMsg
 * To reduce the complexity (and CPU usage) there are no checks (e.g. for dlc)
 * here as it is assumed that the creator of the source struct has done this work
 */
void
frame2msg (struct can_frame *cf, TPCANMsg * msg)
{
    if (cf->can_id & CAN_ERR_FLAG)
    {
        memset (msg, 0, sizeof (*msg));
        msg->MSGTYPE = MSGTYPE_STATUS;
        msg->LEN = 4;

        if (cf->can_id & CAN_ERR_CRTL)
        {
            /* handle data overrun */
            if (cf->data[1] & CAN_ERR_CRTL_RX_OVERFLOW)
                msg->DATA[3] |= CAN_ERR_OVERRUN;

            /* handle CAN_ERR_BUSHEAVY */
            if (cf->data[1] & CAN_ERR_CRTL_RX_WARNING)
                msg->DATA[3] |= CAN_ERR_BUSHEAVY;
        }

        if (cf->can_id & CAN_ERR_BUSOFF_NETDEV)
            msg->DATA[3] |= CAN_ERR_BUSOFF;

        return;
    }

    if (cf->can_id & CAN_RTR_FLAG)
        msg->MSGTYPE = MSGTYPE_RTR;
    else
        msg->MSGTYPE = MSGTYPE_STANDARD;

    if (cf->can_id & CAN_EFF_FLAG)
        msg->MSGTYPE |= MSGTYPE_EXTENDED;

    msg->ID = cf->can_id & CAN_EFF_MASK;        /* remove EFF/RTR/ERR flags */
    msg->LEN = cf->can_dlc;     /* no need to check value range here */

    memcpy (&msg->DATA[0], &cf->data[0], 8);    /* also copy trailing zeros */
}

/* convert struct TPCANMsg to struct can_frame
 * To reduce the complexity (and CPU usage) there are no checks (e.g. for dlc)
 * here as it is assumed that the creator of the source struct has done this work
 */
void
msg2frame (struct can_frame *cf, TPCANMsg * msg)
{
    cf->can_id = msg->ID;

    if (msg->MSGTYPE & MSGTYPE_RTR)
        cf->can_id |= CAN_RTR_FLAG;

    if (msg->MSGTYPE & MSGTYPE_EXTENDED)
        cf->can_id |= CAN_EFF_FLAG;

    /* if (msg->MSGTYPE & MSGTYPE_??????) */
    /*   cf->can_id |= CAN_ERR_FLAG; */

    cf->can_dlc = msg->LEN;     /* no need to check value range here */

    memcpy (&cf->data[0], &msg->DATA[0], 8);    /* also copy trailing zeros */
}

/* request time in msec, fast */
u32
get_mtime (void)
{
    return (jiffies / HZ) * 1000;
}

/* x = (x >= y) ? x - y : 0; */
static void
subtract_timeval (struct timeval *x, struct timeval *y)
{
    if (x->tv_usec >= y->tv_usec)
        x->tv_usec -= y->tv_usec;
    else
    {
        if (x->tv_sec)
        {
            x->tv_sec--;
            x->tv_usec += (1000000 - y->tv_usec);
        }
        else
            goto fail;
    }

    if (x->tv_sec >= y->tv_sec)
    {
        x->tv_sec -= y->tv_sec;
        return;
    }

  fail:
    x->tv_sec = x->tv_usec = 0;
}

/* get relative time to start of driver */
void
get_relative_time (struct timeval *tv, struct timeval *tr)
{
    if (!tv)
        DO_GETTIMEOFDAY ((*tr));
    else
        memcpy (tr, tv, sizeof (*tr));

    subtract_timeval (tr, &pcan_drv.sInitTime);
}

/* convert timeval to pcan used milliseconds / microseconds notation */
void
timeval2pcan (struct timeval *tv, u32 * msecs, u16 * usecs)
{
    *msecs = (u32) (tv->tv_sec * 1000 + tv->tv_usec / 1000);
    *usecs = (u16) (tv->tv_usec % 1000);
}

/* is called when 'cat /proc/pcan' invoked */
static int
pcan_read_procmem (char *page, char **start, off_t offset, int count, int *eof, void *data)
{
    struct pcandev *dev;
    struct list_head *ptr;
    int len = 0;

    DPRINTK ("can_read_procmem()\n");

    len += sprintf (page + len, "\n");
    len +=
        sprintf (page + len,
                 "*----------------- ADLINK PCI-7841 dual-port CAN interface ------------------\n");
    /*   len += sprintf(page + len, "*-------------------------- %s (%s) ----------------------\n", pcan_drv.szVersionString, CURRENT_VERSIONSTRING);
     */
    len +=
        sprintf (page + len,
                 "*--------------------- %d interfaces @ major %03d found -----------------------\n",
                 pcan_drv.wDeviceCount, pcan_drv.nMajor);
    len +=
        sprintf (page + len,
                 "*n -type- ndev --base-- irq --btr- --read-- --write- --irqs-- -errors- status\n");

    /* loop trough my devices */
    for (ptr = pcan_drv.devices.next; ptr != &pcan_drv.devices; ptr = ptr->next)
    {
        u32 dwPort = 0;
        u16 wIrq = 0;

        dev = (struct pcandev *) ptr;
        switch (dev->wType)
        {
        case HW_PCI:
            dwPort = dev->port.pci.dwPort;
            wIrq = dev->port.pci.wIrq;
            break;
        }

        len += sprintf (page + len, "%2d %6s %4s %08x %03d 0x%04x %08lx %08lx %08x %08x 0x%04x\n",
                        dev->nMinor,
                        dev->type,
                        "-NA-",
                        dwPort,
                        wIrq,
                        dev->wBTR0BTR1,
                        (unsigned long) dev->readFifo.dwTotal,
                        (unsigned long) dev->writeFifo.dwTotal,
                        dev->dwInterruptCounter, dev->dwErrorCounter, dev->wCANStatus);
    }

    len += sprintf (page + len, "\n");

    *eof = 1;
    return len;
}

void
remove_dev_list (void)
{
    struct pcandev *dev;
    struct list_head *pos;
    struct list_head *n;

    list_for_each_prev_safe (pos, n, &pcan_drv.devices)
    {
        dev = list_entry (pos, struct pcandev, list);
        if (dev->cleanup != NULL)
        {
            dev->cleanup (dev);
            list_del (&dev->list);
            /* free all device allocated memory */
            kfree (dev);
        }
    }
}

/* called when device is removed (rmmod adlink.ko) */
void
cleanup_module (void)
{
    DPRINTK ("cleanup_module()\n");

    switch (pcan_drv.wInitStep)
    {
    case 4:
        remove_proc_entry (DEVICE_NAME, NULL);
    case 3:
        rt_dev_unregister ();
    case 1:
        class_destroy (pcan_drv.class);
        rt_remove_dev_list ();

    case 0:
        pcan_drv.wInitStep = 0;
    }

    printk (KERN_INFO "[%s] removed.\n", DEVICE_NAME);

}

/* init some equal parts of dev */
void
pcan_soft_init (struct pcandev *dev, char *szType, u16 wType)
{
    dev->wType = wType;
    dev->type = szType;

    dev->nOpenPaths = 0;
    dev->nLastError = 0;
    dev->busStatus = CAN_ERROR_ACTIVE;
    dev->dwErrorCounter = 0;
    dev->dwInterruptCounter = 0;
    dev->wCANStatus = 0;
    dev->bExtended = 1;         /* accept all frames */
    dev->wBTR0BTR1 = bitrate;
    dev->ucCANMsgType = DEFAULT_EXTENDED;
    dev->ucListenOnly = DEFAULT_LISTENONLY;

    memset (&dev->props, 0, sizeof (dev->props));

    /* set default access functions */
    dev->device_open = NULL;
    dev->device_release = NULL;
    dev->device_write = NULL;
    dev->cleanup = NULL;

    dev->device_params = NULL;  /* the default */

    dev->ucPhysicallyInstalled = 0;     /* assume the device is not installed */
    dev->ucActivityState = ACTIVITY_NONE;

    atomic_set (&dev->DataSendReady, 1);

    /* init fifos */
    pcan_fifo_init (&dev->readFifo, &dev->rMsg[0], &dev->rMsg[READ_MESSAGE_COUNT - 1],
                    READ_MESSAGE_COUNT, sizeof (TPCANRdMsg));
    pcan_fifo_init (&dev->writeFifo, &dev->wMsg[0], &dev->wMsg[WRITE_MESSAGE_COUNT - 1],
                    WRITE_MESSAGE_COUNT, sizeof (TPCANMsg));

    INIT_LOCK (&dev->wlock);
    INIT_LOCK (&dev->isr_lock);
}

/* called when device is installed (insmod adlink.ko) */
int
init_module (void)
{
    int result = 0;

    memset (&pcan_drv, 0, sizeof (pcan_drv));
    pcan_drv.wInitStep = 0;
    DO_GETTIMEOFDAY (pcan_drv.sInitTime);       /* store time for timestamp relation, increments in usec */
    pcan_drv.nMajor = PCAN_MAJOR;

#if defined(__BIG_ENDIAN)
    printk ("(be)\n");
#elif defined(__LITTLE_ENDIAN)
    printk ("(le)\n");
#else
#error Endian not set
#endif
#ifdef DEBUG
    printk (KERN_INFO "[%s] DEBUG is switched on\n", DEVICE_NAME);
#endif

    INIT_LIST_HEAD (&pcan_drv.devices);
    INIT_LIST_HEAD (&device_list);

    pcan_drv.wDeviceCount = 0;
    pcan_drv.class = class_create (THIS_MODULE, "adlink");
    pcan_drv.wInitStep = 1;

    /* search PCI */
    if ((result = pcan_search_and_create_pci_devices ()))
        goto fail;

    // no device found, stop all
    if (!pcan_drv.wDeviceCount)
        goto fail;

    pcan_drv.wInitStep = 2;
    
    result = rt_dev_register ();
    if (result < 0)
        goto fail;

    if (!pcan_drv.nMajor)
        pcan_drv.nMajor = result;

    pcan_drv.wInitStep = 3;

    // create the proc entry
    if (create_proc_read_entry (DEVICE_NAME, 0, NULL, pcan_read_procmem, NULL) == NULL)
    {
        result = -ENODEV;       // maybe wrong if there is no proc filesystem configured
        goto fail;
    }

    pcan_drv.wInitStep = 4;

    printk (KERN_INFO "[%s]: major %d.\n", DEVICE_NAME, pcan_drv.nMajor);
    return 0;                   // succeed

  fail:
    cleanup_module ();
    return result;
}

MODULE_AUTHOR ("peter.kotvan@gmail.com");
MODULE_DESCRIPTION ("Driver for dual-port isolated CAN interface card");
MODULE_SUPPORTED_DEVICE ("adlink PCI-7841");
MODULE_LICENSE ("GPL v2");
