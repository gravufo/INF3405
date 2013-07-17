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

#define MAX_TIME_MINUTES 1
#define MAX_CANDIDATES 50
#define MAX_SOCKETS 50

struct _infoSocket {
	sockaddr_in* sockAddrIn;
	SOCKET socket;
} typedef infoSocket, *pInfoSocket;

const std::string CANDIDATES_FILE = "ListeCandidats1.txt";
const std::string LOG_FILE = "log.txt";
const std::string RESULTS_FILE = "resultats.txt";

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
	readCandidateFile();
	
	initialiseTimer();

	initialiseConnection();

	semCandidatesScore = CreateSemaphore(NULL, 1, 1, NULL);

	semLogFile = CreateSemaphore(NULL, 1, 1, NULL);

	if(semLogFile == NULL || semCandidatesScore == NULL) 
	{
		printf("CreateSemaphore error in main thread/function.\n");
		exit(EXIT_FAILURE);
	}

	for(int i = 0; i < MAX_CANDIDATES; ++i)
	{
		candidatesScore[i] = 0;
	}

	for(int i = 0; i <= MAX_SOCKETS; ++i)
	{
		incomingTID[i] = CreateThread(0, 0, acceptConnection, (void*)i, 0, 0);
	}

	WaitForSingleObject(timerThread, INFINITE);

	return EXIT_SUCCESS;
}

void readCandidateFile()
{
	nbCandidates = 0;
	std::ifstream file;

	file.open(CANDIDATES_FILE.c_str());

	if(!file.fail())
	{
		std::string buffer;

		file >> buffer;
		candidates[nbCandidates++] = buffer;

		while(!file.eof())
		{
			file >> buffer;
			candidates[nbCandidates++] = buffer; 
		}

		file.close();
	}
}

void initialiseTimer()
{
	timerThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)timer, NULL, 0, NULL);

	SYSTEMTIME time;
	GetLocalTime(&time);
	char buffer[BUFSIZ];

	sprintf(buffer, "%.4d-%.4d-%.4d %d::%d::%d\n", time.wDay, time.wMonth, time.wYear, time.wHour, time.wMinute, time.wSecond);
	writeLog(buffer);
}

void writeLog(std::string str)
{
	logFile.open(LOG_FILE);

	if(!logFile.fail())
	{
		WaitForSingleObject(semLogFile, INFINITE);

		logFile << str;

		logFile.close();

		ReleaseSemaphore(semLogFile, 1, NULL);
	}
}

void writeResults()
{
	char buffer[256];
	std::ofstream resultsFile;

	resultsFile.open(RESULTS_FILE);

	if(!resultsFile.fail())
	{
		int winnerIndex,
			score = 0;

		for (int i = 0; i < nbCandidates; i++)
		{
			sprintf(buffer, "Candidat #%d : %s\tNombre de votes :\t%i\n", i + 1, candidates[i].c_str(), candidatesScore[i]);
			resultsFile << buffer;
			printf(buffer);

			if (candidatesScore[i] > score)
			{
				score = candidatesScore[i];
				winnerIndex = i;
			}
		}

		sprintf(buffer, "\nLe gagnant est : %s avec %i vote(s).\n", candidates[winnerIndex].c_str(), candidatesScore[winnerIndex]);
		resultsFile << buffer;
		printf(buffer);

		resultsFile.close();
	}
}

DWORD WINAPI timer()
{
	Sleep(MAX_TIME_MINUTES * 60 * 1000);
	printf("Periode de vote ecoulee!\n");
	terminateServer();

	return EXIT_SUCCESS;
}

void terminateServer()
{
	// TODO Close handles...
	for(int i = 0; i <= MAX_SOCKETS; ++i)
	{
		TerminateThread(&incomingTID[i], NULL);
		closesocket(serverSocket[i]);
	}

	for(int i = 0; i <= MAX_SOCKETS; ++i)
	{
		WaitForSingleObject(&processingTID[i], INFINITE);
	}

	WSACleanup();

	writeResults();
}

void initialiseConnection()
{
	int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);

	if (iResult != NO_ERROR)
	{
		printf("Error at WSAStartup()\n");
		exit(EXIT_FAILURE);
	}

	// Recuperation de l'adresse locale
	hostent *thisHost = (hostent*) gethostbyname("localhost");

	char* ip = inet_ntoa(*(struct in_addr*) *thisHost->h_addr_list);

	printf("Adresse locale trouvee %s : \n\n", ip);

	for(int i = 0; i <= MAX_SOCKETS; ++i)
	{
		// Create a SOCKET for listening for incoming connection requests.
		serverSocket[i] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (serverSocket[i] == INVALID_SOCKET)
		{
			WSACleanup();
			exit(EXIT_FAILURE);
		}

		char* option = "1";
		setsockopt(serverSocket[i], SOL_SOCKET, SO_REUSEADDR, option, sizeof(option));

		// The sockaddr_in structure specifies the address family, IP address, and port for the socket that is being bound.
		int port;

		sockaddr_in service;
		service.sin_family = AF_INET;
		service.sin_addr.s_addr = inet_addr(ip);

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

		// Listen for incoming connection requests.on the created socket
		if (listen(serverSocket[i], 30) == SOCKET_ERROR)
		{
			printf("Error listening on socket.\n");
			closesocket(serverSocket[i]);
			WSACleanup();
			exit(EXIT_FAILURE);
		}
	}
}

DWORD WINAPI acceptConnection(void* id)
{
	printf("%i : En attente des connections des clients\n\n", (int)id);

	while (true)
	{
		struct sockaddr_in sinRemote;
		pInfoSocket in = new infoSocket;

		// Create a SOCKET for accepting incoming requests. Accept the connection.
		SOCKET ourSocket = accept(serverSocket[(int)id], NULL, NULL);

		if (ourSocket != INVALID_SOCKET)
		{
			socklen_t len = sizeof(sinRemote);
			getsockname(ourSocket, (sockaddr*)&sinRemote, &len);

			in->sockAddrIn = &sinRemote;
			in->socket = ourSocket;

			printf("Connection acceptee de : %s:%i.\n", inet_ntoa(sinRemote.sin_addr), ntohs(sinRemote.sin_port));

			//TODO erreur quand vient le temps d'afficher, à revoir plus tard...

			processingTID[(int)id] = CreateThread(0, 0, processVote, in, 0, 0);
		}
		else
		{
			printf("Echec d'une connection\n");
		}
	}

	return EXIT_SUCCESS;
}

DWORD WINAPI processVote(LPVOID lpv)
{
	pInfoSocket info = (infoSocket*) lpv;
	sendCandidateList(info);
	receiveVote(info);

	return EXIT_SUCCESS;
}

void sendCandidateList(pInfoSocket info)
{
	std::string buffer;

	for (int i = 0; i < nbCandidates; i++)
	{
		buffer += candidates[i];
		buffer += ' ';
	}

	buffer += ';';

	send(info->socket, buffer.c_str(), buffer.length(), 0);

	// TODO add return value verification
}

void receiveVote(pInfoSocket info)
{
	char buffer[100];
	char cBuffer[2];
	char valid[20];

	int vote;

	recv(info->socket, cBuffer, sizeof(cBuffer), 0);
	// TODO add return value verification


	*buffer = cBuffer[1];
	
	vote = atoi(buffer);

	// Enregistrement du vote
	if(vote >= 0 && vote < nbCandidates && cBuffer[0] == 'c')
	{
		WaitForSingleObject(semCandidatesScore, INFINITE);
		++candidatesScore[vote];
		ReleaseSemaphore(semCandidatesScore, 1, NULL);
		strcpy(valid, "VOTE VALIDE\n");
	}
	else
	{
		strcpy(valid, "VOTE NON-VALIDE\n");
	}

	send(info->socket, valid, sizeof(char[20]), 0);
	
	SYSTEMTIME time;
	GetLocalTime(&time);

	//char port[NI_MAXSERV];
	//char* endPtr;

	//getnameinfo((struct sockaddr*)info->sockAddrIn, sizeof(struct sockaddr), NULL, NULL, port, NI_MAXSERV, NULL);
	
	//buffer = "test";
	/* TODO plante ici ... problème avec le info->(member)->wtv
	sprintf(buffer, "%s:%s %.4d-%.4d-%.4d %d::%d::%d %s", inet_ntoa(info->sockAddrIn->sin_addr), 
			ntohs(info->sockAddrIn->sin_port), time.wDay, time.wMonth, time.wYear, time.wHour, time.wMinute, time.wSecond, valid);*/
	sprintf(buffer, "%s:%i %.2d-%.2d-%.2d %.2d:%.2d:%.2d %s\n", inet_ntoa(info->sockAddrIn->sin_addr), 
		ntohs(info->sockAddrIn->sin_port), time.wDay, time.wMonth, time.wYear, time.wHour, time.wMinute, time.wSecond, valid);

	writeLog(buffer);

	// for debug 
	printf(buffer);
}

// TODO : DONE close all the files
// DONE make sure all pointers are changed (if possible) to non-dynamic solutions to prevent memory leaks
// DONE verify why the port is fucked up <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< mémé arrows
// Verifier la deconnexion du client/serveur