#ifndef COLA_H
#define COLA_H

#include <mqueue.h>
#include "../common/protocolo.h"

/* Nombre de la cola POSIX: debe comenzar con '/'. */
#define NOMBRE_COLA "/matricula_srv"

/* Unidad que viaja por la cola: petición + descriptor del socket origen.
   El despachador necesita connfd para enviar la respuesta al cliente. */
typedef struct {
    int     connfd;
    Mensaje msg;
} ItemCola;

mqd_t cola_crear     (void);
int   cola_encolar   (mqd_t mqd, const ItemCola *item);
int   cola_desencolar(mqd_t mqd, ItemCola *item);
void  cola_destruir  (mqd_t mqd);

#endif /* COLA_H */
