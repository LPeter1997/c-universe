// Cleanup header to undefine anything that we don't want to leak into the public API

#undef LIBRARY_CONCAT_IMPL
#undef LIBRARY_CONCAT
#undef LIBRARY_FUNC
#undef LIBRARY_TYPE
#undef LIBRARY_MACRO
#undef LIBRARY_ASSERT

#undef LIBRARY_UNIQUE_NAME

#undef DYNARRAY_MEMBERS
#undef DYNARRAY
#undef DYNARRAY_LEN
#undef DYNARRAY_AT
#undef DYNARRAY_RESERVE
#undef DYNARRAY_PUSH
#undef DYNARRAY_POP
#undef DYNARRAY_FREE

#undef LIBRARY_NAME_LOWER
#undef LIBRARY_NAME_CAPITALIZED
#undef LIBRARY_NAME_UPPER
