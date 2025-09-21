/******************************************************************************/
/*                                                                            */
/* File:    gethostname.c                                                     */
/*                                                                            */
/* Purpose: Use gethostname to obtain the system's host name.                 */
/*                                                                            */
/* Modifications:                                                             */
/*                                                                            */
/*           Name           Date                     Reason                   */
/*    ------------------ ---------- ----------------------------------------- */
/*    Steven C. Mitchell 2022-11-14 Orignal creation                          */
/*                                                                            */
/******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
/*                                                                            */
/******************************************************************************/
/*                                                                            */
int main(int argc, char *argv[])
{
   int   i_errno;
   char ac_hostname[256];
   int   i_status;
/*                                                                            */
/* Program expects an empty command line:                                     */
/*                                                                            */
   if (argc != 1)
   {
      fprintf(stderr,"usage: gethostname\n");

      return 1;
   }
/*                                                                            */
/* Call gethostname:                                                          */
/*                                                                            */
   errno = 0;
   i_status = gethostname(ac_hostname,sizeof(ac_hostname));
   i_errno = errno;
/*                                                                            */
/* No error so output the name of the host:                                   */
/*                                                                            */
   if (i_status == 0)
   {
      printf("Host name is %s\n",ac_hostname);

      return 0;
   }
/*                                                                            */
/* Function failed to output an error message:                                */
/*                                                                            */
   else
   {
      fprintf(stderr,"gethostname failed with code %d.\n",i_errno);
      fprintf(stderr,"%s\n",strerror(i_errno));

      return 2;
   }
}
