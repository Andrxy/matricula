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
static pthread_mutex_t mtx_profesores  = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mtx_materias    = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mtx_matriculas  = PTHREAD_MUTEX_INITIALIZER;

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

static void serializar_estudiante(char *buf, const Estudiante *e)
{
    snprintf(buf, TAM_LINEA, "%s|%s|%s|%s\n",
             e->cedula, e->nombre, e->apellido, e->email);
}

static int parsear_estudiante(char *linea, Estudiante *e)
{
    memset(e, 0, sizeof(*e));
    linea[strcspn(linea, "\n")] = '\0';
    return (sscanf(linea, "%15[^|]|%63[^|]|%63[^|]|%63[^|]",
                   e->cedula, e->nombre, e->apellido, e->email) == 4) ? 0 : -1;
}

static void serializar_profesor(char *buf, const Profesor *p)
{
    snprintf(buf, TAM_LINEA, "%s|%s|%s|%s|%s\n",
             p->cedula, p->nombre, p->apellido, p->departamento, p->email);
}

static int parsear_profesor(char *linea, Profesor *p)
{
    memset(p, 0, sizeof(*p));
    linea[strcspn(linea, "\n")] = '\0';
    return (sscanf(linea, "%15[^|]|%63[^|]|%63[^|]|%63[^|]|%63[^|]",
                   p->cedula, p->nombre, p->apellido, p->departamento, p->email) == 5) ? 0 : -1;
}

static void serializar_materia(char *buf, const Materia *m)
{
    snprintf(buf, TAM_LINEA, "%s|%s|%s|%d|%s\n",
             m->codigo, m->nombre, m->descripcion, m->creditos, m->cedula_profesor);
}

static int parsear_materia(char *linea, Materia *m)
{
    memset(m, 0, sizeof(*m));
    linea[strcspn(linea, "\n")] = '\0';
    return (sscanf(linea, "%15[^|]|%63[^|]|%127[^|]|%d|%15[^|]",
                   m->codigo, m->nombre, m->descripcion,
                   &m->creditos, m->cedula_profesor) == 5) ? 0 : -1;
}

static void serializar_matricula(char *buf, const Matricula *mc)
{
    snprintf(buf, TAM_LINEA, "%s|%s|%s\n",
             mc->cedula_estudiante, mc->codigo_materia, mc->periodo);
}

static int parsear_matricula(char *linea, Matricula *mc)
{
    memset(mc, 0, sizeof(*mc));
    linea[strcspn(linea, "\n")] = '\0';
    return (sscanf(linea, "%15[^|]|%15[^|]|%15[^|]",
                   mc->cedula_estudiante, mc->codigo_materia, mc->periodo) == 3) ? 0 : -1;
}

/* ── ESTUDIANTE ────────────────────────────────────────────────────────── */

int estudiante_insertar(const Estudiante *e)
{
    char linea[TAM_LINEA];

    pthread_mutex_lock(&mtx_estudiantes);

    FILE *f = fopen(ARCH_ESTUDIANTES, "r");
    if (f) {
        Estudiante tmp;
        while (fgets(linea, sizeof(linea), f)) {
            if (parsear_estudiante(linea, &tmp) == 0 &&
                strncmp(tmp.cedula, e->cedula, TAM_CEDULA) == 0) {
                fclose(f);
                pthread_mutex_unlock(&mtx_estudiantes);
                return RES_DUPLICADO;
            }
        }
        fclose(f);
    }

    f = fopen(ARCH_ESTUDIANTES, "a");
    if (!f) {
        pthread_mutex_unlock(&mtx_estudiantes);
        return RES_ERROR;
    }
    serializar_estudiante(linea, e);
    fputs(linea, f);
    fclose(f);

    pthread_mutex_unlock(&mtx_estudiantes);
    return RES_OK;
}

int estudiante_buscar(const char *cedula, Estudiante *dest)
{
    char linea[TAM_LINEA];
    int resultado = RES_NO_ENCONTRADO;

    pthread_mutex_lock(&mtx_estudiantes);

    FILE *f = fopen(ARCH_ESTUDIANTES, "r");
    if (f) {
        Estudiante tmp;
        while (fgets(linea, sizeof(linea), f)) {
            if (parsear_estudiante(linea, &tmp) == 0 &&
                strncmp(tmp.cedula, cedula, TAM_CEDULA) == 0) {
                *dest = tmp;
                resultado = RES_OK;
                break;
            }
        }
        fclose(f);
    }

    pthread_mutex_unlock(&mtx_estudiantes);
    return resultado;
}

/* ── PROFESOR ──────────────────────────────────────────────────────────── */

int profesor_insertar(const Profesor *p)
{
    char linea[TAM_LINEA];

    pthread_mutex_lock(&mtx_profesores);

    FILE *f = fopen(ARCH_PROFESORES, "r");
    if (f) {
        Profesor tmp;
        while (fgets(linea, sizeof(linea), f)) {
            if (parsear_profesor(linea, &tmp) == 0 &&
                strncmp(tmp.cedula, p->cedula, TAM_CEDULA) == 0) {
                fclose(f);
                pthread_mutex_unlock(&mtx_profesores);
                return RES_DUPLICADO;
            }
        }
        fclose(f);
    }

    f = fopen(ARCH_PROFESORES, "a");
    if (!f) {
        pthread_mutex_unlock(&mtx_profesores);
        return RES_ERROR;
    }
    serializar_profesor(linea, p);
    fputs(linea, f);
    fclose(f);

    pthread_mutex_unlock(&mtx_profesores);
    return RES_OK;
}

int profesor_buscar(const char *cedula, Profesor *dest)
{
    char linea[TAM_LINEA];
    int resultado = RES_NO_ENCONTRADO;

    pthread_mutex_lock(&mtx_profesores);

    FILE *f = fopen(ARCH_PROFESORES, "r");
    if (f) {
        Profesor tmp;
        while (fgets(linea, sizeof(linea), f)) {
            if (parsear_profesor(linea, &tmp) == 0 &&
                strncmp(tmp.cedula, cedula, TAM_CEDULA) == 0) {
                *dest = tmp;
                resultado = RES_OK;
                break;
            }
        }
        fclose(f);
    }

    pthread_mutex_unlock(&mtx_profesores);
    return resultado;
}

/* ── MATERIA ───────────────────────────────────────────────────────────── */

int materia_insertar(const Materia *m)
{
    char linea[TAM_LINEA];

    pthread_mutex_lock(&mtx_materias);

    FILE *f = fopen(ARCH_MATERIAS, "r");
    if (f) {
        Materia tmp;
        while (fgets(linea, sizeof(linea), f)) {
            if (parsear_materia(linea, &tmp) == 0 &&
                strncmp(tmp.codigo, m->codigo, TAM_CODIGO) == 0) {
                fclose(f);
                pthread_mutex_unlock(&mtx_materias);
                return RES_DUPLICADO;
            }
        }
        fclose(f);
    }

    f = fopen(ARCH_MATERIAS, "a");
    if (!f) {
        pthread_mutex_unlock(&mtx_materias);
        return RES_ERROR;
    }
    serializar_materia(linea, m);
    fputs(linea, f);
    fclose(f);

    pthread_mutex_unlock(&mtx_materias);
    return RES_OK;
}

int materia_buscar(const char *codigo, Materia *dest)
{
    char linea[TAM_LINEA];
    int resultado = RES_NO_ENCONTRADO;

    pthread_mutex_lock(&mtx_materias);

    FILE *f = fopen(ARCH_MATERIAS, "r");
    if (f) {
        Materia tmp;
        while (fgets(linea, sizeof(linea), f)) {
            if (parsear_materia(linea, &tmp) == 0 &&
                strncmp(tmp.codigo, codigo, TAM_CODIGO) == 0) {
                *dest = tmp;
                resultado = RES_OK;
                break;
            }
        }
        fclose(f);
    }

    pthread_mutex_unlock(&mtx_materias);
    return resultado;
}

/* ── MATRICULA ─────────────────────────────────────────────────────────── */

int matricula_insertar(const Matricula *mc)
{
    char linea[TAM_LINEA];

    pthread_mutex_lock(&mtx_matriculas);

    FILE *f = fopen(ARCH_MATRICULAS, "r");
    if (f) {
        Matricula tmp;
        while (fgets(linea, sizeof(linea), f)) {
            if (parsear_matricula(linea, &tmp) == 0 &&
                strncmp(tmp.cedula_estudiante, mc->cedula_estudiante, TAM_CEDULA) == 0 &&
                strncmp(tmp.codigo_materia,    mc->codigo_materia,    TAM_CODIGO) == 0) {
                fclose(f);
                pthread_mutex_unlock(&mtx_matriculas);
                return RES_DUPLICADO;
            }
        }
        fclose(f);
    }

    f = fopen(ARCH_MATRICULAS, "a");
    if (!f) {
        pthread_mutex_unlock(&mtx_matriculas);
        return RES_ERROR;
    }
    serializar_matricula(linea, mc);
    fputs(linea, f);
    fclose(f);

    pthread_mutex_unlock(&mtx_matriculas);
    return RES_OK;
}

int matricula_buscar(const char *cedula_est, const char *cod_mat, Matricula *dest)
{
    char linea[TAM_LINEA];
    int resultado = RES_NO_ENCONTRADO;

    pthread_mutex_lock(&mtx_matriculas);

    FILE *f = fopen(ARCH_MATRICULAS, "r");
    if (f) {
        Matricula tmp;
        while (fgets(linea, sizeof(linea), f)) {
            if (parsear_matricula(linea, &tmp) == 0 &&
                strncmp(tmp.cedula_estudiante, cedula_est, TAM_CEDULA) == 0 &&
                strncmp(tmp.codigo_materia,    cod_mat,    TAM_CODIGO) == 0) {
                *dest = tmp;
                resultado = RES_OK;
                break;
            }
        }
        fclose(f);
    }

    pthread_mutex_unlock(&mtx_matriculas);
    return resultado;
}
