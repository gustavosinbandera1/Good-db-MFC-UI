// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< CGIBROWS.CXX >--------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:     30-Oct-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 30-Oct-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// CGI application for browsing database 
//-------------------------------------------------------------------*--------*

#include "goods.h"
#include "client.h"

USE_GOODS_NAMESPACE

class database_browser : public dbs_application { 
  protected: 
    dbs_storage**  storage;
    char*          database_name;
    int            n_storages;
    dnm_buffer     obj_buf;
    dnm_buffer     cls_buf;

    virtual void disconnected(stid_t sid);
    virtual void login_refused(stid_t sid);
    virtual void invalidate(stid_t sid, opid_t opid);
    virtual void receive_message( int message) {};

  public: 
    boolean open(const char* dbs_name);
    void close();       
    void dump_object(stid_t sid, opid_t opid);
    virtual~database_browser() {}
};      

class browser_console : public console { 
  public: 
    static void report(const char* msg)
    {
        printf("Content-type: text/html\n\n"
          "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">"
          "<HTML><HEAD><TITLE>Database browser message</TITLE></HEAD><BODY>"
          "<H1><FONT COLOR=\"#FF0000\">%s</FONT></H1></BODY></HTML>", msg);
    }
    void output_data(output_type, const char* msg, va_list args)
    {
        char buf[1024];
        vsprintf(buf, msg, args);
        report(buf);
        exit(0);
    }
};

browser_console www_console;
   
void database_browser::disconnected(stid_t sid)
{
    browser_console::report("Server is disconnected");
}

void database_browser::login_refused(stid_t sid)
{
    browser_console::report("Authorization procedure fails at server");
}

void database_browser::invalidate(stid_t, opid_t)
{
}

boolean database_browser::open(const char* dbs_name) 
{
    char cfg_file_name[MAX_CFG_FILE_LINE_SIZE];
    char cfg_buf[MAX_CFG_FILE_LINE_SIZE];

    int len = (int)strlen(dbs_name);
    database_name = new char[len+1];
    strcpy(database_name, dbs_name);
    if (len < 4 || strcmp(dbs_name+len-4, ".cfg") != 0) { 
        sprintf(cfg_file_name, "%s.cfg", dbs_name);
    } else {
        strcpy(cfg_file_name, dbs_name);
    }
    FILE* cfg = fopen(cfg_file_name, "r");

    if (cfg == NULL) { 
        browser_console::report("Failed to open database configuration file"); 
        return False;
    }
    if (fgets(cfg_buf, sizeof cfg_buf, cfg) == NULL 
        || sscanf(cfg_buf, "%d", &n_storages) != 1)
    { 
        browser_console::report("Bad format of configuration file");
        return False;
    }
    storage = new dbs_storage*[n_storages];
    memset(storage, 0, n_storages*sizeof(obj_storage*));

    while (fgets(cfg_buf, sizeof cfg_buf, cfg)) { 
        int i;
        char hostname[MAX_CFG_FILE_LINE_SIZE];

        if (sscanf(cfg_buf, "%d:%s", &i, hostname) == 2) { 
            storage[i] = new dbs_client_storage(i, this);
            if (!storage[i]->open(hostname)) { 
                delete storage[i];
                storage[i] = NULL;
            }
        }
    }
    fclose(cfg);
    return True;
}

void database_browser::close()
{
    delete[] database_name;
    for (int i = 0; i < n_storages; i++) {
        if (storage[i] != NULL) { 
            storage[i]->close();
            delete storage[i];
        }
    }
    delete[] storage; 
}

inline boolean is_ascii(char* s, int len) 
{ 
    while (--len >= 0) { 
        int ch = *s++;
        if (ch != 0 && !isprint(ch & 0xFF)) { 
            return False;
        }
    }
    return True;
}

static char* dump_raw_binary(char* bins)
{
    nat4 len;
    bins = unpack4((char*)&len, bins);
    int i, n = len;
    for (i = 0; i < n && bins[i] > 0 && isprint(bins[i]); i++);
    if (i == n || (i == n-1 && bins[i] == '\0')) { 
        printf("\"%.*s\"", n, bins);
    } else { 
        char sep = '{';
        for (i = 0; i < n; i++) { 
            printf("%c0x%02x", sep, bins[i] & 0xFF);
            sep = ',';
        }
        if (sep == '{') { 
            printf("{}");
        } else { 
            printf("}");
        }
    }
    return bins + n;
}

static char* dump_imu_string(char* bins)
{
    nat2 len;
    bins = unpack2((char*)&len, bins);
    if(len == 0xFFFF) {
        printf("null");
    } else {
        printf("\"");
        while(len-- > 0) {
            nat2 ch;
            bins = unpack2((char*)&ch, bins);
            if((ch & 0xFF00) || !isprint(ch)) {
                printf("\\u%04X", ch);
            } else if (ch == '<') {
                printf("&lt;");
            } else if (ch == '>') {
                printf("&gt;");
            } else if (ch == '&') {
                printf("&amp;");
            } else if (ch == '"') {
                printf("&quot;");
            } else {
                putchar((char)ch);
            }
        }
        printf("\"");
    }
    return bins;
}


void dump_fields(dbs_class_descriptor* cld, size_t obj_size,
                 int field_no, int n_fields,
                 char* prefix, int prefix_len, char* database_name,  
                 char* &refs, char* &bins)
{
    if (n_fields == 0)  { 
        return;
    }
    nat2 sid;
    nat4 opid;
    int  next_field = field_no + n_fields;

    do {        
        dbs_field_descriptor* field = &cld->fields[field_no];
        int len = prefix_len;
        prefix_len += sprintf(prefix+len, "%s", &cld->names[field->name]);
        int n = field->is_varying() 
            ? cld->get_varying_length(obj_size) : field->n_items;
        if (field->size == 1 && is_ascii(bins, n)) { 
            if (n == 1) { 
                char ch = *bins++;
                if (ch == 0) { 
                    printf("<TR><TD>%s</TD><TD>0</TD></TR>", prefix);
                } else { 
                    printf("<TR><TD>%s</TD><TD>'%c'(%X)</TD></TR>", 
                           prefix, ch, nat1(ch));
                }
            } else {    
                printf("<TR><TD>%s</TD><TD>\"%.*s\"</TD></TR>", 
                       prefix, n, bins); 
                bins += n; 
            } 
        } else {
            long    prev_val = 0;
            opid_t  prev_opid = 0;
            stid_t  prev_sid = 0;
            boolean etc = False;
            int     len2 = prefix_len;
            
            for (int i = 0; i < n; i++) { 
                if (n > 1) { 
                    prefix_len += sprintf(prefix+len2, "[%d]", i);
                }
                switch (field->type) { 
                  case fld_structure:
                    prefix[prefix_len++] = '.';
                    prefix[prefix_len] = '\0';
                    dump_fields(cld, obj_size, field_no+1, 
                                field->next ? field->next - field_no - 1 
                                            : next_field - field_no - 1,
                                prefix, prefix_len, database_name, 
                                refs, bins);
                    break;
                  case fld_reference:
                    refs = unpackref(sid, opid, refs);
                    if (i > 0 && prev_sid == sid && prev_opid == opid) { 
                        etc = True;
                    } else {
                        if (etc) { 
                            printf("<TR><TD>...</TD><TD>...</TD></TR>");
                            etc = False;
                        } 
                        prev_opid = opid;
                        prev_sid = sid;
                        if (opid == 0) { 
                            printf("<TR><TD>%s</TD><TD>NIL</TD></TR>",
                                   prefix);
                        } else {
                            printf("<TR><TD>%s</TD><TD>"
                                   "<A HREF=\"http://%s%s?%x:%x@%s\">"
                                   "%x:%x</TD></TR>",
                                   prefix, 
                                   getenv("HTTP_HOST"), getenv("SCRIPT_NAME"), 
                                   sid, opid, database_name, 
                                   sid, opid);
                        }
                    }
                    break;
                  case fld_signed_integer:
                    if (field->size == 1) { 
                        int1 val = *(int1*)bins++;
                        if (i > 0 && prev_val == val) { 
                            etc = True;
                        } else { 
                            if (etc) { 
                                printf("<TR><TD>...</TD><TD>...</TD></TR>");
                                etc = False;
                            }
                            prev_val = val;
                            printf("<TR><TD>%s</TD><TD>%d</TD></TR>", 
                                   prefix, val);
                        }
                    } else if (field->size == 2) { 
                        int2 val;
                        bins = unpack2((char*)&val, bins);
                        if (i > 0 && prev_val == val) { 
                            etc = True;
                        } else { 
                            if (etc) { 
                                printf("<TR><TD>...</TD><TD>...</TD></TR>");
                                etc = False;
                            }
                            prev_val = val;
                            printf("<TR><TD>%s</TD><TD>%d</TD></TR>", 
                                   prefix, val);
                        }
                    } else if (field->size == 4) { 
                        int4 val;
                        bins = unpack4((char*)&val, bins);
                        if (i > 0 && prev_val == val) { 
                            etc = True;
                        } else { 
                            if (etc) { 
                                printf("<TR><TD>...</TD><TD>...</TD></TR>");
                                etc = False;
                            }
                            prev_val = val;
                            printf("<TR><TD>%s</TD><TD>%d</TD></TR>", 
                                   prefix, val);
                        }
                    } else { 
                        int8 val;
                        bins = unpack8((char*)&val, bins);
                        printf("<TR><TD>%s</TD><TD>%"INT8_FORMAT"d</TD></TR>",
                               prefix, val);
                    }
                    break;
                  case fld_unsigned_integer:
                    if (field->size == 1) { 
                        nat1 val = *(nat1*)bins++;
                        if (i > 0 && nat1(prev_val) == val) { 
                            etc = True;
                        } else { 
                            if (etc) { 
                                printf("<TR><TD>...</TD><TD>...</TD></TR>");
                                etc = False;
                            }
                            prev_val = val;
                            printf("<TR><TD>%s</TD><TD>0x%x</TD></TR>", 
                                   prefix, val);
                        }
                    } else if (field->size == 2) { 
                        nat2 val;
                        bins = unpack2((char*)&val, bins);
                        if (i > 0 && nat2(prev_val) == val) { 
                            etc = True;
                        } else { 
                            if (etc) { 
                                printf("<TR><TD>...</TD><TD>...</TD></TR>");
                                etc = False;
                            }
                            prev_val = val;
                            printf("<TR><TD>%s</TD><TD>0x%x</TD></TR>", 
                                   prefix, val);
                        }
                    } else if (field->size == 4) { 
                        nat4 val;
                        bins = unpack4((char*)&val, bins);
                        if (i > 0 && nat4(prev_val) == val) { 
                            etc = True;
                        } else { 
                            if (etc) { 
                                printf("<TR><TD>...</TD><TD>...</TD></TR>");
                                etc = False;
                            }
                            prev_val = val;
                            printf("<TR><TD>%s</TD><TD>0x%x</TD></TR>", 
                                   prefix, val);
                        }
                    } else { 
                        nat8 val;
                        bins = unpack8((char*)&val, bins);
                        printf("<TR><TD>%s</TD><TD>%"INT8_FORMAT"x</TD></TR>",
                               prefix, val);
                    }
                    break;
                  case fld_real:
                    if (field->size == 4) { 
                        real4 val;
                        bins = unpack4((char*)&val, bins);
                        printf("<TR><TD>%s</TD><TD>%f</TD></TR>", prefix, val);
                    } else { 
                        real8 val;
                        bins = unpack8((char*)&val, bins);
                        printf("<TR><TD>%s</TD><TD>%f</TD></TR>", prefix, val);
                    }
                    break;
                  case fld_string:
                    printf("<TR><TD>%s</TD><TD>", prefix);
                    bins = dump_imu_string(bins);
                    printf("</TD></TR>");
                    break;                  
                  case fld_raw_binary:
                    printf("<TR><TD>%s</TD><TD>", prefix);
                    bins = dump_raw_binary(bins);
                    printf("</TD></TR>");
                    break;                  
                }
                prefix_len = len2;                  
            }
            if (etc) { 
                printf("<TR><TD>%.*s[%d]</TD><TD>...</TD></TR>", 
                       prefix_len, prefix, n-1);
            } 
        }
        prefix_len = len;
        field_no = field->next;   
    } while (field_no != 0);
}

void database_browser::dump_object(stid_t sid, opid_t opid)
{ 
    dbs_class_descriptor* cld;
    char buf[1024];
    if (sid >= n_storages || storage[sid] == NULL) { 
        browser_console::report("Storage is not in configuration file or not available");
        return;
    }
    storage[sid]->load(opid, lof_none, obj_buf);

    dbs_object_header* hdr = (dbs_object_header*)&obj_buf;
    cpid_t cpid = hdr->get_cpid();
    if (cpid == 0) { 
        browser_console::report("Object is not in the database"); 
        return;
    } else if (cpid == RAW_CPID) { 
        browser_console::report("Abstract root object"); 
        return;
    }   
    storage[sid]->get_class(cpid, cls_buf);
    assert(cls_buf.size() != 0);

    cld = (dbs_class_descriptor*)&cls_buf;
    cld->unpack();
    printf("Content-type: text/html\n\n");
    printf("<!DOCTYPEHTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">");
    printf("<HTML><HEAD><TITLE>Object \"%s\" %x:%x</TITLE></HEAD><BODY>",
           cld->name(), sid, opid);
    printf("<H2><P ALIGN=\"CENTER\">%s %x:%x</P><H2><P>", 
           cld->name(), sid, opid);
    printf("<TABLE BORDER ALIGN=\"CENTER\"><TR BGCOLOR=\"#A0A0A0\">"
           "<TH>Component</TH><TH>Value</TH></TR>");
    
    size_t obj_size = hdr->get_size();
    char* refs = hdr->body();
    char* bins = refs 
        + cld->get_number_of_references(obj_size)*sizeof(dbs_reference_t);
    
    dump_fields(cld, obj_size, 0, cld->n_fields, buf, 0, database_name, 
                refs, bins);
    printf("</TABLE></BODY></HTML>");
}

int main()
{
    char* query_string = getenv("QUERY_STRING");
    if (query_string != NULL) {
        task::initialize(task::huge_stack);
        database_browser db;    
        int sid;
        int opid = ROOT_OPID;
        char dbs_name[64];
        char* p = strchr(query_string, '&');

        if (p != NULL) { // open form
            *p = '\0';
            if (sscanf(query_string, "database=%s", dbs_name) != 1 ||
                sscanf(p+1, "storage=%d", &sid) != 1)
            {
                browser_console::report("Invalid parameters are specified");
                return 0;
            } 
        } else { // go by reference
            if (sscanf(query_string, "%x:%x@%s", &sid, &opid, dbs_name) != 3) {
                browser_console::report("Invalid format of reference");
                return 0;
            } 
        }
        if (db.open(dbs_name)) { 
            db.dump_object(sid, opid);
            db.close();
        }
    }
    return 0;
}


