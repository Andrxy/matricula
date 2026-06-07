#ifndef PERSISTENCIA_H
#define PERSISTENCIA_H

#include "../common/protocolo.h"

/*
 * Crea el directorio de datos si no existe.
 * Debe llamarse una vez desde main antes de cualquier operación.
 */
void persistencia_init    (void);
void persistencia_destruir(void);

/* Devuelven RES_OK, RES_DUPLICADO o RES_ERROR. */
int estudiante_insertar(const Estudiante *e);
int profesor_insertar  (const Profesor   *p);
int materia_insertar   (const Materia    *m);
int matricula_insertar (const Matricula  *mc);

/* Devuelven RES_OK, RES_NO_ENCONTRADO o RES_ERROR. */
int estudiante_buscar(const char *cedula,     Estudiante *dest);
int profesor_buscar  (const char *cedula,     Profesor   *dest);
int materia_buscar   (const char *codigo,     Materia    *dest);
int matricula_buscar (const char *cedula_est, const char *cod_mat, Matricula *dest);

#endif /* PERSISTENCIA_H */
