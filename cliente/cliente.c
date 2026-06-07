#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "menu.h"

#define IP_DEF    "127.0.0.1"
#define PUERTO_DEF 5001

int main(int argc, char *argv[])
{
    const char *ip   = (argc > 1) ? argv[1] : IP_DEF;
    int         port = (argc > 2) ? atoi(argv[2]) : PUERTO_DEF;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        fprintf(stderr, "Direccion IP invalida: %s\n", ip);
        close(sockfd);
        return 1;
    }

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("connect");
        close(sockfd);
        return 1;
    }

    printf("Conectado a %s:%d\n", ip, port);

    int resultado = menu_principal(sockfd);

    close(sockfd);
    printf("\nConexion cerrada. Hasta luego.\n");
    return (resultado < 0) ? 1 : 0;
}
