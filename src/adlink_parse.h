#ifndef __PCAN_PARSE_H__
#define __PCAN_PARSE_H__
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
 * header file for input parser and write output formatter
 */

#include <pcan.h>

int pcan_make_output (char *buffer, TPCANRdMsg * m);
int pcan_parse_input_idle (char *buffer);
int pcan_parse_input_message (char *buffer, TPCANMsg * Message);
int pcan_parse_input_init (char *buffer, TPCANInit * Init);

#endif /* __PCAN_PARSE_H__ */
