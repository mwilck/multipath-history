#define setup_default_hwtable struct hwentry defhwtable[] = { \
	{"COMPAQ  ", "HSV110 (C)COMPAQ", GROUP_BY_TUR, &get_evpd_wwid}, \
	{"COMPAQ  ", "MSA1000         ", GROUP_BY_TUR, &get_evpd_wwid}, \
	{"COMPAQ  ", "MSA1000 VOLUME  ", GROUP_BY_TUR, &get_evpd_wwid}, \
	{"DEC     ", "HSG80           ", GROUP_BY_TUR, &get_evpd_wwid}, \
	{"HP      ", "HSV100          ", GROUP_BY_TUR, &get_evpd_wwid}, \
	{"HP      ", "A6189A          ", MULTIBUS, &get_evpd_wwid}, \
	{"HP      ", "OPEN-           ", MULTIBUS, &get_evpd_wwid}, \
	{"DDN     ", "SAN DataDirector", MULTIBUS, &get_evpd_wwid}, \
	{"FSC     ", "CentricStor     ", MULTIBUS, &get_evpd_wwid}, \
	{"HITACHI ", "DF400           ", MULTIBUS, &get_evpd_wwid}, \
	{"HITACHI ", "DF500           ", MULTIBUS, &get_evpd_wwid}, \
	{"HITACHI ", "DF600           ", MULTIBUS, &get_evpd_wwid}, \
	{"IBM     ", "ProFibre 4000R  ", MULTIBUS, &get_evpd_wwid}, \
	{"SGI     ", "TP9100          ", MULTIBUS, &get_evpd_wwid}, \
	{"SGI     ", "TP9300          ", MULTIBUS, &get_evpd_wwid}, \
	{"SGI     ", "TP9400          ", MULTIBUS, &get_evpd_wwid}, \
	{"SGI     ", "TP9500          ", MULTIBUS, &get_evpd_wwid}, \
	{"3PARdata", "VV              ", GROUP_BY_TUR, &get_evpd_wwid}, \
	{"", "", 0, NULL}, \
}; \

#define default_hwtable_addr &defhwtable[0]
