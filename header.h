#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include "drone_movement.c"
#include <math.h>
#include <sys/msg.h>
#define TRUE 1
#define FALSE 0

#define BUF_SIZE 1024
#define PIPE_NAME "input_pipe"
#define MAX_ARMAZENS 10
#define MAX_PRODUTOS 10
#define SIZENAME 20
#define MAX 100
#define MAX_SALTOS 3000
#define MAX_DRONES 50

typedef struct{
	double x,y;
}Coordenadas;


typedef struct{
	char nome[SIZENAME];
	int quantidade;

}Produto;


typedef struct{
	char nome[SIZENAME];
	Coordenadas local;
	Produto produto[MAX_PRODUTOS];
	
}Armazem;


typedef struct{
	int num;
	Produto *produto;
	Coordenadas destino;
	

}Encomenda;

typedef struct{
	Coordenadas posicao;
	int id;
	bool estado; //FALSE=inativo  TRUE=ativo
	Coordenadas destino;
	Coordenadas destino2;
	int enc_num;

}Drone;
	
typedef struct{
	Armazem *arm[MAX_ARMAZENS];
	int enc_atribuidas;
	int prod_carregadas;
	int total_enc_entregue;
	int total_prod_entregue;
	int temp_medio;
}mem_partilhada;

typedef struct{
	Coordenadas max;	
	int n_drones;
	int freq_abastecimento;
	int unidade_tempo;
	int n_armazens;
	char tipoProduto[MAX_PRODUTOS][SIZENAME];
	int quant_prod;

}Config;


typedef struct{
	long mtype;
	int id_encomenda;
	int drone;
	Encomenda encomenda;
}Mq;
