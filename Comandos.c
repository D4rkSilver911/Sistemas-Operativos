#include "header.h"
int main(int argc, char const *argv[]){
	char command[BUF_SIZE],buffer[BUF_SIZE];
	int pipe;
	if ((mkfifo(PIPE_NAME, O_CREAT|O_EXCL|0600)<0) && (errno!= EEXIST)) {
        perror("Não foi possivel criar pipe: ");
        exit(0);
    }
    printf("\tEstrutura do comando\n");
    printf("ORDER 'order name' prod: 'product name'  'quantity' to: 'x', 'y'\n");
    while(1){
    	 if ((pipe = open(PIPE_NAME, O_WRONLY))< 0) {
            perror("Não foi possível abrir pipe para escrita. ");
            exit(0);
        }
        fflush(stdout);
        printf("Escreva o comando:");
    	fgets(command,BUF_SIZE,stdin);
    	write(pipe,command,BUF_SIZE);
    	close(pipe);

    	if ((pipe = open(PIPE_NAME,O_RDONLY))<0){
            perror("Não foi possível abrir pipe para leitura.");
        }
        read(pipe,buffer,BUF_SIZE);
        printf("%s\n",buffer);
        close(pipe);


	}

	return 0;
}