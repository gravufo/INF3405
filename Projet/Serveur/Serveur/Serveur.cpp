#include <WS2tcpip.h>
#include <WinSock2.h>
#include <string>
#include <fstream>
#include <stdio.h>
#include <cstdlib>
#include <iostream>

// link with Ws2_32.lib
#pragma comment( lib, "ws2_32.lib" )
#pragma warning(disable: 4996)

// Nombre maximal de minutes durant lesquelle on peut accepter les connexions
#define MAX_TIME_MINUTES 5

// Nombre maximal de candidats
#define MAX_CANDIDATES 50

// Nombre maximal de sockets à utiliser
#define MAX_SOCKETS 50

// Structure pour envoyer plusieurs paramètres relatifs à la connexion au thread de traitement
struct _infoSocket
{
	sockaddr_in* sockAddrIn;
	SOCKET socket;
} typedef infoSocket, *pInfoSocket;

// Noms par défaut des fichiers de lecture ou d'écriture
const std::string CANDIDATES_FILE = "ListeCandidats1.txt";
const std::string LOG_FILE = "log.txt";
const std::string RESULTS_FILE = "resultats.txt";

// Prototypage des fonctions
void readCandidateFile();
void initialiseTimer();
DWORD WINAPI timer();
void terminateServer();
void initialiseConnection();
void writeLog(std::string);
DWORD WINAPI acceptConnection(void*);
DWORD WINAPI processVote(void*);
void writeResults();
void sendCandidateList(pInfoSocket);
void receiveVote(pInfoSocket);

// Variables globales
std::string candidates[MAX_CANDIDATES];
int nbCandidates;
std::ofstream logFile;
HANDLE semLogFile;
SOCKET serverSocket[MAX_SOCKETS];
WSADATA wsaData;
HANDLE incomingTID[MAX_SOCKETS];
HANDLE processingTID[MAX_SOCKETS];
int candidatesScore[MAX_CANDIDATES];
HANDLE semCandidatesScore;
HANDLE timerThread;

int main()
{
	// Lecture du fichier de candidats
	readCandidateFile();
	
	// Initialisation du compteur
	initialiseTimer();

	// Initialisation des sockets pour recevoir les connexions
	initialiseConnection();

	// Initialisation des sémaphores
	semCandidatesScore = CreateSemaphore(NULL, 1, 1, NULL);
	semLogFile = CreateSemaphore(NULL, 1, 1, NULL);

	if(semLogFile == NULL || semCandidatesScore == NULL) 
	{
		printf("CreateSemaphore error in main thread/function.\n");
		exit(EXIT_FAILURE);
	}

	// Initialisation du score de tous les candidats
	for(int i = 0; i < nbCandidates; ++i)
	{
		candidatesScore[i] = 0;
	}

	// Création d'un thread par socket pour pouvoir recevoir plusieurs connexions simultanément
	for(int i = 0; i <= MAX_SOCKETS; ++i)
	{
		// On envoit vers la fonction accept(int id)
		incomingTID[i] = CreateThread(0, 0, acceptConnection, (void*)i, 0, 0);
	}

	// On attend la terminaison des threads pour terminer le processus principal
	WaitForSingleObject(timerThread, INFINITE);

	return EXIT_SUCCESS;
}

void readCandidateFile()
{
	nbCandidates = 0;
	std::ifstream file;

	// Ouverture du fichier par défaut
	file.open(CANDIDATES_FILE.c_str());

	if(!file.fail())
	{
		std::string buffer;

		// Lecture initiale
		file >> buffer;
		candidates[nbCandidates++] = buffer;
		
		// Lecture de tous les candidats
		while(!file.eof())
		{
			file >> buffer;
			candidates[nbCandidates++] = buffer; 
		}

		// Fermeture du fichier
		file.close();
	}
}

void initialiseTimer()
{
	// Création du thread du compteur vers la fonction timer()
	timerThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)timer, NULL, 0, NULL);

	// Écriture dans le journal de la date de départ
	SYSTEMTIME time;
	GetLocalTime(&time);
	char buffer[BUFSIZ];

	sprintf(buffer, "START %.2d-%.2d-%.2d %d:%d::%d\n\n", time.wDay, time.wMonth, time.wYear, time.wHour, time.wMinute, time.wSecond);
	writeLog(buffer);
}

void writeLog(std::string str)
{
	// Ouverture du fichier par défaut en mode "append"
	logFile.open(LOG_FILE, std::ios::app);

	if(!logFile.fail())
	{
		// S'assurer que c'est notre tour d'écrire
		WaitForSingleObject(semLogFile, INFINITE);

		// Écrire la donnée
		logFile << str;

		// Fermer le fichier
		logFile.close();

		// Permettre aux autres d'écrire
		ReleaseSemaphore(semLogFile, 1, NULL);
	}
}

void writeResults()
{
	char buffer[256];
	std::ofstream resultsFile;

	// Ouvrir le fichier de résultats par défaut
	resultsFile.open(RESULTS_FILE);

	if(!resultsFile.fail())
	{
		int winnerIndex,
			score = 0;
		bool tie = false;
		std::string winners = "";

		for (int i = 0; i < nbCandidates; i++)
		{
			// Afficher le score de tout les candidats
			sprintf(buffer, "Candidat #%d : %s\tNombre de votes :\t%i\n", i + 1, candidates[i].c_str(), candidatesScore[i]);
			resultsFile << buffer;
			printf(buffer);
			
			// Déterminer le score gagnant
			if (candidatesScore[i] > score)
			{
				score = candidatesScore[i];
				winnerIndex = i;
				winners = candidates[i];
			}
		}

		// Vérifier s'il y a plusieurs personnes avec le même score
		for(int i = 0; i < nbCandidates; i++)
		{
			if(i != winnerIndex && candidatesScore[i] == score)
			{
				winners += ", ";
				winners += candidates[i];

				tie = true;
			}
		}

		// Afficher le ou les gagnant(s)
		if(score != 0 && !tie)
			sprintf(buffer, "\nLe gagnant est : %s avec %i vote(s).\n", winners.c_str(), score);
		else if(score != 0 && tie)
			sprintf(buffer, "\nLes gagnants sont : %s avec %i vote(s).\n", winners.c_str(), score);
		else
			sprintf(buffer, "\nIl n'y a pas de gagnant.\n");

		// Écrire dans le fichier
		resultsFile << buffer;
		printf(buffer);

		// Fermeture du fichier
		resultsFile.close();
	}
}

DWORD WINAPI timer()
{
	// Mettre le thread en veille pendant le nombre de minutes définies
	Sleep(MAX_TIME_MINUTES * 60 * 1000);

	// Afficher que le temps de vote est écoulé
	printf("Periode de vote ecoulee! Voici les resultats:\n\n");

	// Terminer le serveur et les connexions
	terminateServer();

	return EXIT_SUCCESS;
}

void terminateServer()
{
	for(int i = 0; i <= MAX_SOCKETS; ++i)
	{
		// Fermeture de tous les sockets
		closesocket(serverSocket[i]);

		// Arrêt de tous les threads
		TerminateThread(&incomingTID[i], NULL);
	}

	for(int i = 0; i <= MAX_SOCKETS; ++i)
	{
		// S'assurer que tous les threads se sont bien terminés
		WaitForSingleObject(&processingTID[i], INFINITE);
	}

	// Nettoyer le WinSock
	WSACleanup();

	// Écrire les résultats dans la console et le fichier
	writeResults();
	
	// Écrire la date de fin dans le journal
	SYSTEMTIME time;
	GetLocalTime(&time);
	char buffer[BUFSIZ];

	sprintf(buffer, "STOP %.2d-%.2d-%.2d %d:%d::%d\n\n", time.wDay, time.wMonth, time.wYear, time.wHour, time.wMinute, time.wSecond);
	writeLog(buffer);
}

void initialiseConnection()
{
	// Demander WinSock v2.2
	int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);

	if (iResult != NO_ERROR)
	{
		printf("Error at WSAStartup()\n");
		exit(EXIT_FAILURE);
	}

	// Récupération de l'adresse locale
	hostent *thisHost = (hostent*) gethostbyname("");

	char* ip = inet_ntoa(*(struct in_addr*) *thisHost->h_addr_list);

	printf("Adresse locale trouvee %s : \n\n", ip);

	for(int i = 0; i <= MAX_SOCKETS; ++i)
	{
		// Créer un socket pour écouter les connexions entrantes
		serverSocket[i] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (serverSocket[i] == INVALID_SOCKET)
		{
			WSACleanup();
			exit(EXIT_FAILURE);
		}

		char* option = "1";
		setsockopt(serverSocket[i], SOL_SOCKET, SO_REUSEADDR, option, sizeof(option));

		// La structure sockaddr_in contient la famille d'adresse, l'adresse IP et le port du socket qu'on active
		int port;

		sockaddr_in service;
		service.sin_family = AF_INET;
		service.sin_addr.s_addr = inet_addr(ip);
		service.sin_addr.s_addr = htonl(INADDR_ANY);

		port = 5000 + i;

		service.sin_port = htons(port);

		if (bind(serverSocket[i], (SOCKADDR*) &service, sizeof(service)) == SOCKET_ERROR)
		{
			printf("Socket bind failed.\n");
			while(i >= 0)
				closesocket(serverSocket[i--]);
			WSACleanup();
			exit(EXIT_FAILURE);
		}

		// Mise en mode écoute du socket
		if (listen(serverSocket[i], 30) == SOCKET_ERROR)
		{
			printf("Error listening on socket.\n");
			closesocket(serverSocket[i]);
			WSACleanup();
			exit(EXIT_FAILURE);
		}

		printf("En attente des connexions des clients\n\n");
	}
}

DWORD WINAPI acceptConnection(void* id)
{
	while (true)
	{
		struct sockaddr_in sinRemote;
		pInfoSocket in = new infoSocket;

		// Créer le socket qui va accepter la connexion lorsqu'il y en a une qui arrive
		SOCKET ourSocket = accept(serverSocket[(int)id], NULL, NULL);

		if (ourSocket != INVALID_SOCKET)
		{
			socklen_t len = sizeof(sinRemote);
			getsockname(ourSocket, (sockaddr*)&sinRemote, &len);

			in->sockAddrIn = &sinRemote;
			in->socket = ourSocket;

			printf("Connexion acceptee de : %s:%i.\n", inet_ntoa(sinRemote.sin_addr), ntohs(sinRemote.sin_port));

			// Créer un thread pour cette connexion en envoyant la structure pInfoSocket en paramètre et traiter le vote
			processingTID[(int)id] = CreateThread(0, 0, processVote, in, 0, 0);
		}
	}

	return EXIT_SUCCESS;
}

DWORD WINAPI processVote(LPVOID lpv)
{
	pInfoSocket info = (infoSocket*) lpv;

	// Envoyer la liste des candidats
	sendCandidateList(info);

	// Recevoir le vote du client
	receiveVote(info);

	// Fermer le socket pour terminer la connexion
	closesocket(info->socket);

	delete info;

	return EXIT_SUCCESS;
}

void sendCandidateList(pInfoSocket info)
{
	std::string buffer;

	// Construire la phrase contenant tous les candidats séparés par un espace
	for (int i = 0; i < nbCandidates; i++)
	{
		buffer += candidates[i];
		buffer += ' ';
	}

	// Terminer la phrase par un point-virgule pour indiquer que c'est la fin
	buffer += ';';

	// Envoyer le tout au client
	int returnValue = send(info->socket, buffer.c_str(), buffer.length(), 0);

	if (returnValue == SOCKET_ERROR)
	{
		printf("Erreur du send: %d\n", WSAGetLastError());
		closesocket(info->socket);
	}
}

void receiveVote(pInfoSocket info)
{
	char buffer[100],
		cBuffer[2],
		valid[20];

	int vote;

	// Réception du vote
	int returnValue = recv(info->socket, cBuffer, sizeof(cBuffer), 0);
	
	if (returnValue == SOCKET_ERROR)
	{
		printf("Erreur du send: %d\n", WSAGetLastError());
		closesocket(info->socket);
	}

	*buffer = cBuffer[1];
	
	vote = atoi(buffer);

	// Vérification de l'encodage et de la validité des données
	if(vote >= 0 && vote < nbCandidates && cBuffer[0] == 'c')
	{
		// S'assurer qu'on peut modifier le tableau de scores
		WaitForSingleObject(semCandidatesScore, INFINITE);

		// Enregistrer le vote
		++candidatesScore[vote];

		// Permettre aux autres d'enregistrer
		ReleaseSemaphore(semCandidatesScore, 1, NULL);

		strcpy(valid, "VOTE VALIDE\n");
	}
	else
	{
		strcpy(valid, "VOTE NON-VALIDE\n");
	}

	// Envoyer le message de confirmation
	send(info->socket, valid, sizeof(valid), 0);
	
	// Enregistrer la date du vote
	SYSTEMTIME time;
	GetLocalTime(&time);

	sprintf(buffer, "CLIENT %s:%i %.2d-%.2d-%.2d %.2d:%.2d::%.2d %s\n", inet_ntoa(info->sockAddrIn->sin_addr), 
		ntohs(info->sockAddrIn->sin_port), time.wDay, time.wMonth, time.wYear, time.wHour, time.wMinute, time.wSecond, valid);

	writeLog(buffer);

	// for debug 
	printf(buffer);
}
