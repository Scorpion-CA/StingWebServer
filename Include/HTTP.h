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
#define FILE_CHUNK_LEN 4096
#define CONNECTION_TIMEOUT 10000000
#define WEBSITE_ENTRY "index.html"
// Eventually add a config option for the default option for starting file, like index.html
// Eventually add this to a config, and add a check so that if it is 0, then the option is off

// Enumerates HTTP request types for switch statement later
enum REQTYPE {
	GET = 0,
	HEAD = 1,
	POST = 2,
	PUT = 3,
	DELETE = 4,
	CONNECT = 5,
	OPTIONS = 6,
	TRACE = 7
};

class HTTP;

class HMESSAGE {
public:
	HMESSAGE(); // Default constructor
	HMESSAGE(std::string in); // Takes in a string and parses it into a formatted message
	std::string ToString(); // Formats an outgoing string to be sent to a client, does not format incoming messages
	HMESSAGE ErrorMessage(); // Just a way to return an error
private:
	REQTYPE m_iType; // The type of the message, like POST or GET
	std::string m_sVersion; // The HTTP version, generally HTTP/1.1
	std::string m_sStatus; // The status for response messages, like 200 or 404
	std::string m_sURL; // The URL of something like a GET or HEAD request
	std::string m_sContent; // The content of the HTTP response/request
	std::map<std::string, std::string> m_mHeaders; // A vector of maps storing the request/response headers
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
	std::string ReadResource(std::string Path); // Reads a file so that it can be sent to the client
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