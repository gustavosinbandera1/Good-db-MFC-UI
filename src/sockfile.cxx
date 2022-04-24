// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< SOCKFILE.CXX >------------------------------------------------+-----------
// GOODS                     Version 1.0         (c) 2006  SETER   |   *
// (Generic Object Oriented Database System)                       |      /\ v
//                                                                 |_/\__/  \__
//                        Created:     03-Jan-07    Marc Seter     |/  \/    \/ //                        Last update: 03-Jan-07    Marc Seter     |Marc Seter
//-----------------------------------------------------------------+-----------
// Provide a file subclass that streams data across a socket.  Really helpful
// when you want to backup or restore the database using an archive stored on
// a remote server.
//-----------------------------------------------------------------------------

#include "stdinc.h"
#include "sockfile.h"
#include "sockio.h"

BEGIN_GOODS_NAMESPACE

socket_file::socket_file(socket_t *io_socket) :
    file(),
    m_socket(io_socket)
{
}

socket_file::~socket_file()
{
    close();
}

file* socket_file::clone()
{
    return new socket_file(m_socket);
}

file::iop_status socket_file::close()
{
    if (!m_socket || m_socket->close()) {
        return ok;
    }
    return not_opened;
}

void socket_file::dump()
{
    console::output("socket_file::dump() has nothing to say.\n");
}

file::iop_status socket_file::flush()
{
    // socket flushing not wrapped by GOODS - at least not yet...
    return ok;
}

file::iop_status socket_file::open(access_mode, int)
{
    // socket should already be opened.
    return ok;
}

file::iop_status socket_file::remove()
{
    return close();
}

char const* socket_file::get_name() const
{
    return m_socket->get_peer_name();
}

file::iop_status socket_file::set_name(char const* new_name)
{
    // not allowed for a socket...
    return not_opened;
}

file::iop_status socket_file::get_size(fsize_t& ) const
{
    // size is unknown for a socket...
    return not_opened;
}

file::iop_status socket_file::set_size(fsize_t new_size)
{
    // size of a socket stream cannot be set...
    return not_opened;
}

void socket_file::get_error_text(iop_status code,
                                 char* buf, size_t buf_size) const
{
    m_socket->get_error_text(buf, buf_size);
}

file::iop_status socket_file::read(fposi_t, void*, size_t)
{
    // sockets (streams) don't support random access...
    return not_opened;
}

file::iop_status socket_file::write(fposi_t, void const*, size_t)
{
    // sockets (streams) don't support random access...
    return not_opened;
}

file::iop_status socket_file::set_position(fposi_t)
{
    // sockets (streams) don't support random access...
    return not_opened;
}

file::iop_status socket_file::get_position(fposi_t&)
{
    // sockets (streams) don't support random access...
    return not_opened;
}

file::iop_status socket_file::read(void* buf, size_t size)
{
    if (!m_socket->read(buf, size)) {
        return file::end_of_file;
    }
    return ok;
}

file::iop_status socket_file::write(void const* buf, size_t size)
{
    if (!m_socket->write(buf, size)) {
        return not_opened;
    }
    return ok;
}

END_GOODS_NAMESPACE
