#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>
#include <optional>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    if (_arp_table.count(next_hop_ip)) {
        //if the destination Ethernet address is already known, send it right away.
        EthernetFrame frame;
        frame.header().dst = _arp_table[next_hop_ip].ethernet_address;
        frame.header().src = _ethernet_address;
        frame.header().type = EthernetHeader::TYPE_IPv4;
        frame.payload() = dgram.serialize();
        _frames_out.push(frame);
    } else {
        //If the destination Ethernet address is unknown, broadcast an ARP request for the next hop’s Ethernet address, 
        //and queue the IP datagram so it can be sent after the ARP reply is received.
        if(!_datagram_to_send.count(next_hop_ip)){
            //if it's not in the _datagram_to_send, add it.
            ARPMessage arp_request;
            arp_request.opcode = ARPMessage::OPCODE_REQUEST;
            arp_request.sender_ethernet_address = _ethernet_address;
            arp_request.sender_ip_address = _ip_address.ipv4_numeric();
            arp_request.target_ip_address = next_hop_ip;
            //boardcast the arp_request
            EthernetFrame arp_request_frame;
            arp_request_frame.header().dst = ETHERNET_BROADCAST;
            arp_request_frame.header().src = _ethernet_address;    
            arp_request_frame.header().type = EthernetHeader::TYPE_ARP;
            arp_request_frame.payload() = arp_request.serialize();
            _frames_out.push(arp_request_frame);

            //queue the datagram
            _datagram_to_send[next_hop_ip].push_back(dgram);

        }
        
    }
    
}

//! \param[in] frame the incoming Ethernet frame
std::optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if(frame.header().dst!=_ethernet_address&&frame.header().dst!=ETHERNET_BROADCAST){
        return nullopt;
    }
    //if it's ipv4, parse it and return
    if(frame.header().type==EthernetHeader::TYPE_IPv4){
        InternetDatagram ipv4_datagram;
        if(ipv4_datagram.parse(frame.header().payload)==NoError)    return ipv4_datagram;
        return nullopt;
    }

    //if it's arp
    if(frame.header().type==EthernetHeader::TYPE_ARP){
        ARPMessage arp_message;
        if(arp_message.parse(frame.header().payload)!=NoError)    return nullopt;

        uint32_t cur_ip_addr=ip_address().ipv4_numeric();
        //arp request
        if(arp_message.op==ARPMessage::OPCODE_REQUEST&&arp_message.target_ip_addr==cur_ip_addr){
            //make arp_reply
            ARPMessage arp_reply;
            arp_reply.op=ARPMessage::OPCODE_REPLY;
            arp_reply.sender_ethernet_address=_ethernet_address;
            arp_reply.sender_ip_address=cur_ip_addr;
            arp_reply.target_ethernet_address=arp_message.sender_ethernet_address;
            arp_reply.target_ip_address=arp_message.sender_ip_address;
            //send
            EthernetFrame arp_reply_frame;
            arp_request_frame.header().dst = arp_message.sender_ethernet_address;
            arp_request_frame.header().src = _ethernet_address; 
            arp_request_frame.header().type = EthernetHeader::TYPE_ARP;
            arp_request_frame.payload() = arp_reply.serialize();
            _frames_out.push(arp_reply_frame);
        }

        //Learn mappings from both requests and replies.
        _arp_table[arp_message.sender_ip_address]={arp_message.sender_ethernet_address,ARP_ENTRY_TTL_MS};
        //send the InternetDatagram in _datagram_to_send
        if(_datagram_to_send.count(arp_message.sender_ip_address)){
            for(auto &datagram: _datagram_to_send[arp_message.sender_ip_address]){
                EthernetFrame frame;
                frame.header().dst = arp_message.sender_ethernet_address
                frame.header().src = _ethernet_address;
                frame.header().type = EthernetHeader::TYPE_IPv4;
                frame.payload() = datagram.serialize();
                _frames_out.push(frame);
            }
            _datagram_to_send.erase(arp_message.sender_ip_address);
        }
    }
    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    //update arp_table
    for(auto it = _arp_table.begin(); it != _arp_table.end();){
        if(it->second.timer <= ms_since_last_tick){
            it = _arp_table.erase(it);
        }else{
            it->second.timer -= ms_since_last_tick;
            it++;
        }
    }
    //udpate arp_repuest
    for(auto it=_arp_timer.begin();it!=_arp_timer.end();){
        if(if->second<=ms_since_last_tick){
            
        }else{
            it->second-=ms_since_last_tick;
            it++;
        }
    }
}
