CC      = gcc
CFLAGS  = -Wall -Wextra -pthread -std=c11
BINDIR  = build

SRV_SRCS = servidor/servidor.c servidor/cola.c servidor/persistencia.c \
           servidor/matricula.c servidor/despachador.c servidor/log.c \
           common/protocolo.c

CLI_SRCS = cliente/cliente.c cliente/menu.c common/protocolo.c

.PHONY: all servidor cliente clean

all: servidor cliente

servidor: $(BINDIR)/servidor

$(BINDIR)/servidor: $(SRV_SRCS) | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ -lrt

cliente: $(BINDIR)/cliente

$(BINDIR)/cliente: $(CLI_SRCS) | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^

$(BINDIR):
	mkdir -p $(BINDIR)

clean:
	rm -rf $(BINDIR) servidor.log datos
	-rm -f /dev/mqueue/matricula_srv
