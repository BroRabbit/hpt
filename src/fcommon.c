#include <time.h>
#include <fcommon.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>

#include <global.h>

#include <typedefs.h>
#include <compiler.h>
#include <stamp.h>
#include <progprot.h>

char *createTempPktFileName()
{
   char   *fileName = (char *) malloc(strlen(config->outbound)+1+12);
   time_t aTime = time(NULL);  // get actual time
   int counter = 0;

   aTime %= 0xffffff;   // only last 24 bit count

   do {
      sprintf(fileName, "%s%06lx%02x.pkt", config->outbound, aTime, counter);
      counter++;
   } while (fexist(fileName) && (counter<=256));

   if (!fexist(fileName)) return fileName;
   else {
      free(fileName);
      return NULL;
   }
}

int createDirectoryTree(const char *pathName) {

   struct stat buf;
   char *start, *slash;
   char *buff;
   
#ifdef UNIX
   char limiter='/';
#else
   char limiter='\\';
#endif

   int i;

   start = (char *) malloc(strlen(pathName)+2);
   strcpy(start, pathName);
   i = strlen(start)-1;
   if (start[i] != limiter) {
      start[i+1] = limiter;
      start[i+2] = '\0';
   }
   slash = start;

#ifndef UNIX
   // if there is a drivename, jump over it
   if (slash[1] == ':') slash += 2;
#endif

   // jump over first limiter
   slash++;
   
   while ((slash = strchr(slash, limiter)) != NULL) {
      *slash = '\0';

      if (stat(start, &buf) != 0) {
         // this part of the path does not exist, create it
#ifdef UNIX
         if (mkdir(start, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0) {
#else
         if (mkdir(start) != 0) {
#endif
            buff = (char *) malloc(strlen(start)+30);
            sprintf(buff, "Could not create directory %s", start);
            writeLogEntry(log, '5', buff);
            free(buff);
            free(start);
            return 1;
         }
      } else if(!S_ISDIR(buf.st_mode)) {
         buff = (char *) malloc(strlen(start)+30);
         sprintf(buff, "%s is a file not a directory", start);
         writeLogEntry(log, '5', buff);
         free(buff);
         free(start);
         return 1;
      }

      *slash++ = limiter;
   }

   free(start);

   return 0;
}

char *createOutboundFileName(s_addr aka, e_prio prio, e_type typ)
{
   char name[13], zoneSuffix[5], pntDir[14];
   char *fileName;

   if (aka.point != 0) {
      sprintf(pntDir, "%04x%04x.pnt\\", aka.net, aka.node);
#ifdef UNIX
      pntDir[12] = '/';
#endif
      sprintf(name, "%08x.flo", aka.point);
   } else {
      pntDir[0] = 0;
      sprintf(name, "%04x%04x.flo", aka.net, aka.node);
   }

   if (aka.zone != config->addr[0].zone) {
      // add suffix for other zones
      sprintf(zoneSuffix, ".%03x\\", aka.zone);
#ifdef UNIX
      zoneSuffix[4] = '/';
#endif
   } else {
      zoneSuffix[0] = 0;
   }

   switch (typ) {
      case PKT:
         name[9] = 'o'; name[10] = 'u'; name[11] = 't';
         break;
      case REQUEST:
         name[9] = 'r'; name[10] = 'e'; name[11] = 'q';
         break;
      case FLOFILE: break;
   } /* endswitch */

   if (typ != REQUEST) {
      switch (prio) {
         case CRASH : name[9] = 'c';
                      break;
         case HOLD  : name[9] = 'h';
                      break;
         case NORMAL: break;
      } /* endswitch */
   } /* endif */

   fileName = (char *) malloc(strlen(config->outbound)+strlen(pntDir)+strlen(zoneSuffix)+strlen(name)+1);
   strcpy(fileName, config->outbound);
   if (zoneSuffix[0] != 0) strcpy(fileName+strlen(fileName)-1, zoneSuffix);
   strcat(fileName, pntDir);
   createDirectoryTree(fileName); // create directoryTree if necessary
   strcat(fileName, name);
   
   return fileName;
}
