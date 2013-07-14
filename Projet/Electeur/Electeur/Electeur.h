#include <WS2tcpip.h>
#include <WinSock2.h>
#include <iostream>
#include <string>
#include <time.h>

// link with Ws2_32.lib
#pragma comment( lib, "ws2_32.lib" )

class Electeur
{
public:
	Electeur();
	
	static DWORD WINAPI initThread();
	void initiateVote();

private:
	void createServerConnection(SOCKET&);

	void getCandidateList(const SOCKET&, int&);

	void vote(const SOCKET&, int);

	void cleanup(std::string);
};