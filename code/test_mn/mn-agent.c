//gcc -o mn-agent mn-agent.c util.c $(pkg-config --cflags --libs libnl-genl-3.0) -lpthread -lm

#include "mn-agent.h"

#define SERV_ADDR "10.0.0.1"
#define MY_PORT 6666
#define BUF_SIZE 1024
#define WIFI_MAX 32

int index_for_live;

interface_info interface_list[5];
int interface_count = 0;
int wifi_count = 0;
wifi_info wifi_list[WIFI_MAX];
uint32_t user_id = 0;
int sig = -100;
struct nl_sock *sk;
int driver_id;
int sock;
int8_t threshold_value = -100;
struct sockaddr_in serv_adr , from_adr;
interface_info before_interface , after_interface;
uint8_t* message;

hello_info hi;

uint8_t last_cmd = MESSAGE_NAK;
uint8_t status;

int imsi_count = 0;

int count= 0;


uint8_t* make_message(uint16_t type , void* tu);
void *check_threshold(void *data);
int set_threshold(int8_t threshold_value);
int callback_get_interface(struct nl_msg *msg, void *arg);
int finish_handler(struct nl_msg *msg, void *arg);
int ack_handler(struct nl_msg *msg, void *arg);
int no_seq_check(struct nl_msg *msg, void *arg);
int callback_trigger(struct nl_msg *msg, void *arg);
int callback_dump(struct nl_msg *msg, void *arg);
int do_scan_trigger(struct nl_sock *socket, int if_index, int driver_id);
int callback_get_station(struct nl_msg *msg, void *arg);
static int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err, void *arg);
void connect_ap(char[] , char[]);

int main(int argc , char* argv[])
{
	int str_len;
	uint8_t* recv_message = (uint8_t*)malloc(sizeof(uint8_t)*BUF_SIZE);
	socklen_t adr_sz;
	uint16_t total_length;
	message = (uint8_t*)malloc(sizeof(uint8_t)*BUF_SIZE);
	struct timeval tv_timeo = {2,0};

	index_for_live = if_nametoindex("wlan1");

	user_id = atoi(argv[1]);

	printf("threshold_value = %d\n",threshold_value);
	set_threshold(threshold_value);

	//--------------------------------------------------------------------------------------------------

	sock = socket(PF_INET, SOCK_DGRAM, 0);   
	if(sock == -1)
		error_exit("socket() error");
	
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = inet_addr(SERV_ADDR);
	serv_adr.sin_port = htons((int16_t)MY_PORT);

	if(setsockopt(sock , SOL_SOCKET , SO_RCVTIMEO , &tv_timeo , sizeof(tv_timeo))==-1)
		printf("setsockopt error\n");

	printf("(mih_usr) serv_port: %u\n", htons((int16_t)MY_PORT)); 

	//sendto

	message = make_message(HELLO_INFO , NULL);
	//printf("%d %d %d %s\n",message[0] , message[1] , message[2] , &message[4]);

	total_length = ((msg_header*)message)->total_length;

	sendto(sock , message , total_length , 0 , (struct sockaddr*)&serv_adr , sizeof(serv_adr));

	while(1)
	{
		str_len = recvfrom(sock , (char*)recv_message , BUF_SIZE , 0 , (struct sockaddr*)&from_adr , &adr_sz);
		if(str_len < 0)
		{
			if(last_cmd == WIFI_CHANGED)
			{
				printf("WIFI CHANGED Timeout!\n");
				sendto(sock , message , total_length , 0 , (struct sockaddr*)&serv_adr , sizeof(serv_adr));
			}
			if(last_cmd == HELLO_INFO)
			{
				printf("HELLO INFO Timeout\n");
				sendto(sock , message , total_length , 0 , (struct sockaddr*)&serv_adr , sizeof(serv_adr));
			}
			if(last_cmd == DEL_FLOW_TABLE_REQ)
			{
				printf("DEL FLOW TABLE Timeout\n");
				sendto(sock , message , total_length , 0 , (struct sockaddr*)&serv_adr , sizeof(serv_adr));
			}
		}
		else{
		msg_header* msg_hdr = (msg_header*)recv_message;
		uint16_t type = (uint16_t)(msg_hdr->type);

		if(type == HELLO_INFO_ACK)
		{

			uint32_t imsi_id;
			recv_message += sizeof(msg_header);
			memcpy(&imsi_id , recv_message , sizeof(uint32_t));	//get user_id
			if(imsi_id != user_id)
			{
				printf("Invalid user! imsi_id = %u , user_id = %u\n",imsi_id , user_id);
			}

			memcpy(&threshold_value , (int8_t*)(recv_message + sizeof(uint32_t)) , sizeof(int8_t));
			printf("HELLO INFO ACK!!!!!!!!!!!!!!!!!!!!!\n");

			last_cmd = MESSAGE_NAK;

			status = IS_READY;
		}
		else if(type == INTERFACE_INFO_REQ)
		{
			printf("INTERFACE_INFO_REQ\n");
			message = make_message(INTERFACE_INFO_ACK , NULL);
			total_length = ((msg_header*)message)->total_length;
			sendto(sock , message , total_length , 0 , (struct sockaddr*)&serv_adr , sizeof(serv_adr));
		}
		else if(type == THRESHOLD_VIOLATE_ACK)
		{
			printf("TREHSHOLD VIOLATE ACK\n");
			int i=0;
			uint8_t mode;
			char ssid[32] = {0,};

			char* index = (char*)malloc(sizeof(char)*64);
			if_indextoname(index_for_live , index);

			recv_message += sizeof(msg_header);
			memcpy(&mode , (uint8_t*)recv_message , sizeof(uint8_t));
			memcpy(ssid , (char*)(recv_message + sizeof(uint8_t)) , sizeof(char)*32);

			printf("mode = %d , ssid = %s\n",mode , ssid);

			type_union tu;
			tu.wcs.mode = mode;

			if(mode == 1)
			{
				strcpy(tu.wcs.after_interface.ssid , ssid);
				strcpy(tu.wcs.after_interface.ifname , index);

				for(i=0;i<5;i++)
				{
					if(!strcmp(index , interface_list[i].ifname))
					{
						strcpy(tu.wcs.after_interface.mac , interface_list[i].mac);
						break;
					}
				}
			}
			else if(mode == 2)
			{
				for(i=0;i<5;i++)
				{
					printf("index for live : %s , after interface : %s\n",index,interface_list[i].ifname);
					if(strcmp(interface_list[i].ifname , index))
					{
						printf("!!!!!!!!!!!!!!!\n");
						index_for_live = if_nametoindex(interface_list[i].ifname);

						strcpy(tu.wcs.after_interface.ssid , ssid);
						strcpy(tu.wcs.after_interface.ifname , interface_list[i].ifname);
						strcpy(tu.wcs.after_interface.mac , interface_list[i].mac);
						break;
					}
				}
			}
			else
			{
				printf("Wrong interface count!\n");
			}

			for(i=0;i<5;i++)
			{
				//printf("index : %s , strlen(index) : %lu\n",index,strlen(index));
				printf("interface_list[i].ifname : %s , strlen : %lu\n",interface_list[i].ifname , strlen(interface_list[i].ifname));
				if(!strcmp(index , interface_list[i].ifname))
				{
					printf("!strcmp(index , interface_list[i].ifname) executed\n");
					strcpy(tu.wcs.before_interface.ssid , interface_list[i].ssid);
					strcpy(tu.wcs.before_interface.ifname , interface_list[i].ifname);
					strcpy(tu.wcs.before_interface.mac , interface_list[i].mac);
				}
			}

			message = make_message(WIFI_CHANGED , (type_union*)&tu);
			total_length = ((msg_header*)message)->total_length;

			sendto(sock , message , total_length , 0 , (struct sockaddr*)&serv_adr , sizeof(serv_adr));
		}
		else if(type == WIFI_CHANGED_ACK)
		{
			printf("WIFI_CHANGED_ACK\n");
			//wifi_changed_struct wcs;
			int mode;
			recv_message += sizeof(msg_header);
			memcpy(&mode , recv_message , sizeof(int));
			memcpy(&before_interface , recv_message + sizeof(int) , sizeof(interface_info));
			memcpy(&after_interface , recv_message + sizeof(int) + sizeof(interface_info) , sizeof(interface_info));

			connect_ap(after_interface.ifname , after_interface.ssid);		//이때 changed 시키면

			struct iwreq wrq , wrq2;
			char buffer[IW_ESSID_MAX_SIZE + 1];
			double freq = 0;

			memset(buffer, 0, sizeof(buffer));

			wrq.u.data.pointer = (caddr_t) buffer;
			wrq.u.data.length = IW_ESSID_MAX_SIZE + 1;
			wrq.u.data.flags = 0;

			while(1)		//sleep until wifi change complete
			{
				char* ifname = after_interface.ifname;
				int skfd;

				if((skfd = socket(AF_INET , SOCK_DGRAM , 0)) < 0)
				//if((skfd = iw_sockets_open()) < 0)
				{
					printf("socket error\n");
					break;
				}

				if(iw_get_ext(skfd, ifname, SIOCGIWESSID , &wrq) < 0){
					printf("iw_get_ext error\n");
					break;
				}

				//printf("iw_get_ext() -> %s\n",(char*)wrq.u.essid.pointer);
				printf("iw_get_ext() -> %s\n",buffer);

				if(iw_get_ext(skfd, ifname, SIOCGIWFREQ , &wrq2) < 0){
					printf("iw_get_ext_error : freq\n");
					freq = 0;
				}
				else{
					freq = (double)(wrq2.u.freq.m);
			
					printf("freq = %f\n",freq);
				}

				if(!strcmp(after_interface.ssid , buffer) && freq > 0)	break;
				else	usleep(200*1000);
			}
	
			if(mode == 1)
			{
				index_for_live = if_nametoindex(after_interface.ifname);
				message = make_message(HELLO_INFO , NULL);
				total_length = ((msg_header*)message)->total_length;
				sendto(sock , message , total_length , 0 , (struct sockaddr*)&serv_adr , sizeof(serv_adr));
			}
			else if(mode == 2)
			{
				//system()
				message = make_message(DEL_FLOW_TABLE_REQ , (void*)&before_interface);
				total_length = ((msg_header*)message)->total_length;
				sendto(sock , message , total_length , 0 , (struct sockaddr*)&serv_adr , sizeof(serv_adr));
			}
		}
		else if(type == DEL_FLOW_TABLE_ACK)
		{
			printf("DEL FLOW TABLE ACK\n");
			char ifconfig_cmd[BUF_SIZE] ,  down_cmd[BUF_SIZE];

			//printf("last_cmd = %d , HELLO_INFO = %d\n",last_cmd , HELLO_INFO);
			index_for_live = if_nametoindex(after_interface.ifname);
			sprintf(ifconfig_cmd , "ifconfig %s 10.0.0.5/24" , after_interface.ifname);
			sprintf(down_cmd , "ifconfig %s down" , before_interface.ifname);
			system(down_cmd);
			system(ifconfig_cmd);

			message = make_message(HELLO_INFO , NULL);
			total_length = ((msg_header*)message)->total_length;
			sendto(sock , message , total_length , 0 , (struct sockaddr*)&serv_adr , sizeof(serv_adr));

			sprintf(down_cmd , "ifconfig %s up", before_interface.ifname);
			system(down_cmd);
		}
		}
	}

	//-------------------------------------------------------------------------------------------
	
	return 0;
	// Goto statement required by NLA_PUT_U32().
	nla_put_failure:
	return 1;
}

static int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err, void *arg) {
	// Callback for errors.
	printf("error_handler() called.\n");
	int *ret = arg;
	*ret = err->error;
	return NL_STOP;
}


uint8_t* make_message(uint16_t type , void* arg)
{
	printf("make_message function\n");
	uint16_t total_length = 0;
	int i;
	memset(message , 0 , BUF_SIZE);

	msg_header mh = {user_id , type , 0};
	memcpy(message , (uint8_t*)&mh , sizeof(msg_header));
	total_length += sizeof(mh);

	last_cmd = type;

	if(type == INTERFACE_INFO_ACK)
	{
		printf("interface_info_ack\n");
		int ret;
		struct nl_msg *msg;
		// get_interface_list();

		/*struct nl_sock */sk = nl_socket_alloc(); // Allocate new netlink socket in memory.
		genl_connect(sk); // Create file descriptor and bind socket.
		driver_id = genl_ctrl_resolve(sk, "nl80211"); // Find the nl80211 driver ID.

		interface_count = 0;

		static unsigned int dev_dump_wiphy = -1;
		nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM, callback_get_interface, &dev_dump_wiphy);
		msg = nlmsg_alloc(); // Allocate a message.
		printf("interface_info_ack\n");
		genlmsg_put(msg, 0, 0, driver_id, 0, NLM_F_DUMP, NL80211_CMD_GET_INTERFACE, 0); // Setup the message.
		ret = nl_send_auto_complete(sk, msg); // Send the message.
		nl_recvmsgs_default(sk); // Retrieve the kernel's answer.
		//nl_wait_for_ack(sk);

		for(i=0;i<interface_count;i++)
		{
			memcpy((uint8_t*)(message + total_length) , (uint8_t*)&interface_list[i] , sizeof(interface_info));
			total_length += sizeof(interface_info);
		}
		interface_count = 0;
	}
	else if(type == HELLO_INFO)
	{
		printf("hello info\n");
		int ret;
		struct nl_msg *msg;

		memset(interface_list , 0 , sizeof(interface_list));
		interface_count = 0;

		// get_interface_list();
		/*struct nl_sock */sk = nl_socket_alloc(); // Allocate new netlink socket in memory.
		genl_connect(sk); // Create file descriptor and bind socket.
		driver_id = genl_ctrl_resolve(sk, "nl80211"); // Find the nl80211 driver ID.

		memset(&hi , 0 , sizeof(hi));

		static unsigned int dev_dump_wiphy = -1;
		nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM, callback_get_interface, &dev_dump_wiphy);
		msg = nlmsg_alloc(); // Allocate a message.
		genlmsg_put(msg, 0, 0, driver_id, 0, NLM_F_DUMP, NL80211_CMD_GET_INTERFACE, 0); // Setup the message.
		ret = nl_send_auto_complete(sk, msg); // Send the message.
		nl_recvmsgs_default(sk); // Retrieve the kernel's answer.
		//nl_wait_for_ack(sk);

		if(!hi.ssid[0])
		{
			printf("NO CONNECTED SSID\n");
			exit(1);
		}

		memcpy((uint8_t*)(message + total_length) , (uint8_t*)&hi , sizeof(hello_info));
		total_length += sizeof(hello_info);

	}
	else if(type == THRESHOLD_VIOLATE)
	{
		printf("threshold_violate\n");

		wifi_count = 0;
		int err = do_scan_trigger(sk , index_for_live , driver_id);
                if(err != 0)
                {
                	printf("do scan trigger() failed with %d\n",err);
                        exit(err);
                }
                struct nl_msg *msg = nlmsg_alloc();  // Allocate a message.
                genlmsg_put(msg, 0, 0, driver_id, 0, NLM_F_DUMP, NL80211_CMD_GET_SCAN, 0);  // Setup which command to run.
                nla_put_u32(msg, NL80211_ATTR_IFINDEX, index_for_live);  // Add message attribute, which interface to use.
                nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM, callback_dump, NULL);  // Add the callback.
                int ret = nl_send_auto(sk, msg);  // Send the message.
                printf("NL80211_CMD_GET_SCAN sent %d bytes to the kernel.\n", ret);
                ret = nl_recvmsgs_default(sk);  // Retrieve the kernel's answer. callback_dump() prints SSIDs to stdout.
                nlmsg_free(msg);
                if (ret < 0)
                {
                	printf("ERROR: nl_recvmsgs_default() returned %d (%s).\n", ret, nl_geterror(-ret));
                        exit(ret);
                }

		for(i=0;i<wifi_count;i++)
		{
			memcpy((uint8_t*)(message + total_length) , (uint8_t*)&wifi_list[i] , sizeof(wifi_info));
			total_length += sizeof(wifi_info);
		}
	}
	else if(type == WIFI_CHANGED)
	{
		printf("wifi changed\n");
		wifi_changed_struct wcs = *((wifi_changed_struct*)arg);
		memcpy(message + total_length , &wcs.mode , sizeof(int));
		total_length += sizeof(int);
		memcpy(message + total_length , &wcs.before_interface , sizeof(interface_info));
		total_length += sizeof(interface_info);
		memcpy(message + total_length , &wcs.after_interface , sizeof(interface_info));
		total_length += sizeof(interface_info);
		//memcpy((uint8_t*)(message + total_length) , (uint8_t*)&wcs , sizeof(wifi_changed_struct));
		//total_length += sizeof(wifi_changed_struct);

		//printf("wcs.type = %d\n",wcs.mode);
		//printf("(before)mac : %s , ssid : %s , ifname : %s\n",wcs.before_interface.mac, wcs.before_interface.ssid , wcs.before_interface.ifname);
		//printf("(after)mac : %s , ssid : %s , ifname : %s\n",wcs.after_interface.mac, wcs.after_interface.ssid , wcs.after_interface.ifname);
	}
	else if(type == DEL_FLOW_TABLE_REQ)
	{
		interface_info del_info = *((interface_info*)arg);
		memcpy(message + total_length , &del_info , sizeof(interface_info));
		total_length += sizeof(interface_info);
		//printf("del_info.mac = %s\n",del_info.mac);
	}

	memcpy(message + 6 , &total_length ,  sizeof(total_length));  //total_length

	return message;
}

void *check_threshold(void *data)
{
	threshold_value = *(int8_t*)data;
	char buf[512];

       while(1)
        {
		if(threshold_value == -100)
		{
			sleep(1);
			continue;
		}
		if(count == 5)
		{
			printf("always count %d!\n",count);
			break;
		}
		else if(status == IS_READY){
			sleep(5);
                	uint8_t* message = make_message(THRESHOLD_VIOLATE , NULL);
			uint16_t total_length = ((msg_header*)message)->total_length;
			sendto(sock , message , total_length , 0 , (struct sockaddr*)&serv_adr , sizeof(serv_adr));
			status = IS_CHECKING;
			printf("count = %d!!!!!!!!!!!!!!!!!!!\n",count);
			//sleep(1);
		}
	}
}


int set_threshold(int8_t threshold_value)
{
	pthread_t thread_t;
	int status;

	if(pthread_create(&thread_t , NULL , check_threshold , (void*)&threshold_value) <0)
	{
		printf("thread create error\n");
		exit(0);
	}
}

int callback_get_interface(struct nl_msg *msg, void *arg) {
	//printf("callback_get_interface function\n");
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];
	unsigned int *wiphy = arg;	

	unsigned char mac[ETH_NLEN];
	char macaddr_str[20];

	//printf("Got something.\n");
	//printf("%d\n", arg);
	//nl_msg_dump(msg, stdout);
	// Looks like this parses `msg` into the `tb_msg` array with pointers.
	nla_parse(tb_msg, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0), genlmsg_attrlen(gnlh, 0), NULL);
	// Print everything.
	if (tb_msg[NL80211_ATTR_IFNAME]) {
		//printf("Interface %s\n", nla_get_string(tb_msg[NL80211_ATTR_IFNAME]));
		//strcpy(interface_list[interface_count].ifname , nla_get_string(tb_msg[NL80211_ATTR_IFNAME]));
	} else {
		//printf("Unnamed/non-netdev interface\n");
	}
	if (wiphy && tb_msg[NL80211_ATTR_WIPHY]) {
		//printf("List mode, no interface specified.\n");
		unsigned int thiswiphy = nla_get_u32(tb_msg[NL80211_ATTR_WIPHY]);
		//if (*wiphy != thiswiphy)	printf("phy#%d\n", thiswiphy);
		*wiphy = thiswiphy;
	} else if (tb_msg[NL80211_ATTR_WIPHY]) { // From interface.c#n343.
		//printf("wiphy %d\n", nla_get_u32(tb_msg[NL80211_ATTR_WIPHY]));
	}
	if (tb_msg[NL80211_ATTR_SSID]) {
        	char ssid_str[64];
		//printf("nla_len = %d\n",nla_len(tb_msg[NL80211_ATTR_SSID]));
		//printf("ssid = %s\n",(unsigned char*)nla_data(tb_msg[NL80211_ATTR_SSID]));
		//strcpy(interface_list[interface_count].ssid , (char*)nla_data(tb_msg[NL80211_ATTR_SSID]));
		//index_for_live = nla_get_u32(tb_msg[NL80211_ATTR_IFINDEX]);
	}
	if (tb_msg[NL80211_ATTR_MAC]) {
	        memcpy(mac , nla_data(tb_msg[NL80211_ATTR_MAC]), ETH_NLEN);
        	//mac_addr_n2a(macaddr_str, mac);
		sprintf(macaddr_str , "%02x:%02x:%02x:%02x:%02x:%02x" , mac[0] , mac[1] , mac[2] , mac[3] , mac[4] , mac[5]);
		//printf("addr = %s\n",macaddr_str);
		//strcpy(interface_list[interface_count].mac , macaddr_str);
	}
	// Keep printing everything.
	if (tb_msg[NL80211_ATTR_IFINDEX])
	{
		//printf("ifindex %d\n", nla_get_u32(tb_msg[NL80211_ATTR_IFINDEX]));
	}
	// (tb_msg[NL80211_ATTR_WDEV]) printf("wdev 0x%llx\n", (unsigned long long)nla_get_u64(tb_msg[NL80211_ATTR_WDEV]));
	//if (tb_msg[NL80211_ATTR_IFTYPE])
	//	printf("NOT IMPLEMENTED\n");
	//printf("type %s\n", iftype_name(nla_get_u32(tb_msg[NL80211_ATTR_IFTYPE])));
	// Final print.
	if (tb_msg[NL80211_ATTR_WIPHY_FREQ]) { // git.kernel.org/cgit/linux/kernel/git/jberg/iw.git/tree/interface.c#n345
		uint32_t freq = nla_get_u32(tb_msg[NL80211_ATTR_WIPHY_FREQ]);
		//printf("channel %d (%d MHz)", ieee80211_frequency_to_channel(freq), freq);
		//printf("NOT IMPLEMENTED");
		printf("\n");
	}
	if(last_cmd == HELLO_INFO)	//hello_info
	{
		hi.mode = 'm';

		//printf("INTERFACE_INFO_ACK : %d\n",interface_count);
		strcpy(interface_list[interface_count].ifname , nla_get_string(tb_msg[NL80211_ATTR_IFNAME]));
		//printf("ifname : %s\n",nla_get_string(tb_msg[NL80211_ATTR_IFNAME]));
		if(tb_msg[NL80211_ATTR_SSID])
		{
			//printf("ssid : %s\n",(char*)nla_data(tb_msg[NL80211_ATTR_SSID]));
			strcpy(interface_list[interface_count].ssid , (char*)nla_data(tb_msg[NL80211_ATTR_SSID]));
			strcpy(hi.ssid , (char*)nla_data(tb_msg[NL80211_ATTR_SSID]));
		}
		//printf("mac : %s\n",macaddr_str);
		strcpy(interface_list[interface_count].mac , macaddr_str);
		interface_count++;

	}
	else if(last_cmd == INTERFACE_INFO_ACK)
	{

		//printf("INTERFACE_INFO_ACK : %d\n",interface_count);
		strcpy(interface_list[interface_count].ifname , nla_get_string(tb_msg[NL80211_ATTR_IFNAME]));
		//printf("ifname : %s\n",nla_get_string(tb_msg[NL80211_ATTR_IFNAME]));
		if(tb_msg[NL80211_ATTR_SSID])
		{
			//printf("ssid : %s\n",(char*)nla_data(tb_msg[NL80211_ATTR_SSID]));
			strcpy(interface_list[interface_count].ssid , (char*)nla_data(tb_msg[NL80211_ATTR_SSID]));
		}
		//printf("mac : %s\n",macaddr_str);
		strcpy(interface_list[interface_count].mac , macaddr_str);
		interface_count++;
	}

	//printf("------------------------------\n");
	return NL_SKIP;
}

int finish_handler(struct nl_msg *msg, void *arg) {
    // Callback for NL_CB_FINISH.
    int *ret = arg;
    *ret = 0;
    return NL_SKIP;
}


int ack_handler(struct nl_msg *msg, void *arg) {
    // Callback for NL_CB_ACK.
    int *ret = arg;
    *ret = 0;
    return NL_STOP;
}


int no_seq_check(struct nl_msg *msg, void *arg) {
    // Callback for NL_CB_SEQ_CHECK.
    return NL_OK;
}

int callback_trigger(struct nl_msg *msg, void *arg) {
    // Called by the kernel when the scan is done or has been aborted.
    struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
    struct trigger_results *results = arg;

    //printf("Got something.\n");
    //printf("%d\n", arg);
    //nl_msg_dump(msg, stdout);

    if (gnlh->cmd == NL80211_CMD_SCAN_ABORTED) {
        printf("Got NL80211_CMD_SCAN_ABORTED.\n");
        results->done = 1;
        results->aborted = 1;
    } else if (gnlh->cmd == NL80211_CMD_NEW_SCAN_RESULTS) {
        printf("Got NL80211_CMD_NEW_SCAN_RESULTS.\n");
        results->done = 1;
        results->aborted = 0;
    }  // else probably an uninteresting multicast message.

    return NL_SKIP;
}


int callback_dump(struct nl_msg *msg, void *arg) {
    // Called by the kernel with a dump of the successful scan's data. Called for each SSID.
    struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
    char mac_addr[20];
    struct nlattr *tb[NL80211_ATTR_MAX + 1];
    struct nlattr *bss[NL80211_BSS_MAX + 1];
    static struct nla_policy bss_policy[NL80211_BSS_MAX + 1] = {
        [NL80211_BSS_TSF] = { .type = NLA_U64 },
        [NL80211_BSS_FREQUENCY] = { .type = NLA_U32 },
        [NL80211_BSS_BSSID] = { },
        [NL80211_BSS_BEACON_INTERVAL] = { .type = NLA_U16 },
        [NL80211_BSS_CAPABILITY] = { .type = NLA_U16 },
        [NL80211_BSS_INFORMATION_ELEMENTS] = { },
        [NL80211_BSS_SIGNAL_MBM] = { .type = NLA_U32 },
        [NL80211_BSS_SIGNAL_UNSPEC] = { .type = NLA_U8 },
        [NL80211_BSS_STATUS] = { .type = NLA_U32 },
        [NL80211_BSS_SEEN_MS_AGO] = { .type = NLA_U32 },
        [NL80211_BSS_BEACON_IES] = { },
    };

    // Parse and error check.
    nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0), genlmsg_attrlen(gnlh, 0), NULL);
    if (!tb[NL80211_ATTR_BSS]) {
        printf("bss info missing!\n");
        return NL_SKIP;
    }
    if (nla_parse_nested(bss, NL80211_BSS_MAX, tb[NL80211_ATTR_BSS], bss_policy)) {
        printf("failed to parse nested attributes!\n");
        return NL_SKIP;
    }
    if (!bss[NL80211_BSS_BSSID]) return NL_SKIP;
    if (!bss[NL80211_BSS_INFORMATION_ELEMENTS]) return NL_SKIP;

    // Start printing.
    //print_ssid(nla_data(bss[NL80211_BSS_INFORMATION_ELEMENTS]), nla_len(bss[NL80211_BSS_INFORMATION_ELEMENTS]));
    //printf("\n%d MHz, ", nla_get_u32(bss[NL80211_BSS_FREQUENCY]));
    get_ssid(wifi_list[wifi_count].ssid , (unsigned char*)nla_data(bss[NL80211_BSS_INFORMATION_ELEMENTS]) , nla_len(bss[NL80211_BSS_INFORMATION_ELEMENTS]));
    wifi_list[wifi_count].sig = nla_get_u32(bss[NL80211_BSS_SIGNAL_MBM]);
    //printf("ssid : %s , %d dbm\n",wifi_list[wifi_count].ssid , wifi_list[wifi_count].sig);

    wifi_count++;

    return NL_SKIP;
}


int do_scan_trigger(struct nl_sock *socket, int if_index, int driver_id) {
    // Starts the scan and waits for it to finish. Does not return until the scan is done or has been aborted.
    struct trigger_results results = { .done = 0, .aborted = 0 };
    struct nl_msg *msg;
    struct nl_cb *cb;
    struct nl_msg *ssids_to_scan;
    int err;
    int ret;
    int mcid = genl_ctrl_resolve_grp(socket, "nl80211", "scan");
    nl_socket_add_membership(socket, mcid);  // Without this, callback_trigger() won't be called.

    // Allocate the messages and callback handler.
    msg = nlmsg_alloc();
    if (!msg) {
        printf("ERROR: Failed to allocate netlink message for msg.\n");
        return -ENOMEM;
    }
    ssids_to_scan = nlmsg_alloc();
    if (!ssids_to_scan) {
        printf("ERROR: Failed to allocate netlink message for ssids_to_scan.\n");
        nlmsg_free(msg);
        return -ENOMEM;
    }
    cb = nl_cb_alloc(NL_CB_DEFAULT);
    if (!cb) {
        printf("ERROR: Failed to allocate netlink callbacks.\n");
        nlmsg_free(msg);
        nlmsg_free(ssids_to_scan);
        return -ENOMEM;
    }

    // Setup the messages and callback handler.
    genlmsg_put(msg, 0, 0, driver_id, 0, 0, NL80211_CMD_TRIGGER_SCAN, 0);  // Setup which command to run.
    nla_put_u32(msg, NL80211_ATTR_IFINDEX, if_index);  // Add message attribute, which interface to use.
    nla_put(ssids_to_scan, 1, 0, "");  // Scan all SSIDs.
    nla_put_nested(msg, NL80211_ATTR_SCAN_SSIDS, ssids_to_scan);  // Add message attribute, which SSIDs to scan for.
    nlmsg_free(ssids_to_scan);  // Copied to `msg` above, no longer need this.
    nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, callback_trigger, &results);  // Add the callback.
    nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &err);
    nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &err);
    nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &err);
    nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, no_seq_check, NULL);  // No sequence checking for multicast messages.

    // Send NL80211_CMD_TRIGGER_SCAN to start the scan. The kernel may reply with NL80211_CMD_NEW_SCAN_RESULTS on
    // success or NL80211_CMD_SCAN_ABORTED if another scan was started by another process.
    err = 1;
    ret = nl_send_auto(socket, msg);  // Send the message.
    printf("NL80211_CMD_TRIGGER_SCAN sent %d bytes to the kernel.\n", ret);
    printf("Waiting for scan to complete...\n");
    while (err > 0) ret = nl_recvmsgs(socket, cb);  // First wait for ack_handler(). This helps with basic errors.
    if (err < 0) {
        printf("WARNING: err has a value of %d.\n", err);
    }
    if (ret < 0) {
        printf("ERROR: nl_recvmsgs() returned %d (%s).\n", ret, nl_geterror(-ret));
        return ret;
    }
    while (!results.done) nl_recvmsgs(socket, cb);  // Now wait until the scan is done or aborted.
    if (results.aborted) {
        printf("ERROR: Kernel aborted scan.\n");
        return 1;
    }
    printf("Scan is done.\n");

    // Cleanup.
    nlmsg_free(msg);
    nl_cb_put(cb);
    nl_socket_drop_membership(socket, mcid);  // No longer need this.
    return 0;
}

int callback_get_station(struct nl_msg *msg, void *arg)
{
	printf("callback get station\n");
        struct nlattr *tb[NL80211_ATTR_MAX + 1];
        struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
        struct nlattr *sinfo[NL80211_STA_INFO_MAX + 1];
        static struct nla_policy stats_policy[NL80211_STA_INFO_MAX + 1] = {
                //[NL80211_STA_INFO_INACTIVE_TIME] = { .type = NLA_U32 },
                //[NL80211_STA_INFO_RX_BYTES] = { .type = NLA_U32 },
                //[NL80211_STA_INFO_TX_BYTES] = { .type = NLA_U32 },
                //[NL80211_STA_INFO_RX_PACKETS] = { .type = NLA_U32 },
                //[NL80211_STA_INFO_TX_PACKETS] = { .type = NLA_U32 },
                [NL80211_STA_INFO_SIGNAL] = { .type = NLA_U8 },
                //[NL80211_STA_INFO_T_OFFSET] = { .type = NLA_U64 },
                //[NL80211_STA_INFO_TX_BITRATE] = { .type = NLA_NESTED },
                //[NL80211_STA_INFO_RX_BITRATE] = { .type = NLA_NESTED },
                //[NL80211_STA_INFO_LLID] = { .type = NLA_U16 },
                //[NL80211_STA_INFO_PLID] = { .type = NLA_U16 },
                //[NL80211_STA_INFO_PLINK_STATE] = { .type = NLA_U8 },
                //[NL80211_STA_INFO_TX_RETRIES] = { .type = NLA_U32 },
                //[NL80211_STA_INFO_TX_FAILED] = { .type = NLA_U32 },
                //[NL80211_STA_INFO_STA_FLAGS] =
                  //      { .minlen = sizeof(struct nl80211_sta_flag_update) },
                //[NL80211_STA_INFO_LOCAL_PM] = { .type = NLA_U32},
                //[NL80211_STA_INFO_PEER_PM] = { .type = NLA_U32},
                //[NL80211_STA_INFO_NONPEER_PM] = { .type = NLA_U32},
                //[NL80211_STA_INFO_CHAIN_SIGNAL] = { .type = NLA_NESTED },
                //[NL80211_STA_INFO_CHAIN_SIGNAL_AVG] = { .type = NLA_NESTED },
        };
        char *chain;

        nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
                  genlmsg_attrlen(gnlh, 0), NULL);

        if (!tb[NL80211_ATTR_STA_INFO])
        {
                fprintf(stderr, "callback_link(): sta stats missing!\n");
                return NL_SKIP;
        }
        if (nla_parse_nested(sinfo, NL80211_STA_INFO_MAX,
                             tb[NL80211_ATTR_STA_INFO],
                             stats_policy))
        {
                fprintf(stderr, "callback_link(): failed to parse nested "
                                        "attributes!\n");
                return NL_SKIP;
        }

        //memcpy(sta_list[sta_count].addr, nla_data(tb[NL80211_ATTR_MAC]), ETH_NLEN);

        //chain = get_chain_signal(sinfo[NL80211_STA_INFO_CHAIN_SIGNAL]);
        if (sinfo[NL80211_STA_INFO_SIGNAL])
        {
                sig = (int8_t)nla_get_u8(sinfo[NL80211_STA_INFO_SIGNAL]);
                //strcpy(sta_list[sta_count].sig_chain, chain);
        }

        return NL_SKIP;
}

void connect_ap(char* ifname , char* ssid)
{
	printf("connect ap function\n");
	struct iwreq wrq;
	int skfd;
	//char essid[IW_ESSID_MAX_SIZE + 1];
	//char* ifname = argv[1];

	//strcpy(essid , argv[2]);
	if((skfd = socket(AF_INET , SOCK_DGRAM , 0)) < 0)
	{
		printf("socket error\n");
		exit(-1);
	}
	
	wrq.u.essid.flags = 1;
	wrq.u.essid.pointer = (caddr_t)ssid;
	wrq.u.essid.length = strlen(ssid) + 1;

	if(iw_set_ext(skfd, ifname, SIOCSIWESSID , &wrq) < 0){
		printf("iw_set_ext error\n");
		exit(-1);
	}

	printf("count : %d\n",++count);

	close(skfd);
}
