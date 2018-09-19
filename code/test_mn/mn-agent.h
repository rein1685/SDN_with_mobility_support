/*
 *
 *   Authors:
 *    Wanjik Lee		<wjlee@pnu.edu>
 *
 */

#ifndef MN_AGENT_H
#define MN_AGENT_H

#include "includes.h"
#include "util.h"

#include <netlink/errno.h>
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/route/link.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include </usr/include/iwlib.h>

#include "nl80211.h"

typedef struct msg_header_t
{
	uint32_t user_id;
	uint16_t type;
	uint16_t total_length;
}msg_header;

typedef struct interface_info_t
{
	char ifname[64];
	char mac[20];
	char ssid[32];
}interface_info;

typedef struct hello_info_t
{
	char mode;
	char ssid[32];
}hello_info;

struct trigger_results {
    int done;
    int aborted;
};

typedef struct wifi_into_t{
	unsigned char ssid[32];
	int sig;
}wifi_info;

struct my_info
{
	char ifname[64];				// if name
	unsigned char ifaddr[ETH_NLEN];	// mac_addr
	int ifindex;					// if index
	int iftype;						// if type (IFTYPE_STATION or IFTYPE_AP
	uint32_t freq;					// if's WIPHY_FREQ
	char ssid[128];					
	int	sta_count;
};

// my sta's info
struct sta_info
{
	unsigned char addr[ETH_NLEN];

	uint32_t freq;
	uint32_t ia_time;		// inactive time,  ms unit
	uint32_t co_time;		// connected time, sec unit
	uint32_t rx_bytes;
	uint32_t rx_pkts;
	uint32_t tx_bytes;
	uint32_t tx_pkts;
	int8_t sig;
	char sig_chain[128];
	int8_t sig_ave;
	char sig_ave_chain[128];
	double tx_bitrate;		// Mbps unit
	double rx_bitrate;		// Mbps unit
};

struct pro_hdr
{
	uint16_t	type;
	uint16_t	id;
	uint16_t	num;
};

enum StatusType
{
	IS_READY = 1,
	IS_CHECKING
};

enum MessageType
{
	MESSAGE_NAK = 0,

	INTERFACE_INFO_REQ,
	INTERFACE_INFO_ACK,

	STA_INFO_REQ,
	STA_INFO_ACK,
	
	THRESHOLD_VIOLATE,
	THRESHOLD_VIOLATE_ACK,

	HELLO_INFO,
	HELLO_INFO_ACK,

	WIFI_CHANGED,
	WIFI_CHANGED_ACK,

	DEL_FLOW_TABLE_REQ,
	DEL_FLOW_TABLE_ACK
};

enum ProType
{
	PRO_UNKNOWN_TYPE = 0,
	PRO_CMD_GET_AP,
	PRO_ACK_GET_AP,
	PRO_NAK_GET_AP,
	PRO_CMD_GET_STA_LIST,
	PRO_ACK_GET_STA_LIST,
	PRO_NAK_GET_STA_LIST,
	PRO_CMD_DEL_AP,
	PRO_ACK_DEL_AP,
	PRO_NAK_DEL_AP,
	PRO_CMD_DEL_STA,
	PRO_ACK_DEL_STA,
	PRO_NAK_DEL_STA
};

char protype_msg[][64] = {
	"PRO_UNKNOWN_TYPE",
	"PRO_CMD_GET_AP",
	"PRO_ACK_GET_AP",
	"PRO_NAK_GET_AP",
	"PRO_CMD_GET_STA_LIST",
	"PRO_ACK_GET_STA_LIST",
	"PRO_NAK_GET_STA_LIST",
	"PRO_CMD_DEL_AP",
	"PRO_ACK_DEL_AP",
	"PRO_NAK_DEL_AP",
	"PRO_CMD_DEL_STA",
	"PRO_ACK_DEL_STA",
	"PRO_NAK_DEL_STA"
};

typedef struct wifi_changed_struct_t
{
	int mode;
	interface_info before_interface;
	interface_info after_interface;
}wifi_changed_struct;

typedef union type_union_t
{
	wifi_changed_struct wcs;
}type_union;

#endif	// AP_AGENT_H
