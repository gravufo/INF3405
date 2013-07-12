#include "Electeur.h"

Electeur::Electeur()
{

}

DWORD WINAPI Electeur::initThread()
{
	Electeur* e = new Electeur();
	e->initiateVote();
	delete e;
}

void Electeur::initiateVote()
{
	SOCKET ourSocket;
	int nbCandidates = 1; // nombre de candidats minimum

	srand(time(NULL));

	createServerConnection(ourSocket);

	getCandidateList(ourSocket, nbCandidates);

	vote(ourSocket, nbCandidates);

	closesocket(ourSocket);

	cleanup("");
}

void Electeur::createServerConnection(SOCKET &ourSocket)
{
	struct addrinfo *ip = NULL,
		hints;
	std::string sip;
	int returnValue;
	WSADATA wsaData;
	int random;
	char* port;
	char errorMsg[100];

	ZeroMemory( &hints, sizeof(hints));
	hints.ai_family = AF_INET;        // Famille d'adresses
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;  // Protocole utilis� par le serveur

	// Initialisation de Winsock
	returnValue = WSAStartup(MAKEWORD(2,2), &wsaData);
	if (returnValue!= 0)
	{
		printf("Erreur de WSAStartup: %d\n", returnValue);
		exit(returnValue);
	}

	// Initialisation du socket a utiliser
	ourSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (ourSocket == INVALID_SOCKET)
	{
		sprintf(errorMsg, "Erreur de socket(): %ld\n\n", WSAGetLastError());
		
		cleanup(errorMsg);
	}

	std::cout << "Veuillez entrer l'adresse IP du serveur";

	std::cin >> sip;

	do
	{
		do
		{

			// Determiner quel port utiliser
			random = 5000 + rand() % 51;

			itoa(random, port, 10);

			returnValue = getaddrinfo(sip.c_str(), port, &hints, &ip);

			if(returnValue != 0)
			{
				printf("Erreur de getaddrinfo: %d\n", returnValue);
				WSACleanup();
				exit(returnValue);
			}

			// On parcourt les adresses retournees jusqu'a trouver la premiere adresse IPV4
			while(ip != NULL && ip->ai_family != AF_INET)
				ip = ip->ai_next; 

			if(ip == NULL || ip->ai_family != AF_INET)
			{
				freeaddrinfo(ip);
			}

		}while(ip == NULL);

		sockaddr_in *adresse = (struct sockaddr_in *) ip->ai_addr;

		printf("Adresse trouvee pour le serveur %s : %s\n\n", sip.c_str(), inet_ntoa(adresse->sin_addr));
		printf("Tentative de connexion au serveur %s avec le port %s\n\n", inet_ntoa(adresse->sin_addr), port);

		// On va se connecter au serveur en utilisant l'adresse qui se trouve dans la variable ip.
		returnValue = connect(ourSocket, ip->ai_addr, (int)(ip->ai_addrlen));

		if (returnValue == SOCKET_ERROR)
		{
			sprintf(errorMsg, "Impossible de se connecter au serveur %s sur le port %s\n\n", inet_ntoa(adresse->sin_addr), port);
			freeaddrinfo(ip);

			cleanup(errorMsg);
		}
	}while(returnValue == SOCKET_ERROR);

	freeaddrinfo(ip);
}

void Electeur::getCandidateList(const SOCKET &ourSocket, int& nbCandidates)
{
	char* buffer = new char[256];

	int returnValue = recv(ourSocket, buffer, sizeof(buffer), 0);
	
	if (returnValue <= 0)
	{
		char errorMsg[100];
		sprintf(errorMsg, "Erreur de reception : %d\n", WSAGetLastError());
		cleanup(errorMsg);
    }

	//Affichage de la liste des candidats
	for(int characterCounter = 0; buffer[characterCounter] != ';'; ++characterCounter)
	{
		std::string currentName = "";

		while(buffer[characterCounter] != ' ')
		{
			currentName += buffer[characterCounter];
		}

		std::cout << "(" << nbCandidates << ") : " << currentName << std::endl;

		++nbCandidates;
	}
}

void Electeur::vote(const SOCKET &ourSocket, int nbCandidates)
{
	int iChoice, returnValue;
	char* cChoice = new char[2];
	char* errorMsg = new char[256];
	char* ack = new char[20];

	do
	{
		std::cout << "Veuillez entrer le chiffre correspondant au candidat choisi: ";
		std::cin >> iChoice;
	} 
	while (iChoice < 1 || iChoice > nbCandidates);
	
	itoa(iChoice, cChoice, 10);

	returnValue = send(ourSocket, cChoice, sizeof(cChoice), 0);

	if (returnValue == SOCKET_ERROR)
	{
		sprintf(errorMsg, "Erreur du send: %d\n", WSAGetLastError());
		closesocket(ourSocket);
		cleanup(errorMsg);
	}

	returnValue = recv(ourSocket, ack, sizeof(ack), 0);

	printf("%s", ack);
}


void Electeur::cleanup(std::string errorMsg)
{
	WSACleanup();

	std::cout << errorMsg;

	exit(0);
}