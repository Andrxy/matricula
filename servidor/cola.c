#include "cola.h"

#include <fcntl.h>
#include <string.h>
#include <stdio.h>

mqd_t cola_crear(void)
{
    /* Eliminar cola huérfana de ejecución anterior (se ignora ENOENT). */
    mq_unlink(NOMBRE_COLA);

    struct mq_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.mq_flags   = 0;
    attr.mq_maxmsg  = 10;                      /* ≤ /proc/sys/fs/mqueue/msg_max (10) */
    attr.mq_msgsize = (long)sizeof(ItemCola);  /* 524 B < msgsize_max (8192)         */

    mqd_t mqd = mq_open(NOMBRE_COLA, O_CREAT | O_RDWR, 0600, &attr);
    if (mqd == (mqd_t)-1)
        perror("mq_open");
    return mqd;
}

int cola_encolar(mqd_t mqd, const ItemCola *item)
{
    if (mq_send(mqd, (const char *)item, sizeof(ItemCola), 0) < 0) {
        perror("mq_send");
        return -1;
    }
    return 0;
}

int cola_desencolar(mqd_t mqd, ItemCola *item)
{
    /* El buffer debe ser >= mq_msgsize; sizeof(ItemCola) lo garantiza. */
    ssize_t r = mq_receive(mqd, (char *)item, sizeof(ItemCola), NULL);
    if (r < 0) {
        perror("mq_receive");
        return -1;
    }
    return 0;
}

void cola_destruir(mqd_t mqd)
{
    if (mq_close(mqd) < 0)         perror("mq_close");
    if (mq_unlink(NOMBRE_COLA) < 0) perror("mq_unlink");
}
