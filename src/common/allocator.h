/**
 * An allocator struct that allows customizing memory allocation for the library.
 */
typedef struct LIBRARY_TYPE(Allocator) {
    // Context pointer that will be passed to the realloc and free functions
    void* context;
    // A function pointer for reallocating memory, with the same semantics as the standard realloc but with an additional context parameter
    void*(*realloc)(void* ctx, void* ptr, size_t new_size);
    // A function pointer for freeing memory, with the same semantics as the standard free but with an additional context parameter
    void(*free)(void* ctx, void* ptr);
} LIBRARY_TYPE(Allocator);
