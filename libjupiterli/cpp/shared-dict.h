#pragma once

/*
    Shared Memory Append-Only Dictionary (C++ / POSIX shm)

    Overview
    --------
    This file implements a simple shared-memory key-value structure designed
    for multi-process communication using POSIX shared memory.

    The dictionary maps:
        key (fixed-size string)  ->  list of values (variable-length strings)

    Data model
    ----------
    - Keys are stored in a linked list of MapEntry structures.
    - Each MapEntry contains a singly-linked list of ListNode values.
    - Values are stored inline as heap-allocated strings inside shared memory.
    - All allocations are append-only (no free operations).

    Memory model
    ------------
    - A single contiguous shared memory segment is used.
    - Offsets (not pointers) are used for all internal references,
      making the structure valid across process remapping.
    - The segment automatically grows using ftruncate() + mremap() when needed.

    Concurrency model
    ------------------
    - A single global spinlock (SharedMutex) protects structural mutations.
    - Readers also synchronize on this lock for consistency of metadata.
    - Shared memory mapping is versioned using a generation counter.
    - Processes automatically detect remapping events and re-map memory.

    Features
    --------
    - Automatic shared memory creation (first process wins)
    - Multi-process safe append operations
    - Lookup by key
    - Snapshot-based key listing (keys())
    - Iteration over value lists
    - Dynamic segment growth with mremap()

    Limitations / Tradeoffs
    -----------------------
    - Global lock limits write throughput under contention
    - Keys are stored in a linear linked list (O(n) lookup)
    - keys() returns a snapshot (not live view)
    - No deletion or compaction (append-only design)
    - Not crash-consistent across abrupt termination (no WAL)

    Design goal
    -----------
    This is intentionally a minimal, educational shared-memory KV store
    demonstrating:
        - offset-based memory design
        - cross-process pointer safety
        - dynamic shared memory growth
        - basic synchronization primitives in shared memory

    It is not a production database, but a foundation for one.
*/

#include <atomic>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

constexpr uint32_t INVALID_OFFSET = 0;

constexpr size_t KEY_SIZE = 64;
constexpr size_t INITIAL_SHM_SIZE = 1024 * 1024;
constexpr const char* SHM_NAME = "/shared_dict_demo";

////////////////////////////////////////////////////////////

template<typename T>
T* ptr(void* base, uint32_t off)
{
    if(off == INVALID_OFFSET)
        return nullptr;

    return reinterpret_cast<T*>(
        reinterpret_cast<char*>(base) + off);
}

////////////////////////////////////////////////////////////

struct SharedMutex
{
    std::atomic<uint32_t> lock{0};

    void acquire()
    {
        uint32_t expected;
        while(true)
        {
            expected = 0;

            if(lock.compare_exchange_weak(
                    expected, 1,
                    std::memory_order_acquire))
                return;

            while(lock.load(std::memory_order_relaxed));
        }
    }

    void release()
    {
        lock.store(0, std::memory_order_release);
    }
};

////////////////////////////////////////////////////////////

struct SharedHeader
{
    SharedMutex mutex;

    uint64_t segment_size;

    std::atomic<uint64_t> used_bytes;

    std::atomic<uint64_t> generation;

    uint32_t first_entry;
};

struct ListNode
{
    uint32_t next;
    uint32_t string_offset;
    uint32_t string_length;
};

struct MapEntry
{
    uint32_t next_entry;
    char key[KEY_SIZE];
    uint32_t head;
    uint32_t tail;
};

////////////////////////////////////////////////////////////

class SharedMemory
{
public:

    SharedMemory()
    {
        bool creator = false;

        fd_ = shm_open(SHM_NAME, O_RDWR, 0666);

        if(fd_ < 0)
        {
            if(errno != ENOENT)
                throw std::runtime_error("shm_open failed");

            fd_ = shm_open(
                SHM_NAME,
                O_CREAT | O_EXCL | O_RDWR,
                0666);

            if(fd_ < 0)
            {
                fd_ = shm_open(SHM_NAME, O_RDWR, 0666);
                if(fd_ < 0)
                    throw std::runtime_error("shm_open retry failed");
            }
            else
            {
                creator = true;
                ftruncate(fd_, INITIAL_SHM_SIZE);
            }
        }

        map_size_ = get_file_size();
        map();

        if(creator)
        {
            initialize();
        }
        else
        {
            while(header()->generation.load() == 0);
        }

        local_generation_ = header()->generation.load();
    }

    ~SharedMemory()
    {
        munmap(base_, map_size_);
        close(fd_);
    }

    void* base() { return base_; }

    SharedHeader* header()
    {
        return (SharedHeader*)base_;
    }

    void refresh_if_needed()
    {
        auto gen = header()->generation.load();
        if(gen == local_generation_)
            return;

        remap();
        local_generation_ = header()->generation.load();
    }

    void ensure_capacity(size_t bytes)
    {
        auto hdr = header();

        uint64_t used = hdr->used_bytes.load();

        if(used + bytes <= map_size_)
            return;

        hdr->mutex.acquire();

        used = hdr->used_bytes.load();

        if(used + bytes > map_size_)
        {
            grow(used + bytes);

            hdr->segment_size = map_size_;
            hdr->generation.fetch_add(1);
            local_generation_ = hdr->generation.load();
        }

        hdr->mutex.release();
    }

private:

    int fd_;
    void* base_;
    size_t map_size_;
    uint64_t local_generation_ = 0;

    void initialize()
    {
        auto h = header();
        memset(h, 0, sizeof(*h));

        h->segment_size = map_size_;
        h->used_bytes = sizeof(SharedHeader);
        h->generation = 1;
    }

    size_t get_file_size()
    {
        struct stat st;
        fstat(fd_, &st);
        return st.st_size;
    }

    void map()
    {
        base_ = mmap(nullptr, map_size_,
                     PROT_READ | PROT_WRITE,
                     MAP_SHARED, fd_, 0);

        if(base_ == MAP_FAILED)
            throw std::runtime_error("mmap failed");
    }

    void remap()
    {
        size_t new_size = get_file_size();

        base_ = mremap(base_, map_size_, new_size, MREMAP_MAYMOVE);

        if(base_ == MAP_FAILED)
            throw std::runtime_error("mremap failed");

        map_size_ = new_size;
    }

    void grow(size_t required)
    {
        size_t new_size = map_size_;

        while(new_size < required)
            new_size *= 2;

        ftruncate(fd_, new_size);
        remap();
    }
};

////////////////////////////////////////////////////////////

class SharedAllocator
{
public:
    static uint32_t alloc(SharedMemory& shm, size_t bytes)
    {
        bytes = (bytes + 7) & ~7;
        shm.ensure_capacity(bytes);

        return shm.header()->used_bytes.fetch_add(bytes);
    }
};

////////////////////////////////////////////////////////////

class SharedDict
{
public:

    class ValueIterator
    {
    public:
        ValueIterator(SharedMemory& shm, uint32_t n)
            : shm_(shm), node_(n) {}

        bool valid() { return node_ != INVALID_OFFSET; }

        void next()
        {
            shm_.refresh_if_needed();
            auto n = ptr<ListNode>(shm_.base(), node_);
            node_ = n->next;
        }

        const char* value()
        {
            shm_.refresh_if_needed();
            auto n = ptr<ListNode>(shm_.base(), node_);
            return ptr<char>(shm_.base(), n->string_offset);
        }

    private:
        SharedMemory& shm_;
        uint32_t node_;
    };

public:

    SharedDict() : shm_() {}

    void append(const std::string& key, const std::string& value)
    {
        auto h = shm_.header();
        h->mutex.acquire();

        auto e = find_or_create_locked(key);
        uint32_t n = create_node_locked(value);

        if(e->tail == INVALID_OFFSET)
        {
            e->head = e->tail = n;
        }
        else
        {
            auto tail = ptr<ListNode>(shm_.base(), e->tail);
            tail->next = n;
            e->tail = n;
        }

        h->mutex.release();
    }

    bool exists(const std::string& key)
    {
        return find(key) != INVALID_OFFSET;
    }

    uint32_t find(const std::string& key)
    {
        shm_.refresh_if_needed();
        auto h = shm_.header();

        h->mutex.acquire();

        uint32_t cur = h->first_entry;

        while(cur)
        {
            auto e = ptr<MapEntry>(shm_.base(), cur);

            if(strncmp(e->key, key.c_str(), KEY_SIZE) == 0)
            {
                uint32_t r = e->head;
                h->mutex.release();
                return r;
            }

            cur = e->next_entry;
        }

        h->mutex.release();
        return INVALID_OFFSET;
    }

    ValueIterator values(uint32_t h)
    {
        return ValueIterator(shm_, h);
    }

    // ============================
    // NEW: snapshot key list
    // ============================
    std::vector<std::string> keys()
    {
        std::vector<std::string> result;

        shm_.refresh_if_needed();

        auto h = shm_.header();
        h->mutex.acquire();

        uint32_t cur = h->first_entry;

        while(cur)
        {
            auto e = ptr<MapEntry>(shm_.base(), cur);
            result.emplace_back(e->key);
            cur = e->next_entry;
        }

        h->mutex.release();
        return result;
    }

private:

    SharedMemory shm_;

    MapEntry* find_or_create_locked(const std::string& key)
    {
        uint32_t cur = shm_.header()->first_entry;

        while(cur)
        {
            auto e = ptr<MapEntry>(shm_.base(), cur);

            if(strncmp(e->key, key.c_str(), KEY_SIZE) == 0)
                return e;

            cur = e->next_entry;
        }

        uint32_t off = SharedAllocator::alloc(shm_, sizeof(MapEntry));
        auto e = ptr<MapEntry>(shm_.base(), off);

        memset(e, 0, sizeof(*e));
        strncpy(e->key, key.c_str(), KEY_SIZE - 1);

        e->next_entry = shm_.header()->first_entry;
        shm_.header()->first_entry = off;

        return e;
    }

    uint32_t create_node_locked(const std::string& v)
    {
        uint32_t str = SharedAllocator::alloc(shm_, v.size() + 1);

        memcpy(ptr<char>(shm_.base(), str),
               v.c_str(),
               v.size() + 1);

        uint32_t n = SharedAllocator::alloc(shm_, sizeof(ListNode));

        auto node = ptr<ListNode>(shm_.base(), n);
        node->next = INVALID_OFFSET;
        node->string_offset = str;
        node->string_length = v.size();

        return n;
    }
};

/*
////////////////////////////////////////////////////////////

int main(int argc, char** argv)
{
    SharedDict dict;

    if(argc > 1 && std::string(argv[1]) == "write")
    {
        for(int i = 0; i < 10; i++)
            dict.append("ANIMALS", "val_" + std::to_string(i));

        dict.append("cars", "tesla");
        dict.append("cars", "bmw");

        std::cout << "written\n";
        pause();
    }
    else
    {
        auto ks = dict.keys();

        for(const auto& k : ks)
        {
            std::cout << k << "\n";

            auto h = dict.find(k);
            auto it = dict.values(h);

            while(it.valid())
            {
                std::cout << "  " << it.value() << "\n";
                it.next();
            }
        }
    }
}
*/
