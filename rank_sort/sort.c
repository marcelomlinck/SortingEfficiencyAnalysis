/*  File: sort.c *****************************************************/
/*  Author: Marcelo Melo Linck ***************************************/
/*  Version: 1.5 *****************************************************/
/*  Date: 10/10/2013 *************************************************/
/*  marcelo.linck@acad.pucrs.br  *************************************/

#include <mpi.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>

int *input_vec;			//Vetor utilizado para a leitura dos dados do arquivo
int *result_vec;		//Vetor resultado da função RANK SORT	
int *final_vec;			//Vetor final, inteiramente ordenado e pronto para impressao

void merge(int begin, int mid, int end) //Função de organização de dois vetores ordenados
{
    int ib = begin;
    int im = mid;
    int j;
    int size = end-begin;
    int b[size];
					//Fica pulando de lado a lado organizando de acordo com o menor valor
    for (j = 0; j < (size); j++)
	{
        if (ib < mid && (im >= end || final_vec[ib] <= final_vec[im])) 
		{
            b[j] = final_vec[ib]; 
            ib = ib + 1;
        }
        else
		{
            b[j] = final_vec[im];
            im = im + 1;
        }
    }
    for (j=0, ib=begin; ib<end; j++, ib++)
		final_vec[ib] = b[j];
}

void rank_sort(int start, int end)	//Função de ordenação RANK SORT
{
	int i, j, x;
	for(i=start; i<=end; i++)
	{
		x=0;
		for(j=start; j<=end; j++)
		{							     //Procura um numero menor para cada valor no vetor,
			if(input_vec[i]>input_vec[j])//caso ache, incrementa a posição, e por fim, insere em um novo vetor
			x++;
		}
		result_vec[x] = input_vec[i];
	}
}

int main(int argc, char **argv)
{
	int np, id, i=0;	
	struct timeval ti, tf;

	MPI_Init(&argc, &argv);			//Inicializa o MPI
	MPI_Comm_rank(MPI_COMM_WORLD, &id);   //Captura o numero do processo rodando, se 0-mestre, senao, escravo      
	MPI_Comm_size(MPI_COMM_WORLD, &np);	  //Captura o numero de processos rodando ao total	

	if (argc<2)				//Verifica a inicialização correta do programa
	{
		if (!id)
			printf ("Uso: %s <numberofelements>", argv[0]);
		MPI_Finalize();
		return 1;
	}

	int elements_per_task;	//Numero de elementos em cada tarefa, valor passado na chamada do programa

	int input_elements = atoi(argv[1]);		
	if(np>1) elements_per_task = (input_elements/(4*(np-1))); // Numero de elementos por tarefa = numero total/(4*numero de escravos)
	else elements_per_task = input_elements;	//Para o sequencial, numero de elementos = numero total		  

	input_vec = (int *) malloc(input_elements*sizeof(int));			//Alocação dinamica de memoria, alocando espaço para os vetores
	result_vec = (int *) malloc(elements_per_task*sizeof(int));
	final_vec = (int *) malloc(input_elements*sizeof(int));			

	char line[10];	
	FILE * file;				//Leitura dos valores do arquivo texto "arquivo.txt"
	if ((file = fopen("arquivo.txt", "r")) == NULL)
	{
		printf("\n\nFile failed to open file. Check the file existance or the directory permissions.\n");
		exit(1);
	}

	while (i!=input_elements-1)
	{
		fgets(line, sizeof(line), file);
		input_vec[i] = atoi(line);
		i++;
	}
	fclose(file);		//fim da leitura
	
	if(np==1)		//Caso seja sequencial...
	{
		printf("\n\n\t\033[1mSequencial Mode!\033[0m\n\tTotal Elements: %d\n", input_elements);
		gettimeofday(&ti, NULL);		//Função de captura do tempo inicial
		rank_sort(0, input_elements-1);	//Chamada da função de ordenação, sequencial ordena o vetor inteiro de uma vez.
		gettimeofday(&tf, NULL);		//Função de captura do tempo final
		final_vec = result_vec;
		
	}
	else		//Caso seja paralelo...
	{
		if (!id) printf("\n\n\t\033[1mParallel Mode!\033[0m\n\tN Slaves: %d\n\tTotal Elements: %d\n\tElements per task: %d",
														np-1, input_elements, elements_per_task);
		int tag=0, sender;
		
		MPI_Status status;

		if(!id)				
		{
			int times = input_elements/elements_per_task; //numero de tarefas a serem enviadas
			int slave, j;	
			int task[np-1];	 //vetor utilizado para guardar a posição inicial enviada para cada escravo
			int index = 0;	 //variável que auxilia no cálculo da posição de envio para o escravo
			int must_receive = 0; 

			gettimeofday(&ti, NULL); 

			for(slave=1; slave<np; slave++)  //Manda as primeiras tarefas para todo os escravos
			{								
				sender = elements_per_task*(slave-1);	//Utiliza a ID do escravo para o cálculo de sua posição de ordenação
				MPI_Send(&sender, 1, MPI_INT, slave, tag, MPI_COMM_WORLD);   //Função de envio do MPI
				task[slave-1]=elements_per_task*(slave-1);    //Guarga o valor da posição
				times--;
				must_receive++;
			}

			int last_av;
			i=0;

			while(must_receive)    //Enquanto for necessário receber dados, continua no while  
			{		//Função de recebimento MPI
				MPI_Recv(result_vec, elements_per_task, MPI_INT, MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &status); 
				must_receive--;
				slave = status.MPI_SOURCE;        //ID do slave que enviou a mensagem

				last_av = i;  //i é um contador que nunca é resetado, ele sempre guarda a ultima posição livre no vetor final
				for(j=0; i<(last_av+elements_per_task);j++, i++)
				{
					final_vec[i] = result_vec[j]; //insere o vetor organizado pelo escravo no vetor final
				}

				merge(0, i-elements_per_task, i); //organiza o vetor final de forma que todos os elementos fiquem em ordem crescente

				if(times)  //se existirem tarefas a serem feitas...
				{
					sender = (np-1+index)*elements_per_task;   //utiliza index, outro contador, para calular o valor a ser enviado
					MPI_Send(&sender, 1, MPI_INT, slave, tag, MPI_COMM_WORLD); //envia nova tarefa para o escravo
					task[slave-1]=(np-1+index)*elements_per_task;
					index++;
					times--;
					must_receive++;
				}
			}

			int informer=-1;	//Variável utilizada como parâmetro de envio. O valor -1 define o fim das tarefas para o escravo
			for(slave=1; slave<np; slave++)
						MPI_Send(&informer, 1, MPI_INT, slave, tag, MPI_COMM_WORLD);
						
			gettimeofday(&tf, NULL); //Função de captura do tempo final
		}
		else	// Caso não seja mestre, mas sim escravo, segue abaixo...
		{
			int start; //Posição a ser recebida pelo mestre para ordenação 
			do{
				MPI_Recv(&start, 1, MPI_INT, MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &status);
				if(start==-1) //Se for -1, não existem mais tarefas
					break;
				else //senão, ordena e retorna o vetor
				{
					rank_sort(start, start+elements_per_task-1);
					MPI_Send(result_vec, elements_per_task, MPI_INT, 0, tag, MPI_COMM_WORLD);
				}
			}while(1); 
		}
	}

	if (!id) // O mestre
	{	
		//Imprime a diferença entre os tempos capturados
		printf("\n\n\tTime: %ds%dms%dus\n\n", abs((int) (tf.tv_sec - ti.tv_sec)), 
											  abs((int) (tf.tv_usec - ti.tv_usec)/1000),
											  abs((int) (tf.tv_usec - ti.tv_usec)%1000));

		//Salva vetor ordenado em um arquivo texto - vetor_ordenado.txt
		file = fopen("vetor_ordenado.txt", "w");		
		for(i=0; i<input_elements; i++)
		{
			sprintf(line, "%d\n", final_vec[i]);
			fputs(line, file);
		}
		fclose(file);
	}
	MPI_Finalize();				//Finaliza o MPI
}
