#pragma once

#include <new>
#include <memory>
#include <atomic>
#include <vector>

namespace shared
{

    // http://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue
    template<typename T>
    class bounded_queue
    {
    public:

        using item_type = T;

        bounded_queue()
            : bounded_queue(16384)
        {
        }

        bounded_queue(size_t buffer_size)
            :max_size_(buffer_size),
            buffer_(new cell_t[buffer_size]),
            buffer_mask_(buffer_size - 1)
        {
            //queue size must be power of two
            if (!((buffer_size >= 2) && ((buffer_size & (buffer_size - 1)) == 0)))
                throw std::bad_alloc();

            for (size_t i = 0; i != buffer_size; i += 1)
                buffer_[i].sequence_.store(i, std::memory_order_relaxed);
            enqueue_pos_.store(0, std::memory_order_relaxed);
            dequeue_pos_.store(0, std::memory_order_relaxed);
        }

        ~bounded_queue()
        {
            delete[] buffer_;
        }


        bool enqueue(T&& data)
        {
            cell_t* cell;
            size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
            for (;;)
            {
                cell = &buffer_[pos & buffer_mask_];
                size_t seq = cell->sequence_.load(std::memory_order_acquire);
                intptr_t dif = (intptr_t)seq - (intptr_t)pos;
                if (dif == 0)
                {
                    if (enqueue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                        break;
                }
                else if (dif < 0)
                {
                    return false;
                }
                else
                {
                    pos = enqueue_pos_.load(std::memory_order_relaxed);
                }
            }
            cell->data_ = std::move(data);
            cell->sequence_.store(pos + 1, std::memory_order_release);
            return true;
        }

        bool try_dequeue(T& data)
        {
            cell_t* cell;
            size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
            for (;;)
            {
                cell = &buffer_[pos & buffer_mask_];
                size_t seq =
                    cell->sequence_.load(std::memory_order_acquire);
                intptr_t dif = (intptr_t)seq - (intptr_t)(pos + 1);
                if (dif == 0)
                {
                    if (dequeue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                        break;
                }
                else if (dif < 0)
                    return false;
                else
                    pos = dequeue_pos_.load(std::memory_order_relaxed);
            }
            data = std::move(cell->data_);
            cell->sequence_.store(pos + buffer_mask_ + 1, std::memory_order_release);
            return true;
        }

        size_t approx_size()
        {
            size_t first_pos = dequeue_pos_.load(std::memory_order_relaxed);
            size_t last_pos = enqueue_pos_.load(std::memory_order_relaxed);
            if (last_pos <= first_pos)
                return 0;
            auto size = last_pos - first_pos;
            return size < max_size_ ? size : max_size_;
        }

    private:
        struct cell_t
        {
            std::atomic<size_t>   sequence_;
            T                     data_;
        };

        size_t const max_size_;

        static size_t const     cacheline_size = 64;
        typedef char            cacheline_pad_t[cacheline_size];

        cacheline_pad_t         pad0_;
        cell_t* const           buffer_;
        size_t const            buffer_mask_;
        cacheline_pad_t         pad1_;
        std::atomic<size_t>     enqueue_pos_;
        cacheline_pad_t         pad2_;
        std::atomic<size_t>     dequeue_pos_;
        cacheline_pad_t         pad3_;

        bounded_queue(bounded_queue const&) = delete;
        void operator= (bounded_queue const&) = delete;
    };

    // very basic mem pool. 
    template<typename T, int N>
    class MemPool
    {
    public:

        MemPool()
        {
            mem = malloc(sizeof(T) * N);
            T* typed_layout = static_cast<T*>(mem);
            for (int ctr = 0; ctr < N; ctr++)
            {
                poolList.enqueue((void*)&typed_layout[ctr]);
            }
        }
        ~MemPool()
        {
            free(mem);
        }

        T* Get()
        {
            void* object = nullptr;
            if (poolList.try_dequeue(object))
            {
                return static_cast<T*>(object);
            }
            else
            {
                // queue is full - create a new object on the fly. 
                // it will get added on the queue when its deleted. However, these objects will be leaked when MemPool goes away. @todo proper cleanup. 
                return static_cast<T*>(malloc(sizeof(T)));
            }
        }

        void Put(T* DeletePtr)
        {
            poolList.enqueue(((void*)DeletePtr));
        }
    private:

        bounded_queue<void*> poolList; // pointers to objects. 
        void* mem;
    };

    // Simple Byte Array. 
    typedef std::vector<uint8_t> PayLoadType;
    typedef std::unique_ptr<PayLoadType> PayLoadPtr; 
    typedef std::shared_ptr<PayLoadType> PayLoadSharedPtr;
}

// make a simple macro to embed MemPool in a class using overloaded new/delete.

#define MemoryPoolTrait(ClassType,PoolSize) static shared::MemPool<ClassType, PoolSize>& GetDataPool()                  \
{                                                                                                                       \
    static shared::MemPool<ClassType, PoolSize>  data_pool;                                                             \
    return data_pool;                                                                                                   \
}                                                                                                                       \
                                                                                                                        \
void* operator new(size_t sz)                                                                                           \
{                                                                                                                       \
    return GetDataPool().Get();                                                                                         \
}                                                                                                                       \
void operator delete(void* m)                                                                                           \
{                                                                                                                       \
   if( m != nullptr )                                                                                                   \
        GetDataPool().Put((ClassType*)m);                                                                               \
}                                                                                                                       \
\

