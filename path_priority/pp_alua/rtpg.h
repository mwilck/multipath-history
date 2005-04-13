/*
 * (C) Copyright IBM Corp. 2004, 2005   All Rights Reserved.
 *
 * US Government Users Restricted Rights -
 * Use, duplication or disclosure restricted by
 * GSA ADP Schedule Contract with IBM Corp.
 *
 * Author(s):
 *	Jan Kunigk
 *	Stefan Bader <shbader@de.ibm.com>
 */
#ifndef __RTPG_H__
#define __RTPG_H__
#include "spc3.h"

#define RTPG_SUCCESS				0
#define RTPG_INQUIRY_FAILED			1
#define RTPG_NO_TPG_IDENTIFIER			2
#define RTPG_RTPG_FAILED			3
#define RTPG_TPG_NOT_FOUND			4

int get_target_port_group_support(int fd);
int get_target_port_group(int fd);
int get_asymmetric_access_state(int fd, unsigned int tpg);

#endif /* __RTPG_H__ */

