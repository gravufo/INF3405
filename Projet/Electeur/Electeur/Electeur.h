#include <Windows.h>
#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <string>
#include <time.h>

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