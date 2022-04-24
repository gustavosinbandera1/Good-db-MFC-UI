// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
#include <stdio.h>
#include "goods.h"
#include "sockio.h"

static const char *kUsage =
 "Usage: puturl server url file\n\n"
 "  This program delivers the specified file to the specified URL on the\n"
 "  specified web server.  The server's response is printed to standard\n"
 "  output.\n"
 "\n"
 "  server\n"
 "    The `server' parameter specifies which server and server port to\n"
 "    which HTTP content is to be put.  For example, `www.ike.com:80'.\n"
 "\n"
 "  url\n"
 "    The `url' parameter specifies the relative path on the server of the\n"
 "    HTTP content to put.  For example, `/index.html'.\n"
 "\n"
 "  file\n"
 "    The `file' parameter specifies the file whose content is to be put to\n"
 "    the server.\n"
 "\n";

USE_GOODS_NAMESPACE

int main(int argc, char **argv)
{
     int       arg          = 1;
     char     *data         = NULL;
     char     *file         = NULL;
     FILE     *fileFP       = NULL;
     nat4      fileLen      = 0;
     char     *fileName     = NULL;
     int       length;
     char     *server       = NULL;
     socket_t *sock         = NULL;
     char     *url          = NULL;

 task::initialize(task::huge_stack);
 if(argc < 4)
   {
   printf("%s", kUsage);
   return 0;
   }
 while(arg < argc)
   {
   if(!server)
     server = argv[arg];
   else if(!url)
     url = argv[arg];
   else if(!fileName)
     fileName = argv[arg];
   arg++;
   }

 if(!server)
   {
   fprintf(stderr, "Server not specified.\n\n");
   return 0;
   }
 if(!url)
   {
   fprintf(stderr, "URL not specified.\n\n");
   return 0;
   }

 if(!fileName)
   {
   fprintf(stderr, "File not specified.\n\n");
   return 0;
   }

 if(!(fileFP = fopen(fileName, "rb")))
   {
   fprintf(stderr, "Failed to open file \"%s\".\n\n", fileName);
   return 0;
   }
 fseek(fileFP, 0, SEEK_END);
 fileLen = ftell(fileFP);
 rewind(fileFP);
 file = new char[fileLen];
 if(fread(file, sizeof(char), fileLen, fileFP) != fileLen)
   {
   fprintf(stderr, "Failure while reading file \"%s\".\n\n", fileName);
   fclose(fileFP);
   return 0;
   }
 fclose(fileFP);

 if(!(sock = socket_t::connect(server)))
   {
   fprintf(stderr, "Failed to connect to the server \"%s\".\n\n", server);
   return 0;
   }

 if(!(data = sock->putToURL(server, url, file, fileLen, length)))
   {
   fprintf(stderr, "Failed to put to the URL \"%s\"\n "
           "from the server \"%s\".\n"
           "Did you include the port number with the server name?\n\n",
           url, server);
   sock->close();
   delete sock;
   delete file;
   return 0;
   }
 sock->close();
 delete sock;
 delete file;
 printf("%s", data);
 delete data;

 return 0;
}
