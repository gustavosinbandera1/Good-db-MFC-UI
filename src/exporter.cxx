// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< BROWSER.CXX >---------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:     30-Oct-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 30-Oct-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Database browser
//-------------------------------------------------------------------*--------*

#include <ctype.h>
#include "goods.h"
#include "client.h"
#include "locale.h"

USE_GOODS_NAMESPACE

struct ref_t { 
    opid_t opid;
    stid_t sid;
    nat2   depth;
};

#define INT_FORMAT "%d"
#define UINT_FORMAT "%u"
#define LONG_FORMAT "%" INT8_FORMAT "d"
#define ULONG_FORMAT "%" INT8_FORMAT "u"
#define FLOAT_FORMAT "%f"
#define DOUBLE_FORMAT "%lf"
#define REF_FORMAT "%x:%x"
#define BLOB_THRESHOLD 1024
#define BLOB_PATH "./"
#define TREAT_ARRAY_OF_INT1_AS_STRING True
#define TREAT_ARRAY_OF_INT2_AS_STRING False
#define TREAT_ARRAY_OF_UINT1_AS_STRING False
#define TREAT_ARRAY_OF_UINT2_AS_STRING True

class dbs_xml_exporter : public dbs_application {
  protected:
    dbs_storage**    storage;
    int              n_storages;
    dnm_array<dbs_class_descriptor*> class_dict;
    dnm_buffer       obj_buf;
    dnm_buffer       cls_buf;
    dnm_queue<ref_t> greyRefs;
    dnm_array<int>*  greyBitmaps;
    FILE*            output;
    int              nBlobs;
    int              maxDepth;

    void             dump_fields(dbs_class_descriptor* cld, size_t
                                 obj_size, int field_no, int n_fields,
                                 char* &refs, char* &bins, int indent, int depth);
    void             dump_object(stid_t sid, objref_t opid, int depth);
    dbs_class_descriptor* get_class_descriptor(stid_t sid, objref_t opid);

    virtual void disconnected(stid_t sid);
    virtual void login_refused(stid_t sid);
    virtual void invalidate(stid_t sid, objref_t opid);
    virtual void receive_message( int message) {}

    void mark(stid_t sid, objref_t opid) { 
        greyBitmaps[sid][opid >> 5] |= 1 << (opid & 31);
    }

    int isMarked(stid_t sid, objref_t opid) { 
        return  greyBitmaps[sid][opid >> 5] & (1 << (opid & 31));
    }

    char* dump_string(char* fieldName, char* bins, int len, bool zeroTerminated);
    char* dump_string(char* fieldName, char* bins, bool zeroTerminated);
    char* dump_wstring(char* fieldName, char* bins);
    char* dump_wstring(char* fieldName, char* bins, int len);
    
  public:
    boolean open(const char* dbs_name);
    void export_xml(ref_t root, int depth, FILE* f);
    void close();
    dbs_xml_exporter() {}
    virtual~dbs_xml_exporter() {}
};

void dbs_xml_exporter::disconnected(stid_t sid)
{
    console::output("Server %d is disconnected\n", sid);
    storage[sid]->close();
    delete storage[sid];
    storage[sid] = NULL;
}

void dbs_xml_exporter::login_refused(stid_t sid)
{
    console::output("Authorization procedure fails at server %d\n", sid);
    storage[sid]->close();
    delete storage[sid];
    storage[sid] = NULL;
}

void dbs_xml_exporter::invalidate(stid_t sid, objref_t opid)
{
    console::output("Object %x:%x was modified\n", sid, opid);
    storage[sid]->forget_object(opid);
}

boolean dbs_xml_exporter::open(const char* dbs_name)
{
    char cfg_file_name[MAX_CFG_FILE_LINE_SIZE];
    char cfg_buf[MAX_CFG_FILE_LINE_SIZE];

    int len = (int)strlen(dbs_name);
    if (len < 4 || strcmp(dbs_name+len-4, ".cfg") != 0) {
        sprintf(cfg_file_name, "%s.cfg", dbs_name);
    } else {
        strcpy(cfg_file_name, dbs_name);
    }
    FILE* cfg = fopen(cfg_file_name, "r");

    if (cfg == NULL) {
        console::output("Failed to open database configuration file: '%s'\n",
                         cfg_file_name);
        return False;
    }
    if (fgets(cfg_buf, sizeof cfg_buf, cfg) == NULL
        || sscanf(cfg_buf, "%d", &n_storages) != 1)
    {
        console::output("Bad format of configuration file '%s'\n",
                         cfg_file_name);
        return False;
    }
    storage = NEW dbs_storage*[n_storages];
    memset(storage, 0, n_storages*sizeof(obj_storage*));

    while (fgets(cfg_buf, sizeof cfg_buf, cfg)) {
        int i;
        char hostname[MAX_CFG_FILE_LINE_SIZE];

        if (sscanf(cfg_buf, "%d:%s", &i, hostname) == 2) {
            if (storage[i] != NULL) {
                console::output("Duplicated entry in configuration file: %s",
                                 cfg_buf);
            }
            storage[i] = new dbs_client_storage(i, this);
            if (!storage[i]->open(hostname)) {
                console::output("Failed to establish connection with server"
                                 " '%s'\n", hostname);
                delete storage[i];
                storage[i] = NULL;
            }
        }
    }
    greyBitmaps = new dnm_array<int>[n_storages];
    nBlobs = 0;
    fclose(cfg);
    return True;
}

void dbs_xml_exporter::close()
{
    delete[] greyBitmaps;
    for (int i = 0; i < n_storages; i++) {
        if (storage[i] != NULL) {
            storage[i]->close();
            delete storage[i];
        }
    }
    delete[] storage;
}

char* dbs_xml_exporter::dump_string(char* fieldName, char* bins, int len, bool zeroTerminated)
{
    if (len > BLOB_THRESHOLD) { 
        char buf[1024];
        sprintf(buf, "%s%s%d.blob", BLOB_PATH, fieldName, ++nBlobs);
        FILE* blob = fopen(buf, "w");
        if (blob == NULL) { 
            console::output("Failed to open BLOB file %s\n", buf);
        } else { 
            size_t rc = fwrite(bins, 1, len, blob);
            if (rc != len) { 
                console::output("Failed to write BLOB file %s\n", buf);
            }
            fclose(blob);
        }
        fprintf(output, "<%s blob=\"file:///%s\"/>\n", fieldName, buf);
    } else { 
        fprintf(output, "<%s>\"", fieldName);
        for (int i = 0; i < len; i++) {
            char ch = bins[i];
            if (zeroTerminated && ch == 0) { 
                break;
            }
            if (!isprint(ch & 0xFF) || ch == '<' || ch == '>' || ch == '&' || ch == '\"') { 
                fprintf(output, "&#%d;", ch);
            } else { 
                fprintf(output, "%c", ch);
            }
        }
        fprintf(output, "\"</%s>\n", fieldName);
    }
    return bins + len;
}

char* dbs_xml_exporter::dump_string(char* fieldName, char* bins, bool zeroTerminated)
{
    nat4 len;
    bins = unpack4((char*)&len, bins);
    return dump_string(fieldName, bins, len, zeroTerminated);
}

char* dbs_xml_exporter::dump_wstring(char* fieldName, char* bins, int len)
{
    fprintf(output, "<%s>\"", fieldName);
    while (len != 0) {
        nat2 ch;
        bins = unpack2((char*)&ch, bins);
        len -= 1;
        if (ch == 0) { 
            break;
        }
        if ((ch & 0xFF00) != 0 || !isprint(ch) || ch == '<' || ch == '>' || ch == '&' || ch == '\"') {
            fprintf(output, "&#%d;", ch);
        } else {
            fprintf(output, "%c", (char)ch);
        }
    }
    fprintf(output, "\"</%s>\n", fieldName);
    return bins + len*2;
}

char* dbs_xml_exporter::dump_wstring(char* fieldName, char* bins)
{
    nat2 len;
    bins = unpack2((char*)&len, bins);
    if(len == 0xFFFF) {
        fprintf(output, "<%s/>", fieldName);
    } else {
        bins = dump_wstring(fieldName, bins, len);
    }
    return bins;
}

inline void indentation(FILE* f, int n) 
{ 
    while (--n >= 0) { 
        putc('\t', f);
    }
}
        
void dbs_xml_exporter::dump_fields(dbs_class_descriptor* cld, size_t obj_size,
                                   int field_no, int n_fields,
                                   char* &refs, char* &bins, int indent, int depth)
{
    nat2 sid;
    objref_t opid;
    int i, j;

    if (n_fields != 0)  {
        int next_field = field_no + n_fields;

        do {
            dbs_field_descriptor* field = &cld->fields[field_no];
            int n = field->is_varying()
                    ? cld->get_varying_length(obj_size) : field->n_items;
            char* fieldName = &cld->names[field->name];
            if (field->size == 1 && field->type == fld_signed_integer) {
                indentation(output, indent);
                if (n == 1) {
                    fprintf(output, "<%s>" INT_FORMAT "</%s>\n",  fieldName, *bins++, fieldName);
                } else {
                    bins = dump_string(fieldName, bins, n, TREAT_ARRAY_OF_INT1_AS_STRING);
                }
#if TREAT_ARRAY_OF_UINT1_AS_STRING
            } else if (field->size == 1 && field->type == fld_unsigned_integer) {
                indentation(output, indent);
                if (n == 1) {
                    fprintf(output, "<%s>" INT_FORMAT "</%s>\n",  fieldName, *bins++ & 0xFF, fieldName);
                } else {
                    bins = dump_string(fieldName, bins, n, True);
                }
#endif
#if TREAT_ARRAY_OF_INT2_AS_STRING
            } else if (field->size == 2 && field->type == fld_signed_integer) {
                indentation(output, indent);
                if (n == 1) {
                    int2 val;
                    bins = unpack2((char*)&val, bins);
                    fprintf(output, "<%s>" INT_FORMAT "</%s>\n",  fieldName, val, fieldName);
                } else {
                    bins = dump_wstring(fieldName, bins, n);
                }
#endif                
#if TREAT_ARRAY_OF_UINT2_AS_STRING
            } else if (field->size == 2 && field->type == fld_unsigned_integer) {
                indentation(output, indent);
                if (n == 1) {
                    nat2 val;
                    bins = unpack2((char*)&val, bins);
                    fprintf(output, "<%s>" INT_FORMAT "</%s>\n",  fieldName, val, fieldName);
                } else {
                    bins = dump_wstring(fieldName, bins, n);
                }
#endif                
            } else {
                for (i = 0; i < n; i++) {
                    for (j = 0; j < indent; j++) { 
                        putc('\t', output);
                    }

                    switch (field->type) {
                    case fld_structure:
                        fprintf(output, "<%s>\n", fieldName);
                        dump_fields(cld, obj_size, field_no+1,
                                    field->next
                                    ? field->next - field_no - 1
                                    : next_field - field_no - 1,
                                    refs, bins, indent+1, depth);
                        for (j = 0; j < indent; j++) { 
                            putc('\t', output);
                        }
                        fprintf(output, "</%s>\n", fieldName);
                        break;
                    case fld_reference:
                        refs = unpackref(sid, opid, refs);
                        if (opid != 0) { 
                            fprintf(output, "<%s ref=\"" REF_FORMAT "\"/>\n", fieldName, sid, (opid_t)opid);
                            if (isMarked(sid, opid) == 0 && depth < maxDepth) { 
                                ref_t r;
                                mark(sid, opid);
                                r.depth = depth + 1;
                                r.sid = sid;
                                r.opid = opid;
                                greyRefs.put(r);
                            }
                        } else { 
                            fprintf(output, "<%s/>\n", fieldName);
                        }
                        break;
                    case fld_signed_integer:
                        fprintf(output, "<%s>", fieldName);
                        if (field->size == 1) {
                            fprintf(output, INT_FORMAT, *(int1*)bins);
                            bins += 1;
                        } else if (field->size == 2) {
                            int2 val;
                            bins = unpack2((char*)&val, bins);
                            fprintf(output, INT_FORMAT, val);
                        } else if (field->size == 4) {
                            int4 val;
                            bins = unpack4((char*)&val, bins);
                            fprintf(output, INT_FORMAT, val);
                        } else {
                            int8 val;
                            bins = unpack8((char*)&val, bins);
                            fprintf(output, LONG_FORMAT, val);
                        }
                        fprintf(output, "</%s>\n", fieldName);
                        break;
                    case fld_unsigned_integer:
                        fprintf(output, "<%s>", fieldName);
                        if (field->size == 1) {
                            fprintf(output, UINT_FORMAT, *(nat1*)bins);
                            bins += 1;
                        } else if (field->size == 2) {
                            nat2 val;
                            bins = unpack2((char*)&val, bins);
                            fprintf(output, UINT_FORMAT, val);
                        } else if (field->size == 4) {
                            nat4 val;
                            bins = unpack4((char*)&val, bins);
                            fprintf(output, UINT_FORMAT, val);
                        } else {
                            nat8 val;
                            bins = unpack8((char*)&val, bins);
                            fprintf(output, ULONG_FORMAT, val);
                        }
                        fprintf(output, "</%s>\n", fieldName);
                        break;
                    case fld_real:
                        fprintf(output, "<%s>", fieldName);
                        if (field->size == 4) {
                            real4 val;
                            bins = unpack4((char*)&val, bins);
                            fprintf(output, FLOAT_FORMAT, val);
                        } else {
                            real8 val;
                            bins = unpack8((char*)&val, bins);
                            fprintf(output, DOUBLE_FORMAT, val);
                        }
                        fprintf(output, "</%s>\n", fieldName);
                        break;
                    case fld_string:
                        bins = dump_wstring(fieldName, bins);
                        break;
                      case fld_raw_binary:
                          bins = dump_string(fieldName, bins, False);
                        break;
                    }
                }
           }
           field_no = field->next;
       } while (field_no != 0);
   }
}

void dbs_xml_exporter::dump_object(stid_t sid, objref_t opid, int depth)
{
    storage[sid]->load(opid, lof_none, obj_buf);
    dbs_class_descriptor* cld = get_class_descriptor(sid, opid);
    if (cld == NULL) {
        return;
    }
    fprintf(output, "<%s id=\"" REF_FORMAT "\">\n",  cld->name(), sid, (opid_t)opid);

    dbs_object_header* hdr = (dbs_object_header*)&obj_buf;
    size_t obj_size = hdr->get_size();
    char* refs = hdr->body();
    char* bins = refs
        + cld->get_number_of_references(obj_size)*sizeof(dbs_reference_t);

    dump_fields(cld, obj_size, 0, cld->n_fields, refs, bins, 1, depth);
    fprintf(output, "</%s>: %hd, %d\n",  cld->name(), sid, (opid_t)opid);
    storage[sid]->forget_object(opid);
}

dbs_class_descriptor* dbs_xml_exporter::get_class_descriptor(stid_t sid,
 objref_t opid)
{
    dbs_class_descriptor* cld;
    dbs_object_header* hdr = (dbs_object_header*)&obj_buf;
    cpid_t cpid = hdr->get_cpid();
    if (cpid == 0) {
        console::output("Object %x:%x is not in the database\n", sid, opid);
        return NULL;
    } else if (cpid == RAW_CPID) {
        console::output("ABSTRACT ROOT OBJECT\n");
        return NULL;
    }
    if ((cld = class_dict[cpid]) == NULL) {
        storage[sid]->get_class(cpid, cls_buf);
        assert(cls_buf.size() != 0);
        cld = (dbs_class_descriptor*)&cls_buf;
        cld->unpack();
        class_dict[cpid] = cld = cld->clone();
    }
    return cld;
}

void dbs_xml_exporter::export_xml(ref_t root, int depth, FILE* f)
{
    output = f;
    maxDepth = depth;
    fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<database>\n");
    greyRefs.put(root);
    mark(root.sid, root.opid);
    do { 
        ref_t r = greyRefs.get();
        dump_object(r.sid, r.opid, r.depth);        
    } while (!greyRefs.is_empty());

    fprintf(f, "</database>\n");
}

int main(int argc, char* argv[])
{
    setlocale(LC_ALL, "");
    if (argc < 2) {
      Error:
        console::output("GOODS database XML export utility\n"
                        "Usage: exporter [-root SID:OPID] [-depth N] <database name> [<xml-file-name>]\n");
        return EXIT_FAILURE;
    }
    int depth = INT_MAX;
    ref_t root;
    root.depth = 0;
    root.sid = 0;
    root.opid = ROOT_OPID;
    char* dbName = NULL;
    FILE* f = NULL;
    for (int i = 1; i < argc; i++) { 
        if (*argv[i] == '-') { 
            if (strcmp("-root", argv[i]) == 0) {
                if (i+1 == argc || sscanf(argv[i+1], "%hx:%x", &root.sid, &root.opid) != 2) { 
                    goto Error;
                }
                i += 1;
            } else if (strcmp("-depth", argv[i]) == 0) { 
                if (i+1 == argc || sscanf(argv[i+1], "%d", &depth) != 1) { 
                    goto Error;
                }
                i += 1;
            } else { 
                goto Error;

            }
        } else {
            if (dbName == NULL) { 
                dbName = argv[i];
            } else if (f == NULL) { 
                f = fopen(argv[i], "w");
                if (f == NULL) {
                    console::output("Failed to open output file\n");
                    return EXIT_FAILURE;
                }
            } else { 
                goto Error;
            }
        }
    }
    if (dbName == NULL) { 
        goto Error;
    }
    if (f == NULL) { 
        f = stdout;
    }
    task::initialize(task::huge_stack);
    dbs_xml_exporter db;
    if (db.open(dbName)) {
        db.export_xml(root, depth, f);
        db.close();
        console::output("Export completed\n");
        return EXIT_SUCCESS;
    } else {
        console::output("Database not found\n");
        return EXIT_FAILURE;
    }
}
