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
static int       fd_lectura  = -1;
static int       fd_escritura = -1;
static pthread_t hilo_logger;

/* ── Argumento del hilo logger ─────────────────────────────────────────── */
typedef struct {
    int   fd;
    FILE *archivo;
} ArgLogger;

/* ── Lectura robusta del pipe ──────────────────────────────────────────── */
/* read() puede devolver menos bytes de los pedidos aunque el writer
   escribió exactamente TAM_REGISTRO; iteramos hasta completar el registro. */
static ssize_t leer_exacto(int fd, void *buf, size_t n)
{
    size_t total = 0;
    while (total < n) {
        ssize_t r = read(fd, (char *)buf + total, n - total);
        if (r == 0) return (ssize_t)total;   /* EOF: todos los writers cerraron */
        if (r < 0)  return -1;
        total += (size_t)r;
    }
    return (ssize_t)total;
}

/* ── Hilo logger ───────────────────────────────────────────────────────── */
/* Lee registros del extremo de lectura del pipe y los escribe al archivo.
   Termina cuando todos los descriptores del extremo de escritura están
   cerrados (read devuelve 0 antes de completar un registro).              */
static void *hilo_log(void *arg)
{
    ArgLogger *a = (ArgLogger *)arg;
    char buf[TAM_REGISTRO];

    while (leer_exacto(a->fd, buf, TAM_REGISTRO) == TAM_REGISTRO) {
        /* buf es un string null-terminado con '\n' antes del '\0';
           fputs escribe hasta el '\0', volcando solo el texto formateado. */
        fputs(buf, a->archivo);
        fflush(a->archivo);
    }

    fflush(a->archivo);
    fclose(a->archivo);
    close(a->fd);
    free(a);
    return NULL;
}

/* ── API pública ───────────────────────────────────────────────────────── */

int log_iniciar(const char *archivo)
{
    int fds[2];
    if (pipe(fds) == -1) {
        perror("pipe");
        return -1;
    }
    /* fds[0] = extremo de lectura (logger)
       fds[1] = extremo de escritura (workers) */
    fd_lectura   = fds[0];
    fd_escritura = fds[1];

    FILE *f = fopen(archivo, "a");
    if (!f) {
        perror("fopen log");
        close(fds[0]);
        close(fds[1]);
        fd_lectura = fd_escritura = -1;
        return -1;
    }

    ArgLogger *a = malloc(sizeof(ArgLogger));
    if (!a) {
        fclose(f);
        close(fds[0]);
        close(fds[1]);
        fd_lectura = fd_escritura = -1;
        return -1;
    }
    a->fd      = fd_lectura;
    a->archivo = f;

    if (pthread_create(&hilo_logger, NULL, hilo_log, a) != 0) {
        perror("pthread_create logger");
        fclose(f);
        close(fds[0]);
        close(fds[1]);
        free(a);
        fd_lectura = fd_escritura = -1;
        return -1;
    }
    return 0;
}

void log_evento(int nivel, const char *msg)
{
    if (fd_escritura < 0) return;
    if (nivel < LOG_INFO || nivel > LOG_ERROR) nivel = LOG_INFO;

    /* Timestamp thread-safe (localtime_r es reentrant). */
    time_t     ahora = time(NULL);
    struct tm  tm_l;
    localtime_r(&ahora, &tm_l);

    char buf[TAM_REGISTRO];
    snprintf(buf, TAM_REGISTRO,
             "[%04d-%02d-%02d %02d:%02d:%02d] [%s] %s\n",
             tm_l.tm_year + 1900, tm_l.tm_mon + 1, tm_l.tm_mday,
             tm_l.tm_hour,        tm_l.tm_min,      tm_l.tm_sec,
             NIVELES[nivel], msg);

    /* Escritura atómica: TAM_REGISTRO (256) ≤ PIPE_BUF (4096).
       Múltiples hilos pueden llamar write() concurrentemente sin mutex
       y sus registros no se entremezclan en el pipe.                   */
    write(fd_escritura, buf, TAM_REGISTRO);
}

void log_cerrar(void)
{
    if (fd_escritura < 0) return;

    /* Cerrar el extremo de escritura señala EOF al hilo logger. */
    close(fd_escritura);
    fd_escritura = -1;

    /* Esperar a que el logger vacíe el pipe y cierre el archivo. */
    pthread_join(hilo_logger, NULL);
    fd_lectura = -1;
}
