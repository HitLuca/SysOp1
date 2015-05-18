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

void handler ()
{  
	deallocResources();
	printf("\n");
	if(connected==1)
	{
		char disconnectMessage[MAX_MESSAGE_SIZE];
		strcpy(disconnectMessage,"Q|");
		strcat(disconnectMessage,clientData->id);
		write(serverAuthFIFO,disconnectMessage,strlen(disconnectMessage)+1);
	}
	exit(0);
}

void* userInput(void* arg)  
{
	//Il client invia le risposte ad serverAnswerFIFO
	char* input=(char*)malloc(sizeof(char)*MAX_MESSAGE_SIZE);
	size_t size = MAX_MESSAGE_SIZE;
	
	while(1)
	{
		if(newQuestion==1)
		{
			printf("La domanda e' %s\n", currentQuestion.text);
			newQuestion=0;
		}
		printf("La tua risposta>");
		waitingForUserInput=1;
		getline(&input,&size,stdin);
		strchr(input,'\n')[0]='\0';
		fflush(stdin);
		sendResponse(serverAnswerFIFO, input);
		waitingForUserInput=0;
		//mi metto in attesa di una risposta
		pthread_mutex_lock(&mutex);
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
	char *message = (char*)malloc(sizeof(char)*(strlen(pid)+strlen(name)+4));
	strcpy(message,"R|");
	strcat(message,pid);
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
	char* message= malloc(MAX_MESSAGE_SIZE*sizeof(char)); 
	strcpy(message, clientData->id);
	strcat(message, "|");
	strcat(message, currentQuestion.id);
	strcat(message, "|");
	strcat(message, answer);
	//printf("SendResponse ha creato: %s\n", message);
	write(serverAnswerFIFO,message,strlen(message)+1);
}

void initializeQuestion(Message *message)
{
	strcpy(currentQuestion.text,message->parameters[2]);
	strcpy(currentQuestion.id,message->parameters[3]);
}


void setNewQuestion(Message *message)
{
	newQuestion=1;
	strcpy(currentQuestion.text,message->parameters[2]);
	strcpy(currentQuestion.id,message->parameters[1]);
}

void deallocResources(){
	close(inMessageFIFO);
	unlink(messageFIFOName);
	
}

