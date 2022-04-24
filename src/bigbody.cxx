// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< BIGBODY.CXX >-------------------------------------------------+-----------
// GOODS                     Version 1.0         (c) 2006  SETER   |   *
// (Generic Object Oriented Database System)                       |      /\ v
//                                                                 |_/\__/  \__
//                        Created:     22-Feb-06    Marc Seter     |/  \/    \/
//                        Last update: 22-Feb-06    Marc Seter     |Marc Seter
//-----------------------------------------------------------------+-----------
// Provide an interface to & manage a temporary file containing the data from
// large HTTP data whose size exceeds the amount we want to handle in memory.
// Using this interface to handle HTTP data enables a server to handle huge
// uploads (like a 10GB upload!), but slows down the server because it uses a
// temporary file on disk instead of RAM.
//-----------------------------------------------------------------------------

#include "stdinc.h"

#include "bigbody.h"
#include "osfile.h"
#if 0 // server doesn't include wwwapi ...
#include "wwwapi.h"
#endif

BEGIN_GOODS_NAMESPACE

nat8 big_body::Mthreshold = nat8(8) * 1024 * 1024 * 1024;

static const nat4  kPageSize = 64 * 1024;


/*****************************************************************************\
big_body - default constructor
\*****************************************************************************/

big_body::big_body(void) :
  mParts(),
  mTmpFile(NULL)
{
}


/*****************************************************************************\
~big_body - destructor
\*****************************************************************************/

big_body::~big_body(void)
{
  deleteParts();
  deleteTmpFile();
}


#if 0 // server doesn't include wwwapi ...
void big_body::addNameValuePairs(WWWconnection &ioCnxn)
{
  big_body_part *part = (big_body_part *)mParts.next;

  while(part != &mParts) {
    part->addNameValuePair(ioCnxn);
    part = part->getNext();
  }
}
#endif


/*****************************************************************************\
deleteParts - delete this object's linked list of parts
\*****************************************************************************/

void big_body::deleteParts(void)
{
  while(!mParts.empty()) {
    big_body_part *part = mParts.getNext();
    part->unlink();
    delete part;
  }
}


/*****************************************************************************\
deleteTmpFile - delete this object's temporary file & string holding its name
\*****************************************************************************/

void big_body::deleteTmpFile(void)
{
  if(mTmpFile) {
    os_file f(mTmpFile);
    f.remove();
    free(mTmpFile);
  }
  mTmpFile = NULL;
}


big_body_part *big_body::end(void)
{
  return &mParts;
}


big_body_part *big_body::first(void)
{
  return mParts.getNext();
}


/*****************************************************************************\
hasData - determine if this object has data
\*****************************************************************************/

boolean big_body::hasData(void) const
{
  return (mTmpFile != NULL);
}


big_body_part *big_body::getFile(int iIndex)
{
  big_body_part *part  = first();

  while(part != end()) {
    if(part->hasFilename()) {
      if(iIndex == 0) {
        return part;
      } else {
        --iIndex;
      }
    }
    part = part->getNext();
  }

  return NULL;
}


const char *big_body::getTmpFilename(void) const
{
  return mTmpFile;
}


/*****************************************************************************\
numFiles - determine how many files are included in the multi-part data
\*****************************************************************************/

int big_body::numFiles(void)
{
  int            count = 0;
  big_body_part *part  = first();

  while(part != end()) {
    if(part->hasFilename())
      count++;
    part = part->getNext();
  }

  return count;
}


/*****************************************************************************\
take - assume ownership of that temporary file & its string, and parse the file
\*****************************************************************************/

boolean big_body::take(char *iTmpFile, char *iHTTPheader)
{
  if(iTmpFile == NULL)
    return false;

  char          *bend     = NULL;
  char          *boundary = NULL;
  big_body_file  f(iTmpFile);

  mTmpFile = iTmpFile;

  boundary = stristr(iHTTPheader, "boundary");
  if(!boundary)
    return false;
  boundary += 8;
  while(*boundary == ' ') boundary++;
  while(*boundary == '=') boundary++;
  while(*boundary == ' ') boundary++;
  bend = boundary;
  while(*bend != '\n' && *bend != '\r' && *bend != '\0' && *bend != ' ') bend++;

  if(f.open(f.fa_read, f.fo_random | f.fo_largefile) != f.ok)
    return false;

  if(!mParts.findOffsets(f, boundary, bend))
    return false;
  if(!mParts.parseParts(f))
    return false;

  return true;
}


big_body_file::big_body_file(char *iFile) :
  os_file(iFile),
  mPage(),
  mPageOffset(0),
  mPagePos(0),
  mPageSize(0),
  mSize(0)
{
  mPage.put(kPageSize);
}


file::iop_status big_body_file::get_position(fposi_t &oPos)
{
  oPos = mPageOffset + mPagePos;
  return ok;
}

file::iop_status big_body_file::readChar(char *oChar)
{
  iop_status stat;

  if((mSize == 0) && ((stat = get_size(mSize)) != ok)) {
    return stat;
  }

  if(mPagePos >= mPageSize) {
    mPageOffset += mPageSize;
    if(mPageOffset == mSize)
      return end_of_file;

    if(mSize - mPageOffset < kPageSize)
      mPageSize = mSize - mPageOffset;
    else
      mPageSize = kPageSize;
    if((stat = read(mPage.put(mPageSize), mPageSize)) != ok) {
      return stat;
    }

    mPagePos = 0;
  }

  *oChar = mPage[mPagePos++];
  return ok;
}


/*****************************************************************************\
big_body_part - default constructor
\*****************************************************************************/

big_body_part::big_body_part(void) :
  l2elem(),
  mData(NULL),
  mDataOffset(0),
  mFilename(NULL),
  mHeader(NULL),
  mName(NULL),
  mOffset(0)
{
}


/*****************************************************************************\
~big_body_part - destructor
\*****************************************************************************/

big_body_part::~big_body_part(void)
{
  delete[] mHeader;
}


#if 0 // server doesn't include wwwapi ...
void big_body_part::addNameValuePair(WWWconnection &ioCnxn)
{
  if(hasFilename()) {
    ioCnxn.addPair("filename", mFilename);
  } else {
    ioCnxn.addPair(mName, mData);
  }
}
#endif


#if 0 // Nice idea, but 2 passes seems like the way to go ...
boolean big_body_part::findToken(os_file &ioFile, const char *iToken,
  char **oValue)
{
  char        c;
  const char *dToken = iToken;
  int         numCRs = 0;
  int         numLFs = 0;

  while(*dToken != 0) {
    if(ioFile.read(&c, 1) != ioFile.ok)
      return false;

    if((c == '\n') || (c == '\r')) {
      numCRs += (c == '\n');
      numLFs += (c == '\r')
      if((numCRs == 2) && (numLFs == 2))
        return false;
    } else {
      numCRs = numLFs = 0;
      if(c == *dToken)
        dToken++;
      else
        dToken = iToken;
    }
  }
}
#endif
boolean big_body_part::findToken(char **oPtr, const char *iTokenName)
{
  char *end;
  char *start;

  if((start = stristr(mHeader, iTokenName)) == NULL)
    return false;
  start += strlen(iTokenName);
  while(*start != '=')  start++;
  while(*start == '=')  start++;
  while(*start == '\"')  start++;
  end = *oPtr = start;
  while(*end != '\"')  end++;
  *end = 0;

  return true;
}


fposi_t big_body_part::getFileOffset(void) const
{
  return mDataOffset;
}


fsize_t big_body_part::getFileSize(void) const
{
  return getNext()->mOffset - mDataOffset - 2;
}


big_body_part *big_body_part::getNext(void) const
{
  return (big_body_part *)next;
}


boolean big_body_part::hasFilename(void) const
{
  return (mFilename != NULL);
}


boolean big_body_part::loadData(os_file &ioFile)
{
  fposi_t  dataSize = getNext()->mOffset - mDataOffset;
  char    *end;

  delete[] mData;
  mData = new char[(int)dataSize];
  if(ioFile.read(mDataOffset, mData, (int)dataSize) != ioFile.ok)
    return false;
  end = mData;
  while((*end != '\n') && (*end != '\r') && (*end != 0)) end++;
  *end = 0;
  return true;
}


boolean big_body_part::loadHeader(os_file &ioFile)
{
  int size = (int)(mDataOffset - mOffset);

  delete[] mHeader;
  mHeader = new char[size];
  if(ioFile.read(mOffset, mHeader, size) != ioFile.ok)
    return false;
  mHeader[size - 1] = 0;

  findToken(&mFilename, "filename");

  return findToken(&mName, "name");
}


const char *big_body_part::name(void) const
{
  if(hasFilename()) {
    return "filename";
  }
  return mName;
}


#if 0 // Nice idea, but 2 passes seems like the way to go ...
boolean big_body_part::readNext(os_file &ioFile, char *iBoundary,
  char *iBoundEnd)
{
  char *p;

}
#endif


void big_body_part::setOffsets(fposi_t iOffset, fposi_t iDataOffset)
{
  mDataOffset = iDataOffset;
  mOffset     = iOffset;
}


const char *big_body_part::value(void) const
{
  if(hasFilename()) {
    return mFilename;
  }
  return mData;
}


/*****************************************************************************\
big_body_parts - default constructor
\*****************************************************************************/

big_body_parts::big_body_parts(void) :
  big_body_part()
{
}


/*****************************************************************************\
~big_body_parts - destructor
\*****************************************************************************/

big_body_parts::~big_body_parts(void)
{
}


boolean big_body_parts::findData(big_body_file &ioFile)
{
  char        c;
  int         numCRs = 0;
  int         numLFs = 0;

  while(ioFile.readChar(&c) == ioFile.ok) {
    if(c == '\n') {
      numCRs += 1;
    } else if(c == '\r') {
      numLFs += 1;
    } else {
      numCRs = numLFs = 0;
    }

    if((numCRs == 2) && (numLFs == 0))
      return true;
    else if((numCRs == 0) && (numLFs == 2))
      return true;
    else if((numCRs == 2) && (numLFs == 2))
      return true;
  }
  return false;
}


/*****************************************************************************\
findNextHeader - read from that file past the next boundary marker
-------------------------------------------------------------------------------
This function advances the specified file's read position (by reading from the
file) until the specified multi-part boundary is located.  When this function
completes, the file position will be the byte that follows the next boundary,
and TRUE is returned.  If no more boundary strings exist in the file (EOF is
reached), FALSE is returned.

ioFile    specifies the file whose read position is to be advanced past the
          next multi-part boundary
iBoundary points at the boundary string (NOT NULL-terminated!)
iBoundEnd points at the byte past the last character in the boundary string
          pointed at by iBoundary; the length of the boundary string is
          (iBend - iBoundary)
\*****************************************************************************/

fposi_t big_body_parts::findNextHeader(big_body_file &ioFile,
  dnm_buffer &iBoundary)
{
  char    c;
  fposi_t hpos = 0;
  nat4    i    = 0;
  fposi_t pos  = 0;

  if(ioFile.get_position(pos) != ioFile.ok)
    return 0;

  while(i < iBoundary.size()) {
    if(ioFile.readChar(&c) != ioFile.ok)
      return 0;
    pos++;
    if(c == iBoundary[i]) {
      if(i == 0)
        hpos = pos;
      i++;
    } else {
      i = 0;
    }
  }

  return hpos;
}


boolean big_body_parts::findOffsets(big_body_file &ioFile, char *iBoundary,
  char *iBoundEnd)
{
  dnm_buffer    boundary;
  fposi_t       dataOffset;
  fposi_t       offset     = 0;

  boundary.append("\n--", 3);
  boundary.append(iBoundary, iBoundEnd - iBoundary);

  do {
    if(!findData(ioFile))
      return (ioFile.get_size(mOffset) == ioFile.ok);
    if(ioFile.get_position(dataOffset) != ioFile.ok)
      return false;

    big_body_part *newPart = new big_body_part;
    newPart->setOffsets(offset, dataOffset);
    newPart->link_before(this);

    offset = findNextHeader(ioFile, boundary);
  } while(offset != 0);

  return false;
}


boolean big_body_parts::parseParts(os_file &ioFile)
{
  big_body_part *part = getNext();

  while(part != this) {
    if(!part->loadHeader(ioFile))
      return false;
    if(!part->hasFilename() && !part->loadData(ioFile))
      return false;
    part = part->getNext();
  }

  return true;
}

END_GOODS_NAMESPACE
