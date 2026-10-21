#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <setjmp.h>

#if defined(__has_include)
#  if __has_include(<valgrind/valgrind.h>) && __has_include(<valgrind/memcheck.h>)
#    include <valgrind/valgrind.h>
#    include <valgrind/memcheck.h>
#    define HAS_VALGRIND 1
#  endif
#endif

#ifndef VALGRIND_MAKE_MEM_DEFINED
#  define VALGRIND_MAKE_MEM_DEFINED(addr, len) ((void)0)
#endif

#include "gc.h"
#undef malloc  

typedef struct GC_Block {
    void *ptr;              
    size_t size;            
    bool marked;            
    struct GC_Block *next;  
} GC_Block;

static GC_Block *head = NULL;
static void *stack_base = NULL;

void GC_init(void) {
    stack_base = __builtin_frame_address(0);
}

// Algoritmo de recolección Mark and Sweep con rastreo transitivo en Heap y Stack
void GC_collect(void) {
    jmp_buf env;
    memset(&env, 0, sizeof(jmp_buf));
    setjmp(env); 

    void *stack_top = __builtin_frame_address(0);
    uintptr_t *low = (uintptr_t*)(stack_top < stack_base ? stack_top : stack_base);
    uintptr_t *high = (uintptr_t*)(stack_top > stack_base ? stack_top : stack_base);

    // FASE 1: Marcar todo como no vivo
    GC_Block *curr = head;
    while (curr) {
        curr->marked = false;
        curr = curr->next;
    }

    // Macro auxiliar para marcar un bloque si una dirección cae dentro de su rango
    #define CHECK_AND_MARK(addr) \
        do { \
            uintptr_t a = (uintptr_t)(addr); \
            if (a == 0) break; \
            GC_Block *b = head; \
            while (b) { \
                uintptr_t s = (uintptr_t)b->ptr; \
                uintptr_t e = s + b->size; \
                if (a >= s && a < e) { \
                    b->marked = true; \
                    break; \
                } \
                b = b->next; \
            } \
        } while(0)

    // 1. Escanear raíces en la pila (Stack)
#ifdef HAS_VALGRIND
    VALGRIND_MAKE_MEM_DEFINED(low, (size_t)((char*)high - (char*)low));
#endif

    for (uintptr_t *p = low; p < high; p++) {
        uintptr_t val = 0;
        memcpy(&val, p, sizeof(uintptr_t));
        CHECK_AND_MARK(val);
    }

    // 2. Escanear registros en setjmp
    uintptr_t *env_ptr = (uintptr_t*)&env;
    for (size_t i = 0; i < sizeof(jmp_buf) / sizeof(uintptr_t); i++) {
        CHECK_AND_MARK(env_ptr[i]);
    }

    // 3. Propagación transitiva en el Heap (Heap-to-Heap scanning):
    // El GC escanea el contenido de los bloques ya marcados en busca de punteros internos a otros bloques.
    bool new_marks;
    do {
        new_marks = false;
        GC_Block *b = head;
        while (b) {
            if (b->marked) {
                uintptr_t *payload = (uintptr_t*)b->ptr;
                size_t num_words = b->size / sizeof(uintptr_t);
                for (size_t i = 0; i < num_words; i++) {
                    uintptr_t val = payload[i];
                    if (val != 0) {
                        GC_Block *target = head;
                        while (target) {
                            uintptr_t s = (uintptr_t)target->ptr;
                            uintptr_t e = s + target->size;
                            if (val >= s && val < e) {
                                if (!target->marked) {
                                    target->marked = true;
                                    new_marks = true; // Encontramos una nueva referencia, repetimos el ciclo
                                }
                                break;
                            }
                            target = target->next;
                        }
                    }
                }
            }
            b = b->next;
        }
    } while (new_marks);

    // FASE 2: Barrido (Sweep) - liberar lo que no esté marcado
    GC_Block **p_curr = &head;
    while (*p_curr) {
        GC_Block *target = *p_curr;
        if (!target->marked) {
            *p_curr = target->next;
            free(target->ptr);
            free(target);
        } else {
            p_curr = &target->next;
        }
    }
}

void* GC_malloc(size_t size) {
    if (size == 0) return NULL;

    void *raw_ptr = malloc(size); 
    if (!raw_ptr) return NULL;
    memset(raw_ptr, 0, size);

    GC_Block *node = malloc(sizeof(GC_Block));
    if (!node) {
        free(raw_ptr);
        return NULL;
    }

    node->ptr = raw_ptr;
    node->size = size;
    node->marked = false;
    node->next = head;
    head = node;

    return raw_ptr;
}

void GC_destroy(void) {
    GC_Block *curr = head;
    while (curr) {
        GC_Block *next = curr->next;
        free(curr->ptr);
        free(curr);
        curr = next;
    }
    head = NULL;
}