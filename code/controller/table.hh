#ifndef __TABLE_HH__
#define __TABLE_HH__

#include <list>
#include <fluid/of10msg.hh>
#include <fluid/of13msg.hh>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fluid/OFServer.hh>

using namespace fluid_base;


struct mn_info {
	uint8_t node_id;
	uint64_t mac;
	uint32_t ip;
};

struct conn_table {
	uint8_t sw_id;
	uint32_t ip;
	OFConnection *ofconn;
	std::map<uint8_t , class mn_info> info;
};

#endif
