#include <signal.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <pcap.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include </usr/include/net/ethernet.h>
#include "MsgApps.hh"
#include <stdio.h>

Controller ctrl("0.0.0.0", 6653, 2);

void error_handling(char* message);
void select_process();
void callback(u_char *useless, const struct pcap_pkthdr *pkthdr, const u_char *packet);
void* read_thread(void* arg);

extern std::map<uint32_t , Node> table;
extern std::map<uint32_t , struct conn_table> sw_table;
extern std::map<uint32_t , struct mn_info> mn_table;

static uint32_t controller_ip;
extern int serv_sock;

extern int fd_for_recv;
extern int fd_for_send;

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Choose an application to run (\"l2\" or \"cbench\"):\n");
        printf("  ./msg_controller l2|cbench\n");
        return 0;
    }

    controller_ip = inet_addr("164.125.234.94");
    //controller_ip = iat.s_addr;

    MultiLearningSwitch l2;
    CBench cbench;
    if (!strcmp(argv[1], "l2")) {
        ctrl.register_for_event(&l2, EVENT_PACKET_IN);
        ctrl.register_for_event(&l2, EVENT_SWITCH_UP);
        ctrl.register_for_event(&l2, EVENT_SWITCH_DOWN);
	ctrl.register_for_event(&l2, EVENT_VENDOR);		//register vendor event

	//socket_pthread();
        printf("l2 application (MultiLearningSwitch) started\n");
    }
    else if (!strcmp(argv[1], "cbench")) {
        ctrl.register_for_event(&cbench, EVENT_PACKET_IN);
        printf("cbench (CBench) application started\n");
    }
    else {
        printf("Invalid application. Must be either \"l2\" or \"cbench\".\n");
        return 0;
    }

    int res = mkfifo(SEND_FIFO , 0777);
    res = mkfifo(RECV_FIFO , 0777);

    fd_for_send = open(SEND_FIFO , O_RDWR);
    fd_for_recv = open(RECV_FIFO , O_RDWR);
    
    if(fd_for_recv == -1)	printf("open pipe error : send\n");

    pthread_t a_thread;
    int ret = pthread_create(&a_thread , NULL , read_thread , NULL);
    
    ctrl.start();
    wait_for_sigint();
    ctrl.stop();

    return 0;
}

void* read_thread(void* arg)
{

	char message[BUF_SIZE];

	while(read(fd_for_recv , message , BUF_SIZE))
	{
		printf("-------------------------------------------\n");
		printf("packet from control-app recv : msg/MsgController.cc\n");
		msg_header* mh = (msg_header*)message;

		uint32_t user_id = mh->user_id;
		uint64_t src_mac = 0;
		uint32_t src_ip = 0 , dst_ip = 0;
		uint16_t sport = 0 , dport = 0;
		OFConnection* ofconn;

		//for(int i=0;i<10;i++)	printf("%x ",message[i]);
		//puts("");

		//vendor_header vh;
		//vh.id = mh->user_id;

		std::map<uint32_t , conn_table>::iterator it = sw_table.find(user_id);

		if(it == sw_table.end())
		{
			//printf("mn executed : %u\n",user_id);
			std::map<uint32_t , mn_info>::iterator it_mn = mn_table.find(user_id);
			if(it_mn == mn_table.end())
			{
				//printf("not found!\n");
				continue;
			}
			else
			{
				//printf("find!\n");
				user_id = it_mn->first;
				src_ip = it_mn->second.src_ip;
				dst_ip = it_mn->second.dst_ip;
				sport = it_mn->second.sport;
				dport = it_mn->second.dport;
				src_mac = it_mn->second.mac;
				ofconn = it_mn->second.ofconn;	
			}		
			 
		}
		else{
			printf("switch executed : %u\n",user_id);
			user_id = it->first;
			src_ip = 0;
			dst_ip = 0;
			sport = 0;
			dport = 0;
			src_mac = 0;
			ofconn = it->second.ofconn;
		}
		of10::Vendor vd(0xeeeb , 33);

		uint8_t buffer[BUF_SIZE + 21];
		//memcpy(buffer , (uint8_t*)&vh , sizeof(vh));
		memcpy(buffer , &user_id , sizeof(uint32_t));
		memcpy(buffer + 4 , &src_mac , sizeof(uint64_t));
		memcpy(buffer + 12 , &src_ip , sizeof(uint32_t));
		memcpy(buffer + 16 , &dst_ip , sizeof(uint32_t));
		memcpy(buffer + 20 , &sport , sizeof(uint16_t));
		memcpy(buffer + 22 , &dport , sizeof(uint16_t));
		memcpy(buffer + 24 , message , mh->total_length);
		//memcpy(buffer + sizeof(vh) , (uint8_t*)&message , mh->total_length);
		vd.data(buffer , mh->total_length + 24);
		printf("user_id = %u , mh->total_length = %u\n",((vendor_header*)buffer)->id , mh->total_length);
		printf("type = %s\n",MessageTypeStr[mh->type]);
		printf("ofconn.getid() = %u\n",ofconn->get_id());

		uint16_t imsi_length;
		memcpy(&imsi_length , buffer + 30 , sizeof(uint16_t));

		uint8_t* send_message = vd.pack();
		ofconn->send(send_message , vd.length());
		printf("-------------------------------------------------------\n");
	}
}

void error_handling(char* message)
{
	fputs(message , stderr);
	fputc('\n' , stderr);
	exit(1);
}
