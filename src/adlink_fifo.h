#ifndef __PCAN_FIFO_H__
#define __PCAN_FIFO_H__
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

#include <adlink_main.h>

int pcan_fifo_reset(register FIFO_MANAGER *anchor);
int pcan_fifo_init(register FIFO_MANAGER *anchor, void *bufferBegin, void *bufferEnd, int nCount, u16 wCopySize);
int pcan_fifo_put(register FIFO_MANAGER *anchor, void *pvPutData);
int pcan_fifo_get(register FIFO_MANAGER *anchor, void *pvPutData);
int pcan_fifo_status(FIFO_MANAGER *anchor);
int pcan_fifo_not_full(FIFO_MANAGER *anchor);
int pcan_fifo_empty(FIFO_MANAGER *anchor);

#endif                          /* __PCAN_FIFO_H__ */
