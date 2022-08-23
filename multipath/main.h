/*
 * Soft:        Description here...
 *
 * Version:     $Id: main.h,v 0.0.1 2003/09/18 15:13:38 cvaroqui Exp $
 *
 * Author:      Copyright (C) 2003 Christophe Varoqui
 *
 *              This program is distributed in the hope that it will be useful,
 *              but WITHOUT ANY WARRANTY; without even the implied warranty of
 *              MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *              See the GNU General Public License for more details.
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 */

#ifndef _MAIN_H
#define _MAIN_H

/*
 * configurator actions
 */
#define ACT_NOTHING_STR		"unchanged"
#define ACT_RELOAD_STR		"reload"
#define ACT_SWITCHPG_STR	"switchpg"
#define ACT_CREATE_STR		"create"

enum actions {
	ACT_RESERVED,
	ACT_NOTHING,
	ACT_RELOAD,
	ACT_SWITCHPG,
	ACT_CREATE
};

/*
 * Build version
 */
#define PROG    "multipath"

#define VERSION_CODE 0x000401
#define DATE_CODE    0x150c04

#define MULTIPATH_VERSION(version)	\
	(version >> 16) & 0xFF,		\
	(version >> 8) & 0xFF,		\
	version & 0xFF

#define VERSION_STRING PROG" v%d.%d.%d (%.2d/%.2d, 20%.2d)\n",	\
                MULTIPATH_VERSION(VERSION_CODE),		\
                MULTIPATH_VERSION(DATE_CODE)

#endif
