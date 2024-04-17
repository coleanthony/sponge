#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <optional>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    ,_timer(retx_timeout) {}

//在传输的部分的字节数
uint64_t TCPSender::bytes_in_flight() const {return _bytes_in_flight;}

void TCPSender::fill_window() {
    uint16_t window_size=_window_size?_window_size:1;
    while(_window_size>_bytes_in_flight){
        TCPSegment tcpsegment;
        if(!is_sync_flag){
            //一开始发SYN包
            is_sync_flag=true;
            tcpsegment.header().syn=true;
        }
        //目前还能放进的字节数量
        size_t sendlen=min(TCPConfig::MAX_PAYLOAD_SIZE,(_stream.buffer_size(),window_size-_bytes_in_flight-tcpsegment.header().syn));
        //读取字节数
        std::string ss=_stream.read(sendlen);
        //填充数据
        tcpsegment.payload()=Buffer(std::move(ss));

        //如果第一次到达FIN
        if(!is_fin_flag&&tcpsegment.length_in_sequence_space()<window_size&&_stream.eof()){
            is_fin_flag=true;
            tcpsegment.header().fin=true;
        }

        //空包就不发送了
        if(tcpsegment.length_in_sequence_space()==0){
            break;
        }

        tcpsegment.header().seqno=next_seqno();
        if(!_timer.is_running()){
            _timer.restart();
        }

        //发送到_segments_out，保存还在flight的到_segments_in_flight
        _segments_out.push(tcpsegment);
        _bytes_in_flight+=tcpsegment.length_in_sequence_space();
        _segments_in_flight.push(make_pair(_next_seqno,tcpsegment));
        _next_seqno+=tcpsegment.length_in_sequence_space();
    }
}

//1. Set the RTO back to its “initial value.”
//2. If the sender has any outstanding data, restart the retransmission timer so that it 
//   will expire after RTO milliseconds (for the current value of RTO).
//3. Reset the count of “consecutive retransmissions” back to zero.
//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    //首先将收到的 ackno 转化为 absolute ackno ，便于后续处理已经收到的包
    uint64_t absolute_ackno=unwrap(ackno,_isn,_next_seqno);
    // 传入的 ACK 是不可靠的，直接丢弃
    if(absolute_ackno>_next_seqno)  return;

    while(!_segments_in_flight.empty()){
        size_t segment_size=_segments_in_flight.front().second.length_in_sequence_space();
        if(_segments_in_flight.front().first+segment_size<=absolute_ackno){
            //删除已经收到的包
            _bytes_in_flight-=segment_size;
            _segments_in_flight.pop();
            _timer.reset();
        }else{
            break;
        }
    }

    //最后将连续重传计数清零。
    _consecutive_retransmissions_count=0;
    _window_size=window_size;
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
//Periodically, the owner of the TCPSender will call the TCPSender’s tick method, indicating the passage of time.
void TCPSender::tick(const size_t ms_since_last_tick) {
    _timer.tick(ms_since_last_tick);
    //超时且有未发送的包
    if(_timer.check_time_out()&&!_segments_in_flight.empty()){
        _segments_out.push(_segments_in_flight.front().second);
        //重传
        if(_window_size>0){
            _timer.set_time_out(_timer.get_time_out()*2);
            _consecutive_retransmissions_count++;
        }
        _timer.restart();
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions_count; }

void TCPSender::send_empty_segment() {
    TCPSegment tcpsegment;
    tcpsegment.header().seqno=next_seqno();
    _segments_out.push(tcpsegment);
}
