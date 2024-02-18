#include "Include/HTTP.h"
#include "Include/Connections.h"

#include <iostream>
#include <sstream>
#include <cstring>
#include <fcntl.h>
#include <thread>
#include <sys/stat.h>

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

void HTTP::HandleMessage(int Sock, CON* Parent) {
	std::string req; // The string to hold the entire request
	char buf[HTTP_CHUNK_LEN + 1] = {0}; // char array to be read into, with an extra for a null terminator
	int count = HTTP_CHUNK_LEN; // holds the amount of bytes remaining to be read, initialized to the size of the char array - 1
	bool hasHitDoubleEndline = false; // Flag for if the client has sent the \r\n\r\n, which means the message is done
	int contentRead = 0;
	int contentStart = 0;
	int contentSize = 0;
	bool hasContent = false;
	bool hasHitEndOfContent = false;

	// HTTP Chunking, this took longer than it should have and I was so close like 80 times
	// More for large requests, I will eventually add the ability for the message to be broken up into chunks
	while(1) {
		count = HTTP_CHUNK_LEN;
		while (count == HTTP_CHUNK_LEN || !hasHitDoubleEndline || (hasContent && contentRead != contentSize)) { // Only reads if count is equal to the size of the buffer, because if it's less then that
			count = read(Sock, buf, HTTP_CHUNK_LEN); // then the last read was the last data, and doing it would get the program stuck
			
			if (count < 0) {
				std::cout << "[ERROR] Failed to read from socket | ";
				char errBuf[256];
				char* errMsg = strerror_r(errno, errBuf, 256);
				printf("[ %s ]\n", errMsg);
				return;				
			}

			if (!hasHitDoubleEndline) { // Checks if we have hit the end of the headers yet
				for (int i = 0; i < count; i++) 
					if (buf[i] == '\n' && buf[i - 1] == '\r' && buf[i - 2] == '\n' && buf[i - 3] == '\r') {
						hasHitDoubleEndline = true;
						contentStart = i;
					}
			}

			if (count == HTTP_CHUNK_LEN)
				buf[HTTP_CHUNK_LEN] = '\0'; // Adds null terminator so that it goes into the std::string properly
			if (count > 0) req += std::string(buf); // adds the array to the req std::string
			memset(buf, 0, HTTP_CHUNK_LEN + 1); // sets the array to 0s after being done with it, good practise I think
		
			if (hasHitDoubleEndline && hasContent) {
				contentRead += count;
				std::cout << contentRead << std::endl;
			}
			
			if (hasHitDoubleEndline) {		// I have no way to know if this works yet so lol
				HMESSAGE headers(req);      // I mean logically it should work but things never work logically when they should
			
				if (headers.m_mHeaders.count("Content-Length")) {
					hasContent = true;
					contentSize = std::stoi(headers["Content-Length"]);
					contentRead = count - contentSize;
				}
			}
		}
		
		if (count == 0)
			continue;

		if (req == "" || req.empty()) {
			//std::cout << "[ERROR] Recieved empty request" << std::endl; // find out if theres an HTTP response for this
			continue; // actually don't do that because the spamming thing happens after a bit and if 
		}	// we spammed the user with responses it would kill the program

		//std::cout << "______ [DEBUG] ______" << std::endl << req << std::endl << "_____________________" << std::endl;

		HMESSAGE msg(req);

		// Do processing with the message, like handle post requests and such

		switch(msg.m_iType) {
			case REQTYPE::GET: {
				GetResource(&msg, Sock, true);
				break;
			}
			case REQTYPE::HEAD: {
				GetResource(&msg, Sock, false);
				break;
			}
			case REQTYPE::POST: {
				break;
			}
			case REQTYPE::PUT: {
				break;
			}
			case REQTYPE::DELETE: {
				break;
			}
			case REQTYPE::CONNECT: {
				break;
			}
			case REQTYPE::OPTIONS: {
				break;
			}
			case REQTYPE::TRACE: {
				break;
			}
			case REQTYPE::PATCH: {
				break;
			}
			case REQTYPE::INVALID: {
				// Send error response
				break;
			}
		}

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
			  << " Port: " << ntohs(m_SocketAddr.sin_port) << std::endl;

	std::cout << "Starting HouseKeeper thread" << std::endl << std::endl;

	std::thread HouseKeeperThread(&HTTP::HouseKeeper, this); // Creates the housekeeper thread to monitor different
	HouseKeeperThread.detach();						  		 // connections and kill idle ones and detach it

	mainInitCompleted = true;

	while (true) { // main loop, should be able to be stopped through the dashboard planned to be created
		// Accept incoming connection
		if (AcceptConnection(m_iClientSocket))
			if (m_iClientSocket > 0) {
				m_vConnections.push_back(CON(this, m_iClientSocket));
			}
		// close(m_iClientSocket); Permanent Comment of shame | This motherfucker was closing the file descriptor after 
		// I made the thread lmao. Some of the more elegant solutions were probably working fine
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

int SendResource(int fd, int Sock, bool header) {

	return 0;
}

void HTTP::GetResource(HMESSAGE* msg, int Sock, bool header) {
	std::string url; // Stores the url after I check if it's just a /, pretty much here for 1 stupid reason
	std::string Path; // Stores the final Path of the requested file after doing some processing on it
	std::string ext; // Stores the extension of the file

	if (m_sDir[m_sDir.size() - 1] == '/')  // Checks if there is already a / on the directory and gets rid of it
		m_sDir.erase(std::next(m_sDir.begin(), m_sDir.size() - 1)); // so that it doesn't have 2 of them when adding
																	// the request dir to it
	Path += m_sDir;
	if (msg->m_sURL == "/" || msg->m_sURL == "" || msg->m_sURL == " " || msg->m_sURL.empty()) { // Checks if the request has a blank URL (/) and sets it to the default home page
		Path += std::string(msg->m_sURL + msg->m_sURL == "/" ? "" : "/" + std::string("/index.html"));
		url = "/index.html"; // change this to the config option when it happens because it breaks with the define
	} else
		Path += std::string(msg->m_sURL);
	
	int File = open(Path.c_str(), O_RDONLY); // Opens the requested file
	char buf[FILE_CHUNK_LEN];

	if (File < 0) {
		std::cout << "[ERROR] Cannot open file at " << Path << std::endl;
		return; // do more error checking to tell the user exactly what failed and why
	}

	bool firstDot = false; // this is just a flag for the thing below

	for (auto& i : msg->m_sURL) { // goes through the given URL and gets the extension
		if (i == '.')
			firstDot = true;
		if (firstDot)
			ext += i;
	}

	HMESSAGE out("HTTP/1.1", "200", "OK"); // Creates a new HMESSAGE to be used for the response
	out["Connection"] = "keep-alive";

	if (ext == ".html") { // Checks what type of extension the url has and sets the header accordingly
		out["Content-Type"] = "text/html; charset=utf-8";
	} else if (ext == ".css") {
		out["Content-Type"] = "text/css; charset=utf-8";
	} else if (ext == ".js") {
		out["Content-Type"] = "text/javascript; charset=utf-8";
	} else if (ext == ".jpg" || ext == ".jpeg") {
		out["Content-Type"] = "image/jpeg";
	} else if (ext == ".png") {
		out["Content-Type"] = "image/png";
	} else if (ext == ".avif") {
		out["Content-Type"] = "image/avif";
	} else if (ext == ".webp") {
		out["Content-Type"] = "image/webp";
	} else if (ext == ".svg") {
		out["Content-Type"] = "image/svg+xml";
	} else if (ext == ".gif") {
		out["Content-Type"] = "image/gif";
	} else {
		out["Content-Type"] = "*/*";
	}

	int fSize = 0; // Length of the requested file
	struct stat64 statBuf; 
	fSize = fstat64(File, &statBuf); // Finds the length of the requested file
	if (fSize != 0) { std::cout << "[ERROR] Error reading file size of " << Path << std::endl; return; }
	fSize = statBuf.st_size;

	out["Server"] = SERVER_VERSION;
	out["Content-Length"] = std::to_string(fSize);

	std::string response = out.ToString(true); // A string containing the headers to be sent to the client

	write(Sock, response.c_str(), response.size()); // Sends the headers to the client

	if (header) { // Checks to make sure this is not a HEAD request, which only wants the headers
		int count = FILE_CHUNK_LEN;
		while (count == FILE_CHUNK_LEN) { // The same as the HTTP chunking but for the file instead
			count = read(File, buf, FILE_CHUNK_LEN);
			if (count < 0) { // Checks if the File is messed up
				std::cout << "[ERROR] Error reading file at " << Path << std::endl;
				return; // sometimes the server fucks up here, not sure why
			}
			write(Sock, buf, count);	// Need to figure out why this is slow, gotta know if it's my code being
			memset(buf, 0, FILE_CHUNK_LEN); // shit or if it's just the server having bad hardware
						// From what it looks like, the initial connection is slow for images but it's fine on the same connection
		} // This whole thing is why I want the Handler to wait until it knows it has reached the end of the request
	}
	close(File); // Closes the file desctriptor so linux doesn't screech at me
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

	if (!in.empty() && in != "" && in != " ") 
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

	if (!items.empty() && items.size() > 2) { // Checks to make sure there is actually 3 items in items
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
		else if (items[0] == "PATCH")
			m_iType = REQTYPE::PATCH;
		else 
			m_iType = REQTYPE::INVALID;

		m_sURL = items[1]; // Both of these just take the appropriate item from the vector of items and puts it in the
		m_sVersion = items[2]; // proper variable

		items.clear(); // Not really necessary but clears the vector of items, saves something like 20 bytes of memory
		return;
	}

	// Make something that sends an invalid HTTP request response if it can't get the things above
}

HMESSAGE::HMESSAGE(std::string Version, std::string Status, std::string Type) {
	m_sVersion = Version;
	m_sStatus = Status;
	m_sResType = Type;
}

std::string HMESSAGE::ToString(bool Response) {
	std::string out;
	if (Response)
		out += (m_sVersion + " " + m_sStatus + " " + m_sResType);
	else
		out += (m_iType + " " + m_sURL + " " + m_sVersion);
	out += "\r\n";
	for (auto& i : m_mHeaders)
		out += (i.first + ": " + i.second + "\r\n");
	out += "\r\n"; // since content is sent differently it is handled in a different function
	return out;
}

HMESSAGE HMESSAGE::ErrorMessage() { // eventually add error message
	HMESSAGE out;
	out.m_sContent = "[ERROR]";
	return out;
}

void HTTP::HouseKeeper() { // for some reason, this only works if opera connects? what?
	while(1) {	// This code causes seg faults sometimes, need to add some metric in order to do this without
		//for (int i = 0; i < m_vConnections.size(); i++) {	// seg faulting because this is necessary functionality
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

			//std::cout << "Testing the Housekeeper loop" << std::endl;
			//sleep(1000000);
		//}
	}
}
/*
	In case I forget to come back to this tonight, the thread for some reason doesn't start when a phone joins
	and I want to weed out all the causes and find out if it's because of this thread or if it's because of the
	way I handle requests, still freezing when the program starts for some reason, gotta figure out why
*/