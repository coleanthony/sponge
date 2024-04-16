#include "stream_reassembler.hh"
#include <vector>
#include <iostream>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) :_buffer(capacity),_output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t st=max(index,_curindex);
    size_t ed=min(index+data.size(),min(_curindex+_capacity-_output.buffer_size(),_endindex));
    if(eof) _endindex=min(_endindex,index+data.size());
    //i is the start of _buffer,j is the start of data
    for(size_t i=st,j=st-index;i<ed;i++,j++){
        size_t id=i%_capacity;
        if(_buffer[id].second==true){
            //the _buffer[id].first is not written to the output stream
            //do not deal with it
        }else{
            //is written to the output stream
            //std::cout<<data[j]<<"  is written to the output stream"<<std::endl;
            _buffer[id]=make_pair(data[j],true);
            _unassembled_bytes++;
        }
    }
    std::string ss;
    while(_curindex<_endindex){
        if(_buffer[_curindex%_capacity].second==false) break;
        ss.push_back(_buffer[_curindex%_capacity].first);
        _buffer[_curindex%_capacity].second=false;
        _unassembled_bytes--;
        _curindex++;
    }
    //std::cout<<ss<<std::endl;
    _output.write(ss);
    if(_curindex==_endindex){
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const {
    return _unassembled_bytes;
}

bool StreamReassembler::empty() const {
    return unassembled_bytes() == 0;
}
