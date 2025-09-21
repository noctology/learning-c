/******************************************************************************/
/*                                                                            */
/* Application: WSselect                                                      */
/*                                                                            */
/* File:        WSselect.c                                                    */
/*                                                                            */
/* Purpose:     Demonstrate use of the "select" function.  The original       */
/*              select.c uses stdin which is a file descriptor.  This works   */
/*              in Unix/Linux because file descriptors and socket descriptors */
/*              are essentially the same and even share some functions (e.g., */
/*              close).  This is not true in Windows.  This program is a      */
/*              Windows program that selects a socket for the demonstration.  */
/*                                                                            */
/* Modifications:                                                             */
/*                                                                            */
/*           Name           Date                     Reason                   */
/*    ------------------ ---------- ----------------------------------------- */
/*    Steven C. Mitchell 2022-12-12 Orignal creation                          */
/*    Steven C. Mitchell 2023-01-04 Fixed code output if getaddrinfo error    */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Prevent automatic include of winsock.h which does not play nice with       */
/* winsock2.h:                                                                */
/*                                                                            */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <stdio.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define PORT "3490" // the port users will be connecting to
#define BACKLOG 10  // how many pending connections queue will hold

void get_msg_text(DWORD, char**);
SOCKET open_a_socket(char*);
/*                                                                            */
/******************************************************************************/
/*                                                                            */
int main(void)
{
    char* nc_error;
    DWORD           dw_error;
    fd_set           s_readfds;
    SOCKET             sockfd;
    int              i_status;
    struct timeval   s_time_value;
    WSADATA          s_wsaData;
    /*                                                                            */
    /* Initialize Winsock and request version 2.2:                                */
    /*                                                                            */
    i_status = WSAStartup(MAKEWORD(2, 2), &s_wsaData);

    if (i_status != 0)
    {
        dw_error = (DWORD)i_status;
        get_msg_text(dw_error,
            &nc_error);
        fprintf(stderr, "WSAStartup failed with code %d.\n", i_status);
        fprintf(stderr, "%s\n", nc_error);
        LocalFree(nc_error);

        return 1;
    }
    /*                                                                            */
    /* Verify that version 2.2 is available:                                      */
    /*                                                                            */
    if (LOBYTE(s_wsaData.wVersion) < 2 ||
        HIBYTE(s_wsaData.wVersion) < 2)
    {
        fprintf(stderr, "Version 2.2 of Winsock is not available.\n");
        WSACleanup();

        return 2;
    }
    /*                                                                            */
    /* Open a socket:                                                             */
    /*                                                                            */
    sockfd = open_a_socket(PORT);

    if (sockfd == INVALID_SOCKET)
    {
        WSACleanup();

        return 3;
    }
    /*                                                                            */
    /* Listen for a connection:                                                   */
    /*                                                                            */
    i_status = listen(sockfd, BACKLOG);

    if (i_status == SOCKET_ERROR)
    {
        dw_error = (DWORD)WSAGetLastError();
        get_msg_text(dw_error,
            &nc_error);
        fprintf(stderr, "listen failed with code %ld.\n", dw_error);
        fprintf(stderr, "%s\n", nc_error);
        LocalFree(nc_error);
        closesocket(sockfd);
        WSACleanup();

        return 4;
    }
    /*                                                                            */
    /* Call select on this socket:                                                */
    /*                                                                            */
    s_time_value.tv_sec = 4;
    s_time_value.tv_usec = 1000000;

    FD_ZERO(&s_readfds);
    FD_SET(sockfd, &s_readfds);
    /* don't care about writefds and exceptfds:                                   */
    select(sockfd + 1, &s_readfds, NULL, NULL, &s_time_value);
    /* Let the user know what happened:                                           */
    if (FD_ISSET(sockfd, &s_readfds))
    {
        send(sockfd, "Select successful.", 18, 0);
        printf("A client connected!\n");
    }
    else
    {
        printf("Timed out.\n");
    }
    /*                                                                            */
    /* Close down Winsock:                                                        */
    /*                                                                            */
    closesocket(sockfd);

    WSACleanup();
    /*                                                                            */
    /* Return success code:                                                       */
    /*                                                                            */
    return 0;
}
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Get Error Message Text:                                                    */
/*                                                                            */
void get_msg_text
(
    DWORD    dw_error, /* in   - Error code                                    */
    char** pnc_msg    /* out  - Error message                                 */
)
{
    DWORD dw_flags;
    /*                                                                            */
    /* Set message options:                                                       */
    /*                                                                            */
    dw_flags = FORMAT_MESSAGE_ALLOCATE_BUFFER
        | FORMAT_MESSAGE_FROM_SYSTEM
        | FORMAT_MESSAGE_IGNORE_INSERTS;
    /*                                                                            */
    /* Create the message string:                                                 */
    /*                                                                            */
    FormatMessage(dw_flags, NULL, dw_error, LANG_SYSTEM_DEFAULT, (LPTSTR)pnc_msg, 0,
        NULL);
}
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Open a socket on the specified port:                                       */
/*                                                                            */
SOCKET open_a_socket
(
    char* nc_port /* in   - Port to use for connection                         */
)
{
    struct addrinfo* ps_address;
    struct addrinfo* ps_address_list;
    int               i_addrlen;
    char* nc_error;
    DWORD            dw_error;
    struct addrinfo   s_hints;
    SOCKET              sockfd;
    int               i_status;
    BOOL              B_yes;
    /*                                                                            */
    /* Set the desired IP address characteristics:                                */
    /*                                                                            */
    memset(&s_hints, 0, sizeof(s_hints));

    s_hints.ai_family = AF_UNSPEC;
    s_hints.ai_socktype = SOCK_STREAM;
    s_hints.ai_flags = AI_PASSIVE; // use my IP
    /*                                                                            */
    /* Request a list of IP addresses:                                            */
    /*                                                                            */
    i_status = getaddrinfo(NULL, PORT, &s_hints, &ps_address_list);

    if (i_status != 0)
    {
        dw_error = (DWORD)WSAGetLastError();
        get_msg_text(dw_error,
            &nc_error);
        fprintf(stderr, "getaddrinfo failed with code %ld.\n", dw_error);
        fprintf(stderr, "%s\n", nc_error);
        LocalFree(nc_error);

        return(INVALID_SOCKET);
    }
    /*                                                                            */
    /* Loop through the results and bind to the first we can:                     */
    /*                                                                            */
    for (ps_address = ps_address_list;
        ps_address != NULL;
        ps_address = ps_address->ai_next)
    {
        /* Attempt to open the socket:                                                */
        sockfd = socket(ps_address->ai_family, ps_address->ai_socktype,
            ps_address->ai_protocol);
        if (sockfd == INVALID_SOCKET)
        {
            continue;
        }
        /* Allow reuse of the socket:                                                 */
        B_yes = TRUE;
        i_status = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&B_yes,
            sizeof(B_yes));
        if (i_status == SOCKET_ERROR)
        {
            dw_error = (DWORD)WSAGetLastError();
            get_msg_text(dw_error,
                &nc_error);
            fprintf(stderr, "setsockopt failed with code %ld.\n", dw_error);
            fprintf(stderr, "%s\n", nc_error);
            LocalFree(nc_error);
            closesocket(sockfd);
            freeaddrinfo(ps_address_list);

            return(INVALID_SOCKET);
        }
        /* Bind to the socket:                                                        */
        i_addrlen = (int)ps_address->ai_addrlen;
        i_status = bind(sockfd, ps_address->ai_addr, i_addrlen);

        if (i_status == SOCKET_ERROR)
        {
            closesocket(sockfd);

            continue;
        }

        break;
    }
    /*                                                                            */
    /* Check for a connection:                                                    */
    /*                                                                            */
    if (ps_address == NULL)
    {
        fprintf(stderr, "Failed to bind to a socket.\n");
        closesocket(sockfd);
        freeaddrinfo(ps_address_list);

        return(INVALID_SOCKET);
    }
    /*                                                                            */
    /* Successful so return the socket:                                           */
    /*                                                                            */
    freeaddrinfo(ps_address_list);

    return(sockfd);
}

// Last modified: 2023-01-04 01:14 (MST)
