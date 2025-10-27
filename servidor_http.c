#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#define MAX_CONEXOES 10

// Função para obter o tipo MIME baseado na extensão do arquivo
const char* obter_tipo_mime(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (ext == NULL) return "application/octet-stream";
    
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
    if (strcmp(ext, ".txt") == 0) return "text/plain";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".pdf") == 0) return "application/pdf";
    if (strcmp(ext, ".zip") == 0) return "application/zip";
    if (strcmp(ext, ".json") == 0) return "application/json";
    
    return "application/octet-stream";
}

// Função para URL decode
void url_decode(char *str) {
    char *src = str, *dst = str;
    while (*src) {
        if (*src == '%' && src[1] && src[2]) {
            char hex[3] = {src[1], src[2], 0};
            *dst = (char)strtol(hex, NULL, 16);
            src += 2;
        } else if (*src == '+') {
            *dst = ' ';
        } else {
            *dst = *src;
        }
        src++;
        dst++;
    }
    *dst = '\0';
}

// Função para enviar resposta HTTP
void enviar_resposta(int client_socket, int status, const char *status_msg, 
                    const char *content_type, const char *content, size_t content_length) {
    char header[1024];
    time_t now = time(NULL);
    struct tm *tm = gmtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%a, %d %b %Y %H:%M:%S GMT", tm);
    
    snprintf(header, sizeof(header),
             "HTTP/1.1 %d %s\r\n"
             "Server: MeuServidor/1.0\r\n"
             "Date: %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n",
             status, status_msg, time_str, content_type, content_length);
    
    send(client_socket, header, strlen(header), 0);
    if (content && content_length > 0) {
        send(client_socket, content, content_length, 0);
    }
}

// Função para enviar arquivo
int enviar_arquivo(int client_socket, const char *caminho_completo) {
    FILE *arquivo = fopen(caminho_completo, "rb");
    if (arquivo == NULL) {
        return 0;
    }
    
    // Obter tamanho do arquivo
    fseek(arquivo, 0, SEEK_END);
    long tamanho = ftell(arquivo);
    fseek(arquivo, 0, SEEK_SET);
    
    // Ler arquivo
    char *buffer = malloc(tamanho);
    if (buffer == NULL) {
        fclose(arquivo);
        return 0;
    }
    
    fread(buffer, 1, tamanho, arquivo);
    fclose(arquivo);
    
    // Enviar resposta
    const char *tipo_mime = obter_tipo_mime(caminho_completo);
    enviar_resposta(client_socket, 200, "OK", tipo_mime, buffer, tamanho);
    
    free(buffer);
    return 1;
}

// Função para gerar listagem de diretório
char* gerar_listagem_diretorio(const char *caminho, const char *caminho_url) {
    DIR *dir = opendir(caminho);
    if (dir == NULL) {
        return NULL;
    }
    
    char *html = malloc(BUFFER_SIZE * 10);
    if (html == NULL) {
        closedir(dir);
        return NULL;
    }
    
    // Cabeçalho HTML
    char titulo[512];
    snprintf(titulo, sizeof(titulo), "Listagem de diretório: %s", caminho_url);
    
    strcpy(html, "<!DOCTYPE html>\n"
                 "<html>\n"
                 "<head>\n"
                 "<title>");
    strcat(html, titulo);
    strcat(html, "</title>\n"
                 "<style>\n"
                 "body { font-family: Arial, sans-serif; margin: 40px; }\n"
                 "h1 { color: #333; }\n"
                 "ul { list-style-type: none; padding: 0; }\n"
                 "li { margin: 5px 0; }\n"
                 "a { text-decoration: none; color: #0066cc; }\n"
                 "a:hover { text-decoration: underline; }\n"
                 "</style>\n"
                 "</head>\n"
                 "<body>\n"
                 "<h1>");
    strcat(html, titulo);
    strcat(html, "</h1>\n<ul>\n");
    
    // Adicionar link para diretório pai (se não for root)
    if (strcmp(caminho_url, "/") != 0) {
        strcat(html, "<li><a href=\"../\">../</a></li>\n");
    }
    
    // Listar arquivos e diretórios
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0) continue;
        
        char caminho_completo[1024];
        snprintf(caminho_completo, sizeof(caminho_completo), "%s/%s", caminho, entry->d_name);
        
        struct stat st;
        if (stat(caminho_completo, &st) == 0) {
            char entrada[512];
            if (S_ISDIR(st.st_mode)) {
                snprintf(entrada, sizeof(entrada), 
                        "<li><a href=\"%s/\">%s/</a></li>\n", 
                        entry->d_name, entry->d_name);
            } else {
                snprintf(entrada, sizeof(entrada), 
                        "<li><a href=\"%s\">%s</a> (%ld bytes)</li>\n", 
                        entry->d_name, entry->d_name, st.st_size);
            }
            strcat(html, entrada);
        }
    }
    
    closedir(dir);
    
    strcat(html, "</ul>\n</body>\n</html>\n");
    return html;
}

// Função para processar requisição
void processar_requisicao(int client_socket, const char *diretorio_base) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_recebidos = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_recebidos <= 0) {
        close(client_socket);
        return;
    }
    
    buffer[bytes_recebidos] = '\0';
    
    // Verificar se é uma requisição GET
    if (strncmp(buffer, "GET ", 4) != 0) {
        enviar_resposta(client_socket, 405, "Method Not Allowed", 
                       "text/plain", "Método não permitido\n", 21);
        close(client_socket);
        return;
    }
    
    // Extrair caminho da URL
    char *inicio_caminho = buffer + 4;
    char *fim_caminho = strchr(inicio_caminho, ' ');
    if (fim_caminho == NULL) {
        enviar_resposta(client_socket, 400, "Bad Request", 
                       "text/plain", "Requisição inválida\n", 20);
        close(client_socket);
        return;
    }
    
    *fim_caminho = '\0';
    char caminho_url[1024];
    strncpy(caminho_url, inicio_caminho, sizeof(caminho_url) - 1);
    caminho_url[sizeof(caminho_url) - 1] = '\0';
    
    // URL decode
    url_decode(caminho_url);
    
    // Prevenir directory traversal
    if (strstr(caminho_url, "..") != NULL) {
        enviar_resposta(client_socket, 403, "Forbidden", 
                       "text/plain", "Acesso negado\n", 14);
        close(client_socket);
        return;
    }
    
    // Construir caminho completo no sistema de arquivos
    char caminho_completo[2048];
    if (strcmp(caminho_url, "/") == 0) {
        snprintf(caminho_completo, sizeof(caminho_completo), "%s", diretorio_base);
    } else {
        snprintf(caminho_completo, sizeof(caminho_completo), "%s%s", diretorio_base, caminho_url);
    }
    
    // Verificar se é um diretório
    struct stat st;
    if (stat(caminho_completo, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            // Verificar se existe index.html
            char index_path[2048];
            snprintf(index_path, sizeof(index_path), "%s/index.html", caminho_completo);
            
            if (access(index_path, R_OK) == 0) {
                // Servir index.html
                if (!enviar_arquivo(client_socket, index_path)) {
                    enviar_resposta(client_socket, 500, "Internal Server Error", 
                                   "text/plain", "Erro interno do servidor\n", 25);
                }
            } else {
                // Gerar listagem do diretório
                char *listagem = gerar_listagem_diretorio(caminho_completo, caminho_url);
                if (listagem) {
                    enviar_resposta(client_socket, 200, "OK", "text/html", 
                                   listagem, strlen(listagem));
                    free(listagem);
                } else {
                    enviar_resposta(client_socket, 403, "Forbidden", 
                                   "text/plain", "Acesso negado\n", 14);
                }
            }
        } else {
            // É um arquivo regular
            if (!enviar_arquivo(client_socket, caminho_completo)) {
                enviar_resposta(client_socket, 404, "Not Found", 
                               "text/plain", "Arquivo não encontrado\n", 23);
            }
        }
    } else {
        enviar_resposta(client_socket, 404, "Not Found", 
                       "text/plain", "Arquivo não encontrado\n", 23);
    }
    
    close(client_socket);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <diretório>\n", argv[0]);
        fprintf(stderr, "Exemplo: %s /home/usuario/meusite\n", argv[0]);
        return 1;
    }
    
    const char *diretorio_base = argv[1];
    
    // Verificar se o diretório existe e é acessível
    if (access(diretorio_base, R_OK) != 0) {
        perror("Erro ao acessar diretório");
        return 1;
    }
    
    // Criar socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Erro ao criar socket");
        return 1;
    }
    
    // Permitir reuso da porta
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Erro setsockopt");
        close(server_socket);
        return 1;
    }
    
    // Configurar endereço
    struct sockaddr_in endereco;
    memset(&endereco, 0, sizeof(endereco));
    endereco.sin_family = AF_INET;
    endereco.sin_addr.s_addr = INADDR_ANY;
    endereco.sin_port = htons(PORT);
    
    // Vincular socket
    if (bind(server_socket, (struct sockaddr *)&endereco, sizeof(endereco)) < 0) {
        perror("Erro ao vincular socket");
        close(server_socket);
        return 1;
    }
    
    // Escutar conexões
    if (listen(server_socket, MAX_CONEXOES) < 0) {
        perror("Erro ao escutar");
        close(server_socket);
        return 1;
    }
    
    printf("Servidor HTTP rodando em http://localhost:%d\n", PORT);
    printf("Servindo arquivos de: %s\n", diretorio_base);
    printf("Pressione Ctrl+C para parar o servidor\n");
    
    // Loop principal do servidor
    while (1) {
        struct sockaddr_in cliente_endereco;
        socklen_t cliente_tamanho = sizeof(cliente_endereco);
        
        int client_socket = accept(server_socket, 
                                 (struct sockaddr *)&cliente_endereco, 
                                 &cliente_tamanho);
        
        if (client_socket < 0) {
            perror("Erro ao aceitar conexão");
            continue;
        }
        
        // Processar requisição em um processo filho
        pid_t pid = fork();
        if (pid == 0) {
            // Processo filho
            close(server_socket);
            processar_requisicao(client_socket, diretorio_base);
            exit(0);
        } else if (pid > 0) {
            // Processo pai
            close(client_socket);
        } else {
            perror("Erro ao criar processo filho");
            close(client_socket);
        }
    }
    
    close(server_socket);
    return 0;
}
