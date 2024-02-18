#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string>
#include <map>
#include <unistd.h>
#include <vector>
#include <thread>
#include <time.h>

class CONNECTION;
class CON;

//change this to a veriable set in config, whenever I get around to that
#define MAX_CLIENTS 1024
#define HTTP_CHUNK_LEN 2048
#define FILE_CHUNK_LEN 65536
#define CONNECTION_TIMEOUT 10000000
#define WEBSITE_ENTRY "index.html"
#define SERVER_VERSION "Sting Web Server - Linux - 0.0.1"
// Eventually add a config option for the default option for starting file, like index.html
// Eventually add this to a config, and add a check so that if it is 0, then the option is off

inline bool mainInitCompleted = false; // I know, global variables booooo but I needed a way to make sure main was done

// Enumerates HTTP request types for switch statement later
enum REQTYPE {
	GET = 0,
	HEAD = 1,
	POST = 2,
	PUT = 3,
	DELETE = 4,
	CONNECT = 5,
	OPTIONS = 6,
	TRACE = 7,
	PATCH = 8,
	INVALID = 9
};

class HTTP;

class HMESSAGE {
public:
	friend HTTP;
public:
	HMESSAGE(); // Default constructor
	HMESSAGE(std::string in); // Takes in a string and parses it into a formatted message
	HMESSAGE(std::string Version, std::string Status, std::string Type); // Makes a new message for an outgoing packet
	std::string ToString(bool Response); // Formats the headers for an outgoing message
	HMESSAGE ErrorMessage(); // Just a way to return an error
	std::string& operator[] (std::string in) { // Allows you to index into the headers map in a cleaner way
		return m_mHeaders[in];
	}
private:
	REQTYPE m_iType; // The type of the message, like POST or GET
	std::string m_sResType; // The type of the response
	std::string m_sVersion; // The HTTP version, generally HTTP/1.1
	std::string m_sStatus; // The status for response messages, like 200 or 404
	std::string m_sURL; // The URL of something like a GET or HEAD request
	std::string m_sContent; // The content of the HTTP response/request
	std::map<std::string, std::string> m_mHeaders; // A map storing the request/response headers
};

class HTTP {
public:
	friend CON;
public:
	HTTP();
	HTTP(std::string IPAddr, int Port, std::string Dir); // Initializes the server and sets a directory for the website
	~HTTP(); // Calls CloseServer
public: // I don't like this but it didn't play nice with threads
	int StartServer(); // Starts the server
	void CloseServer(); // Closes open ports and exits the process
	bool StartListen(); // Sets up the connection listening
	void HandleMessage(int Sock, CON* Parent); // Handles requests, puts them into an std::string to be parsed and formatted
	bool AcceptConnection(int& ClientSocket); // Accepts a pending connection
	std::string FormatHeaders(HMESSAGE in); // Formats the headers to be sent
	void GetResource(HMESSAGE* msg, int Sock, bool header); // Reads a document so that it can be sent to the client
	int SendDocument(int fd, int Sock); // Reads a text document, returns how many bytes are left so it can be looped
	int SendResource(int fd, int Sock, bool header); // Reads a resource in binary, returns how many bytes are left so it can be looped
private:
	std::string m_sIPAddr; // IP address the server is to be hosted on
	int m_iPort; // The port the server uses, generally 80 or 8080, 8081 for the dashboard
	int m_iSocket; // The file descriptor for the socket the server is using
	int m_iClientSocket; // The file descriptor for a client socket
	long m_lIncomingMessage; // The incoming message
	sockaddr_in m_SocketAddr; // A struct containing info about the connection
	unsigned int m_uSocketAddressLen; // Length of the SockAddr structure
	std::string m_sDir; // The directory for the server, may need to be cleansed so it can't be used to ../ into the root directory
private:
	void HouseKeeper(); // Loops through the list of connections and kills any that have been idle too long
	std::vector<CON> m_vConnections; // Stores active connections, need to figure out if there's a way to like
};									 // loop through it and make it smaller and get rid of the stale connection inside
									 // could be done by making it a pointer, and having part of the housekeeper
									 // function dedicated to occasionally making a new vector, looping through this
									 // one, and adding only the active ones, switching the pointer to that vector
									 // and then deleting what this one used to be