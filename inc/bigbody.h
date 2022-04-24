// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< BIGBODY.H >---------------------------------------------------+-----------
// GOODS                     Version 1.0         (c) 2006  SETER   |   *
// (Generic Object Oriented Database System)                       |      /\ v
//                                                                 |_/\__/  \__
//                        Created:     22-Feb-06    Marc Seter     |/  \/    \/ //                        Last update: 22-Feb-06    Marc Seter     |Marc Seter
//-----------------------------------------------------------------+-----------
// Provide an interface to & manage a temporary file containing the data from
// large HTTP data whose size exceeds the amount we want to handle in memory.
// Using this interface to handle HTTP data enables a server to handle huge
// uploads (like a 10GB upload!), but slows down the server because it uses a
// temporary file on disk instead of RAM.
//-----------------------------------------------------------------------------

#ifndef __BIG_POST_H__
#define __BIG_POST_H__

#include "file.h"
#include "goodsdlx.h"
#include "osfile.h"
#include "support.h"

BEGIN_GOODS_NAMESPACE

class os_file;
class WWWconnection;

class GOODS_DLL_EXPORT big_body_file : public os_file
{
  protected :
    dnm_buffer mPage;
    fposi_t    mPageOffset;
    nat4       mPagePos;
    nat4       mPageSize;
    fsize_t    mSize;

  public :
    big_body_file(char *iFile);

    iop_status get_position(fposi_t &oPos);
    iop_status readChar(char *oChar);
};

class GOODS_DLL_EXPORT big_body_part : public l2elem
{
  public :
    big_body_part(void);
    virtual ~big_body_part(void);

#if 0 // server doesn't include wwwapi ...
    void    addNameValuePair(WWWconnection &ioCnxn);
#endif
    big_body_part *getNext(void) const;
    fposi_t        getFileOffset(void) const;
    fsize_t        getFileSize(void) const;
    boolean        hasFilename(void) const;
    boolean        loadData(os_file &ioFile);
    boolean        loadHeader(os_file &ioFile);
    const char    *name(void) const;
    void           setOffsets(fposi_t iOffset, fposi_t iDataOffset);
    const char    *value(void) const;
#if 0 // Nice idea, but 2 passes seems like the way to go ...
    boolean readNext(os_file &ioFile, char *iBoundary, char *iBoundEnd);
#endif

  protected :
    char    *mData;
    fposi_t  mDataOffset;
    char    *mFilename;
    char    *mHeader;
    char    *mName;
    fposi_t  mOffset;

    boolean findToken(char **oPtr, const char *iTokenName);
};

class GOODS_DLL_EXPORT big_body_parts : public big_body_part
{
  public :
    big_body_parts(void);
    virtual ~big_body_parts(void);

    boolean findOffsets(big_body_file &ioFile, char *iBoundary,
              char *iBoundEnd);
    boolean parseParts(os_file &ioFile);

  protected :
    boolean findData(big_body_file &ioFile);
    fposi_t findNextHeader(big_body_file &ioFile, dnm_buffer &iBoundary);
};

class GOODS_DLL_EXPORT big_body
{
  public :
    static nat8 Mthreshold;

    big_body(void);
    virtual ~big_body(void);

    //--- Accessors --------------------------------------------
#if 0 // server doesn't include wwwapi ...
    void           addNameValuePairs(WWWconnection &ioCnxn);
#endif
    big_body_part *end(void);
    big_body_part *getFile(int iIndex = 0);
    const char    *getTmpFilename(void) const;
    big_body_part *first(void);
    boolean        hasData(void) const;
    int            numFiles(void);

    //--- Utilities --------------------------------------------
    boolean take(char *iTmpFile, char *iHTTPheader);

  protected :
    //--- Members ----------------------------------------------
    big_body_parts  mParts;
    char           *mTmpFile;

    void deleteParts(void);
    void deleteTmpFile(void);
};

END_GOODS_NAMESPACE

#endif
