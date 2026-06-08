#ifndef LOG_H
#define LOG_H

/* Niveles de log */
#define LOG_INFO   0
#define LOG_WARN   1
#define LOG_ERROR  2

/* Abre el archivo de log y arranca el hilo lector del pipe.
   Debe llamarse una vez desde main antes de crear los hilos trabajadores.
   Devuelve 0 en éxito, -1 en error. */
int log_iniciar(const char *ruta_archivo);

/* Escribe un evento al pipe interno.
   Segura para llamarse desde múltiples hilos simultáneamente:
   el registro tiene tamaño fijo ≤ PIPE_BUF → escritura atómica. */
void log_evento(int nivel, const char *texto);

/* Cierra el extremo de escritura del pipe, espera a que el hilo
   registrador vacíe y cierre el archivo, luego retorna. */
void log_cerrar(void);

#endif /* LOG_H */
