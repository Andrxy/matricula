#ifndef PERSISTENCIA_H
#define PERSISTENCIA_H

#include "../protocolo/protocolo.h"

/* Crea el directorio de datos. Llamar una vez al inicio. */
void persistencia_init(void);
void persistencia_destruir(void);

/* Devuelven RES_OK, RES_DUPLICADO o RES_ERROR. */
int estudiante_insertar(const Estudiante *est);
int profesor_insertar(const Profesor *prof);
int materia_insertar(const Materia *mat);
int matricula_insertar(const Matricula *insc);

/* Devuelven RES_OK, RES_NO_ENCONTRADO o RES_ERROR. */
int estudiante_buscar(const char *cedula, Estudiante *destino);
int profesor_buscar(const char *cedula, Profesor *destino);
int materia_buscar(const char *codigo, Materia *destino);
int matricula_buscar(const char *codigo_matricula, Matricula *destino);

#endif 
