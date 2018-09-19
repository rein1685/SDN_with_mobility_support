#ifndef __MSGAPPS_HH__
#define __MSGAPPS_HH__

#include "Controller.hh"
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
#include "table.hh"

#define BUF_SIZE 1024
#define SEND_FIFO "/tmp/send_fifo"
#define RECV_FIFO "/tmp/recv_fifo"

using namespace fluid_msg;

static std::map<uint32_t , Node> table;
static std::map<uint32_t , conn_table> sw_table;
static std::map<uint32_t , mn_info> mn_table;

static int fd_for_send;
static int fd_for_recv;

static int serv_sock;

class CBench: public Application {
    virtual void event_callback(ControllerEvent* ev) {
        if (ev->get_type() == EVENT_PACKET_IN) {
            of10::FlowMod fm;
            fm.command(of10::OFPFC_ADD);
            uint8_t* buffer;
            buffer = fm.pack();
            ev->ofconn->send(buffer, fm.length());
            OFMsg::free_buffer(buffer);
        }
    }
};

class MultiLearningSwitch: public BaseLearningSwitch {
public:
virtual void event_callback(ControllerEvent* ev) {
        uint8_t ofversion = ev->ofconn->get_version();
	
	if(ev->get_type() == EVENT_VENDOR) {
	    VendorEvent* vd = static_cast<VendorEvent*>(ev);

	    uint32_t vendor;
	    uint32_t src_ip , dst_ip;
	    struct in_addr imsi_src , imsi_dst;
	    uint8_t node_id , conn_id;
	    uint16_t dport , sport;
	    uint32_t user_id;
	    int fd;
	    char message[BUF_SIZE] = {0,};

	    of10::Vendor ofvd(0,0);
	    ofvd.unpack(vd->data);
	    uint8_t* data = (uint8_t*)ofvd.data();
	    uint8_t mode;
	    uint64_t src_mac = 0;

	    memcpy(&user_id , data , sizeof(uint32_t));
	    memcpy(&src_mac , data + 4 , sizeof(uint64_t));
	    memcpy(&src_ip , data + 12 , sizeof(uint32_t));
	    memcpy(&dst_ip , data + 16 , sizeof(uint32_t));
	    memcpy(&sport , data + 20 , sizeof(uint16_t));
	    memcpy(&dport , data + 22 , sizeof(uint16_t));
	    data += 24;
	    
	    msg_header* mh = (msg_header*)data;
	    data = data + sizeof(msg_header);
	    printf("---------------------------------------------------------\n");
	    printf("mh->user_id = %d , mh->type = %s , mh->total_length = %u\n",mh->user_id , MessageTypeStr[mh->type] , mh->total_length);

	    if(mh->type == HELLO_INFO)
	    {
		hello_info* hi = (hello_info*)data;
		printf("HELLO_INFO packet recv : %c\n",hi->mode);

		if(hi->mode == 's')
		{
			//printf("switch\n");
			conn_table imsi_conn;
			strcpy(imsi_conn.sw_id , hi->ssid);
			printf("imsi_conn.sw_id = %s\n",imsi_conn.sw_id);
			imsi_conn.ofconn = ev->ofconn;
			sw_table.insert(std::pair<uint8_t , conn_table>(mh->user_id , imsi_conn));
		}
		else if(hi->mode == 'm')
		{
			//printf("hello! i'm mobile node\n");
			std::map<uint32_t , mn_info>::iterator it = mn_table.find(mh->user_id);
			if(it == mn_table.end())
			{
				printf("mobile node connected ssid : %s\n",hi->ssid);
				mn_info mi;
				mi.mac = src_mac;
				mi.src_ip = htonl(src_ip);
				mi.dst_ip = htonl(dst_ip);
				mi.sport = htons(sport);
				mi.dport = htons(dport);
				mi.ofconn = ev->ofconn;
				mn_table.insert(std::pair<uint32_t , mn_info>(mh->user_id , mi));
				
			}
			else{
				printf("mobile changed!!!!\n");
				printf("mobile node connected ssid : %s\n",hi->ssid);
				it->second.mac = src_mac;
				it->second.src_ip = htonl(src_ip);
				it->second.dst_ip = htonl(dst_ip);
				it->second.sport = htons(sport);
				it->second.dport = htons(dport);
				it->second.ofconn = ev->ofconn;

				//printf("it->second.mac = %lx , it->second.ofconn->get_id() = %u\n",it->second.mac , it->second.ofconn->get_id());
			}
		}
		else	printf("??????????????\n");

	    }
	    else if(mh->type == WIFI_CHANGED)
	    {
		printf("AP_CHANGED packet recv\n");

		std::map<uint32_t , mn_info>::iterator it = mn_table.find(mh->user_id);

		it->second.mac = src_mac;
		it->second.src_ip = htonl(src_ip);
		it->second.dst_ip = htonl(dst_ip);
		it->second.sport = htons(sport);
		it->second.dport = htons(dport);
		it->second.ofconn = ev->ofconn;

		interface_info before_interface , after_interface;
		int mode;
		memcpy(&mode , data , sizeof(int));
		memcpy(&before_interface , data + sizeof(int) , sizeof(interface_info));
		memcpy(&after_interface , data + sizeof(int) + sizeof(interface_info) , sizeof(interface_info));

		printf("before_interface.mac = %s , before_interface.ssid = %s\n",before_interface.mac , before_interface.ssid);
		printf("after_interface.mac = %s , after_interface.ssid = %s\n",after_interface.mac , after_interface.ssid);

	    	uint8_t* before_mac_data;
		uint64_t before_mac = 0;
		before_mac_data = EthAddress::data_from_string(before_interface.mac);
		memcpy(((uint8_t*)&before_mac) + 2 , (uint8_t*)before_mac_data, 6);

		uint8_t *after_mac_data;
		uint64_t after_mac = 0;
		after_mac_data = EthAddress::data_from_string(after_interface.mac);
		memcpy(((uint8_t*)&after_mac) + 2 , (uint8_t*)after_mac_data, 6);

		L2TABLE::iterator it_table;

		OFConnection* before_conn , *after_conn;
		L2TABLE* before_l2table , *after_l2table;

		std::map<uint32_t , conn_table>::iterator it_sw;
		for(it_sw = sw_table.begin() ; it_sw != sw_table.end() ; ++it_sw)
		{
			if(!strcmp(it_sw->second.sw_id , before_interface.ssid))
			{
				before_conn = it_sw->second.ofconn;
			}
			if(!strcmp(it_sw->second.sw_id , after_interface.ssid))
			{
				after_conn = it_sw->second.ofconn;
			}
		}

		before_l2table = get_l2table(before_conn);
		after_l2table = get_l2table(after_conn);

		printf("-----------------------------------------------------------------------\n");

		L2TABLE::iterator it_l2table;
		printf("before table\n");
		for(it_l2table = before_l2table->begin() ; it_l2table != before_l2table->end(); ++it_l2table)
			printf("%-10.10lx	|	%u\n",it_l2table->first , it_l2table->second);

		printf("\nafter table\n");

		for(it_l2table = after_l2table->begin() ; it_l2table != after_l2table->end(); ++it_l2table)
			printf("%-10.10lx	|	%u\n",it_l2table->first , it_l2table->second);

		printf("-----------------------------------------------------------------------\n");

		if(mode == 1)
		{
			printf("one_interface_mode\n");

			if(!strcmp(after_interface.ssid,  before_interface.ssid))
			{
				ssize_t size = write(fd_for_send , (char*)mh , mh->total_length);
				return;
			}
			else
			{
				if(!strcmp(before_interface.ssid , "SDN-AP1"))
				{
					printf("before_interface.ssid = SDN-AP1\n");
					L2TABLE::iterator it_table = before_l2table->find(before_mac);
					if(it_table == before_l2table->end())	printf("not found\n");
					(*before_l2table)[before_mac] = 2;
					//it_table->second = 2;

					it_table = after_l2table->find(before_mac);
					if(it_table == before_l2table->end())	printf("not found\n");
					(*after_l2table)[before_mac] = 1;
					//it_table->second = 1;
				}
				else if(!strcmp(before_interface.ssid , "SDN-AP2"))
				{
					printf("before_interface.ssid = SDN-AP2\n");
					L2TABLE::iterator it_table = before_l2table->find(before_mac);
					if(it_table == before_l2table->end())	printf("not found\n");
					(*before_l2table)[before_mac] = 2;
					//it_table->second = 2;
	
					it_table = after_l2table->find(before_mac);
					if(it_table == before_l2table->end())	printf("not found\n");
					(*after_l2table)[before_mac] = 1;
					//it_table->second = 1;
				}
			}
			//printf("after modify L2TABLE\n");
		}
		if(mode == 2)
		{
			//printf("ev->ofconn = %u , it->second.ofconn = %u\n",ev->ofconn->get_id() , it->second.ofconn->get_id());
			printf("one_interface_mode\n");
			L2TABLE* before_table = get_l2table(before_conn);
			(*before_table)[after_mac] = 2;

			for(L2TABLE::iterator erase_it = before_table->begin() ; erase_it != before_table->end() ;)
			{
				if(erase_it->first == before_mac)
				{
					printf("erase_it->first == before_mac\n");
					printf("%lx	|	%u\n",erase_it->first , erase_it->second);
					before_table->erase(erase_it++);
				}
				else	++erase_it;
			}

			L2TABLE* after_table = get_l2table(after_conn);
			(*after_table)[after_mac] = 1;
	
			for(L2TABLE::iterator erase_it = after_table->begin() ; erase_it != after_table->end() ;)
			{
				if(erase_it->first == before_mac)
				{
					printf("erase_it->first == before_mac\n");
					printf("%lx	|	%u\n",erase_it->first , erase_it->second);
					after_table->erase(erase_it++);
				}
				else	++erase_it;
			}
		}
		//-----------------------------------------------------------------------------
		printf("-----------------------------------------------------------------------\n");

		printf("before table\n");
		for(it_l2table = before_l2table->begin() ; it_l2table != before_l2table->end(); ++it_l2table)
			printf("%-10.10lx	|	%u\n",it_l2table->first , it_l2table->second);

		printf("\nafter table\n");

		for(it_l2table = after_l2table->begin() ; it_l2table != after_l2table->end(); ++it_l2table)
			printf("%-10.10lx	|	%u\n",it_l2table->first , it_l2table->second);

		printf("-----------------------------------------------------------------------\n");

		for(it_table = before_l2table->begin() ; it_table != before_l2table->end() ; ++it_table)
		{
			if(it_table->first == before_mac || it_table->first == after_mac)	continue;

			of10::FlowMod fm(ofvd.xid(),  //xid 
		           		123, // cookie
		               		of10::OFPFC_ADD, // command
		               		5, // idle timeout
		               		10, // hard timeout
		               		100, // priority
		               		-1, //buffer id
		               		2, // outport
		               		0); // flags

			L2TABLE::iterator imsi_it = before_l2table->find(after_mac);
			if(imsi_it == before_l2table->end())	printf("not found\n");

			of10::Match m;			//make flowmod packet
		       	m.dl_src(((uint8_t*) &it_table->first) + 2);
		       	m.dl_dst(((uint8_t*) &after_mac) + 2);
		       	fm.match(m);
		       	of10::OutputAction act(imsi_it->second , 1024);
			printf("%lx -> %lx : %u\n",it_table->first , after_mac , imsi_it->second);

		       	fm.add_action(act);
		       	uint8_t* buffer = fm.pack();
		       	before_conn->send(buffer, fm.length());

			//usleep(10*1000);
		}
		for(it_table = after_l2table->begin() ; it_table != after_l2table->end() ; ++it_table)
		{
			if(it_table->first == before_mac || it_table->first == after_mac)	continue;

			of10::FlowMod fm(ofvd.xid(),  //xid 
		             		123, // cookie
		               		of10::OFPFC_ADD, // command
		               		5, // idle timeout
		               		10, // hard timeout
		               		100, // priority
		               		-1, //buffer id
		               		2, // outport
		               		0); // flags

			L2TABLE::iterator imsi_it = after_l2table->find(after_mac);
			if(imsi_it == before_l2table->end())	printf("not found\n");

			of10::Match m;			//make flowmod packet
		       	m.dl_src(((uint8_t*) &it_table->first) + 2);
		       	m.dl_dst(((uint8_t*) &after_mac) + 2);
		       	fm.match(m);
		       	of10::OutputAction act(imsi_it->second , 1024);

			printf("%-10.10lx -> %-10.10lx : %u\n",it_table->first , after_mac , imsi_it->second);
		       	fm.add_action(act);
		       	uint8_t* buffer = fm.pack();
		       	after_conn->send(buffer, fm.length());

			//usleep(10*1000);
		}
	    }
	    else if(mh->type == DEL_FLOW_TABLE_REQ)
	    {
		printf("DEL FLOW TABLE REQ EXECUTED!\n");

		interface_info del_info;
		memcpy(&del_info , data , sizeof(interface_info));

		OFConnection* ofconn;
		L2TABLE* del_l2table;
		uint64_t del_mac;
		uint8_t* del_mac_data;

		del_mac_data = EthAddress::data_from_string(del_info.mac);
		memcpy(((uint8_t*)&del_mac) + 2 , (uint8_t*)del_mac_data, 6);

		std::map<uint32_t , conn_table>::iterator it_sw;
		for(it_sw = sw_table.begin() ; it_sw != sw_table.end() ; ++it_sw)
		{
			ofconn = it_sw->second.ofconn;
			del_l2table = get_l2table(ofconn);

			of10::FlowMod fm(ofvd.xid(),  //xid 
		             	123, // cookie
		               	of10::OFPFC_DELETE, // command
		               	5, // idle timeout
		              	10, // hard timeout
		              	100, // priority
		               	-1, //buffer id
		               	2, // outport
		               	0); // flags

			L2TABLE::iterator it_table;
			//for(it_table = del_l2table->begin() ; it_table != del_l2table->end() ; ++it_table)
			//{
				of10::Match m;			//make flowmod packet
				//m.dl_src(((uint8_t*) &it_table->first) + 2);	
				m.dl_src(((uint8_t*) &del_mac) + 2);
				m.dl_dst(((uint8_t*) &del_mac) + 2);
				fm.match(m);
				uint8_t* buffer = fm.pack();
				ofconn->send(buffer, fm.length());
			//}
		}	
	    }

            ssize_t size = write(fd_for_send , (char*)mh , mh->total_length);
	}
        else if (ev->get_type() == EVENT_PACKET_IN) {
            L2TABLE* l2table = get_l2table(ev->ofconn);
            if (l2table == NULL) {
                return;
            }

            uint64_t dst = 0, src = 0;
            PacketInEvent* pi = static_cast<PacketInEvent*>(ev);

            void* ofpip;
            uint8_t* data;
            uint16_t in_port;
            if (ofversion == of10::OFP_VERSION) {
                of10::PacketIn *ofpi = new of10::PacketIn();
                ofpip = ofpi;
                ofpi->unpack(pi->data);
                data = (uint8_t*) ofpi->data();
                memcpy(((uint8_t*) &dst) + 2, (uint8_t*) ofpi->data(), 6);
                memcpy(((uint8_t*) &src) + 2, (uint8_t*) ofpi->data() + 6, 6);
                in_port = ofpi->in_port();
            }
            else if (ofversion == of13::OFP_VERSION) {
                of13::PacketIn *ofpi = new of13::PacketIn();
                ;
                ofpip = ofpi;
                ofpi->unpack(pi->data);
                data = (uint8_t*) ofpi->data();
                memcpy(((uint8_t*) &dst) + 2, (uint8_t*) ofpi->data(), 6);
                memcpy(((uint8_t*) &src) + 2, (uint8_t*) ofpi->data() + 6, 6);
                if (ofpi->match().in_port() == NULL) {
                    return;
                }
                in_port = ofpi->match().in_port()->value();
            }

            // Learn the source
            (*l2table)[src] = in_port;

            // Try to find the destination
            L2TABLE::iterator it = l2table->find(dst);
            if (it == l2table->end()) {
                if (ofversion == of10::OFP_VERSION) {
                    flood10(*((of10::PacketIn*) ofpip), ev->ofconn);
                    delete (of10::PacketIn*) ofpip;
                }
                else if (ofversion == of13::OFP_VERSION) {
                    flood13(*((of13::PacketIn*) ofpip), ev->ofconn, in_port);
                    delete (of13::PacketIn*) ofpip;
                }
                return;
            }

            if (ofversion == of10::OFP_VERSION) {
                install_flow_mod10(*((of10::PacketIn*) ofpip), ev->ofconn, src,
                    dst, it->second);
                delete (of10::PacketIn*) ofpip;
            }
            else if (ofversion == of13::OFP_VERSION) {
                install_flow_mod13(*((of13::PacketIn*) ofpip), ev->ofconn, src,
                    dst, it->second);
                delete (of13::PacketIn*) ofpip;
            }
        }
        else if (ev->get_type() == EVENT_SWITCH_UP) {
            BaseLearningSwitch::event_callback(ev);
            if (ofversion == of13::OFP_VERSION) {
                install_default_flow13(ev->ofconn);
            }
        }

        else {
            BaseLearningSwitch::event_callback(ev);
        }
    }

    void install_flow_mod10(of10::PacketIn &pi, OFConnection* ofconn,
        uint64_t src, uint64_t dst, uint16_t out_port) {
        // Flow mod message
        uint8_t* buffer;
        /* Messages constructors allow to add all 
         values in a row. The fields order follows
         the specification */
        of10::FlowMod fm(pi.xid(),  //xid 
            123, // cookie
            of10::OFPFC_ADD, // command
            5, // idle timeout
            10, // hard timeout
            100, // priority
            pi.buffer_id(), //buffer id
            2, // outport
            0); // flags
        of10::Match m;
        m.dl_src(((uint8_t*) &src) + 2);
        m.dl_dst(((uint8_t*) &dst) + 2);
        fm.match(m);
        of10::OutputAction act(out_port, 1024);
	//printf("flow mod called , out_port = %u , ofconn = %u\n",out_port , ofconn->get_id());
        fm.add_action(act);
        buffer = fm.pack();
        ofconn->send(buffer, fm.length());
        OFMsg::free_buffer(buffer);
    }

    void flood10(of10::PacketIn &pi, OFConnection* ofconn) {
        uint8_t* buf;
        of10::PacketOut po(pi.xid(), pi.buffer_id(), pi.in_port());
        /*Add Packet in data if the packet was not buffered*/
        if (pi.buffer_id() == -1) {
            po.data(pi.data(), pi.data_len());
        }
        of10::OutputAction act(of10::OFPP_FLOOD, 1024);
        po.add_action(act);
        buf = po.pack();
        ofconn->send(buf, po.length());
        OFMsg::free_buffer(buf);
    }

    void install_default_flow13(OFConnection* ofconn) {
        uint8_t* buffer;
        of13::FlowMod fm(42, 0, 0xffffffffffffffff, 0, of13::OFPFC_ADD, 0, 0, 0,
            0xffffffff, 0, 0, 0);
        of13::OutputAction *act = new of13::OutputAction(of13::OFPP_CONTROLLER,
            of13::OFPCML_NO_BUFFER);
        of13::ApplyActions *inst = new of13::ApplyActions();
        inst->add_action(act);
        fm.add_instruction(inst);
        buffer = fm.pack();
        ofconn->send(buffer, fm.length());
        OFMsg::free_buffer(buffer);
    }

    void install_flow_mod13(of13::PacketIn &pi, OFConnection* ofconn,
        uint64_t src, uint64_t dst, uint32_t out_port) {
        // Flow mod message
        uint8_t* buffer;
        /*You can also set the message field using
         class methods which have the same names from
         the field present on the specification*/
        of13::FlowMod fm;
        fm.xid(pi.xid());
        fm.cookie(123);
        fm.cookie_mask(0xffffffffffffffff);
        fm.table_id(0);
        fm.command(of13::OFPFC_ADD);
        fm.idle_timeout(5);
        fm.hard_timeout(10);
        fm.priority(100);
        fm.buffer_id(pi.buffer_id());
        fm.out_port(0);
        fm.out_group(0);
        fm.flags(0);
        of13::EthSrc fsrc(((uint8_t*) &src) + 2);
        of13::EthDst fdst(((uint8_t*) &dst) + 2);
        fm.add_oxm_field(fsrc);
        fm.add_oxm_field(fdst);
        of13::OutputAction act(out_port, 1024);
        of13::ApplyActions inst;
        inst.add_action(act);
        fm.add_instruction(inst);
        buffer = fm.pack();
        ofconn->send(buffer, fm.length());
        OFMsg::free_buffer(buffer);
        of13::Match m;
        of13::MultipartRequestFlow rf(2, 0x0, 0, of13::OFPP_ANY, of13::OFPG_ANY,
            0x0, 0x0, m);
        buffer = rf.pack();
        ofconn->send(buffer, rf.length());
        OFMsg::free_buffer(buffer);
    }

    void flood13(of13::PacketIn &pi, OFConnection* ofconn, uint32_t in_port) {
        uint8_t* buf;
        of13::PacketOut po(pi.xid(), pi.buffer_id(), in_port);
        /*Add Packet in data if the packet was not buffered*/
        if (pi.buffer_id() == -1) {
            po.data(pi.data(), pi.data_len());
        }
        of13::OutputAction act(of13::OFPP_FLOOD, 1024); // = new of13::OutputAction();
        po.add_action(act);
        buf = po.pack();
        ofconn->send(buf, po.length());
        OFMsg::free_buffer(buf);
    }
};

#endif
