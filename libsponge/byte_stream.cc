#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) {
    _capacity=capacity;
}

size_t ByteStream::write(const string &data) {
    size_t cap=min(data.length(),remaining_capacity());
    for(size_t i=0;i<cap;i++){
        _buffer.push_back(data[i]);
    }
    _write_count+=cap;
    return cap;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t cplen=min(len,buffer_size());
    return {_buffer.begin(),_buffer.begin()+cplen};
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { 
    size_t popnum=min(len,buffer_size());
    _buffer.erase(_buffer.begin(),_buffer.begin()+popnum);
    _read_count+=popnum;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    std::string res=peek_output(len);
    pop_output(len);
    return res;
}

void ByteStream::end_input() {  _intputendflag=true;}

bool ByteStream::input_ended() const { return _intputendflag; }

size_t ByteStream::buffer_size() const { return _buffer.size(); }

bool ByteStream::buffer_empty() const { return _buffer.empty(); }

bool ByteStream::eof() const { return buffer_empty()&&input_ended(); }

size_t ByteStream::bytes_written() const { return _write_count; }

size_t ByteStream::bytes_read() const { return _read_count; }

size_t ByteStream::remaining_capacity() const { return _capacity-_buffer.size(); }
