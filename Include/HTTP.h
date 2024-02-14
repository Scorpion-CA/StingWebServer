#pragma once

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>

//change this to a veriable set in config, whenever I get around to that
#define MAX_CLIENTS 1024

class HTTP {
public:
	HTTP(std::string IPAddr, int Port, std::string Dir);
	~HTTP();
private:
	int StartServer();
	void CloseServer();
	bool StartListen();
	bool AcceptConnection(int& ClientSocket);
private:
	std::string m_sIPAddr;
	int m_iPort;
	int m_iSocket;
	int m_iClientSocket;
	long m_lIncomingMessage;
	sockaddr_in m_SocketAddr;
	unsigned int m_uSocketAddressLen;
	std::string m_sDir;
};
