#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <queue>

//! \brief The timer for TCP's ticker implementation
class Timer{
  private:
    uint32_t _time_count;
    uint32_t _time_out;
    bool _is_running;
  public:
    Timer()=default;
    Timer(uint32_t time_out): _time_count(0),_time_out(time_out),_is_running(false){}
    void start(){_is_running=true;}
    void stop(){_is_running=false;}
    void reset(){_time_count=0;}
    void restart(){_is_running=true,_time_count=0;}
    bool is_running() {return _is_running;}
    uint32_t get_time_count() {return _time_count;}
    uint32_t get_time_out() {return _time_out;}
    void set_time_out(uint32_t time_out)  {_time_out=time_out;}
    void tick(size_t ms_since_last_tick)  {
      if (_is_running){
        _time_count+=ms_since_last_tick;
      }
    }
    bool check_time_out(){
      if (_is_running && _time_count>=_time_out){
        return true;
      }
      return false;
    }
};

//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
//1.将 ByteStream 中的数据以 TCP 报文形式持续发送给接收者。
//2.处理 TCPReceiver 传入的 ackno 和 window size，以追踪接收者当前的接收状态，以及检测丢包情况。
//3.若经过一个超时时间后仍然没有接收到 TCPReceiver 发送的针对某个数据包的 ack 包，则重传对应的原始数据包。
class TCPSender {
  private:
    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;
    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out{};
    //! retransmission timer for the connection
    unsigned int _initial_retransmission_timeout;
    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;
    //! the (absolute) sequence number for the next byte to be sent
    uint64_t _next_seqno{0};
    //! timer
    Timer _timer;
    //the number of bytes that have been sent but not acknowledged
    uint64_t _bytes_in_flight{0};
    //the initial window size
    uint64_t _window_size{1};
    //the number of consecutive retransmission
    uint64_t _consecutive_retransmissions_count{0};
    //contain syn flag or fin flay;
    bool is_sync_flag{false},is_fin_flag{false};
    //the segments that have been sent but not acknowledged
    std::queue<std::pair<uint64_t,TCPSegment>> _segments_in_flight{};

  public:
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
