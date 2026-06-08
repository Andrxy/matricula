#include "matricula.h"
#include "persistencia.h"

#include <pthread.h>

/* ── Mutex de validación (patrón guia-mutex: PTHREAD_MUTEX_INITIALIZER) ──
   Un único mutex protege la secuencia completa:
     buscar_estudiante → buscar_materia → buscar_profesor → insertar_matricula
   como sección crítica atómica, evitando que dos hilos realicen la misma
   validación concurrentemente y ambos pasen a insertar.

   Orden de adquisición:
     mtx_validacion (este módulo)  →  mutexes de entidad (persistencia.c)
   Los mutexes de entidad se adquieren y liberan dentro de cada *_buscar/
   matricula_insertar; nunca se sostienen dos a la vez, así que no hay
   riesgo de deadlock.                                                     */
static pthread_mutex_t mtx_validacion = PTHREAD_MUTEX_INITIALIZER;

int matricula_insertar_validado(const Matricula *insc)
{
    Estudiante est;
    Materia mat;
    Profesor prof;
    int resultado;

    pthread_mutex_lock(&mtx_validacion);

    /* 1. Verificar que el estudiante existe. */
    if (estudiante_buscar(insc->cedula_estudiante, &est) != RES_OK) {
        pthread_mutex_unlock(&mtx_validacion);
        return RES_MATRICULA_SIN_ESTUDIANTE;
    }

    /* 2. Verificar que la materia existe.
          Si existe, mat.cedula_profesor contiene la referencia al profesor. */
    if (materia_buscar(insc->codigo_materia, &mat) != RES_OK) {
        pthread_mutex_unlock(&mtx_validacion);
        return RES_MATRICULA_SIN_MATERIA;
    }

    /* 3. Verificar que el profesor asignado a esa materia existe. */
    if (profesor_buscar(mat.cedula_profesor, &prof) != RES_OK) {
        pthread_mutex_unlock(&mtx_validacion);
        return RES_MATRICULA_SIN_PROFESOR;
    }

    /* 4. Todas las referencias son válidas; delegar la inserción
          (que incluye su propio chequeo de duplicado). */
    resultado = matricula_insertar(insc);

    pthread_mutex_unlock(&mtx_validacion);
    return resultado;
}
