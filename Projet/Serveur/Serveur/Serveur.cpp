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

#define MAX_TIME_MINUTES 5
#define MAX_CANDIDATES 50
#define MAX_SOCKETS 50

struct _infoSocket {
	sockaddr_in* sockAddrIn;
	SOCKET socket;
} typedef infoSocket, *pInfoSocket;

const std::string CANDIDATES_FILE = "ListeCandidats1.txt";

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
DWORD incomingTID[MAX_SOCKETS];
DWORD processingTID[MAX_SOCKETS];
int candidatesScore[MAX_CANDIDATES];
HANDLE semCandidatesScore;
HANDLE timerThread;

int main()
{
	readCandidateFile();
	
	initialiseTimer();

	initialiseConnection();

	semCandidatesScore = CreateSemaphore(NULL, 1, 1, NULL);

	for(int i = 0; i <= MAX_SOCKETS; ++i)
	{
		CreateThread(0, 0, acceptConnection, (void*)i, 0, &incomingTID[i]);
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
	}
}

void initialiseTimer()
{
	timerThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)timer, NULL, 0, NULL);

	SYSTEMTIME time;
	GetLocalTime(&time);
	char buffer[BUFSIZ];

	sprintf(buffer, "%.4d-%.4d-%.4d %d::%d::%d", time.wDay, time.wMonth, time.wYear, time.wHour, time.wMinute, time.wSecond);
	writeLog(buffer);
}

void writeLog(std::string str)
{
	if(!logFile.is_open())
	{
		logFile.open("log.txt");

		semLogFile = CreateSemaphore(NULL, 1, 1, NULL);

		if(semLogFile == NULL) 
		{

			printf("CreateSemaphore error: %d\n", GetLastError());
			exit(EXIT_FAILURE);
		}
	}

	if(!logFile.fail())
	{
		WaitForSingleObject(semLogFile, INFINITE);

		logFile << str;

		ReleaseSemaphore(semLogFile, 1, NULL);
	}
}

void writeResults()
{
	char* buffer = new char[256];
	std::ofstream resultsFile;

	resultsFile.open("resultats.txt");

	if(!resultsFile.fail())
	{
		int winnerIndex, score = 0;
		// TODO : find the winner and display the results (both in the file AND the server console)
		for (int i = 0; i < nbCandidates; i++)
		{
			sprintf(buffer, "Candidat #%d : %s\tNombre de votes :\t%d\n", i+1, candidates[i], candidatesScore[i]);
			resultsFile << buffer;
			printf(buffer);

			if (candidatesScore[i] > score)
			{
				score = candidatesScore[i];
				winnerIndex = i;
			}
		}

		sprintf(buffer, "\nLe gagnant est : %s avec %d votes.\n", candidates[winnerIndex], candidatesScore[winnerIndex]);
		resultsFile << buffer;
		printf(buffer);
	}
}

DWORD WINAPI timer()
{
	Sleep(MAX_TIME_MINUTES * 60 * 1000);
	printf("done sleeping");
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
		std::cerr << "Error at WSAStartup()\n" << std::endl;
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
			//std::cerr << WSAGetLastErrorMessage("bind() failed.") << std::endl;
			while(i >= 0)
				closesocket(serverSocket[i--]);
			WSACleanup();
			exit(EXIT_FAILURE);
		}

		// Listen for incoming connection requests.on the created socket
		if (listen(serverSocket[i], 30) == SOCKET_ERROR)
		{
			//std::cerr << WSAGetLastErrorMessage("Error listening on socket.") << std::endl;
			closesocket(serverSocket[i]);
			WSACleanup();
			exit(EXIT_FAILURE);
		}
	}
}

DWORD WINAPI acceptConnection(void* id)
{
	printf("En attente des connections des clients\n\n");

	while (true)
	{
		sockaddr_in sinRemote;
		pInfoSocket in = new infoSocket;
		
		int nAddrSize = sizeof(sinRemote);

		// Create a SOCKET for accepting incoming requests. Accept the connection.
		SOCKET ourSocket = accept(serverSocket[(int)id], (sockaddr*)&sinRemote, &nAddrSize);
		
		in->sockAddrIn = &sinRemote;
		in->socket = ourSocket;

		if (in->socket != INVALID_SOCKET)
		{
			//printf("Connection acceptee de : %s:%s.\n", inet_ntoa(in->sockAddrIn->sin_addr), ntohs(in->sockAddrIn->sin_port));
			printf("Connection acceptee de : %s:%s.\n", inet_ntoa(in->sockAddrIn->sin_addr), ntohs(in->sockAddrIn->sin_port));
			//TODO erreur quand vient le temps d'afficher, à revoir plus tard...

			CreateThread(0, 0, processVote, in, 0, &processingTID[(int)id]);
		}
		else
		{
			std::cerr << "Echec d'une connection" << std::endl;
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
	const char* cBuffer;

	for (int i = 0; i < nbCandidates; i++)
	{
		buffer += candidates[i];
		buffer += ' ';
	}

	buffer += ';';
	cBuffer = buffer.c_str();

	send(info->socket, cBuffer, sizeof(cBuffer), 0);

	// TODO add return value verification
}

void receiveVote(pInfoSocket info)
{
	char* buffer = new char[256];
	char* valid = new char[256];

	int vote;

	recv(info->socket, buffer, sizeof(buffer), 0);
	// TODO add return value verification

	vote = atoi(buffer);

	// Enregistrement du vote
	if( vote > 0 && vote < nbCandidates )
	{
		WaitForSingleObject(semCandidatesScore, INFINITE);
		++candidatesScore[vote];
		ReleaseSemaphore(semCandidatesScore, 1, NULL);
		valid = "VOTE VALIDE";
	}
	else
	{
		valid = "VOTE NON-VALIDE";
	}

	send(info->socket, valid, sizeof(valid), 0);
	
	SYSTEMTIME time;
	GetLocalTime(&time);

	buffer = "test";
	/* TODO plante ici ... problème avec le info->(member)->wtv
	sprintf(buffer, "%s:%s %.4d-%.4d-%.4d %d::%d::%d %s", inet_ntoa(info->sockAddrIn->sin_addr), 
			ntohs(info->sockAddrIn->sin_port), time.wDay, time.wMonth, time.wYear, time.wHour, time.wMinute, time.wSecond, valid);*/
	
	writeLog(buffer);

	delete buffer;
	delete valid;
}
