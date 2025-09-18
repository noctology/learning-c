/******************************************************************************/
/*                                                                            */
/* File:    poll.c                                                            */
/*                                                                            */
/* Purpose: Poll a socket.  The original poll.c polls stdin which is a file   */
/*          descriptor.  This works in Unix/Linux because file descriptors    */
/*          and socket descriptors are essentially the same and even share    */
/*          some functions (e.g., close).  This is not true in Windows.       */
/*          This program is a Unix/Linux program that polls a socket.         */
/*                                                                            */
/* Modifications:                                                             */
/*                                                                            */
/*           Name           Date                     Reason                   */
/*    ------------------ ---------- ----------------------------------------- */
/*    Steven C. Mitchell 2022-11-17 Orignal creation                          */
/*                                                                            */
/******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <poll.h>

#define PORT "3490" // the port users will be connecting to
#define BACKLOG 10  // how many pending connections queue will hold

void *get_in_addr(struct sockaddr *);
void sigchld_handler(int);
/*                                                                            */
/******************************************************************************/
/*                                                                            */
int main(void)
{
   struct addrinfo         *ps_address;
   struct addrinfo           s_hints;
   int                       i_errno;
   int                       i_new_fd;  // new connection on new_fd
   int                       i_num_events;
   struct pollfd            as_pfds[1]; // More if you want to monitor more
   int                       i_pollin_happened;
   int                       i_rv;
   struct sigaction          s_sa;
   char                     ac_server[INET6_ADDRSTRLEN];
   struct addrinfo         *ps_servinfo;
   socklen_t                   sin_size;
   int                       i_sockfd;  // listen on sock_fd
   struct sockaddr_storage   s_their_addr; // connector's address information
   int                       i_yes;
/*                                                                            */
/* Get a list of possible connections:                                        */
/*                                                                            */
   memset(&s_hints,0,sizeof(s_hints));
   s_hints.ai_family = AF_UNSPEC;
   s_hints.ai_socktype = SOCK_STREAM;
   s_hints.ai_flags = AI_PASSIVE; // use my IP

   i_rv = getaddrinfo(NULL,PORT,&s_hints,&ps_servinfo);
   if (i_rv != 0)
   {
      fprintf(stderr,"getaddrinfo failed with code %d.\n",i_rv);
      fprintf(stderr,"%s\n",gai_strerror(i_rv));

      return 1;
   }
/*                                                                            */
/* Loop through all the results and bind to the first we can:                 */
/*                                                                            */
   for (ps_address = ps_servinfo ;
        ps_address != NULL ;
        ps_address = ps_address->ai_next)
   {
      errno = 0;
      i_sockfd = socket(ps_address->ai_family,ps_address->ai_socktype,
                        ps_address->ai_protocol);
      i_errno = errno;

      if (i_sockfd == -1)
      {
         fprintf(stderr,"socket returned code %d.\n",i_errno);
         fprintf(stderr,"%s\n",strerror(i_errno));

         continue;
      }

      i_yes = 1;
      errno = 0;
      i_rv = setsockopt(i_sockfd,SOL_SOCKET,SO_REUSEADDR,&i_yes,sizeof(int));
      i_errno = errno;

      if (i_rv == -1)
      {
         fprintf(stderr,"setsockopt failed with code %d.\n",i_errno);
         fprintf(stderr,"%s\n",strerror(i_errno));
         close(i_sockfd);
         freeaddrinfo(ps_servinfo);

         return 2;
      }

      errno = 0;
      i_rv = bind(i_sockfd,ps_address->ai_addr,ps_address->ai_addrlen);
      i_errno = errno;

      if (i_rv == -1)
      {
         fprintf(stderr,"bind returned code %d.\n",i_errno);
         fprintf(stderr,"%s\n",strerror(i_errno));
         close(i_sockfd);

         continue;
      }

      break;
   }
/*                                                                            */
/* All done with this structure:                                              */
/*                                                                            */
   freeaddrinfo(ps_servinfo);
/*                                                                            */
/* Check for a connection:                                                    */
/*                                                                            */
   if (ps_address == NULL)
   {
      fprintf(stderr,"poll failed to bind to a socket.\n");
      close(i_sockfd);

      return 3;
   }
/*                                                                            */
/* Listen on this socket:                                                     */
/*                                                                            */
   errno = 0;
   i_rv = listen(i_sockfd,BACKLOG);
   i_errno = errno;

   if (i_rv == -1)
   {
      fprintf(stderr,"listen failed with code %d.\n",i_errno);
      fprintf(stderr,"%s\n",strerror(i_errno));
      close(i_sockfd);

      return 4;
   }
/*                                                                            */
/* Reap all dead processes:                                                   */
/*                                                                            */
   s_sa.sa_handler = sigchld_handler; 
   sigemptyset(&s_sa.sa_mask);
   s_sa.sa_flags = SA_RESTART;

   if (sigaction(SIGCHLD,&s_sa,NULL) == -1)
   {
      perror("sigaction");
      close(i_sockfd);

      return 5;
   }
/*                                                                            */
/* Poll bound socket:                                                         */
/*                                                                            */
   as_pfds[0].fd = i_sockfd;   // Connected socket
   as_pfds[0].events = POLLIN; // Tell me when ready to read

// If you needed to monitor other things, as well:
//    pfds[1].fd = some_socket; // Some socket descriptor
//    pfds[1].events = POLLIN;  // Tell me when ready to read

   printf("Start client or wait 5 seconds for timeout...\n");

   i_num_events = poll(as_pfds,1,5000); // 5 second timeout

   if (i_num_events == 0)
   {
      printf("Poll timed out!\n");
      close(i_sockfd);

      return 0;
   }
   else
   {
      i_pollin_happened = as_pfds[0].revents & POLLIN;

      if (i_pollin_happened)
      {
         printf("File descriptor %d is ready to read\n",as_pfds[0].fd);

         sin_size = sizeof(s_their_addr);
         errno = 0;
         i_new_fd = accept(i_sockfd,(struct sockaddr *)&s_their_addr,&sin_size);
         i_errno = errno;

         if (i_new_fd == -1)
         {
            fprintf(stderr,"accept failed with code %d.\n",i_errno);
            fprintf(stderr,"%s\n",strerror(i_errno));
            close(i_sockfd);

            return 6;
         }
         else
         {
            send(i_new_fd,"Poll successful.",16,0);
            close(i_new_fd);
            close(i_sockfd);

            return 0;
         }
      }
      else
      {
         printf("Unexpected event occurred: %d\n",as_pfds[0].revents);
         close(i_sockfd);

         return 7;
      }
   }
}
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Get sockaddr, IPv4 or IPv6:                                                */
/*                                                                            */
void *get_in_addr(struct sockaddr *sa)
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
/* Reap dead process:                                                         */
/*                                                                            */
void sigchld_handler(int s)
{
   (void)s; // quiet unused variable warning

// waitpid() might overwrite errno, so we save and restore it:
   int i_saved_errno = errno;

   while (waitpid(-1,NULL,WNOHANG) > 0);

   errno = i_saved_errno;
}
