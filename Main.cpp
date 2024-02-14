#include <iostream>
#include <string>

#include "Include/HTTP.h"

std::string GetExePath() {
	char result[256];
	ssize_t count = readlink("/proc/self/exe", result, 256);
	return std::string(result, (count > 0) ? count : 0);
}

int main() {
	HTTP http = HTTP("0.0.0.0", 8080, "/mnt/Vault/");

	// Under the present way it works, this will never be reached, will have to make 2 threads and run these 
	// 2 servers in parallel
	HTTP dash = HTTP("0.0.0.0", 8081, GetExePath());

	return 0;
}
