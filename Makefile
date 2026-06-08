CC      = gcc
CFLAGS  = -Wall -Wextra -pthread -std=c11
BINDIR  = bin

SRV_SRCS = servidor/servidor.c servidor/cola.c servidor/persistencia.c \
           servidor/matricula.c servidor/despachador.c servidor/log.c \
           protocolo/protocolo.c

CLI_SRCS = cliente/cliente.c cliente/menu.c protocolo/protocolo.c

.PHONY: all run-servidor run-cliente kill clean

all: $(BINDIR)/servidor $(BINDIR)/cliente

$(BINDIR)/servidor: $(SRV_SRCS) | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ -lrt

$(BINDIR)/cliente: $(CLI_SRCS) | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^

$(BINDIR):
	mkdir -p $(BINDIR)

run-servidor: $(BINDIR)/servidor
	./$(BINDIR)/servidor

run-cliente: $(BINDIR)/cliente
	./$(BINDIR)/cliente

kill:
	-pkill -x servidor
	-rm -f /dev/mqueue/matricula_srv

clean:
	rm -rf $(BINDIR) servidor.log datos
	-rm -f /dev/mqueue/matricula_srv
