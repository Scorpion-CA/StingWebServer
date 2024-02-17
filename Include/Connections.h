#pragma once
#include <thread>
#include <time.h>

class HTTP;

class CON { // Maybe I can get this to work? Right now it causes a bunch of issues with seg faulting so when I figure that out
public:
	friend HTTP;
public:
	CON(HTTP* Parent, int Sock);
	void Loop(int Sock);
private:
	int m_iSocket;
	bool m_bShouldQuit = false;
	std::clock_t m_cTimer;
	std::thread m_thread;
	HTTP* m_pParent;
};