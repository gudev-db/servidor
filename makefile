CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pedantic
LDFLAGS = 

# Nomes dos executáveis
CLIENTE = meu_navegador
SERVIDOR = meu_servidor

# Arquivos fonte
CLIENTE_SRC = cliente_http.c
SERVIDOR_SRC = servidor_http.c

# Alvo padrão
all: $(CLIENTE) $(SERVIDOR)

# Compilar cliente
$(CLIENTE): $(CLIENTE_SRC)
	$(CC) $(CFLAGS) -o $(CLIENTE) $(CLIENTE_SRC) $(LDFLAGS)

# Compilar servidor
$(SERVIDOR): $(SERVIDOR_SRC)
	$(CC) $(CFLAGS) -o $(SERVIDOR) $(SERVIDOR_SRC) $(LDFLAGS)

# Limpar arquivos compilados
clean:
	rm -f $(CLIENTE) $(SERVIDOR)

# Recompilar tudo
re: clean all

# Instalar (copiar para /usr/local/bin)
install: all
	cp $(CLIENTE) $(SERVIDOR) /usr/local/bin/

.PHONY: all clean re install
