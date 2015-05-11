#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#include "clientlib.h"

void* userInput(void* arg)  //METTERE A POSTO QUESTO THREAD, NON VA BENE
{
	int serverAnswerFIFO = open(SERVER_ANSWER_FIFO,O_RDWR);

	printf("La domanda è: %s\n", (char*) arg);
	printf("Scrivi la tua risposta\n");
	char input[MAX_MESSAGE_SIZE];
	strcpy(currentQuestion.id, "0");  //SETTATO L'ID DI TUTTE LE DOMANDE A 0

	//Il client invia le risposte ad serverAnswerFIFO
	while(1)
	{
		printf(">");
		scanf("%s",input);
		sendResponse(serverAnswerFIFO, input);
	}
}

int validateUsername(char* username)
{
	printf("hai scelto %s\n",username);
	if(strlen(username)<MIN_USERNAME_LENGHT)
	{
		printf("Errore: username troppo corto\n");
		return -1;
	}
	else if(strlen(username)>MAX_USERNAME_LENGHT)
	{
		printf("Errore: username troppo lungo\n");
		return -1;
	}
	else
	{
		int i;
		for(i=0;i<strlen(username);i++)
		{
			if(!isalnum(username[i]))
			{
				printf("Errore: carattere %c non valido\n",username[i]);
				return -1;
			}
		}
		return 0;
	}
	
}

char* authRequestMessage(char* pid,char* name)
{
	char *message = (char*)malloc(sizeof(char)*(strlen(pid)+strlen(name)+2));
	strcpy(message,pid);
	strcat(message,"|");
	strcat(message,name);
	return message;
}

int checkServerAuthResponse(Message* message){
	if(strcmp(message->parameters[0],"A")==0)
	{
		if(message->parameterCount==2)
		{
			return atoi(message->parameters[1]);
		}
		else if(message->parameterCount==5)
		{
			return 0;
		}
	}
	return -1;
}

void initializeClientData(Message *message){
	clientData->id=(char*)malloc(sizeof(char)*(strlen(message->parameters[1])+1));
	strcpy(clientData->id,message->parameters[1]);
	clientData->points=(char*)malloc(sizeof(char)*(strlen(message->parameters[4])+1));
	strcpy(clientData->points,message->parameters[4]);
}

void sendResponse(int serverAnswerFIFO, char* answer)
{
	char* message= malloc(MAX_MESSAGE_SIZE*sizeof(char)); //<----- MALLOC FATTA A CASO, SETTARE I VALORI CORRETTI!
	strcpy(message, clientData->id);
	strcat(message, "|");
	strcat(message, currentQuestion.id);
	strcat(message, "|");
	strcat(message, answer);
	printf("SendResponse ha creato: %s\n", message);
	write(serverAnswerFIFO,message,strlen(message)+1);
}