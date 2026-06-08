#ifndef MATRICULA_H
#define MATRICULA_H

#include "../comun/protocolo.h"

/* Códigos de resultado específicos de validación (extienden los RES_* de protocolo.h).
   Se envían en el campo 'resultado' de la respuesta para que el cliente
   distinga la razón exacta del rechazo. */
#define RES_MATRICULA_SIN_ESTUDIANTE  10  /* cédula de estudiante no existe */
#define RES_MATRICULA_SIN_MATERIA     11  /* código de materia no existe    */
#define RES_MATRICULA_SIN_PROFESOR    12  /* cédula de profesor de la materia no existe */

/* Valida la integridad referencial antes de insertar la matrícula:
 *   1. Estudiante con cedula_estudiante debe existir.
 *   2. Materia con codigo_materia debe existir.
 *   3. Profesor referenciado por esa materia debe existir.
 * Si todo es válido, delega en matricula_insertar() de persistencia.
 * Devuelve: RES_OK, RES_DUPLICADO, RES_ERROR, o uno de los RES_MATRICULA_*. */
int matricula_insertar_validado(const Matricula *insc);

#endif /* MATRICULA_H */
