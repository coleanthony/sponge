#include "tcp_connection.hh"

#include <iostream>
#include "tcp_state.hh"

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::set_rst_flag(){
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _linger_after_streams_finish=false;
    _is_active=false;
}

//在发送当前数据包之前，TCPConnection 会获取当前它自己的 TCPReceiver 的 ackno 和 window size，将其放置进待发送 TCPSegment 中，并设置其 ACK 标志
void TCPConnection::send_segment() {
    if(!_is_active) return;
    while(!_sender.segments_out().empty()){
        TCPSegment seg=_sender.segments_out().front();
        _sender.segments_out().pop();
        seg.header().win=_receiver.window_size();
        //设置ackno
        if(_receiver.ackno().has_value()){
            seg.header().ack=true;
            seg.header().ackno=_receiver.ackno().value();
        }
        _segments_out.push(seg);
    }
}

void TCPConnection::send_rst_segment(){
    if(!_sender.segments_out().empty()){
        TCPSegment seg=_sender.segments_out().front();
        _sender.segments_out().pop();
        seg.header().rst=true;
        _segments_out.push(seg);
    }
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    //清零倒计时
    _time_since_last_segment_received=0;
    //如果 RST 标志位被设置，那么立刻将 inbound 和 outbound 流置于 error state，并永久关闭当前连接
    if(seg.header().rst){
        set_rst_flag();
        return;
    }
    //将 segment 交给 TCPReceiver，以便它可以检查传入段上关心的字段：seqno、SYN、payload 和 FIN
    _receiver.segment_received(seg);

    //如果segment有序列号，TCPConnection确保至少有一个segment回复，以反映 ackno 和 window_size 的更新
    //keep alive机制也要空包
    bool need_empty_ack=seg.length_in_sequence_space();
    //如果ack标志位被设置，传入tcpsender
    if(seg.header().ack){
        _sender.ack_received(seg.header().ackno,seg.header().win);
        if(need_empty_ack&&!_sender.segments_out().empty()){
            need_empty_ack=false;
        }
    }

    //状态转换
    //listen时收到syn，进入syn_rcvd状态
    if(TCPState::state_summary(_sender)==TCPSenderStateSummary::CLOSED&&TCPState::state_summary(_receiver)==TCPReceiverStateSummary::SYN_RECV){
        connect();
        return;
    }
    //判断是否为passive close
    if(TCPState::state_summary(_sender)==TCPSenderStateSummary::SYN_ACKED&&TCPState::state_summary(_receiver)==TCPReceiverStateSummary::FIN_RECV){
        _linger_after_streams_finish=false;
    }
    //passive close
    if(!_linger_after_streams_finish&&TCPState::state_summary(_sender)==TCPSenderStateSummary::FIN_ACKED&&TCPState::state_summary(_receiver)==TCPReceiverStateSummary::FIN_RECV){
        _is_active=false;
        return;
    }

    //keep alive
    if(_receiver.ackno().has_value()&&seg.length_in_sequence_space()==0&&seg.header().ackno==_receiver.ackno().value()-1){
        need_empty_ack=true;
    }
    if(need_empty_ack){
        _sender.send_empty_segment();
    }
    send_segment();
}

bool TCPConnection::active() const { return _is_active; }

//当 TCPSender 将一个 TCPSegment 数据包添加到待发送队列中时，TCPConnection 需要从中取出并将其发送。
size_t TCPConnection::write(const string &data) {
    if(data.empty())    return 0;
    size_t writelen=_sender.stream_in().write(data);
    _sender.fill_window();
    send_segment();
    return writelen;
}

//输入流的报文已经结束并全部组装完毕
bool TCPConnection::check_input_end(){
    if(_receiver.stream_out().buffer_empty()&&_receiver.unassembled_bytes()==0){
        return true;
    }
    return false;
}

//输出流的报文已经结束并且已经全部发送给对方（包括 FIN）
bool TCPConnection::check_output_end(){
    if(_sender.stream_in().buffer_empty()&&_sender.bytes_in_flight()==0&&_sender.next_seqno_absolute()==_sender.stream_in().bytes_written()+2){
        return true;
    }
    return false;
}

//告知 TCPSender 时间的流逝，这可能会让 TCPSender 重新发送被丢弃的数据包
//如果连续重传次数超过 TCPConfig::MAX RETX ATTEMPTS，则发送一个 RST 包。
//在条件适合的情况下关闭 TCP 连接（当处于 TCP 的 TIME_WAIT 状态时）。
//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    //更新倒计时
    _time_since_last_segment_received+=ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    //如果连续重传次数超过 TCPConfig::MAX RETX ATTEMPTS，则发送一个 RST 包。
    if(_sender.consecutive_retransmissions()>TCPConfig::MAX_RETX_ATTEMPTS){
        while(!_sender.segments_out().empty()){
            _sender.segments_out().pop();
        }
        send_rst_segment();
        return;
    }
    
    send_segment();
    //在条件适合的情况下关闭 TCP 连接
    //clean shutdown
    if(check_input_end()&&check_output_end()){
        if(!_linger_after_streams_finish){
            _is_active=false;
        }else if(_linger_after_streams_finish&&_time_since_last_segment_received>=10*TCPConfig::TIMEOUT_DFLT){
            _is_active=false;
        }
    }
}

//关闭发送窗口
void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    send_segment();
}

//发送SYN包
void TCPConnection::connect() {
    _sender.fill_window();
    send_segment();                                 
}

//析构，发送RST包
TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            // Your code here: need to send a RST segment to the peer
            _sender.stream_in().set_error();
            _receiver.stream_out().set_error();
            _is_active=false;
            //发送RST包
            send_rst_segment();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
