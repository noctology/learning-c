Port server.c to Windows:

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

    WSAStartup   - 0 = Success.  On error, the function returns the error code.
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
    send         - Value greater than 0 is the number of bytes sent.  On error,
                   SOCKET_ERROR is returned.  Call WSAGetLastError.
    CreateThread - On error, NULL is returned.  Call GetLastError.

 8. Change socket declarations from int to SOCKET.
    (SOCKET is typedef'ed to UINT_PTR).
    (UNIT_PTR is typedef'ed to unsigned int.)

 9. setsockopt value (argument 4) must be declared BOOL and cast to char *
    for switch values:
    BOOL yes=TRUE;
    (char *)&yes

10. bind address length (third argument) must be int, not size_t.

11. Replace close with closesocket.

12. Windows does not have fork().  Use CreateThread as described below:

    a. Windows does not have waitpid so remove sigchld_handler(),
       struct sigaction sa, and block of code that calls sigaction().
       As I understand it, Windows will clear all threads and their memory
       when the process exits but don't quote me on this. :)

    b. Replace fork() with CreateThread (see sample code for my solution):

       Replace:
          if (!fork()) { // this is the child process
             closesocket(sockfd); // child doesn't need the listener
             if (send(new_fd, "Hello, world!", 13, 0) == -1)
                perror("send");
             closesocket(new_fd);
             exit(0);
          }
          closesocket(new_fd);  // parent doesn't need this

       with:
          h_thread = CreateThread(NULL,0,thread_function,&new_fd,0,
                                  &dw_thread_id);
          if (h_thread == NULL)
          {
             fprintf(stderr,"CreateThread failed.\n");
             closesocket(new_fd); // No longer needed
          }

       Add:
          DWORD WINAPI thread_function(LPVOID lpParam)
          {
             char   *nc_error;
             DWORD   dw_error;
             DWORD   dw_exit_code;
             SOCKET  *p_new_fd;
             int      i_status;

             dw_exit_code = 0;
             p_new_fd = (SOCKET *)lpParam;

             i_status = send(*p_new_fd,"Hello, world!",13,0);
             if (i_status == SOCKET_ERROR)
             {
                dw_error = (DWORD)WSAGetLastError();
                get_msg_text(dw_error,
                             &nc_error);
                fprintf(stderr,"send failed with code %ld\n",dw_error);
                fprintf(stderr,"%s\n",nc_error);
                LocalFree(nc_error);

                dw_exit_code = 1;
             }

             closesocket(*p_new_fd); // No longer needed
             return(dw_exit_code);
          }
