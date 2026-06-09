#ifndef COLA_H
#define COLA_H

#include <mqueue.h>
#include "../protocolo/protocolo.h"

/* El nombre debe comenzar con '/'. */
#define NOMBRE_COLA "/matricula_srv"

/* fd_conexion es necesario para que el despachador envíe la respuesta al cliente. */
typedef struct {
    int fd_conexion;
    Mensaje mensaje;
} ItemCola;

mqd_t cola_crear(void);
int cola_encolar(mqd_t desc_cola, const ItemCola *elemento);
int cola_desencolar(mqd_t desc_cola, ItemCola *elemento);
void cola_destruir(mqd_t desc_cola);

#endif /* COLA_H */
