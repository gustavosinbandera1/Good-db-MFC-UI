

#ifndef __EXTRAFILE_H__
#define __EXTRAFILE_H__

#include "osfile.h"

BEGIN_GOODS_NAMESPACE

// -- class sequencial_pool_file
class sequential_buffered_file : public os_file
{
public:
	sequential_buffered_file(char const *pFileName = 0) 
		: os_file(pFileName) 
	{}
	virtual file::iop_status open(access_mode mode, int flags);
};

END_GOODS_NAMESPACE

#endif