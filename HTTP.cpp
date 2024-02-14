#include "Include/HTTP.h"

#include <iostream>
#include <sstream>

HTTP::HTTP(std::string IPAddr, int Port, std::string Dir) : m_sIPAddr(IPAddr), m_iPort(Port), m_iSocket(), m_iClientSocket(),
										   					m_lIncomingMessage(), m_SocketAddr(), 
															m_uSocketAddressLen(sizeof(m_SocketAddr)), m_sDir(Dir) {
	m_SocketAddr.sin_family = AF_INET;
	m_SocketAddr.sin_port = htons(m_iPort);
	m_SocketAddr.sin_addr.s_addr = inet_addr(m_sIPAddr.c_str());
	StartServer();
}

HTTP::~HTTP() {
	CloseServer();
}

int HTTP::StartServer() {
	// Create socket to be used for server
	m_iSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (m_iSocket < 0) {
		std::cout << "[ERROR] Failed to create socket." << std::endl;
		return -1;
	}
	std::cout << "[SUCCESS] Created socket on: " << m_sIPAddr << ", Port: " << m_iPort << std::endl;

	// Bind socket to address
	if (bind(m_iSocket, (sockaddr *)&m_SocketAddr, m_uSocketAddressLen) < 0) {
		std::cout << "[ERROR] Failed to bind socket." << std::endl;
		return -1;
	}
	std::cout << "[SUCCESS] Successfully bound socket to address" << std::endl;

	// Listen for connections
	if (!StartListen()) {
		return -1;
	}
	return 0;
}

bool HTTP::StartListen() {
	// Listen for connections to the socket, with the max number of connections set in config
	if (listen(m_iSocket, MAX_CLIENTS) < 0) {
		std::cout << "[ERROR] Failed to start socket listen" << std::endl;
		return false;
	}
	std::cout << "Listening on " << inet_ntoa(m_SocketAddr.sin_addr)
			  << " Port: " << ntohs(m_SocketAddr.sin_port) << std::endl << std::endl;

	while (true) { // main loop, should be able to be stopped through the dashboard planned to be created
		// Accept incoming connection
		AcceptConnection(m_iClientSocket);

		char buf[32768] = {0}; // eventually change this, HTTP requests can be sent in pieces, so this
							   // really only needs to be enough to accept the first header then create
							   // a thread that handles the rest of the data
		if (read(m_iClientSocket, buf, 32768) < 0) {
			std::cout << "[ERROR] Failed to read data from socket connection" << std::endl;
			close(m_iClientSocket);
			return false;
		}

		std::cout << "Connection Accepted" << std::endl << std::endl; // Only for debugging, should be removed

		std::cout << buf << std::endl;

		std::ostringstream oss;

		oss << "HTTP/1.1 200 OK\r\n";
		oss << "Connection: keep-alive\r\n";
		oss << "Content-Type: text/html; charset=utf-8\r\n";
		oss << "Content-Length: 38\r\n\r\n";
		oss << "<p>Sting Web Server, Linux, 0.0.1</p>\r\n";

		write(m_iClientSocket, oss.str().c_str(), oss.str().size());

		close(m_iClientSocket);
	}
	return true;
}

bool HTTP::AcceptConnection(int& ClientSocket) {
	// Accept an incoming connection and set the argument socket to it
	ClientSocket = accept(m_iSocket, (sockaddr *)&m_SocketAddr, &m_uSocketAddressLen);

	if (ClientSocket < 0) {
		std::cout << "[ERROR] Failed to accept incoming connection. Address: "
				  << inet_ntoa(m_SocketAddr.sin_addr) << ", Port: "
				  << ntohs(m_SocketAddr.sin_port);
		return false;
	}

	return true;
}

void HTTP::CloseServer() {
	close(m_iSocket);
	close(m_iClientSocket);
	exit(0);
}
