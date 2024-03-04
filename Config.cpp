#include <unistd.h>
#include <string>
#include <string.h>
#include <iostream>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>

#include "Include/Config.h"

void CFG::InitProps() {
    this->m_mProps["MAX_CONNECTIONS"] = "1024";
    this->m_mProps["HTTP_CHUNK_LEN"] = "8192";
    this->m_mProps["FILE_CHUNK_LEN"] = "65535";
    this->m_mProps["CONNECTION_TIMEOUT"] = "10000000";
    this->m_mProps["WEBSITE_ENTRY"] = "index.html";
    this->m_mProps["SERVER_VERSION"] = "Sting Web Server - Linux - 0.0.1";
}

bool CFG::Init(std::string Path) {
    InitProps();
    int File = open(std::string(Path + "ServerProperties.cfg").c_str(), O_CREAT | O_RDWR, 0666);
    if (File < 0) {
        std::cout << "[CONFIG ERROR] Config file not found, creating new with default values" << std::endl;
        char errBuf[256];
		char* errMsg = strerror_r(errno, errBuf, 256);
		printf("[ %s ]\n", errMsg);
        CFG::InitProps();
        return false;
    }

    int fSize = 0; // Length of the requested file
	struct stat64 statBuf; 
	fSize = fstat64(File, &statBuf); // Finds the length of the requested file
	if (fSize != 0) {
        std::cout << "[CONFIG ERROR] Cannot get stats for config file" << std::endl;
        char errBuf[256];
		char* errMsg = strerror_r(errno, errBuf, 256);
		printf("[ %s ]\n", errMsg);
        CFG::InitProps();
        return false;
    }
	fSize = statBuf.st_size;

    char* cfg = (char*)malloc(fSize + 1);

    if (read(File, cfg, fSize) < 0) {
        std::cout << "[CONFIG ERROR] Error reading config file, creating new with default values" << std::endl;
        CFG::InitProps(); // Eventually make it so that if there is an error reading one of the config values it will save the old config as "CONFIG_OLD" or something so that if someone had a bunch of properties they're not lost
        return false; // Also add error checking
    }

    std::string prop;
    std::string val;
    bool value = false;
    for (int i = 0; i < fSize; i++) {
        if (cfg[i] == '\n') {
            m_mProps[prop] = val;
            value = false;
            prop.clear();
            val.clear();
            continue;
        }
        if (cfg[i] == '=' && !value) {
            value = true;
            continue;
        }
        if (value) {
            val += cfg[i];
            continue;
        }
        prop += cfg[i];
    }

    close(File);
    std::cout << "[CONFIG] Successfully loaded config" << std::endl;
    return true;
}

bool CFG::Write(std::string Path) {
    int File = open(std::string(Path + "ServerProperties.cfg").c_str(), O_CREAT | O_RDWR, 0666);

    for (auto& i : m_mProps) {
        std::string line = i.first + "=" + i.second + "\n";
        write(File, line.c_str(), line.size());
    }

    close(File);
    std::cout << "[CONFIG] Saving Config" << std::endl;
    return true;
}
