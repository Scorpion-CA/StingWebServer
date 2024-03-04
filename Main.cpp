#include <iostream>
#include <string>
#include <thread>
#include <filesystem>
#include <chrono>

#include "Include/HTTP.h"
#include "Include/Config.h"

std::string GetExePath() {
	char result[256];
	ssize_t count = readlink("/proc/self/exe", result, 256);
	return std::string(result, (count > 0) ? count : 0);
}

void StartThread(std::string ip, int Port, std::string Dir, CFG* Config, bool Dashboard, HTTP* Server) { // Can't figure out how to make a thread be a constructor
	Server = new HTTP(ip, Port, Dir, Config, Dashboard);
}

void StartHouseKeepers(HTTP* MainServer, HTTP* Dashboard) { // I fucking hate chrono why didn't I use time.h
	std::cout << "[SERVER] Started Housekeeper thread" << std::endl;

	auto first = std::chrono::high_resolution_clock::now().time_since_epoch();
	auto second = std::chrono::high_resolution_clock::now().time_since_epoch();
	auto firstMS = std::chrono::duration_cast<std::chrono::milliseconds>(first).count();
	auto secondMS = std::chrono::duration_cast<std::chrono::milliseconds>(second).count();
	while(!killServer) {
		second = std::chrono::high_resolution_clock::now().time_since_epoch();
		secondMS = std::chrono::duration_cast<std::chrono::milliseconds>(second).count();
		if (secondMS - firstMS >= 1000) {
			MainServer->HouseKeeper();
			Dashboard->HouseKeeper();

			first = std::chrono::high_resolution_clock::now().time_since_epoch();
			firstMS = std::chrono::duration_cast<std::chrono::milliseconds>(first).count();
		}
	}
	//Kill the Server
}

int main() {
	std::filesystem::create_directory("Configs");
	CFG cfg;
	cfg.Init("./Configs/");

	HTTP mainServer;
	HTTP ctrlServer;

	std::thread t1(&StartThread, "0.0.0.0", 8080, "/mnt/Vault/Website/", &cfg, false, &mainServer); // Main server thread
	while(!mainInitCompleted) {}
	
	std::filesystem::create_directory("Dashboard");
	//std::thread t2(&StartThread, "0.0.0.0", 8081, GetExePath() + "./Dashboard/", &cfg);
	// Gotta make a function that parses getexepath and returns the path minus the exe name
	std::thread t2(&StartThread, "0.0.0.0", 8081, "./Dashboard/", &cfg, true, &ctrlServer);
	
	StartHouseKeepers(&mainServer, &ctrlServer);

	while(!killServer) {
		// just checks if the server is ready to die
	}

	cfg.Write("./Configs/");

	mainServer.~HTTP();
	ctrlServer.~HTTP();
	t1.join(); // Eventually make t2 require an admin login
	t2.join();

	return 0;
}
