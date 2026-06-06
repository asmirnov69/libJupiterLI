#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include <iostream>
#include "t-message.h"
#include "circular-buffer.h"

int main()
{
  constexpr const char* SHM = "/example_queue";
  using Buffer = CircularBuffer<Message>;

  int fd = shm_open(SHM, O_RDWR, 0666);

  /*
    Consumer must know total size.
    
    Either:
    - hardcode capacity
    - store size externally
    - add shm header
  */
  
  size_t capacity = 1024;    
  size_t bytes = Buffer::bytes_required(capacity);
  
  void* mem = mmap(nullptr, bytes, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
  Buffer* buffer = reinterpret_cast<Buffer*>(mem);
  
  Message msg;
  
  while(true) {
    if(buffer->pop_front(msg)) {
      std::cout << "Consumed " << msg.value << "\n";
    }
  }
}
