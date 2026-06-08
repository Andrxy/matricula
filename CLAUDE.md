# Guía de trabajo con Claude Code — Sistema de Matrícula Distribuido

Secuencia de prompts ordenada por fases, con el anclaje correcto a las **dos guías reales** que existen:

- `docs/guia-mutex.md` → fases 3c, 3d, 4
- `docs/guia-colas-mensajes.md` → fases 3b, 3d
- Todo lo demás (sockets, pipes, persistencia, protocolo, cliente, Makefile) → **autónomo**, con prácticas POSIX estándar.

> **Tip de uso:** antes de cada prompt que cite una guía, abrila en el contexto con `@docs/guia-X.md` para que entre al contexto inmediato (no basta con mencionarla de nombre).

> **Orden de trabajo:** 0 → 1 → 2 → 3a → 3b → 3c → 3d → 3e → 4 → 5 → 6. Compilar en cada paso.

---

## Estructura del repo

```
matricula-distribuido/
├── CLAUDE.md                 # convenciones + arquitectura + rúbrica (contexto permanente)
├── Makefile
├── docs/                     
│  
├── common/
│   ├── protocolo.h           # structs entidades + struct mensaje + códigos
│   └── protocolo.c           # serializar / deserializar
├── servidor/
│   ├── servidor.c            # main + accept loop
│   ├── cola.c/.h             # wrapper de la message queue POSIX
│   ├── despachador.c/.h      # lee cola → lanza hilo por operación
│   ├── persistencia.c/.h     # CRUD por archivo + mutex por entidad
│   ├── matricula.c/.h        # validación de integridad referencial
│   └── log.c/.h              # pipe + hilo de logs
└── cliente/
    ├── cliente.c             # main + conexión socket
    └── menu.c/.h             # menús
```

---

## Fase 0 — Contexto permanente (`CLAUDE.md`)

✅ **Ya hecho.** Es el archivo que define convenciones, arquitectura y rúbrica para todos los prompts siguientes.

## Fase 1 — Guías MD

✅ **Ya hechas** (mutex y colas de mensajes).

---

## Fase 2 — Protocolo base

✅ **Ya hecho.**

> Implementa `common/protocolo.h` y `protocolo.c`: structs de las 4 entidades, struct de mensaje genérico (entidad, operación, payload fijo), `#define` de códigos, y serializar/deserializar a buffer plano. Sin red todavía. Debe compilar.

---

## Fase 3 — Servidor incremental

### 3a — Socket de escucha *(autónomo)*

> Implementa el accept loop TCP/IP en `servidor/servidor.c`: `socket`, `bind`, `listen`, `accept`, y un hilo por conexión que recibe un mensaje serializado y por ahora solo lo imprime. Usa `protocolo.h`. Respeta el `CLAUDE.md` y que compile con `-Wall -Wextra -pthread`.

### 3b — Cola de mensajes *(ancla: guia-colas-mensajes.md)*

> Implementa `servidor/cola.c/.h` **siguiendo el patrón de docs/guia-colas-mensajes.md**. Debe ser una **message queue POSIX del kernel** (`mq_open`, `mq_send`, `mq_receive`, `mq_close`, `mq_unlink`), **NO** un TDA cola hecho a mano con lista enlazada. Envuelve la cola en funciones propias del módulo. El hilo de conexión del socket, en lugar de imprimir el mensaje recibido, lo encola con `mq_send`; deja la cola lista para que el despachador (3d) la consuma con `mq_receive`. Recuerda enlazar con `-lrt`.

### 3c — Persistencia + mutex *(ancla: guia-mutex.md)*

> Implementa `servidor/persistencia.c/.h`: insertar y buscar por campo clave para cada entidad, cada una en su archivo de texto separado, búsqueda leyendo el archivo línea por línea (sin cargar todo en memoria estática). Para la exclusión mutua usá un mutex propio por entidad **siguiendo el patrón de docs/guia-mutex.md**. Respeta el `CLAUDE.md`.

### 3d — Despachador *(ancla: guia-colas-mensajes.md + guia-mutex.md)*

> Implementa `servidor/despachador.c/.h`: saca mensajes de la cola **(usando el wrapper de docs/guia-colas-mensajes.md)** y por cada uno lanza un **hilo dedicado por operación** **(exclusión mutua según docs/guia-mutex.md)** que llama a persistencia y responde al cliente por el socket.

### 3e — Logs por pipe *(autónomo)*

> Implementa `servidor/log.c/.h`: un pipe interno (`pipe()`) y un hilo que lee del pipe y escribe a un archivo de log. Los hilos trabajadores escriben sus eventos al pipe. Sincronizá la escritura al pipe si hace falta.

---

## Fase 4 — Matrícula (integridad referencial) *(ancla: guia-mutex.md)*

> Implementa `servidor/matricula.c/.h` reutilizando las búsquedas de `persistencia.c`: antes de insertar una matrícula valida que existan **cédula de estudiante, cédula de profesor y código de materia** en sus archivos; si falta alguno, **deniega** la operación completa y devuelve el motivo. Usá el mismo manejo de mutex que `persistencia.c`.

---

## Fase 5 — Cliente *(autónomo)*

> Implementa `cliente/menu.c/.h` y `cliente/cliente.c` usando `protocolo.h` para serializar: menú principal (Estudiantes, Profesores, Materias, Matrícula, Salir) y submenú (Ingresar, Buscar, Regresar). Lectura guiada por teclado, conexión por socket TCP/IP, envío del mensaje y muestra de la respuesta. Control de errores de captura de datos y salida limpia.

---

## Fase 6 — Makefile y pruebas *(autónomo)*

> Crea un `Makefile` que compile cliente y servidor por separado con `-Wall -Wextra -pthread` (y `-lrt` para el servidor), con targets `all`, `servidor`, `cliente`, `clean`. Luego dame un guion de prueba manual que verifique cada criterio de la rúbrica.

---

## Recordatorios clave

- **Message queue ≠ TDA cola.** La tarea exige el objeto IPC del kernel POSIX, no una lista enlazada. Vale 10 pts junto con los pipes.
- **El ítem IPC (10 pts) pide AMBAS cosas:** message queue *y* pipes. No descuides los logs por pipe (3e) aunque parezca lo menos vistoso.
- **Hilos (15 pts): "un hilo por comando".** Cada operación debe lanzar su propio `pthread`, no procesarse en línea.
- **Compilar en cada incremento** con `-Wall -Wextra -pthread` sin warnings.
- **`-lrt`** es obligatorio para enlazar message queues POSIX.

## Mapa de anclaje a guías

| Fase | Módulo | Guía |
|------|--------|------|
| 3b | cola.c/.h | guia-colas-mensajes.md |
| 3c | persistencia.c/.h | guia-mutex.md |
| 3d | despachador.c/.h | guia-colas-mensajes.md + guia-mutex.md |
| 4 | matricula.c/.h | guia-mutex.md (vía persistencia) |
| 3a, 3e, 5, 6 | sockets, pipes, cliente, Makefile | autónomo (POSIX estándar) |

## Entrega

- **Formato:** múltiples `.c`/`.h` + Makefile, comprimido en `.zip` o `.tar.gz`.
- **Fecha límite:** viernes 8 de junio, 23:55, vía aula virtual.
- **Modalidad:** grupos previamente organizados.