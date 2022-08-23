#ifndef _DICT_H
#define _DICT_H

#ifndef _VECTOR_H
#include "vector.h"
#endif

#include "byteorder.h"

void init_keywords(vector keywords);
int get_sys_max_fds(int *);
int print_rr_weight (char * buff, int len, void *ptr);
int print_pgfailback (char * buff, int len, void *ptr);
int print_pgpolicy(char * buff, int len, void *ptr);
int print_no_path_retry(char * buff, int len, void *ptr);
int print_fast_io_fail(char * buff, int len, void *ptr);
int print_dev_loss(char * buff, int len, void *ptr);
int print_reservation_key(char * buff, int len, struct be64 key, int source);
int print_off_int_undef(char * buff, int len, void *ptr);
#endif /* _DICT_H */
