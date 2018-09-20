#include "control-app.h"
#include <map>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MYPORT 6666
#define BUF_SIZE 1024
#define DEFAULT_THRESHOLD_VALUE -45
#define CONTROLLER_ID 255
#define RECV_FIFO "/tmp/recv_fifo"
#define SEND_FIFO "/tmp/send_fifo"

int connect_app(char* , char*);
void print_cmd();
int proc_msg(char* data);
int proc_cmd(enum CmdType cmd, uint32_t user_id);
enum CmdType parse_cmd(char *msg, uint32_t *user_id);
int send_msg(uint32_t id , enum ProType type);

uint32_t user_id;
int fd_for_send ,  fd_for_recv;
std::map<uint32_t , ap_info> ap_list;
std::map<uint32_t , mn_info> mn_list;
uint8_t last_cmd;

int main(int argc, char *argv[])
{
	int i, ret, fd_max, fd_num, rcvlen;
	struct sockaddr_in serv_adr , clnt_adr;
	char rcvbuf[BUF_SIZE];
	char sendbuf[BUF_SIZE];
	char cmd_msg[128];
	char buff[128], ip_str[32], port_str[16], seps[] = " ,\t\n", *token;
	fd_set reads, cpy_reads;
	struct timeval timeout;
	enum CmdType cmd_type;
	int ap_i, sta_i;
	char mode;
	socklen_t adr_sz;

	//register_app_info();	//read from conf file, register

	user_id = 0;

	FD_ZERO(&reads);
	FD_SET(0, &reads);		// stdin
	fd_max = 0;

	fd_for_recv = open(SEND_FIFO , O_RDWR);
	fd_for_send = open(RECV_FIFO , O_RDWR);

	FD_SET(fd_for_recv , &reads);
	fd_max = fd_for_recv;

	//must_pcmd = 1;
	print_cmd();
	while(1)
	{
		cpy_reads = reads;
		timeout.tv_sec = 2;
		timeout.tv_usec = 0;

		if ((fd_num = select(fd_max+1, &cpy_reads, 0, 0, &timeout)) == -1)
		{
			printf("select error return -1\n");
			exit(-1);
		}
		
		if (fd_num == 0)
			;			// TO DO: resending msg which is not acked

		for (i=0; i<fd_max+1; i++)
		{
			if (FD_ISSET(i, &cpy_reads))
			{
				if (i == 0)		// stdin
				{
					fgets(cmd_msg, 128 , stdin);//get_cmd(cmd_buf, 128);
					cmd_type = parse_cmd(cmd_msg, &user_id);
					if (cmd_type == CMD_QUIT)
						goto main_out;
					ret = proc_cmd(cmd_type, user_id);
					if (ret < 0)
						printf("proc_cmd return %d\n", ret);

				}
				else			// recv msg event
				{
					adr_sz = sizeof(clnt_adr);
					//rcvlen = recvfrom(i, rcvbuf, BUF_SIZE , 0 , (struct sockaddr*)&clnt_adr , &adr_sz);
					read(i,rcvbuf,BUF_SIZE);
					printf("message recv : %d %d\n", rcvlen , *rcvbuf);
					if (rcvlen < sizeof(msg_header))
					{
						printf("Waring, received msg w/ wrong size: %d -> ignore\n", rcvlen);
					}
					else
					{
						ret = proc_msg(rcvbuf);
						if (ret < 0)
						{
							printf("Warning proc_msg return %d\n", ret);
						}
					}
					print_cmd();

				}

			}
		}

	}
main_out:
	//close_all();
	return 0;
}

void print_cmd()
{
	//if (!must_pcmd)
	//	return;

	fprintf(stdout, "\n\n");
	fprintf(stdout, "==============================================================\n");
	fprintf(stdout, "  MIH Control App program\n");
	fprintf(stdout, "  Usage: show ap list || show ap [#ap] || show sta [#ap] ||\n"
					"         del sta [#ap] [#sta] || show mn list || show mn [#mn] quit\n");
	fprintf(stdout, "==============================================================\n");
	fprintf(stdout, "cmd> ");
	fflush(stdout);

	//must_pcmd = 0;
	return;
}

int proc_msg(char* data)
{
	uint8_t type;
	uint32_t id;
	uint16_t total_length;

	type = ((msg_header*)data)->type;
	total_length = ((msg_header*)data)->total_length;
	id = ((msg_header*)data)->user_id;
	
	data += sizeof(msg_header);
	total_length -= sizeof(msg_header);
	
	printf("id = %u , type = %u\n",id , type);

	if(type == HELLO_INFO)
	{
		hello_info* hi = (hello_info*)data;
		if(hi->mode == 'm')
		{
			printf("hi i'm mobile node\n");
			mn_info mi;
			mi.interface_count = 0;
			strcpy(mi.connected_ap , hi->ssid);
			std::map<uint32_t , mn_info>::iterator it;

			it = mn_list.find(id);
			if(it == mn_list.end())
				mn_list.insert(std::pair<uint32_t , mn_info>(id , mi));
			else
			{
				strcpy(it->second.connected_ap , hi->ssid);
			}

			total_length = sizeof(msg_header) + sizeof(int8_t) + sizeof(uint32_t);
			msg_header send_hdr = {id , HELLO_INFO_ACK , total_length};

			char* message = (char*)malloc(sizeof(char)*BUF_SIZE);
			int8_t set_threshold_value = DEFAULT_THRESHOLD_VALUE;
			
			message = (char*)&send_hdr;
			//message += sizeof(msg_header);
			memcpy((message + sizeof(msg_header)) , &user_id , sizeof(uint32_t));
			memcpy((int8_t*)(message + sizeof(msg_header) + sizeof(uint32_t)) , &set_threshold_value , sizeof(int8_t));

			printf("threshold_value = %d\n",set_threshold_value);

			//sendto(sock , (char*)message , total_length , 0 , (struct sockaddr*)clnt_adr , sizeof(*clnt_adr));
			write(fd_for_send , (char*)message , total_length);
		}
		else if(hi->mode == 's')
		{
			printf("hi i'm switch\n");
			ap_info ai;
			strcpy(ai.my_ssid , hi->ssid);
			ap_list.insert(std::pair<uint8_t , ap_info>(id , ai));

			total_length = sizeof(msg_header) + sizeof(uint8_t);
			msg_header send_hdr = {id , HELLO_INFO_ACK , total_length};
			printf("send_hdr.total_length = %u\n",send_hdr.total_length);

			uint8_t* message = (uint8_t*)malloc(sizeof(uint8_t)*BUF_SIZE);
			memcpy(message , (uint8_t*)&send_hdr , sizeof(msg_header));
			//message += sizeof(msg_header);
			memcpy(message + sizeof(msg_header) , &id , sizeof(uint32_t));

			//for(int i=0;i<10;i++)	printf("%x ",message[i]);

			//sendto(sock , (char*)&message , total_length , 0 , (struct sockaddr*)clnt_adr , sizeof(*clnt_adr));
			write(fd_for_send , (char*)message , total_length);
		}
	}
	else if(type == INTERFACE_INFO_ACK)
	{
		uint8_t interface_count = 0;

		if(id == 0)	printf("message invalid\n");

		std::map<uint32_t , mn_info>::iterator it = mn_list.find(id);
		
		while(total_length >= sizeof(interface_info))
		{
			interface_info* if_info = (interface_info*)data;
			strcpy(it->second.if_info[interface_count].ifname , if_info->ifname);
			strcpy(it->second.if_info[interface_count].mac , if_info->mac);
			strcpy(it->second.if_info[interface_count].ssid , if_info->ssid);
			interface_count++;
			total_length -= sizeof(interface_info);
			data += sizeof(interface_info);
		}
		it->second.interface_count = interface_count;

		printf("User id : %d\n", id);

		if(last_cmd == CMD_SHOW_MN)
		{
			printf("	Interface count : %d\n",interface_count);
			for(int i=0; i<interface_count; i++)
			{
				printf("----------------------------------------------------\n");
				printf("		interface name : %s\n", it->second.if_info[i].ifname);
				printf("		mac : %s\n", it->second.if_info[i].mac);
				printf("		ssid : %s\n", it->second.if_info[i].ssid);
			}
		}
	}
	else if(type == STA_INFO_ACK)
	{
		printf("STA_INFO_ACK executed\n");
		uint8_t sta_count = 0;

		if(id == 0)	printf("message invalid\n");

		std::map<uint32_t , ap_info>::iterator it = ap_list.find(id);

		memcpy((switch_info*)&it->second.sw_info , (switch_info*)data , sizeof(switch_info));
		data += sizeof(switch_info);
		total_length -= sizeof(switch_info);

		while(total_length >= sizeof(sta_info))
		{
			sta_info* si = (sta_info*)data;
			printf("si->addr = %s\n",si->addr);
			total_length -= sizeof(sta_info);
			memcpy((sta_info*)&it->second.stinfo[sta_count] , (sta_info*)si , sizeof(sta_info)); 
			sta_count++;
		}

		printf("User id : %d , mac : %s\n",id , it->second.sw_info.mac);
		printf("	Sta count : %d\n",it->second.sw_info.sta_count);
		for(int i=0;i<sta_count;i++)
		{
			printf("		mac : %s\n",it->second.stinfo[i].addr);
			printf("		sig : %d , freq = %u\n",it->second.stinfo[i].sig , it->second.stinfo[i].freq);
		}
	}
	else if(type == THRESHOLD_VIOLATE)
	{
		int max_sig = -100;
		char max_ssid[32] = {0,};
		printf("THRESHOLD_VIOLATE executed\n");

		std::map<uint32_t , mn_info>::iterator it = mn_list.find(id);
		printf("before ap ssid : %s , \n", it->second.connected_ap);

		while(total_length > sizeof(wifi_info))
		{
			wifi_info* wi = (wifi_info*)data;
			data += sizeof(wifi_info);
			wi->sig = wi->sig/100;

			printf("wi->ssid = %s , wi->sig = %d\n",wi->ssid , wi->sig);
			printf("total_length = %u\n",total_length);
			printf("sizeof(wifi_info) = %lu\n",sizeof(wifi_info));
			total_length -= sizeof(wifi_info);
			
			std::map<uint32_t , ap_info>::iterator it_for_ap;
			for(it_for_ap = ap_list.begin() ; it_for_ap != ap_list.end() ; ++it_for_ap)
			{
				printf("wi->ssid = %s , it_for_ap->second.my_ssid = %s , it->second.connected_ap = %s\n",wi->ssid , it_for_ap->second.my_ssid , it->second.connected_ap);
				if(!strcmp(wi->ssid , "SDN-AP3"))	break;
				if(!strstr(wi->ssid , "SDN"))	break;
				if(!strcmp(wi->ssid , it->second.connected_ap))	break;
				if(!strcmp(wi->ssid , it_for_ap->second.my_ssid))
				{
					if(wi->sig > max_sig)
					{
						max_sig = wi->sig;
						strcpy(max_ssid , wi->ssid);
					}
					break;
				}
			}
		}

		printf("max_sig = %d , max_ssid = %s\n",max_sig , max_ssid);
		uint8_t mode = 1;
		total_length = sizeof(msg_header) + 32 + sizeof(uint8_t);
		msg_header send_hdr = {id , THRESHOLD_VIOLATE_ACK , total_length};

		char* message = (char*)malloc(sizeof(char)*BUF_SIZE);
			
		message = (char*)&send_hdr;	

		memcpy(message + sizeof(msg_header) , &mode , sizeof(uint8_t));
		memcpy(message + sizeof(msg_header) + sizeof(uint8_t) , max_ssid , 32);

		//sendto(sock , (char*)message , total_length , 0 , (struct sockaddr*)clnt_adr , sizeof(*clnt_adr));
		ssize_t write_size = write(fd_for_send , (char*)message , total_length);
		printf("size = %ld\n",write_size);
	}
	else if(type == WIFI_CHANGED)
	{
		printf("WIFI CHANGED!\n");
		int mode;
		interface_info before_interface , after_interface;
		memcpy(&mode , data , sizeof(int));
		memcpy(&before_interface , data + sizeof(int) , sizeof(interface_info));
		memcpy(&after_interface, data + sizeof(int) + sizeof(interface_info) , sizeof(interface_info));

		printf("mode = %d , before_mac = %s , after_mac = %s\n",mode , before_interface.mac, after_interface.mac);
		//wifi_changed_struct wcs;
		/*memcpy((wifi_changed_struct*)&wcs , data , sizeof(wifi_changed_struct));

		total_length = sizeof(msg_header) + sizeof(wifi_changed_struct);
		printf("total_length = %u , id = %u , type = %d\n",total_length,id,WIFI_CHANGED_ACK);
		printf("wcs.mode = %d , wcs.before_mac = %s\n",wcs.mode , wcs.before_interface.mac);*/
		total_length = sizeof(msg_header) + sizeof(int) + sizeof(interface_info) + sizeof(interface_info);
		msg_header send_hdr = {id , WIFI_CHANGED_ACK , total_length};
		printf("total_length = %u , id = %u , type = %d\n",total_length,id,WIFI_CHANGED_ACK);

		//char* message = (char*)malloc(sizeof(char)*BUF_SIZE);
		//memset(message , 0 , BUF_SIZE);
		char message[BUF_SIZE];

		memcpy(message , &send_hdr , sizeof(msg_header));

		//message = (char*)&send_hdr;
		//memcpy(message + sizeof(msg_header) , data , total_length);
		memcpy(message + sizeof(msg_header) , &mode , sizeof(int));
		memcpy(message + sizeof(msg_header) + sizeof(int), &before_interface , sizeof(interface_info));
		memcpy(message + sizeof(msg_header) + sizeof(int) + sizeof(interface_info) , &after_interface , sizeof(interface_info));

		ssize_t write_size = write(fd_for_send , (char*)message , total_length);
		printf("size = %ld\n",write_size);
	}
	else if(type == DEL_FLOW_TABLE_REQ)
	{
		printf("DEL_FLOW_TABLE REQ\n");

		total_length = sizeof(msg_header);
		msg_header send_hdr = {id , DEL_FLOW_TABLE_ACK , total_length};

		ssize_t write_size = write(fd_for_send , (char*)&send_hdr , total_length);
	}

		
			
};

int proc_cmd(enum CmdType cmd, uint32_t user_id)
{
	int i, ret;
	
//	printf("CMD: %s %d %d\n", cmdtype_msg[cmd], ap_idx, sta_idx);

	switch (cmd)
	{
		case CMD_SHOW_AP_LIST:
		{
			last_cmd = CMD_SHOW_AP_LIST;

			for(std::map<uint32_t , ap_info>::iterator it=ap_list.begin() ; it != ap_list.end() ; ++it)
			{
				printf("id = %u\n",it->first);
				ret = send_msg(it->first , PRO_CMD_GET_AP);
				if(ret<0)
				{
					printf("ERROR: in_proc_cmd() send_msg error mn[%d] ret: %d\n",i , ret);
				}
			}
			break;
		}
		case CMD_SHOW_AP:
		{
			std::map<uint32_t , ap_info>::iterator it=ap_list.find(user_id);

			/*if (ap_idx >= ap_count || ap_idx < 0)
			{
				printf("ERROR: in proc_cmd() CMD_SHOW_AP ap_idx: %d\n", ap_idx);
				return -1;
			}*/
			last_cmd = CMD_SHOW_AP;

			ret = send_msg(it->first , PRO_CMD_GET_AP);
			if (ret < 0)
			{
				printf("ERROR: in_proc_cmd() send_msg error\n");
				return -1;
			}
			break;
		}
		case CMD_SHOW_MN_LIST:
		{
			last_cmd = CMD_SHOW_MN_LIST;

			for(std::map<uint32_t , mn_info>::iterator it=mn_list.begin() ; it != mn_list.end() ; ++it)
			{
				printf("it->first = %u\n",it->first);
				ret = send_msg(it->first , PRO_CMD_GET_MN);
				if(ret<0)
				{
					printf("ERROR: in_proc_cmd() send_msg error mn[%d] ret: %d\n",i , ret);
				}
			}
			break;
		}
		case CMD_SHOW_MN:
		{
			std::map<uint32_t , mn_info>::iterator it=mn_list.find(user_id);

			/*if (ap_idx >= ap_count || ap_idx < 0)
			{
				printf("ERROR: in proc_cmd() CMD_SHOW_AP ap_idx: %d\n", ap_idx);
				return -1;
			}*/
			last_cmd = CMD_SHOW_MN;

			ret = send_msg(it->first , PRO_CMD_GET_MN);
			if (ret < 0)
			{
				printf("ERROR: in_proc_cmd() send_msg error\n");
				return -1;
			}
			break;
		}
		default:
		{
			printf("ERROR: in_proc_cmd() Parsing CMD error cmd: %d\n", cmd);
			return -1;
		}
	}
	return 0;
}

enum CmdType parse_cmd(char *msg, uint32_t *user_id)
{
	char seps[] = " ,\t\n";
	char *token;
	enum CmdType ret;

	token = strtok(msg, seps);
	if (!token)
		return CMD_EMPTY;

	if (!strcmp(token, "show"))
	{
		token = strtok(NULL, seps);
		if (!token)
			return CMD_WRONG;

		else if(!strcmp(token , "mn"))
		{
			token = strtok(NULL , seps);
			if(!token)	return CMD_WRONG;

			if(!strcmp(token, "list"))
			{
				return CMD_SHOW_MN_LIST;
			}
			else if(isdigit(token[0]))
			{
				*user_id = atoi(token);
				return CMD_SHOW_MN;
			}
			else
			{
				return CMD_WRONG;
			}
		}
		else if(!strcmp(token , "ap"))
		{
			token = strtok(NULL , seps);
                        if(!token)      return CMD_WRONG;

                        if(!strcmp(token, "list"))
                        {
                                return CMD_SHOW_AP_LIST;
                        }
                        else if(isdigit(token[0]))
                        {
                                *user_id = atoi(token);
                                return CMD_SHOW_AP;
                        }
                        else
                        {
                                return CMD_WRONG;
                        }
		}
		return CMD_WRONG;
	}
	else if (!strcmp(token, "quit"))
	{
		return CMD_QUIT;
	}

	return CMD_WRONG;
}

int send_msg(uint32_t id , enum ProType type)
{
	int len;
	msg_header mh;

	if(type == PRO_CMD_GET_MN)
	{
		std::map<uint32_t , mn_info>::iterator it = mn_list.find(id);
		if(it == mn_list.end())	printf("not found %u in mn list\n",id);
		mh.type = INTERFACE_INFO_REQ;
		mh.user_id = id;
		mh.total_length = sizeof(msg_header);
		printf("before write\n");
		len = write(fd_for_send , (char*)&mh , sizeof(msg_header));
	}
	if(type == PRO_CMD_GET_AP)
	{
		std::map<uint32_t , ap_info>::iterator it = ap_list.find(id);
		if(it == ap_list.end())	printf("not found %u in ap list\n",id);
		mh.type = STA_INFO_REQ;
		mh.user_id = id;
		mh.total_length = sizeof(msg_header);

		len = write(fd_for_send , (char*)&mh , sizeof(msg_header));
	}

	return len;
}
