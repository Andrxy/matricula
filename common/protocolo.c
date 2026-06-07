#include "protocolo.h"

#include <string.h>
#include <stdint.h>

/* ── Serialización ─────────────────────────────────────────────────────── */

/*
 * Formato del buffer plano (little-endian, sin padding implícito):
 *   [0]      entidad      (uint8)
 *   [1]      operacion    (uint8)
 *   [2]      resultado    (uint8)
 *   [3]      _pad         (uint8, siempre 0)
 *   [4..7]   tam_payload  (uint32, big-endian para portabilidad en red)
 *   [8..8+TAM_PAYLOAD-1]  payload completo
 */

int msg_serializar(const Mensaje *msg, char buf[TAM_BUFFER_MSG])
{
    if (!msg || !buf)
        return -1;

    buf[0] = (char)msg->entidad;
    buf[1] = (char)msg->operacion;
    buf[2] = (char)msg->resultado;
    buf[3] = 0;

    /* tam_payload en big-endian */
    uint32_t tp = msg->tam_payload;
    buf[4] = (char)((tp >> 24) & 0xFF);
    buf[5] = (char)((tp >> 16) & 0xFF);
    buf[6] = (char)((tp >>  8) & 0xFF);
    buf[7] = (char)( tp        & 0xFF);

    memcpy(buf + TAM_CABECERA_MSG, msg->payload, TAM_PAYLOAD);
    return 0;
}

int msg_deserializar(const char buf[TAM_BUFFER_MSG], Mensaje *msg)
{
    if (!buf || !msg)
        return -1;

    msg->entidad    = (uint8_t)buf[0];
    msg->operacion  = (uint8_t)buf[1];
    msg->resultado  = (uint8_t)buf[2];
    msg->_pad       = 0;

    msg->tam_payload =
        ((uint32_t)(uint8_t)buf[4] << 24) |
        ((uint32_t)(uint8_t)buf[5] << 16) |
        ((uint32_t)(uint8_t)buf[6] <<  8) |
        ((uint32_t)(uint8_t)buf[7]);

    memcpy(msg->payload, buf + TAM_CABECERA_MSG, TAM_PAYLOAD);
    return 0;
}

/* ── Helpers de payload ─────────────────────────────────────────────────── */

void msg_empacar_estudiante(Mensaje *msg, const Estudiante *e)
{
    memcpy(msg->payload, e, sizeof(Estudiante));
    msg->tam_payload = (uint32_t)sizeof(Estudiante);
}

void msg_empacar_profesor(Mensaje *msg, const Profesor *p)
{
    memcpy(msg->payload, p, sizeof(Profesor));
    msg->tam_payload = (uint32_t)sizeof(Profesor);
}

void msg_empacar_materia(Mensaje *msg, const Materia *m)
{
    memcpy(msg->payload, m, sizeof(Materia));
    msg->tam_payload = (uint32_t)sizeof(Materia);
}

void msg_empacar_matricula(Mensaje *msg, const Matricula *mc)
{
    memcpy(msg->payload, mc, sizeof(Matricula));
    msg->tam_payload = (uint32_t)sizeof(Matricula);
}

void msg_desempacar_estudiante(const Mensaje *msg, Estudiante *e)
{
    memcpy(e, msg->payload, sizeof(Estudiante));
}

void msg_desempacar_profesor(const Mensaje *msg, Profesor *p)
{
    memcpy(p, msg->payload, sizeof(Profesor));
}

void msg_desempacar_materia(const Mensaje *msg, Materia *m)
{
    memcpy(m, msg->payload, sizeof(Materia));
}

void msg_desempacar_matricula(const Mensaje *msg, Matricula *mc)
{
    memcpy(mc, msg->payload, sizeof(Matricula));
}
