#ifndef LOG_H
#define LOG_H

#define LOG_INFO   0
#define LOG_WARN   1
#define LOG_ERROR  2

/* Abre el log y arranca el hilo registrador. Llamar antes de crear hilos. */
int log_iniciar(const char *ruta_archivo);

/* Escribe un evento al pipe. Segura para múltiples hilos (registro menor a PIPE_BUF). */
void log_evento(int nivel, const char *texto);

/* Señala EOF al registrador y espera a que vacíe el archivo. */
void log_cerrar(void);

#endif /* LOG_H */
