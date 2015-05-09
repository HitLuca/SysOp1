#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#include "serverlib.h"

void* authorizationThread(void* arg)
{
	//Avvio il monitoraggio delle autorizzazioni
	int serverAuthFIFO = open(SERVER_AUTHORIZATION_FIFO,O_RDWR);
	char clientMessage[MAX_MESSAGE_SIZE];
	char serverMessage[MAX_MESSAGE_SIZE];
	char fifoPath [MAX_FIFO_NAME_SIZE];
	
	char* answer;
	
	while(1)
	{
		strcpy(fifoPath,CLIENT_MESSAGE_FIFO);
		read(serverAuthFIFO,clientMessage,MAX_MESSAGE_SIZE);
		printf("authThread: Ho ricevuto %s \n",clientMessage);
		Message *message = parseMessage(clientMessage);
		
		strcat(fifoPath,message->parameters[0]);
		
		printf(" %s ha effettuato una richiesta di connessione\n",message->parameters[1]);
		
		//apro la fifo del client
		int clientMessageFIFO = open(fifoPath,O_RDWR);
		write(clientMessageFIFO,"1schifoso",strlen("0schifoso"));
		
		int id=checkClientAuthRequest(message);
		
		if(id>=0)
		{
			connectNewClient(id,message->parameters[1],message->parameters[0]);
			answer=authAcceptMessage(id);
		}
		else
		{
			answer=authRejectMessage(id);
		}
		write(clientMessageFIFO,answer, strlen(answer)+1);
		free(message);
		free(answer);
	}
}

void* senderThread(void*arg)
{
	//Creo il collegamento alla FIFO del client
	char fifoPath [MAX_FIFO_NAME_SIZE];
	strcpy(fifoPath,CLIENT_MESSAGE_FIFO);
	strcat(fifoPath,((char**)arg)[0]);
	printf("Scrivo sulla FIFO: %s\n",fifoPath);

	int clientMessageFIFO = open(fifoPath,O_RDWR);
	write(clientMessageFIFO,((char**)arg)[1], strlen(((char**)arg)[1])+1);
	free(arg);
}

void* bashThread(void*arg)
{
	char comando[MAX_COMMAND_SIZE];
	printf("\e[1;1H\e[2J");
	printf("Benvenuto nel terminale utente!\nDigita help per una lista dei comandi\n");
	while(1)
	{
		printf(">");
		scanf("%s", comando);
		if (strcmp(comando, "help")==0)
		{
			printf("Lista comandi utente:\n");
			printf("help: Richiama questo messaggio di aiuto\n");
			printf("kick <utente>: Kick di <utente> dalla partita\n");
			printf("question <question>: Invia una nuova domanda a tutti gli utenti\n");
			printf("list: Stampa la lista degli utenti connessi\n");
			printf("clear: Pulisce la schermata corrente\n");
		}
		else if (strcmp(comando, "clear")==0)
		{
			printf("\e[1;1H\e[2J");
		}
		else
		{
			printf("%s:Comando non riconosciuto\n",comando);
		}
	}
}

int checkClientAuthRequest(Message *message)
{
	if(message->parameterCount!=2)
	{
		return -2; //WRONG PARAMETER COUNT
	}
	else
	{
		if (connectedClientsNumber<clientsMaxNumber)
		{
			int i;
			for(i=0;i<clientsMaxNumber;i++)
			{
				if(clientData[i]!=NULL)
				{
					if(strcmp(clientData[i]->name,message->parameters[1])==0)
					{
						return -4; //NAME ALREADY IN USE
					}
				}
			}
			for(i=0;i<clientsMaxNumber;i++)
			{
				if(clientData[i]==NULL)
				{
					/*clientNames[i]=(char*)malloc((srtlen(message->parameters[1])+1)*sizeof(char));
					strcpy(clientNames[i],message->parameters[1]);
					connectedClientsNumber++;*/
					return i;
				}
			}
			
		}
		else
		{
			return -3; //SERVER FULL
		}
	}
}

void initializeClientData()
{
	clientData = (ClientData**)malloc(sizeof(ClientData*)*clientsMaxNumber);
	int i;
	for(i=0;i<clientsMaxNumber;i++)
	{
		clientData[i]=NULL;
	}
}

void connectNewClient(int id,char* name,char* pid)
{
	clientData[id]=(ClientData*)malloc(sizeof(ClientData));
	clientData[id]->name=(char*)malloc(sizeof(char)*(strlen(name)+1));
	strcpy(clientData[id]->name,name);
	clientData[id]->pid=(char*)malloc(sizeof(char)*(strlen(pid)+1));
	strcpy(clientData[id]->pid,pid);
	clientData[id]->points=clientsMaxNumber-connectedClientsNumber;
	connectedClientsNumber++;
	printf("Ho allocato lo slot %d con il client %s che ha il pid %s e parte con %d punti, sono rimasti %d posti liberi\n",id,clientData[id]->name,clientData[id]->pid,clientData[id]->points,clientsMaxNumber-connectedClientsNumber);
}

char* authAcceptMessage(int id)
{
	char idc[3];
	char points[10];
	sprintf(idc,"%d",id);
	sprintf(points,"%d",clientData[id]->points);
	char *answer = (char*)malloc(sizeof(char)*MAX_MESSAGE_SIZE);
	strcpy(answer,"A|");
	strcat(answer,idc);
	strcat(answer,"|");
	strcat(answer,currentQuestion.text);
	strcat(answer,"|");
	strcat(answer,currentQuestion.id);
	strcat(answer,"|");
	strcat(answer,points);
	return answer;
}

char* authRejectMessage(int error)
{
	char errors[4];
	sprintf(errors,"%d",error);
	char *answer = (char*)malloc(sizeof(char)*MAX_MESSAGE_SIZE);
	strcpy(answer,"A|");
	strcat(answer,errors);
	return answer;
}