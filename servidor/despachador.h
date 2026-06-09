#ifndef DESPACHADOR_H
#define DESPACHADOR_H

#include <pthread.h>
#include <mqueue.h>

/* Arranca el despachador en su propio hilo. Retorna -1 en error. */
int despachador_iniciar(mqd_t desc_cola, pthread_t *hilo_resultado);

#endif /* DESPACHADOR_H */
