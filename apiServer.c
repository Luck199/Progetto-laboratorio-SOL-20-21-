#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>        /* ind AF_UNIX */
#include <errno.h>
#include <assert.h>
#include <limits.h>

#include "apiServer.h"
#include "utility.h"
#include "gestioneFile.h"

#define DIM 100
#define UNIX_PATH_MAX 108






#define CHECK_FD_SK(S)                                  \
  CHECK_FD;                                             \
  if (!S || strncmp(s\ocketname, S, UNIX_PATH_MAX) != 0) \
  {                                                     \
    errno = EINVAL;                                     \
    return -1;                                          \
  }

#define AINO(code, message, action) \
  if (code == -1)                   \
  {                                 \
    perror(message);                \
    action                          \
  }




int fd_socket = -1;
char actualpath [PATH_MAX+1];

int statoFileDescriptor()
{
	if (fd_socket < 0)
	{
		    errno = EBADFD;
		    return -1;
	}
	else
	{
		return 0;
	}
}

//funzione che riceve il path relativo di un file e, se termina con successo, ritorna il path assoluto
//altrimenti stampa l' errore riscontrato e ritorna NULL

//utilizza realpath, ricorda che in relazione devi documentare realpath

//**************************
//DEVI DOCUMENTARE REALPATH!!!!!
// *************************

char* relativoToAssoluto(const char *fd)
{
	//*fd = "who.txt";

	char *ptr;
	ptr = realpath(fd, actualpath);
	if((ptr== NULL) && (errno==EACCES))
	{
		//printf("Errore: l'autorizzazione di lettura o ricerca è stata negata per un componente del prefisso del percorso.\n");
		return NULL;
	}

	else if((ptr== NULL) && (errno==EINVAL))
	{
		//printf("Errore: il path risulta NULL.\n");
		return NULL;
	}

	else if((ptr== NULL) && (errno==EIO))
	{
		//printf("Errore: si è verificato un errore di I/O durante la lettura dal filesystem.\n");
		return NULL;
	}

	else if((ptr== NULL) && (errno==ELOOP))
	{
		//printf("Errore: Sono stati rilevati troppi collegamenti simbolici nella traduzione del percorso.\n");
		return NULL;
	}

	else if((ptr== NULL) && (errno==ENAMETOOLONG))
	{
		//printf("Un componente di un percorso ha superato i caratteri NAME_MAX o un intero percorso ha superato i caratteri PATH_MAX. \n");
		return NULL;
	}

	else if((ptr== NULL) && (errno==ENOENT))
	{
		//printf("Errore: il file non risulta presente in questa directory.\n");
		return NULL;
	}

	else if((ptr== NULL) && (errno==ENOMEM))
	{
		//printf("Errore: fuori dalla memoria.\n");
		return NULL;
	}

	else if((ptr== NULL) && (errno==ENOTDIR))
	{
		//printf("Errore: un componente del path prefisso non è una directory\n");
		return NULL;
	}
	else
	{
		//printf("%s\n",actualpath);
	}
	return actualpath;
}


char bufferRicezione[200];

int statoFd=0;
//Api per interagire con il file server


int openConnection(const char *sockname, int msec, const struct timespec abstime)
{

	//creo il socket e gestisco gli eventuali errori
	fd_socket = socket(AF_UNIX, SOCK_STREAM, 0);

	if (fd_socket == -1 && errno == EINTR)
	{
		perror("SERVER-> un segnale ha interrotto la funzione socket\n");
		return -1;
	}
	else if (fd_socket == -1)
	{
		perror("SERVER-> la funzione socket ha risocntrato un errore\n");
		return -1;
	}

	//setto la struttura sockaddr_un neccessaria per effettuare la connect
	struct sockaddr_un sa;
	strncpy(sa.sun_path, sockname, UNIX_PATH_MAX);
	sa.sun_family = AF_UNIX;

	//setto la struttura per poter riprovare la connessione in caso di fallimento,
	//per un tempo max di msec millisecondi
	struct timespec request;
	struct timespec remaining;
	request.tv_sec=msec / 1000;
	request.tv_nsec=(msec % 1000) * 1000000L;

	fd_socket=socket(AF_UNIX, SOCK_STREAM, 0);
	while (connect(fd_socket,(struct sockaddr*)&sa, sizeof(sa)) == -1 )
	{
		if (errno == ENOENT)
		{
			time_t current = time(NULL);

			if (current >= abstime.tv_sec)
			{
				//è finito il tempo
				errno = ETIME;
				return -1;
			}

			if(nanosleep(&request,&remaining)<0)
			{
				//printf("errore nanosleep\n");
			}
		}
		else
		{
			return -1;
		}
	}
	int readReturnValue=read(fd_socket,bufferRicezione,sizeof(bufferRicezione));
	if(readReturnValue > 0)
	{
		//printf("CLIENT-> risposta server: %s\n",bufferRicezione);
	}
	return 0;
}


int closeConnection(const char* sockname)
{
	int closeReturnValue = close(fd_socket);
	if ((closeReturnValue == -1) && (errno == EBADF))
	{
		perror("CLIENT-> Errore: file descritor aperto non valido\n");
	}

	if ((closeReturnValue == -1) && (errno == EINTR))
	{
		perror("CLIENT-> Errore: close interrotta da un segnale\n");
	}
	if ((closeReturnValue == -1) && (errno == EIO))
	{
		perror("CLIENT-> Errore input/output");
	}
	if ((closeReturnValue == -1) && ((errno == ENOSPC) || (errno ==EDQUOT)))
	{
		perror("CLIENT-> Errore: scrittura in spazio di archiviazione non disponibile!\n");
	}
	fd_socket = -1;
	//printf("CLIENT-> La connessione è stata chiusa correttamente\n");
	return closeReturnValue;
}



//il client chaima la funzione passandogli solo il path name, sarà poi la procedura che lo concatenerà
//con la stringa OPEN_FILE
int openFile(const char* pathname, int flags)
{
	char daInviare[200]="OPEN_FILE;";

	statoFd=statoFileDescriptor();
	if(statoFd < 0)
	{
		return -1;
	}
	pathname=relativoToAssoluto(pathname);




	strcat(daInviare,pathname);
	char flags_array[3];
	 sprintf(flags_array, ";%d", flags);
	strcat(daInviare,flags_array);
	printf("Sono in openFile -> la stringa finale da inviare risulta : %s\n\n\n\n ",daInviare);
	//printf("pathname che invia client con openFile: %s\n",pathname);
	//printf("messaggio che invio: %s\n",daInviare);
	write(fd_socket,daInviare,200);
	int readReturnValue=read(fd_socket,bufferRicezione,sizeof(bufferRicezione));
	if(readReturnValue > 0)
	{
		//printf("CLIENT-> risposta server: %s\n",bufferRicezione);
	}
	return 1;
}

int readFile(const char* pathname, void** buf, size_t* size)
{
	statoFd=statoFileDescriptor();
	if(statoFd < 0)
	{
		return -1;
	}
	pathname=relativoToAssoluto(pathname);


	write(fd_socket,pathname,40);
	int readReturnValue=read(fd_socket,bufferRicezione,sizeof(bufferRicezione));
	if(readReturnValue > 0)
	{
		//printf("CLIENT-> risposta server: %s\n",bufferRicezione);
	}
	return 1;
}

int readNFiles(int N, const char* dirname)
{
	write(fd_socket,dirname,40);
	int readReturnValue=read(fd_socket,bufferRicezione,sizeof(bufferRicezione));
	if(readReturnValue > 0)
	{
		//printf("CLIENT-> risposta server: %s\n",bufferRicezione);
	}
	return 1;
}


int writeFile(const char* pathname, const char* dirname)
{
	write(fd_socket,dirname,40);
	int readReturnValue=read(fd_socket,bufferRicezione,sizeof(bufferRicezione));
	if(readReturnValue > 0)
	{
		//printf("CLIENT-> risposta server: %s\n",bufferRicezione);
	}
	return 1;
}

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname)
{
	write(fd_socket,dirname,40);
	int readReturnValue=read(fd_socket,bufferRicezione,sizeof(bufferRicezione));
	if(readReturnValue > 0)
	{
		//printf("CLIENT-> risposta server: %s\n",bufferRicezione);
	}
	return 1;
}

int lockFile(const char* pathname)
{
	write(fd_socket,pathname,40);
	int readReturnValue=read(fd_socket,bufferRicezione,sizeof(bufferRicezione));
	if(readReturnValue > 0)
	{
		//printf("CLIENT-> risposta server: %s\n",bufferRicezione);
	}
	return 1;
}

int unlockFile(const char* pathname)
{
	write(fd_socket,pathname,40);
	int readReturnValue=read(fd_socket,bufferRicezione,sizeof(bufferRicezione));
	if(readReturnValue > 0)
	{
		//printf("CLIENT-> risposta server: %s\n",bufferRicezione);
	}
	return 1;
}

int closeFile(const char* pathname)
{
	write(fd_socket,pathname,40);
	int readReturnValue=read(fd_socket,bufferRicezione,sizeof(bufferRicezione));
	if(readReturnValue > 0)
	{
		//printf("CLIENT-> risposta server: %s\n",bufferRicezione);
	}
	return 1;
}

int removeFile(const char* pathname)
{
	write(fd_socket,pathname,40);
	int readReturnValue=read(fd_socket,bufferRicezione,sizeof(bufferRicezione));
	if(readReturnValue > 0)
	{
		//printf("CLIENT-> risposta server: %s\n",bufferRicezione);
	}
	return 1;
}








