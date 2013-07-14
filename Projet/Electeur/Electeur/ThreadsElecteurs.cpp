#include "Electeur.h"

#define MAX_THREADS 1

int main()
{
	DWORD dwThreadIdArray[MAX_THREADS];
	HANDLE hThreadArray[MAX_THREADS];

	for(int i = 0; i < MAX_THREADS; ++i)
	{
		hThreadArray[i] = CreateThread( 
            NULL,                   // default security attributes
            0,                      // use default stack size  
			(LPTHREAD_START_ROUTINE)Electeur::initThread,       // thread function name
            NULL,          // argument to thread function 
            0,                      // use default creation flags 
            &dwThreadIdArray[i]);   // returns the thread identifier 

	}

	for(int i = 0; i < MAX_THREADS; ++i)
	{
		WaitForSingleObject(hThreadArray[i], INFINITE);
	}
}
