#ifndef __SJA1000_H__
#define __SJA1000_H__
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
 * prototypes for sja1000 access functions
 */
#include <linux/types.h>
#include <linux/interrupt.h> // 2.6. special
#include <adlink_common.h>
#include <adlink_main.h>

int  sja1000_open(struct pcandev *dev, u16 btr0btr1, u8 bExtended, u8 bListenOnly);
void sja1000_release(struct pcandev *dev);

int sja1000_write(struct pcandev *dev, struct pcanctx_rt *ctx);
int sja1000_irqhandler_rt(rtdm_irq_t *irq_context);
int sja1000_irqhandler_common(struct pcandev *dev, struct pcanctx_rt *ctx);
int  sja1000_write_frame(struct pcandev *dev, struct can_frame *cf);

int  sja1000_probe(struct pcandev *dev);
u16  sja1000_bitrate(u32 dwBitRate);

#endif /* __SJA1000_H__ */
