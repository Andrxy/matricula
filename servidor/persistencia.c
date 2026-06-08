#include "persistencia.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

/* ── Archivos de datos (uno por entidad) ───────────────────────────────── */
#define DIR_DATOS           "datos"
#define ARCH_ESTUDIANTES    DIR_DATOS "/estudiantes.dat"
#define ARCH_PROFESORES     DIR_DATOS "/profesores.dat"
#define ARCH_MATERIAS       DIR_DATOS "/materias.dat"
#define ARCH_MATRICULAS     DIR_DATOS "/matriculas.dat"

/* Tamaño máximo de una línea serializada.
   La línea más larga es Profesor: 15+1+63+1+63+1+63+1+63+\n ≈ 273 bytes. */
#define TAM_LINEA 512

/* ── Mutex por entidad (patrón guia-mutex: PTHREAD_MUTEX_INITIALIZER) ─── */
static pthread_mutex_t mtx_estudiantes = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mtx_profesores = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mtx_materias = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mtx_matriculas = PTHREAD_MUTEX_INITIALIZER;

/* ── Init / destruir ───────────────────────────────────────────────────── */

void persistencia_init(void)
{
    if (mkdir(DIR_DATOS, 0755) == -1 && errno != EEXIST)
        perror("mkdir " DIR_DATOS);
}

void persistencia_destruir(void)
{
    pthread_mutex_destroy(&mtx_estudiantes);
    pthread_mutex_destroy(&mtx_profesores);
    pthread_mutex_destroy(&mtx_materias);
    pthread_mutex_destroy(&mtx_matriculas);
}

/* ── Serialización / parseo por entidad ────────────────────────────────── */
/* Formato de cada línea: campos separados por '|', terminados en '\n'.    */

static void serializar_estudiante(char *linea, const Estudiante *est)
{
    snprintf(linea, TAM_LINEA, "%s|%s|%s|%s\n",
             est->cedula, est->nombre, est->apellido, est->email);
}

static int parsear_estudiante(char *linea, Estudiante *est)
{
    memset(est, 0, sizeof(*est));
    linea[strcspn(linea, "\n")] = '\0';
    return (sscanf(linea, "%15[^|]|%63[^|]|%63[^|]|%63[^|]",
                   est->cedula, est->nombre, est->apellido, est->email) == 4) ? 0 : -1;
}

static void serializar_profesor(char *linea, const Profesor *prof)
{
    snprintf(linea, TAM_LINEA, "%s|%s|%s|%s|%s\n",
             prof->cedula, prof->nombre, prof->apellido, prof->departamento, prof->email);
}

static int parsear_profesor(char *linea, Profesor *prof)
{
    memset(prof, 0, sizeof(*prof));
    linea[strcspn(linea, "\n")] = '\0';
    return (sscanf(linea, "%15[^|]|%63[^|]|%63[^|]|%63[^|]|%63[^|]",
                   prof->cedula, prof->nombre, prof->apellido,
                   prof->departamento, prof->email) == 5) ? 0 : -1;
}

static void serializar_materia(char *linea, const Materia *mat)
{
    snprintf(linea, TAM_LINEA, "%s|%s|%s|%d|%s\n",
             mat->codigo, mat->nombre, mat->descripcion,
             mat->creditos, mat->cedula_profesor);
}

static int parsear_materia(char *linea, Materia *mat)
{
    memset(mat, 0, sizeof(*mat));
    linea[strcspn(linea, "\n")] = '\0';
    return (sscanf(linea, "%15[^|]|%63[^|]|%127[^|]|%d|%15[^|]",
                   mat->codigo, mat->nombre, mat->descripcion,
                   &mat->creditos, mat->cedula_profesor) == 5) ? 0 : -1;
}

static void serializar_matricula(char *linea, const Matricula *insc)
{
    snprintf(linea, TAM_LINEA, "%s|%s|%s\n",
             insc->cedula_estudiante, insc->codigo_materia, insc->periodo);
}

static int parsear_matricula(char *linea, Matricula *insc)
{
    memset(insc, 0, sizeof(*insc));
    linea[strcspn(linea, "\n")] = '\0';
    return (sscanf(linea, "%15[^|]|%15[^|]|%15[^|]",
                   insc->cedula_estudiante, insc->codigo_materia,
                   insc->periodo) == 3) ? 0 : -1;
}

/* ── ESTUDIANTE ────────────────────────────────────────────────────────── */

int estudiante_insertar(const Estudiante *est)
{
    char linea[TAM_LINEA];

    pthread_mutex_lock(&mtx_estudiantes);

    FILE *archivo = fopen(ARCH_ESTUDIANTES, "r");
    if (archivo) {
        Estudiante temporal;
        while (fgets(linea, sizeof(linea), archivo)) {
            if (parsear_estudiante(linea, &temporal) == 0 &&
                strncmp(temporal.cedula, est->cedula, TAM_CEDULA) == 0) {
                fclose(archivo);
                pthread_mutex_unlock(&mtx_estudiantes);
                return RES_DUPLICADO;
            }
        }
        fclose(archivo);
    }

    archivo = fopen(ARCH_ESTUDIANTES, "a");
    if (!archivo) {
        pthread_mutex_unlock(&mtx_estudiantes);
        return RES_ERROR;
    }
    serializar_estudiante(linea, est);
    fputs(linea, archivo);
    fclose(archivo);

    pthread_mutex_unlock(&mtx_estudiantes);
    return RES_OK;
}

int estudiante_buscar(const char *cedula, Estudiante *destino)
{
    char linea[TAM_LINEA];
    int resultado = RES_NO_ENCONTRADO;

    pthread_mutex_lock(&mtx_estudiantes);

    FILE *archivo = fopen(ARCH_ESTUDIANTES, "r");
    if (archivo) {
        Estudiante temporal;
        while (fgets(linea, sizeof(linea), archivo)) {
            if (parsear_estudiante(linea, &temporal) == 0 &&
                strncmp(temporal.cedula, cedula, TAM_CEDULA) == 0) {
                *destino = temporal;
                resultado = RES_OK;
                break;
            }
        }
        fclose(archivo);
    }

    pthread_mutex_unlock(&mtx_estudiantes);
    return resultado;
}

/* ── PROFESOR ──────────────────────────────────────────────────────────── */

int profesor_insertar(const Profesor *prof)
{
    char linea[TAM_LINEA];

    pthread_mutex_lock(&mtx_profesores);

    FILE *archivo = fopen(ARCH_PROFESORES, "r");
    if (archivo) {
        Profesor temporal;
        while (fgets(linea, sizeof(linea), archivo)) {
            if (parsear_profesor(linea, &temporal) == 0 &&
                strncmp(temporal.cedula, prof->cedula, TAM_CEDULA) == 0) {
                fclose(archivo);
                pthread_mutex_unlock(&mtx_profesores);
                return RES_DUPLICADO;
            }
        }
        fclose(archivo);
    }

    archivo = fopen(ARCH_PROFESORES, "a");
    if (!archivo) {
        pthread_mutex_unlock(&mtx_profesores);
        return RES_ERROR;
    }
    serializar_profesor(linea, prof);
    fputs(linea, archivo);
    fclose(archivo);

    pthread_mutex_unlock(&mtx_profesores);
    return RES_OK;
}

int profesor_buscar(const char *cedula, Profesor *destino)
{
    char linea[TAM_LINEA];
    int resultado = RES_NO_ENCONTRADO;

    pthread_mutex_lock(&mtx_profesores);

    FILE *archivo = fopen(ARCH_PROFESORES, "r");
    if (archivo) {
        Profesor temporal;
        while (fgets(linea, sizeof(linea), archivo)) {
            if (parsear_profesor(linea, &temporal) == 0 &&
                strncmp(temporal.cedula, cedula, TAM_CEDULA) == 0) {
                *destino = temporal;
                resultado = RES_OK;
                break;
            }
        }
        fclose(archivo);
    }

    pthread_mutex_unlock(&mtx_profesores);
    return resultado;
}

/* ── MATERIA ───────────────────────────────────────────────────────────── */

int materia_insertar(const Materia *mat)
{
    char linea[TAM_LINEA];

    pthread_mutex_lock(&mtx_materias);

    FILE *archivo = fopen(ARCH_MATERIAS, "r");
    if (archivo) {
        Materia temporal;
        while (fgets(linea, sizeof(linea), archivo)) {
            if (parsear_materia(linea, &temporal) == 0 &&
                strncmp(temporal.codigo, mat->codigo, TAM_CODIGO) == 0) {
                fclose(archivo);
                pthread_mutex_unlock(&mtx_materias);
                return RES_DUPLICADO;
            }
        }
        fclose(archivo);
    }

    archivo = fopen(ARCH_MATERIAS, "a");
    if (!archivo) {
        pthread_mutex_unlock(&mtx_materias);
        return RES_ERROR;
    }
    serializar_materia(linea, mat);
    fputs(linea, archivo);
    fclose(archivo);

    pthread_mutex_unlock(&mtx_materias);
    return RES_OK;
}

int materia_buscar(const char *codigo, Materia *destino)
{
    char linea[TAM_LINEA];
    int resultado = RES_NO_ENCONTRADO;

    pthread_mutex_lock(&mtx_materias);

    FILE *archivo = fopen(ARCH_MATERIAS, "r");
    if (archivo) {
        Materia temporal;
        while (fgets(linea, sizeof(linea), archivo)) {
            if (parsear_materia(linea, &temporal) == 0 &&
                strncmp(temporal.codigo, codigo, TAM_CODIGO) == 0) {
                *destino = temporal;
                resultado = RES_OK;
                break;
            }
        }
        fclose(archivo);
    }

    pthread_mutex_unlock(&mtx_materias);
    return resultado;
}

/* ── MATRICULA ─────────────────────────────────────────────────────────── */

int matricula_insertar(const Matricula *insc)
{
    char linea[TAM_LINEA];

    pthread_mutex_lock(&mtx_matriculas);

    FILE *archivo = fopen(ARCH_MATRICULAS, "r");
    if (archivo) {
        Matricula temporal;
        while (fgets(linea, sizeof(linea), archivo)) {
            if (parsear_matricula(linea, &temporal) == 0 &&
                strncmp(temporal.cedula_estudiante, insc->cedula_estudiante, TAM_CEDULA) == 0 &&
                strncmp(temporal.codigo_materia, insc->codigo_materia, TAM_CODIGO) == 0) {
                fclose(archivo);
                pthread_mutex_unlock(&mtx_matriculas);
                return RES_DUPLICADO;
            }
        }
        fclose(archivo);
    }

    archivo = fopen(ARCH_MATRICULAS, "a");
    if (!archivo) {
        pthread_mutex_unlock(&mtx_matriculas);
        return RES_ERROR;
    }
    serializar_matricula(linea, insc);
    fputs(linea, archivo);
    fclose(archivo);

    pthread_mutex_unlock(&mtx_matriculas);
    return RES_OK;
}

int matricula_buscar(const char *cedula_estudiante, const char *codigo_materia, Matricula *destino)
{
    char linea[TAM_LINEA];
    int resultado = RES_NO_ENCONTRADO;

    pthread_mutex_lock(&mtx_matriculas);

    FILE *archivo = fopen(ARCH_MATRICULAS, "r");
    if (archivo) {
        Matricula temporal;
        while (fgets(linea, sizeof(linea), archivo)) {
            if (parsear_matricula(linea, &temporal) == 0 &&
                strncmp(temporal.cedula_estudiante, cedula_estudiante, TAM_CEDULA) == 0 &&
                strncmp(temporal.codigo_materia, codigo_materia, TAM_CODIGO) == 0) {
                *destino = temporal;
                resultado = RES_OK;
                break;
            }
        }
        fclose(archivo);
    }

    pthread_mutex_unlock(&mtx_matriculas);
    return resultado;
}
