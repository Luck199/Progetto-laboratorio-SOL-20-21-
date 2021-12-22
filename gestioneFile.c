//Created on: 24 nov 2021
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <sys/time.h>

#include "gestioneFile.h"
#include "utility.h"

#define MAXSTRING 250
pthread_mutex_t lockStrutturaFile = PTHREAD_MUTEX_INITIALIZER;

int memoriaDisponibile;
int numFileDisponibili;
int posizioneLibera;
int numFilePresenti;
int filePiuVecchio;
void allocaStrutturaFile()
{
	int i=0;
	memoriaDisponibile = dim_memoria;
	numFileDisponibili = num_max_file;
	posizioneLibera = 0;
	numFilePresenti = 0;
	filePiuVecchio = 0;
	array_file = calloc(num_max_file, sizeof(struct info_file));//per array

	for(i=0;i<num_max_file;i++)
	{
		array_file[i].path=(char*)malloc(sizeof(char)*MAXSTRING);
		strncpy(array_file[i].path,"/0",3);
		array_file[i].data=0;//(char*)malloc(sizeof(char)*MAXSTRING);
		array_file[i].dimensione=0;
		array_file[i].lettoriAttivi=0;
		array_file[i].scrittoriAttivi=0;

		pthread_mutex_init(&(array_file[i].lockFile), NULL);
		//capire perchè errore!!!!!****************
		//array_file[i].lockFile = PTHREAD_MUTEX_INITIALIZER;


	}
	//printf("Allocazione struttura file terminata con successo\n");
}


void deallocaStrutturaFile()
{
	int i=0;
	for(i=0;i<num_max_file;i++)
	{
		free(array_file[i].path);
		//free(array_file[i].data);
		pthread_mutex_destroy(&(array_file[i].lockFile));
	}
	free(array_file);
}

void accediStrutturaFile()
{
	int errore=0;
	if ( ( errore=pthread_mutex_lock(&lockStrutturaFile)) != 0 )
	{
		perror("lock struttura file\n");
		pthread_exit(&errore);
	}
}


void lasciaStrutturaFile()
{
	int errore=0;
	if ( ( errore=pthread_mutex_unlock(&lockStrutturaFile)) != 0 )
	{
		perror("unlock struttura file\n");
		pthread_exit(&errore);
	}

}
void visualizzaArrayFile()
{
	int i=0;
	printf("\n\n\narray dei file: \n");
	printf("numero di file presenti: %d\n",numFilePresenti);
	printf("memoria disponibile: %d\n",memoriaDisponibile);
	printf("numero dei file ancora inseribili: %d\n",numFileDisponibili);

	//***********************************
	//Tale procedura dà errore con valgrind, non utilizzare in progetto finale
	//***********************************
	for(i=0;i<num_max_file;i++)
	{
		printf("posizione: %d -> file : %s\n",i,array_file[i].path);
		//printf("dimensione in byte: %ld\n",array_file[i].dimensione);
		//printf("data e ora di inserimento: %ld\n",array_file[i].data);

	}
}

 int aggiungiFile(char * token)
 {
	size_t sizeFile=0;
	sizeFile=strlen(token);
	struct stat st;
	accediStrutturaFile();


	if (stat(token, &st) == 0)
	{
		if(((int)st.st_size)>dim_memoria)
		{
			printf("Il file richiesto è più grande di tutta la memoria disponibile! Non può essere inserito!\n");
			lasciaStrutturaFile();
			return -1;
		}
		else
		{
			int verificaInserimentoReturnValue=verificaInserimento(st.st_size);
			if(verificaInserimentoReturnValue != 1)
			{
				perror("Errore nell' inserimento del nuovo file\n");
				pthread_exit(NULL);
			}
			//se arrivo qui vuol dire che l' inserimento del file può essere fatto
			strncpy(array_file[posizioneLibera].path,token,sizeFile);
			array_file[posizioneLibera].data= time(NULL);

			//funzione stat utilizzata per recuperare la dimensione del file
			array_file[posizioneLibera].dimensione=st.st_size;
			printf("il file occupa %ld byte\n",array_file[posizioneLibera].dimensione);
			numFilePresenti++;
		}
	}
	else
	{
		perror("ERRORE\n");
		return -1;
	}

	//ho aggiunto correttamente un file alla struttura, modifico le variabili necessarie
	posizioneLibera+=1;
	numFileDisponibili-=1;
	memoriaDisponibile=memoriaDisponibile-st.st_size;
	visualizzaArrayFile();
	lasciaStrutturaFile();
	return 1;

}

int verificaInserimento(int dimFile)
{
	if((memoriaDisponibile >= dimFile) && (numFileDisponibili>0))
	{
		return 1;
	}
	else
	{
		//siamo in un caso di capacity misses
		printf("applico fifo!!\n");
		applicaFifo();
		visualizzaArrayFile();
		return 1;
	}
}

void applicaFifo()
{
	//grazie alla politica fifo, so che una volta eliminato il file più vecchio, il quale
	//indice è presente nella variabile filePiuVecchio, il file presente nell' array alla sua destra ( in modulo ) sarà sempre il più vecchio
	int i=filePiuVecchio,trovato=0;
	size_t daLiberare=0;
	while(trovato != 1)
	{
		if((array_file[i].lettoriAttivi == 0) &&  (array_file[i].scrittoriAttivi == 0))
		{
			//elimino il file in questa posizione, perchè è quello che è da più tempo nell' array e in questo momento non è in stato di lock

			strncpy(array_file[i].path,"/0",3);
			array_file[i].data=0;//(char*)malloc(sizeof(char)*MAXSTRING);
			daLiberare=array_file[i].dimensione;
			array_file[i].dimensione=0;
			array_file[i].lettoriAttivi=0;
			array_file[i].scrittoriAttivi=0;
			trovato = 1;
			printf("elimino il file in posizione %d\n",filePiuVecchio);
			filePiuVecchio=(i+1)%num_max_file;
			numFilePresenti--;
			posizioneLibera= i;
		}
		else
		{
			i=(i+1)%num_max_file;
		}
	}


	numFileDisponibili+=1;
	memoriaDisponibile=memoriaDisponibile+daLiberare;


}

void assumiLockFileLettura(int indiceFile)
{
	int errore=0;
	if ( ( errore=pthread_mutex_lock(&array_file[indiceFile].lockFile)) != 0 )
	{
		perror("lock singolo file\n");
		pthread_exit(&errore);
	}
	array_file[indiceFile].lettoriAttivi++;
}

void lasciaLockFileLettura(int indiceFile)
{
	int errore=0;
	if ( ( errore=pthread_mutex_unlock(&array_file[indiceFile].lockFile)) != 0 )
	{
		perror("lock singolo file\n");
		pthread_exit(&errore);
	}
	array_file[indiceFile].lettoriAttivi--;
}

void assumiLockFileScrittura(int indiceFile)
{
	int errore=0;
	if ( ( errore=pthread_mutex_lock(&array_file[indiceFile].lockFile)) != 0 )
	{
		perror("lock singolo file\n");
		pthread_exit(&errore);
	}
	array_file[indiceFile].scrittoriAttivi++;
}

void lasciaLockFileScrittura(int indiceFile)
{
	int errore=0;
	if ( ( errore=pthread_mutex_unlock(&array_file[indiceFile].lockFile)) != 0 )
	{
		perror("lock singolo file\n");
		pthread_exit(&errore);
	}
	array_file[indiceFile].scrittoriAttivi--;
}

int cercaFile(char* pathname)
{
	int i=0;

	accediStrutturaFile();

	for(i=0; i<num_max_file; i++)
	{
		if(strcmp(array_file[i].path,pathname) == 0 )
		{
			return 1;
		}
	}



	lasciaStrutturaFile();
	return -1;
}

//NOn ancora gestito caso in cui entrambi i flag
int openFileServer(char *path, int flag)
{
	printf("è stata chiamata dal thread worker la open file sul file %s con il flag : %d\n",path,flag);
	//verifico se il flag ricevuto ha un valore uguale a 0 oppure a 1
	if (flag < 0 || flag > 2)
	{
		errno = EINVAL;
		return -1;
	}

	//se il file richiesto è presente nell' array e il flag risulta 0 ( ovvero O_CREATE) ritorno errore
	int cercaFileReturnValue=cercaFile(path);
	if((flag == 0) && (cercaFileReturnValue == 1))
	{
		printf("si vuole creare un file già presente, errore!\n");
		return -1;
	}

	//se il file richiesto non è presente nell' array e il flag risulta 1 ( ovvero O_LOCK) ritorno errore
	if((flag == 1) && (cercaFile(path) == -1))
	{
		printf("si vuole lockare un file non presente, errore!\n");
		return -1;
	}


	//Inserisco file se non risulta presente
	if(cercaFileReturnValue == -1)
	{
		aggiungiFile(path);
	}


	//Se sono arrivato qui vuol dire che posso procedere ad eseguire l' operazione di openFile
	//accediStrutturaFile();



	//lasciaStrutturaFile();

	return 1;
}











