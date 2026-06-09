#include "log.h"

#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Debe caber en PIPE_BUF (4096 en Linux) para que write() sea atómica. */
#define TAM_REGISTRO 256

static const char *const NIVELES[] = { "info ", "warn ", "error" };

static int fd_lectura = -1;
static int fd_escritura = -1;
static pthread_t hilo_registrador;

typedef struct {
    int descriptor;
    FILE *archivo;
} ArgRegistrador;

/* read() puede devolver menos de TAM_REGISTRO por llamada. */
static ssize_t leer_exacto(int descriptor, void *buf, size_t tam)
{
    size_t total = 0;
    while (total < tam) {
        ssize_t leidos = read(descriptor, (char *)buf + total, tam - total);
        if (leidos == 0) return (ssize_t)total;   /* EOF: todos los writers cerraron */
        if (leidos < 0) return -1;
        total += (size_t)leidos;
    }
    return (ssize_t)total;
}

/* Lee registros del pipe y los vuelca al archivo hasta EOF. */
static void *hilo_log(void *argumento)
{
    ArgRegistrador *args = (ArgRegistrador *)argumento;
    char buf_registro[TAM_REGISTRO];

    while (leer_exacto(args->descriptor, buf_registro, TAM_REGISTRO) == TAM_REGISTRO) {
        fputs(buf_registro, args->archivo);
        fflush(args->archivo);
    }

    fflush(args->archivo);
    fclose(args->archivo);
    close(args->descriptor);
    free(args);
    return NULL;
}

int log_iniciar(const char *ruta_archivo)
{
    int extremos_pipe[2];
    if (pipe(extremos_pipe) == -1) {
        perror("pipe");
        return -1;
    }
    fd_lectura = extremos_pipe[0];   /* extremo de lectura del registrador */
    fd_escritura = extremos_pipe[1]; /* extremo de escritura de los workers */

    FILE *archivo = fopen(ruta_archivo, "a");
    if (!archivo) {
        perror("fopen log");
        close(extremos_pipe[0]);
        close(extremos_pipe[1]);
        fd_lectura = fd_escritura = -1;
        return -1;
    }

    ArgRegistrador *args = malloc(sizeof(ArgRegistrador));
    if (!args) {
        fclose(archivo);
        close(extremos_pipe[0]);
        close(extremos_pipe[1]);
        fd_lectura = fd_escritura = -1;
        return -1;
    }
    args->descriptor = fd_lectura;
    args->archivo = archivo;

    if (pthread_create(&hilo_registrador, NULL, hilo_log, args) != 0) {
        perror("pthread_create registrador");
        fclose(archivo);
        close(extremos_pipe[0]);
        close(extremos_pipe[1]);
        free(args);
        fd_lectura = fd_escritura = -1;
        return -1;
    }
    return 0;
}

void log_evento(int nivel, const char *texto)
{
    if (fd_escritura < 0) return;
    if (nivel < LOG_INFO || nivel > LOG_ERROR) nivel = LOG_INFO;

    /* Timestamp thread-safe (localtime_r es reentrant). */
    time_t ahora = time(NULL);
    struct tm tiempo_local;
    localtime_r(&ahora, &tiempo_local);

    char buf_registro[TAM_REGISTRO];
    snprintf(buf_registro, TAM_REGISTRO,
             "%04d-%02d-%02d %02d:%02d:%02d  %s  %s\n",
             tiempo_local.tm_year + 1900, tiempo_local.tm_mon + 1, tiempo_local.tm_mday,
             tiempo_local.tm_hour, tiempo_local.tm_min, tiempo_local.tm_sec,
             NIVELES[nivel], texto);

    write(fd_escritura, buf_registro, TAM_REGISTRO);
}

void log_cerrar(void)
{
    if (fd_escritura < 0) return;

    close(fd_escritura);           /* señala EOF al hilo registrador */
    fd_escritura = -1;

    pthread_join(hilo_registrador, NULL); /* espera a que el registrador vacíe el pipe */
    fd_lectura = -1;
}
