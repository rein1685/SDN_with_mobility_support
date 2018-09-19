#include "Switch.hh"
#include <iostream>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>

#include <signal.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>

#include "table.hh"

#define BUF_SIZE 1024
#define RECV_FIFO "/tmp/recv_fifo"
#define SEND_FIFO "/tmp/send_fifo"

static int fd_for_send , fd_for_recv;

void* socket_pthread(void* arg);
void error_handling(char*);

extern std::map<uint32_t , Node> table;
Switch *sw;

uint64_t str_to_uint64(char *str){
	return strtoull (str, NULL, 16);
}

int main(int argc, char **argv) {
	std::vector<std::string> ports;
	if(argc < 2){
		std::cout << "Usage: switch -i [interface1 interface2 ...] -d [DPId]" << std::endl;
		exit(0);
	}    
	if(strcmp(argv[1],"-i")){
		std::cout << "Invalid argument " << argv[1] << std::endl;
		exit(0);
	}
	int i = 0;
	bool with_dp = false;
	for(i = 2; i < argc; ++i){
		if(!strcmp(argv[i], "-d")){			
			with_dp = true;
			break;
		}
		ports.push_back(argv[i]);
	}
	if(with_dp){
		uint64_t dpid;
		if( argv[i+1] == NULL || !(dpid = str_to_uint64(argv[i+1]))){
			std::cout << "Invalid datapath id. Valid example: 0x0000000000000001" << std::endl;			
			ports.clear();
			exit(0);
		}
		sw = new Switch(0, "164.125.234.65", 6653, ports, dpid);
	}
	else {
    	sw = new Switch(0, "164.125.234.65", 6653, ports);
    }

    mkfifo(RECV_FIFO , 0777);
    mkfifo(SEND_FIFO , 0777);

   
    pthread_t thread;
    int ret_thread;

    ret_thread = pthread_create(&thread , NULL , socket_pthread , NULL);

    sw->start();    
    wait_for_sigint();
    sw->stop();
    delete sw;
    return 0;
}

void* socket_pthread(void* arg)
{
	uint32_t user_id;
	uint64_t src_mac=0;
	uint32_t src_ip=0 , dst_ip=0;
	uint16_t sport = 0 , dport = 0;
	//fd_for_send = open(SEND_FIFO , O_WRONLY);

    fd_for_send = open(SEND_FIFO , O_WRONLY);
    fd_for_recv = open(RECV_FIFO , O_RDONLY);

	//fd_for_send = open(SEND_FIFO , O_WRONLY);
	//sleep(1);
        //fd_for_recv = open(RECV_FIFO , O_RDONLY);
	char buf[BUF_SIZE] = {0,};
	char packet[BUF_SIZE + 24] = {0,};

        while(read(fd_for_recv , buf , BUF_SIZE))
        {

		OFConnection *ofconn = sw->get_conn();
		of10::Vendor vd(0xeeea , 32);

		msg_header* mh = (msg_header*)buf;
		printf("mh->id = %u , mh->type = %u , mh->total_length = %u\n",mh->user_id , mh->type , mh->total_length);
		user_id = mh->user_id;
		memcpy(packet , &user_id , sizeof(uint32_t));
		memcpy(packet + 4 , &src_mac , sizeof(uint64_t));
		memcpy(packet + 12 , &src_ip , sizeof(uint32_t));
		memcpy(packet + 16 , &dst_ip , sizeof(uint32_t));
		memcpy(packet + 20 , &sport , sizeof(uint16_t));
		memcpy(packet + 22 , &dport , sizeof(uint16_t));
		//vendor_header vh = {mh->user_id , 0 , 0 , 0};

		//memcpy(packet , (uint8_t*)&vh , sizeof(vh));
		memcpy(packet + 24 , (uint8_t*)buf , mh->total_length);

		vd.data(packet , mh->total_length + 24);
		uint8_t* buffer = vd.pack();
		ofconn->send(buffer , vd.length());
        } 
}

void error_handling(char* message)
{
        fputs(message , stderr);
        fputc('\n' , stderr);
        exit(1);
}
