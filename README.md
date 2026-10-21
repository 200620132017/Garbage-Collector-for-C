# C Conservative Garbage Collector (Mark & Sweep)

[English](#english) | [Español](#español)

---

## English

### 1. Introduction & Overview
This project provides a lightweight, integrated **Conservative Garbage Collector** implemented from scratch in C. It relies on the **Mark-and-Sweep** algorithm combined with **transitive heap-to-heap tracing**. 

By including `"gc.h"`, standard `malloc` calls are transparently intercepted and managed. You no longer need to write manual `free()` calls for nodes, composite objects, or containers; instead, memory is reclaimed automatically based on reachability and explicit collection checkpoints.

---

### 2. Comprehensive Usage Guide & Scenarios

Understanding how the garbage collector determines whether an object is "alive" or "dead" (garbage) depends entirely on **references and pointers**. Below are all possible use cases and scenarios you will encounter.

#### Scenario A: Standard Allocation (Transparent `malloc`)
When you allocate memory—either directly in `main` or inside your data structures—you just use standard `malloc`. Because of the macro redefinition in `gc.h`, it calls `GC_malloc`.

```c
#include "cola.h"
#include "gc.h"

// Inside your initialization function:
struct Cola *ptrCola = malloc(sizeof(struct Cola)); 
// -> Handled transparently by the GC and tracked internally.
```
* **What happens:** The GC allocates the memory, wraps it in an internal `GC_Block`, and tracks it. At this point, `ptrCola` lives on the stack (or heap if nested), so the collector marks it as **live**.

---

#### Scenario B: Creating Nested Objects (Nodes & Payloads)
When a structure contains pointers to other heap blocks (e.g., a Queue Node pointing to a Patient struct, which in turn points to a Song string):

```c
struct Paciente *paciente = malloc(sizeof(struct Paciente)); // Heap block 1
struct Nodo *nodo = malloc(sizeof(struct Nodo));           // Heap block 2

nodo->persona = paciente; // Heap block 2 points to Heap block 1
```
* **What happens:** 
  1. The stack holds a reference to `nodo`. 
  2. During `GC_collect()`, the GC finds `nodo` via stack roots.
  3. During the **transitive heap-to-heap trace**, the GC inspects the internal bytes of `nodo`, finds the pointer to `paciente`, and automatically marks `paciente` as live as well.

---

#### Scenario C: Unlinking and Immediate Collection (The Queue Dequeue Pattern)
When you remove an item from a data structure (like `desencolar`), you break its link from the main structure. However, if a local variable in your function still points to it, the stack will keep it alive.

To clean it up instantly, follow the **Unlink $\rightarrow$ Null $\rightarrow$ Collect** pattern:

```c
bool desencolar(struct Cola* ptrCola) {
    if (ptrCola != NULL && ptrCola->tam > 0 && ptrCola->primero != NULL) {
        struct Nodo *ptr = ptrCola->primero; // ptr points to the node to remove
        ptrCola->primero = ptr->siguiente;    // 1. Unlink from the queue

        ptrCola->tam--;

        // 2. Clear the local pointer so it disappears from the stack frame
        ptr = NULL; 

        // 3. Trigger collection
        GC_collect(); 
        
        return true;
    }
    return false;
}
```
* **What happens step-by-step:**
  1. The node is detached from `ptrCola->primero`. The queue no longer points to it.
  2. Setting `ptr = NULL` ensures that when `GC_collect()` runs right after, **no lingering stack reference** is detected.
  3. The GC performs a trace. Because the queue doesn't point to it anymore, and the stack doesn't point to it either, the node becomes an **orphan**.
  4. The transitive tracer also sees that the `paciente` inside that node is no longer reachable from anywhere else, so **both the node and the patient are freed instantly in cascade**.

---

#### Scenario D: Extracting a Child Object (Returning a Payload)
What happens if you want to remove a node from a queue or list, **delete the node itself**, but **keep and return the child object** (e.g., returning a `Paciente` or `Cancion` while destroying the container node)?

```c
struct Paciente* extraerPrimerPaciente(struct Cola* ptrCola) {
    if (ptrCola == NULL || ptrCola->primero == NULL) return NULL;

    struct Nodo *nodoViejo = ptrCola->primero;
    ptrCola->primero = nodoViejo->siguiente; // Unlink node
    ptrCola->tam--;

    // Extract the payload pointer
    struct Paciente *pacienteRescatado = nodoViejo->persona;

    // Clear local pointers to the node, but KEEP the rescue pointer alive
    nodoViejo = NULL; 

    // Trigger collection
    GC_collect(); 

    // Because 'pacienteRescatado' is returned and will be stored in a variable 
    // in main(), the GC sees it via the stack and SAVES it from destruction, 
    // while the container 'nodoViejo' is completely wiped out!
    return pacienteRescatado;
}
```
* **What happens:** 
  * The container node has no references left $\rightarrow$ **Deleted by GC**.
  * The `pacienteRescatado` is assigned to a variable in `main()` $\rightarrow$ **Saved from deletion** because its pointer exists on the stack.

---

#### Scenario E: Reassigning or Dropping Pointers to NULL
If you have a pointer variable in your `main` function pointing to a heap structure, and you decide to drop it:

```c
struct Cola *ptrCola = inicializarCola();
encolar(ptrCola, "Alice", 25);

// Suddenly, you decide to discard the queue entirely without calling liberarCola()
ptrCola = NULL; // You break the stack reference

// If you run GC_collect() now:
GC_collect();
```
* **What happens:** Since `ptrCola` was the only root reference pointing to the queue, setting it to `NULL` leaves the queue and all its nodes completely unreachable. The garbage collector will sweep and destroy the entire queue and its contents automatically.

---

### 3. Compilation and Validation with Valgrind
Always compile with debug symbols (`-g`) and disable optimizations (`-O0`) to ensure accurate stack inspections:

```bash
gcc -g -O0 main.c cola.c gc.c -o test_cola
valgrind --leak-check=full ./test_cola
```

---
---

## Español

### 1. Introducción y Descripción General
Este proyecto ofrece un **Recolector de Basura Conservador** ligero e integrado, implementado desde cero en C. Utiliza el algoritmo **Mark-and-Sweep** combinado con **rastreo transitivo en el heap** (*heap-to-heap tracing*).

Al incluir `"gc.h"`, las llamadas estándar a `malloc` se interceptan y gestionan de forma transparente. Ya no necesitas escribir llamadas manuales a `free()` para nodos, objetos compuestos o contenedores; la memoria se recupera automáticamente según la accesibilidad (*reachability*) y los puntos de control de recolección.

---

### 2. Guía de Uso Completa y Casos Prácticos

Comprender cómo el recolector decide si un objeto está "vivo" o es "basura" depende enteramente de los **punteros y referencias**. A continuación se detallan todas las casuísticas posibles que encontrarás al programar.

#### Caso A: Asignación Estándar (`malloc` transparente)
Cuando reservas memoria —ya sea directamente en el `main` o dentro de tus estructuras de datos— utilizas el `malloc` de toda la vida. Gracias a la redefinición de macros en `gc.h`, el compilador redirige esa llamada a `GC_malloc`.

```c
#include "cola.h"
#include "gc.h"

// Dentro de tu función de inicialización:
struct Cola *ptrCola = malloc(sizeof(struct Cola)); 
// -> Gestionado de forma transparente por el GC y registrado internamente.
```
* **Qué ocurre:** El GC asigna la memoria, la encapsula en un bloque de control interno (`GC_Block`) y la rastrea. Como `ptrCola` vive en la pila (o en otro bloque del heap), el recolector lo marca como **vivo**.

---

#### Caso B: Creación de Objetos Anidados (Nodos y Contenido)
Cuando una estructura contiene punteros a otros bloques del heap (por ejemplo, un Nodo de una Cola que apunta a una estructura `Paciente`, la cual a su vez apunta a una cadena de texto o canción):

```c
struct Paciente *paciente = malloc(sizeof(struct Paciente)); // Bloque heap 1
struct Nodo *nodo = malloc(sizeof(struct Nodo));           // Bloque heap 2

nodo->persona = paciente; // El bloque 2 apunta al bloque 1
```
* **Qué ocurre:** 
  1. La pila local retiene una referencia a `nodo`. 
  2. Al ejecutar `GC_collect()`, el GC localiza `nodo` a través de las raíces de la pila.
  3. Durante el **rastreo transitivo en el heap**, el GC examina los bytes internos de `nodo`, descubre el puntero que apunta a `paciente` y marca automáticamente a `paciente` también como vivo.

---

#### Caso C: Desvinculación y Recolección Inmediata (El Patrón de Desencolar)
Cuando eliminas un elemento de una estructura de datos (como al `desencolar`), rompes su enlace con la estructura principal. Sin embargo, si una variable local dentro de tu función sigue apuntando a él, la pila (*stack*) lo mantendrá artificialmente vivo.

Para eliminarlo al instante de forma limpia, sigue el patrón **Desvincular $\rightarrow$ Poner a NULL $\rightarrow$ Recolectar**:

```c
bool desencolar(struct Cola* ptrCola) {
    if (ptrCola != NULL && ptrCola->tam > 0 && ptrCola->primero != NULL) {
        struct Nodo *ptr = ptrCola->primero; // ptr apunta al nodo a retirar
        ptrCola->primero = ptr->siguiente;    // 1. Desvincular de la cola

        ptrCola->tam--;

        // 2. Limpiar el puntero local para que desaparezca del marco de la pila
        ptr = NULL; 

        // 3. Disparar el recolector
        GC_collect(); 
        
        return true;
    }
    return false;
}
```
* **Paso a paso de lo que ocurre:**
  1. El nodo se desconecta de `ptrCola->primero`. La cola ya no apunta a él.
  2. Poner `ptr = NULL` garantiza que, cuando `GC_collect()` se ejecute inmediatamente después, **no quede ningún rastro en la pila**.
  3. El GC hace el rastreo y comprueba que ni la cola ni la pila apuntan al nodo. Se convierte en un **huérfano**.
  4. El rastreador transitivo detecta también que el `paciente` que vivía dentro de ese nodo ya no es alcanzable desde ningún otro sitio, por lo que **tanto el nodo como el paciente se liberan juntos al instante en cascada**.

---

#### Caso D: Extraer y Devolver un Objeto Hijo (Rescatar Datos)
¿Qué ocurre si deseas eliminar un nodo de una cola o lista, **destruir el nodo contenedor**, pero **conservar y devolver el objeto hijo** (por ejemplo, retornar un `Paciente` mientras destruyes el nodo de la cola)?

```c
struct Paciente* extraerPrimerPaciente(struct Cola* ptrCola) {
    if (ptrCola == NULL || ptrCola->primero == NULL) return NULL;

    struct Nodo *nodoViejo = ptrCola->primero;
    ptrCola->primero = nodoViejo->siguiente; // Desvincular nodo
    ptrCola->tam--;

    // Rescatar el puntero al contenido interno
    struct Paciente *pacienteRescatado = nodoViejo->persona;

    // Limpiar los punteros al nodo, pero MANTENER vivo el puntero de rescate
    nodoViejo = NULL; 

    // Disparar recolección
    GC_collect(); 

    // Como 'pacienteRescatado' se devuelve y se guardará en una variable del main(),
    // el GC lo detecta a través de la pila y lo SALVA de la destrucción, 
    // mientras que el contenedor 'nodoViejo' es eliminado por completo.
    return pacienteRescatado;
}
```
* **Qué ocurre:** 
  * El nodo contenedor ya no tiene referencias vivas $\rightarrow$ **Eliminado por el GC**.
  * El `pacienteRescatado` se asigna a una variable en el `main()` $\rightarrow$ **Se salva de la eliminación** porque su dirección existe en la pila de ejecución.

---

#### Caso E: Sobrescribir o Perder Referencias a Propósito (`NULL`)
Si tienes una variable puntero en tu función `main` apuntando a una estructura en el heap, y decides descartarla por completo:

```c
struct Cola *ptrCola = inicializarCola();
encolar(ptrCola, "Carlos", 30);

// De repente, decides ignorar la cola por completo sin llamar a liberarCola()
ptrCola = NULL; // Rompes la única referencia activa en la pila

// Si ejecutas GC_collect() en este punto:
GC_collect();
```
* **Qué ocurre:** Dado que `ptrCola` era la única raíz que mantenía viva la estructura, al ponerla a `NULL`, la cola y todos sus nodos encadenados pasan a ser inalcanzables. El recolector de basura los barrerá y destruirá todos automáticamente en la siguiente recolección.

---

### 3. Compilación y Validación con Valgrind
Compila siempre tus archivos activando los símbolos de depuración (`-g`) y desactivando las optimizaciones (`-O0`) para garantizar una inspección milimétrica de la pila:

```bash
gcc -g -O0 main.c cola.c gc.c -o test_cola
valgrind --leak-check=full ./test_cola