#include "goods.h"
#include "osfile.h"
#include "sockio.h"
#include "support.h"

USE_GOODS_NAMESPACE

#include <stdio.h>

static const size_t k_bufsize = 1024 * 1024;

static const char *k_usage =
 "Usage: %s --backup/--restore port [file]\n\n"
 " this utility backs up or restores a GOODS database over a network.  It's\n"
 " useful if you've got barely enough space on your database server for the\n"
 " GOODS database (and not enough for the backup file!), or if you're using\n"
 " gigabit ethernet or fiber connections to make database backups & restores\n"
 " faster.\n\n"
 " --backup\n"
 "   tells garcc to create a GOODS backup, receiving data from the GOODS\n"
 "   database server's garcc port and streaming it into the specified file.\n\n"
 " --restore\n"
 "   tells garcc to restore a GOODS database using the specified backup\n"
 "   file, by streaming the backup file to the GOODS database server's garcc\n"
 "   port.\n"
 " port\n"
 "   specifies the GOODS database server's garcc port.  This port is set\n"
 "   using the \"server.garcc_port\" option in the database's .srv file.\n"
 " file\n"
 "   specifies the name of a local file.  If `garcc --backup ...` is used,\n"
 "   a GOODS database backup will be streamed into this file.\n"
 "   If `garcc --restore ...` is used, this file will contain the backup\n"
 "   used to restore a GOODS database.\n\n"
 "   If not specified, backups stream to stdout and restores use stdin.\n"
 "   You can then use pipes to compress the backup on-the-fly.  Examples:\n"
 "   . garcc --backup 10.0.0.100:3007 | gzip > some_backup_file_bak.gz\n"
 "   . zcat some_backup_file_bak.gz | garcc --restore 10.0.0.100:3007\n\n";


enum ReturnCodes {
 k_ok                            = 0,
 k_couldnt_connect_to_server     = -1,
 k_couldnt_create_file           = -2,
 k_couldnt_write_to_file         = -3,
 k_couldnt_open_file_for_read    = -4,
 k_couldnt_read_backup_file_size = -5,
 k_couldnt_read_from_file        = -6,
 k_couldnt_write_to_garcc_port   = -7
};


int do_backup(socket_t *io_sock, const char *i_port, const char *i_file)
{
    dnm_buffer        buf;
    int               nr;
    size_t            nw;
    os_file           out(i_file);
    file::iop_status  status;

    buf.put(k_bufsize);

    if (i_file != NULL) {
        status = out.open(file::fa_write,
            file::fo_largefile|file::fo_truncate|file::fo_create);
        if (status != file::ok) {
            fprintf(stderr, "Could not create file \"%s\"\n\n", i_file);
            return k_couldnt_create_file;
        }
    }

    io_sock->write("b", 1);

    while ((nr = io_sock->read(buf.getPointerAt(0), 1, k_bufsize)) > 0) {
        nw = (size_t)nr;
        if (i_file != NULL) {
            status = out.write(buf.getPointerAt(0), nw);

            if (status != file::ok) {
                out.remove();
                fprintf(stderr, "Error while writing file \"%s\"; "
                        "disk full?\n\n", i_file);
                return k_couldnt_write_to_file;
            }
        } else {
            if (fwrite(buf.getPointerAt(0), sizeof(char), nw, stdout) != nw) {
                fprintf(stderr, "Error while writing backup to stdout.\n\n");
                return k_couldnt_write_to_file;
            }
        }
    }
    return k_ok;
}


int do_restore(socket_t *io_sock, const char *i_port, const char *i_file)
{
    dnm_buffer        buf;
    size_t            d_size;
    os_file           in(i_file);
    fsize_t           size;
    fsize_t           size_n;
    file::iop_status  status;

    buf.put(k_bufsize);

    status = in.open(file::fa_read, file::fo_largefile);
    if (status != file::ok) {
        fprintf(stderr, "Could not open file \"%s\" for reading.\n\n",
                i_file);
        return k_couldnt_open_file_for_read;
    }

    io_sock->write("r", 1);

    status = in.get_size(size_n);
    if (status != file::ok) {
        fprintf(stderr, "Failed to read backup file's size.\n\n");
        return k_couldnt_read_backup_file_size;
    }

    for (size = 0; size < size_n; size += k_bufsize) {
        d_size = (size_n - size > k_bufsize) ? k_bufsize : size_n - size;
        status = in.read(buf.getPointerAt(0), d_size);
        if (status != file::ok) {
            fprintf(stderr, "Error while reading file \"%s\" "
                "at pos %" INT8_FORMAT "u.\n\n", i_file, (nat8)size);
            return k_couldnt_read_from_file;
        }

        if (!io_sock->write(buf.getPointerAt(0), d_size)) {
            if (size == 0) {
                fprintf(stderr, "Restore failed; storage not closed?\n\n");
            } else {
                fprintf(stderr, "Error while writing to garcc server port "
                        "\"%s\" at pos %" INT8_FORMAT "u.\n\n", i_port,
                        (nat8)size);
            }
            return k_couldnt_write_to_garcc_port;
        }
    }
    return k_ok;
}


int do_restore_stdin(socket_t *io_sock, const char *i_port)
{
    dnm_buffer        buf;
    boolean           done       = false;
    long              d_size;
    fsize_t           size       = 0;

    buf.put(k_bufsize);

    io_sock->write("r", 1);

    while (!done) {
        d_size = (long)fread(buf.getPointerAt(0), sizeof(char), k_bufsize, stdin);
        if (d_size < 0) {
            fprintf(stderr, "Error while reading backup file from stdin "
                    "at pos %" INT8_FORMAT "u.\n\n", size);
            return k_couldnt_read_from_file;
        } else if (d_size > 0) {
            if (!io_sock->write(buf.getPointerAt(0), d_size)) {
                fprintf(stderr, "Error while writing to garcc server port "
                    "\"%s\" at pos %" INT8_FORMAT "u.\n\n", i_port, (nat8)size);
                return k_couldnt_write_to_garcc_port;
            }
        } else {
            done = true;
        }
    }
    return k_ok;
}


int main(int argc, char **argv)
{
    const char       *file   = NULL;
    const char       *op;
    const char       *port;
    int               result;
    socket_t         *sock   = NULL;

    task::initialize();

    if ((argc != 3) && (argc != 4))
    {
        printf(k_usage, argv[0]);
        return 0;
    }

    op   = argv[1];
    port = argv[2];
    if (argc == 4) {
        file = argv[3];
    }

    if(!(sock = socket_t::connect(port, socket_t::sock_global_domain)))
    {
        fprintf(stderr, "Could not connect to \"%s\".\n\n", port);
        return k_couldnt_connect_to_server;
    }

    if (strcmp(op, "--backup") == 0) {
        result = do_backup(sock, port, file);
    } else if (strcmp(op, "--restore") == 0) {
        if (file != NULL) {
            result = do_restore(sock, port, file);
        } else {
            result = do_restore_stdin(sock, port);
        }
    } else {
        printf(k_usage, argv[0]);
    }
    delete sock;

    return result;
}
