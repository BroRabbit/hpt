#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if !defined(MSDOS) || defined(__DJGPP__)
#include <fidoconf/fidoconf.h>
#include <fidoconf/common.h>
#else
#include <fidoconf/fidoconf.h>
#include <fidoconf/common.h>
#endif

#include <global.h>
#include <version.h>
#include <pkt.h>
#include <xstr.h>
#include <recode.h>

int main(int argc, char *argv[])
{
    s_pktHeader  header;
    s_message    msg;
    FILE         *pkt;
    time_t       t;
    struct tm    *tm;
    char *area = NULL, *passwd = NULL, *tearl = NULL, *orig = NULL, *dir = NULL;
    FILE *text = NULL;
    int quit, n = 1;
    CHAR *textBuffer = NULL;
    char tmp[512];

    memset (&header,0,sizeof(s_pktHeader));
    memset (&msg,0,sizeof(s_message));

   if (argc == 1) {
      printf("\nUsage:\n");
      printf("txt2pkt -xf \"<pkt from address>\" -xt \"<pkt to address>\" -af \"<from address>\" -at \"<to address>\" -nf \"<from name>\" -nt \"<to name>\" -e \"echo name\" -p \"password\" -t \"tearline\" -o \"origin\" -s \"subject\" -d \"<directory>\" <text file>\n");
      exit(1);
   }

   config = readConfig();
   if (NULL == config) {
      printf("Config not found\n");
      exit(1);
   }

   for (quit = 0;n < argc && !quit; n++) {
      if (*argv[n] == '-') {
         switch(argv[n][1]) {
            case 'a':    // address
               switch(argv[n][2]) {
                  case 'f':
                     string2addr(argv[++n], &(msg.origAddr));
                     break;
                  case 't':
                     string2addr(argv[++n], &(msg.destAddr));
                     break;
                  default:
                     quit = 1;
                     break;
               }; break;
            case 'x':    // address
               switch(argv[n][2]) {
                  case 'f':
                     string2addr(argv[++n], &(header.origAddr));
                     break;
                  case 't':
                     string2addr(argv[++n], &(header.destAddr));
                     break;
                  default:
                     quit = 1;
                     break;
               }; break;
            case 'n':    // name
               switch(argv[n][2]) {
                  case 't':
                     msg.toUserName = (char *) malloc(strlen(argv[++n]) + 1);
                     strcpy(msg.toUserName, argv[n]);
#ifdef __NT__
                     CharToOem(msg.toUserName, msg.toUserName);
#endif
                     break;
                  case 'f':
                     msg.fromUserName = (char *) malloc(strlen(argv[++n]) + 1);
                     strcpy(msg.fromUserName, argv[n]);
#ifdef __NT__
                     CharToOem(msg.fromUserName, msg.fromUserName);
#endif
                     break;
                  default:
                     quit = 1;
                     break;
               }; break;
            case 'e':    // echo name
               area = argv[++n];
               break;
            case 'p':    // password
               passwd = argv[++n];
               break;
            case 't':    // tearline
               tearl = argv[++n];
#ifdef __NT__
               CharToOem(tearl, tearl);
#endif
               break;
            case 'o':    // origin
               orig = argv[++n];
#ifdef __NT__
               CharToOem(orig, orig);
#endif
               break;
            case 'd':    // directory
               dir = argv[++n];
               break;
            case 's':    // subject
               msg.subjectLine = (char *) malloc(strlen(argv[++n]) + 1);
               strcpy(msg.subjectLine, argv[n]);
#ifdef __NT__
               CharToOem(msg.subjectLine, msg.subjectLine);
#endif
               break;
	    default:
               quit = 1;
               break;
         };
      } else {
         if ((text = fopen(argv[n], "rt")) != NULL) {
            /* reserve 512kb + 1 (or 32kb+1) text Buffer */
            textBuffer = (CHAR *) malloc(TEXTBUFFERSIZE+1);
            for (msg.textLength = 0; msg.textLength < (long) TEXTBUFFERSIZE; msg.textLength++) {
               if ((textBuffer[msg.textLength] = getc(text)) == 0)
                  break;
               if (feof(text)) {
                  textBuffer[++msg.textLength] = 0;
                  break;
               }; /* endif */
               if ('\r' == textBuffer[msg.textLength])
                  msg.textLength--;
               if ('\n' == textBuffer[msg.textLength])
                  textBuffer[msg.textLength] = '\r';
            }; /* endfor */
            textBuffer[msg.textLength-1] = 0;
            fclose(text);
         } else {
	    printf("Text file not found\n");
	    exit(1);
	 };
      };  
   };

   header.hiProductCode  = 0;
   header.loProductCode  = 0xfe;
   header.majorProductRev = 0;
   header.minorProductRev = 26;
   if (passwd!=NULL) strcpy(header.pktPassword, passwd);
   header.pktCreated = time(NULL);

   header.capabilityWord = 1;
   header.prodData = 0;

   strcpy(tmp,dir);
#ifdef UNIX
   if (tmp[strlen(tmp)-1] != '/') strcat(tmp,"/");
#else
   if (tmp[strlen(tmp)-1] != '\\')  strcat(tmp,"\\");
#endif
   sprintf(tmp + strlen(tmp),"%08lx.pkt",(long)time(NULL));

   if (header.origAddr.zone==0) header.origAddr = msg.origAddr;
   if (header.destAddr.zone==0) header.destAddr = msg.destAddr;

   pkt = createPkt(tmp, &header);

   if (pkt != NULL) {

      msg.attributes = 1;

      t = time (NULL);
      tm = localtime(&t);
      strftime(msg.datetime, 21, "%d %b %y  %T", tm);

      msg.netMail = 1;

      sprintf(tmp, "--- %s\r", tearl);
      strcat(textBuffer, tmp);
      if (area != NULL) {
         sprintf(tmp, " * Origin: %s (%d:%d/%d.%d)\r",
                 orig, msg.origAddr.zone, msg.origAddr.net,
                 msg.origAddr.node, msg.origAddr.point);
         strcat(textBuffer, tmp);
         sprintf(tmp,"SEEN-BY: %d/%d\r\1PATH: %d/%d\r",
	         header.origAddr.net,header.origAddr.node,
		 header.origAddr.net,header.origAddr.node);
         strcat(textBuffer, tmp);
      }
      xstrcat(&versionStr,"txt2pkt");
      msg.text = createKludges(area, &msg.origAddr, &msg.destAddr);
      xstrcat(&(msg.text), textBuffer);
      if (area == NULL) {
         time(&t);
         tm = gmtime(&t);
         xscatprintf(&(msg.text), "\001Via %u:%u/%u.%u @%04u%02u%02u.%02u%02u%02u.UTC %s\r",
                 header.origAddr.zone, header.origAddr.net, header.origAddr.node, header.origAddr.point,
                 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, versionStr);
      }
      
      free(textBuffer);
      free(versionStr);
      msg.textLength=strlen(textBuffer);

      if (config->outtab != NULL) {
         // load recoding tables
         getctab(outtab, config->outtab);
         // recoding text to TransportCharSet
         recodeToTransportCharset(msg.text);
         recodeToTransportCharset(msg.subjectLine);
         recodeToTransportCharset(msg.fromUserName);
         recodeToTransportCharset(msg.toUserName);
      }

      writeMsgToPkt(pkt, msg);

      closeCreatedPkt(pkt);
      sleep(1);
   } else {
      printf("Could not create pkt");
   } /* endif */

   return 0;
}
