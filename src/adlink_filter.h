#ifndef __PCAN_FILTER_H__
#define __PCAN_FILTER_H__
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

#include <linux/types.h>

void *pcan_create_filter_chain(void); // returns a handle pointer
int   pcan_add_filter(void *handle, u32 FromID, u32 ToID, u8 MSGTYPE);
void  pcan_delete_filter_all(void *handle);
int   pcan_do_filter(void *handle, u32 can_id); // take the netdev can_id as input
void  pcan_delete_filter_chain(void *handle);

#endif                          /* __PCAN_FILTER_H__ */
