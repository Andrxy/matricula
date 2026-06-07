#include "menu.h"
#include "../common/protocolo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

/* Códigos de validación de matrícula (deben coincidir con servidor/matricula.h) */
#define RES_MATRICULA_SIN_ESTUDIANTE  10
#define RES_MATRICULA_SIN_MATERIA     11
#define RES_MATRICULA_SIN_PROFESOR    12

/* ── Helpers de entrada ────────────────────────────────────────────────── */

/* Descarta caracteres sobrantes en stdin hasta '\n' o EOF. */
static void vaciar_stdin(void)
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF)
        ;
}

/*
 * Lee un campo de texto no vacío.
 * Si el usuario escribe más de max-1 chars, descarta el exceso.
 * Retorna 0 en éxito, -1 en EOF (Ctrl+D).
 */
static int leer_campo(const char *etiqueta, char *dest, size_t max)
{
    for (;;) {
        printf("  %-16s: ", etiqueta);
        fflush(stdout);
        if (!fgets(dest, (int)max, stdin)) return -1;
        if (!strchr(dest, '\n')) vaciar_stdin();
        dest[strcspn(dest, "\n")] = '\0';
        if (dest[0] != '\0') return 0;
        printf("  (campo requerido, intente de nuevo)\n");
    }
}

/* Lee un entero >= 1. Retorna 0 en éxito, -1 en EOF. */
static int leer_entero(const char *etiqueta, int *dest)
{
    char buf[32];
    for (;;) {
        printf("  %-16s: ", etiqueta);
        fflush(stdout);
        if (!fgets(buf, sizeof(buf), stdin)) return -1;
        if (!strchr(buf, '\n')) vaciar_stdin();
        buf[strcspn(buf, "\n")] = '\0';
        char *fin;
        long v = strtol(buf, &fin, 10);
        if (fin != buf && *fin == '\0' && v >= 1) {
            *dest = (int)v;
            return 0;
        }
        printf("  (ingrese un numero entero mayor que cero)\n");
    }
}

/* Lee una opción entera en [min, max]. Retorna el valor o -1 en EOF. */
static int leer_opcion(int min, int max)
{
    char buf[16];
    for (;;) {
        printf("Opcion [%d-%d]: ", min, max);
        fflush(stdout);
        if (!fgets(buf, sizeof(buf), stdin)) return -1;
        if (!strchr(buf, '\n')) vaciar_stdin();
        buf[strcspn(buf, "\n")] = '\0';
        char *fin;
        long v = strtol(buf, &fin, 10);
        if (fin != buf && *fin == '\0' && v >= min && v <= max)
            return (int)v;
        printf("  Ingrese un numero entre %d y %d.\n", min, max);
    }
}

/* ── Comunicación con el servidor ──────────────────────────────────────── */

/* send() puede escribir menos bytes de los solicitados; iteramos. */
static int enviar_msg(int sockfd, const Mensaje *msg)
{
    char buf[TAM_BUFFER_MSG];
    msg_serializar(msg, buf);
    size_t total = 0;
    while (total < TAM_BUFFER_MSG) {
        ssize_t s = send(sockfd, buf + total, TAM_BUFFER_MSG - total, MSG_NOSIGNAL);
        if (s <= 0) return -1;
        total += (size_t)s;
    }
    return 0;
}

/* recv() puede devolver menos bytes de los solicitados; iteramos. */
static int recibir_msg(int sockfd, Mensaje *msg)
{
    char buf[TAM_BUFFER_MSG];
    size_t total = 0;
    while (total < TAM_BUFFER_MSG) {
        ssize_t r = recv(sockfd, buf + total, TAM_BUFFER_MSG - total, 0);
        if (r <= 0) return -1;
        total += (size_t)r;
    }
    return msg_deserializar(buf, msg);
}

/* Envía msg y recibe la respuesta en resp. Imprime error y retorna -1 si falla. */
static int transaccion(int sockfd, const Mensaje *msg, Mensaje *resp)
{
    if (enviar_msg(sockfd, msg) < 0 || recibir_msg(sockfd, resp) < 0) {
        printf("\n  [ERROR] Fallo de comunicacion con el servidor.\n");
        return -1;
    }
    return 0;
}

/* ── Visualización de entidades ────────────────────────────────────────── */

static void mostrar_estudiante(const Estudiante *e)
{
    printf("  Cedula   : %s\n", e->cedula);
    printf("  Nombre   : %s %s\n", e->nombre, e->apellido);
    printf("  Email    : %s\n", e->email);
}

static void mostrar_profesor(const Profesor *p)
{
    printf("  Cedula   : %s\n", p->cedula);
    printf("  Nombre   : %s %s\n", p->nombre, p->apellido);
    printf("  Depto    : %s\n", p->departamento);
    printf("  Email    : %s\n", p->email);
}

static void mostrar_materia(const Materia *m)
{
    printf("  Codigo      : %s\n", m->codigo);
    printf("  Nombre      : %s\n", m->nombre);
    printf("  Descripcion : %s\n", m->descripcion);
    printf("  Creditos    : %d\n", m->creditos);
    printf("  Profesor    : %s\n", m->cedula_profesor);
}

static void mostrar_matricula(const Matricula *mc)
{
    printf("  Estudiante  : %s\n", mc->cedula_estudiante);
    printf("  Materia     : %s\n", mc->codigo_materia);
    printf("  Periodo     : %s\n", mc->periodo);
}

/* ── Operaciones: Estudiante ───────────────────────────────────────────── */

static int op_ingresar_estudiante(int sockfd)
{
    printf("\n--- Ingresar Estudiante ---\n");
    Estudiante e;
    memset(&e, 0, sizeof(e));
    if (leer_campo("Cedula",   e.cedula,   TAM_CEDULA)   < 0) return -1;
    if (leer_campo("Nombre",   e.nombre,   TAM_NOMBRE)   < 0) return -1;
    if (leer_campo("Apellido", e.apellido, TAM_APELLIDO) < 0) return -1;
    if (leer_campo("Email",    e.email,    TAM_EMAIL)    < 0) return -1;

    Mensaje msg, resp;
    memset(&msg, 0, sizeof(msg));
    msg.entidad   = ENT_ESTUDIANTE;
    msg.operacion = OP_INSERTAR;
    msg_empacar_estudiante(&msg, &e);
    if (transaccion(sockfd, &msg, &resp) < 0) return -1;

    switch (resp.resultado) {
        case RES_OK:        printf("  [OK] Estudiante registrado.\n");                     break;
        case RES_DUPLICADO: printf("  [!] Ya existe un estudiante con esa cedula.\n");     break;
        default:            printf("  [ERROR] Respuesta inesperada (cod=%u).\n", resp.resultado);
    }
    return 0;
}

static int op_buscar_estudiante(int sockfd)
{
    printf("\n--- Buscar Estudiante ---\n");
    Estudiante e;
    memset(&e, 0, sizeof(e));
    if (leer_campo("Cedula", e.cedula, TAM_CEDULA) < 0) return -1;

    Mensaje msg, resp;
    memset(&msg, 0, sizeof(msg));
    msg.entidad   = ENT_ESTUDIANTE;
    msg.operacion = OP_BUSCAR;
    msg_empacar_estudiante(&msg, &e);
    if (transaccion(sockfd, &msg, &resp) < 0) return -1;

    if (resp.resultado == RES_OK) {
        msg_desempacar_estudiante(&resp, &e);
        printf("\n  -- Estudiante encontrado --\n");
        mostrar_estudiante(&e);
    } else {
        printf("  [!] Estudiante no encontrado.\n");
    }
    return 0;
}

/* ── Operaciones: Profesor ─────────────────────────────────────────────── */

static int op_ingresar_profesor(int sockfd)
{
    printf("\n--- Ingresar Profesor ---\n");
    Profesor p;
    memset(&p, 0, sizeof(p));
    if (leer_campo("Cedula",       p.cedula,       TAM_CEDULA)   < 0) return -1;
    if (leer_campo("Nombre",       p.nombre,       TAM_NOMBRE)   < 0) return -1;
    if (leer_campo("Apellido",     p.apellido,     TAM_APELLIDO) < 0) return -1;
    if (leer_campo("Departamento", p.departamento, TAM_DEPTO)    < 0) return -1;
    if (leer_campo("Email",        p.email,        TAM_EMAIL)    < 0) return -1;

    Mensaje msg, resp;
    memset(&msg, 0, sizeof(msg));
    msg.entidad   = ENT_PROFESOR;
    msg.operacion = OP_INSERTAR;
    msg_empacar_profesor(&msg, &p);
    if (transaccion(sockfd, &msg, &resp) < 0) return -1;

    switch (resp.resultado) {
        case RES_OK:        printf("  [OK] Profesor registrado.\n");                   break;
        case RES_DUPLICADO: printf("  [!] Ya existe un profesor con esa cedula.\n");  break;
        default:            printf("  [ERROR] Respuesta inesperada (cod=%u).\n", resp.resultado);
    }
    return 0;
}

static int op_buscar_profesor(int sockfd)
{
    printf("\n--- Buscar Profesor ---\n");
    Profesor p;
    memset(&p, 0, sizeof(p));
    if (leer_campo("Cedula", p.cedula, TAM_CEDULA) < 0) return -1;

    Mensaje msg, resp;
    memset(&msg, 0, sizeof(msg));
    msg.entidad   = ENT_PROFESOR;
    msg.operacion = OP_BUSCAR;
    msg_empacar_profesor(&msg, &p);
    if (transaccion(sockfd, &msg, &resp) < 0) return -1;

    if (resp.resultado == RES_OK) {
        msg_desempacar_profesor(&resp, &p);
        printf("\n  -- Profesor encontrado --\n");
        mostrar_profesor(&p);
    } else {
        printf("  [!] Profesor no encontrado.\n");
    }
    return 0;
}

/* ── Operaciones: Materia ──────────────────────────────────────────────── */

static int op_ingresar_materia(int sockfd)
{
    printf("\n--- Ingresar Materia ---\n");
    Materia m;
    memset(&m, 0, sizeof(m));
    if (leer_campo("Codigo",        m.codigo,          TAM_CODIGO)      < 0) return -1;
    if (leer_campo("Nombre",        m.nombre,          TAM_NOMBRE)      < 0) return -1;
    if (leer_campo("Descripcion",   m.descripcion,     TAM_DESCRIPCION) < 0) return -1;
    if (leer_entero("Creditos",     &m.creditos)                        < 0) return -1;
    if (leer_campo("Ced. Profesor", m.cedula_profesor, TAM_CEDULA)      < 0) return -1;

    Mensaje msg, resp;
    memset(&msg, 0, sizeof(msg));
    msg.entidad   = ENT_MATERIA;
    msg.operacion = OP_INSERTAR;
    msg_empacar_materia(&msg, &m);
    if (transaccion(sockfd, &msg, &resp) < 0) return -1;

    switch (resp.resultado) {
        case RES_OK:        printf("  [OK] Materia registrada.\n");                    break;
        case RES_DUPLICADO: printf("  [!] Ya existe una materia con ese codigo.\n");   break;
        default:            printf("  [ERROR] Respuesta inesperada (cod=%u).\n", resp.resultado);
    }
    return 0;
}

static int op_buscar_materia(int sockfd)
{
    printf("\n--- Buscar Materia ---\n");
    Materia m;
    memset(&m, 0, sizeof(m));
    if (leer_campo("Codigo", m.codigo, TAM_CODIGO) < 0) return -1;

    Mensaje msg, resp;
    memset(&msg, 0, sizeof(msg));
    msg.entidad   = ENT_MATERIA;
    msg.operacion = OP_BUSCAR;
    msg_empacar_materia(&msg, &m);
    if (transaccion(sockfd, &msg, &resp) < 0) return -1;

    if (resp.resultado == RES_OK) {
        msg_desempacar_materia(&resp, &m);
        printf("\n  -- Materia encontrada --\n");
        mostrar_materia(&m);
    } else {
        printf("  [!] Materia no encontrada.\n");
    }
    return 0;
}

/* ── Operaciones: Matricula ────────────────────────────────────────────── */

static int op_ingresar_matricula(int sockfd)
{
    printf("\n--- Ingresar Matricula ---\n");
    Matricula mc;
    memset(&mc, 0, sizeof(mc));
    if (leer_campo("Ced. Estudiante", mc.cedula_estudiante, TAM_CEDULA)        < 0) return -1;
    if (leer_campo("Cod. Materia",    mc.codigo_materia,    TAM_CODIGO)        < 0) return -1;
    if (leer_campo("Periodo",         mc.periodo,           sizeof(mc.periodo)) < 0) return -1;

    Mensaje msg, resp;
    memset(&msg, 0, sizeof(msg));
    msg.entidad   = ENT_MATRICULA;
    msg.operacion = OP_INSERTAR;
    msg_empacar_matricula(&msg, &mc);
    if (transaccion(sockfd, &msg, &resp) < 0) return -1;

    switch (resp.resultado) {
        case RES_OK:
            printf("  [OK] Matricula registrada.\n");
            break;
        case RES_DUPLICADO:
            printf("  [!] El estudiante ya esta matriculado en esa materia/periodo.\n");
            break;
        case RES_MATRICULA_SIN_ESTUDIANTE:
            printf("  [!] No existe un estudiante con esa cedula.\n");
            break;
        case RES_MATRICULA_SIN_MATERIA:
            printf("  [!] No existe una materia con ese codigo.\n");
            break;
        case RES_MATRICULA_SIN_PROFESOR:
            printf("  [!] La materia no tiene un profesor registrado valido.\n");
            break;
        default:
            printf("  [ERROR] Respuesta inesperada (cod=%u).\n", resp.resultado);
    }
    return 0;
}

static int op_buscar_matricula(int sockfd)
{
    printf("\n--- Buscar Matricula ---\n");
    Matricula mc;
    memset(&mc, 0, sizeof(mc));
    if (leer_campo("Ced. Estudiante", mc.cedula_estudiante, TAM_CEDULA) < 0) return -1;
    if (leer_campo("Cod. Materia",    mc.codigo_materia,    TAM_CODIGO) < 0) return -1;

    Mensaje msg, resp;
    memset(&msg, 0, sizeof(msg));
    msg.entidad   = ENT_MATRICULA;
    msg.operacion = OP_BUSCAR;
    msg_empacar_matricula(&msg, &mc);
    if (transaccion(sockfd, &msg, &resp) < 0) return -1;

    if (resp.resultado == RES_OK) {
        msg_desempacar_matricula(&resp, &mc);
        printf("\n  -- Matricula encontrada --\n");
        mostrar_matricula(&mc);
    } else {
        printf("  [!] Matricula no encontrada.\n");
    }
    return 0;
}

/* ── Submenús por entidad ──────────────────────────────────────────────── */

static int submenu_estudiante(int sockfd)
{
    for (;;) {
        printf("\n=== Estudiantes ===\n");
        printf("  1. Ingresar\n  2. Buscar\n  0. Regresar\n");
        int op = leer_opcion(0, 2);
        if (op < 0) return -1;
        if (op == 0) return 0;
        int r = (op == 1) ? op_ingresar_estudiante(sockfd)
                          : op_buscar_estudiante(sockfd);
        if (r < 0) return -1;
    }
}

static int submenu_profesor(int sockfd)
{
    for (;;) {
        printf("\n=== Profesores ===\n");
        printf("  1. Ingresar\n  2. Buscar\n  0. Regresar\n");
        int op = leer_opcion(0, 2);
        if (op < 0) return -1;
        if (op == 0) return 0;
        int r = (op == 1) ? op_ingresar_profesor(sockfd)
                          : op_buscar_profesor(sockfd);
        if (r < 0) return -1;
    }
}

static int submenu_materia(int sockfd)
{
    for (;;) {
        printf("\n=== Materias ===\n");
        printf("  1. Ingresar\n  2. Buscar\n  0. Regresar\n");
        int op = leer_opcion(0, 2);
        if (op < 0) return -1;
        if (op == 0) return 0;
        int r = (op == 1) ? op_ingresar_materia(sockfd)
                          : op_buscar_materia(sockfd);
        if (r < 0) return -1;
    }
}

static int submenu_matricula(int sockfd)
{
    for (;;) {
        printf("\n=== Matriculas ===\n");
        printf("  1. Ingresar\n  2. Buscar\n  0. Regresar\n");
        int op = leer_opcion(0, 2);
        if (op < 0) return -1;
        if (op == 0) return 0;
        int r = (op == 1) ? op_ingresar_matricula(sockfd)
                          : op_buscar_matricula(sockfd);
        if (r < 0) return -1;
    }
}

/* ── Menú principal ────────────────────────────────────────────────────── */

int menu_principal(int sockfd)
{
    printf("\n========================================\n");
    printf("   Sistema de Matricula Universitaria   \n");
    printf("========================================\n");

    for (;;) {
        printf("\n  1. Estudiantes\n");
        printf("  2. Profesores\n");
        printf("  3. Materias\n");
        printf("  4. Matriculas\n");
        printf("  0. Salir\n\n");

        int op = leer_opcion(0, 4);
        if (op < 0 || op == 0) return 0;

        int r;
        switch (op) {
            case 1: r = submenu_estudiante(sockfd); break;
            case 2: r = submenu_profesor(sockfd);   break;
            case 3: r = submenu_materia(sockfd);    break;
            case 4: r = submenu_matricula(sockfd);  break;
            default: r = 0;
        }
        if (r < 0) return -1;
    }
}
