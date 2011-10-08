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
 * Manages the ring buffers for read and write data
 */
#define PCAN_FIFO_FIX_NOT_FULL_TEST

#include <adlink_common.h>
#include <linux/types.h>
#include <linux/errno.h>        /* error codes */
#include <linux/string.h>       /* memcpy */
#include <linux/sched.h>
#include <asm/system.h>         /* cli(), save_flags(), restore_flags() */
#include <linux/spinlock.h>

#include <adlink_fifo.h>

int pcan_fifo_reset(register FIFO_MANAGER *anchor)
{
  DECLARE_SPIN_LOCK_IRQSAVE_FLAGS;

  SPIN_LOCK_IRQSAVE(&anchor->lock);

  anchor->dwTotal       = 0;
  anchor->nStored       = 0;
  anchor->r = anchor->w = anchor->bufferBegin; // nothing to read

  SPIN_UNLOCK_IRQRESTORE(&anchor->lock);

        /* DPRINTK(KERN_DEBUG "%s: pcan_fifo_reset() %d %p %pd\n", DEVICE_NAME, anchor->nStored, anchor->r, anchor->w); */

  return 0;
}

int pcan_fifo_init(register FIFO_MANAGER *anchor, void *bufferBegin, void *bufferEnd, int nCount, u16 wCopySize)
{
  anchor->wCopySize   = wCopySize;
  anchor->wStepSize   = (bufferBegin == bufferEnd) ? 0 : ((bufferEnd - bufferBegin) / (nCount - 1));
  anchor->nCount      = nCount;
  anchor->bufferBegin = bufferBegin;
  anchor->bufferEnd   = bufferEnd;

        /* check for fatal program errors */
  if ((anchor->wStepSize < anchor->wCopySize) || (anchor->bufferBegin > anchor->bufferEnd) || (nCount <= 1))
    return -EINVAL;

  INIT_LOCK(&anchor->lock);

  return pcan_fifo_reset(anchor);
}

int pcan_fifo_put(register FIFO_MANAGER *anchor, void *pvPutData)
{
  int err = 0;
  DECLARE_SPIN_LOCK_IRQSAVE_FLAGS;

        /* DPRINTK(KERN_DEBUG "%s: pcan_fifo_put() %d %p %p\n", DEVICE_NAME, anchor->nStored, anchor->r, anchor->w); */

  SPIN_LOCK_IRQSAVE(&anchor->lock);

  if (anchor->nStored < anchor->nCount)
  {
    memcpy(anchor->w, pvPutData, anchor->wCopySize);

    anchor->nStored++;
    anchor->dwTotal++;

    if (anchor->w < anchor->bufferEnd)
      anchor->w += anchor->wStepSize;   // increment to next
    else
      anchor->w = anchor->bufferBegin;  // start from begin
  }
  else
    err = -ENOSPC;

  SPIN_UNLOCK_IRQRESTORE(&anchor->lock);

  return err;
}


int pcan_fifo_get(register FIFO_MANAGER *anchor, void *pvGetData)
{
  int err = 0;
  DECLARE_SPIN_LOCK_IRQSAVE_FLAGS;

        /* DPRINTK(KERN_DEBUG "%s: pcan_fifo_get() %d %p %p\n", DEVICE_NAME, anchor->nStored, anchor->r, anchor->w); */

  SPIN_LOCK_IRQSAVE(&anchor->lock);

  if (anchor->nStored > 0)
  {
    memcpy(pvGetData, anchor->r, anchor->wCopySize);

    anchor->nStored--;
    if (anchor->r < anchor->bufferEnd)
      anchor->r += anchor->wStepSize;       /* increment to next */
    else
      anchor->r = anchor->bufferBegin;      /* start from begin */
  }
  else
    err = -ENODATA;

  SPIN_UNLOCK_IRQRESTORE(&anchor->lock);

  return err;
}


/* returns the current count of elements in fifo */
int pcan_fifo_status(FIFO_MANAGER *anchor)
{
  return anchor->nStored;
}

/* returns 0 if the fifo is full */
int pcan_fifo_not_full(FIFO_MANAGER *anchor)
{
#ifdef PCAN_FIFO_FIX_NOT_FULL_TEST
  int r;
  DECLARE_SPIN_LOCK_IRQSAVE_FLAGS;

  SPIN_LOCK_IRQSAVE(&anchor->lock);
  r = (anchor->nStored < anchor->nCount);
  SPIN_UNLOCK_IRQRESTORE(&anchor->lock);

  return r;
#else
  return (anchor->nStored < (anchor->nCount - 1));
#endif
}

/* returns !=0 if the fifo is empty */
int pcan_fifo_empty(FIFO_MANAGER *anchor)
{
  return !(anchor->nStored);
}

