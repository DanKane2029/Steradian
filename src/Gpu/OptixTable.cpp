// The one place OptiX's function table is defined.
//
// optix_function_table_definition.h declares storage, not just declarations, so including
// it in more than one translation unit is a duplicate symbol at link time. It lives here
// alone, and every other file includes optix_stubs.h instead, which only refers to it.

#include <optix_function_table_definition.h>
