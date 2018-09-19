#ifndef __TABLE_H__
#define __TABLE_H__

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

typedef struct vendor_header_t{
	uint8_t id;
	uint64_t mac;
	uint32_t src_ip;
	uint32_t dst_ip;
	uint16_t sport;
	uint16_t dport;
	uint8_t padding[8];
}vendor_header;

typedef struct msg_header_t{
	uint32_t user_id;
	uint16_t type;
	uint16_t total_length;
}msg_header;

#endif
