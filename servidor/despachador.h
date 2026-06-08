#ifndef DESPACHADOR_H
#define DESPACHADOR_H

#include <pthread.h>
#include <mqueue.h>

/* Arranca el hilo despachador que consume items de la cola desc_cola.
   El hilo corre indefinidamente; el caller puede pthread_detach o pthread_join.
   Devuelve 0 en éxito, -1 en error. */
int despachador_iniciar(mqd_t desc_cola, pthread_t *hilo_resultado);

#endif /* DESPACHADOR_H */
