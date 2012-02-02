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
 * pci specific stuff
 */
#include <adlink_common.h>      /* must always be the 1st include */
#include <linux/ioport.h>
#include <linux/pci.h>          /* all about pci */
#include <asm/io.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <adlink_pci.h>
#include <adlink_sja1000.h>
#include <adlink_filter.h>
#include <adlink_pci_rt.c>

#define PCAN_PCI_MINOR_BASE 0   /* the base of all pci device minors */

/* important PITA registers */
#define PITA_ICR         0x00   /* interrupt control register */
#define PITA_GPIOICR     0x18   /* general purpose IO interface control register */
#define PITA_MISC        0x1C   /* miscellanoes register */

#define ADLINK_PCI_VENDOR_ID   0x144A     /* the PCI device and vendor IDs */
#define ADLINK_PCI_DEVICE_ID   0x7841     /* ID for PCI / PCIe Slot cards */

#define PCI_CONFIG_PORT_SIZE 0x007f     /* size of the config io-memory */
#define PCI_PORT_SIZE        0x007f     /* size of a channel io-memory */

static struct pci_device_id pcan_pci_tbl[] = {
    {ADLINK_PCI_VENDOR_ID, ADLINK_PCI_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0}, {0,}
};

static int
pcan_pci_register_driver (struct pci_driver *p_pci_drv)
{
    p_pci_drv->name = DEVICE_NAME;
    p_pci_drv->id_table = pcan_pci_tbl;

    return pci_register_driver (p_pci_drv);
}

static void
pcan_pci_unregister_driver (struct pci_driver *p_pci_drv)
{
    pci_unregister_driver (p_pci_drv);
}

MODULE_DEVICE_TABLE (pci, pcan_pci_tbl);

static u16 _pci_devices = 0;    /* count the number of pci devices */

static u8
pcan_pci_readreg (struct pcandev *dev, u8 port) /* read a register */
{
    u32 lPort = port << 2;
    DPRINTK("dev->port.pci.pvVirtPort: %x\n",dev->port.pci.pvVirtPort + lPort);
    return readb (dev->port.pci.pvVirtPort + lPort);
}

static void
pcan_pci_writereg (struct pcandev *dev, u8 port, u8 data)       /* write a register */
{
    u32 lPort = port << 2;
    writeb (data, dev->port.pci.pvVirtPort + lPort);
}

/* select and clear in Pita stored interrupt */
void
pcan_pci_clear_stored_interrupt (struct pcandev *dev)
{
    u16 PitaICRLow = readw (dev->port.pci.pvVirtConfigPort);    // PITA_ICR
    switch (dev->port.pci.nChannel)
    {
    case 0:
        if (PitaICRLow & 0x0002)
            writew (0x0002, dev->port.pci.pvVirtConfigPort);
        break;
    case 1:
        if (PitaICRLow & 0x0001)
            writew (0x0001, dev->port.pci.pvVirtConfigPort);
        break;
    case 2:
        if (PitaICRLow & 0x0040)
            writew (0x0040, dev->port.pci.pvVirtConfigPort);
        break;
    case 3:
        if (PitaICRLow & 0x0080)
            writew (0x0080, dev->port.pci.pvVirtConfigPort);
        break;
    }
}

/* enable interrupt again */
void
pcan_pci_enable_interrupt (struct pcandev *dev)
{
    u16 PitaICRHigh = readw (dev->port.pci.pvVirtConfigPort + PITA_ICR + 2);
    switch (dev->port.pci.nChannel)
    {
    case 0:
        PitaICRHigh |= 0x0002;
        break;                  /* bit 17 (-16 = 1) */
    case 1:
        PitaICRHigh |= 0x0001;
        break;                  /* bit 16 (-16 = 0) */
    case 2:
        PitaICRHigh |= 0x0040;
        break;                  /* bit 22 (-16 = 6) */
    case 3:
        PitaICRHigh |= 0x0080;
        break;                  /* bit 23 (-16 = 7) */
    }
    writew (PitaICRHigh, dev->port.pci.pvVirtConfigPort + PITA_ICR + 2);

    dev->wInitStep++;
}

static void
pcan_pci_free_irq (struct pcandev *dev)
{
    u16 PitaICRHigh;

    if (dev->wInitStep == 6)
    {
        /* disable interrupt */
        PitaICRHigh = readw (dev->port.pci.pvVirtConfigPort + PITA_ICR + 2);
        switch (dev->port.pci.nChannel)
        {
        case 0:
            PitaICRHigh &= ~0x0002;
            break;
        case 1:
            PitaICRHigh &= ~0x0001;
            break;
        case 2:
            PitaICRHigh &= ~0x0040;
            break;
        case 3:
            PitaICRHigh &= ~0x0080;
            break;
        }
        writew (PitaICRHigh, dev->port.pci.pvVirtConfigPort + PITA_ICR + 2);

        PCI_FREE_IRQ ();

        dev->wInitStep--;
    }
}

/* release and probe */
static int
pcan_pci_cleanup (struct pcandev *dev)
{
    DPRINTK ("pcan_pci_cleanup()\n");

    switch (dev->wInitStep)
    {
    case 6:
        pcan_pci_free_irq (dev);
    case 5:
        _pci_devices--;
    case 4:
        iounmap (dev->port.pci.pvVirtPort);
    case 3:
//        release_mem_region (dev->port.pci.dwPort, PCI_PORT_SIZE);
        release_region (dev->port.pci.dwPort, PCI_PORT_SIZE);
    case 2:
        if (dev->port.pci.nChannel == 0)
            iounmap (dev->port.pci.pvVirtConfigPort);
    case 1:
        if (dev->port.pci.nChannel == 0)
//            release_mem_region (dev->port.pci.dwConfigPort, PCI_CONFIG_PORT_SIZE);
            release_region (dev->port.pci.dwConfigPort, PCI_CONFIG_PORT_SIZE);
    case 0:
        pcan_delete_filter_chain (dev->filter);
        dev->filter = NULL;
        dev->wInitStep = 0;
        if ((_pci_devices == 0) && (dev->drvRegistered[0] == 1))
        {
            DPRINTK("pci_devices && dev registered : true");
            pcan_pci_unregister_driver (&pcan_drv.pci_drv);
        }
    }

    return 0;
}

/* interface depended open and close */
static int
pcan_pci_open (struct pcandev *dev)
{
    dev->ucActivityState = ACTIVITY_IDLE;
    return 0;
}

static int
pcan_pci_release (struct pcandev *dev)
{
    dev->ucActivityState = ACTIVITY_INITIALIZED;
    return 0;
}

static int
pcan_pci_channel_init (struct pcandev *dev, u32 dwConfigPort, u32 dwPort, u16 wIrq,
                       struct pcandev *master_dev)
{
    DPRINTK ("pcan_pci_channel_init(), _pci_devices = %d\n", _pci_devices);

    dev->props.ucMasterDevice = CHANNEL_MASTER; /* obsolete - will be removed soon */

    /* init process wait queues */
    init_waitqueue_head (&dev->read_queue);
    init_waitqueue_head (&dev->write_queue);

    /* set this before any instructions, fill struct pcandev, part 1 */
    dev->wInitStep = 0;
    dev->readreg = pcan_pci_readreg;
    dev->writereg = pcan_pci_writereg;
    dev->cleanup = pcan_pci_cleanup;
    dev->req_irq = pcan_pci_req_irq;
    dev->free_irq = pcan_pci_free_irq;
    dev->open = pcan_pci_open;
    dev->release = pcan_pci_release;
    dev->nMajor = pcan_drv.nMajor;
    dev->nMinor = PCAN_PCI_MINOR_BASE + _pci_devices;
    dev->filter = pcan_create_filter_chain ();

    /* fill struct pcandev, part 1 */
    dev->port.pci.dwPort = dwPort;
    dev->port.pci.dwConfigPort = dwConfigPort;
    dev->port.pci.wIrq = wIrq;

    /* reject illegal combination */
    if (!dwPort || !wIrq)
        return -EINVAL;

    /* do it only if the device is channel master, and channel 0 is it always */
    if (dev->port.pci.nChannel == 0)
    {
        DPRINTK ("dev->port.pci.dwConfigPort: %p\n", dev->port.pci.dwConfigPort);
        if (check_region (dev->port.pci.dwConfigPort, PCI_CONFIG_PORT_SIZE))
            return -EBUSY;
        DPRINTK("Config port check_region succesful.\n");

        request_region (dev->port.pci.dwConfigPort, PCI_CONFIG_PORT_SIZE, "adlink");

        dev->wInitStep = 1;

        dev->port.pci.pvVirtConfigPort = ioremap (dwConfigPort, PCI_CONFIG_PORT_SIZE);
        if (dev->port.pci.pvVirtConfigPort == NULL)
            return -ENODEV;

        dev->wInitStep = 2;

        /* configuration of the PCI chip, part 2 */

        writew (0x0005, dev->port.pci.pvVirtConfigPort + PITA_GPIOICR + 2);     /*set GPIO control register */

        writeb (0x00, dev->port.pci.pvVirtConfigPort + PITA_GPIOICR);   /* enable all channels */

        writeb (0x05, dev->port.pci.pvVirtConfigPort + PITA_MISC + 3);  /* toggle reset */
        mdelay (5);
        writeb (0x04, dev->port.pci.pvVirtConfigPort + PITA_MISC + 3);  /* leave parport mux mode */
        wmb ();
    }
    else
        dev->port.pci.pvVirtConfigPort = master_dev->port.pci.pvVirtConfigPort;

    DPRINTK ("dev->port.pci.dwPort: %p\n", dev->port.pci.dwPort);
    if (check_region (dev->port.pci.dwPort, PCI_PORT_SIZE))
        return -EBUSY;
    DPRINTK("First port check_region succesful.\n");

    request_region (dev->port.pci.dwPort, PCI_PORT_SIZE, "adlink");

    dev->wInitStep = 3;

    dev->port.pci.pvVirtPort = ioremap (dwPort, PCI_PORT_SIZE);

    if (dev->port.pci.pvVirtPort == NULL)
        return -ENODEV;
    dev->wInitStep = 4;

    _pci_devices++;
    dev->wInitStep = 5;

    printk (KERN_INFO "[%s]: pci device minor %d found\n", DEVICE_NAME, dev->nMinor);

    return 0;
}

/**
 * create one pci based device - this may be one of multiple from a card
 */
static int
create_one_pci_device (struct pci_dev *pciDev, int nChannel, struct pcandev *master_dev,
                       struct pcandev **dev)
{
    struct pcandev *local_dev = NULL;
    int result = 0;
    DPRINTK ("create_one_pci_device(nChannel=%d)\n", nChannel);

    /* make the first device on board */
    if ((local_dev = (struct pcandev *) kmalloc (sizeof (struct pcandev), GFP_KERNEL)) == NULL)
    {
        result = -ENOMEM;
        goto fail;
    }

    pcan_soft_init (local_dev, "pci", HW_PCI);

    local_dev->device_open = sja1000_open;
    local_dev->device_write = sja1000_write;
    local_dev->device_release = sja1000_release;
    local_dev->port.pci.nChannel = nChannel;
    local_dev->port.pci.pciDev = NULL;
    local_dev->drvRegistered[0] = 0;

    local_dev->props.ucExternalClock = 1;

    DPRINTK ("pciDev->resource[1].start: %p\n", pciDev->resource[1].start);
    DPRINTK ("pciDev->resource[2].start: %p\n", pciDev->resource[2].start);
    DPRINTK ("pciDev->resource[3].start: %p\n", pciDev->resource[3].start);
    result = pcan_pci_channel_init (local_dev, (u32) pciDev->resource[1].start,
                                    (u32) pciDev->resource[2].start + nChannel * 0x0080,
                                    (u16) pciDev->irq, master_dev);
    DPRINTK ("Channel %i init result: %i\n", nChannel, result);

    if (!result)
        result = sja1000_probe (local_dev);

    if (result)
    {
        local_dev->cleanup (local_dev);
        kfree (local_dev);
        *dev = NULL;
    }
    else
    {
        local_dev->ucPhysicallyInstalled = 1;
        local_dev->port.pci.pciDev = pciDev;

        list_add_tail (&local_dev->list, &pcan_drv.devices);    // add this device to the list
        pcan_drv.wDeviceCount++;
        *dev = local_dev;
    }

  fail:
    if (result)
    {
        DPRINTK ("create_one_pci_device(nChannel=%d) discarded - %d\n", nChannel, result);
    }

    return result;
}

/**
 * search all pci based devices from peak
 */
int
pcan_search_and_create_pci_devices (void)
{
    int result = 0;
    struct pcandev *dev = NULL;
    struct pcandev *master_dev = NULL;

    /* search pci devices */
    DPRINTK ("pcan_search_and_create_pci_devices()\n");
    if (CONFIG_PCI)
    {
        struct pci_dev *pciDev;
        int n = sizeof (pcan_pci_tbl) / sizeof (pcan_pci_tbl[0]) - 1;
        int i;

        for (i = 0; i < n; i++)
        {
            struct pci_dev *from = NULL;
            do
            {
                pciDev = pci_get_device (pcan_pci_tbl[i].vendor, pcan_pci_tbl[i].device, from);

                if (pciDev != NULL)
                {
                    u16 wSubSysID;

                    /* a PCI device with PCAN_PCI_VENDOR_ID and PCAN_PCI_DEVICE_ID was found */
                    from = pciDev;

                    if (pci_enable_device (pciDev))
                        continue;

                    /* get the PCI Subsystem-ID */
                    result = pci_read_config_word (pciDev, PCI_SUBSYSTEM_ID, &wSubSysID);
                    if (result)
                        goto fail;

                    /* configure the PCI chip, part 1 */
                    result = pci_write_config_word (pciDev, PCI_COMMAND, 2);
                    if (result)
                        goto fail;

                    result = pci_write_config_word (pciDev, 0x44, 0);
                    if (result)
                        goto fail;
                    wmb ();

                    /* 1 channel per card */
                    if ((result = create_one_pci_device (pciDev, 0, NULL, &dev)))
                        goto fail;
                    master_dev = dev;

                    /* add a 2nd channel per card */
                    if ((result = create_one_pci_device (pciDev, 1, master_dev, &dev)))
                        goto fail;

                  fail:
                    if (result)
                        pci_disable_device (pciDev);
                }
            }
            while ((pciDev != NULL) && !result);
        }

        DPRINTK ("search_and_create_pci_devices() ended\n");

        if (!result && master_dev)      /* register only if at least one channel was found */
        {
            pcan_pci_register_driver (&pcan_drv.pci_drv);
            dev->drvRegistered[0] = 1;
        }
    }

    return result;
}
