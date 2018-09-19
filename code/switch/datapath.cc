#include "datapath.hh"
#include "port.hh"
#include "packets.h"
#include "table.hh"

#define BUF_SIZE 1024

uint32_t Datapath::buffer_id = 0;
static std::map<uint32_t , Node> table;
static uint8_t packet[BUF_SIZE*2 + 1];

char MessageTypeStr[25][25] = {
	"MESSAGE_NAK",

	"INTERFACE_INFO_REQ",
	"INTERFACE_INFO_ACK",

	"STA_INFO_REQ",
	"STA_INFO_ACK",

	"THRESHOLD_VIOLATE",
	"THRESHOLD_VIOLATE_ACK",

	"HELLO_INFO",
	"HELLO_INFO_ACK",

	"WIFI_CHANGED",
	"WIFI_CHANGED_ACK",

	"DEL_FLOW_TABLE_REQ",
	"DEL_FLOW_TABLE_ACK",
};

Datapath::~Datapath(){
	for(std::map<uint32_t, struct packet*>::iterator it = this->pkt_buffer.begin();
		  				it != this->pkt_buffer.end(); ++it){
		delete it->second;
	}
}

void Datapath::handle_barrier_request(uint8_t* data){	
	OFMsg msg(data);
	of10::BarrierReply br(msg.xid());
	uint8_t* buffer = br.pack();
	(*(this->conn))->send(buffer, br.length());
	OFMsg::free_buffer(buffer);	
}

void Datapath::action_handler(Action *act, struct packet *pkt){
	//printf("action handler start\n");		
 		switch(act->type()){ 			
 			case of10::OFPAT_OUTPUT:{ 				
 				of10::OutputAction *oa = static_cast<of10::OutputAction*>(act);
 				//send the packet through the same port 				 				
 				if(oa->port() == of10::OFPP_IN_PORT){
					//printf("case ofpp_in_port , pkt->in_port = %d\n",pkt->in_port);
 					SwPort *port = this->ports[pkt->in_port - 1];
 					port->port_output_packet(pkt->data, pkt->len);
 				}
 				/*Flood or All. As we are not dealing with STP the actions are the same*/
 				else if(oa->port() ==  of10::OFPP_FLOOD || oa->port() ==  of10::OFPP_ALL){ 
					//printf("case ofpp flood\n");
 					for(std::vector<SwPort*>::iterator it = this->ports.begin();
		  				it != this->ports.end(); ++it){
 						if((*it)->port_no() == pkt->in_port)
 							continue;
 						(*it)->port_output_packet(pkt->data, pkt->len);
 					}
 				}
 				else if (oa->port() == of10::OFPP_CONTROLLER){
					//printf("case ofpp_controller\n");
 					Datapath::buffer_id++;
 					this->pkt_buffer.insert(std::pair<uint32_t,struct packet*>(Datapath::buffer_id,pkt));
 					uint16_t max_len = pkt->len > oa->max_len()? oa->max_len():pkt->len;
 					of10::PacketIn pi(21,  -1, pkt->in_port, pkt->len, of10::OFPR_ACTION);
			 		pi.data(pkt->data, max_len);
			 		uint8_t* buffer  = pi.pack();
			 		(*(this->conn))->send(buffer, pi.length());
			 		OFMsg::free_buffer(buffer);
 				}
				else if(oa->port() == of10::OFPP_UDP_VENDOR)
				{
					uint8_t* data = pkt->data;
                                        //Datapath::buffer_id++;
                                        //this->pkt_buffer.insert(std::pair<uint32_t,struct packet*>(Datapath::buffer_id,pkt));

					of10::Vendor vd(0xeeea , 32);

					uint64_t src_mac =0 , dst_mac =0;

					memcpy(((uint8_t*) &dst_mac) + 2, (uint8_t*)data, 6);
                			memcpy(((uint8_t*) &src_mac) + 2, (uint8_t*)data+ 6, 6);

					data = data+sizeof(eth_header);

					uint32_t src_ip , dst_ip;

					src_ip = ((ip_header*)data)->ip_src;
					dst_ip = ((ip_header*)data)->ip_dst;

					data = data+sizeof(ip_header);
					uint16_t sport , dport;

					sport = htons(((udp_header*)data)->udp_src);
					dport = htons(((udp_header*)data)->udp_dst);

					data = data + sizeof(udp_header);

					msg_header* mh = (msg_header*)data;
					/*printf("-----------------------------------------------------\n");
					printf("packet recv from mobile node! id : %u\n",mh->user_id);
					printf("mh->total_length = %u\n",mh->total_length);
					printf("mh->user_id = %u\n",mh->user_id);
					printf("oa->max_len() = %u\n",oa->max_len());*/


					printf("MN -> AP -> Controller\n");
					printf("user_id = %u , total_length = %u , type = %s\n",mh->user_id , mh->total_length , MessageTypeStr[mh->type]);

					uint16_t max_len = 24 + mh->total_length > oa->max_len()? oa->max_len():24+mh->total_length;

					//vendor_header vh = {mh->user_id , src_mac , src_ip ,dst_ip , sport , dport};
					memcpy(packet , &mh->user_id , sizeof(uint32_t));
					memcpy(packet + 4 , &src_mac , sizeof(uint64_t));
					memcpy(packet + 12 , &src_ip , sizeof(uint32_t));
					memcpy(packet + 16 , &dst_ip , sizeof(uint32_t));
					memcpy(packet + 20 , &sport , sizeof(uint16_t));
					memcpy(packet + 22 , &dport , sizeof(uint16_t));
					memcpy(packet + 24 , (uint8_t*)data , mh->total_length);
					//printf("sizeof(vendor_header) = %lu\n",sizeof(struct vendor_header_t));
					//memcpy(packet , (uint8_t*)&vh , sizeof(vh));
					//memcpy(packet + sizeof(vendor_header) , (uint8_t*)data , mh->total_length);

					vd.data(packet , max_len);
					//vd.data(packet , oa->max_len());

                                        uint8_t* buffer = vd.pack();


                                        (*(this->conn))->send(buffer, vd.length());	
                                        OFMsg::free_buffer(buffer);
					//OFMsg::free_buffer(packet);
					//printf("after free buffer\n");	
				}
 				else {
					//printf("case else , oa->port() = %d\n" , oa->port());
 					SwPort *port = this->ports[oa->port()-1];
 					port->port_output_packet(pkt->data, pkt->len);
 				}
 				break;
 			}

 		}
}

void Datapath::handle_flow_mod(uint8_t *data){
	of10::FlowMod fm;
	fm.unpack((uint8_t*)data);
	switch(fm.command()){
		case of10::OFPFC_ADD:{
			Flow flow;
			flow.priority_ = fm.priority();
			flow.match = fm.match();
			flow.actions = fm.actions();					

			for(std::set<Flow>::iterator it = this->flow_table.begin();
                                it != this->flow_table.end(); ++it){
                                if (Flow::strict_match(*it, flow)){
                                        this->flow_table.erase(it);
                                        break;
                                }
                        }

			this->flow_table.insert(flow);
			if(fm.buffer_id() != -1) {
				struct packet *pkt =  this->pkt_buffer[fm.buffer_id()];	
				of10::Match pkt_match;
				pkt_match.in_port(pkt->in_port);
				Flow::extract_flow_fields(pkt_match, pkt->data, pkt->len);
				if(Flow::pkt_match(flow, pkt_match)){
					//Apply the actions and leave
					ActionList l = flow.actions;
				    std::list<Action*> acts = l.action_list();			    
					for(std::list<Action*>::iterator act_it = acts.begin();
	 					act_it != acts.end(); ++act_it){					
						action_handler(*act_it, pkt);
	 				}
	 			delete pkt;	
				}								
			}

			break;					
		}
		//Delete all matching flows
		case of10::OFPFC_DELETE:{				
				Flow flow;
				flow.match = fm.match();
				if(!strcmp(flow.match.dl_src().to_string().c_str() , flow.match.dl_dst().to_string().c_str()))
				{
					std::set<Flow>::iterator it_imsi;
					for(it_imsi = this->flow_table.begin(); it_imsi != this->flow_table.end() ;)
					{
						of10::Match mt = it_imsi->match;
						if(!strcmp(mt.dl_src().to_string().c_str() , flow.match.dl_src().to_string().c_str()))
						{
							this->flow_table.erase(it_imsi++);
							continue;
						}
						else if(!strcmp(mt.dl_dst().to_string().c_str() , flow.match.dl_dst().to_string().c_str()))
						{
							this->flow_table.erase(it_imsi++);
							continue;
						}
						else{
							++it_imsi;
						}
					}
				}
				std::set<Flow>::iterator it; 
				for (it = this->flow_table.begin(); it != 
					this->flow_table.end(); ) {
				    if (Flow::non_strict_match(flow, *it)) {
				        this->flow_table.erase(it++);
				    }
				    else {
				        ++it;
				    }
				}
			}			
			break;
		}

                                printf("==================================================\n");
                        for(std::set<Flow>::iterator it = this->flow_table.begin();
                                it != this->flow_table.end(); ++it){

                                of10::Match mt = it->match;
                                ActionList aclist = it->actions;
                                std::list<Action*> la = aclist.action_list();
                                uint16_t op;
                                uint64_t sm , dm;

                                for(std::list<Action*>::iterator ita = la.begin() ; ita != la.end() ; ++ita){
                                       of10::OutputAction* imsi = (of10::OutputAction*)(*ita);
                                       op = imsi->port();
                                }

                                printf("%s -> %s : %u\n", mt.dl_src().to_string().c_str() , mt.dl_dst().to_string().c_str() , op);
                        }
                                printf("==================================================\n");
}

void Datapath::handle_packet_out(uint8_t* data){
	of10::PacketOut po;
	po.unpack((uint8_t*)data);
	ActionList l = po.actions();
	std::list<Action*> acts = l.action_list();
	struct packet *pkt;
	if(po.buffer_id() == -1){
		pkt = new struct packet();
		pkt->in_port = po.in_port();
		pkt->data = (uint8_t*) po.data();
		pkt->len = po.data_len();
	}
	else {
		pkt = this->pkt_buffer[po.buffer_id()];				
	}
	for(std::list<Action*>::const_iterator it = acts.begin();
			it != acts.end(); ++it){ 								
		action_handler(*it, pkt);
	}
	delete pkt;
}

