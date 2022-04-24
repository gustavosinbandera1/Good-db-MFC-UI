// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
#include "goods.h"
#include "sockio.h"

USE_GOODS_NAMESPACE

static const char *kUsage =
 "Usage: geturl server url [-r redirects] [-v]\n\n"
 "  This program retrieves the specified URL from the specified web server.\n"
 "  The retrieved content is printed to standard output.\n"
 "\n"
 "  server\n"
 "    The `server' parameter specifies which server and server port from\n"
 "    which HTTP content is to be retrieved.  For example,\n"
 "    `www.ike.com:80'.\n"
 "\n"
 "  url\n"
 "    The `url' parameter specifies the relative path on the server of the\n"
 "    HTTP content to retrieve.  For example, `/index.html'.\n"
 "\n"
 "  -r redirects\n"
 "    The optional parameter `redirects' specifies the maximum number of\n"
 "    redirections this program will follow to retrieve content before\n"
 "    failing.\n"
 "\n"
 "  -v\n"
 "    The optional parameter `-v' specifies that the program should indicate\n"
 "    download progress as it is made.  Progress is displayed as a ratio\n"
 "    that indicates the number of bytes transferred versus the total number\n"
 "    of bytes to be transferred.\n"
 "\n";

boolean displayProgress(nat4 iSoFar, nat4 iTotal)
{
 static boolean first = True;

 if(!first)
   {
   for(int i = 0; i < 19; i++)
     printf("%c", 8);
   }
 printf("%8u / %8u", iSoFar, iTotal);
 first = False;
 return True;
}


int main(int argc, char **argv)
{
     int       arg          = 1;
     char     *data;
     int       length;
     int       numRedirects = 0;
     char     *server       = NULL;
     socket_t *sock;
     char     *url          = NULL;
     boolean   verbose      = False;

 task::initialize(task::huge_stack);
 if(argc < 3)
   {
   printf("%s", kUsage);
   return 0;
   }
 while(arg < argc)
   {
   if((strcmp(argv[arg], "-r") == 0) && (arg + 1 < argc))
     numRedirects = atol(argv[++arg]);
   else if(strcmp(argv[arg], "-v") == 0)
     verbose = True;
   else if(!server)
     server = argv[arg];
   else if(!url)
     url = argv[arg];
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

 if(!(sock = socket_t::connect(server)))
   {
   fprintf(stderr, "Failed to connect to the server \"%s\".\n\n", server);
   return 0;
   }

 if(verbose)
   sock->setProgressFn(displayProgress);

 if(!(data = sock->getURL(server, url, length, numRedirects)))
   {
   fprintf(stderr, "Failed to retrieve the URL \"%s\"\n "
           "from the server \"%s\".\n"
           "Did you include the port number with the server name?\n\n",
           url, server);
   sock->close();
   delete sock;
   return 0;
   }
 sock->close();
 delete sock;
 if(verbose)
   printf("\n");
 printf("%s", data);
 delete data;

 return 0;
}

