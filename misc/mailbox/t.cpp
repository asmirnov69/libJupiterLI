/*
    Shared Memory Append-Only Dictionary

    Features:
      - multi-process shared memory dictionary
      - append-only allocation
      - dynamic growth using mremap()
      - automatic create/open
      - lookup by key
      - statistics snapshot
      - change detection
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

constexpr uint32_t INVALID_OFFSET=0;

constexpr size_t KEY_SIZE=64;

constexpr size_t INITIAL_SHM_SIZE=
    1024*1024;

constexpr const char* SHM_NAME=
    "/shared_dict_demo";

////////////////////////////////////////////////////////////

template<typename T>
T* ptr(
    void* base,
    uint32_t off)
{
    if(off==INVALID_OFFSET)
        return nullptr;

    return reinterpret_cast<T*>(
        reinterpret_cast<char*>(base)
        + off);
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
            expected=0;

            if(
                lock.compare_exchange_weak(
                    expected,
                    1,
                    std::memory_order_acquire))
            {
                return;
            }

            while(
                lock.load(
                    std::memory_order_relaxed))
            {
            }
        }
    }

    void release()
    {
        lock.store(
            0,
            std::memory_order_release);
    }
};

////////////////////////////////////////////////////////////

struct SharedHeader
{
    SharedMutex mutex;

    uint64_t segment_size;

    std::atomic<uint64_t> used_bytes;

    std::atomic<uint64_t> generation;

    std::atomic<uint64_t> change_counter;

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

    uint32_t value_count;
};

////////////////////////////////////////////////////////////

class SharedMemory
{
public:

    SharedMemory()
    {
        bool creator=false;

        fd_=
            shm_open(
                SHM_NAME,
                O_RDWR,
                0666);

        if(fd_<0)
        {
            if(errno!=ENOENT)
                throw std::runtime_error(
                    "shm_open");

            fd_=
                shm_open(
                    SHM_NAME,
                    O_CREAT|
                    O_EXCL|
                    O_RDWR,
                    0666);

            if(fd_<0)
            {
                fd_=
                    shm_open(
                        SHM_NAME,
                        O_RDWR,
                        0666);

                if(fd_<0)
                    throw std::runtime_error(
                        "shm_open");
            }
            else
            {
                creator=true;

                ftruncate(
                    fd_,
                    INITIAL_SHM_SIZE);
            }
        }

        map_size_=
            get_file_size();

        map();

        if(creator)
        {
            initialize();
        }
        else
        {
            while(
                header()
                ->generation
                .load()==0)
            {
            }
        }

        local_generation_=
            header()
            ->generation
            .load();
    }

    ~SharedMemory()
    {
        munmap(
            base_,
            map_size_);

        close(fd_);
    }

    SharedHeader*
    header()
    {
        return
            (SharedHeader*)
            base_;
    }

    void* base()
    {
        return base_;
    }

    void refresh_if_needed()
    {
        auto gen=
            header()
            ->generation
            .load();

        if(
            gen==
            local_generation_)
            return;

        remap();

        local_generation_=gen;
    }

    void ensure_capacity(
        size_t bytes)
    {
        auto h=
            header();

        uint64_t used=
            h->used_bytes.load();

        if(
            used+bytes<=map_size_)
        {
            return;
        }

        h->mutex.acquire();

        used=
            h->used_bytes.load();

        if(
            used+bytes>map_size_)
        {
            grow(
                used+bytes);

            h->segment_size=
                map_size_;

            h->generation
             .fetch_add(1);

            local_generation_=
                h->generation
                .load();
        }

        h->mutex.release();
    }

private:

    int fd_;

    void* base_;

    size_t map_size_;

    uint64_t local_generation_=0;

    void initialize()
    {
        auto h=
            header();

        memset(
            h,
            0,
            sizeof(*h));

        h->segment_size=
            map_size_;

        h->used_bytes=
            sizeof(
                SharedHeader);

        h->generation=1;

        h->change_counter=0;
    }

    size_t get_file_size()
    {
        struct stat st;

        fstat(
            fd_,
            &st);

        return st.st_size;
    }

    void map()
    {
        base_=
            mmap(
                nullptr,
                map_size_,
                PROT_READ|
                PROT_WRITE,
                MAP_SHARED,
                fd_,
                0);

        if(
            base_==
            MAP_FAILED)
        {
            throw std::runtime_error(
                "mmap");
        }
    }

    void remap()
    {
        size_t new_size=
            get_file_size();

        base_=
            mremap(
                base_,
                map_size_,
                new_size,
                MREMAP_MAYMOVE);

        map_size_=
            new_size;
    }

    void grow(
        size_t required)
    {
        size_t new_size=
            map_size_;

        while(
            new_size<
            required)
        {
            new_size*=2;
        }

        ftruncate(
            fd_,
            new_size);

        remap();
    }
};

////////////////////////////////////////////////////////////

class SharedAllocator
{
public:

    static uint32_t alloc(
        SharedMemory& shm,
        size_t bytes)
    {
        bytes=
            (bytes+7)&~7;

        shm.ensure_capacity(
            bytes);

        return shm.header()
            ->used_bytes
            .fetch_add(
                bytes);
    }
};

////////////////////////////////////////////////////////////

struct DictStats
{
    uint32_t key_count=0;

    std::vector<
        std::pair<
            std::string,
            uint32_t>>
    entries;
};

////////////////////////////////////////////////////////////

class SharedDict
{
public:

    class ValueIterator
    {
    public:

        ValueIterator(
            SharedMemory& shm,
            uint32_t node)
            :
            shm_(shm),
            node_(node)
        {
        }

        bool valid()
        {
            return
                node_
                != INVALID_OFFSET;
        }

        void next()
        {
            auto n=
                ptr<ListNode>(
                    shm_.base(),
                    node_);

            node_=
                n->next;
        }

        const char*
        value()
        {
            auto n=
                ptr<ListNode>(
                    shm_.base(),
                    node_);

            return ptr<char>(
                shm_.base(),
                n->string_offset);
        }

    private:

        SharedMemory& shm_;

        uint32_t node_;
    };

public:

    SharedDict()
    {
        last_seen_change_=
            shm_.header()
            ->change_counter
            .load();
    }

    ////////////////////////////////////////////////////

    bool changed()
    {
        shm_.refresh_if_needed();

        auto current=
            shm_.header()
            ->change_counter
            .load();

        if(
            current
            !=
            last_seen_change_)
        {
            last_seen_change_=
                current;

            return true;
        }

        return false;
    }

    ////////////////////////////////////////////////////

    void append(
        const std::string& key,
        const std::string& value)
    {
        auto h=
            shm_.header();

        h->mutex.acquire();

        auto e=
            find_or_create_locked(
                key);

        uint32_t node=
            create_node_locked(
                value);

        if(
            e->tail==
            INVALID_OFFSET)
        {
            e->head=node;
            e->tail=node;
        }
        else
        {
            auto tail=
                ptr<ListNode>(
                    shm_.base(),
                    e->tail);

            tail->next=node;

            e->tail=node;
        }

        e->value_count++;

        h->change_counter
         .fetch_add(1);

        h->mutex.release();
    }

    ////////////////////////////////////////////////////

    DictStats stats()
    {
        DictStats result;

        auto h=
            shm_.header();

        h->mutex.acquire();

        uint32_t cur=
            h->first_entry;

        while(cur)
        {
            auto e=
                ptr<MapEntry>(
                    shm_.base(),
                    cur);

            result.entries.emplace_back(
                e->key,
                e->value_count);

            result.key_count++;

            cur=
                e->next_entry;
        }

        h->mutex.release();

        return result;
    }

private:

    SharedMemory shm_;

    uint64_t last_seen_change_=0;

    ////////////////////////////////////////////////////

    MapEntry*
    find_or_create_locked(
        const std::string& key)
    {
        uint32_t cur=
            shm_.header()
            ->first_entry;

        while(cur)
        {
            auto e=
                ptr<MapEntry>(
                    shm_.base(),
                    cur);

            if(
                strncmp(
                    e->key,
                    key.c_str(),
                    KEY_SIZE)==0)
            {
                return e;
            }

            cur=
                e->next_entry;
        }

        uint32_t off=
            SharedAllocator
            ::alloc(
                shm_,
                sizeof(
                    MapEntry));

        auto e=
            ptr<MapEntry>(
                shm_.base(),
                off);

        memset(
            e,
            0,
            sizeof(*e));

        strncpy(
            e->key,
            key.c_str(),
            KEY_SIZE-1);

        e->next_entry=
            shm_.header()
            ->first_entry;

        shm_.header()
            ->first_entry=
            off;

        return e;
    }

    ////////////////////////////////////////////////////

    uint32_t create_node_locked(
        const std::string& value)
    {
        uint32_t str=
            SharedAllocator
            ::alloc(
                shm_,
                value.size()+1);

        memcpy(
            ptr<char>(
                shm_.base(),
                str),
            value.c_str(),
            value.size()+1);

        uint32_t node=
            SharedAllocator
            ::alloc(
                shm_,
                sizeof(
                    ListNode));

        auto n=
            ptr<ListNode>(
                shm_.base(),
                node);

        n->next=
            INVALID_OFFSET;

        n->string_offset=
            str;

        n->string_length=
            value.size();

        return node;
    }
};

////////////////////////////////////////////////////////////

int main(
    int argc,
    char** argv)
{
    SharedDict dict;

    if(argc>1 && std::string(argv[1])=="write") {
        int i=0;

        while(true) {
	  dict.append("animals", "value_"+std::to_string(i++));	  
	  sleep(2);
        }
    }

    if(argc>1 && std::string(argv[1])=="watch") {
      while(true) {
	if(dict.changed()) {
	  auto s= dict.stats();

	  std::cout << "changed: keys=" << s.key_count << "\n";

	  for(auto const& [k,c]: s.entries) {
	    std::cout << "  " << k << " -> " << c << "\n";
	  }

	  std::cout << "\n";
	}

	sleep(1);
      }
    }
}
