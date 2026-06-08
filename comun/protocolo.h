#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include <stdint.h>
#include <stddef.h>

/* ── Tamaños de campos ─────────────────────────────────────────────────── */
#define TAM_NOMBRE      64
#define TAM_APELLIDO    64
#define TAM_CEDULA      16
#define TAM_CODIGO      16
#define TAM_DESCRIPCION 128
#define TAM_EMAIL       64
#define TAM_DEPTO       64
#define TAM_PAYLOAD     512

/* ── Códigos de entidad ────────────────────────────────────────────────── */
#define ENT_ESTUDIANTE  1
#define ENT_PROFESOR    2
#define ENT_MATERIA     3
#define ENT_MATRICULA   4

/* ── Códigos de operación ──────────────────────────────────────────────── */
#define OP_INSERTAR     1
#define OP_BUSCAR       2
#define OP_LISTAR       3
#define OP_ELIMINAR     4

/* ── Códigos de resultado ──────────────────────────────────────────────── */
#define RES_OK          0
#define RES_ERROR       1
#define RES_NO_ENCONTRADO 2
#define RES_DUPLICADO   3

/* ── Structs de entidades ──────────────────────────────────────────────── */

typedef struct {
    char cedula[TAM_CEDULA];
    char nombre[TAM_NOMBRE];
    char apellido[TAM_APELLIDO];
    char email[TAM_EMAIL];
} Estudiante;

typedef struct {
    char cedula[TAM_CEDULA];
    char nombre[TAM_NOMBRE];
    char apellido[TAM_APELLIDO];
    char departamento[TAM_DEPTO];
    char email[TAM_EMAIL];
} Profesor;

typedef struct {
    char codigo[TAM_CODIGO];
    char nombre[TAM_NOMBRE];
    char descripcion[TAM_DESCRIPCION];
    int creditos;
    char cedula_profesor[TAM_CEDULA];
} Materia;

typedef struct {
    char cedula_estudiante[TAM_CEDULA];
    char codigo_materia[TAM_CODIGO];
    /* periodo: ej. "2025-1" */
    char periodo[16];
} Matricula;

/* ── Mensaje genérico ──────────────────────────────────────────────────── */

typedef struct {
    uint8_t entidad;            /* ENT_*  */
    uint8_t operacion;          /* OP_*   */
    uint8_t resultado;          /* RES_*  (solo en respuestas) */
    uint8_t _pad;               /* alineación explícita */
    uint32_t tam_payload;       /* bytes válidos en payload */
    char payload[TAM_PAYLOAD];
} Mensaje;

/* Tamaño del encabezado serializado (campos fijos antes del payload) */
#define TAM_CABECERA_MSG  8      /* 4 × uint8 + uint32 */

/* Tamaño total del buffer plano: cabecera + payload completo */
#define TAM_BUFFER_MSG    (TAM_CABECERA_MSG + TAM_PAYLOAD)

/* ── Serialización / deserialización ───────────────────────────────────── */

/* Serializa men en buf (TAM_BUFFER_MSG bytes). Devuelve 0 en éxito, -1 si NULL. */
int msg_serializar(const Mensaje *men, char buf[TAM_BUFFER_MSG]);

/* Deserializa buf en men. Devuelve 0 en éxito, -1 si algún puntero es NULL. */
int msg_deserializar(const char buf[TAM_BUFFER_MSG], Mensaje *men);

/* ── Helpers de payload ─────────────────────────────────────────────────── */

/* Copia la entidad en men->payload y ajusta tam_payload. */
void msg_empacar_estudiante(Mensaje *men, const Estudiante *est);
void msg_empacar_profesor(Mensaje *men, const Profesor *prof);
void msg_empacar_materia(Mensaje *men, const Materia *mat);
void msg_empacar_matricula(Mensaje *men, const Matricula *insc);

/* Extrae la entidad desde men->payload. */
void msg_desempacar_estudiante(const Mensaje *men, Estudiante *est);
void msg_desempacar_profesor(const Mensaje *men, Profesor *prof);
void msg_desempacar_materia(const Mensaje *men, Materia *mat);
void msg_desempacar_matricula(const Mensaje *men, Matricula *insc);

#endif /* PROTOCOLO_H */
