/*  File: dec.c ******************************************************/
/*  Author: Marcelo Melo Linck ***************************************/
/*  Version: 1.0 *****************************************************/
/*  Date: 11/05/2013 *************************************************/
/*  marcelo.linck@acad.pucrs.br  *************************************/
/*	Description: This software sorts a disorganized list of numbers **/
/*  into a crescent order using the merge sort and the Division and **/
/*  Conquer parallel programming model.                             **/

#include <mpi.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <math.h>

void Merge(int array[], int begin, int mid, int end) //Função que organiza de forma crescente dois vetores ordenados
{
    int ib = begin;
    int im = mid;
    int j;
    int size = end-begin;
    int b[size];
					
    for (j = 0; j < (size); j++)
	{
        if (ib < mid && (im >= end || array[ib] <= array[im])) 
		{
            b[j] = array[ib]; 
            ib = ib + 1;
        }
        else
		{
            b[j] = array[im];
            im = im + 1;
        }
    }
    for (j=0, ib=begin; ib<end; j++, ib++)
		array[ib] = b[j];
}

void Sort(int array[], int begin, int end)	//Função de ordenação recursiva de chamada da Merge Sort
{
	int mid;
	if (begin == end)
		return;
	if (begin == end - 1)
		return;
	mid = (begin + end)/2;
	Sort(array, begin, mid);
	Sort(array, mid, end);
	Merge(array, begin, mid, end);
}

int main(int argc, char **argv)	
{
	int np, id, i;	
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
			printf ("Uso: %s <numberofelements>", argv[0]);
		MPI_Finalize();
		return 1;
	}

	int 	input_size = atoi(argv[1]),				//tamanho de entrada
			target_size = input_size/np,			//Define o tamanho de cada vetor no ultimo nivel
			level=0,
			actual_size,
			* input_vec = (int *) malloc(input_size*sizeof(int)),
			* result_vec = (int *) malloc(input_size*sizeof(int)),
			next;

	//################################## leitura do arquivo de entrada
	if (!id) 			//Apenas o nodo 0 da arvore é capaz de ver a lista de entrada
	{					//os outros apenas recebem pedaços dela
		if ((file = fopen("arquivo.txt", "r")) == NULL) {
			printf("\n\nFile failed to open file. Check the file existance or the directory permissions.\n");
			exit(1);
		}
		while (i!=input_size-1) {
			fgets(line, sizeof(line), file);
			input_vec[i] = atoi(line);
			i++;
		}
		fclose(file);
	}

	//################################## se for sequencial
	
	if(np==1) {
		printf("\n\n\t\033[1mSequencial Mode!\033[0m\n\tTotal Elements: %d\n\n",input_size);
		gettimeofday(&ti, NULL); 		//Captura tempo inicial
		Sort(input_vec, 0, input_size);	//Realiza a ordenação de todo o vetor de entrada
		gettimeofday(&tf, NULL);		//Captura tempo inicial
		result_vec = input_vec;			//Prepara para impressão
	}

	//################################## se for paralelo
	else {
	//################################## ID==0
		if(!id) {
			printf("\n\n\t\033[1mParallel Mode!\033[0m\n\tDepth: %d\n\tTotal Elements: %d\n\tElements per node: %d\n\n",
																(int)log2(input_size/target_size), input_size, target_size);
			gettimeofday(&ti, NULL);		//Captura tempo inicial
			actual_size = input_size;		//Tamanho do vetor atual é o tamanho de entrada
			while(actual_size != target_size){		//Até atingir o tamanho necessario, este nodo vai dividindo seu vetor pela metade
				level=(int)log2(input_size/actual_size);
				actual_size=actual_size/2;
				next = (int)id+(int)pow(2,level);	//O nodo que recebe sua metade é calculado pela formula ao lado
				MPI_Send(input_vec+actual_size, actual_size, MPI_INT, next, 0, MPI_COMM_WORLD);		//Envio da metade direita do vetor
			}
			for(i=0;i<actual_size;i++) result_vec[i]=input_vec[i];		//transfere os valores do vetor de entrada para o vetor resultado
			Sort(result_vec, 0, actual_size);				//Ordena
			while(actual_size != input_size){				//Recebe os vetores ordenados de seus filhos ate atingir o tamanho de entrada
				level = (int)log2(input_size/actual_size);	//Formula para calcular o level
				next = (int)id+(int)pow(2,(level-1));		//Calcula de quem receber
				MPI_Recv(result_vec+actual_size, input_size, MPI_INT, next, 0, MPI_COMM_WORLD, &status);
				Merge(result_vec, 0, actual_size, actual_size*2);	//Junta e organiza os dois vetores
				actual_size = actual_size*2;			//O tamanho aumenta o dobro
			}
			gettimeofday(&tf, NULL);				//Captura tempo final (toda a execução acaba quando este nodo completa a organização)
		}

	//################################## outros
		else{
			int start_level, father;
			actual_size=0;
			MPI_Recv(result_vec, input_size, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status); //Recebe vetor do pai
			MPI_Get_count(&status, MPI_INT, &actual_size);				//Verifica numero de elementos recebidos
			start_level=(int)log2(input_size/actual_size);				//Calcula level de inicio
			father = status.MPI_SOURCE;									//Verifica pai
			do{
				level=(int)log2(input_size/actual_size);				//Calcula level atual
				if(actual_size == target_size) break;					//Se o tamanho atual é o tamanho necessario para ordenar sai do loop
				else {
					next = (int)id+(int)pow(2,level);					//Senao, calcula ID do filho, divide e envia vetor
					actual_size=actual_size/2;
					MPI_Send(result_vec+actual_size, actual_size, MPI_INT, next, 0, MPI_COMM_WORLD);
				}
			}while(1);
			Sort(result_vec, 0, actual_size);		//Apos dividir todas as vezes necessarias, conquista... ou seja, ordena.
			while(level!=start_level){				//Enquanto o nivel atual nao for o nivel que o nodo iniciou...
				level = (int)log2(input_size/actual_size);	
				next = (int)id+(int)pow(2,(level-1));		//Nodo calcula e recebe o vetor ordenado de seu filho
				MPI_Recv(result_vec+actual_size, input_size, MPI_INT, next, 0, MPI_COMM_WORLD, &status);
				Merge(result_vec, 0, actual_size, actual_size*2);		// Junta
				actual_size = actual_size*2;				//Atualiza tamanho e level
				level--;
			}									//Por fim, nodo envia seu vetor ordenado e organizado ao pai
			MPI_Send(result_vec, actual_size, MPI_INT, father, 0, MPI_COMM_WORLD);	
		}
			
	}

	//################################## Impressão dos dados

	if (!id) 		//Apenas o nodo 0, que possui o vetor completo ordenado, tem a prioridade de salvar os dados
	{			
				//E de imprimir os tempos
		printf("\tTime: %ds%dms%dus\n\n", abs((int) (tf.tv_sec - ti.tv_sec)), 
											  abs((int) (tf.tv_usec - ti.tv_usec)/1000),
											  abs((int) (tf.tv_usec - ti.tv_usec)%1000));

		file = fopen("vetor_ordenado.txt", "w");		
		for(i=0; i<input_size; i++)
		{
			sprintf(line, "%d\n", result_vec[i]);
			fputs(line, file);
		}
		fclose(file);
	}
	MPI_Finalize();				//Finaliza o MPI
}
