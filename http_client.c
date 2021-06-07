/*Bibliotecas Padrão*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*Bibliotecas de Sockets*/
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define PORT 80 /*Porta padrão para o HTTP*/
#define FLAG 0
#define MAX 1024

char* removeHttp(char* url);
void confereStatus(int status);
char* findCRLF(char* str);

int main(int argc, char* argv[]){
    int sockfd, i, status, n_bytes, pos_html, fim_http_header = 0;
    char http_res[MAX], http_req[MAX];
    char *url_completa, *url_host, *url_path, *ini_html;
    FILE *arq;
    struct sockaddr_in serv_addr;
    struct hostent *servidor;

    url_path = (char*)malloc(512 * sizeof(char));
    memset(&url_path, 0, strlen(url_path));

    /*Confere os parametros de entrada*/
    if(argc < 2){
        printf("Erro no parametro de entrada!\nUse: %s <URL completa (com http://)>\n", argv[0]);
        return -1;
    }

    /*Realiza a separação da url*/
    url_completa = argv[1];

    url_host = removeHttp(url_completa);
    /*O http:// da URL é removido, devido dar erro na função do gethostbyname se ele estiver presente*/
    strcpy(url_path, url_host);

    for(i = 0; i < strlen(url_host); i++){
        if(url_host[i] == '/'){
            url_host[i] = '\0';
            break;
        }
    }

    url_path += strlen(url_host);

    /*No caso onde só se foi passado o endereço do host, o path recebe um / */
    if(strlen(url_path) == 0){
        strcpy(url_path, "/");
    }

    /*Criação da Socket*/
    sockfd = socket(AF_INET, SOCK_STREAM, FLAG);
    if (sockfd == -1){
        printf("Falha ao criar o socket...\n");
        exit(1);
    }else{
        printf("Socket criada com sucesso...\n");
    }

    /*Define o endereço*/
    servidor = gethostbyname(url_host);
    if(servidor == NULL){
        printf("O host informado não existe...\n");
        exit(1);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_addr.s_addr = inet_addr(inet_ntoa(*(struct in_addr*)*(servidor->h_addr_list)));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    /*Conecta ao servidor HTTP*/
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
        printf("Falha ao conectar ao servidor...\n");
        exit(1);
    }else{
        printf("Conectado com o servidor...\n");
    }

    /*Manda a requisição ao servidor*/
    sprintf(http_req, "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", url_path, url_host);
    send(sockfd, http_req, strlen(http_req), 0);

    recv(sockfd, http_res, 12, 0);
    sscanf(http_res, "%*s %d", &status);
    printf("Status da conexão: %d\n", status);
    confereStatus(status);

    if((arq = fopen("Arquivo.html", "w")) == NULL){
        printf("Falha ao criar o arquivo...\n");
        exit(1);
    }
    /*Estrutura de repetição para inserir o corpo da resposta do HTTP num arquivo HTML*/
    while ((n_bytes = recv(sockfd, http_res, sizeof(http_res), 0))) {
        http_res[n_bytes] = '\0';
        if(fim_http_header == 0){
            ini_html = findCRLF(http_res);
            /*Como o fim do header da resposta do HTTP é um \r\n\r\n, ele confere se achou o fim
            do header na string do http_res, caso tenha achado, a posição do vetor é alterado para
            o indice após o fim do header e permitindo a gravação no arquivo, assim pegando somente 
            o corpo da resposta, que é a parte que nos interessa */
            if(ini_html != NULL){
                pos_html = ini_html - http_res;    
                memmove(http_res, http_res+pos_html+1, strlen(http_res) - pos_html);
                fim_http_header = 1;
            }
        }
        
        if(fim_http_header){
            fprintf(arq, "%s",http_res);
        }
        memset(&http_res, '\0', sizeof(http_res));
    }

    printf("Arquivo gerado com sucesso!\n");

    fclose(arq);
    return 0;
}

char* removeHttp(char* url){
    char* host = url + 7;
    return host;
}

void confereStatus(int status){
    /*Função que mostra o retorno da solicitação, tendo algumas mensagens especificas para os erros mais comuns*/
    if(status >= 100 && status < 200){
        /*Apesar de estar implementada, o HTTP não costuma retornar uma resposta (que contém o corpo) com o status 1XX*/
        printf("Resposta Informativa.\n");
        printf("Porém não houve o retorno do corpo do HTTP\n");
        exit(1);
    }else if(status < 300){
        printf("Resposta de Sucesso - A solicitação foi aceita!\n");
        if(status != 200){
            printf("Porém não houve o retorno do corpo do HTTP\n");
            exit(1);
        }
    }else if(status < 400){
        printf("Mensagem de Redirecionamento - É necessário outra ação para completar a solicitação!\n");
        if(status == 301){
            printf("A página solicitada foi movida permanentemente!\n");
        }else if(status == 302){
            printf("Página encontrada, porém ela foi mudada temporariamente.\n");
        }
        exit(1);
    }else if(status < 500){
        printf("Erro do Cliente\n");
        if(status == 400){
            printf("Requesição com sintaxe inválida.\n");
        }else if(status == 404){
            printf("Não foi possível encontrar essa página.\n");
        }else if(status == 418){
            /*Eu só coloquei esse erro porque ele é engraçado*/
            printf("O servidor recusa a tentativa de coar café num bule de chá.\n");
        }
        exit(1);
    }else if(status < 600){
        printf("Erro do Servidor\n");
        if(status == 500){
            printf("Houve um erro interno no servidor.\n");
        }else if(status == 503){
            printf("O serviço do servidor está indisponível!\n");
        }else if(status == 524){
            printf("O servidor da web de origem atingiu o tempo limite para resposta a esta solicitação!\n");
        }
        exit(1);
    }
}

char* findCRLF(char* str){
    /*Função responsável por achar a sequencia de 2 CRFL (\r\n\r\n)*/
    for(; *str != '\0'; str++) {
        if(*str == '\r' && *(str + 1) == '\n' && *(str + 2) == '\r' && *(str + 3) == '\n') {
            str += 3;
           return str;
        }
    }
    return NULL;
}