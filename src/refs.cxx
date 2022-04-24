
#include "goods.h"
#include "..\inc\refs.h"

mutex object_reference::m_blocker_cs;

void object_reference::blocker_lock() const
{
	m_blocker_cs.enter();
}

void object_reference::blocker_unlock() const
{
	m_blocker_cs.leave();
}