# Referencia de Inline Assembly - GLOBEX_OS

**Versión:** 1.0  
**Fecha:** 9 de marzo de 2026  
**Arquitectura:** x86_64 (Intel/AMD 64-bit)

---

## Resumen Ejecutivo

Este documento proporciona una referencia completa de todo el inline assembly utilizado en el kernel GLOBEX_OS. Cada instrucción está documentada con:

- Sintaxis y operandos
- Efectos y propósito
- Ciclos de CPU aproximados
- Barreras de memoria
- Referencias a documentación oficial

---

## Tabla de Instrucciones

| Instrucción | Archivo | Línea | Propósito |
|-------------|---------|-------|-----------|
| `outb` | serial.cpp | 107 | Escritura de puertos E/S |
| `inb` | serial.cpp | 133 | Lectura de puertos E/S |
| `nop` | serial.cpp | 297, 348 | Delay de CPU |
| `""` (memory barrier) | serial.cpp, vga.cpp | Múltiple | Barrera de compilador |
| `pushfq; pop` | spinlock.cpp | 57 | Leer RFLAGS |
| `cli` | spinlock.cpp, panic.cpp | 63, 40 | Deshabilitar interrupciones |
| `sti` | spinlock.cpp | 79 | Habilitar interrupciones |
| `lock cmpxchg` | spinlock.cpp | 169, 237 | Compare-and-swap atómico |
| `pause` | spinlock.cpp | 203 | Optimizar spin loop |
| `hlt` | panic.cpp | 119 | Detener CPU |

---

## Instrucciones Detalladas

### `outb` - Output Byte

**Ubicación:** `src/impl/x86_64/serial.cpp:107`

```cpp
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}
```

#### Descripción

Escribe un byte en un puerto de E/S especificado.

#### Operandos

| Operando | Constraint | Registro | Propósito |
|----------|------------|----------|-----------|
| `%0` | `"a"` | AL | Valor a escribir |
| `%1` | `"Nd"` | DX | Puerto de E/S (0-65535) |

#### Constraints Explicados

- **`"a"`**: Usa el registro AL/AX/EAX/RAX. El compilador coloca `value` en AL.
- **`"Nd"`**: 
  - `N`: Valor inmediato 0-255
  - `d`: Registro DX
  - El compilador elige según el valor del puerto

#### Características

| Propiedad | Valor |
|-----------|-------|
| **Ciclos** | ~100-1000 (depende del dispositivo) |
| **Barreras** | Implícita (volatile) |
| **Privilegio** | Ring 0 (kernel mode) |
| **Arquitectura** | x86/x86_64 específica |

#### Notas Importantes

1. **I/O Mapped Memory**: Los puertos de E/S están en un espacio de direcciones separado de la memoria principal.
2. **Volatile**: La instrucción no puede ser reordenada por el compilador.
3. **Timing**: El tiempo real depende del dispositivo hardware. Puertos seriales típicos: ~1000 ciclos.

#### Ejemplo de Uso

```cpp
// Enviar byte 0x5A al puerto serial COM1 (0x3F8)
outb(0x3F8 + SERIAL_THR, 0x5A);
```

#### Referencias

- [Intel SDM Vol 2B - OUT](https://www.felixcloutier.com/x86/out)
- [PC16550D UART Datasheet](https://www.ti.com/lit/pdf/snla024)

---

### `inb` - Input Byte

**Ubicación:** `src/impl/x86_64/serial.cpp:133`

```cpp
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
```

#### Descripción

Lee un byte desde un puerto de E/S especificado.

#### Operandos

| Operando | Constraint | Registro | Propósito |
|----------|------------|----------|-----------|
| `%0` | `"=a"` | AL | Valor leído (output) |
| `%1` | `"Nd"` | DX | Puerto de E/S (input) |

#### Constraints Explicados

- **`"=a"`**: Escribe en AL/AX/EAX/RAX. El `=` indica output-only.
- **`"Nd"`**: Igual que `outb`.

#### Características

| Propiedad | Valor |
|-----------|-------|
| **Ciclos** | ~100-1000 (depende del dispositivo) |
| **Barreras** | Implícita (volatile) |
| **Privilegio** | Ring 0 (kernel mode) |
| **Arquitectura** | x86/x86_64 específica |

#### Notas Importantes

1. **Valor de Retorno**: Después de la instrucción, el valor está en AL.
2. **Serialización**: En algunos CPUs, `inb` serializa todas las instrucciones anteriores.
3. **Stale Data**: Leer un puerto no presente puede retornar 0xFF (bus floating).

#### Ejemplo de Uso

```cpp
// Leer estado del puerto serial
uint8_t lsr = inb(0x3F8 + SERIAL_LSR);
if (lsr & SERIAL_LSR_THRE) {
    // Transmisor vacío, podemos escribir
}
```

#### Referencias

- [Intel SDM Vol 2A - IN](https://www.felixcloutier.com/x86/in)

---

### `nop` - No Operation

**Ubicación:** `src/impl/x86_64/serial.cpp:297, 348`

```cpp
for (volatile int i = 0; i < 1000; i++) {
    __asm__ volatile ("nop");
}
```

#### Descripción

No realiza ninguna operación. Consume 1 ciclo de CPU.

#### Características

| Propiedad | Valor |
|-----------|-------|
| **Ciclos** | 1 |
| **Barreras** | Ninguna |
| **Privilegio** | Todos los rings |
| **Arquitectura** | x86/x86_64 específica |

#### Usos en GLOBEX_OS

1. **Delay Corto**: 1000 nops ≈ 0.5ms en 2GHz
2. **Prevenir Saturación de Bus**: En bucles de polling

#### Notas Importantes

1. **Volatile en Contador**: Necesario para prevenir que el compilador optimice el bucle.
2. **Alternativa Moderna**: Usar `rdtsc` para delays precisos.

#### Referencias

- [Intel SDM Vol 2B - NOP](https://www.felixcloutier.com/x86/nop)

---

### `""` (Empty Inline Assembly) - Memory Barrier

**Ubicación:** `src/impl/x86_64/serial.cpp:41`, `vga.cpp:49`, `spinlock.cpp:85`

```cpp
#define mb()  __asm__ volatile ("" ::: "memory")
#define rmb() __asm__ volatile ("" ::: "memory")
#define wmb() __asm__ volatile ("" ::: "memory")
```

#### Descripción

Barrera de memoria a nivel de compilador. Previene reordenamiento de accesos a memoria.

#### Operandos

| Sección | Contenido | Propósito |
|---------|-----------|-----------|
| **Output** | (vacío) | Ninguno |
| **Input** | (vacío) | Ninguno |
| **Clobber** | `"memory"` | Memoria puede ser modificada |

#### Características

| Propiedad | Valor |
|-----------|-------|
| **Ciclos** | 0 (directiva al compilador) |
| **Barreras** | Compilador (no hardware) |
| **Privilegio** | Todos los rings |
| **Arquitectura** | Portable (GCC/Clang) |

#### Tipos de Barreras

| Macro | Efecto |
|-------|--------|
| `mb()` | Barrera completa (load + store) |
| `rmb()` | Barrera de lectura (load ordering) |
| `wmb()` | Barrera de escritura (store ordering) |

#### Por Qué es Necesario

En x86_64:
- **Hardware**: Tiene ordenamiento fuerte de memoria
- **Compilador**: Puede reordenar instrucciones libremente

El clobber `"memory"` le dice al compilador:
> "Cualquier acceso a memoria debe completarse antes de continuar"

#### Ejemplo de Uso

```cpp
// Escribir dato
data = 42;
wmb();  // Asegura que 'data' se escribe antes
// Escribir flag
flag = 1;
```

#### Referencias

- [GCC Inline Assembly - Memory Clobber](https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html)
- [Linux Kernel Memory Barriers](https://www.kernel.org/doc/Documentation/memory-barriers.txt)

---

### `pushfq; pop` - Leer RFLAGS

**Ubicación:** `src/impl/x86_64/spinlock.cpp:57`

```cpp
uint64_t rflags;
__asm__ volatile ("pushfq; pop %0" : "=r"(rflags));
```

#### Descripción

Lee el registro RFLAGS para verificar el estado de interrupciones.

#### Operandos

| Operando | Constraint | Registro | Propósito |
|----------|------------|----------|-----------|
| `%0` | `"=r"` | Cualquiera | Valor de RFLAGS (output) |

#### Características

| Propiedad | Valor |
|-----------|-------|
| **Ciclos** | ~3-5 |
| **Barreras** | Ninguna |
| **Privilegio** | Ring 0 (parcialmente) |
| **Arquitectura** | x86_64 específica |

#### RFLAGS Layout (Bits Relevantes)

| Bit | Nombre | Propósito |
|-----|--------|-----------|
| 0 | CF | Carry Flag |
| 2 | PF | Parity Flag |
| 6 | ZF | Zero Flag |
| 9 | **IF** | **Interrupt Flag** |
| 10 | DF | Direction Flag |

#### Código de Verificación

```cpp
state->interrupts_enabled = (rflags & 0x200) != 0;  // Bit 9 = IF
```

#### Notas Importantes

1. **pushfq en Long Mode**: Push 64-bit RFLAGS
2. **Alternativa**: `pushfq; popf` no funciona en todos los contextos
3. **Seguridad**: Esta secuencia es safe en cualquier contexto

#### Referencias

- [Intel SDM Vol 1 - RFLAGS](https://www.felixcloutier.com/x86/flags)

---

### `cli` - Clear Interrupt Flag

**Ubicación:** `src/impl/x86_64/spinlock.cpp:63`, `panic.cpp:40`

```cpp
__asm__ volatile ("cli" ::: "memory");
```

#### Descripción

Deshabilita interrupciones maskables estableciendo IF=0 en RFLAGS.

#### Características

| Propiedad | Valor |
|-----------|-------|
| **Ciclos** | ~3 |
| **Barreras** | Implícita (instrucción privilegiada) |
| **Privilegio** | Ring 0 |
| **Arquitectura** | x86/x86_64 específica |

#### Efectos

- **IF = 0**: Interrupciones maskables deshabilitadas
- **NMI**: Non-Maskable Interrupts aún pueden ocurrir
- **Excepciones**: Faults, traps, aborts aún pueden ocurrir

#### Usos en GLOBEX_OS

1. **spinlock_acquire()**: Prevenir deadlock si ISR intenta adquirir mismo lock
2. **panic()**: Prevenir reentrancia durante output de error

#### Notas Importantes

1. **STI Required**: Las interrupciones deben ser re-habilitadas explícitamente.
2. **No Reordenar**: El clobber `"memory"` previene reordenamiento.
3. **NMI Still Works**: Solo interrupciones maskables son bloqueadas.

#### Ejemplo de Uso

```cpp
// Critical section con interrupciones deshabilitadas
cli();
// ... código crítico ...
sti();
```

#### Referencias

- [Intel SDM Vol 2A - CLI](https://www.felixcloutier.com/x86/cli)

---

### `sti` - Set Interrupt Flag

**Ubicación:** `src/impl/x86_64/spinlock.cpp:79`

```cpp
if (state->interrupts_enabled) {
    __asm__ volatile ("sti" ::: "memory");
}
```

#### Descripción

Habilita interrupciones maskables estableciendo IF=1 en RFLAGS.

#### Características

| Propiedad | Valor |
|-----------|-------|
| **Ciclos** | ~3 |
| **Barreras** | Implícita |
| **Privilegio** | Ring 0 |
| **Arquitectura** | x86/x86_64 específica |

#### Notas Importantes

1. **Delay de 1 Instrucción**: En algunos CPUs, `sti` no habilita interrupciones hasta después de la siguiente instrucción.
2. **Solo si Era Necesario**: El código verifica `interrupts_enabled` antes de llamar.

#### Referencias

- [Intel SDM Vol 2B - STI](https://www.felixcloutier.com/x86/sti)

---

### `lock cmpxchg` - Compare-and-Swap Atómico

**Ubicación:** `src/impl/x86_64/spinlock.cpp:169, 237`

```cpp
uint64_t result;
__asm__ volatile (
    "lock cmpxchg %2, %1"
    : "=a"(result), "+m"(lock->locked)
    : "r"(desired), "a"(expected)
    : "memory"
);
```

#### Descripción

Instrucción atómica de compare-and-swap. Fundamental para sincronización SMP.

#### Operandos

| Operando | Constraint | Registro | Propósito |
|----------|------------|----------|-----------|
| `%0` | `"=a"` | RAX | Resultado (output) |
| `%1` | `"+m"` | Memoria | `lock->locked` (input/output) |
| `%2` | `"r"` | Cualquiera | `desired` (valor a escribir) |
| `%3` | `"a"` | RAX | `expected` (valor esperado) |

#### Algoritmo

```
IF [lock->locked] == expected (0):
    [lock->locked] ← desired (1)
    ZF ← 1 (zero flag set)
    result ← expected (0)
ELSE:
    result ← [lock->locked] (valor actual)
    ZF ← 0 (zero flag clear)
```

#### Características

| Propiedad | Valor |
|-----------|-------|
| **Ciclos (sin contención)** | ~20-50 |
| **Ciclos (con contención)** | Variable |
| **Barreras** | LOCK actúa como barrera completa |
| **Privilegio** | Todos los rings |
| **Arquitectura** | x86/x86_64 específica |

#### Prefijo LOCK

El prefijo `lock` garantiza:
1. **Atomicidad**: La operación es indivisible en todo el sistema
2. **Bloqueo de Bus**: Otros CPUs no pueden acceder a la misma dirección
3. **Barrera de Memoria**: Todas las operaciones anteriores completan, las posteriores esperan

#### Zero Flag (ZF)

- **ZF = 1**: Éxito (lock adquirido, `result == 0`)
- **ZF = 0**: Fallo (otro CPU tiene el lock, `result != 0`)

#### Usos en GLOBEX_OS

1. **spinlock_acquire()**: 
   - **Fast path**: Intenta adquirir sin deshabilitar IRQ (sin contención)
   - **Slow path**: Deshabilita IRQ y hace spin (hay contención)
2. **spinlock_try_acquire()**: Intentar adquirir lock sin spin

#### Notas Importantes

1. **RAX Requerido**: `expected` DEBE estar en RAX antes de la instrucción.
2. **Memory Clobber**: `"memory"` previene reordenamiento por compilador.
3. **Performance**: Con alta contención, considerar backoff exponencial.
4. **Fast Path Design**: El nuevo diseño intenta primero sin deshabilitar IRQ, 
   previniendo deadlock en sistemas de un solo CPU.

#### Ejemplo de Uso

```cpp
// Intentar adquirir lock
uint64_t expected = 0;  // Esperamos unlocked
uint64_t desired = 1;   // Queremos locked
uint64_t result;

__asm__ volatile (
    "lock cmpxchg %2, %1"
    : "=a"(result), "+m"(lock->locked)
    : "r"(desired), "a"(expected)
    : "memory"
);

if (result == 0) {
    // Lock adquirido (ZF=1)
} else {
    // Lock ocupado (ZF=0)
}
```

#### Referencias

- [Intel SDM Vol 2A - CMPXCHG](https://www.felixcloutier.com/x86/cmpxchg)
- [Intel SDM Vol 2A - LOCK](https://www.felixcloutier.com/x86/lock)

---

### `pause` - Optimizar Spin Loop

**Ubicación:** `src/impl/x86_64/spinlock.cpp:203`

```cpp
__asm__ volatile ("pause" ::: "memory");
```

#### Descripción

Instrucción SSE2 que optimiza bucles de espera (spin loops).

#### Características

| Propiedad | Valor |
|-----------|-------|
| **Ciclos** | ~10-20 (varía por CPU) |
| **Barreras** | Suave (no reordena, pero da oportunidad) |
| **Privilegio** | Todos los rings |
| **Arquitectura** | x86/x86_64 (SSE2+) |

#### Efectos

1. **Previene Pipeline Stalls**: Evita detección falsa de dependencias de memoria
2. **Reduce Consumo**: Menor power consumption durante spin
3. **Mejora Hyperthreading**: Previene starvation del hyperthread hermano

#### Por Qué es Necesario

Sin `pause`, un spinlock puede causar:
- **Pipeline Stalls**: El CPU detecta dependencias falsas
- **Power Waste**: Consumo innecesario de energía
- **HT Starvation**: El hyperthread hermano no puede progresar

#### Notas Importantes

1. **SSE2 Required**: Disponible en todos los x86_64 CPUs (SSE2 es mandatory).
2. **No es Barrera**: No previene reordenamiento, pero da oportunidad a otros threads.

#### Referencias

- [Intel SDM Vol 2A - PAUSE](https://www.felixcloutier.com/x86/pause)

---

### `hlt` - Halt CPU

**Ubicación:** `src/impl/x86_64/panic.cpp:119`

```cpp
for (;;) {
    __asm__ volatile ("hlt");
}
```

#### Descripción

Detiene la CPU hasta la próxima interrupción externa.

#### Características

| Propiedad | Valor |
|-----------|-------|
| **Ciclos** | N/A (CPU detenido) |
| **Barreras** | Implícita |
| **Privilegio** | Ring 0 |
| **Arquitectura** | x86/x86_64 específica |

#### Efectos

- **CPU Stop**: Ejecución se detiene
- **Low Power**: CPU entra en estado de bajo consumo
- **Wake Up**: Interrupción externa despierta CPU

#### Usos en GLOBEX_OS

1. **panic()**: Detener sistema después de error fatal
2. **Idle Loop**: En kernel_main() después de tests completados

#### Por Qué en Bucle

```cpp
for (;;) {
    __asm__ volatile ("hlt");
}
```

1. **NMI Wake Up**: Non-Maskable Interrupts pueden despertar el CPU
2. **Re-Halt**: Después de cada NMI, volvemos a hlt
3. **Solo Reset Recupera**: Hardware reset es la única forma de salir

#### Notas Importantes

1. **Preferible a Empty Loop**: `hlt` consume menos energía que `jmp $`
2. **Debugging**: Hardware debuggers pueden detectar estado HLT
3. **STI + HLT**: Patrón común para idle: `sti; hlt` (habilita, luego espera)

#### Referencias

- [Intel SDM Vol 2A - HLT](https://www.felixcloutier.com/x86/hlt)

---

## Apéndice A: Constraints de GCC

### Tabla de Constraints Comunes

| Constraint | Descripción | Ejemplo |
|------------|-------------|---------|
| `"a"` | Registro RAX/EAX/AX/AL | `"a"(value)` |
| `"b"` | Registro RBX/EBX/BX/BL | `"b"(value)` |
| `"c"` | Registro RCX/ECX/CX/CL | `"c"(value)` |
| `"d"` | Registro RDX/EDX/DX/DL | `"d"(port)` |
| `"S"` | Registro RSI/ESI/SI | `"S"(src)` |
| `"D"` | Registro RDI/EDI/DI | `"D"(dest)` |
| `"r"` | Cualquier registro general | `"r"(value)` |
| `"m"` | Operando de memoria | `"m"(var)` |
| `"i"` | Valor inmediato entero | `"i"(42)` |
| `"N"` | Inmediato 0-255 | `"N"(port)` |
| `"Nd"` | Inmediato 0-255 o registro DX | `"Nd"(port)` |

### Modificadores de Constraint

| Modificador | Significado | Ejemplo |
|-------------|-------------|---------|
| `=` | Output-only | `"=a"(result)` |
| `+` | Input-output | `"+m"(lock->locked)` |
| `&` | Early clobber | `"=&a"(temp)` |
| `~` | Clobber | `"memory"` |

---

## Apéndice B: Recursos Adicionales

### Documentación Oficial

- [Intel Software Developer Manual](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)
- [AMD64 Architecture Programmer's Manual](https://www.amd.com/en/support/tech-docs)
- [GCC Extended Asm](https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html)
- [Clang Inline Assembly](https://clang.llvm.org/docs/InlineAssembly.html)

### Referencias de Instrucciones

- [Felix Cloutier x86 Reference](https://www.felixcloutier.com/x86/)
- [Sandpile.org x86 Reference](https://www.sandpile.org/x86/)
- [OSDev Wiki - Inline Assembly](https://wiki.osdev.org/Inline_Assembly)

### Libros Recomendados

- "Intel® 64 and IA-32 Architectures Software Developer's Manual"
- "System V AMD64 ABI"
- "Linux Kernel Development" por Robert Love

---

**Fin del Documento**

*Última actualización: 9 de marzo de 2026*  
*Próxima revisión: Después de implementar AP startup*
