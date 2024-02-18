#include <iostream>
#include <string>
#include <thread>
#include <filesystem>

#include "Include/HTTP.h"
#include "Include/Config.h"

std::string GetExePath() {
	char result[256];
	ssize_t count = readlink("/proc/self/exe", result, 256);
	return std::string(result, (count > 0) ? count : 0);
}

void StartThread(std::string ip, int Port, std::string Dir) { // Can't figure out how to make a thread be a constructor
	HTTP http = HTTP(ip, Port, Dir);
}

int main() {
	std::filesystem::create_directory("Configs");
	std::thread t1(&StartThread, "0.0.0.0", 8080, "/mnt/Vault/Website/"); // Main server thread
	while(!mainInitCompleted) {}
	
	std::filesystem::create_directory("Dashboard");
	std::thread t2(&StartThread, "0.0.0.0", 8081, GetExePath() + "/Dashboard/");
	t1.join();
	t2.join();
	return 0;
}
