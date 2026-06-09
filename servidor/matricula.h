#ifndef MATRICULA_H
#define MATRICULA_H

#include "../protocolo/protocolo.h"

/* Códigos de error específicos de matrícula. */
#define RES_MATRICULA_SIN_ESTUDIANTE  10  /* cédula de estudiante no existe */
#define RES_MATRICULA_SIN_MATERIA     11  /* código de materia no existe    */
#define RES_MATRICULA_SIN_PROFESOR    12  /* cédula de profesor de la materia no existe */

/* Valida referencias y luego inserta. Retorna RES_* o RES_MATRICULA_*. */
int matricula_insertar_validado(const Matricula *insc);

#endif /* MATRICULA_H */
