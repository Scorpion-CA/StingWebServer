#include <vector>
#include <iostream>

#include "Include/Connections.h"
#include "Include/HTTP.h"

CON::CON(HTTP* Parent, int Sock) {
	m_iSocket = Sock;
	m_pParent = Parent;

	m_thread = std::thread(&CON::Loop, this, Sock);
    m_thread.detach();
}

void CON::Loop(int Sock) {
	m_pParent->HandleMessage(Sock, this);
}
