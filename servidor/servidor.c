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

#include "../common/protocolo.h"
#include "cola.h"
#include "persistencia.h"
#include "despachador.h"
#include "log.h"

#define PUERTO      5001
#define BACKLOG     16
#define ARCHIVO_LOG "servidor.log"

/* ── Argumento del hilo por conexión ───────────────────────────────────── */

typedef struct {
    int   connfd;
    mqd_t qid;
} ArgHilo;

/* ── Lectura robusta ────────────────────────────────────────────────────── */

static ssize_t recibir_exacto(int fd, void *buf, size_t n)
{
    size_t total = 0;
    while (total < n) {
        ssize_t r = recv(fd, (char *)buf + total, n - total, 0);
        if (r == 0)  return 0;
        if (r < 0)   return -1;
        total += (size_t)r;
    }
    return (ssize_t)total;
}

/* ── Hilo por conexión ──────────────────────────────────────────────────── */

static void *atender_cliente(void *arg)
{
    ArgHilo *a = (ArgHilo *)arg;
    int   connfd = a->connfd;
    mqd_t qid    = a->qid;
    free(a);

    char ip_cliente[INET_ADDRSTRLEN] = "desconocida";
    struct sockaddr_in peer;
    socklen_t peer_len = sizeof(peer);
    if (getpeername(connfd, (struct sockaddr *)&peer, &peer_len) == 0)
        inet_ntop(AF_INET, &peer.sin_addr, ip_cliente, sizeof(ip_cliente));

    char lbuf[128];
    snprintf(lbuf, sizeof(lbuf), "conexión fd=%d ip=%s", connfd, ip_cliente);
    log_evento(LOG_INFO, lbuf);

    char buf[TAM_BUFFER_MSG];
    Mensaje msg;

    for (;;) {
        ssize_t r = recibir_exacto(connfd, buf, TAM_BUFFER_MSG);
        if (r == 0) {
            snprintf(lbuf, sizeof(lbuf), "desconexión fd=%d ip=%s", connfd, ip_cliente);
            log_evento(LOG_INFO, lbuf);
            break;
        }
        if (r < 0) {
            snprintf(lbuf, sizeof(lbuf), "recv error fd=%d ip=%s", connfd, ip_cliente);
            log_evento(LOG_ERROR, lbuf);
            break;
        }

        if (msg_deserializar(buf, &msg) < 0) {
            snprintf(lbuf, sizeof(lbuf), "msg_deserializar falló fd=%d", connfd);
            log_evento(LOG_ERROR, lbuf);
            break;
        }

        ItemCola item;
        item.connfd = connfd;
        item.msg    = msg;
        if (cola_encolar(qid, &item) < 0) {
            snprintf(lbuf, sizeof(lbuf), "cola_encolar falló fd=%d", connfd);
            log_evento(LOG_ERROR, lbuf);
            break;
        }

        snprintf(lbuf, sizeof(lbuf),
                 "petición encolada fd=%d entidad=%u op=%u",
                 connfd, msg.entidad, msg.operacion);
        log_evento(LOG_INFO, lbuf);
    }

    close(connfd);
    return NULL;
}

/* ── Main ───────────────────────────────────────────────────────────────── */

int main(void)
{
    if (log_iniciar(ARCHIVO_LOG) < 0) exit(1);

    persistencia_init();
    log_evento(LOG_INFO, "persistencia inicializada");

    mqd_t qid = cola_crear();
    if (qid == (mqd_t)-1) {
        log_evento(LOG_ERROR, "no se pudo crear la cola de mensajes");
        persistencia_destruir();
        log_cerrar();
        exit(1);
    }

    char lbuf[64];
    snprintf(lbuf, sizeof(lbuf), "cola de mensajes POSIX creada mqd=%d", (int)qid);
    log_evento(LOG_INFO, lbuf);

    pthread_t hilo_desp;
    if (despachador_iniciar(qid, &hilo_desp) < 0) {
        log_evento(LOG_ERROR, "no se pudo iniciar el despachador");
        cola_destruir(qid);
        persistencia_destruir();
        log_cerrar();
        exit(1);
    }
    pthread_detach(hilo_desp);
    log_evento(LOG_INFO, "despachador iniciado");

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        log_evento(LOG_ERROR, "socket() falló");
        cola_destruir(qid);
        persistencia_destruir();
        log_cerrar();
        exit(1);
    }

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(PUERTO);

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        log_evento(LOG_ERROR, "bind() falló");
        cola_destruir(qid); persistencia_destruir(); log_cerrar(); exit(1);
    }
    if (listen(sockfd, BACKLOG) == -1) {
        log_evento(LOG_ERROR, "listen() falló");
        cola_destruir(qid); persistencia_destruir(); log_cerrar(); exit(1);
    }

    snprintf(lbuf, sizeof(lbuf), "escuchando en puerto %d", PUERTO);
    log_evento(LOG_INFO, lbuf);
    printf("[servidor] escuchando en puerto %d — log: %s\n", PUERTO, ARCHIVO_LOG);
    fflush(stdout);

    for (;;) {
        struct sockaddr_in cliente_addr;
        socklen_t cliente_len = sizeof(cliente_addr);

        int connfd = accept(sockfd, (struct sockaddr *)&cliente_addr,
                            &cliente_len);
        if (connfd == -1) {
            if (errno == EINTR) continue;
            log_evento(LOG_ERROR, "accept() falló");
            continue;
        }

        ArgHilo *a = malloc(sizeof(ArgHilo));
        if (!a) { close(connfd); continue; }
        a->connfd = connfd;
        a->qid    = qid;

        pthread_t hilo;
        if (pthread_create(&hilo, NULL, atender_cliente, a) != 0) {
            log_evento(LOG_ERROR, "pthread_create falló para hilo de conexión");
            free(a);
            close(connfd);
            continue;
        }
        pthread_detach(hilo);
    }

    close(sockfd);
    cola_destruir(qid);
    persistencia_destruir();
    log_cerrar();
    return 0;
}
