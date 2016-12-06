/*  File: fpar_oddeven.c *********************************************/
/*  Author: Marcelo Melo Linck ***************************************/
/*  Version: 1.1 *****************************************************/
/*  Date: 11/27/2013 *************************************************/
/*  marcelo.linck@acad.pucrs.br  *************************************/
/*	Description: This program sorts a vector using the Odd Even ******/
/*  Transposition Sort applying the parallel programming model: ******/
/*  Parallel Phases.											******/

#include <mpi.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <math.h>

#define PAR 0
#define IMPAR 1

void log_results(int array[], int max)				//Funcao para impressao dos resultados
{    
    int i;
    FILE * file;
    char line[10];
    file = fopen("vetor_ordenado.txt", "a");
    for(i=0; i<max; i++){
        sprintf(line, "%d\n", array[i]);
        fputs(line, file);
    }
    fclose(file);
}

int OddEven_Sort(int array[], int n)		// Funcao de ordenação - Uma variação do BubbleSort
{
	int ordenado=0;
	int i, aux;
	int changed=0;
	while(!ordenado){					//Enquanto o vetor nao estiver ordenado	
		ordenado=1;
		for(i=1;i<n-1;i+=2){			// Somente os impares
			if(array[i]>array[i+1]){	//Se o proximo for maior que o atual, troca. Contador vai de 2 em 2.
				aux = array[i];
				array[i] = array[i+1];
				array[i+1] = aux;
				ordenado=0;
				changed=1;				
			}
		}
		for(i=0;i<n;i+=2){				//Somente os pares
			if(array[i]>array[i+1]){	
				aux = array[i];
				array[i] = array[i+1];
				array[i+1] = aux;
				ordenado=0;
				changed=1;
			}
		}
	}
	return changed;				//Se houve alguma troca, retorna 1, senao, vetor ordenado, retorna 0
}

int main(int argc, char **argv)	
{
	int np, id, i=0;	
	struct timeval ti, tf;
	FILE * file;
	char line[10];
	MPI_Status status;

	MPI_Init(&argc, &argv);					//Inicia MPI
	MPI_Comm_rank(MPI_COMM_WORLD, &id);		//Captura ID do processo
	MPI_Comm_size(MPI_COMM_WORLD, &np);	  	//Captura numero de processos sendo usados

	if (argc<2)			
	{
		if (!id)
			printf ("Uso: %s <numberofelements> <buffer:padrao=1/3*numberofelements/np>\n", argv[0]);
		MPI_Finalize();
		return 1;
	}

	int 	input_size = atoi(argv[1]),				//tamanho de entrada
			target_size = input_size/np,			//tamanho do vetor de cada processo
			* input_vec = (int *) malloc(input_size*sizeof(int)),	
			* result_vec,
			extra_size;			

	if (argc==2)			//Se o buffer nao for informado, buffer é 1/3 do vetor do processo
		extra_size = target_size/3;
	else 					//Senao, buffer é o valor informado
		extra_size = atoi(argv[2]);

	if (extra_size%2 != 0) extra_size--;	//Testes foram feitos e mostraram uma melhor performance com um buffer par
	
	if(extra_size>input_size){
		if(!id) printf("\n\nBuffer nao pode ser maior que Total de elementos \nsobre numero de processadores (total/np).\n\n");
		MPI_Finalize();
		exit(1);
	}

	remove("vetor_ordenado.txt");			//Remove arquivo já existente (se houver)
	
	//################################## leitura do arquivo de entrada
	if ((file = fopen("arquivo.txt", "r")) == NULL) {
		if(!id) printf("\n\nFile failed to open file. Check the file existance or the directory permissions.\n");
		MPI_Finalize();
		exit(1);
	}
	while (i!=input_size-1) {
		fgets(line, sizeof(line), file);
		input_vec[i] = atoi(line);
		i++;
	}
	fclose(file);

	//################################## se for sequencial
	
	if(np==1) {
		printf("\n\n\t\033[1mSequencial Mode!\033[0m\n\tTotal Elements: %d\n\n",input_size);
		gettimeofday(&ti, NULL);				//Captura tempo inicial
		OddEven_Sort(input_vec, input_size);	//Ordena
		gettimeofday(&tf, NULL);				//Captura tempo final
		log_results(input_vec, input_size);		//Imprime no arquivo de saida 
	}

	//################################## se for paralelo
	else {
		if(!id) printf("\n\n\t\033[1mParallel Mode!\033[0m\n\tTotal Elements: %d\n\tElements per node: %d\n\tBuffer: %d\n\n",
																input_size, target_size, extra_size);
						//vetor do vetor do processo é o tamanho calculado anteriormente, e a concatenação do tamanho do buffer e cada lado
		result_vec = (int *) malloc((extra_size+target_size+extra_size)*sizeof(int));	
		int	actual_size = extra_size+target_size+extra_size,
			j=0,
			changed=0,
			changed_ans=1,
			mode=PAR;
		for(i=0;i<extra_size;i++) result_vec[i] = 0;		//primeiros valores do buffer sao 0
		for(i=actual_size-extra_size;i<actual_size;i++) result_vec[i] = 200000;		//ultimos valores do buffer sao 200000
		for(i=extra_size, j=id*target_size;i<actual_size-extra_size, j<id*target_size+target_size;i++, j++) 
			result_vec[i] = input_vec[j];	// o resto dos valores do vetor são equivalentes ao vetor de entrada de acordo com a ID do processo

		gettimeofday(&ti, NULL);		//captura do tempo inicial
		OddEven_Sort(result_vec+extra_size, target_size);		//ordenação inicial
		do{
			if(mode==PAR){		//Se for modo PAR
				if(id%2==0){	//Para ID pares, manda maiores elementos e recebe menores elementos 
					MPI_Send(result_vec+actual_size-extra_size*2, extra_size, MPI_INT, id+1, 0, MPI_COMM_WORLD);
					MPI_Recv(result_vec+actual_size-extra_size, extra_size, MPI_INT, id+1, 0, MPI_COMM_WORLD, &status);
					changed = OddEven_Sort(result_vec+extra_size, target_size+extra_size);
				}		//Ordena vetor com o buffer recebido 
				else{		//para ID impar, recebe maiores e manda menores elementos
					MPI_Recv(result_vec, extra_size, MPI_INT, id-1, 0, MPI_COMM_WORLD, &status);
					MPI_Send(result_vec+extra_size, extra_size, MPI_INT, id-1, 0, MPI_COMM_WORLD);
					changed = OddEven_Sort(result_vec, target_size+extra_size);
				}       //Ordena vetor com o buffer recebido
			}
			else{		//Se for modo IMPAR 
				if(id%2==0){	
					if(id!=0){	//Para ID pares e nao 0, recebe maiores e manda menores elementos
						MPI_Recv(result_vec, extra_size, MPI_INT, id-1, 0, MPI_COMM_WORLD, &status);
						MPI_Send(result_vec+extra_size, extra_size, MPI_INT, id-1, 0, MPI_COMM_WORLD);
						changed = OddEven_Sort(result_vec, target_size+extra_size);
					}	//Ordena vetor com o buffer recebido
				}
				else {
					if(id!=np-1){ //Para ID impar e nao ultimo, manda maiores elementos e recebe menores elementos
						MPI_Send(result_vec+actual_size-extra_size*2, extra_size, MPI_INT, id+1, 0, MPI_COMM_WORLD);
						MPI_Recv(result_vec+actual_size-extra_size, extra_size, MPI_INT, id+1, 0, MPI_COMM_WORLD, &status);
						changed = OddEven_Sort(result_vec+extra_size, target_size+extra_size);
					}	//Ordena vetor com o buffer recebido
				}
			}
			if (mode == PAR) mode = IMPAR;	//troca de modo
				else mode = PAR;
			MPI_Allreduce(&changed, &changed_ans, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD); //soma todos as variaveis changed
		}while(changed_ans);		//se caso a soma seja 0, nenhum processo ordenou, logo, todos estao ordenados
		gettimeofday(&tf, NULL);		//Captura tempo final

		//################################## Impressão dos dados
		int ans=1;
		if(id!=0) MPI_Recv(&ans, 1, MPI_INT, id-1, 0, MPI_COMM_WORLD, &status);	//Se ele nao for o primeiro, recebe token para impressao
		log_results(result_vec+extra_size, target_size);			//Imprime
		if(id!=np-1) MPI_Send(&ans, 1, MPI_INT, id+1, 0, MPI_COMM_WORLD);	//Se nao for o ultimo, manda token para proximo

	}



	if (!id) 		//Apenas o nodo 0 
	{			
				//pode imprimir os tempos
		printf("\tTime: %ds%dms%dus\n\n", abs((int) (tf.tv_sec - ti.tv_sec)), 
											  abs((int) (tf.tv_usec - ti.tv_usec)/1000),
											  abs((int) (tf.tv_usec - ti.tv_usec)%1000));
	}
	MPI_Finalize();				//Finaliza o MPI
}
