#include "Include/HTTP.h"
#include "Include/Connections.h"

#include <iostream>
#include <sstream>
#include <cstring>
#include <fcntl.h>
#include <thread>

HTTP::HTTP(std::string IPAddr, int Port, std::string Dir) : m_sIPAddr(IPAddr), m_iPort(Port), m_iSocket(), m_iClientSocket(),
										   					m_lIncomingMessage(), m_SocketAddr(), 
															m_uSocketAddressLen(sizeof(m_SocketAddr)), m_sDir(Dir) {
	m_SocketAddr.sin_family = AF_INET;
	m_SocketAddr.sin_port = htons(m_iPort);
	m_SocketAddr.sin_addr.s_addr = inet_addr(m_sIPAddr.c_str());

	std::thread HouseKeeperThread(&HTTP::HouseKeeper, this); // Creates the housekeeper thread to monitor different
	HouseKeeperThread.detach();						  		 // connections and kill idle ones and detach it
	StartServer();
}

HTTP::~HTTP() {
	CloseServer();
}

void HTTP::HandleMessage(int Sock, CON* Parent) {
	std::string req; // The string to hold the entire request
	char buf[HTTP_CHUNK_LEN + 1] = {0}; // char array to be read into, with an extra for a null terminator
	int count = HTTP_CHUNK_LEN; // holds the amount of bytes remaining to be read, initialized to the size of the char array - 1

	// HTTP Chunking, this took longer than it should have and I was so close like 80 times
	// More for large requests, I will eventually add the ability for the message to be broken up into chunks
	while(1) {
		count = HTTP_CHUNK_LEN;
		while (count == HTTP_CHUNK_LEN) { // Only reads if count is equal to the size of the buffer, because if it's less then that
			count = read(Sock, buf, HTTP_CHUNK_LEN); // then the last read was the last data, and doing it would get the program stuck
			
			if (count == -1) {
				std::cout << "[ERROR] Failed to read from socket | ";
				char errBuf[256];
				char* errMsg = strerror_r(errno, errBuf, 256);
				printf("[ %s ]\n", errMsg);
				return;				
			}

			if (count == HTTP_CHUNK_LEN)
				buf[HTTP_CHUNK_LEN] = '\0'; // Adds null terminator so that it goes into the std::string properly
			req += std::string(buf); // adds the array to the req std::string
			memset(buf, 0, HTTP_CHUNK_LEN + 1); // sets the array to 0s after being done with it, good practise I think
			
		}

		req = std::string(buf);

		HMESSAGE msg(req);

		std::string content = ReadResource(m_sDir + WEBSITE_ENTRY);

		std::ostringstream oss;

		oss << "HTTP/1.1 200 OK\r\n";
		oss << "Connection: keep-alive\r\n";
		oss << "Content-Type: text/html; charset=utf-8\r\n";
		oss << "Content-Length: " << content.size() << "\r\n\r\n";
		oss << content;

		if (count > 0) write(Sock, oss.str().c_str(), oss.str().size()); // 1 character, 1 = caused this to crash the
		// program when connecting on an iphone, it was so dumb
		req.clear();
		if (count > 0) Parent->m_cTimer = std::clock();
	}
	return;
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
		if (AcceptConnection(m_iClientSocket))
			if (m_iClientSocket > 0) {
				std::cout << "Connection accepted " << m_iClientSocket << std::endl;
				m_vConnections.push_back(CON(this, m_iClientSocket));
			}

			for (int i = 0; i < m_vConnections.size(); i++) {
				printf("%d",i);
			}
		// close(m_iClientSocket); Permanent Comment of shame | This motherfucker was closing the file descriptor after 
		// I made the thread lmao. Some of the more elegant solutions were probably working fine
		sleep(1); // Just to make sure it doesn't use 100% of CPU, eventually will want to remove this
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

// Eventually make this also handle images, like in the Windows one
std::string HTTP::ReadResource(std::string Path) {
	int File = open(Path.c_str(), O_RDONLY);
	char buf[FILE_CHUNK_LEN + 1];
	std::string out;

	if (File < 0) {
		std::cout << "[ERROR] Cannot open file at " << Path << std::endl;
		return "404"; // do more error checking to tell the user exactly what failed and why
	}
	// Eventually add chunking like when getting HTTP requests, like where the function gets called multiple times
	// and sent in separate packets
	int count = FILE_CHUNK_LEN;
	while (count == FILE_CHUNK_LEN) {
		count = read(File, buf, FILE_CHUNK_LEN);
		if (count < 0) {
			std::cout << "[ERROR] Error reading file at " << Path << std::endl;
			return "ERROR";
		}
		if (count == FILE_CHUNK_LEN)
			buf[FILE_CHUNK_LEN] = '\0';
		out += buf;
		memset(buf, 0, FILE_CHUNK_LEN + 1);
	}

	close(File);

	return out;
}

void HTTP::CloseServer() {
	close(m_iSocket);
	close(m_iClientSocket);
	exit(0);
}

// HMESSAGE

HMESSAGE::HMESSAGE() {

}

HMESSAGE::HMESSAGE(std::string in) {
	std::vector<std::string> items; // Holds the individual items parsed in the first line
	std::string item; // Holds the individual item being parsed
	std::string headerLine[2]; // Holds both sides of the header lines
	int idx = 0; // Just holds which side of the line is being worked on

	bool firstLine = true; // Flag for if it is currently in the first line
	bool content = false; // Flag for if the headers have finished and the rest should be dumped into m_sHeaders

	for (int i = 0; i < in.size(); i++) {
		if (!content) {
			if (firstLine) { // Parses the first line for info about the request, I know there is standard library
				if (in[i] == '\r') // functions for this, but 1: I forgot when writing this, and 2: this project is
					continue; // meant to be for learning, so I plan on doing as much as possible with my own code
				if (in[i] == '\n') { // Checks for end of line then saves all the items for future use
					items.push_back(item);
					item.clear();
					firstLine = false;
					continue;
				}
				if (in[i] == ' ') { // Breaks the line into its parts
					items.push_back(item);
					item.clear();
					continue;
				}
				item += in[i]; // Adds the current character to the current item
				continue;
			}
			if (in[i] == '\n' && in[i - 2] == '\n' && in[i - 1] == '\r' && in[i - 3] == '\r') { // End of headers
				content = true; // Sets the flag to start dumping the whole thing to m_sContent
				m_mHeaders[headerLine[0]] = headerLine[1]; // adds the final header and data to the map
				headerLine[0].clear();
				headerLine[1].clear();
				idx = 0;
				continue;
			}
			if (in[i] == '\n' && in[i - 1] == '\r') { // Checks for end of line
				m_mHeaders[headerLine[0]] = headerLine[1]; // adds the current header and data to the map
				headerLine[0].clear();
				headerLine[1].clear();
				idx = 0;
				continue;
			}
			if (in[i] == ' ' || in[i] == ':') { // Gets rid of the separators for the header and data
				idx = 1;
				continue;
			}
			if (in[i] == '\r' || in[i] == '\n') // Gets rid of end of line characters in case my logic is bad and
				continue;						// they somehow managed to squeek by to this point

			headerLine[idx] += in[i]; // Adds the current character to which ever side of the line being worked on
		}
		m_sContent += in[i]; // Dumps the rest of the data in the request to content after the flag is set
	}

	if (items[0] == "GET") 			// Checks what type of request it is, don't know if there is a better way to do
		m_iType = REQTYPE::GET;		// this since switch can't be used on strings. or at least it wouldn't let me
	else if (items[0] == "HEAD")
		m_iType = REQTYPE::HEAD;
	else if (items[0] == "POST")
		m_iType = REQTYPE::POST;
	else if (items[0] == "PUT")
		m_iType = REQTYPE::PUT;
	else if (items[0] == "DELETE")
		m_iType = REQTYPE::DELETE;
	else if (items[0] == "CONNECT")
		m_iType = REQTYPE::CONNECT;
	else if (items[0] == "OPTIONS")
		m_iType = REQTYPE::OPTIONS;
	else if (items[0] == "TRACE")
		m_iType = REQTYPE::TRACE;

	m_sURL = items[1]; // Both of these just take the appropriate item from the vector of items and puts it in the
	m_sVersion = items[2]; // proper variable

	items.clear(); // Not really necessary but clears the vector of items, saves something like 20 bytes of memory
}

HMESSAGE HMESSAGE::ErrorMessage() { // eventually add error message
	HMESSAGE out;
	out.m_sContent = "[ERROR]";
	return out;
}

void HTTP::HouseKeeper() { // for some reason, this only works if opera connects? what?
	while(1) {	// This code causes seg faults sometimes, need to add some metric in order to do this without
		for (int i = 0; i < m_vConnections.size(); i++) {	// seg faulting because this is necessary functionality
			//if ((std::clock() - m_vConnections[i].m_cTimer) > CONNECTION_TIMEOUT && m_vConnections[i].m_cTimer > 0 && !m_vConnections[i].m_bShouldQuit) {
				/*if (m_vConnections[i].m_thread.joinable()) {
					pthread_cancel(m_vConnections[i].m_thread.native_handle());
					m_vConnections[i].m_thread.detach();

					std::ostringstream oss;

					oss << "HTTP/1.1 200 OK\r\n";
					oss << "Connection: close\r\n";
					oss << "Content-Type: text/html; charset=utf-8\r\n";

					write(m_vConnections[i].m_iSocket, oss.str().c_str(), oss.str().size());

					close(m_vConnections[i].m_iSocket);

					std::cout << "[CLOSED] Closed " << m_vConnections[i].m_iSocket << std::endl;
				}*/

				/*std::cout << "It's been " << (std::clock() - m_vConnections[i].m_cTimer) << " microseconds on socket " << m_vConnections[i].m_iSocket << std::endl;

				std::ostringstream oss;

				oss << "HTTP/1.1 200 OK\r\n";
				oss << "Connection: close\r\n";
				oss << "Content-Type: text/html; charset=utf-8\r\n";

				write(m_vConnections[i].m_iSocket, oss.str().c_str(), oss.str().size());
				m_vConnections[i].m_bShouldQuit = true;

				close(m_vConnections[i].m_iSocket);

				std::cout << "[CLOSED] Closed " << m_vConnections[i].m_iSocket << std::endl;

				m_vConnections.erase(std::next(m_vConnections.begin(), i));*/

				//pthread_cancel(m_vConnections[i].m_thread.native_handle());
				//m_vConnections[i].m_thread.join();

				// Send the connection close stuff, if that doesn't kill the socket on its own then 
				// close the socket of the connection which will cause the read call in the loop to end
			//}

			//std::cout << i << std::endl;
			//sleep(1000000);
		}
	}
}
