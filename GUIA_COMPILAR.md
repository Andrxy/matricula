# Sistema de Matrícula — Guía rápida

## Compilar y correr

```bash
make
```

Terminal 1:
```bash
./bin/servidor
```

Terminal 2:
```bash
./bin/cliente
# o con IP remota:
./bin/cliente 192.168.1.10 5001
```

## Detener / limpiar

```bash
Ctrl+C          # detener servidor
make kill       # forzar cierre + borrar cola POSIX
make clean      # borrar binarios, log y datos/
```

## Prueba rápida

1. `make`
2. `./bin/servidor` (terminal 1)
3. `./bin/cliente` (terminal 2)
4. Ingresar materia → profesor → estudiante → matricular

## Notas

- Log en tiempo real: `tail -f servidor.log`
- Datos en `datos/*.dat` (texto plano separado por `|`)
- El servidor requiere que materia, profesor y estudiante existan antes de matricular
