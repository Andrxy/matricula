#include "protocolo.h"

#include <string.h>
#include <stdint.h>

/* ── Serialización ─────────────────────────────────────────────────────── */

/*
 * Formato del buffer plano (big-endian para tam_payload):
 *   [0]      entidad      (uint8)
 *   [1]      operacion    (uint8)
 *   [2]      resultado    (uint8)
 *   [3]      _pad         (uint8, siempre 0)
 *   [4..7]   tam_payload  (uint32, big-endian para portabilidad en red)
 *   [8..8+TAM_PAYLOAD-1]  payload completo
 */

int msg_serializar(const Mensaje *men, char buf[TAM_BUFFER_MSG])
{
    if (!men || !buf)
        return -1;

    buf[0] = (char)men->entidad;
    buf[1] = (char)men->operacion;
    buf[2] = (char)men->resultado;
    buf[3] = 0;

    /* tam_payload en big-endian */
    uint32_t tam_carga = men->tam_payload;
    buf[4] = (char)((tam_carga >> 24) & 0xFF);
    buf[5] = (char)((tam_carga >> 16) & 0xFF);
    buf[6] = (char)((tam_carga >> 8) & 0xFF);
    buf[7] = (char)(tam_carga & 0xFF);

    memcpy(buf + TAM_CABECERA_MSG, men->payload, TAM_PAYLOAD);
    return 0;
}

int msg_deserializar(const char buf[TAM_BUFFER_MSG], Mensaje *men)
{
    if (!buf || !men)
        return -1;

    men->entidad = (uint8_t)buf[0];
    men->operacion = (uint8_t)buf[1];
    men->resultado = (uint8_t)buf[2];
    men->_pad = 0;

    men->tam_payload =
        ((uint32_t)(uint8_t)buf[4] << 24) |
        ((uint32_t)(uint8_t)buf[5] << 16) |
        ((uint32_t)(uint8_t)buf[6] << 8) |
        ((uint32_t)(uint8_t)buf[7]);

    memcpy(men->payload, buf + TAM_CABECERA_MSG, TAM_PAYLOAD);
    return 0;
}

/* ── Helpers de payload ─────────────────────────────────────────────────── */

void msg_empacar_estudiante(Mensaje *men, const Estudiante *est)
{
    memcpy(men->payload, est, sizeof(Estudiante));
    men->tam_payload = (uint32_t)sizeof(Estudiante);
}

void msg_empacar_profesor(Mensaje *men, const Profesor *prof)
{
    memcpy(men->payload, prof, sizeof(Profesor));
    men->tam_payload = (uint32_t)sizeof(Profesor);
}

void msg_empacar_materia(Mensaje *men, const Materia *mat)
{
    memcpy(men->payload, mat, sizeof(Materia));
    men->tam_payload = (uint32_t)sizeof(Materia);
}

void msg_empacar_matricula(Mensaje *men, const Matricula *insc)
{
    memcpy(men->payload, insc, sizeof(Matricula));
    men->tam_payload = (uint32_t)sizeof(Matricula);
}

void msg_desempacar_estudiante(const Mensaje *men, Estudiante *est)
{
    memcpy(est, men->payload, sizeof(Estudiante));
}

void msg_desempacar_profesor(const Mensaje *men, Profesor *prof)
{
    memcpy(prof, men->payload, sizeof(Profesor));
}

void msg_desempacar_materia(const Mensaje *men, Materia *mat)
{
    memcpy(mat, men->payload, sizeof(Materia));
}

void msg_desempacar_matricula(const Mensaje *men, Matricula *insc)
{
    memcpy(insc, men->payload, sizeof(Matricula));
}
