/*
The MIT License (MIT)

Copyright (c) 2018 James Boer

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "ScsInternal.h"

// FD_SET macro triggers this warning
#pragma warning( disable : 4127 )

using namespace Scs;


Socket::Socket(AddressPtr address) :
	m_address(address),
	m_socket(INVALID_SOCKET)
{
	// Create the SOCKET
	if (!m_address)
	{
		LogWriteLine("Null socket error");
		return;
	}
	addrinfo * addr = m_address->GetCurrent();
	if (!addr)
	{
		LogWriteLine("Null socket address error");
		return;
	}
	m_socket = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
	if (m_socket == INVALID_SOCKET)
	{
		LogWriteLine("Error at socket(): %ld", SocketLastError);
	}

}

Socket::Socket(AddressPtr address, SOCKET sckt) :
	m_address(address),
	m_socket(sckt)
{
}

Socket::~Socket()
{
	if (m_socket != INVALID_SOCKET)
	{
		shutdown(m_socket, SD_SEND);
#ifdef SCS_WINDOWS
		closesocket(m_socket);
#else
        close(m_socket);
#endif
	}
}

SocketPtr Socket::Accept()
{
	SOCKET newSocket = INVALID_SOCKET;
	newSocket = accept(m_socket, nullptr, nullptr);
	if (newSocket == INVALID_SOCKET)
	{
		LogWriteLine("Socket accept failed with error: %d", SocketLastError);
		return nullptr;
	}
	return CreateSocket(m_address, newSocket);
}

bool Socket::Bind(addrinfo * addr)
{
	// Setup the TCP listening socket
	int result = bind(m_socket, addr->ai_addr, (int)addr->ai_addrlen);
	int lastError = SocketLastError;
	if (result == SOCKET_ERROR && lastError != LAIR_EWOULDBLOCK)
	{
		LogWriteLine("Bind failed with error: %d", lastError);
		return false;
	}
	return true;
}

bool Socket::Connect()
{
	if (m_socket == INVALID_SOCKET)
	{
		LogWriteLine("Invalid socket");
		return false;
	}

	// Connect to server.
	addrinfo * addr = m_address->GetCurrent();
	int result = connect(m_socket, addr->ai_addr, (int)addr->ai_addrlen);
	int lastError = SocketLastError;
	if (result == SOCKET_ERROR && lastError != LAIR_EWOULDBLOCK)
	{
		LogWriteLine("Socket connect failed: %d", lastError);
		return false;
	}
	return true;
}

bool Socket::IsInvalid() const
{
	if (m_socket == INVALID_SOCKET)
		return true;
	fd_set socketSet;
	FD_ZERO(&socketSet);
	FD_SET(m_socket, &socketSet);
	timeval tmval;
	tmval.tv_sec = 0;
	tmval.tv_usec = 1000;
	int result = select(0, nullptr, nullptr, &socketSet, &tmval);
	return (result == 1) ? true : false;
}

bool Socket::IsReadable() const
{
	fd_set socketSet;
	FD_ZERO(&socketSet);
	FD_SET(m_socket, &socketSet);
	timeval tmval;
	tmval.tv_sec = 0;
	tmval.tv_usec = 1000;
	int result = select(0, &socketSet, nullptr, nullptr, &tmval);
	return (result == 1) ? true : false;
}

bool Socket::IsWritable() const
{
	fd_set socketSet;
	FD_ZERO(&socketSet);
	FD_SET(m_socket, &socketSet);
	timeval tmval;
	tmval.tv_sec = 0;
	tmval.tv_usec = 1000;
	int result = select(0, nullptr, &socketSet, nullptr, &tmval);
	return (result == 1) ? true : false;
}

bool Socket::Listen()
{
	// Connect to server.
	int result = listen(m_socket, SOMAXCONN);
	int lastError = SocketLastError;
	if (result == SOCKET_ERROR && lastError != LAIR_EWOULDBLOCK)
	{
		LogWriteLine("Socket listen failed: %d", lastError);
		return false;
	}
	return true;
}

size_t Socket::Receive(void * data, size_t bytes, uint32_t flags)
{
	auto received = recv(m_socket, static_cast<char *>(data), static_cast<int>(bytes), flags);
	if (received == SOCKET_ERROR || received <= 0)
		return 0;
	return static_cast<size_t>(received);
}

bool Socket::Send(void * data, size_t bytes, uint32_t flags, size_t * bytesSent)
{
	assert(bytesSent);
	ssize_t sent = send(m_socket, static_cast<const char*>(data), static_cast<int>(bytes), flags);
	int lastError = SocketLastError;
	if (sent == SOCKET_ERROR || sent <= 0)
	{
		LogWriteLine("Socket send failed: %d", lastError);
		return false;
	}
	*bytesSent += static_cast<size_t>(sent);
	return true;
}

void Socket::SetNonBlocking(bool nonBlocking)
{
	// Set socket to non-blocking
	u_long mode = nonBlocking ? 1 : 0;
#ifdef SCS_WINDOWS
	ioctlsocket(m_socket, FIONBIO, &mode);
#else
	ioctl(m_socket, FIONBIO, &mode);
#endif
}

void Socket::SetNagle(bool nagle)
{
	// Turn nagling on and off for this socket
	uint32_t flag = nagle ? 1 : 0;
    setsockopt(m_socket, SOL_SOCKET, TCP_NODELAY, (char *) &flag, sizeof(int));
}

SocketPtr Scs::CreateSocket(AddressPtr address)
{
	return std::allocate_shared<Socket>(Allocator<Address>(), address);
}

SocketPtr Scs::CreateSocket(AddressPtr address, SOCKET sckt)
{
	return std::allocate_shared<Socket>(Allocator<Address>(), address, sckt);
}

