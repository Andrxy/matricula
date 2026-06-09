#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include "../protocolo/protocolo.h"
#include "cola.h"
#include "persistencia.h"
#include "despachador.h"
#include "log.h"

#define PUERTO      5001
#define BACKLOG     16
#define ARCHIVO_LOG "servidor.log"

typedef struct {
    int fd_conexion;
    mqd_t desc_cola;
} ArgHilo;

static const char *ent_str(uint8_t e) {
    switch(e) {
        case ENT_ESTUDIANTE: return "estudiante";
        case ENT_PROFESOR:   return "profesor";
        case ENT_MATERIA:    return "materia";
        case ENT_MATRICULA:  return "matricula";
        default:             return "?";
    }
}

static const char *op_str(uint8_t op) {
    switch(op) {
        case OP_INSERTAR: return "insertar";
        case OP_BUSCAR:   return "buscar";
        default:          return "?";
    }
}

/* recv() puede devolver menos de tam bytes por llamada. */
static ssize_t recibir_exacto(int descriptor, void *buf, size_t tam)
{
    size_t total = 0;
    while (total < tam) {
        ssize_t leidos = recv(descriptor, (char *)buf + total, tam - total, 0);
        if (leidos == 0) return 0;
        if (leidos < 0) return -1;
        total += (size_t)leidos;
    }
    return (ssize_t)total;
}

/* Recibe mensajes del cliente y los encola para el despachador. */
static void *atender_cliente(void *argumento)
{
    ArgHilo *args = (ArgHilo *)argumento;
    int fd_conexion = args->fd_conexion;
    mqd_t desc_cola = args->desc_cola;
    free(args);

    char ip_cliente[INET_ADDRSTRLEN] = "desconocida";
    struct sockaddr_in par_remoto;
    socklen_t tam_par = sizeof(par_remoto);
    if (getpeername(fd_conexion, (struct sockaddr *)&par_remoto, &tam_par) == 0)
        inet_ntop(AF_INET, &par_remoto.sin_addr, ip_cliente, sizeof(ip_cliente));

    char buf_log[128];
    snprintf(buf_log, sizeof(buf_log), "cliente %s conectado", ip_cliente);
    log_evento(LOG_INFO, buf_log);

    char buf_msg[TAM_BUFFER_MSG];
    Mensaje men;

    for (;;) {
        ssize_t leidos = recibir_exacto(fd_conexion, buf_msg, TAM_BUFFER_MSG);
        if (leidos == 0) {
            snprintf(buf_log, sizeof(buf_log), "cliente %s desconectado", ip_cliente);
            log_evento(LOG_INFO, buf_log);
            break;
        }
        if (leidos < 0) {
            snprintf(buf_log, sizeof(buf_log), "error recv de %s", ip_cliente);
            log_evento(LOG_ERROR, buf_log);
            break;
        }

        if (msg_deserializar(buf_msg, &men) < 0) {
            snprintf(buf_log, sizeof(buf_log), "mensaje inválido de %s", ip_cliente);
            log_evento(LOG_ERROR, buf_log);
            break;
        }

        ItemCola peticion;
        peticion.fd_conexion = fd_conexion;
        peticion.mensaje = men;
        if (cola_encolar(desc_cola, &peticion) < 0) {
            snprintf(buf_log, sizeof(buf_log), "cola llena, petición de %s descartada", ip_cliente);
            log_evento(LOG_ERROR, buf_log);
            break;
        }

        snprintf(buf_log, sizeof(buf_log), "%s %s de %s",
                 op_str(men.operacion), ent_str(men.entidad), ip_cliente);
        log_evento(LOG_INFO, buf_log);
    }

    close(fd_conexion);
    return NULL;
}

/* Arranca subsistemas y entra al accept loop. */
int main(void)
{
    if (log_iniciar(ARCHIVO_LOG) < 0) exit(1);

    persistencia_init();
    log_evento(LOG_INFO, "datos listos");

    mqd_t desc_cola = cola_crear();
    if (desc_cola == (mqd_t)-1) {
        log_evento(LOG_ERROR, "no se pudo crear la cola");
        persistencia_destruir();
        log_cerrar();
        exit(1);
    }
    log_evento(LOG_INFO, "cola lista");

    pthread_t hilo_despachador;
    if (despachador_iniciar(desc_cola, &hilo_despachador) < 0) {
        log_evento(LOG_ERROR, "no se pudo iniciar el despachador");
        cola_destruir(desc_cola);
        persistencia_destruir();
        log_cerrar();
        exit(1);
    }
    pthread_detach(hilo_despachador);
    log_evento(LOG_INFO, "despachador listo");

    int fd_escucha = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_escucha == -1) {
        log_evento(LOG_ERROR, "socket falló");
        cola_destruir(desc_cola);
        persistencia_destruir();
        log_cerrar();
        exit(1);
    }

    int reusar = 1;
    setsockopt(fd_escucha, SOL_SOCKET, SO_REUSEADDR, &reusar, sizeof(reusar));

    struct sockaddr_in direccion;
    memset(&direccion, 0, sizeof(direccion));
    direccion.sin_family = AF_INET;
    direccion.sin_addr.s_addr = INADDR_ANY;
    direccion.sin_port = htons(PUERTO);

    if (bind(fd_escucha, (struct sockaddr *)&direccion, sizeof(direccion)) == -1) {
        perror("bind");
        log_evento(LOG_ERROR, "bind falló, quizá hay otro servidor en este puerto");
        cola_destruir(desc_cola); persistencia_destruir(); log_cerrar(); exit(1);
    }
    if (listen(fd_escucha, BACKLOG) == -1) {
        log_evento(LOG_ERROR, "listen falló");
        cola_destruir(desc_cola); persistencia_destruir(); log_cerrar(); exit(1);
    }

    char buf_log[64];
    snprintf(buf_log, sizeof(buf_log), "escucha en :%d", PUERTO);
    log_evento(LOG_INFO, buf_log);
    printf("[servidor] escuchando en puerto %d — log: %s\n", PUERTO, ARCHIVO_LOG);
    fflush(stdout);

    for (;;) {
        struct sockaddr_in dir_cliente;
        socklen_t tam_dir_cliente = sizeof(dir_cliente);

        int fd_conexion = accept(fd_escucha, (struct sockaddr *)&dir_cliente,
                                 &tam_dir_cliente);
        if (fd_conexion == -1) {
            if (errno == EINTR) continue;
            log_evento(LOG_ERROR, "accept falló");
            continue;
        }

        ArgHilo *args = malloc(sizeof(ArgHilo));
        if (!args) { close(fd_conexion); continue; }
        args->fd_conexion = fd_conexion;
        args->desc_cola = desc_cola;

        pthread_t hilo_cliente;
        if (pthread_create(&hilo_cliente, NULL, atender_cliente, args) != 0) {
            log_evento(LOG_ERROR, "no se pudo crear hilo de conexión");
            free(args);
            close(fd_conexion);
            continue;
        }
        pthread_detach(hilo_cliente);
    }

    close(fd_escucha);
    cola_destruir(desc_cola);
    persistencia_destruir();
    log_cerrar();
    return 0;
}
