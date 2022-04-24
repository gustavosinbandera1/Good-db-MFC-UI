// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< EXVAL.CXX >-----------------------------------------------------*--------*
//-------------------------------------------------------------------*--------*
// GOODS post extract from backup database validator
//-------------------------------------------------------------------*--------*

#include "goods.h"
#include "server.h"

class database_validator {
  protected:
    dnm_buffer  buf;
    const char* filename;
    os_file*    odb;
    fsize_t     size;

    char* read(fposi_t i_pos, size_t i_size);

    enum { k_max_buf = (1024 * 1024) };

  public:
    database_validator(void);
    virtual ~database_validator(void);

    void    close(void);
    fsize_t get_size(void) { return size; };
    boolean open(const char* db_file_name);
    boolean validate(fposi_t i_pos, size_t i_size, database_validator &i_that);
};

static const char* k_usage =
"GOODS post extract from backup database validator\n\n"
"Usage: exval <database odb> <databaseCopy odb> <backup odb>\n\n"
"exval validates that rolling back a set of objects copied exactly the right\n"
"bytes from the specified backup file, and modified ONLY those bytes in the\n"
"specified database file.\n\n"
"This tool expects two columns of numbers it can read on stdin.  Each row of\n"
"input contains the offset and size of each object rolled back using the\n"
"'extract' GOODS administrative console command.\n\n";

int main(int argc, char* argv[])
{
    if (argc != 4) {
        fprintf(stdout, "%s", k_usage);
        return EXIT_FAILURE;
    }

    // Open the odb files to be compared.
    database_validator db, db_back, db_copy;
    if (!db.open(argv[1]) || !db_copy.open(argv[2]) || !db_back.open(argv[3])) {
        return EXIT_FAILURE;
    }

    fposi_t i = 0;
    while (i < db.get_size()) {
        nat8 size = 0;
        nat8 pos = 0;

        if (scanf("%"INT8_FORMAT"u %"INT8_FORMAT"u\n", &pos, &size) == EOF)
            break;

        if (i < pos) {
            db.validate(i, pos - i, db_copy);
        }
        db.validate(pos, size, db_back);
        i = pos + size;
    }

    if (i < db.get_size()) {
        db.validate(i, db.get_size() - i, db_copy);
    }

    return EXIT_SUCCESS;
}

database_validator::database_validator(void) :
    buf(),
    filename(NULL),
    odb(NULL),
    size(0)
{
}

database_validator::~database_validator(void)
{
    close();
}

void database_validator::close(void)
{
    if (odb) {
        odb->close();
        delete odb;
        odb = NULL;
    }
}

boolean database_validator::open(const char* db_file_name)
{
    close();
    filename = db_file_name;
    odb = new os_file(filename);
    if ((odb->open(odb->fa_read, odb->fo_random | odb->fo_largefile) != odb->ok)
       || (odb->get_size(size) != odb->ok)) {
        printf("Failed opening file \"%s\".\n", filename);
        close();
        return false;
    }
    return true;
}

char* database_validator::read(fposi_t i_pos, size_t i_size)
{
    if (i_pos + i_size > size) {
        printf("Block at offset %"INT8_FORMAT"u, size %"INT8_FORMAT"u is "
               "outside bounds of file \"%s\".\n", i_pos, (fsize_t)i_size, filename);
        return NULL;
    }

    char* p = buf.put(i_size);
    if (odb->read(i_pos, p, i_size) != odb->ok) {
        printf("Error reading %"INT8_FORMAT"u bytes from file %s at offset %"
               INT8_FORMAT"u.\n", (fsize_t)i_size, filename, i_pos);
        return NULL;
    }

    return p;
}

boolean database_validator::validate(fposi_t i_pos, size_t i_size,
    database_validator &i_that)
{
    fposi_t d_pos  = i_pos;
    size_t  d_size = i_size;

    while (d_size > 0) {
        size_t inc = (d_size < k_max_buf) ? d_size : k_max_buf;

        char* dThis = read(d_pos, inc);
        char* dThat = i_that.read(d_pos, inc);

        if (!dThis || !dThat)
            return false;

        if (memcmp(dThis, dThat, inc) != 0) {
            printf("Failed to validate block; pos: %"INT8_FORMAT"u, size; %"
                   INT8_FORMAT"u.\n", i_pos, (fsize_t)i_size);
            return false;
        }

        d_pos  += inc;
        d_size -= inc;
    }

    return true;
}
