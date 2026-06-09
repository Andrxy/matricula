#include "menu.h"
#include "../protocolo/protocolo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

/* Códigos de validación de matrícula (deben coincidir con servidor/matricula.h) */
#define RES_MATRICULA_SIN_ESTUDIANTE  10
#define RES_MATRICULA_SIN_MATERIA     11
#define RES_MATRICULA_SIN_PROFESOR    12

/* Descarta el sobrante del buffer de stdin hasta '\n' o EOF. */
static void vaciar_stdin(void)
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF)
        ;
}

/* Lee un campo no vacío. Repite si queda vacío y descarta el exceso. */
static int leer_campo(const char *etiqueta, char *destino, size_t tam_max)
{
    for (;;) {
        printf("  %-16s: ", etiqueta);
        fflush(stdout);
        if (!fgets(destino, (int)tam_max, stdin)) return -1;
        if (!strchr(destino, '\n')) vaciar_stdin();
        destino[strcspn(destino, "\n")] = '\0';
        if (destino[0] != '\0') return 0;
        printf("  (campo requerido, intente de nuevo)\n");
    }
}

/* Lee un entero en [minimo, maximo]. Repite si la entrada es inválida. */
static int leer_opcion(int minimo, int maximo)
{
    char buf_lectura[16];
    for (;;) {
        printf("Opcion [%d-%d]: ", minimo, maximo);
        fflush(stdout);
        if (!fgets(buf_lectura, sizeof(buf_lectura), stdin)) return -1;
        if (!strchr(buf_lectura, '\n')) vaciar_stdin();
        buf_lectura[strcspn(buf_lectura, "\n")] = '\0';
        char *fin;
        long valor = strtol(buf_lectura, &fin, 10);
        if (fin != buf_lectura && *fin == '\0' && valor >= minimo && valor <= maximo)
            return (int)valor;
        printf("  Ingrese un numero entre %d y %d.\n", minimo, maximo);
    }
}

/* send() puede devolver menos de TAM_BUFFER_MSG por llamada. */
static int enviar_msg(int fd_socket, const Mensaje *men)
{
    char buf_serial[TAM_BUFFER_MSG];
    msg_serializar(men, buf_serial);
    size_t total = 0;
    while (total < TAM_BUFFER_MSG) {
        ssize_t enviados = send(fd_socket, buf_serial + total,
                                TAM_BUFFER_MSG - total, MSG_NOSIGNAL);
        if (enviados <= 0) return -1;
        total += (size_t)enviados;
    }
    return 0;
}

/* recv() puede devolver menos de TAM_BUFFER_MSG por llamada. */
static int recibir_msg(int fd_socket, Mensaje *men)
{
    char buf_serial[TAM_BUFFER_MSG];
    size_t total = 0;
    while (total < TAM_BUFFER_MSG) {
        ssize_t recibidos = recv(fd_socket, buf_serial + total,
                                 TAM_BUFFER_MSG - total, 0);
        if (recibidos <= 0) return -1;
        total += (size_t)recibidos;
    }
    return msg_deserializar(buf_serial, men);
}

/* Envía y recibe. Retorna -1 si falla la comunicación. */
static int transaccion(int fd_socket, const Mensaje *men, Mensaje *respuesta)
{
    if (enviar_msg(fd_socket, men) < 0 || recibir_msg(fd_socket, respuesta) < 0) {
        printf("\n  [ERROR] Fallo de comunicacion con el servidor.\n");
        return -1;
    }
    return 0;
}

/* Submenú para elegir el grado académico. */
static int leer_grado_academico(char *destino, size_t tam_max)
{
    static const char *grados[] = {"Licenciatura", "Maestria", "Doctorado"};
    printf("  Grado Academico:\n");
    printf("    1. Licenciatura\n    2. Maestria\n    3. Doctorado\n");
    int opcion = leer_opcion(1, 3);
    if (opcion < 0) return -1;
    strncpy(destino, grados[opcion - 1], tam_max - 1);
    destino[tam_max - 1] = '\0';
    return 0;
}

static void mostrar_estudiante(const Estudiante *est)
{
    printf("  Cedula    : %s\n", est->cedula);
    printf("  Nombre    : %s\n", est->nombre);
    printf("  Direccion : %s\n", est->direccion);
    printf("  Telefono  : %s\n", est->telefono);
}

static void mostrar_profesor(const Profesor *prof)
{
    printf("  Cedula    : %s\n", prof->cedula);
    printf("  Nombre    : %s\n", prof->nombre);
    printf("  Direccion : %s\n", prof->direccion);
    printf("  Telefono  : %s\n", prof->telefono);
    printf("  Grado     : %s\n", prof->grado_academico);
}

static void mostrar_materia(const Materia *mat)
{
    printf("  Codigo      : %s\n", mat->codigo);
    printf("  Descripcion : %s\n", mat->descripcion);
}

static void mostrar_matricula(const Matricula *insc)
{
    printf("  Cod. Matricula : %s\n", insc->codigo_matricula);
    printf("  Estudiante     : %s\n", insc->cedula_estudiante);
    printf("  Profesor       : %s\n", insc->cedula_profesor);
    printf("  Grupo          : %s\n", insc->grupo);
    printf("  NRC            : %s\n", insc->nrc);
    printf("  Materia        : %s\n", insc->codigo_materia);
    printf("  Horario        : %s\n", insc->horario);
}

/* Lee los datos e inserta el estudiante. */
static int op_ingresar_estudiante(int fd_socket)
{
    printf("\n--- Ingresar Estudiante ---\n");
    Estudiante est;
    memset(&est, 0, sizeof(est));
    if (leer_campo("Cedula", est.cedula, TAM_CEDULA) < 0) return -1;
    if (leer_campo("Nombre", est.nombre, TAM_NOMBRE) < 0) return -1;
    if (leer_campo("Direccion", est.direccion, TAM_DIRECCION) < 0) return -1;
    if (leer_campo("Telefono", est.telefono, TAM_TELEFONO) < 0) return -1;

    Mensaje men, respuesta;
    memset(&men, 0, sizeof(men));
    men.entidad = ENT_ESTUDIANTE;
    men.operacion = OP_INSERTAR;
    msg_empacar_estudiante(&men, &est);
    if (transaccion(fd_socket, &men, &respuesta) < 0) return -1;

    switch (respuesta.resultado) {
        case RES_OK: printf("  [OK] Estudiante registrado.\n"); break;
        case RES_DUPLICADO: printf("  [!] Ya existe un estudiante con esa cedula.\n"); break;
        default: printf("  [ERROR] Respuesta inesperada (cod=%u).\n", respuesta.resultado);
    }
    return 0;
}

/* Busca un estudiante por cédula. */
static int op_buscar_estudiante(int fd_socket)
{
    printf("\n--- Buscar Estudiante ---\n");
    Estudiante est;
    memset(&est, 0, sizeof(est));
    if (leer_campo("Cedula", est.cedula, TAM_CEDULA) < 0) return -1;

    Mensaje men, respuesta;
    memset(&men, 0, sizeof(men));
    men.entidad = ENT_ESTUDIANTE;
    men.operacion = OP_BUSCAR;
    msg_empacar_estudiante(&men, &est);
    if (transaccion(fd_socket, &men, &respuesta) < 0) return -1;

    if (respuesta.resultado == RES_OK) {
        msg_desempacar_estudiante(&respuesta, &est);
        printf("\n  -- Estudiante encontrado --\n");
        mostrar_estudiante(&est);
    } else {
        printf("  [!] Estudiante no encontrado.\n");
    }
    return 0;
}

/* Lee los datos e inserta el profesor. */
static int op_ingresar_profesor(int fd_socket)
{
    printf("\n--- Ingresar Profesor ---\n");
    Profesor prof;
    memset(&prof, 0, sizeof(prof));
    if (leer_campo("Cedula", prof.cedula, TAM_CEDULA) < 0) return -1;
    if (leer_campo("Nombre", prof.nombre, TAM_NOMBRE) < 0) return -1;
    if (leer_campo("Direccion", prof.direccion, TAM_DIRECCION) < 0) return -1;
    if (leer_campo("Telefono", prof.telefono, TAM_TELEFONO) < 0) return -1;
    if (leer_grado_academico(prof.grado_academico, TAM_GRADO_ACADEMICO) < 0) return -1;

    Mensaje men, respuesta;
    memset(&men, 0, sizeof(men));
    men.entidad = ENT_PROFESOR;
    men.operacion = OP_INSERTAR;
    msg_empacar_profesor(&men, &prof);
    if (transaccion(fd_socket, &men, &respuesta) < 0) return -1;

    switch (respuesta.resultado) {
        case RES_OK: printf("  [OK] Profesor registrado.\n"); break;
        case RES_DUPLICADO: printf("  [!] Ya existe un profesor con esa cedula.\n"); break;
        default: printf("  [ERROR] Respuesta inesperada (cod=%u).\n", respuesta.resultado);
    }
    return 0;
}

/* Busca un profesor por cédula. */
static int op_buscar_profesor(int fd_socket)
{
    printf("\n--- Buscar Profesor ---\n");
    Profesor prof;
    memset(&prof, 0, sizeof(prof));
    if (leer_campo("Cedula", prof.cedula, TAM_CEDULA) < 0) return -1;

    Mensaje men, respuesta;
    memset(&men, 0, sizeof(men));
    men.entidad = ENT_PROFESOR;
    men.operacion = OP_BUSCAR;
    msg_empacar_profesor(&men, &prof);
    if (transaccion(fd_socket, &men, &respuesta) < 0) return -1;

    if (respuesta.resultado == RES_OK) {
        msg_desempacar_profesor(&respuesta, &prof);
        printf("\n  -- Profesor encontrado --\n");
        mostrar_profesor(&prof);
    } else {
        printf("  [!] Profesor no encontrado.\n");
    }
    return 0;
}

/* Lee los datos e inserta la materia. */
static int op_ingresar_materia(int fd_socket)
{
    printf("\n--- Ingresar Materia ---\n");
    Materia mat;
    memset(&mat, 0, sizeof(mat));
    if (leer_campo("Codigo", mat.codigo, TAM_CODIGO) < 0) return -1;
    if (leer_campo("Descripcion", mat.descripcion, TAM_DESCRIPCION) < 0) return -1;

    Mensaje men, respuesta;
    memset(&men, 0, sizeof(men));
    men.entidad = ENT_MATERIA;
    men.operacion = OP_INSERTAR;
    msg_empacar_materia(&men, &mat);
    if (transaccion(fd_socket, &men, &respuesta) < 0) return -1;

    switch (respuesta.resultado) {
        case RES_OK: printf("  [OK] Materia registrada.\n"); break;
        case RES_DUPLICADO: printf("  [!] Ya existe una materia con ese codigo.\n"); break;
        default: printf("  [ERROR] Respuesta inesperada (cod=%u).\n", respuesta.resultado);
    }
    return 0;
}

/* Busca una materia por código. */
static int op_buscar_materia(int fd_socket)
{
    printf("\n--- Buscar Materia ---\n");
    Materia mat;
    memset(&mat, 0, sizeof(mat));
    if (leer_campo("Codigo", mat.codigo, TAM_CODIGO) < 0) return -1;

    Mensaje men, respuesta;
    memset(&men, 0, sizeof(men));
    men.entidad = ENT_MATERIA;
    men.operacion = OP_BUSCAR;
    msg_empacar_materia(&men, &mat);
    if (transaccion(fd_socket, &men, &respuesta) < 0) return -1;

    if (respuesta.resultado == RES_OK) {
        msg_desempacar_materia(&respuesta, &mat);
        printf("\n  -- Materia encontrada --\n");
        mostrar_materia(&mat);
    } else {
        printf("  [!] Materia no encontrada.\n");
    }
    return 0;
}

/* Lee los datos e inserta la matrícula. Informa qué referencia falta si falla. */
static int op_ingresar_matricula(int fd_socket)
{
    printf("\n--- Ingresar Matricula ---\n");
    Matricula insc;
    memset(&insc, 0, sizeof(insc));
    if (leer_campo("Cod. Matricula", insc.codigo_matricula, TAM_CODIGO) < 0) return -1;
    if (leer_campo("Ced. Estudiante", insc.cedula_estudiante, TAM_CEDULA) < 0) return -1;
    if (leer_campo("Ced. Profesor", insc.cedula_profesor, TAM_CEDULA) < 0) return -1;
    if (leer_campo("Grupo", insc.grupo, TAM_GRUPO) < 0) return -1;
    if (leer_campo("NRC", insc.nrc, TAM_NRC) < 0) return -1;
    if (leer_campo("Cod. Materia", insc.codigo_materia, TAM_CODIGO) < 0) return -1;
    if (leer_campo("Horario", insc.horario, TAM_HORARIO) < 0) return -1;

    Mensaje men, respuesta;
    memset(&men, 0, sizeof(men));
    men.entidad = ENT_MATRICULA;
    men.operacion = OP_INSERTAR;
    msg_empacar_matricula(&men, &insc);
    if (transaccion(fd_socket, &men, &respuesta) < 0) return -1;

    switch (respuesta.resultado) {
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
            printf("  [ERROR] Respuesta inesperada (cod=%u).\n", respuesta.resultado);
    }
    return 0;
}

/* Busca una matrícula por código. */
static int op_buscar_matricula(int fd_socket)
{
    printf("\n--- Buscar Matricula ---\n");
    Matricula insc;
    memset(&insc, 0, sizeof(insc));
    if (leer_campo("Cod. Matricula", insc.codigo_matricula, TAM_CODIGO) < 0) return -1;

    Mensaje men, respuesta;
    memset(&men, 0, sizeof(men));
    men.entidad = ENT_MATRICULA;
    men.operacion = OP_BUSCAR;
    msg_empacar_matricula(&men, &insc);
    if (transaccion(fd_socket, &men, &respuesta) < 0) return -1;

    if (respuesta.resultado == RES_OK) {
        msg_desempacar_matricula(&respuesta, &insc);
        printf("\n  -- Matricula encontrada --\n");
        mostrar_matricula(&insc);
    } else {
        printf("  [!] Matricula no encontrada.\n");
    }
    return 0;
}

/* Submenú de Estudiantes. */
static int submenu_estudiante(int fd_socket)
{
    for (;;) {
        printf("\n=== Estudiantes ===\n");
        printf("  1. Ingresar\n  2. Buscar\n  0. Regresar\n");
        int opcion = leer_opcion(0, 2);
        if (opcion < 0) return -1;
        if (opcion == 0) return 0;
        int resultado = (opcion == 1) ? op_ingresar_estudiante(fd_socket)
                                      : op_buscar_estudiante(fd_socket);
        if (resultado < 0) return -1;
    }
}

/* Submenú de Profesores. */
static int submenu_profesor(int fd_socket)
{
    for (;;) {
        printf("\n=== Profesores ===\n");
        printf("  1. Ingresar\n  2. Buscar\n  0. Regresar\n");
        int opcion = leer_opcion(0, 2);
        if (opcion < 0) return -1;
        if (opcion == 0) return 0;
        int resultado = (opcion == 1) ? op_ingresar_profesor(fd_socket)
                                      : op_buscar_profesor(fd_socket);
        if (resultado < 0) return -1;
    }
}

/* Submenú de Materias. */
static int submenu_materia(int fd_socket)
{
    for (;;) {
        printf("\n=== Materias ===\n");
        printf("  1. Ingresar\n  2. Buscar\n  0. Regresar\n");
        int opcion = leer_opcion(0, 2);
        if (opcion < 0) return -1;
        if (opcion == 0) return 0;
        int resultado = (opcion == 1) ? op_ingresar_materia(fd_socket)
                                      : op_buscar_materia(fd_socket);
        if (resultado < 0) return -1;
    }
}

/* Submenú de Matrículas. */
static int submenu_matricula(int fd_socket)
{
    for (;;) {
        printf("\n=== Matriculas ===\n");
        printf("  1. Ingresar\n  2. Buscar\n  0. Regresar\n");
        int opcion = leer_opcion(0, 2);
        if (opcion < 0) return -1;
        if (opcion == 0) return 0;
        int resultado = (opcion == 1) ? op_ingresar_matricula(fd_socket)
                                      : op_buscar_matricula(fd_socket);
        if (resultado < 0) return -1;
    }
}

/* Menú principal. Retorna al salir o si se pierde la conexión. */
int menu_principal(int fd_socket)
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

        int opcion = leer_opcion(0, 4);
        if (opcion < 0 || opcion == 0) return 0;

        int resultado;
        switch (opcion) {
            case 1: resultado = submenu_estudiante(fd_socket); break;
            case 2: resultado = submenu_profesor(fd_socket); break;
            case 3: resultado = submenu_materia(fd_socket); break;
            case 4: resultado = submenu_matricula(fd_socket); break;
            default: resultado = 0;
        }
        if (resultado < 0) return -1;
    }
}
