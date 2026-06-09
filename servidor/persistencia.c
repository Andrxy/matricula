#include "persistencia.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#define DIR_DATOS           "datos"
#define ARCH_ESTUDIANTES    DIR_DATOS "/estudiantes.dat"
#define ARCH_PROFESORES     DIR_DATOS "/profesores.dat"
#define ARCH_MATERIAS       DIR_DATOS "/materias.dat"
#define ARCH_MATRICULAS     DIR_DATOS "/matriculas.dat"

/* La línea más larga (Profesor) mide unos 272 bytes. */
#define TAM_LINEA 512

/* Mutex por entidad */
static pthread_mutex_t mtx_estudiantes = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mtx_profesores = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mtx_materias = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mtx_matriculas = PTHREAD_MUTEX_INITIALIZER;

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

/* Divide línea en tokens '|' en sitio y llena campos con punteros a cada uno. */
static int dividir_campos(char *linea, char **campos, int max_campos)
{
    int n = 0;
    char *p = linea;
    while (n < max_campos) {
        campos[n++] = p;
        char *sep = strchr(p, '|');
        if (!sep) break;
        *sep = '\0';
        p = sep + 1;
    }
    return n;
}

/* Serializa como línea de texto separada por '|'. */
static void serializar_estudiante(char *linea, const Estudiante *est)
{
    snprintf(linea, TAM_LINEA, "%s|%s|%s|%s\n",
             est->cedula, est->nombre, est->direccion, est->telefono);
}

/* Parsea una línea y llena el struct. Retorna -1 si incompleta. */
static int parsear_estudiante(char *linea, Estudiante *est)
{
    memset(est, 0, sizeof(*est));
    linea[strcspn(linea, "\n")] = '\0';
    char *campos[4];
    if (dividir_campos(linea, campos, 4) < 4) return -1;
    strncpy(est->cedula,    campos[0], TAM_CEDULA    - 1);
    strncpy(est->nombre,    campos[1], TAM_NOMBRE    - 1);
    strncpy(est->direccion, campos[2], TAM_DIRECCION - 1);
    strncpy(est->telefono,  campos[3], TAM_TELEFONO  - 1);
    return 0;
}

/* Serializa como línea de texto separada por '|'. */
static void serializar_profesor(char *linea, const Profesor *prof)
{
    snprintf(linea, TAM_LINEA, "%s|%s|%s|%s|%s\n",
             prof->cedula, prof->nombre, prof->direccion,
             prof->telefono, prof->grado_academico);
}

/* Parsea una línea y llena el struct. Retorna -1 si incompleta. */
static int parsear_profesor(char *linea, Profesor *prof)
{
    memset(prof, 0, sizeof(*prof));
    linea[strcspn(linea, "\n")] = '\0';
    char *campos[5];
    if (dividir_campos(linea, campos, 5) < 5) return -1;
    strncpy(prof->cedula,          campos[0], TAM_CEDULA          - 1);
    strncpy(prof->nombre,          campos[1], TAM_NOMBRE          - 1);
    strncpy(prof->direccion,       campos[2], TAM_DIRECCION       - 1);
    strncpy(prof->telefono,        campos[3], TAM_TELEFONO        - 1);
    strncpy(prof->grado_academico, campos[4], TAM_GRADO_ACADEMICO - 1);
    return 0;
}

/* Serializa como línea de texto separada por '|'. */
static void serializar_materia(char *linea, const Materia *mat)
{
    snprintf(linea, TAM_LINEA, "%s|%s\n", mat->codigo, mat->descripcion);
}

/* Parsea una línea y llena el struct. Retorna -1 si incompleta. */
static int parsear_materia(char *linea, Materia *mat)
{
    memset(mat, 0, sizeof(*mat));
    linea[strcspn(linea, "\n")] = '\0';
    char *campos[2];
    if (dividir_campos(linea, campos, 2) < 2) return -1;
    strncpy(mat->codigo,      campos[0], TAM_CODIGO      - 1);
    strncpy(mat->descripcion, campos[1], TAM_DESCRIPCION - 1);
    return 0;
}

/* Serializa como línea de texto separada por '|'. */
static void serializar_matricula(char *linea, const Matricula *insc)
{
    snprintf(linea, TAM_LINEA, "%s|%s|%s|%s|%s|%s|%s\n",
             insc->codigo_matricula, insc->cedula_estudiante, insc->cedula_profesor,
             insc->grupo, insc->nrc, insc->codigo_materia, insc->horario);
}

/* Parsea una línea y llena el struct. Retorna -1 si incompleta. */
static int parsear_matricula(char *linea, Matricula *insc)
{
    memset(insc, 0, sizeof(*insc));
    linea[strcspn(linea, "\n")] = '\0';
    char *campos[7];
    if (dividir_campos(linea, campos, 7) < 7) return -1;
    strncpy(insc->codigo_matricula,  campos[0], TAM_CODIGO  - 1);
    strncpy(insc->cedula_estudiante, campos[1], TAM_CEDULA  - 1);
    strncpy(insc->cedula_profesor,   campos[2], TAM_CEDULA  - 1);
    strncpy(insc->grupo,             campos[3], TAM_GRUPO   - 1);
    strncpy(insc->nrc,               campos[4], TAM_NRC     - 1);
    strncpy(insc->codigo_materia,    campos[5], TAM_CODIGO  - 1);
    strncpy(insc->horario,           campos[6], TAM_HORARIO - 1);
    return 0;
}

/* Verifica duplicado por cédula y agrega al final si no existe. */
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

/* Busca por cédula recorriendo el archivo línea por línea. */
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

/* Verifica duplicado por cédula y agrega al final si no existe. */
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

/* Busca por cédula recorriendo el archivo línea por línea. */
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

/* Verifica duplicado por código y agrega al final si no existe. */
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

/* Busca por código recorriendo el archivo línea por línea. */
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

/* Verifica duplicado por código y agrega al final si no existe. */
int matricula_insertar(const Matricula *insc)
{
    char linea[TAM_LINEA];

    pthread_mutex_lock(&mtx_matriculas);

    FILE *archivo = fopen(ARCH_MATRICULAS, "r");
    if (archivo) {
        Matricula temporal;
        while (fgets(linea, sizeof(linea), archivo)) {
            if (parsear_matricula(linea, &temporal) == 0 &&
                strncmp(temporal.codigo_matricula, insc->codigo_matricula, TAM_CODIGO) == 0) {
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

/* Busca por código recorriendo el archivo línea por línea. */
int matricula_buscar(const char *codigo_matricula, Matricula *destino)
{
    char linea[TAM_LINEA];
    int resultado = RES_NO_ENCONTRADO;

    pthread_mutex_lock(&mtx_matriculas);

    FILE *archivo = fopen(ARCH_MATRICULAS, "r");
    if (archivo) {
        Matricula temporal;
        while (fgets(linea, sizeof(linea), archivo)) {
            if (parsear_matricula(linea, &temporal) == 0 &&
                strncmp(temporal.codigo_matricula, codigo_matricula, TAM_CODIGO) == 0) {
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
