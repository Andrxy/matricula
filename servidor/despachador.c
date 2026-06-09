#include "despachador.h"
#include "cola.h"
#include "persistencia.h"
#include "matricula.h"
#include "log.h"
#include "../protocolo/protocolo.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>

/* Evita que respuestas concurrentes se entrelacen al escribir al socket. */
static pthread_mutex_t mtx_envio = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    int fd_conexion;
    Mensaje mensaje;
} ArgOperacion;

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

static const char *res_str(uint8_t r) {
    switch(r) {
        case RES_OK:                       return "ok";
        case RES_ERROR:                    return "error";
        case RES_NO_ENCONTRADO:            return "no encontrado";
        case RES_DUPLICADO:                return "duplicado";
        case RES_MATRICULA_SIN_ESTUDIANTE: return "sin estudiante";
        case RES_MATRICULA_SIN_MATERIA:    return "sin materia";
        case RES_MATRICULA_SIN_PROFESOR:   return "sin profesor";
        default:                           return "?";
    }
}

/* send() puede devolver menos de tam bytes por llamada. */
static int enviar_exacto(int descriptor, const void *buf, size_t tam)
{
    size_t total = 0;
    while (total < tam) {
        /* MSG_NOSIGNAL: evita SIGPIPE si el cliente ya cerró el socket */
        ssize_t enviados = send(descriptor, (const char *)buf + total,
                                tam - total, MSG_NOSIGNAL);
        if (enviados <= 0) return -1;
        total += (size_t)enviados;
    }
    return 0;
}

/* Arma y envía la respuesta bajo el mutex de envío. */
static void responder(int fd_conexion, uint8_t entidad, uint8_t operacion,
                      uint8_t cod_resultado, const void *datos, uint32_t tam)
{
    Mensaje respuesta;
    memset(&respuesta, 0, sizeof(respuesta));
    respuesta.entidad = entidad;
    respuesta.operacion = operacion;
    respuesta.resultado = cod_resultado;
    if (datos && tam > 0) {
        memcpy(respuesta.payload, datos, tam);
        respuesta.tam_payload = tam;
    }

    char buf_serial[TAM_BUFFER_MSG];
    msg_serializar(&respuesta, buf_serial);

    pthread_mutex_lock(&mtx_envio);
    if (enviar_exacto(fd_conexion, buf_serial, TAM_BUFFER_MSG) < 0)
        perror("send respuesta");
    pthread_mutex_unlock(&mtx_envio);
}

/* Procesa una operación y envía la respuesta al cliente. */
static void *ejecutar_operacion(void *argumento)
{
    ArgOperacion *args = (ArgOperacion *)argumento;
    int fd_conexion = args->fd_conexion;
    Mensaje *men = &args->mensaje;
    uint8_t resultado = RES_ERROR;

    char buf_log[64];
    snprintf(buf_log, sizeof(buf_log), ">> %s %s",
             op_str(men->operacion), ent_str(men->entidad));
    log_evento(LOG_INFO, buf_log);

    switch (men->entidad) {

        case ENT_ESTUDIANTE: {
            Estudiante est;
            msg_desempacar_estudiante(men, &est);
            switch (men->operacion) {
                case OP_INSERTAR:
                    resultado = (uint8_t)estudiante_insertar(&est);
                    responder(fd_conexion, men->entidad, men->operacion, resultado, NULL, 0);
                    break;
                case OP_BUSCAR:
                    /* est.cedula lleva la clave de búsqueda y se sobreescribe con el resultado. */
                    resultado = (uint8_t)estudiante_buscar(est.cedula, &est);
                    responder(fd_conexion, men->entidad, men->operacion, resultado,
                              (resultado == RES_OK) ? &est : NULL,
                              (resultado == RES_OK) ? (uint32_t)sizeof(Estudiante) : 0);
                    break;
                default:
                    responder(fd_conexion, men->entidad, men->operacion, RES_ERROR, NULL, 0);
            }
            break;
        }

        case ENT_PROFESOR: {
            Profesor prof;
            msg_desempacar_profesor(men, &prof);
            switch (men->operacion) {
                case OP_INSERTAR:
                    resultado = (uint8_t)profesor_insertar(&prof);
                    responder(fd_conexion, men->entidad, men->operacion, resultado, NULL, 0);
                    break;
                case OP_BUSCAR:
                    resultado = (uint8_t)profesor_buscar(prof.cedula, &prof);
                    responder(fd_conexion, men->entidad, men->operacion, resultado,
                              (resultado == RES_OK) ? &prof : NULL,
                              (resultado == RES_OK) ? (uint32_t)sizeof(Profesor) : 0);
                    break;
                default:
                    responder(fd_conexion, men->entidad, men->operacion, RES_ERROR, NULL, 0);
            }
            break;
        }

        case ENT_MATERIA: {
            Materia mat;
            msg_desempacar_materia(men, &mat);
            switch (men->operacion) {
                case OP_INSERTAR:
                    resultado = (uint8_t)materia_insertar(&mat);
                    responder(fd_conexion, men->entidad, men->operacion, resultado, NULL, 0);
                    break;
                case OP_BUSCAR:
                    resultado = (uint8_t)materia_buscar(mat.codigo, &mat);
                    responder(fd_conexion, men->entidad, men->operacion, resultado,
                              (resultado == RES_OK) ? &mat : NULL,
                              (resultado == RES_OK) ? (uint32_t)sizeof(Materia) : 0);
                    break;
                default:
                    responder(fd_conexion, men->entidad, men->operacion, RES_ERROR, NULL, 0);
            }
            break;
        }

        case ENT_MATRICULA: {
            Matricula insc;
            msg_desempacar_matricula(men, &insc);
            switch (men->operacion) {
                case OP_INSERTAR:
                    resultado = (uint8_t)matricula_insertar_validado(&insc);
                    responder(fd_conexion, men->entidad, men->operacion, resultado, NULL, 0);
                    break;
                case OP_BUSCAR:
                    resultado = (uint8_t)matricula_buscar(insc.codigo_matricula, &insc);
                    responder(fd_conexion, men->entidad, men->operacion, resultado,
                              (resultado == RES_OK) ? &insc : NULL,
                              (resultado == RES_OK) ? (uint32_t)sizeof(Matricula) : 0);
                    break;
                default:
                    responder(fd_conexion, men->entidad, men->operacion, RES_ERROR, NULL, 0);
            }
            break;
        }

        default:
            responder(fd_conexion, men->entidad, men->operacion, RES_ERROR, NULL, 0);
            resultado = RES_ERROR;
    }

    snprintf(buf_log, sizeof(buf_log), "<< %s %s  %s",
             op_str(men->operacion), ent_str(men->entidad), res_str(resultado));
    log_evento(resultado == RES_OK ? LOG_INFO : LOG_WARN, buf_log);

    free(args);
    return NULL;
}

/* Extrae peticiones de la cola y lanza un hilo por operación. */
static void *loop_despachador(void *argumento)
{
    mqd_t desc_cola = *(mqd_t *)argumento;
    free(argumento);

    for (;;) {
        ItemCola peticion;

        if (cola_desencolar(desc_cola, &peticion) < 0) {
            if (errno == EINTR) continue;
            log_evento(LOG_ERROR, "error al leer la cola");
            break;   /* EBADF u otro error fatal: no seguir girando */
        }

        ArgOperacion *args = malloc(sizeof(ArgOperacion));
        if (!args) {
            log_evento(LOG_ERROR, "sin memoria, petición descartada");
            continue;
        }
        args->fd_conexion = peticion.fd_conexion;
        args->mensaje = peticion.mensaje;

        pthread_t hilo_op;
        if (pthread_create(&hilo_op, NULL, ejecutar_operacion, args) != 0) {
            log_evento(LOG_ERROR, "no se pudo crear hilo de operación");
            free(args);
            continue;
        }
        pthread_detach(hilo_op);
    }
    return NULL;
}

/* desc_heap evita que el descriptor en la pila del caller se invalide. */
int despachador_iniciar(mqd_t desc_cola, pthread_t *hilo_resultado)
{
    mqd_t *desc_heap = malloc(sizeof(mqd_t));
    if (!desc_heap) return -1;
    *desc_heap = desc_cola;

    if (pthread_create(hilo_resultado, NULL, loop_despachador, desc_heap) != 0) {
        perror("pthread_create despachador");
        free(desc_heap);
        return -1;
    }
    return 0;
}
