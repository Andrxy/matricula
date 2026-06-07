#ifndef MENU_H
#define MENU_H

/*
 * Muestra el menú principal y gestiona toda la interacción con el usuario.
 * sockfd : descriptor del socket ya conectado al servidor.
 * Retorna 0 al elegir "Salir", -1 ante error de comunicación o EOF en stdin.
 */
int menu_principal(int sockfd);

#endif /* MENU_H */
