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

#include <adlink_common.h>

#include <linux/types.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/device.h>
#include <linux/pci.h>

#include <linux/can.h>
#include <linux/can/error.h>
#include <linux/if_arp.h>
#include <linux/if_ether.h>

#include <linux/spinlock.h>

#include <asm/atomic.h>
/* fix overlap in namespace between socketcan can/error.h and pcan.h */
#define CAN_ERR_BUSOFF_NETDEV CAN_ERR_BUSOFF
#undef CAN_ERR_BUSOFF

#include <rtdm/rtdm_driver.h>
struct pcanctx_rt;

#include <pcan.h>

/* Defines */
#define CHANNEL_SINGLE 0
#define CHANNEL_MASTER
#define CHANNEL_SLAVE

#define READBUFFER_SIZE      80 /* read and write buffers */
#define WRITEBUFFER_SIZE     80
#define READ_MESSAGE_COUNT  500 /* read and write message count */
#define WRITE_MESSAGE_COUNT  50

/* wBTR0BTR1 parameter - bitrate of BTR0/BTR1 registers */
#define CAN_BAUD_1M     0x0014
#define CAN_BAUD_500K   0x001C
#define CAN_BAUD_250K   0x011C
#define CAN_BAUD_125K   0x031C
#define CAN_BAUD_100K   0x432F
#define CAN_BAUD_50K    0x472F
#define CAN_BAUD_20K    0x532F
#define CAN_BAUD_10K    0x672F
#define CAN_BAUD_5K     0x7F7F

/* Activity states */
#define ACTIVITY_NONE        0
#define ACTIVITY_INITIALIZED 1  /* set when channel is initialised */
#define ACTIVITY_IDLE        2  /* set when channel is ready to receive or transmit */
#define ACTIVITY_XMIT        3  /* set when channel has received or transmitted */

#define CAN_ERROR_ACTIVE     0  /* initial and normal state */
#define CAN_ERROR_PASSIVE    1  /* receive only state */
#define CAN_BUS_OFF          2  /* switched off from Bus */


/**
 * @brief a helper for fast conversion between 'SJA1000' data ordering and host data order
 */
typedef union
{
    u8 uc[4];
    u32 ul;
} ULCONV;

typedef struct
{
    u16 wStepSize;              /* size of bytes to step to next entry */
    u16 wCopySize;              /* size of bytes to copy */
    void *bufferBegin;          /* points to first element */
    void *bufferEnd;            /* points to last element */
    u32 nCount;                 /* max counts of elements in fifo */
    u32 nStored                 /* count of currently stored messages */
      u32 dwTotal;              /* received messages */
    void *r;                    /* nest Msg to read into the read buffer */
    void *w;                    /* next Msg to write into the read buffer */
    spinlock_t lock;            /* mutual exclusion lock */
} FIFO_MANAGER;

typedef struct
{
    u32 dwPort;                 /* the port of the transport layer */
    u32 dwConfigPort;           /* the confiuration port, PCI only */
    void *pvVirtPort;           /* virtual address of port */
    void *pvVirtConfigPort;     /* only PCI, the virtual address of the config port */
    u16 wIrq;                   /* the associated irq level */
    int nChannel;               /* associated channel of the card - channel #0 is special */
    struct pci_dev *pciDev;     /* remember the hosting PCI card */
} PCI_PORT;

typedef struct pcandev
{
    struct list_head list;      /* link anchor for list of devices */
    int nOpenPaths;             /* number of open paths linked to the device */
    u16 wInitStep;              /* device specific init state */
    int nMajor;                 /* the device major (USB devices have their own major USB_MAJOR!!!) */
    int nMinor;                 /* the associated minor */
    char *type;                 /* the literal type of the device, info only */
    u16 wType;                  /* (number type) to distinguish sp and epp */


    union
    {
        DONGLE_PORT dng;        /* private data of the various ports */
        ISA_PORT isa;
        PCI_PORT pci;
    } port;

    struct chn_props props;     /* various channel properties */

      u8 (*readreg) (struct pcandev * dev, u8 port);    /* read a register */
    void (*writereg) (struct pcandev * dev, u8 port, u8 data);  /* write a register */
    int (*cleanup) (struct pcandev * dev);      /* cleanup the interface */
    int (*open) (struct pcandev * dev); /* called at open of a path */
    int (*release) (struct pcandev * dev);      /* called at release of a path */
    int (*req_irq) (struct rtdm_dev_context * context); /* install the interrupt handler */
    void (*free_irq) (struct pcandev * dev);    /* release the interrupt */

    int (*device_open) (struct pcandev * dev, u16 btr0btr1, u8 bExtended, u8 bListenOnly);      /* open the device itself */
    void (*device_release) (struct pcandev * dev);      /* release the device itself */
    int (*device_write) (struct pcandev * dev, struct pcanctx_rt * ctx);        /* write the device */

    int (*device_params) (struct pcandev * dev, TPEXTRAPARAMS * params);        /* a generalized interface to set */
    /* or get special parameters from the device */

    wait_queue_head_t read_queue;       /* read process wait queue anchor */
    wait_queue_head_t write_queue;      /* write process wait queue anchor */

    u8 bExtended;               /* if 0, no extended frames are accepted */
    int nLastError;             /* last error written */
    int busStatus;              /* follows error status of CAN-Bus */
    u32 dwErrorCounter;         /* counts all fatal errors */
    u32 dwInterruptCounter;     /* counts all interrupts */
    u16 wCANStatus;             /* status of CAN chip */
    u16 wBTR0BTR1;              /* the persistent storage for BTR0 and BTR1 */
    u8 ucCANMsgType;            /* the persistent storage for 11 or 29 bit identifier */
    u8 ucListenOnly;            /* the persistent storage for listen-only mode */
    u8 ucPhysicallyInstalled;   /* the device is PhysicallyInstalled */
    u8 ucActivityState;         /* follow the state of a channel activity */
    atomic_t DataSendReady;     /* !=0 if all data are send */

    FIFO_MANAGER readFifo;      /* manages the read fifo */
    FIFO_MANAGER writeFifo;     /* manages the write fifo */
    TPCANRdMsg rMsg[READ_MESSAGE_COUNT];        /* all read messages */
    TPCANMsg wMsg[WRITE_MESSAGE_COUNT]; /* all write messages */
    void *filter;               /* a ID filter - currently associated to device */
    spinlock_t wlock;           /* mutual exclusion lock for write invocation */
    spinlock_t isr_lock;        /* in isr */
} PCANDEV;

struct pcanctx_rt
{
    struct pcandev *dev;        /* pointer to related device */
    u8 pcReadBuffer[READBUFFER_SIZE];   /* buffer used in read() call */
    u8 *pcReadPointer;          /* points into current read data rest */
    int nReadRest;              /* rest of data left to read */
    int nTotalReadCount;        /* for test only */
    u8 pcWriteBuffer[WRITEBUFFER_SIZE]; /* buffer used in write() call */
    u8 *pcWritePointer;         /* work pointer into buffer */
    int nWriteCount;

    rtdm_irq_t irq_handle;
    rtdm_event_t in_event;
    rtdm_event_t out_event;
    rtdm_event_t empty_event;
    rtdm_lock_t in_lock;        /* read mutual exclusion lock */
    rtdm_lock_t out_lock;       /* write mutual exclusion lock */
    rtdm_lock_t sja_lock;       /* sja mutual exclusion lock */
    unsigned int irq;           /* the associated irq level */
};

typedef struct driverobj
{
    int nMajor;                 /* the major number of Pcan interfaces */
    u16 wDeviceCount;           /* count of found devices */
    u16 wInitStep;              /* driver specific init state */
    struct timeval sInitTime;   /* time in usec when init was called */
    struct list_head devices;   /* base of list of devices */
    u8 *szVersionString;        /* pointer to the driver version string */

    struct pci_driver pci_drv;  /* pci driver structure */

    struct class *class;        /* the associated class of pcan devices */
} DRIVEROBJ;

typedef struct rt_device
{
    struct list_head list;
    struct rtdm_device *device;
} RT_DEVICE;

/* global driver object */
extern struct driverobj pcan_drv;
extern struct list_head device_list;

/* exported functions (not to kenrel) */
u32 get_mtime (void);           /* request time in msec, fast */
void get_relative_time (struct timeval *tv, struct timeval *tr);        /* request time from drivers start */
void timeval2pcan (struct timeval *tv, u32 * msecs, u16 * usecs);       /* convert to pcan time */

void pcan_soft_init (struct pcandev *dev, char *szType, u16 wType);
void buffer_dump (u8 * pucBuffer, u16 wLineCount);
void frame2msg (struct can_frame *cf, TPCANMsg * msg);
void msg2frame (struct can_frame *cf, TPCANMsg * msg);
int pcan_chardev_rx (struct pcandev *dev, struct can_frame *cf, struct timeval *tv);

void dev_unregister (void);

void remove_dev_list (void);
