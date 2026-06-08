#ifndef COLA_H
#define COLA_H

#include <mqueue.h>
#include "../comun/protocolo.h"

/* Nombre de la cola POSIX: debe comenzar con '/'. */
#define NOMBRE_COLA "/matricula_srv"

/* Unidad que viaja por la cola: petición + descriptor del socket origen.
   El despachador necesita fd_conexion para enviar la respuesta al cliente. */
typedef struct {
    int fd_conexion;
    Mensaje mensaje;
} ItemCola;

mqd_t cola_crear(void);
int cola_encolar(mqd_t desc_cola, const ItemCola *elemento);
int cola_desencolar(mqd_t desc_cola, ItemCola *elemento);
void cola_destruir(mqd_t desc_cola);

#endif /* COLA_H */
