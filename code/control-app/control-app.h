#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>

#define STA_MAX 64

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

typedef struct switch_info_t
{
	char ifname[64];
	char mac[20];
	int ifindex;
	int iftype;
	uint32_t freq;
	char ssid[32];
	int sta_count;
}switch_info;

typedef struct wifi_info_t{
	char ssid[32];
	int sig;
}wifi_info;

struct sta_info
{
	unsigned char addr[20];

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

typedef struct mn_info_t{
	uint8_t interface_count;
	char connected_ap[32];
	char ip[16];
	uint16_t port;
	interface_info if_info[5];
}mn_info;

typedef struct ap_info_t{
	char my_ssid[32];
	char ip[16];
	uint16_t port;
	switch_info sw_info;
	struct sta_info stinfo[STA_MAX];
}ap_info;

enum ProType
{
	PRO_UNKNOWN_TYPE = 0,
	PRO_CMD_GET_AP,
	PRO_ACK_GET_AP,
	PRO_NAK_GET_AP,
	PRO_CMD_GET_MN,
	PRO_ACK_GET_MN,
	PRO_NAK_GET_MN,
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

enum CmdType
{
	CMD_EMPTY = 0,
	CMD_SHOW_AP_LIST,
	CMD_SHOW_AP,
	CMD_SHOW_MN_LIST,
	CMD_SHOW_MN,
	CMD_SHOW_STA,
	CMD_DEL_STA,
	CMD_DEL_AP,
	CMD_QUIT,
	CMD_WRONG
};

char cmdtype_msg[][64] = {
	"CMD_EMPTY",
	"CMD_SHOW_AP_LIST",
	"CMD_SHOW_AP",
	"CMD_SHOW_STA",
	"CMD_DEL_STA",
	"CMD_DEL_AP",
	"CMD_QUIT",
	"CMD_WRONG"
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

typedef struct wifi_changed_struct_t{
	int mode;
	interface_info before_interface;
	interface_info after_interface;
}wifi_changed_struct;
