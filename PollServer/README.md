Port pollserver.c to Windows:

 1. Replace socket headers with winsock2.h

 2. Include ws2tcpip.h

 3. Unlike most Windows APIs, you do not need to include windows.h before
    including winsock2.h.  If windows.h is needed, add these lines before
    including any windows headers:
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif

    This will prevent windows.h from including winsock.h which will cause
    duplicate definition errors when winsock2.h is included.

    Note that this prevents a number of headers from being included
    automatically so it may be necessary to include them as needed.

 4. It is necessary to initialize Winsock before using it.  This is also where
    you specify the desired version of Winsock.  Need Winsock version 2.2 to get
    IPv6 support.

    WSADATA wsaData; 
    int status = WSAStartup(MAKEWORD(2,2),wsaData);

    Check that status == 0.  If not, initialization failed.

    Check that both the low and high order bytes of wsaData.wVersion are 2.
    If not, version 2.2 is not available.

 5. Call WSACleanup before all return and exit.

 6. Add Ws2_32.lib to the list of needed libraries before linking.

 7. Error handling is different than Unix/Linux.  In most cases, the error
    code is retrieved by calling WSAGetLastError.  FormatMessage is used to
    convert the error code to an error message.  errno is not used by Winsock
    so perror will not return useful information.

    Note that gai_strerror is not thread safe and its use is not recommended.

    WSAStartup   - 0 = Success.  On error, the function returns the erorr code.
                   Do NOT call WSAGetLastError.
    getaddrinfo  - 0 = Success.  This function returns an error code but it may
                   not be the correct error code.  Call WSAGetLastError.
    socket       - Positive interger value = Success.  On error, INVALID_SOCKET
                   is returned.  Call WSAGetLastError.
    setsockopt   - 0 = Success.  On error, SOCKET_ERROR is returned.  Call
                   WSAGetLastError.
    bind         - 0 = Success.  On error, SOCKET_ERROR is returned.  Call
                   WSAGetLastError.
    listen       - 0 = Success.  On error, SOCKET_ERROR is returned.  Call
                   WSAGetLastError.
    accept       - Positive interger value = Success.  On error, INVALID_SOCKET
                   is returned.  Call WSAGetLastError.
    recv         - Value greater than 0 is the number of bytes received.  0
                   means the socket was closed by the server.  On error,
                   SOCKET_ERROR is returned.  Call WSAGetLastError.
    send         - Value greater than 0 is the number of bytes sent.  On error,
                   SOCKET_ERROR is returned.  Call WSAGetLastError.
    WSAPoll      - 0  = Timer expired.  Greater than 0 = number of elements in
                   array with updates.  On error, SOCKET_ERROR is returned.
                   Call WSAGetLastError.

 8. Change socket declarations from int to SOCKET.
    (SOCKET is typedef'ed to UINT_PTR).
    (UNIT_PTR is typedef'ed to unsigned int.)

 9. Change "struct pollfd" to "WSAPOLLFD".

10. Change call to "poll" to "WSAPoll".

    All of the structrues in the WSAPOLLFD array must be initialized before
    they are used.  The fd element must be set to a negative number so that
    WSAPOLL will ignore that socket.  I am not certain how this makes much
    sense since SOCKET ends up typedef'ed to an unsigned int but without
    this, it gets very confused.

    On hangup, WSAPOLL returns POLLHUP, not POLLIN.

11. setsockopt value (argument 4) must be declared BOOL and cast to char *
    for switch values:
    BOOL yes=TRUE;
    (char *)&yes

12. Third argument of accept must be declared int, not size_t.

13. Replace close with closesocket.

-----------------

PollServer

Implement a crude, multi-person chat server to demonstrate using WSAPoll on multiple servers. Note the name of the function in Windows is "WSAPoll", not "poll".
https://www.tallyhawk.net/WinsockExamples/pollserver_README.txt
https://www.tallyhawk.net/WinsockExamples/WSpollserver.c

Another thing to note is that telnet in Windows works differently than in Unix/Linux. Unix/Linux telnet is line buffered. This means that nothing is sent until the user presses the <Enter> key. Windows telnet is not line buffered. It sends each character as it is typed. To send an entire line, return to the telnet prompt (usually <Ctrl-]>) and type "send", a space, and the message followed by the <Enter> key.

A final note is that telnet is not available by default in Windows. It must be installed and/or activated before it can be used. Use your favorite internet search engine to look for "Windows telnet" to find instructions.
