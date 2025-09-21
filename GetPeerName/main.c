/******************************************************************************/
/*                                                                            */
/* Application: WSgetpeername                                                 */
/*                                                                            */
/* File:        main.c                                                        */
/*                                                                            */
/* Purpose:     Use getpeername to identify host to which a socket is         */
/*              connected.                                                    */
/*                                                                            */
/* Modifications:                                                             */
/*                                                                            */
/*           Name           Date                     Reason                   */
/*    ------------------ ---------- ----------------------------------------- */
/*    Steven C. Mitchell 2022-11-14 Orignal creation                          */
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
#include <string.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define PORT "3490" // the port client will be connecting to 

#define MAXDATASIZE 100 // max number of bytes we can get at once 

void* get_in_addr(struct sockaddr*);

void get_msg_text(DWORD, char**);


int main(int argc, char* argv[])
{
    struct sockaddr_in  s_ip4_peer;
    struct sockaddr_in6 s_ip6_peer;
    socklen_t             addrlen;

    struct addrinfo* ps_address;
    int               i_addrlen;
    char* nc_error;
    DWORD            dw_error;
    struct addrinfo   s_hints;
    char             ac_host[NI_MAXHOST];
    char             ac_server[INET6_ADDRSTRLEN];
    char             ac_serv[NI_MAXSERV];
    struct addrinfo* ps_servinfo;
    SOCKET              sockfd;
    int               i_status;
    WSADATA           s_wsaData;
    /*                                                                            */
    /* The program expects one command line argument, the host's name:            */
    /*                                                                            */
    if (argc != 2)
    {
        fprintf(stderr, "usage: WSgetpeername hostname\n");

        return 1;
    }
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

        return 2;
    }
    /*                                                                            */
    /* Verify that version 2.2 is available:                                      */
    /*                                                                            */
    if (LOBYTE(s_wsaData.wVersion) < 2 ||
        HIBYTE(s_wsaData.wVersion) < 2)
    {
        fprintf(stderr, "Version 2.2 of Winsock is not available.\n");
        WSACleanup();

        return 3;
    }
    /*                                                                            */
    /* Set the desired IP address characteristics:                                */
    /*                                                                            */
    memset(&s_hints, 0, sizeof(s_hints));

    s_hints.ai_family = AF_UNSPEC;   // AF_INET or AF_INET6 to force version
    s_hints.ai_socktype = SOCK_STREAM; // Streaming socket
    /*                                                                            */
    /* Request the list of matching IP addresses for the specified host:          */
    /*                                                                            */
    i_status = getaddrinfo(argv[1], PORT, &s_hints, &ps_servinfo);

    if (i_status != 0)
    {
        dw_error = (DWORD)WSAGetLastError();
        get_msg_text(dw_error,
            &nc_error);
        fprintf(stderr, "getaddrinfo failed with code %ld.\n", dw_error);
        fprintf(stderr, "%s\n", nc_error);
        LocalFree(nc_error);
        WSACleanup();

        return 4;
    }
    /*                                                                            */
    /* Loop through all the results and connect to the first we can:              */
    /*                                                                            */
    for (ps_address = ps_servinfo;
        ps_address != NULL;
        ps_address = ps_address->ai_next)
    {
        sockfd = socket(ps_address->ai_family, ps_address->ai_socktype,
            ps_address->ai_protocol);
        if (sockfd == INVALID_SOCKET)
        {
            dw_error = (DWORD)WSAGetLastError();
            get_msg_text(dw_error,
                &nc_error);
            fprintf(stderr, "socket failed with code %ld.\n", dw_error);
            fprintf(stderr, "%s\n", nc_error);
            LocalFree(nc_error);

            continue;
        }

        i_addrlen = (int)ps_address->ai_addrlen;
        i_status = connect(sockfd, ps_address->ai_addr, i_addrlen);

        if (i_status == SOCKET_ERROR)
        {
            dw_error = (DWORD)WSAGetLastError();
            get_msg_text(dw_error,
                &nc_error);
            fprintf(stderr, "connect failed with code %ld.\n", dw_error);
            fprintf(stderr, "%s\n", nc_error);
            LocalFree(nc_error);
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
        fprintf(stderr, "Failed to connect.\n");
        freeaddrinfo(ps_servinfo);
        WSACleanup();

        return 5;
    }
    /*                                                                            */
    /* Tell user to which server a connection was made:                           */
    /*                                                                            */
    inet_ntop(ps_address->ai_family,
        get_in_addr((struct sockaddr*)ps_address->ai_addr),
        ac_server, sizeof(ac_server));

    printf("Connecting to %s\n", ac_server);
    /*                                                                            */
    /* Free the list of addresses:                                                */
    /*                                                                            */
    freeaddrinfo(ps_servinfo);
    /*                                                                            */
    /* Get the name of the server to which the client connected:                  */
    /*                                                                            */
    if (ps_address->ai_family == AF_INET)
    {
        memset(&s_ip4_peer, 0, sizeof(s_ip4_peer));
        addrlen = sizeof(s_ip4_peer);
        i_status = getpeername(sockfd, (struct sockaddr*)&s_ip4_peer, &addrlen);
    }
    else // AF_INET6
    {
        memset(&s_ip6_peer, 0, sizeof(s_ip6_peer));
        addrlen = sizeof(s_ip6_peer);
        i_status = getpeername(sockfd, (struct sockaddr*)&s_ip6_peer, &addrlen);
    }

    if (i_status == SOCKET_ERROR)
    {
        dw_error = (DWORD)WSAGetLastError();
        get_msg_text(dw_error,
            &nc_error);
        fprintf(stderr, "getpeername failed with code %ld.\n", dw_error);
        fprintf(stderr, "%s\n", nc_error);
        LocalFree(nc_error);
        closesocket(sockfd);

        return 3;
    }

    if (ps_address->ai_family == AF_INET)
    {
        addrlen = sizeof(s_ip4_peer);
        i_status = getnameinfo((struct sockaddr*)&s_ip4_peer, addrlen, ac_host,
            sizeof(ac_host), ac_serv, sizeof(ac_serv), 0);
    }
    else
    {
        addrlen = sizeof(s_ip6_peer);
        i_status = getnameinfo((struct sockaddr*)&s_ip6_peer, addrlen, ac_host,
            sizeof(ac_host), ac_serv, sizeof(ac_serv), 0);
    }

    if (i_status != 0)
    {
        dw_error = (DWORD)WSAGetLastError();
        get_msg_text(dw_error,
            &nc_error);
        fprintf(stderr, "getnameinfo failed with code %ld\n", dw_error);
        fprintf(stderr, "%s\n", nc_error);
        LocalFree(nc_error);
        closesocket(sockfd);

        return 4;
    }

    printf("Host: %s\n", ac_host);
    printf("Serv: %s\n", ac_serv);

    closesocket(sockfd);

    return 0;
}
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Get sockaddr, IPv4 or IPv6:                                                */
/*                                                                            */
void* get_in_addr(struct sockaddr* sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
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
