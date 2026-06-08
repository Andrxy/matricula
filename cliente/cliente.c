#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "menu.h"

#define IP_DEF     "127.0.0.1"
#define PUERTO_DEF 5001

int main(int argc, char *argv[])
{
    const char *ip_servidor = (argc > 1) ? argv[1] : IP_DEF;
    int puerto = (argc > 2) ? atoi(argv[2]) : PUERTO_DEF;

    int fd_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_socket == -1) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in direccion;
    memset(&direccion, 0, sizeof(direccion));
    direccion.sin_family = AF_INET;
    direccion.sin_port = htons((uint16_t)puerto);
    if (inet_pton(AF_INET, ip_servidor, &direccion.sin_addr) != 1) {
        fprintf(stderr, "Direccion IP invalida: %s\n", ip_servidor);
        close(fd_socket);
        return 1;
    }

    if (connect(fd_socket, (struct sockaddr *)&direccion, sizeof(direccion)) == -1) {
        perror("connect");
        close(fd_socket);
        return 1;
    }

    printf("Conectado a %s:%d\n", ip_servidor, puerto);

    int resultado = menu_principal(fd_socket);

    close(fd_socket);
    printf("\nConexion cerrada. Hasta luego.\n");
    return (resultado < 0) ? 1 : 0;
}
