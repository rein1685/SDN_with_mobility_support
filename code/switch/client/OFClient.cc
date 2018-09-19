#include "OFClient.hh"
#include <fluid/of10msg.hh>
#include "table.hh"
#define BUF_SIZE 512
#define SEND_FIFO "/tmp/send_fifo"

using namespace fluid_base;
using namespace fluid_msg;

extern std::map<uint32_t , Node> table;
static int sock;
int fd_for_send;
static uint8_t* packet;

char MessageTypeStr2[25][25] = {
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

void OFClient::base_message_callback(BaseOFConnection* c, void* data, size_t len) {
    uint8_t type = ((uint8_t*) data)[1];
    OFConnection* cc = (OFConnection*) c->get_manager();

    // We trust that the other end is using the negotiated protocol
    // version. Should we?

    if (ofsc.liveness_check() and type == of10::OFPT_ECHO_REQUEST) {
        uint8_t msg[8];
        memset((void*) msg, 0, 8);
        msg[0] = ((uint8_t*) data)[0];
        msg[1] = of10::OFPT_ECHO_REPLY;
        ((uint16_t*) msg)[1] = htons(8);
        ((uint32_t*) msg)[1] = ((uint32_t*) data)[1];
        // TODO: copy echo data
        c->send(msg, 8);

        if (ofsc.dispatch_all_messages()) goto dispatch; else goto done;
    }

    if (ofsc.handshake() and type == of10::OFPT_HELLO) {

        int str_len;
        socklen_t adr_sz;

        //printf("message = %s\n",message);
	fd_for_send = open(SEND_FIFO , O_RDWR);
        sock = socket(PF_INET , SOCK_DGRAM , 0);
        if(sock == -1)  printf("socket() error\n");

        uint8_t version = ((uint8_t*) data)[0];
        if (not this->ofsc.supported_versions() & (1 << (version - 1))) {
            uint8_t msg[12];
            memset((void*) msg, 0, 8);
            msg[0] = version;
            msg[1] = of10::OFPT_ERROR;
            ((uint16_t*) msg)[1] = htons(12);
            ((uint32_t*) msg)[1] = ((uint32_t*) data)[1];
            ((uint16_t*) msg)[4] = htons(of10::OFPET_HELLO_FAILED);
            ((uint16_t*) msg)[5] = htons(of10::OFPHFC_INCOMPATIBLE);
            cc->send(msg, 12);
            cc->close();
            cc->set_state(OFConnection::STATE_FAILED);
            connection_callback(cc, OFConnection::EVENT_FAILED_NEGOTIATION);
        }

        if (ofsc.dispatch_all_messages()) goto dispatch; else goto done;
    }

    if (ofsc.liveness_check() and type == of10::OFPT_ECHO_REPLY) {
        if (ntohl(((uint32_t*) data)[1]) == ECHO_XID) {
            cc->set_alive(true);
        }

        if (ofsc.dispatch_all_messages()) goto dispatch; else goto done;
    }

    if (ofsc.handshake() and type == of10::OFPT_FEATURES_REQUEST) {
        cc->set_version(((uint8_t*) data)[0]);
        cc->set_state(OFConnection::STATE_RUNNING);        
        of10::FeaturesRequest freq;
        freq.unpack((uint8_t*) data);
        of10::FeaturesReply fr(freq.xid(), this->datapath_id, 1, 1, 0x0, 0x0);
        uint8_t *buffer =  fr.pack();
        c->send(buffer, fr.length());
        OFMsg::free_buffer(buffer);

        if (ofsc.liveness_check())
            c->add_timed_callback(send_echo, ofsc.echo_interval() * 1000, cc);
        connection_callback(cc, OFConnection::EVENT_ESTABLISHED);

        goto dispatch;
    }
    if (ofsc.handshake() and type == of10::OFPT_VENDOR) {	//if controller send vendor message, process
	uint32_t src_ip , dst_ip;
	uint16_t sport , dport;
	uint64_t src_mac;
	uint32_t user_id;
	char message[BUF_SIZE];
	msg_header* mh;

	of10::Vendor vd;
	vd.unpack((uint8_t*)data);
	
	//printf("vendor message recv\n");
	packet = (uint8_t*)vd.data();
	
	//vendor_header *vh = (vendor_header*)packet;
	//packet += sizeof(vendor_header);
	memcpy(&user_id , (uint8_t*)packet , sizeof(uint32_t));
	memcpy(&src_mac , (uint8_t*)(packet + 4) , sizeof(uint64_t));
	memcpy(&src_ip , (uint8_t*)(packet + 12) , sizeof(uint32_t));
	memcpy(&dst_ip , (uint8_t*)(packet + 16) , sizeof(uint32_t));
	memcpy(&sport , (uint8_t*)(packet + 20 ), sizeof(uint16_t));
	memcpy(&dport , (uint8_t*)(packet + 22) , sizeof(uint16_t));
	//memcpy(&mh , packet + 24 , sizeof(msg_header));
	mh = (msg_header*)(packet + 24);
	
	uint16_t total_length;
	memcpy(&total_length , (uint8_t*)(packet + 30) , sizeof(uint16_t));

	printf("Controller -> AP -> MN\n");
	printf("user_id = %u , total_length = %u , type = %s\n",mh->user_id , mh->total_length , MessageTypeStr2[mh->type]);
	//printf("vendor message : %x %lx %x %x\n",vh->id , vh->mac , vh->ip , vh->port);

	if(!src_ip || !sport || !src_mac)
	{
		printf("Switch packet from controller recv : %d\n",fd_for_send);
		write(fd_for_send , (char*)mh , mh->total_length);
	}
	else
	{
		int str_len;
		socklen_t adr_sz;

		struct sockaddr_in dst_adr , src_adr;
		if(sock == -1)	printf("socket() error\n");

		memset(&dst_adr , 0 , sizeof(dst_adr));
		dst_adr.sin_family = AF_INET;
		dst_adr.sin_addr.s_addr = htonl(src_ip);
		dst_adr.sin_port = sport;

        	memset(&src_adr , 0 , sizeof(src_adr));
        	src_adr.sin_family = AF_INET;
        	src_adr.sin_addr.s_addr = htonl(dst_ip);
        	src_adr.sin_port = dport;


       		if(bind(sock , (struct sockaddr*)&src_adr , sizeof(src_adr))==-1);


		ssize_t ssize;
		ssize = sendto(sock , (char*)mh , mh->total_length , 0 , (struct sockaddr*)&dst_adr , sizeof(dst_adr));
	}
	goto dispatch;
    }

    goto dispatch;

    // Dispatch a message and goto done
    dispatch:
        message_callback(cc, type, data, len);
        goto done;
    // Free the message and return
    done:
        c->free_data(data);
        return;
}

void OFClient::base_connection_callback(BaseOFConnection* c, BaseOFConnection::Event event_type) {
    /* If the connection was closed, destroy it.
    There's no need to notify the user, since a DOWN event already
    means a CLOSED event will happen and nothing should be expected from
    the connection. */
    if (event_type == BaseOFConnection::EVENT_CLOSED) {
        BaseOFClient::base_connection_callback(c, event_type);        
        // TODO: delete the OFConnection?
        return;
    }

    int conn_id = c->get_id();
    if (event_type == BaseOFConnection::EVENT_UP) {
        if (ofsc.handshake()) {
            struct of10::ofp_hello msg;
            msg.header.version = this->ofsc.max_supported_version();
            msg.header.type = of10::OFPT_HELLO;
            msg.header.length = htons(8);
            msg.header.xid = HELLO_XID;
            c->send(&msg, 8);
        }

		this->conn = new OFConnection(c, this);
        connection_callback(this->conn, OFConnection::EVENT_STARTED);
    }
    else if (event_type == BaseOFConnection::EVENT_DOWN) {
        connection_callback(this->conn, OFConnection::EVENT_CLOSED);
    }
}

void OFClient::free_data(void* data) {
    BaseOFClient::free_data(data);
}

void* OFClient::send_echo(void* arg) {
    OFConnection* cc = static_cast<OFConnection*>(arg);

    if (!cc->is_alive()) {
        cc->close();
        cc->get_ofhandler()->connection_callback(cc, OFConnection::EVENT_DEAD);
        return NULL;
    }

    uint8_t msg[8];
    memset((void*) msg, 0, 8);
    msg[0] = (uint8_t) cc->get_version();
    msg[1] = of10::OFPT_ECHO_REQUEST;
    ((uint16_t*) msg)[1] = htons(8);
    ((uint32_t*) msg)[1] = htonl(ECHO_XID);

    cc->set_alive(false);
    cc->send(msg, 8);

    return NULL;
}



