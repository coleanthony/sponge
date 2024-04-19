#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

/*
!1.Listen waiting for syn:ackno is empty
!2.syn_recv: ackno exists and input stream hasn't ended
!3.fin_recv
*/
void TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader &header = seg.header();
    //listen阶段
    if(!_is_sync_set){
        if (!header.syn){
            return;
        }
        _is_sync_set=true;
        _isn=header.seqno;
    }
    //syn_recv阶段
    uint64_t checkpoint=_reassembler.stream_out().bytes_written();
    uint64_t abs_no=unwrap(header.seqno,_isn.value(),checkpoint);
    uint64_t seqnum=abs_no-1+header.syn;
    _reassembler.push_substring(seg.payload().copy(),seqnum,header.fin);
}

std::optional<WrappingInt32> TCPReceiver::ackno() const {
    if(!_is_sync_set){
        return std::nullopt;
    }
    //第一个没有收到的，所以+1，fin标志位+1
    return wrap(_reassembler.stream_out().bytes_written()+1+_reassembler.stream_out().input_ended(), _isn.value());
}

size_t TCPReceiver::window_size() const {
    return _capacity-_reassembler.stream_out().buffer_size();
}
