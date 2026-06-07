#!/bin/bash
# prueba.sh — Verificación automática de la rúbrica del Sistema de Matrícula
# Uso: bash prueba.sh
# Requiere: gcc, make, python3

set -uo pipefail

# ── Colores ──────────────────────────────────────────────────────────────────
R='\033[0;31m'; G='\033[0;32m'; Y='\033[1;33m'; B='\033[1;34m'; N='\033[0m'

PASS=0; FAIL=0; SRV_PID=""

paso() { printf "${G}[PASS]${N} %s\n" "$1"; ((PASS++)) || true; }
falla(){ printf "${R}[FAIL]${N} %s\n" "$1"; ((FAIL++)) || true; }
info() { printf "${B}[INFO]${N} %s\n" "$1"; }

# ── Limpieza al salir ────────────────────────────────────────────────────────
cleanup() {
    [ -n "$SRV_PID" ] && kill "$SRV_PID" 2>/dev/null || true
    wait "$SRV_PID" 2>/dev/null || true
}
trap cleanup EXIT

echo ""
printf "${B}=================================================${N}\n"
printf "${B}  Sistema de Matrícula Distribuido — Rúbrica${N}\n"
printf "${B}=================================================${N}\n\n"

# ────────────────────────────────────────────────────────────────────────────
# [1] COMPILACIÓN LIMPIA (-Wall -Wextra -pthread sin warnings)
# ────────────────────────────────────────────────────────────────────────────
info "[1] Compilación -Wall -Wextra -pthread -std=c11"
make clean > /dev/null 2>&1 || true
MAKE_LOG=$(make 2>&1)
MAKE_RC=$?
WARNINGS=$(echo "$MAKE_LOG" | grep -i "warning:" | head -5)
if [ $MAKE_RC -eq 0 ] && [ -z "$WARNINGS" ]; then
    paso "make completó sin warnings"
else
    falla "make con errores o warnings"
    echo "$MAKE_LOG" | head -20
fi

# Verificar que los binarios existen
[ -x build/servidor ] && paso "binario build/servidor generado" \
                      || falla "binario build/servidor no encontrado"
[ -x build/cliente  ] && paso "binario build/cliente generado"  \
                      || falla "binario build/cliente no encontrado"

# ────────────────────────────────────────────────────────────────────────────
# [2] USO DE -lrt / POSIX MQUEUE EN EL BINARIO
# ────────────────────────────────────────────────────────────────────────────
info "[2] Verificar POSIX mqueue (mq_open, -lrt)"
if nm build/servidor 2>/dev/null | grep -q "mq_open\|mq_send\|mq_receive"; then
    paso "símbolos mq_open/mq_send/mq_receive presentes en el binario"
elif ldd build/servidor 2>/dev/null | grep -q "librt"; then
    paso "librt enlazada (POSIX mqueue)"
else
    falla "no se detectaron símbolos de POSIX mqueue"
fi

# ────────────────────────────────────────────────────────────────────────────
# [3] ARRANQUE DEL SERVIDOR
# ────────────────────────────────────────────────────────────────────────────
info "[3] Iniciando servidor"
rm -rf datos servidor.log
rm -f /dev/mqueue/matricula_srv 2>/dev/null || true

build/servidor > /tmp/srv_stdout.txt 2>&1 &
SRV_PID=$!
sleep 1   # tiempo para que el servidor abra el socket y la cola

if kill -0 "$SRV_PID" 2>/dev/null; then
    paso "servidor arrancó (PID=$SRV_PID)"
else
    falla "servidor terminó inesperadamente"
    cat /tmp/srv_stdout.txt
    exit 1
fi

# ────────────────────────────────────────────────────────────────────────────
# [4] POSIX MQUEUE EN /dev/mqueue/
# ────────────────────────────────────────────────────────────────────────────
info "[4] Cola POSIX en /dev/mqueue/"
if [ -e "/dev/mqueue/matricula_srv" ]; then
    paso "/dev/mqueue/matricula_srv existe (IPC POSIX activo)"
else
    falla "/dev/mqueue/matricula_srv no encontrado"
fi

# ────────────────────────────────────────────────────────────────────────────
# [5] PRUEBAS DE PROTOCOLO Y PERSISTENCIA (via Python)
# ────────────────────────────────────────────────────────────────────────────
info "[5] Pruebas de socket TCP/IP, persistencia e integridad referencial"

python3 << 'PYEOF'
import socket, struct, sys, os

# ── Constantes del protocolo (deben coincidir con protocolo.h) ────────────
TAM_MSG = 520          # TAM_BUFFER_MSG = TAM_CABECERA_MSG(8) + TAM_PAYLOAD(512)
HOST, PORT = "127.0.0.1", 5001

ENT_ESTUDIANTE, ENT_PROFESOR, ENT_MATERIA, ENT_MATRICULA = 1, 2, 3, 4
OP_INSERTAR, OP_BUSCAR = 1, 2
RES_OK, RES_ERROR, RES_NO_ENCONTRADO, RES_DUPLICADO = 0, 1, 2, 3
RES_SIN_ESTUDIANTE, RES_SIN_MATERIA, RES_SIN_PROFESOR = 10, 11, 12

PASS = 0; FAIL = 0

def ok(msg):
    global PASS; PASS += 1
    print(f"  [PASS] {msg}")

def ko(msg, extra=""):
    global FAIL; FAIL += 1
    print(f"  [FAIL] {msg}" + (f"  → {extra}" if extra else ""))

# ── Serialización (protocolo.c: entidad|op|resultado|pad|tam BE32|payload) ─
def build_msg(ent, op, payload=b''):
    hdr = bytes([ent, op, 0, 0]) + struct.pack('>I', len(payload))
    return (hdr + payload).ljust(TAM_MSG, b'\x00')[:TAM_MSG]

def send_recv(sock, msg):
    sock.sendall(msg)
    buf = b''
    while len(buf) < TAM_MSG:
        chunk = sock.recv(TAM_MSG - len(buf))
        if not chunk:
            raise ConnectionError("servidor cerró la conexión")
        buf += chunk
    resultado    = buf[2]
    tam_payload  = struct.unpack('>I', buf[4:8])[0]
    payload      = buf[8:8+tam_payload] if tam_payload else b''
    return resultado, payload

def pad(s, n):
    b = s.encode() if isinstance(s, str) else s
    return b.ljust(n, b'\x00')[:n]

# ── Empaquetado de entidades (debe coincidir con el layout de los structs C) ─
#   Estudiante: cedula[16]+nombre[64]+apellido[64]+email[64]         = 208 B
#   Profesor:   cedula[16]+nombre[64]+apellido[64]+depto[64]+email[64]= 272 B
#   Materia:    codigo[16]+nombre[64]+descripcion[128]+creditos<i4>+cedula_prof[16] = 228 B
#   Matricula:  cedula_est[16]+codigo_mat[16]+periodo[16]            =  48 B

def pack_estudiante(c, n, a, e):
    return pad(c,16)+pad(n,64)+pad(a,64)+pad(e,64)

def pack_profesor(c, n, a, d, e):
    return pad(c,16)+pad(n,64)+pad(a,64)+pad(d,64)+pad(e,64)

def pack_materia(c, n, d, cr, cp):
    return pad(c,16)+pad(n,64)+pad(d,128)+struct.pack('<i', cr)+pad(cp,16)

def pack_matricula(ce, cm, per):
    return pad(ce,16)+pad(cm,16)+pad(per,16)

# ── Conexión ────────────────────────────────────────────────────────────────
try:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(5)
    s.connect((HOST, PORT))
except Exception as e:
    print(f"  [FAIL] Conexión TCP al servidor: {e}")
    sys.exit(2)

ok("conexión TCP/IP establecida con el servidor")

# ── Insertar Profesor ────────────────────────────────────────────────────────
r, _ = send_recv(s, build_msg(ENT_PROFESOR, OP_INSERTAR,
    pack_profesor("P001", "Ana", "Rojas", "Informatica", "ana@una.ac.cr")))
if r == RES_OK: ok("insertar Profesor")
else: ko("insertar Profesor", f"res={r}")

# ── Insertar Materia (cedula_profesor apunta a P001) ────────────────────────
r, _ = send_recv(s, build_msg(ENT_MATERIA, OP_INSERTAR,
    pack_materia("MAT101", "Algoritmos", "Fundamentos de algoritmos", 4, "P001")))
if r == RES_OK: ok("insertar Materia")
else: ko("insertar Materia", f"res={r}")

# ── Insertar Estudiante ──────────────────────────────────────────────────────
r, _ = send_recv(s, build_msg(ENT_ESTUDIANTE, OP_INSERTAR,
    pack_estudiante("E001", "Carlos", "Mora", "cmora@una.ac.cr")))
if r == RES_OK: ok("insertar Estudiante")
else: ko("insertar Estudiante", f"res={r}")

# ── Insertar Matricula válida (todas las referencias existen) ────────────────
r, _ = send_recv(s, build_msg(ENT_MATRICULA, OP_INSERTAR,
    pack_matricula("E001", "MAT101", "2025-1")))
if r == RES_OK: ok("insertar Matricula válida")
else: ko("insertar Matricula válida", f"res={r}")

# ── Duplicado ────────────────────────────────────────────────────────────────
r, _ = send_recv(s, build_msg(ENT_ESTUDIANTE, OP_INSERTAR,
    pack_estudiante("E001", "Carlos", "Mora", "cmora@una.ac.cr")))
if r == RES_DUPLICADO: ok("insertar duplicado → RES_DUPLICADO (3)")
else: ko("insertar duplicado", f"esperado={RES_DUPLICADO} obtenido={r}")

# ── Buscar entidades existentes ──────────────────────────────────────────────
r, p = send_recv(s, build_msg(ENT_ESTUDIANTE, OP_BUSCAR, pad("E001",16)))
if r == RES_OK:
    nombre = p[16:80].split(b'\x00')[0].decode(errors='replace')
    ok(f"buscar Estudiante existente → nombre='{nombre}'")
else: ko("buscar Estudiante existente", f"res={r}")

r, _ = send_recv(s, build_msg(ENT_PROFESOR, OP_BUSCAR, pad("P001",16)))
if r == RES_OK: ok("buscar Profesor existente")
else: ko("buscar Profesor existente", f"res={r}")

r, _ = send_recv(s, build_msg(ENT_MATERIA, OP_BUSCAR, pad("MAT101",16)))
if r == RES_OK: ok("buscar Materia existente")
else: ko("buscar Materia existente", f"res={r}")

r, _ = send_recv(s, build_msg(ENT_MATRICULA, OP_BUSCAR,
    pack_matricula("E001", "MAT101", "")))
if r == RES_OK: ok("buscar Matricula existente")
else: ko("buscar Matricula existente", f"res={r}")

# ── Buscar entidad inexistente ───────────────────────────────────────────────
r, _ = send_recv(s, build_msg(ENT_ESTUDIANTE, OP_BUSCAR, pad("XXXNOEXISTE",16)))
if r == RES_NO_ENCONTRADO: ok("buscar Estudiante inexistente → RES_NO_ENCONTRADO (2)")
else: ko("buscar Estudiante inexistente", f"esperado={RES_NO_ENCONTRADO} obtenido={r}")

# ── Integridad referencial — falta Estudiante ────────────────────────────────
r, _ = send_recv(s, build_msg(ENT_MATRICULA, OP_INSERTAR,
    pack_matricula("XNOEXISTE", "MAT101", "2025-1")))
if r == RES_SIN_ESTUDIANTE: ok("matricula sin estudiante → 10 (RES_MATRICULA_SIN_ESTUDIANTE)")
else: ko("matricula sin estudiante", f"esperado=10 obtenido={r}")

# ── Integridad referencial — falta Materia ───────────────────────────────────
r, _ = send_recv(s, build_msg(ENT_MATRICULA, OP_INSERTAR,
    pack_matricula("E001", "XNOEXISTE", "2025-1")))
if r == RES_SIN_MATERIA: ok("matricula sin materia  → 11 (RES_MATRICULA_SIN_MATERIA)")
else: ko("matricula sin materia",  f"esperado=11 obtenido={r}")

# ── Integridad referencial — falta Profesor (materia sin profesor válido) ────
# Insertamos una materia con un profesor que no existe, luego intentamos matricular
import socket as _s2mod
s2 = _s2mod.socket(_s2mod.AF_INET, _s2mod.SOCK_STREAM)
s2.settimeout(5)
s2.connect((HOST, PORT))
send_recv(s2, build_msg(ENT_MATERIA, OP_INSERTAR,
    pack_materia("MAT999", "Materia sin prof", "test", 3, "PROFXNOEXISTE")))
send_recv(s2, build_msg(ENT_ESTUDIANTE, OP_INSERTAR,
    pack_estudiante("E002", "Luis", "Fallas", "lf@una.ac.cr")))
r, _ = send_recv(s2, build_msg(ENT_MATRICULA, OP_INSERTAR,
    pack_matricula("E002", "MAT999", "2025-1")))
s2.close()
if r == RES_SIN_PROFESOR: ok("matricula sin profesor → 12 (RES_MATRICULA_SIN_PROFESOR)")
else: ko("matricula sin profesor", f"esperado=12 obtenido={r}")

s.close()

print(f"\n  Subtotal protocolo/persistencia: {PASS} pass, {FAIL} fail")
sys.exit(0 if FAIL == 0 else 1)
PYEOF
PY_RC=$?
if [ $PY_RC -eq 0 ]; then
    paso "todas las pruebas de protocolo/persistencia pasaron"
elif [ $PY_RC -eq 2 ]; then
    falla "no se pudo conectar al servidor (pruebas omitidas)"
else
    falla "una o más pruebas de protocolo/persistencia fallaron"
fi

# ────────────────────────────────────────────────────────────────────────────
# [6] PERSISTENCIA — archivos de datos en disco
# ────────────────────────────────────────────────────────────────────────────
info "[6] Archivos de persistencia"
sleep 0.3   # pequeña espera para que el hilo logger vacíe el pipe
for ent in estudiantes profesores materias matriculas; do
    if [ -s "datos/${ent}.dat" ]; then
        N=$(wc -l < "datos/${ent}.dat")
        paso "datos/${ent}.dat existe ($N línea/s)"
    else
        falla "datos/${ent}.dat ausente o vacío"
    fi
done

# ────────────────────────────────────────────────────────────────────────────
# [7] LOG — pipe interno → archivo
# ────────────────────────────────────────────────────────────────────────────
info "[7] Log interno por pipe"
if [ -s "servidor.log" ]; then
    LINEAS=$(wc -l < servidor.log)
    paso "servidor.log tiene $LINEAS entradas"
    grep -q "cola de mensajes POSIX" servidor.log \
        && paso "log registra creación de cola POSIX" \
        || falla "log no registra creación de cola POSIX"
    grep -q "inicio op" servidor.log \
        && paso "log registra inicio de hilo por operación" \
        || falla "log no registra inicio de hilo por operación"
else
    falla "servidor.log ausente o vacío"
fi

# ────────────────────────────────────────────────────────────────────────────
# [8] HILOS — un hilo por operación
# ────────────────────────────────────────────────────────────────────────────
info "[8] Hilos por operación"
OPS=$(grep -c "inicio op" servidor.log 2>/dev/null || true)
if [ "${OPS:-0}" -ge 10 ]; then
    paso "≥ 10 eventos 'inicio op' en log (un hilo por operación)"
else
    falla "menos de 10 eventos de operación registrados (OPS=${OPS:-0})"
fi

# ────────────────────────────────────────────────────────────────────────────
# [9] ACCESO CONCURRENTE — mutex por entidad
# ────────────────────────────────────────────────────────────────────────────
info "[9] Acceso concurrente (inserción paralela de 5 estudiantes)"
python3 << 'PYEOF2'
import socket, struct, threading, sys

HOST, PORT, TAM_MSG = "127.0.0.1", 5001, 520
ENT_ESTUDIANTE, OP_INSERTAR = 1, 1

def pad(s, n): return s.encode().ljust(n, b'\x00')[:n]
def pack_est(c, n, a, e): return pad(c,16)+pad(n,64)+pad(a,64)+pad(e,64)
def build_msg(ent, op, payload=b''):
    hdr = bytes([ent, op, 0, 0]) + struct.pack('>I', len(payload))
    return (hdr + payload).ljust(TAM_MSG, b'\x00')[:TAM_MSG]

results = []

def insertar(idx):
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(5)
        s.connect((HOST, PORT))
        payload = pack_est(f"C{idx:03d}", f"Nombre{idx}", "Apellido", f"c{idx}@una.ac.cr")
        s.sendall(build_msg(ENT_ESTUDIANTE, OP_INSERTAR, payload))
        buf = b''
        while len(buf) < TAM_MSG:
            chunk = s.recv(TAM_MSG - len(buf))
            if not chunk: break
            buf += chunk
        results.append(buf[2])  # resultado
        s.close()
    except Exception as e:
        results.append(255)

threads = [threading.Thread(target=insertar, args=(i,)) for i in range(5)]
for t in threads: t.start()
for t in threads: t.join()

ok_count = sum(1 for r in results if r == 0)
print(f"  {'[PASS]' if ok_count == 5 else '[FAIL]'} {ok_count}/5 inserciones concurrentes exitosas")
sys.exit(0 if ok_count == 5 else 1)
PYEOF2
[ $? -eq 0 ] && paso "inserciones concurrentes correctas (mutex por entidad)" \
             || falla "inserciones concurrentes fallaron"

# ────────────────────────────────────────────────────────────────────────────
# RESUMEN
# ────────────────────────────────────────────────────────────────────────────
echo ""
printf "${B}=================================================${N}\n"
printf "${B}  RESUMEN${N}\n"
printf "${B}=================================================${N}\n"
printf "${G}  Pasaron: %d${N}\n" "$PASS"
printf "${R}  Fallaron: %d${N}\n" "$FAIL"
echo ""

if [ "$FAIL" -eq 0 ]; then
    printf "${G}  ✓ Todos los criterios de la rúbrica verificados${N}\n\n"
    exit 0
else
    printf "${R}  ✗ ${FAIL} criterio(s) no cumplidos${N}\n\n"
    exit 1
fi
