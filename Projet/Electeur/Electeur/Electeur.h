#include <WS2tcpip.h>
#include <WinSock2.h>
#include <iostream>
#include <string>
#include <time.h>

// link with Ws2_32.lib
#pragma comment( lib, "ws2_32.lib" )
#pragma warning(disable: 4996)

class Electeur
{
public:
	Electeur();

	void initiateVote();

private:
	void createServerConnection(SOCKET&);

	void getCandidateList(const SOCKET&, int&);

	void vote(const SOCKET&, int);

	void cleanup(std::string);
};