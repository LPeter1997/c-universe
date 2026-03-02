static void* LIBRARY_FUNC(default_realloc)(void* ctx, void* ptr, size_t new_size) {
    (void)ctx;
    return realloc(ptr, new_size);
}

static void LIBRARY_FUNC(default_free)(void* ctx, void* ptr) {
    (void)ctx;
    free(ptr);
}

static void LIBRARY_FUNC(allocator_init)(LIBRARY_TYPE(Allocator)* allocator) {
    if (allocator->realloc != NULL || allocator->free != NULL) {
        LIBRARY_ASSERT(allocator->realloc != NULL && allocator->free != NULL, "both realloc and free function pointers must be set in allocator");
        return;
    }
    allocator->realloc = LIBRARY_FUNC(default_realloc);
    allocator->free = LIBRARY_FUNC(default_free);
}

static void* LIBRARY_FUNC(allocator_realloc)(LIBRARY_TYPE(Allocator)* allocator, void* ptr, size_t size) {
    LIBRARY_FUNC(allocator_init)(allocator);
    void* result = allocator->realloc(allocator->context, ptr, size);
    LIBRARY_ASSERT(result != NULL, "failed to allocate memory");
    return result;
}

static void LIBRARY_FUNC(allocator_free)(LIBRARY_TYPE(Allocator)* allocator, void* ptr) {
    LIBRARY_FUNC(allocator_init)(allocator);
    allocator->free(allocator->context, ptr);
}
