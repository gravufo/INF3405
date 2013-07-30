#include "Electeur.h"

int main()
{
	Electeur* e = new Electeur();
	e->initiateVote();
	delete e;
	return EXIT_SUCCESS;
}

Electeur::Electeur()
{

}

void Electeur::initiateVote()
{
	SOCKET ourSocket;
	int nbCandidates = 1; // nombre de candidats minimum

	// Initialisation du "seed" pour la fonction random (pour simuler la partie aléatoire)
	srand((unsigned int) time(NULL));

	// Créer la connexion au server
	createServerConnection(ourSocket);

	// Obtenir la liste des candidats
	getCandidateList(ourSocket, nbCandidates);

	// Obtenir le choix de l'usager et l'envoyer au server
	vote(ourSocket, nbCandidates);

	// Fermer le socket principal
	closesocket(ourSocket);

	// Faire les opérations nécessaires pour terminer le programme
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
	char port[256];
	char errorMsg[100];

	ZeroMemory( &hints, sizeof(hints));
	hints.ai_family = AF_INET;        // Famille d'adresses
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;  // Protocole utilisé par le serveur

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
		sprintf_s(errorMsg, "Erreur de socket(): %ld\n\n", WSAGetLastError());
		
		cleanup(errorMsg);
	}

	std::cout << "Veuillez entrer l'adresse IP du serveur\n";
	std::cin >> sip;

	do
	{
		do
		{

			// Determiner quel port utiliser de façon aléatoire
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
			sprintf_s(errorMsg, "Impossible de se connecter au serveur %s sur le port %s\n\n", inet_ntoa(adresse->sin_addr), port);
			freeaddrinfo(ip);

			cleanup(errorMsg);
		}
	}while(returnValue == SOCKET_ERROR);

	freeaddrinfo(ip);
}

void Electeur::getCandidateList(const SOCKET &ourSocket, int& nbCandidates)
{
	char buffer[256];

	// Réception de la liste des candidats
	int returnValue = recv(ourSocket, buffer, sizeof(buffer), 0);
	
	if (returnValue <= 0)
	{
		char errorMsg[100];
		sprintf_s(errorMsg, "Erreur de reception : %d\n", WSAGetLastError());
		cleanup(errorMsg);
    }

	// Affichage de la liste des candidats
	for(int characterCounter = 0; buffer[characterCounter] != ';'; ++characterCounter)
	{
		std::string currentName = "";

		while(buffer[characterCounter] != ' ')
		{
			currentName += buffer[characterCounter++];
		}

		std::cout << "(" << nbCandidates << ") : " << currentName << std::endl;

		++nbCandidates;
	}
}

void Electeur::vote(const SOCKET &ourSocket, int nbCandidates)
{
	int iChoice,
		returnValue;

	char cChoice[2],
		errorMsg[256],
		ack[20];

	// Recevoir le choix de l'usager
	do
	{
		std::cout << "Veuillez entrer le chiffre correspondant au candidat choisi: ";
		std::cin >> iChoice;

		if(iChoice < 1)
			std::cout << "Erreur: La valeur doit etre plus grande que 0. Veuillez essayer de nouveau." << std::endl;
	}
	while (iChoice < 1);
	
	// Standardiser le message pour usage sur le serveur (-1) et encoder avec 'c'
	itoa(iChoice - 1, cChoice, 10);
	cChoice[1] = cChoice[0];
	cChoice[0] = 'c';

	// Envoyer le vote au serveur
	returnValue = send(ourSocket, cChoice, sizeof(cChoice), 0);

	if (returnValue == SOCKET_ERROR)
	{
		sprintf(errorMsg, "Erreur du send: %d\n", WSAGetLastError());
		closesocket(ourSocket);
		cleanup(errorMsg);
	}

	// Attendre le message de confirmation (ack)
	returnValue = recv(ourSocket, ack, sizeof(ack), 0);

	if (returnValue == SOCKET_ERROR)
	{
		sprintf(errorMsg, "Erreur du recv: %d\n", WSAGetLastError());
		closesocket(ourSocket);
		cleanup(errorMsg);
	}

	printf("%s", ack);
}

void Electeur::cleanup(std::string errorMsg)
{
	WSACleanup();

	std::cout << errorMsg;

	exit(EXIT_SUCCESS);
}
