/*
 * Copyright (C) 2001-2004 Sistina Software, Inc. All rights reserved.
 * Copyright (C) 2004 Red Hat, Inc. All rights reserved.
 *
 * This file is part of the device-mapper userspace tools.
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU Lesser General Public License v.2.1.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef LIB_DMLOG_H
#define LIB_DMLOG_H

#include "libdevmapper.h"

#define _LOG_DEBUG 7
#define _LOG_INFO 6
#define _LOG_NOTICE 5
#define _LOG_WARN 4
#define _LOG_ERR 3
#define _LOG_FATAL 2

extern dm_log_fn _log;

#define plog(l, x...) _log(l, __FILE__, __LINE__, ## x)

#define log_error(x...) plog(_LOG_ERR, x)
#define log_print(x...) plog(_LOG_WARN, x)
#define log_verbose(x...) plog(_LOG_NOTICE, x)
#define log_very_verbose(x...) plog(_LOG_INFO, x)
#define log_debug(x...) plog(_LOG_DEBUG, x)

#endif
