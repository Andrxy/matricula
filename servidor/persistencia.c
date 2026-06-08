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
   La línea más larga es Profesor: 15+1+63+1+127+1+31+1+31+\n ≈ 272 bytes. */
#define TAM_LINEA 512

/* ── Mutex por entidad (patrón guia-mutex: PTHREAD_MUTEX_INITIALIZER) ─── */
static pthread_mutex_t mtx_estudiantes = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mtx_profesores = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mtx_materias = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mtx_matriculas = PTHREAD_MUTEX_INITIALIZER;

/* Crea el directorio de datos donde se guardan los archivos .dat, ignorando el error si ya existe. */
void persistencia_init(void)
{
    if (mkdir(DIR_DATOS, 0755) == -1 && errno != EEXIST)
        perror("mkdir " DIR_DATOS);
}

/* Libera los cuatro mutexes de entidad al momento de cierre del servidor. */
void persistencia_destruir(void)
{
    pthread_mutex_destroy(&mtx_estudiantes);
    pthread_mutex_destroy(&mtx_profesores);
    pthread_mutex_destroy(&mtx_materias);
    pthread_mutex_destroy(&mtx_matriculas);
}

/* Divide la línea en tokens separados por '|' modificándola directamente sobre el buffer y llena el arreglo campos con un puntero al inicio de cada token. */
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

/* Serializa un Estudiante como línea de texto con los campos separados por '|' y terminada en '\n'. */
static void serializar_estudiante(char *linea, const Estudiante *est)
{
    snprintf(linea, TAM_LINEA, "%s|%s|%s|%s\n",
             est->cedula, est->nombre, est->direccion, est->telefono);
}

/* Parsea una línea del archivo y llena el struct Estudiante campo a campo. Retorna error si la línea está incompleta. */
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

/* Serializa un Profesor como línea de texto con los cinco campos separados por '|'. */
static void serializar_profesor(char *linea, const Profesor *prof)
{
    snprintf(linea, TAM_LINEA, "%s|%s|%s|%s|%s\n",
             prof->cedula, prof->nombre, prof->direccion,
             prof->telefono, prof->grado_academico);
}

/* Parsea una línea del archivo y llena el struct Profesor. Retorna error si faltan campos. */
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

/* Serializa una Materia como línea de texto con código y descripción separados por '|'. */
static void serializar_materia(char *linea, const Materia *mat)
{
    snprintf(linea, TAM_LINEA, "%s|%s\n", mat->codigo, mat->descripcion);
}

/* Parsea una línea del archivo y llena el struct Materia. Retorna error si faltan campos. */
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

/* Serializa una Matricula como línea de texto con los siete campos separados por '|'. */
static void serializar_matricula(char *linea, const Matricula *insc)
{
    snprintf(linea, TAM_LINEA, "%s|%s|%s|%s|%s|%s|%s\n",
             insc->codigo_matricula, insc->cedula_estudiante, insc->cedula_profesor,
             insc->grupo, insc->nrc, insc->codigo_materia, insc->horario);
}

/* Parsea una línea del archivo y llena el struct Matricula. Retorna error si faltan campos. */
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

/* Adquiere el mutex, recorre el archivo para detectar cédula duplicada y, si no existe, agrega el registro al final en modo append. */
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

/* Adquiere el mutex y recorre el archivo línea por línea comparando cédulas hasta encontrar el registro o agotar el archivo. */
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

/* Adquiere el mutex, revisa duplicados por cédula en el archivo y agrega el profesor al final si no existe. */
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

/* Adquiere el mutex y busca el profesor recorriendo el archivo línea por línea por su cédula. */
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

/* Adquiere el mutex, revisa duplicados por código en el archivo y agrega la materia al final si no existe. */
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

/* Adquiere el mutex y busca la materia recorriendo el archivo línea por línea por su código. */
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

/* Adquiere el mutex, revisa duplicados por código de matrícula en el archivo y agrega el registro al final si no existe. */
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

/* Adquiere el mutex y busca la matrícula recorriendo el archivo línea por línea por su código. */
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
