#ifndef __ADLINK_PCI_H__
#define __ADLINK_PCI_H__
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

#ifdef PCIEC_SUPPORT
int pcan_pci_init (void);
void pcan_pci_deinit (void);
#else
int pcan_search_and_create_pci_devices (void);
#endif

void pcan_pci_clear_stored_interrupt (struct pcandev *dev);
void pcan_pci_enable_interrupt (struct pcandev *dev);

#endif /* __ADLINK_PCI_H__ */
