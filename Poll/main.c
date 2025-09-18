/******************************************************************************/
/*                                                                            */
/* Application: WSpoll                                                        */
/*                                                                            */
/* File:        WSpoll.c                                                      */
/*                                                                            */
/* Purpose:     Poll a socket.  The original poll.c polls stdin which is a    */
/*              file descriptor.  This works in Unix/Linux because file       */
/*              descriptors and socket descriptors are essentially the same   */
/*              and even share some functions (e.g., close).  This is not     */
/*              true in Windows.  This program is a Windows program that      */
/*              polls a socket.                                               */
/*                                                                            */
/* Modifications:                                                             */
/*                                                                            */
/*           Name           Date                     Reason                   */
/*    ------------------ ---------- ----------------------------------------- */
/*    Steven C. Mitchell 2022-11-17 Orignal creation                          */
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

#define PORT "3490" // the port users will be connecting to
#define BACKLOG 10  // how many pending connections queue will hold

void get_msg_text(DWORD,char **);
/*                                                                            */
/******************************************************************************/
/*                                                                            */
int main(void)
{
   struct addrinfo         *ps_address;
   int                       i_addrlen;
   char                    *nc_error;
   DWORD                    dw_error;
   struct addrinfo           s_hints;
   SOCKET                      new_fd;  // new connection on new_fd
   int                       i_num_events;
   WSAPOLLFD                as_pfds[1]; // More if you want to monitor more
   int                       i_pollin_happened;
   struct addrinfo         *ps_servinfo;
   socklen_t                   sin_size;
   SOCKET                      sockfd;  // listen on sock_fd
   int                       i_status;
   struct sockaddr_storage   s_their_addr; // connector's address information
   WSADATA                   s_wsaData;
   BOOL                      B_yes;
/*                                                                            */
/* Initialize Winsock and request version 2.2:                                */
/*                                                                            */
   i_status = WSAStartup(MAKEWORD(2,2),&s_wsaData);

   if (i_status != 0)
   {
      dw_error = (DWORD)i_status;
      get_msg_text(dw_error,
                   &nc_error);
      fprintf(stderr,"WSAStartup failed with code %d.\n",i_status);
      fprintf(stderr,"%s\n",nc_error);
      LocalFree(nc_error);

      return 1;
   }
/*                                                                            */
/* Verify that version 2.2 is available:                                      */
/*                                                                            */
   if (LOBYTE(s_wsaData.wVersion) < 2 ||
       HIBYTE(s_wsaData.wVersion) < 2)
   {
      fprintf(stderr,"Version 2.2 of Winsock is not available.\n");
      WSACleanup();

      return 2;
   }
/*                                                                            */
/* Set the desired IP address characteristics:                                */
/*                                                                            */
   memset(&s_hints,0,sizeof(s_hints));

   s_hints.ai_family   = AF_UNSPEC;
   s_hints.ai_socktype = SOCK_STREAM;
   s_hints.ai_flags    = AI_PASSIVE; // use my IP
/*                                                                            */
/* Request a list of IP addresses:                                            */
/*                                                                            */
   i_status = getaddrinfo(NULL,PORT,&s_hints,&ps_servinfo);

   if (i_status != 0)
   {
      dw_error = (DWORD)WSAGetLastError();
      get_msg_text(dw_error,
                   &nc_error);
      fprintf(stderr,"getaddrinfo failed with code %ld.\n",dw_error);
      fprintf(stderr,"%s\n",nc_error);
      LocalFree(nc_error);
      WSACleanup();

      return 3;
   }
/*                                                                            */
/* Loop through all the results and bind to the first we can:                 */
/*                                                                            */
   for (ps_address = ps_servinfo ;
        ps_address != NULL ;
        ps_address = ps_address->ai_next)
   {
      sockfd = socket(ps_address->ai_family,ps_address->ai_socktype,
                      ps_address->ai_protocol);
      if (sockfd == INVALID_SOCKET)
      {
         dw_error = (DWORD)WSAGetLastError();
         get_msg_text(dw_error,
                      &nc_error);
         fprintf(stderr,"socket failed with code %ld.\n",dw_error);
         fprintf(stderr,"%s\n",nc_error);
         LocalFree(nc_error);

         continue;
      }
/* Allow reuse of the socket:                                                 */
      B_yes = TRUE;
      i_status = setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,(char *)&B_yes,
                            sizeof(B_yes));
      if (i_status == SOCKET_ERROR)
      {
         dw_error = (DWORD)WSAGetLastError();
         get_msg_text(dw_error,
                       &nc_error);
         fprintf(stderr,"setsockopt failed with code %ld.\n",dw_error);
         fprintf(stderr,"%s\n",nc_error);
         LocalFree(nc_error);
         closesocket(sockfd);
         freeaddrinfo(ps_servinfo);
         WSACleanup();

         return 4;
      }
/* Bind to the socket:                                                        */
      i_addrlen = (int)ps_address->ai_addrlen;
      i_status = bind(sockfd,ps_address->ai_addr,i_addrlen);

      if (i_status == SOCKET_ERROR)
      {
         dw_error = (DWORD)WSAGetLastError();
         get_msg_text(dw_error,
                      &nc_error);
         fprintf(stderr,"bind failed with code %ld.\n",dw_error);
         fprintf(stderr,"%s\n",nc_error);
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
      fprintf(stderr,"Failed to bind to a socket.\n");
      closesocket(sockfd);
      freeaddrinfo(ps_servinfo);
      WSACleanup();

      return 5;
   }
/*                                                                            */
/* Listen for a connection:                                                   */
/*                                                                            */
   i_status = listen(sockfd,BACKLOG);

   if (i_status == SOCKET_ERROR)
   {
      dw_error = (DWORD)WSAGetLastError();
      get_msg_text(dw_error,
                   &nc_error);
      fprintf(stderr,"listen failed with code %ld.\n",dw_error);
      fprintf(stderr,"%s\n",nc_error);
      LocalFree(nc_error);
      closesocket(sockfd);
      freeaddrinfo(ps_servinfo);
      WSACleanup();

      return 6;
   }
/*                                                                            */
/* Free the list of addresses:                                                */
/*                                                                            */
   freeaddrinfo(ps_servinfo);
/*                                                                            */
/* Poll bound socket:                                                         */
/*                                                                            */
   as_pfds[0].fd = sockfd;     // Connected socket
   as_pfds[0].events = POLLIN; // Tell me when ready to read
   as_pfds[0].revents = 0;

// If you needed to monitor other things, as well:
//    pfds[1].fd = some_socket; // Some socket descriptor
//    pfds[1].events = POLLIN;  // Tell me when ready to read
//    pfds[1].revents = 0;

   printf("Start client or wait 5 seconds for timeout...\n");

   i_num_events = WSAPoll(as_pfds,1,5000); // 5 second timeout

   if (i_num_events == 0)
   {
      printf("Poll timed out!\n");
      closesocket(sockfd);
      WSACleanup();

      return 0;
   }
   else if (i_num_events == SOCKET_ERROR)
   {
      printf("Unexpected event occurred: %d\n",as_pfds[0].revents);
      closesocket(sockfd);
      WSACleanup();

      return 8;
   }
   else
   {
      i_pollin_happened = as_pfds[0].revents & POLLIN;

      if (i_pollin_happened)
      {
         printf("File descriptor %lld is ready to read\n",as_pfds[0].fd);

         sin_size = sizeof(s_their_addr);
         new_fd = accept(sockfd,(struct sockaddr *)&s_their_addr,&sin_size);

         if (new_fd == INVALID_SOCKET)
         {
            dw_error = (DWORD)WSAGetLastError();
            get_msg_text(dw_error,
                         &nc_error);
            fprintf(stderr,"accept failed with code %ld.\n",dw_error);
            fprintf(stderr,"%s\n",nc_error);
            LocalFree(nc_error);
            closesocket(sockfd);
            WSACleanup();

            return 7;
         }
         else
         {
            send(new_fd,"Poll successful.",16,0);
            closesocket(new_fd);
            closesocket(sockfd);
            WSACleanup();

            return 0;
         }
      }
   }
}
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Get Error Message Text:                                                    */
/*                                                                            */
void get_msg_text
(
   DWORD    dw_error, /* in   - Error code                                    */
   char  **pnc_msg    /* out  - Error message                                 */
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
   FormatMessage(dw_flags,NULL,dw_error,LANG_SYSTEM_DEFAULT,(LPTSTR)pnc_msg,0,
                 NULL);
}
