#include "cola.h"

#include <fcntl.h>
#include <string.h>
#include <stdio.h>

mqd_t cola_crear(void)
{
    /* Limpia cola huérfana de corridas anteriores. */
    mq_unlink(NOMBRE_COLA);

    struct mq_attr atributos;
    memset(&atributos, 0, sizeof(atributos));
    atributos.mq_flags = 0;
    atributos.mq_maxmsg = 10;                       /* ≤ /proc/sys/fs/mqueue/msg_max (10) */
    atributos.mq_msgsize = (long)sizeof(ItemCola);  /* 524 B < msgsize_max (8192)         */

    mqd_t desc_cola = mq_open(NOMBRE_COLA, O_CREAT | O_RDWR, 0600, &atributos);
    if (desc_cola == (mqd_t)-1)
        perror("mq_open");
    return desc_cola;
}

int cola_encolar(mqd_t desc_cola, const ItemCola *elemento)
{
    if (mq_send(desc_cola, (const char *)elemento, sizeof(ItemCola), 0) < 0) {
        perror("mq_send");
        return -1;
    }
    return 0;
}

int cola_desencolar(mqd_t desc_cola, ItemCola *elemento)
{
    /* sizeof(ItemCola) satisface el mínimo de mq_msgsize. */
    ssize_t leidos = mq_receive(desc_cola, (char *)elemento, sizeof(ItemCola), NULL);
    if (leidos < 0) {
        perror("mq_receive");
        return -1;
    }
    return 0;
}

void cola_destruir(mqd_t desc_cola)
{
    if (mq_close(desc_cola) < 0) perror("mq_close");
    if (mq_unlink(NOMBRE_COLA) < 0) perror("mq_unlink");
}
