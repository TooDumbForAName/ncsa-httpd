/* Various debugging routines */

#ifdef DEBUG

#include "config.h"
#include "portability.h"

#include "constants.h"
#include "host_config.h"
#include "http_request.h"


#ifdef SOLARIS2

#include <fcntl.h>
#include <sys/procfs.h>
#include <errno.h>

prpsinfo_t p;
long current_process_size() 
{
   int retval;
   char filename[MAX_STRING_LEN];
   int fd;
   int pid;

   pid = getpid();

   fprintf(stderr,"%d\n",pid);

   if (pid < 10) {
     sprintf(filename,"/proc/0000%d",pid);
   } else if (pid < 100) {
     sprintf(filename,"/proc/000%d",pid);
   } else if (pid < 1000) {
     sprintf(filename,"/proc/00%d",pid);
   } else if (pid < 10000) {
     sprintf(filename,"/proc/0%d",pid);
   } else {
     sprintf(filename,"/proc/%d",pid);
   }

     
/*   fprintf(stderr,filename); */
   if (!(fd = open(filename,O_RDONLY))) {
     fprintf(stderr,"Error openning file %s, errno=%d\n",filename,errno);
   }
   retval = ioctl(fd,PIOCPSINFO,&p);
   if (retval == -1) {
     fprintf(stderr,"Error in ioctl, errno = %d\n", errno);
   }
   close(fd);

   fprintf(stderr,"%X  %X\n",&p,filename);
   sprintf(filename,"%d bytes memory", p.pr_bysize);
   log_error(filename,gConfiguration->error_log);

}

#endif /* SOLARIS2 */

#ifdef AIX3

#include <malloc.h>

long current_process_size(char *msg) {
   char S[MAX_STRING_LEN];
   struct mallinfo mi;
   int memory;

   mi = mallinfo();


   memory = mi.usmblks+mi.uordblks; 
   sprintf(S,"%25s: Space in Use: %d", msg,memory);
   log_error(S,gConfiguration->error_log);

   return memory;
}

#endif /* AIX3 */

#endif /* DEBUG */
