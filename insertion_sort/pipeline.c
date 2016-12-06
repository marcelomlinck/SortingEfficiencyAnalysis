/*  File: pipeline.c *************************************************/
/*  Author: Marcelo Melo Linck ***************************************/
/*  Version: 1.3 *****************************************************/
/*  Date: 10/29/2013 *************************************************/
/*  marcelo.linck@acad.pucrs.br  *************************************/
/*  Description: This program sorts an N-size array of non repea- ****/
/*	ting values in crescent order using the INSERTION SORT algo-  ****/
/* 	rithm and the parallel processing model PIPELINE.             ****/

#include <mpi.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>

int *input_vec;			
int *result_vec;		

//#########################

void log_results(int max)
{    
    int i;
    FILE * file;
    char line[10];
    file = fopen("vetor_ordenado.txt", "a");
    for(i=0; i<max; i++){
        sprintf(line, "%d\n", result_vec[i]);
        fputs(line, file);
    }
    fclose(file);
}

int insertion_sort(int val, int i, int full)
{
	int aux, j, init_val;
	for(j=0;j<i;j++){
		if(val < result_vec[j]){
			aux = result_vec[j];
			result_vec[j] = val;
			val = aux;
		}
	}
	if (full) return val;
//	if (val > result_vec[i] && full) return val;
	else{
		result_vec[i] = val;
		return (-1);
	}
}

//######################### Declara variavel

int main(int argc, char **argv){
	int np, id, i=0;
	struct timeval ti, tf;
    int input_elements = atoi(argv[1]);

//######################### incia MPI

	MPI_Init(&argc, &argv);		
	MPI_Comm_rank(MPI_COMM_WORLD, &id);  	
	MPI_Comm_size(MPI_COMM_WORLD, &np);	  

//######################### verifica chamada do programa

	if (argc<2){
		if (!id)
			printf ("Uso: %s <numberofelements>", argv[0]);
		MPI_Finalize();
		return 1;
	}
	
//######################### lê arquivo de entrada

	int max = input_elements/np;
	result_vec=(int*)malloc(max*sizeof(int));
	
	if(!id) {
		input_vec=(int*)malloc(input_elements*sizeof(int));
		char line[10];	
		FILE * file;				
		if ((file = fopen("arquivo.txt", "r")) == NULL){
			printf("\n\nFile failed to open file. Check the file existance or the directory permissions.\n");
			exit(1);
		}
		while (i!=input_elements){
			fgets(line, sizeof(line), file);
			input_vec[i] = atoi(line);
			i++;
		}
		fclose(file);
	}
	
//######################### modo sequencial

	if(np==1){
		printf("\n\n\t\033[1mSequencial Mode!\033[0m\n\tTotal Elements: %d\n\n", input_elements);
		gettimeofday(&ti, NULL);
        for(i=0;i<input_elements;i++)
            insertion_sort(input_vec[i], i, 0);
		gettimeofday(&tf, NULL);
        log_results(input_elements);
	}
	
//######################### modo paralelo

	else{
		if (!id) printf("\n\n\t\033[1mParallel Mode!\033[0m\n\tN Processes: %d\n\tTotal Elements: %d\n\tElements per process: %d\n\n",
														np, input_elements, max);
		int tag=0, mpi_arg, index;
		
		MPI_Status status;

//######################### se id==0
		
		if(!id){
			int aux;
			gettimeofday(&ti, NULL);
            for(i=0; i<max; i++)
                aux = insertion_sort(input_vec[i], i, 0);
            index = i;
            do{
                mpi_arg = insertion_sort(input_vec[index], max, 1);
                MPI_Send(&mpi_arg, 1, MPI_INT, id+1, tag, MPI_COMM_WORLD);
                index++;
            }while(index != input_elements);
						
			do{
            	MPI_Recv(&mpi_arg, 1, MPI_INT, np-1, tag, MPI_COMM_WORLD, &status);
			}while(mpi_arg!=1);

			gettimeofday(&tf, NULL);
            
			log_results(max);
            
            mpi_arg = 0;
            MPI_Send(&mpi_arg, 1, MPI_INT, id+1, tag, MPI_COMM_WORLD);
		}
        
//######################### se id for o resto

		else{
			index=0;
			int element_counter=0;			
            do{
                MPI_Recv(&mpi_arg, 1, MPI_INT, id-1, tag, MPI_COMM_WORLD, &status);
              	element_counter++;
                if (index == max){
                    mpi_arg = insertion_sort(mpi_arg, max, 1);
                    MPI_Send(&mpi_arg, 1, MPI_INT, id+1, tag, MPI_COMM_WORLD);
                }
                else{
                    insertion_sort(mpi_arg, index, 0);
                    index++;
                }
				if(element_counter==max*(np-id)) break;
            }while(1); 			

			if(id==np-1){
				mpi_arg=1;	
				MPI_Send(&mpi_arg, 1, MPI_INT, 0, tag, MPI_COMM_WORLD);
			}
 			
            MPI_Recv(&mpi_arg, 1, MPI_INT, id-1, tag, MPI_COMM_WORLD, &status);
            
            log_results(max);
            
            if(id!=np-1){
                MPI_Send(&mpi_arg, 1, MPI_INT, id+1, tag, MPI_COMM_WORLD);
            }
		}
	}
    
	
//######################### fim

	if (!id){
		printf("\tTime: %ds%dms%dus\n\n", abs((int) (tf.tv_sec - ti.tv_sec)),
											  abs((int) (tf.tv_usec - ti.tv_usec)/1000),
											  abs((int) (tf.tv_usec - ti.tv_usec)%1000));
	}
	MPI_Finalize();				//Finaliza o MPI
}
