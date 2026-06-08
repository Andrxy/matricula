#include "log.h"

#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── Tamaño del registro ───────────────────────────────────────────────────
   Debe ser ≤ PIPE_BUF (4096 en Linux) para que write() sea atómica y los
   registros de distintos hilos no se entrelacen en el pipe.               */
#define TAM_REGISTRO 256

static const char *const NIVELES[] = { "INFO ", "WARN ", "ERROR" };

/* ── Estado del módulo ─────────────────────────────────────────────────── */
static int fd_lectura = -1;
static int fd_escritura = -1;
static pthread_t hilo_registrador;

/* ── Argumento del hilo registrador ────────────────────────────────────── */
typedef struct {
    int descriptor;
    FILE *archivo;
} ArgRegistrador;

/* ── Lectura robusta del pipe ──────────────────────────────────────────── */
/* read() puede devolver menos bytes de los pedidos aunque el writer
   escribió exactamente TAM_REGISTRO; iteramos hasta completar el registro. */
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

/* ── Hilo registrador ──────────────────────────────────────────────────── */
/* Lee registros del extremo de lectura del pipe y los escribe al archivo.
   Termina cuando todos los descriptores del extremo de escritura están
   cerrados (read devuelve 0 antes de completar un registro).              */
static void *hilo_log(void *argumento)
{
    ArgRegistrador *args = (ArgRegistrador *)argumento;
    char buf_registro[TAM_REGISTRO];

    while (leer_exacto(args->descriptor, buf_registro, TAM_REGISTRO) == TAM_REGISTRO) {
        /* buf_registro es un string null-terminado con '\n' antes del '\0';
           fputs escribe hasta el '\0', volcando solo el texto formateado. */
        fputs(buf_registro, args->archivo);
        fflush(args->archivo);
    }

    fflush(args->archivo);
    fclose(args->archivo);
    close(args->descriptor);
    free(args);
    return NULL;
}

/* ── API pública ───────────────────────────────────────────────────────── */

int log_iniciar(const char *ruta_archivo)
{
    int extremos_pipe[2];
    if (pipe(extremos_pipe) == -1) {
        perror("pipe");
        return -1;
    }
    /* extremos_pipe[0] = extremo de lectura (registrador)
       extremos_pipe[1] = extremo de escritura (workers) */
    fd_lectura = extremos_pipe[0];
    fd_escritura = extremos_pipe[1];

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
             "[%04d-%02d-%02d %02d:%02d:%02d] [%s] %s\n",
             tiempo_local.tm_year + 1900, tiempo_local.tm_mon + 1, tiempo_local.tm_mday,
             tiempo_local.tm_hour, tiempo_local.tm_min, tiempo_local.tm_sec,
             NIVELES[nivel], texto);

    /* Escritura atómica: TAM_REGISTRO (256) ≤ PIPE_BUF (4096).
       Múltiples hilos pueden llamar write() concurrentemente sin mutex
       y sus registros no se entremezclan en el pipe.                   */
    write(fd_escritura, buf_registro, TAM_REGISTRO);
}

void log_cerrar(void)
{
    if (fd_escritura < 0) return;

    /* Cerrar el extremo de escritura señala EOF al hilo registrador. */
    close(fd_escritura);
    fd_escritura = -1;

    /* Esperar a que el registrador vacíe el pipe y cierre el archivo. */
    pthread_join(hilo_registrador, NULL);
    fd_lectura = -1;
}
