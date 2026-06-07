#include "despachador.h"
#include "cola.h"
#include "persistencia.h"
#include "matricula.h"
#include "log.h"
#include "../common/protocolo.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>

/* ── Mutex de envío (patrón guia-mutex: PTHREAD_MUTEX_INITIALIZER) ──────
   Serializa los send() de todos los hilos de operación para evitar que
   respuestas concurrentes al mismo socket se entrelacen en el wire.     */
static pthread_mutex_t mtx_envio = PTHREAD_MUTEX_INITIALIZER;

/* ── Argumento del hilo de operación ───────────────────────────────────── */

typedef struct {
    int     connfd;
    Mensaje msg;
} ArgOperacion;

/* ── Envío robusto ─────────────────────────────────────────────────────── */

/* send() puede escribir menos bytes de los solicitados; iteramos. */
static int enviar_exacto(int fd, const void *buf, size_t n)
{
    size_t total = 0;
    while (total < n) {
        /* MSG_NOSIGNAL: evita SIGPIPE si el cliente ya cerró el socket */
        ssize_t s = send(fd, (const char *)buf + total, n - total, MSG_NOSIGNAL);
        if (s <= 0) return -1;
        total += (size_t)s;
    }
    return 0;
}

/* ── Construcción y envío de respuesta ─────────────────────────────────── */

static void responder(int connfd, uint8_t entidad, uint8_t operacion,
                      uint8_t resultado, const void *datos, uint32_t tam)
{
    Mensaje resp;
    memset(&resp, 0, sizeof(resp));
    resp.entidad    = entidad;
    resp.operacion  = operacion;
    resp.resultado  = resultado;
    if (datos && tam > 0) {
        memcpy(resp.payload, datos, tam);
        resp.tam_payload = tam;
    }

    char buf[TAM_BUFFER_MSG];
    msg_serializar(&resp, buf);

    /* Sección crítica: un solo hilo escribe al socket a la vez.
       Patrón guia-mutex: lock → sección crítica → unlock.          */
    pthread_mutex_lock(&mtx_envio);
    if (enviar_exacto(connfd, buf, TAM_BUFFER_MSG) < 0)
        perror("send respuesta");
    pthread_mutex_unlock(&mtx_envio);
}

/* ── Hilo de operación ─────────────────────────────────────────────────── */

/*
 * Cada mensaje de la cola se despacha a un hilo independiente que:
 *   1. Identifica entidad + operación.
 *   2. Llama a la función de persistencia correspondiente.
 *      (La exclusión mutua sobre los archivos la proveen los mutexes
 *       por entidad declarados en persistencia.c.)
 *   3. Envía la respuesta al cliente vía el socket connfd.
 */
static void *ejecutar_operacion(void *arg)
{
    ArgOperacion *a      = (ArgOperacion *)arg;
    int           connfd = a->connfd;
    Mensaje      *msg    = &a->msg;
    uint8_t       res = RES_ERROR;

    char lbuf[128];
    snprintf(lbuf, sizeof(lbuf),
             "inicio op fd=%d entidad=%u op=%u",
             connfd, msg->entidad, msg->operacion);
    log_evento(LOG_INFO, lbuf);

    switch (msg->entidad) {

        case ENT_ESTUDIANTE: {
            Estudiante e;
            msg_desempacar_estudiante(msg, &e);
            switch (msg->operacion) {
                case OP_INSERTAR:
                    res = (uint8_t)estudiante_insertar(&e);
                    responder(connfd, msg->entidad, msg->operacion, res, NULL, 0);
                    break;
                case OP_BUSCAR:
                    /* El cliente envía la cédula en e.cedula; se sobreescribe con el resultado. */
                    res = (uint8_t)estudiante_buscar(e.cedula, &e);
                    responder(connfd, msg->entidad, msg->operacion, res,
                              (res == RES_OK) ? &e    : NULL,
                              (res == RES_OK) ? (uint32_t)sizeof(Estudiante) : 0);
                    break;
                default:
                    responder(connfd, msg->entidad, msg->operacion, RES_ERROR, NULL, 0);
            }
            break;
        }

        case ENT_PROFESOR: {
            Profesor p;
            msg_desempacar_profesor(msg, &p);
            switch (msg->operacion) {
                case OP_INSERTAR:
                    res = (uint8_t)profesor_insertar(&p);
                    responder(connfd, msg->entidad, msg->operacion, res, NULL, 0);
                    break;
                case OP_BUSCAR:
                    res = (uint8_t)profesor_buscar(p.cedula, &p);
                    responder(connfd, msg->entidad, msg->operacion, res,
                              (res == RES_OK) ? &p    : NULL,
                              (res == RES_OK) ? (uint32_t)sizeof(Profesor) : 0);
                    break;
                default:
                    responder(connfd, msg->entidad, msg->operacion, RES_ERROR, NULL, 0);
            }
            break;
        }

        case ENT_MATERIA: {
            Materia m;
            msg_desempacar_materia(msg, &m);
            switch (msg->operacion) {
                case OP_INSERTAR:
                    res = (uint8_t)materia_insertar(&m);
                    responder(connfd, msg->entidad, msg->operacion, res, NULL, 0);
                    break;
                case OP_BUSCAR:
                    res = (uint8_t)materia_buscar(m.codigo, &m);
                    responder(connfd, msg->entidad, msg->operacion, res,
                              (res == RES_OK) ? &m    : NULL,
                              (res == RES_OK) ? (uint32_t)sizeof(Materia) : 0);
                    break;
                default:
                    responder(connfd, msg->entidad, msg->operacion, RES_ERROR, NULL, 0);
            }
            break;
        }

        case ENT_MATRICULA: {
            Matricula mc;
            msg_desempacar_matricula(msg, &mc);
            switch (msg->operacion) {
                case OP_INSERTAR:
                    res = (uint8_t)matricula_insertar_validado(&mc);
                    responder(connfd, msg->entidad, msg->operacion, res, NULL, 0);
                    break;
                case OP_BUSCAR:
                    /* Clave compuesta: cedula_estudiante + codigo_materia. */
                    res = (uint8_t)matricula_buscar(mc.cedula_estudiante,
                                                   mc.codigo_materia, &mc);
                    responder(connfd, msg->entidad, msg->operacion, res,
                              (res == RES_OK) ? &mc   : NULL,
                              (res == RES_OK) ? (uint32_t)sizeof(Matricula) : 0);
                    break;
                default:
                    responder(connfd, msg->entidad, msg->operacion, RES_ERROR, NULL, 0);
            }
            break;
        }

        default:
            responder(connfd, msg->entidad, msg->operacion, RES_ERROR, NULL, 0);
            res = RES_ERROR;
    }

    snprintf(lbuf, sizeof(lbuf),
             "fin op fd=%d entidad=%u op=%u resultado=%u",
             connfd, msg->entidad, msg->operacion, res);
    log_evento(res == RES_OK ? LOG_INFO : LOG_WARN, lbuf);

    free(a);
    return NULL;
}

/* ── Loop del despachador ──────────────────────────────────────────────── */

static void *loop_despachador(void *arg)
{
    mqd_t qid = *(mqd_t *)arg;
    free(arg);

    char lbuf[64];
    snprintf(lbuf, sizeof(lbuf), "despachador listo mqd=%d", (int)qid);
    log_evento(LOG_INFO, lbuf);

    for (;;) {
        ItemCola item;

        if (cola_desencolar(qid, &item) < 0) {
            if (errno == EINTR) continue;
            log_evento(LOG_ERROR, "cola_desencolar falló");
            continue;
        }

        ArgOperacion *a = malloc(sizeof(ArgOperacion));
        if (!a) {
            log_evento(LOG_ERROR, "malloc falló en despachador, petición descartada");
            continue;
        }
        a->connfd = item.connfd;
        a->msg    = item.msg;

        pthread_t hilo;
        if (pthread_create(&hilo, NULL, ejecutar_operacion, a) != 0) {
            log_evento(LOG_ERROR, "pthread_create falló para hilo de operación");
            free(a);
            continue;
        }
        /* El hilo libera su propio ArgOperacion; se limpia solo al terminar. */
        pthread_detach(hilo);
    }
    return NULL;
}

/* ── Punto de entrada público ──────────────────────────────────────────── */

int despachador_iniciar(mqd_t qid, pthread_t *hilo_out)
{
    mqd_t *qid_heap = malloc(sizeof(mqd_t));
    if (!qid_heap) return -1;
    *qid_heap = qid;

    if (pthread_create(hilo_out, NULL, loop_despachador, qid_heap) != 0) {
        perror("pthread_create despachador");
        free(qid_heap);
        return -1;
    }
    return 0;
}
