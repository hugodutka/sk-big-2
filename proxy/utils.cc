#include <cstdlib>
#include <memory>

using namespace std;

template <typename T>
struct MemChunk {
  size_t size;
  shared_ptr<T> chunk;

  T* get() { return chunk.get(); }

  MemChunk<T>(size_t size) : size(size) {
    T* chunk_ptr = static_cast<T*>(malloc(size));
    if (chunk_ptr == nullptr) {
      throw "memory allocation failed";
    }
    chunk = shared_ptr<T>(chunk_ptr, free);
  }
};
