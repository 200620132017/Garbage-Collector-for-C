#ifndef GC_H
#define GC_H

#include <stddef.h>

// Funciones principales del Garbage Collector
void GC_init(void);
void* GC_malloc(size_t size);
void GC_collect(void);
void GC_destroy(void);

// Redefinición transparente de malloc para la gestión automática
#define malloc(size) GC_malloc(size)

#endif /* GC_H */