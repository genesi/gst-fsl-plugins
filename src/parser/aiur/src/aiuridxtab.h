/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/ 

/*
 * Copyright (C) 2010 Freescale Semiconductor, Inc. All rights reserved.
 *
 */ 

/*
 * Module Name:    aiuridxtab.h
 *
 * Description:    Head file of utils for import/export index table
 *                 for unified parser gstreamer plugin
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog: 
 *
 */


#ifndef __AIURIDXTAB_H__
#define __AIURIDXTAB_H__

#define AIUR_IDX_TABLE_MAX_SIZE 1000000

typedef struct {
    unsigned int magic;
    unsigned int version;
    unsigned int size;
    unsigned char * idx;
    unsigned int crc;
}AiurIndexTable;

AiurIndexTable * aiurdemux_import_idx_table(const char * filename);
int aiurdemux_export_idx_table(const char * filename, char * index, int len);
void aiurdemux_destroy_idx_table(AiurIndexTable * idxtable);

#endif /* __AIURIDXTAB_H__ */

