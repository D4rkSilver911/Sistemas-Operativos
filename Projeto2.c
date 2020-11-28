#include "header.h"

//CARLOS LIMA 2017266922
//ANTONIO MARIA 2017265346

Config cf;
Coordenadas max;
Armazem armazem[MAX_ARMAZENS];
mem_partilhada *mp;
int shmid;
int fd;
pid_t armazens[MAX_ARMAZENS];
int n_produtos=0;
Encomenda pedidos[MAX];
Encomenda pedidos_suspensos[MAX];
pid_t id_centr,getid;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
sem_t *sem_esta;
sem_t *sem_drone;
sigset_t set_bloqueado,intmask;
int mq_id;
Drone drones_ativo[MAX_DRONES];
int done=1;
int suspenso=0;



//Inicializar semaforos
void init (){
	sem_unlink("SEM_ESTA");
	if((sem_esta = sem_open("SEM_ESTA",O_CREAT|O_EXCL,0700,1)) == SEM_FAILED){
		perror("Failed to initialize semaphore");
  	}
	sem_unlink("SEM_DRONE");
	if((sem_drone = sem_open("SEM_DRONE",O_CREAT|O_EXCL,0700,1)) == SEM_FAILED){
		perror("Failed to initialize semaphore");
	}
}


//ler config
void ler_config(Armazem* armazem){

	FILE *f;
	char linha[MAX];
	char *prod;
	int i = 0;

	f = fopen("config.txt","r"); //Abrir ficheiro
	
	/* Se ficheiro nao existir */
	if (f == NULL){
		printf("Error: File not Found!\n");
	}

	fgets(linha, MAX, f);
	cf.max.x = atoi(strtok(linha,",")); //xmax
	cf.max.y = atoi(strtok(NULL, ",")); //ymax
	if(cf.max.y <=0 || cf.max.x <= 0){
		printf("%f %f", cf.max.x, cf.max.y);
		printf("Dimensões não podem ser negativas!");
		exit(0);
	}
	printf("\n------------------------------------\n");

	printf("XMáx: %f YMáx: %f\n", cf.max.x, cf.max.y);

	fgets(linha, MAX, f);
	char *token = strtok(linha, ", ");
	printf("Produtos: ");
	while(token != NULL){
		strcpy(cf.tipoProduto[n_produtos],token);
		token = strtok(NULL, ", ");
		printf("%s ", cf.tipoProduto[n_produtos]);
		n_produtos++;
		if(n_produtos == MAX_PRODUTOS){
			printf("Numero limite de produtos atingido!\n");
			exit(0);
		}
		
	}
	
	/*Numero de drones*/
	fgets(linha, MAX, f);
	cf.n_drones = atoi(linha);
	if(cf.n_drones <= 0 ){
		printf("Numero de drones incorretos!\n");
		exit(0);
	}
	printf("Nº Drones: %d\n",cf.n_drones);

	/*Freq. Abastecimento, Quantidade e Unidade de tempo*/
	fgets(linha, MAX, f);

	cf.freq_abastecimento = atoi(strtok(linha,","));
	if(cf.freq_abastecimento<0){
		printf("Frequência de Abastecimento incorreta!\n");
		exit(0);
	}
	printf("Freq. Abastecimento: %d\n",cf.freq_abastecimento);

	cf.quant_prod = atoi(strtok(NULL, ","));
	if(cf.quant_prod<0){
		printf("Quantidade de produtos inválida!\n");
		exit(0);
	}
	printf("Quantidade: %d\n",cf.quant_prod);

	cf.unidade_tempo = atoi(strtok(NULL, ","));
	if(cf.unidade_tempo<0){
		printf("Unidade de tempo inválida!\n");
		exit(0);
	}
	printf("Unidade de tempo: %d\n",cf.unidade_tempo);

	/*Nº Armazens*/

	fgets(linha, MAX, f);
	cf.n_armazens = atoi(linha);
	if(cf.n_armazens<1 || cf.n_armazens>MAX_ARMAZENS){
		printf("Numero de armazéns inválidos!\n");
	}
	printf("Nº Armazéns: %d\n",cf.n_armazens);
 	for(i=0;i<cf.n_armazens;i++){
        	fscanf(f,"%s %*s %lf %*s %lf %*s %[^\n]",armazem[i].nome,&armazem[i].local.x,&armazem[i].local.y,linha);
        	if(armazem[i].local.x<0 || armazem[i].local.y<0 ){
            		printf("Coordenadas do armazem %s fora do limite da simulação!!\n",armazem[i].nome);
           		 exit(0);
        	}
        printf("\n%s xy: %f, %f prod: ",armazem[i].nome,armazem[i].local.x,armazem[i].local.y);
        prod = strtok (linha,", ");
        int j =0;
        while (prod != NULL){
            strncpy(armazem[i].produto[j].nome, prod,SIZENAME);
            j++;
            if(j==MAX_PRODUTOS){
                printf("\nNumero de produtos inicial do armazem %s superior ao permitido!!\n",armazem[i].nome);
                exit(0);
            }
            prod = strtok (NULL, ", ");
            armazem[i].produto[j-1].quantidade=atoi(prod);
            prod = strtok (NULL, ", ");
            printf("%s, %d, ",armazem[i].produto[j-1].nome,armazem[i].produto[j-1].quantidade);
            
        }
        
    }
    
    printf("\n------------------------------------\n");
    fclose(f);

}

void esta_handler(int signum){
	sem_wait(sem_esta);
	printf("Número total de encomendas atribuidas aos drones: %d\n",mp->enc_atribuidas);
	printf("Número total de produtos carregados de armazens: %d\n",mp->prod_carregadas);
	printf("Número total de encomendas entregues: %d\n",mp->total_enc_entregue);
	printf("Número total de produtos entregues: %d\n",mp->total_prod_entregue);
	printf("Tempo médio para conclusão de uma encomenda:%d\n",mp->temp_medio);
	sem_post(sem_esta);
}

//
void cria_armazem(int id,Armazem *armazem){

	init();
	Mq abastece;
	int i,k;
	while(1){

		printf("Armazem[%d] a receber stock!\n", getpid());
		msgrcv(mq_id, &abastece, sizeof(Mq)-sizeof(long),20181, 0);

		printf("Armazem[%d] recebeu %d produtos do produto %s", getpid(), abastece.encomenda.produto->quantidade, abastece.encomenda.produto->nome);
		
		for (i = 0; i < MAX_ARMAZENS; i++){
			for(k = 0; k < MAX_PRODUTOS; k++){
				if ( strcmp(armazem[i].produto[k].nome, abastece.encomenda.produto->nome)==0){
					armazem[i].produto[k].quantidade = abastece.encomenda.produto->quantidade;
				}
			}
		}

	}
	exit(0);
}

void repouso(Drone *drone){
	int my_id = drone->id;
	sem_wait(sem_drone);
	//meter o drone no array
	drones_ativo[my_id].posicao.x = drone->posicao.x;
	drones_ativo[my_id].posicao.y = drone->posicao.y;
	drones_ativo[my_id].id = drone->id;
	drones_ativo[my_id].estado = drone->estado;
	drones_ativo[my_id].destino.x = 0;
	drones_ativo[my_id].destino.y = 0;
	sem_post(sem_drone);
}


int drone_to_armazem(Drone *drone){
	int y; //Variavel que vai ver se o drone chegou ou nao ao armazem
	int i;
	int sucesso = 1; //Se ele chegou lá dou return 1
	int failure = 2; //Se nao chegou dou return em 2
	for ( i = 0; i < MAX_SALTOS; i++){ //Este MAX Sáltos é o numero de "saltos" que o drone dá até ao destino, defini 3000 porque nesse numero ele provavelmente ja la chegou
		y = move_towards(&drone->posicao.x, &drone->posicao.y, drone->destino.x, drone->destino.y);

		if (y == 0){
			printf("\nSUCESSO! Drone [%d] chegou ao armazem com coordenadas (%.2f, %.2f)\n", drone->id, drone->destino.x, drone->destino.y);
			return sucesso;
		}

		/*else if(y < 1){
			printf("Erro: Drone (%.2f, %.2f) != Armazem (%.2f, %.2f)\n", drone->posicao.x, drone->posicao.y, drone->destino.x, drone->destino.y);
			
		}*/


	}
	if (drone->posicao.x != drone->destino.x || drone->posicao.y != drone->destino.y){
		printf("Oops! O drone %d não chegou ao seu destino!\nFicou parado em: (%.2f, %.2f)\n", drone->id, drone->posicao.x, drone->posicao.y);

		return failure;
	}
}

int drone_to_encomenda_destino(Drone *drone){
	int i;
	int y; //Variavel do move_towards
	int sucesso = 1;
	int failure = 0;
	for(i = 0; i < MAX_SALTOS; i++){
		y = move_towards(&drone->posicao.x, &drone->posicao.y, drone->destino2.x, drone->destino2.y);

		if (y == 0){
			printf("SUCESSO! Drone [%d] entregou a encomenda em (%f, %f)\n", drone->id, drone->destino2.x, drone->destino2.y);
			drone->estado=FALSE;
			return sucesso;
		}

		/*else if(y < 1){
			printf("Erro: Drone (%f, %f) != Encomenda (%f, %f)\n", drone->posicao.x, drone->posicao.y, drone->destino.x, drone->destino.y);
						
		}	*/	

	}
	if (drone->posicao.x != drone->destino2.x || drone->posicao.y != drone->destino2.y){
		printf("Oops! O drone %d não chegou às coordenadas da encomenda!\nFicou parado em: (%f, %f)\n", drone->id, drone->posicao.x, drone->posicao.y);

		return failure;
	}
	
}

int drone_retorno_base(Drone *drone){
	//Calcular base mais perto

	int distancia_1; 
	int distancia_2;
	int distancia_3;
	int distancia_4;
	int menor;
	int i, j;
	int x,y;
	int success = 1;
	int failure = 0;

	distancia_1 = distance(drone->posicao.x, drone->posicao.y, 0,0);
	distancia_2 = distance(drone->posicao.x, drone->posicao.y,0, cf.max.y);
	distancia_3 = distance(drone->posicao.x, drone->posicao.y,cf.max.x, 0);
	distancia_4 = distance(drone->posicao.x, drone->posicao.y,cf.max.x, cf.max.y);
	if(distancia_2 < distancia_1){
		menor = distancia_2;
		x = 0;
		y = cf.max.y;
	}
	else{
		menor = distancia_1;
		x = 0;
		y = 0;
	}
	if(distancia_3 < menor){
		menor = distancia_3;
		x = cf.max.x;
		y = 0;
	}
	if(distancia_4 < menor){
		menor = distancia_4;
		x = cf.max.x;
		y = cf.max.y;

	}

	for(i = 0; i < MAX_SALTOS; i++){
		j = move_towards(&drone->posicao.x, &drone->posicao.y,x,y);

		if (j == 0){
			printf("Drone [%d] chegou com sucesso à base (%f, %f)\n",drone->id, drone->posicao.x, drone->posicao.y);
			return success;
		}
		else if(j < 1){
			printf("Erro: Drone (%f, %f) != Base (%d, %d)\n", drone->posicao.x, drone->posicao.y, x,y);
		
		}

		if(drone->estado == TRUE){ //Se ele receber encomenda a meio, ele da return em 2
			return 2;
		}


	}
	if (drone->posicao.x != x || drone->posicao.y != y){
		printf("Oops! O drone %d não chegou ao seu destino!\nFicou parado em: (%f, %f)\n", drone->id, drone->posicao.x, drone->posicao.y);

		return failure;
	
	}



}

void comunica_armazem(Drone *drone, Armazem armazem){
	Mq recebe;
	Mq envia;
	int i;
	int quantidade;
	char tipo[BUF_SIZE];
	envia.mtype = 20183;
	msgrcv(mq_id, &recebe, sizeof(recebe)-sizeof(long), 20182, 0);

	printf("\nArmazem %s recebeu um pedido de uma encomenda com o ID = %d\n", armazem.nome, recebe.id_encomenda);
	for (i = 0; i < sizeof(pedidos)/sizeof(pedidos[0]); i++){
		if(pedidos[i].num == recebe.id_encomenda){
			quantidade = pedidos[i].produto->quantidade;
			strcpy(tipo, pedidos[i].produto->nome);
			printf("\nNumero da encomenda recebida: %d\nTipo de Produto: %s\nQuantidade do produto: %d\n\n", recebe.id_encomenda, tipo, quantidade);
		}

	}
	envia.encomenda.produto->quantidade = quantidade;
	strcpy(envia.encomenda.produto->nome, tipo);

	//printf("Envia quantidade : %d\nEnvia nome: %s",envia.encomenda.produto->quantidade,  envia.encomenda.produto->nome);

	msgsnd(mq_id, &envia, sizeof(envia)-sizeof(long), 0);

}
//
void *drone(void* idp){
	Armazem arm_escolhido;
	Mq envia_arm;
	Mq recebe_arm;
	int num;
	int i, j;
	int my_id=*((int*)idp);
	Drone *drone_escolhido=&drones_ativo[my_id];
	//escrever na memoria partilhada drone
	int estado_drone = 1;
	drone_escolhido->id=my_id;
	drone_escolhido->estado = FALSE;
	num=rand()%4;
	if (num==0){
		drone_escolhido->posicao.x=0;
		drone_escolhido->posicao.y=0;
	}
	if (num==1){
		drone_escolhido->posicao.x=cf.max.x;
		drone_escolhido->posicao.y=0;
	}
	if (num==2){
		drone_escolhido->posicao.x=0;
		drone_escolhido->posicao.y=cf.max.y;
	}
	if (num==3){
		drone_escolhido->posicao.x=cf.max.x;
		drone_escolhido->posicao.y=cf.max.y;
	}
	drone_escolhido->estado=false;


	

	printf("Drone[%d] criado com sucesso com coordenadas = (%.2f, %.2f)\n",drone_escolhido->id, drone_escolhido->posicao.x, drone_escolhido->posicao.y);


	envia_arm.mtype = 20182;
	

 
	if(estado_drone==1){
		//printf("Entrou no if 1");
		repouso(drone_escolhido);
		pthread_mutex_lock(&mutex);
		while(drone_escolhido->estado==FALSE){
			pthread_cond_wait(&cond, &mutex);
		}
		pthread_mutex_unlock(&mutex);
		if(drone_escolhido->estado == TRUE){
			envia_arm.id_encomenda = drone_escolhido->enc_num;
			estado_drone++;
		}
	}
	if(estado_drone==2){
		if(drone_to_armazem(drone_escolhido)==1){
			estado_drone++;
		}
	}

	if(estado_drone==3){
		for(i = 0; i < cf.n_armazens; i++){
			for(j = 0; j < n_produtos; j++){
				if(drone_escolhido->posicao.x == armazem[i].local.x && drone_escolhido->posicao.y == armazem[i].local.y){
					strcpy(arm_escolhido.nome,armazem[i].nome);
					arm_escolhido.local.x = armazem[i].local.x;
					arm_escolhido.local.y = armazem[i].local.y;
					strcpy(arm_escolhido.produto[j].nome, armazem[i].produto[j].nome);
					arm_escolhido.produto[j].quantidade = armazem[i].produto[j].quantidade;
				}
			}
		}
		msgsnd(mq_id, &envia_arm, sizeof(envia_arm)-sizeof(long),0);
		comunica_armazem(drone_escolhido, arm_escolhido);
		msgrcv(mq_id, &recebe_arm, sizeof(recebe_arm)-sizeof(long),20183, 0);
		printf("\nDrone recebeu encomenda de armazem!\nVai começar a transportar o produto!\n");
		estado_drone++;
		
	}
	
	if(estado_drone==4){
		if(drone_to_encomenda_destino(drone_escolhido)==1){;
			estado_drone++;
		}
		init();
		sem_close(sem_drone);
		sem_wait(sem_esta);
		mp->total_enc_entregue += 1;
		sem_post(sem_esta);
		sem_close(sem_esta);
	}
	if(estado_drone==5){
		if(drone_retorno_base(drone_escolhido)==1){
			estado_drone = 1;
		}
		else if(drone_retorno_base(drone_escolhido)==2){
			estado_drone = 2;
		}
	}


	


	sleep(20);
	pthread_exit(NULL);
}

//
void envia_abastecimento(int id, Armazem *armazem){
	Mq abastece;
	
	while(1){
		abastece.mtype = 20181;
		int i = rand() % n_produtos;
		switch(i){
		case 1: strcpy(abastece.encomenda.produto->nome,armazem[id].produto[0].nome);
				abastece.encomenda.produto->quantidade = cf.quant_prod;
		break;
		case 2: strcpy(abastece.encomenda.produto->nome,armazem[id].produto[1].nome);
				abastece.encomenda.produto->quantidade = cf.quant_prod;
		break;
		case 3: strcpy(abastece.encomenda.produto->nome,armazem[id].produto[2].nome);
				abastece.encomenda.produto->quantidade = cf.quant_prod;
		break;

	}

		msgsnd(mq_id, &abastece, sizeof(Mq)-sizeof(long), 0);
		//Escreve no log
		FILE *log = fopen("log.txt", "a+");
			if (log == NULL){
	       	 		printf("Não foi possível abrir ficheiro de log");
	       	 		exit(0);
			}
		char abastecimento[20];
		time_t tem_abastecimento=time(0);
		struct tm *timeinfo;
		timeinfo=gmtime(&tem_abastecimento);
		strftime(abastecimento,sizeof(abastecimento),"%H:%M:%S",timeinfo);
		fprintf(log,"Armazem %s recebeu novo stock às:%s\n",armazem[id].nome,abastecimento);
		printf("Armazem %s recebeu novo stock às:%s\n",armazem[id].nome,abastecimento);
		fclose(log);

		
	}

}

Encomenda faz_encomenda(char comando[BUF_SIZE]){
	//printf("\nEntrou na função faz encomenda!!\n");
	char* token;
	Encomenda *pedido = (Encomenda*) malloc(sizeof(Encomenda));
	int j;
	int quant;
	int x,y;
	int i=0;
    token = strtok(comando, " ");
    //printf("Token 1 (Supposed to be ORDER): %s", token);
    	while (token != NULL) {
       		token = strtok(NULL, " ");
			i++;
			if(i==3){
				for(j=0;j<n_produtos;j++){
					if(strcmp(token,cf.tipoProduto[j])==0){
						strcpy(pedido->produto->nome,cf.tipoProduto[j]);
						//printf("\n Nome do produto: %s\n", pedido->produto->nome);
						}
					}
				}
			if(i==4){
				quant=atoi(token);
				pedido->produto->quantidade=quant;
				//printf("Quantidade do produto: %d\n", pedido->produto->quantidade);

			}
			if(i==6){
				x=atoi(token);
				pedido->destino.x=x;
				//printf("DestinoX do produto: %fn", pedido->destino.x);

				}
			if(i==7){
				y=atoi(token);
				pedido->destino.y=y;
				//printf("DestinoY do produto: %f\n", pedido->destino.y);
				}
    		}
		return *pedido;



}

int ler_comando(char comando[BUF_SIZE]){
	char* token;
	int i=0;
	int j;
	int x,y;
	int count=0;
    token = strtok(comando, " ");
	if(strcmp(token,"ORDER")!=0){
		return -1;
	}
    	while (token != NULL) {
       		token = strtok(NULL, " ");
			i++;
			if(i==2 && strcmp(token,"prod:")!=0){
				return -1;
				}
			if(i==3){
				for(j=0;j<n_produtos;j++){
					if(strcmp(token,cf.tipoProduto[j])==0){
						count++;
						}
					}
				if(count==0){
						return -1;
						}
				}
			if(i==5 && strcmp(token,"to:")!=0){
				return -1;	
				}
			if(i==6){
				x=atoi(token);
				if(x>cf.max.x || x<0){
					return -1;
					}
				}
			if(i==7){
				y=atoi(token);
				if(y>cf.max.y || y<0){
					return -1;
					}
				}
    		}
		return 0;


}

int verifica_pedido(Encomenda pedido){
	//printf("\nENTROU NA VERIFICA PEDddIDOO\n");
	int i,id_arm;
	for(id_arm=0;id_arm<cf.n_armazens;id_arm++){
		//printf("Entrou no For 1\n");
		
		
		for(i=0;i<n_produtos;i++){
		//printf("Entrou no For 2\n");
			if(strcmp(mp->arm[id_arm]->produto[i].nome,pedido.produto->nome)==0){
				
				printf("Armazem com produto: %s\n", mp->arm[id_arm]->nome);
				if(mp->arm[id_arm]->produto[i].quantidade>=pedido.produto->quantidade){ //Se o armazem tiver mais 
					//printf("Armazem %s tem %d produtos do tipo %s, ou seja, maior do que o pedido %d\n", mp->arm[id_arm]->nome, mp->arm[id_arm]->produto[i].quantidade, mp->arm[id_arm]->produto[i].nome,pedido.produto->quantidade);
					return 0;
					}
					else{
						continue;
						}
				}
				else{
					continue;
				}
			}
			
	}

	return -1;

}


int avalia_distancia(Encomenda pedido){
	int i,id_arm;
	int ativos=0;
	Armazem com_prod[MAX_ARMAZENS];
	int k=0;
	int j=0;
	int dist;
	int dist_armazem;
	int dist_destino;
	int menor=1000000;
	int id_drone;
	int l;
	int prod,arm_prox;

	//Armazens com produto e quantidade necessaria
	for(id_arm=0;id_arm<cf.n_armazens;id_arm++){
		for(i=0;i<n_produtos;i++){
			if(strcmp(mp->arm[id_arm]->produto[i].nome,pedido.produto->nome)==0){
				if(mp->arm[id_arm]->produto[i].quantidade>=pedido.produto->quantidade){
					strcpy(com_prod[j].nome,mp->arm[id_arm]->nome);
					com_prod[j].local.x = mp->arm[id_arm]->local.x;
					com_prod[j].local.y = mp->arm[id_arm]->local.y;
					strcpy(com_prod[j].produto->nome, mp->arm[id_arm]->produto->nome);
					com_prod[j].produto->quantidade = mp->arm[id_arm]->produto->quantidade;
					prod=i;

					j++;
				}
			}
		}
	}
	l = j;
	//printf("Vai para o for !\n");
	//Avaliar distancia
	for(i=0;i<cf.n_drones;i++){
		//printf("Entrou no 1 for\n");
		//printf("Drone %d - Estado %d  ", i, drones_ativo[i].estado);
		if(drones_ativo[i].estado==0){ //Drone Inativo
			
			for(k=0;k<j;k++){
				
				dist_armazem=distance(drones_ativo[i].posicao.x,drones_ativo[i].posicao.y,com_prod[k].local.x,com_prod[k].local.y);
				//printf("Distancia de (%f, %f) até (%f, %f) = %d\n",drones_ativo[i].posicao.x,drones_ativo[i].posicao.y,com_prod[k].local.x,com_prod[k].local.y, dist_armazem);
				dist_destino=distance(com_prod[k].local.x,com_prod[k].local.y,pedido.destino.x,pedido.destino.y);
				//printf("Distancia de (%f, %f) até (%f, %f) = %d",com_prod[k].local.x,com_prod[k].local.y,pedido.destino.x,pedido.destino.y, dist_destino);
				dist=dist_armazem+dist_destino;
				//printf("\nDistancia total do Drone %d até ao armazem =  %d\n", i, dist);
				if(dist<menor){
					menor=dist;
					//printf("Menor: %d", menor);
					id_drone=i;
					//printf("Id Drone: %d", id_drone);
					drones_ativo[i].destino.x = com_prod[k].local.x;
					drones_ativo[i].destino.y = com_prod[k].local.y;
					drones_ativo[i].destino2.x = pedido.destino.x;
					drones_ativo[i].destino2.y= pedido.destino.y;
					arm_prox=k;
					}
				}
			}
		else{ //Drone ativo
			ativos++;
			continue;
		}	
	}
	printf("\nMenor distância encontrada: %d\n", menor);
	//Enquanto nao houver drones livres
	while(ativos==20){
		for(i=0;i<cf.n_drones;i++){
		if(drones_ativo[i].estado==FALSE){ //Drone Inativo
			ativos--;
			for(k=0;k<l;k++){
				dist_armazem=distance(drones_ativo[i].posicao.x,drones_ativo[i].posicao.y,com_prod[k].local.x,com_prod[k].local.y);
				dist_destino=distance(com_prod[k].local.x,com_prod[k].local.y,pedido.destino.x,pedido.destino.y);
				dist=dist_armazem+dist_destino;
				if(dist<menor){
					menor=dist;
					id_drone=i;
					drones_ativo[i].destino.x = com_prod[k].local.x;
					drones_ativo[i].destino.y = com_prod[k].local.y;
					}
				}
			}
		}
	}
	//Reservar produto
	mp->arm[arm_prox]->produto[prod].quantidade=mp->arm[arm_prox]->produto[prod].quantidade - pedido.produto->quantidade;
	mp->enc_atribuidas=mp->enc_atribuidas+1;
	mp->prod_carregadas += pedido.produto->quantidade;
	mp->temp_medio += cf.unidade_tempo * menor;
	
	return id_drone;


}

void Central(){
	char *token;
	char nome_encomenda[BUF_SIZE];
	pthread_t drones[cf.n_drones];	
	char command[BUF_SIZE],command_log[BUF_SIZE], command2[BUF_SIZE];
	int id_drone;
	int k=0;
	int id[cf.n_drones];
	int ORDER_NO=0;
	int i;//menor,j,id_drone,dist;
	//menor=MAX;


	//CRIAR NAMED PIPE
	unlink(PIPE_NAME);
	if (mkfifo(PIPE_NAME, O_CREAT|O_EXCL|0600) == -1  && (errno!= EEXIST)){
		perror("Failed to create pipe");
		exit(0);
	}

	//CRIAR THREADS DRONE
	for(i=0;i<cf.n_drones;i++){
		id[i]=i;
		if(pthread_create(&drones[i],NULL,drone,&id[i]) != 0){
			perror("Erro na criação das threads");
			exit(1);
		}
	}
	i = 0;
	while(done==1){
		if ((fd = open(PIPE_NAME, O_RDONLY)) < 0) {
		perror("Cannot open pipe for reading.");
		exit(0);
	}

		read(fd,command,BUF_SIZE);
		close(fd);

		strcpy(command_log,command);
		strcpy(command2, command);
		//printf("Command: %s \n", command);
		//printf("command2 : %s\n", command2);
		//printf("Log: %s\n", command_log);

		//Verifica se comando esta correto
		if(ler_comando(command)==-1){
			if ((fd = open(PIPE_NAME, O_WRONLY)) < 0) {
				perror("Cannot open pipe for writing.");
				exit(0);
			}
			//Escreve no log comando descartado
			FILE *log = fopen("log.txt", "a+");
			if (log == NULL){
	       	 		printf("Não foi possível abrir ficheiro de log");
	       	 		exit(0);
			}
			fprintf(log,"Comando descartado:");
			fprintf(log,"%s\n",command_log);
			write(fd,"Erro no comando!",BUF_SIZE);
			fclose(log);
			close(fd);
			
		}
		else if(ler_comando(command)==0){
			if ((fd = open(PIPE_NAME, O_WRONLY)) < 0) {
				perror("Cannot open pipe for writing.");
				exit(0);
			}
			write(fd,"Comando entregue!",BUF_SIZE);
			close(fd);
			printf("\nOrdem recebida!!\n");
			ORDER_NO++;
			k++;
			pedidos[k].num = ORDER_NO;
			token = strtok(command_log, " ");
    		while(token != NULL){
    			token = strtok(NULL, " ");
    			i++;
    			if (i == 1){
    				strcpy(nome_encomenda, token);
    			}
    		}

			//Escreve no log ordem recebida
			FILE *log = fopen("log.txt", "a+");
				if (log == NULL){
	       	 		printf("Não foi possível abrir ficheiro de log");
	       	 		exit(0);
				}
			char encomenda[20];
			time_t tem_encomenda=time(0);
			struct tm *timeinfo;
			timeinfo=gmtime(&tem_encomenda);
			strftime(encomenda,sizeof(encomenda),"%H:%M:%S",timeinfo);
			fprintf(log,"Encomenda nº%d (%s) recebida pela central às: %s. \n",ORDER_NO,nome_encomenda,encomenda);
			printf("Encomenda nº%d (%s) recebida pela central às: %s. \n",ORDER_NO,nome_encomenda,encomenda);
			fclose(log);


			//Criar pedido
			pedidos[k]=faz_encomenda(command2);
			pedidos[k].num=ORDER_NO;
			//Verificar produtos no armazem
			//Caso existe
		
			printf("\n"); 
			init();
			sem_close(sem_drone);
			sem_wait(sem_esta);
			if( (verifica_pedido(pedidos[k]))==0){
				sem_post(sem_esta);
				sem_close(sem_esta);
				
				//id do drone mais proximo do destino
				//printf("ENTROU NO IF\nVAI ENTRAR NA AVALIA DISTANCIA\n");
				init();
				sem_close(sem_drone);
				sem_wait(sem_esta);
				id_drone=avalia_distancia(pedidos[k]);
				sem_post(sem_esta);
				sem_close(sem_esta);
				printf("\nID DRONE ESCOLHIDO: %d\n", id_drone);
				pthread_mutex_lock(&mutex);
				drones_ativo[id_drone].estado=TRUE;

				pthread_cond_broadcast(&cond);
				pthread_mutex_unlock(&mutex);
				drones_ativo[id_drone].enc_num = pedidos[k].num;

				//Escreve no log
				FILE *log = fopen("log.txt", "a+");
				if (log == NULL){
	       	 		printf("Não foi possível abrir ficheiro de log");
	       	 		exit(0);
				}
				timeinfo=gmtime(&tem_encomenda);
				strftime(encomenda,sizeof(encomenda),"%H:%M:%S",timeinfo);
				fprintf(log,"Encomenda nº%d (%s) enviada ao drone %d às: %s. \n",ORDER_NO,nome_encomenda,id_drone,encomenda);
				printf("Encomenda nº%d (%s) enviada ao drone %d às: %s. \n",ORDER_NO,nome_encomenda,id_drone,encomenda);
				fclose(log);
				init();
				sem_close(sem_drone);
				sem_wait(sem_esta);
				mp->enc_atribuidas=mp->enc_atribuidas+1;
				sem_post(sem_esta);
				sem_close(sem_esta);
			}
			//Caso não exista ou não haja quantidade suficiente
			else if(verifica_pedido(pedidos[k])==-1){
				sem_post(sem_esta);
				sem_close(sem_esta);
				suspenso++;
				pedidos_suspensos[suspenso]=pedidos[k];
				//Escreve no log
				FILE *log = fopen("log.txt", "a+");
				if (log == NULL){
	       	 		printf("Não foi possível abrir ficheiro de log");
	       	 		exit(0);
				}
				timeinfo=gmtime(&tem_encomenda);
				strftime(encomenda,sizeof(encomenda),"%H:%M:%S",timeinfo);
				fprintf(log,"Encomenda %s-%d suspensa por falta de stock às:%s. \n",nome_encomenda,ORDER_NO,encomenda);
				printf("Encomenda %s-%d suspensa por falta de stock às:%s. \n",nome_encomenda,ORDER_NO,encomenda);
				fclose(log);
			}
		}
	}

	
	

	//espera ate os drones terminarem
	for(i=0;i<cf.n_drones;i++){
		pthread_join(drones[i],NULL);
	}
}


void terminar(){
	int i;
	done=0;
	FILE *log = fopen("log.txt", "a+");
	if (log == NULL){
	        printf("Não foi possível abrir ficheiro de log");
	        exit(0);
	}
	
	for(i=1;i<suspenso+1;i++){
		fprintf(log,"Encomenda não tratada: Produto:%s, Quantidade:%d para o local:%f, %lff",pedidos_suspensos[suspenso].produto->nome,pedidos_suspensos[suspenso].produto->quantidade,pedidos_suspensos[suspenso].destino.x,pedidos_suspensos[suspenso].destino.y);
	}
	//Termina processos armazem
	for(i=1;i<cf.n_armazens+1;i++){
		if (kill(armazens[i], SIGKILL) == -1){
			perror("Failed to send the KILL signal");
		}
		else{
			//tempo final armazem
			char fin_arm[20];
			time_t fim_arm=time(0);
			struct tm *timeinfo;
			timeinfo=gmtime(&fim_arm);
			strftime(fin_arm,sizeof(fin_arm),"%H:%M:%S",timeinfo);
			fprintf(log,"Armazem[%ld] terminou as:%s\n",(long)armazens[i],fin_arm);
			printf("Armazem[%ld] terminou as:%s\n",(long)armazens[i],fin_arm);
		}
	}


	

	// Elimina os semáforos
	int error = 0;
	if (sem_close(sem_esta) == -1){
		error = errno;
	}
	if (sem_unlink("SEM_ESTA") == -1){
		error=errno;
	}	
	if (error){ /* set errno to first error that occurred */
		errno = error;
	}

	error=0;
	if (sem_close(sem_drone) == -1){
		error = errno;
	}
	if (sem_unlink("SEM_DRONE") == -1){
		error=errno;
	}	
	if (error){ /* set errno to first error that occurred */
		errno = error;
	}

	//terminar memoria partilhada
	error = 0;
	if (shmdt(&mp) == -1){
		error = errno;
	}
	if ((shmctl(shmid, IPC_RMID, NULL) == -1) && !error){
		error = errno;
	}
	if(error){
		errno = error;
	}

	//Terminar pipe
	if (unlink(PIPE_NAME) == -1){
		perror("Failed to remove pipe");
	}
	close(fd);

	//Terminar message queue
	if(msgctl(mq_id, IPC_RMID, NULL)==-1){
		perror("Failed to eliminate message queue.");
	}

	//tempo final
	char final[20];
	time_t fim=time(0);
	struct tm *timeinfo;
	timeinfo=gmtime(&fim);
	strftime(final,sizeof(final),"%H:%M:%S",timeinfo);
	fprintf(log,"Evento terminou as:%s\n",final);
	printf("Evento terminou as:%s\n",final);
	fclose(log);

	//termina central
	if (kill(id_centr, SIGKILL) == -1){
		perror("Failed to send the KILL signal");
	}
	exit(0);

}


int main(){
	int status;
	int i;
	pid_t my_armazem;

	//Ignora os sinais
	if(sigfillset(&set_bloqueado)==-1){
		perror("Failed to create set.");
	}
	if(sigdelset(&set_bloqueado,SIGINT)==-1){
		perror("Failed to delete SIGINT from set.");
	}
	if(sigdelset(&set_bloqueado,SIGUSR1)==-1){
		perror("Failed to delete SIGUSR1 from set.");
	}
	if(sigprocmask(SIG_BLOCK,&set_bloqueado,NULL)==-1){
		perror("Failed to block set.");
	}
	if ((sigemptyset(&intmask) == -1) || (sigaddset(&intmask, SIGINT) == -1)){
   		perror("Failed to initialize the signal mask");
	}
	if (sigprocmask(SIG_BLOCK, &intmask, NULL) == -1){
       		perror("Failed to block SIGINT.");
	}


	//Cria ficheiro log
	FILE *log = fopen("log.txt", "w+");
	if (log == NULL){
	        printf("Não foi possível criar ficheiro de log");
	        exit(0);
	}
	//tempo inicial
	char inicial[20];
	time_t inicio=time(0);
	struct tm *timeinfo;
	timeinfo=gmtime(&inicio);
	strftime(inicial,sizeof(inicial),"%H:%M:%S",timeinfo);
	fprintf(log,"Evento ocorreu as:%s\n", inicial);
	printf ("Evento ocorreu as:%s\n", inicial);


	init();
	ler_config(armazem);


	//CRIAÇÃO SHARED MEMORY
	if ((shmid = shmget(IPC_PRIVATE, sizeof(mem_partilhada), IPC_CREAT | 0777)) < 0){
		perror("Erro ao criar a memoria partilhada\n");
		exit(1);
 	}
 	mp = (mem_partilhada*)shmat(shmid, NULL, 0);
	for(i=0;i<cf.n_armazens;i++){
		mp->arm[i]=&armazem[i];
	}
	mp->enc_atribuidas=0;
	mp->prod_carregadas=0;
	mp->total_enc_entregue=0;
	mp->total_prod_entregue=0;
	mp->temp_medio=0;
 	
 	//AO RECEBER ESTE SINAL IMPRIME ESTATISTICAS
	signal(SIGUSR1, esta_handler);
	
	//CRIAR MESSAGE QUEUE
	mq_id = msgget(IPC_PRIVATE, IPC_CREAT|0777);
	if (mq_id < 0){
		perror("Message Queue\n");
		exit(0);
	}

	//CRIA CENTRAL
	getid = fork();
	if(getid != 0){
   		id_centr = getpid();
 		printf("Central criada[%ld]\n",(long)id_centr);
 		Central();
		exit(0);
	}


	//CRIA PROCESSOS ARMAZEM
	for(i = 1; i < cf.n_armazens+1; i++){
		if((armazens[i] = fork()) == 0){
			my_armazem=getpid();
			//tempo do armazem
			char tem_armazem[20];
			time_t intervalo=time(0);
			timeinfo=gmtime(&intervalo);
			strftime(tem_armazem,sizeof(tem_armazem),"%H:%M:%S",timeinfo);
			fprintf(log,"Armazem[%ld] criado as:%s\n",(long)my_armazem,tem_armazem);
			printf("Armazem[%ld] criado as:%s\n",(long)my_armazem,tem_armazem);
			sem_close(sem_drone);
			sem_wait(sem_esta);
			cria_armazem(i,&armazem[i]);
			sleep(cf.freq_abastecimento);
			while(done != 0){
				envia_abastecimento(i, &armazem[i]);
				sleep(cf.freq_abastecimento);
			}
			sem_post(sem_esta);
			
			exit(0);
	   		}

		}
		sem_close(sem_esta);
	fclose(log);

	//Desbloqueia SIGINT
	if (sigprocmask(SIG_UNBLOCK, &intmask, NULL) == -1){
       		perror("Failed to unblock SIGINT.");
	}
	signal(SIGINT,terminar);

	

	//Espera que todos os armazens terminem
	do {
  		status = wait(0);
    		if(status == -1 && errno != ECHILD) {
        		perror("Erro na espera.");
        		abort();
    		}
	} while (status > 0);



	



	return 0;


}



