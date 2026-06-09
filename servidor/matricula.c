#include "matricula.h"
#include "persistencia.h"

#include <pthread.h>

/* Sección crítica: evita que dos hilos validen e inserten el mismo registro a la vez. */
static pthread_mutex_t mtx_validacion = PTHREAD_MUTEX_INITIALIZER;

int matricula_insertar_validado(const Matricula *insc)
{
    Estudiante est;
    Materia mat;
    Profesor prof;
    int resultado;

    pthread_mutex_lock(&mtx_validacion);

    if (estudiante_buscar(insc->cedula_estudiante, &est) != RES_OK) {
        pthread_mutex_unlock(&mtx_validacion);
        return RES_MATRICULA_SIN_ESTUDIANTE;
    }

    if (profesor_buscar(insc->cedula_profesor, &prof) != RES_OK) {
        pthread_mutex_unlock(&mtx_validacion);
        return RES_MATRICULA_SIN_PROFESOR;
    }

    if (materia_buscar(insc->codigo_materia, &mat) != RES_OK) {
        pthread_mutex_unlock(&mtx_validacion);
        return RES_MATRICULA_SIN_MATERIA;
    }

    resultado = matricula_insertar(insc);

    pthread_mutex_unlock(&mtx_validacion);
    return resultado;
}
