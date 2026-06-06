#pragma once
#include <atomic>
#include <cstddef>
#include <cstring>

template<typename T> class CircularBuffer
{
public:
  static size_t bytes_required(size_t capacity) { return sizeof(CircularBuffer) + capacity * sizeof(T); }

  void initialize(size_t capacity) {
    capacity_ = capacity;
    head_.store(0);
    tail_.store(0);
  }
  
  bool push_back(const T& value) {
    size_t tail = tail_.load(std::memory_order_relaxed);
    size_t next = increment(tail);
    if (next == head_.load(std::memory_order_acquire)) {
      return false;
    }
    
    storage()[tail] = value;
    tail_.store(next, std::memory_order_release);
    
    return true;
  }
  
  bool pop_front(T& out) {
    size_t head = head_.load(std::memory_order_relaxed);
    
    if (head ==tail_.load(std::memory_order_acquire)) {
      return false;
    }
    
    out = std::move(storage()[head]);
    head_.store(increment(head), std::memory_order_release);
    
    return true;
  }
  
private:
  size_t increment(size_t x) const { return (x + 1) % capacity_; }  
  T* storage() { return reinterpret_cast<T*>(reinterpret_cast<char*>(this) + sizeof(CircularBuffer)); }
  const T* storage() const { return reinterpret_cast<const T*>(reinterpret_cast<const char*>(this) + sizeof(CircularBuffer)); }
  
  size_t capacity_;
  // Avoid false sharing
  // This keeps producer and consumer from constantly invalidating each other's cache lines.
  alignas(64) std::atomic<size_t> head_;
  alignas(64) std::atomic<size_t> tail_;
};
