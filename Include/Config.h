#pragma once
#include <map>

class CFG {
public:
    bool Init(std::string Path);
    void InitProps(); // Only used when config does not exist, or when there is an error reading it
    bool Write(std::string Path);

    std::string& operator[] (std::string in) {
        return m_mProps[in];
    }
private:
    std::map<std::string, std::string> m_mProps;
};