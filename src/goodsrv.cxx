// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< GOODSRV.CXX >---------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      7-Jun-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 17-Oct-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Example of server program
//-------------------------------------------------------------------*--------*

#include "confgrtr.h"
#include "server.h"

#pragma comment(lib, "wsock32.lib")

USE_GOODS_NAMESPACE

int main(int argc, char* argv[]) 
{
    int retval;

    task::initialize(task::huge_stack);
    retval = goodsrv(argc, argv);
    if (retval != EXIT_SUCCESS)
        return retval;
 
    return EXIT_SUCCESS;
}

