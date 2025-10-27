#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>

#define BUFFER_SIZE 4096
#define MAX_REDIRECTS 5

// Estrutura para armazenar informações da URL
typedef struct {
    char protocolo[16];
    char host[256];
    char caminho[1024];
    int porta;
} URL_INFO;

// Função para analisar a URL
int parse_url(const char *url, URL_INFO *url_info) {
    char temp[2048];
    strncpy(temp, url, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    // Inicializar estrutura
    memset(url_info, 0, sizeof(URL_INFO));
    url_info->porta = 80; // Porta padrão HTTP

    // Verificar se começa com http://
    if (strncmp(temp, "http://", 7) == 0) {
        strcpy(url_info->protocolo, "http");
        memmove(temp, temp + 7, strlen(temp) - 6); // Remover "http://"
    } else {
        fprintf(stderr, "Erro: URL deve começar com http://\n");
        return -1;
    }

    // Encontrar separador de host e caminho
    char *separador = strchr(temp, '/');
    if (separador) {
        *separador = '\0';
        strncpy(url_info->caminho, separador + 1, sizeof(url_info->caminho) - 1);
    } else {
        strcpy(url_info->caminho, "");
    }

    // Verificar se há porta especificada
    char *porta_ptr = strchr(temp, ':');
    if (porta_ptr) {
        *porta_ptr = '\0';
        porta_ptr++;
        url_info->porta = atoi(porta_ptr);
        if (url_info->porta <= 0 || url_info->porta > 65535) {
            fprintf(stderr, "Erro: Porta inválida\n");
            return -1;
        }
    }

    strncpy(url_info->host, temp, sizeof(url_info->host) - 1);

    if (strlen(url_info->host) == 0) {
        fprintf(stderr, "Erro: Host não especificado\n");
        return -1;
    }

    return 0;
}

// Função para extrair nome do arquivo do caminho
char* extrair_nome_arquivo(const char *caminho) {
    const char *nome = strrchr(caminho, '/');
    if (nome == NULL) {
        return strdup("index.html");
    }
    nome++; // Pular a barra
    
    if (strlen(nome) == 0) {
        return strdup("index.html");
    }
    
    return strdup(nome);
}

// Função para extrair código de status da resposta HTTP
int extrair_codigo_status(const char *resposta) {
    char codigo[4];
    if (sscanf(resposta, "HTTP/1.%*d %3s", codigo) == 1) {
        return atoi(codigo);
    }
    return 0;
}

// Função para extrair localização de redirecionamento
char* extrair_localizacao(const char *resposta) {
    char *linha = strstr(resposta, "Location:");
    if (linha == NULL) {
        linha = strstr(resposta, "location:");
    }
    
    if (linha) {
        char *inicio = strchr(linha, ':') + 1;
        while (*inicio == ' ') inicio++;
        
        char *fim = strchr(inicio, '\r');
        if (fim == NULL) {
            fim = strchr(inicio, '\n');
        }
        
        if (fim) {
            size_t len = fim - inicio;
            char *localizacao = malloc(len + 1);
            strncpy(localizacao, inicio, len);
            localizacao[len] = '\0';
            return localizacao;
        }
    }
    
    return NULL;
}

// Função para baixar arquivo
int baixar_arquivo(const char *url, int redirecionamentos) {
    if (redirecionamentos > MAX_REDIRECTS) {
        fprintf(stderr, "Erro: Muitos redirecionamentos\n");
        return -1;
    }

    URL_INFO url_info;
    if (parse_url(url, &url_info) != 0) {
        return -1;
    }

    // Resolver nome do host
    struct hostent *he = gethostbyname(url_info.host);
    if (he == NULL) {
        fprintf(stderr, "Erro: Não foi possível resolver o host %s\n", url_info.host);
        return -1;
    }

    // Criar socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Erro ao criar socket");
        return -1;
    }

    // Configurar endereço do servidor
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(url_info.porta);
    memcpy(&serv_addr.sin_addr, he->h_addr_list[0], he->h_length);

    // Conectar ao servidor
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Erro ao conectar");
        close(sockfd);
        return -1;
    }

    // Preparar requisição HTTP
    char requisicao[2048];
    if (strlen(url_info.caminho) == 0) {
        snprintf(requisicao, sizeof(requisicao),
                "GET / HTTP/1.1\r\n"
                "Host: %s\r\n"
                "Connection: close\r\n"
                "User-Agent: MeuNavegador/1.0\r\n"
                "\r\n", url_info.host);
    } else {
        snprintf(requisicao, sizeof(requisicao),
                "GET /%s HTTP/1.1\r\n"
                "Host: %s\r\n"
                "Connection: close\r\n"
                "User-Agent: MeuNavegador/1.0\r\n"
                "\r\n", url_info.caminho, url_info.host);
    }

    // Enviar requisição
    if (send(sockfd, requisicao, strlen(requisicao), 0) < 0) {
        perror("Erro ao enviar requisição");
        close(sockfd);
        return -1;
    }

    // Receber resposta
    char buffer[BUFFER_SIZE];
    char resposta[BUFFER_SIZE * 10] = {0};
    ssize_t total_recebido = 0;
    ssize_t bytes_recebidos;

    while ((bytes_recebidos = recv(sockfd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        if (total_recebido + bytes_recebidos < sizeof(resposta)) {
            memcpy(resposta + total_recebido, buffer, bytes_recebidos);
            total_recebido += bytes_recebidos;
        }
        buffer[bytes_recebidos] = '\0';
    }

    close(sockfd);

    if (total_recebido == 0) {
        fprintf(stderr, "Erro: Nenhum dado recebido do servidor\n");
        return -1;
    }

    resposta[total_recebido] = '\0';

    // Verificar código de status
    int codigo_status = extrair_codigo_status(resposta);
    
    if (codigo_status == 301 || codigo_status == 302) {
        // Redirecionamento
        char *nova_url = extrair_localizacao(resposta);
        if (nova_url) {
            printf("Redirecionando para: %s\n", nova_url);
            int resultado = baixar_arquivo(nova_url, redirecionamentos + 1);
            free(nova_url);
            return resultado;
        }
    }

    if (codigo_status != 200) {
        fprintf(stderr, "Erro HTTP: Código %d\n", codigo_status);
        return -1;
    }

    // Encontrar fim do cabeçalho (CRLF CRLF)
    char *inicio_dados = strstr(resposta, "\r\n\r\n");
    if (inicio_dados == NULL) {
        inicio_dados = strstr(resposta, "\n\n");
        if (inicio_dados) {
            inicio_dados += 2;
        } else {
            fprintf(stderr, "Erro: Cabeçalho HTTP inválido\n");
            return -1;
        }
    } else {
        inicio_dados += 4;
    }

    // Extrair nome do arquivo
    char *nome_arquivo = extrair_nome_arquivo(url_info.caminho);
    
    // Salvar arquivo
    FILE *arquivo = fopen(nome_arquivo, "wb");
    if (arquivo == NULL) {
        perror("Erro ao criar arquivo");
        free(nome_arquivo);
        return -1;
    }

    size_t tamanho_dados = total_recebido - (inicio_dados - resposta);
    if (fwrite(inicio_dados, 1, tamanho_dados, arquivo) != tamanho_dados) {
        perror("Erro ao escrever arquivo");
        fclose(arquivo);
        free(nome_arquivo);
        return -1;
    }

    fclose(arquivo);
    printf("Arquivo salvo como: %s (%zu bytes)\n", nome_arquivo, tamanho_dados);
    free(nome_arquivo);

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <URL>\n", argv[0]);
        fprintf(stderr, "Exemplo: %s http://www.exemplo.com/arquivo.jpg\n", argv[0]);
        return 1;
    }

    return baixar_arquivo(argv[1], 0) == 0 ? 0 : 1;
}
