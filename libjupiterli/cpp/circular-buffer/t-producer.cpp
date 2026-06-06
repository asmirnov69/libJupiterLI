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

    size_t capacity = 1024;
    size_t bytes = Buffer::bytes_required(capacity);

    int fd = shm_open(SHM, O_CREAT | O_RDWR, 0666);
    ftruncate(fd, bytes);

    void* mem = mmap(nullptr, bytes, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    Buffer* buffer = reinterpret_cast<Buffer*>(mem);

    buffer->initialize(capacity);

    for(int i = 0; i < 10;i++) {
      Message m{i};
      while(!buffer->push_back(m)) {}
      std::cout << "Produced " << i << "\n";
    }

    munmap(mem, bytes);

    close(fd);
}
