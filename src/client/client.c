#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include "clientlib.h"

int main(int argc, char** argv) //client --test --color
{
	struct sigaction sa;
	sa.sa_handler = &handler;
	  
	sigaction (SIGKILL, &sa, NULL);
	sigaction (SIGABRT, &sa, NULL);
	sigaction (SIGQUIT, &sa, NULL);
	sigaction (SIGINT, &sa, NULL);
	
	connected=0;
	
	//provo ad aprire la fifo di autorizzazione del server
	serverAuthFIFO = open(SERVER_AUTHORIZATION_FIFO,O_WRONLY);
	
	printScreen(colorRun, DEFAULT, "\e[1;1H\e[2J");

	if(serverAuthFIFO==-1 )	//se la fifo non è presente significa che non vi è nessun server
	{
		printScreen(colorRun, ERROR, "Server non presente\n");
		return 1;
	}
	else	
	{
		//genero il nome della mia fifo univocamente utilizzando il PID e faccio il paring
		char pid[100];
		sprintf(pid,"%d",getpid());
		strcpy(messageFIFOName, CLIENT_MESSAGE_FIFO);
		strcat(messageFIFOName,pid);
		
		mkfifo(messageFIFOName,FILE_MODE);
		inMessageFIFO = open(messageFIFOName,O_RDWR);
	
		//se l'apertura non va a buon fine stampo un errore
		if(inMessageFIFO==-1)
		{
		printScreen(colorRun, ERROR, "Errore di apertura FIFO\n");
		return 0;
		}
		
		//Leggo i parametri
		if (strcmp(argv[1],"0")!=0)
		{	
			testRun = atoi(argv[1]);
		}
		else
		{
			testRun=0;
		}

		if (strcmp(argv[2],"0")!=0)
		{	
			colorRun = 1;
		}
		else
		{
			colorRun = 0;
		}
		
		char* testFileName=(char*)malloc(sizeof(char)*10);
		strcpy(testFileName,argv[1]);
		
		char* username=(char*)malloc(sizeof(char)*MAX_USERNAME_LENGHT);
		size_t size=MAX_USERNAME_LENGHT;
		
		if(testRun==0) //se non è una run di test chiedo di inserire un username
		{
			int correctUsername=-1;
			while(correctUsername==-1)
			{
				printScreen(colorRun, DEFAULT, "Inserisci il tuo username> ");
				getline(&username,&size,stdin);
				strchr(username,'\n')[0]='\0';
				fflush(stdin);
				correctUsername = validateUsername(username);
			}
		}
		else //altrimenti lo leggo da file
		{
			char filePath[1000];
			strcpy(filePath,"../assets/client/");
			strcat(filePath,testFileName);
			strcat(filePath,".test");
			testFile = fopen(filePath,"r");
			if(testFile==NULL)
			{
				printScreen(colorRun, ERROR, "Errore apertura file di test\n\n");
				return 0;
			}
			int size = 20;
			char delay[20];

			fscanf(testFile,"%s",delay);
			fscanf(testFile,"%s",username);
			sleep(atoi(delay)/1000.0);
		}
		
		//mando un messaggio al server richiedendo l'autorizzazione e passandogli il mio pid e il mio username
		char *message = authRequestMessage(pid,username);
		
		
		if(write(serverAuthFIFO,message,strlen(message)+1))
		{
			//aspetto una risposta
			char answerBuffer[MAX_MESSAGE_SIZE];
			read(inMessageFIFO,answerBuffer,MAX_MESSAGE_SIZE);
			Message *answer = parseMessage(answerBuffer);
			
			int answerResult = checkServerAuthResponse(answer);
			
			if(answerResult<0)
			{
				sprintf(stringBuffer, "Errore %d\n",answerResult); //TODO gestire i codici di errore
				printScreen(colorRun, ERROR, stringBuffer);
				deallocResources();
				return answerResult;
			}
			else
			{
				//provo ad aprire la FIFO delle risposte alle domande da inviare al server
				serverAnswerFIFO = open(SERVER_ANSWER_FIFO,O_RDWR);
				
				if(serverAnswerFIFO == -1)
				{
					printScreen(colorRun, ERROR, "Errore di connessione al server\n");
					deallocResources();
					return 0;
				}
				
				printScreen(colorRun, INFO, "Connessione Riuscita\n");
				
				//inizializzo le variabili
				connected=1;
				clientData = (ClientData*) malloc(sizeof(ClientData));
				clientData->name=username;
				initializeClientData(answer);
				initializeQuestion(answer);
				
				sprintf(stringBuffer, "Il server mi ha assegnato %s punti\n",clientData->points);
				printScreen(colorRun, INFO, stringBuffer);

				//Componenti del thread bash
				pthread_t bash;
				char arg[MAX_MESSAGE_SIZE];
				if(testRun==0)
				{
					pthread_create (&bash, NULL, &userInput, arg);
				}
				else
				{
					pthread_create (&bash, NULL, &testInput, arg);
				}
				char rawMessages[MAX_MESSAGE_SIZE];
				
				//setto il mutex a 0
				pthread_mutex_lock(&mutex);
				waitingForUserInput=0;
				newQuestion=1;
				endGame=0;
				
				while (1)
				{
					//mi metto in ascolto
					int size = read(inMessageFIFO,rawMessages,MAX_MESSAGE_SIZE*MAX_CONCURRENT_MESSAGES);
					if(size>0)
					{
						Message** messageList = parseMessages(rawMessages,size); 
						int i=0;
						
						
						while(messageList[i]!=NULL)
						{
							Message* message = messageList[i];
							i++;
							
							if(strchr(message->parameters[0],'K')!=NULL) //messaggio di kick
							{
								printScreen(colorRun, DEFAULT, "\n");
								printScreen(colorRun, AUTH, "Espulso dal server\n");
								deallocResources();
								return 0;
							}
							else if(strchr(message->parameters[0],'D')!=NULL) //server chiuso
							{
								printScreen(colorRun, DEFAULT, "\n");
								printScreen(colorRun, ERROR, "Il server e' stato chiuso\n");
								deallocResources();
								return 0;
							}
							else if(strchr(message->parameters[0],'W')!=NULL) //risposta sbagliata
							{
								printScreen(colorRun, GAME, "Risposta Sbagliata!\n");
								sprintf(stringBuffer, "Ora hai %s punti\n",message->parameters[2]);
								printScreen(colorRun, INFO, stringBuffer);
								pthread_mutex_unlock(&mutex);
							}
							else if(strchr(message->parameters[0],'C')!=NULL) //risposta giusta
							{
								printScreen(colorRun, GAME, "Risposta Corretta!\n");
								sprintf(stringBuffer, "Ora hai %s punti\n",message->parameters[2]);
								printScreen(colorRun, INFO, stringBuffer);
							}
							else if(strchr(message->parameters[0],'T')!=NULL) //risposta giusta ma in ritardo
							{
								printScreen(colorRun, GAME, "Qualcuno ha risposto correttamente prima di te!\n");
								sprintf(stringBuffer, "Ora hai %s punti\n",message->parameters[2]);
								printScreen(colorRun, INFO, stringBuffer);
							}
							else if(strchr(message->parameters[0],'Q')!=NULL) //nuova domanda
							{
								setNewQuestion(message);
								if(waitingForUserInput==1 && testRun==0) //se il thread è bloccato in attesa di una risposta dall utente
								{
									printScreen(colorRun, DEFAULT, "\n");
									waitingForUserInput=0;
									if (pthread_cancel(bash)!=0)//lo chiudo
									{
										printScreen(colorRun, ERROR, "Impossibile terminare il thread bash\n");
									}
									//e lo ricreo
									pthread_create (&bash, NULL, &userInput, &arg);
								}
								else //altrimenti lo sblocco semplicemente
								{
									pthread_mutex_unlock(&mutex);
								}
							}
							else if(strchr(message->parameters[0],'N')!=NULL) //messaggio di notifica
							{
								sprintf(stringBuffer, "%s",message->parameters[1]);
								printScreen(colorRun, DEFAULT, "\n");
								printScreen(colorRun, INFO, stringBuffer);
								if(waitingForUserInput==1 && endGame==0 && testRun==0) //se il thread è bloccato in attesa di una risposta dall utente
								{
									waitingForUserInput=0;
									if (pthread_cancel(bash)!=0)//lo chiudo
									{
										printScreen(colorRun, ERROR, "Impossibile terminare il thread bash\n");
									}
									//e lo ricreo
									pthread_create (&bash, NULL, &userInput, &arg);
								}
							}
							else if(strchr(message->parameters[0],'R')!=NULL) //fine partita e classifica
							{
								printScreen(colorRun, GAME, "La partita si e' conclusa\nNome Punteggio\n----------------\n");
								endGame=1;
								sprintf(stringBuffer, "%s\n",message->parameters[1]);
								printScreen(colorRun, DEFAULT, stringBuffer);
								if (pthread_cancel(bash)!=0) //chiudo il thread bash
								{
									printScreen(colorRun, ERROR, "Impossibile terminare il thread bash :(\n");
								}
								deallocResources();	
								printScreen(colorRun, DEFAULT, "\n");					
								return 0;		
							}
							else
							{
								sprintf(stringBuffer, "Messaggio sconosciuto ricevuto: %s \n",message->parameters[0]);
								printScreen(colorRun, ERROR, stringBuffer);
								deallocResources();
								return 0;
							}
						
						}
						
					}
					else // altrimenti se ricevo un errore mi chiudo preventivamente
					{
						printScreen(colorRun, ERROR, "Errore in lettura messaggio server\n");
						deallocResources();
						return 0;
					}
				}
			}
		} else {
			deallocResources();
			printScreen(colorRun, ERROR, "Errore richiesta autorizzazione\n");
		}
	}
	return 0;
}
