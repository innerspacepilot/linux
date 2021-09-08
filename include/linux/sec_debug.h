#ifndef SEC_DEBUG_H
#define SEC_DEBUG_H

#define SEC_LKMSG_MAGICKEY 0x0000000a6c6c7546
extern void sec_debug_save_last_kmsg(unsigned char *head_ptr, unsigned char *curr_ptr, size_t buf_size);

#endif
