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

const char* format_i = "%d";
const char* format_u = "%u";

// Would be neater to ask each storage its object index size, but this works.
// This is the object index size for a 2.1GB (2^31) .idx file.
const int k_max_objects_per_storage = 178956970;

class magaya_client_storage : public dbs_client_storage
{
public:
	magaya_client_storage(stid_t sid, dbs_application* app) : dbs_client_storage(sid, app) { }
protected:
	virtual boolean open(const char* server_connection_name, const char* login, const char* password)
	{
		sock = socket_t::connect(server_connection_name);
		if (!sock->is_ok()) 
		{ 
			msg_buf buf;
			sock->get_error_text(buf, sizeof buf);
			console::output("Failed to connect server '%s': %s\n", server_connection_name, buf);
			delete sock;
			sock = NULL; 
			return False;
		}

		opened = True;
		closing = False;
		waiting_reply = False;
		proceeding_invalidation = False;

		notify_buf.change_size(0);

		char host_name[MAX_LOGIN_NAME];
		char user_name[MAX_LOGIN_NAME];
		DWORD nSize = MAX_LOGIN_NAME;
		::GetUserName(user_name, &nSize);
		gethostname(host_name, MAX_LOGIN_NAME);

		char login_name[MAX_LOGIN_NAME];
		size_t login_name_len;

		if (login != NULL) 
		{ 
			if (password == NULL) password = "";
			login_name_len = sprintf(login_name, 
									 "1\t%s\t%s\t%s\t%x\t%p\t%s\t%s", 
									 "el", host_name, user_name, GetCurrentProcessId(), task::current(), login, password);
		}
		else
		{
			login_name_len = sprintf(login_name, 
									 "0\t%s\t%s\t%s\t%x\t%p", 
									 "el", host_name, user_name, GetCurrentProcessId(), task::current());
		}

		snd_buf.put(sizeof(dbs_request) + login_name_len);

		dbs_request* login_req = (dbs_request*)&snd_buf;
		login_req->cmd = dbs_request::cmd_login;
		login_req->login.name_len = login_name_len;
		strcpy((char*)(login_req+1), login_name); 

		write(login_req, sizeof(dbs_request) + login_name_len); 
		dbs_request rcv_req;
		read(&rcv_req, sizeof rcv_req);
		rcv_req.unpack();

		assert(rcv_req.cmd == dbs_request::cmd_ok ||
			   rcv_req.cmd == dbs_request::cmd_bye ||
			   rcv_req.cmd == dbs_request::cmd_refused);

		switch (rcv_req.cmd) 
		{ 
			case dbs_request::cmd_bye: 
				opened = False;
				application->disconnected(id);
				return False;

			case dbs_request::cmd_refused: 
				opened = False;
				application->receive_message(rcv_req.any.arg3);
				application->login_refused(id);
				return False;
		}   

		task::create(magaya_client_storage::receiver, this, task::pri_high); 
		return True;
	}
};

class database_browser : public dbs_application {
  protected:
    dbs_storage**  storage;
    int            n_storages;
    dnm_array<dbs_class_descriptor*> *class_dict;
    dnm_buffer     obj_buf;
    dnm_buffer     cls_buf;
    const char*    field_to_set;
    int            field_to_set_index;
    const char*    value_to_set;
    const char*    set_error;
    opid_t         opid_to_find;
    stid_t         sid_to_find;
    dnm_buffer*    search_maps;
    nat4           find_count;

    void                  dump_fields(dbs_class_descriptor* cld, size_t
                                      obj_size, int field_no, int n_fields,
                                      char* &refs, char* &bins);
    void                  dump_object(stid_t sid, opid_t opid);
    int                   find(stid_t sid, opid_t opid);
    boolean               find_path_to_object(void);
    dbs_class_descriptor* get_class_descriptor(stid_t sid, opid_t opid);
    dbs_field_descriptor* get_field_descriptor(stid_t sid, opid_t opid,
                                               const char* iFieldName);
    boolean               set_field_value(dbs_class_descriptor* iClassDesc,
                                          dbs_field_descriptor* iFieldDesc,
                                          char* oField, int obj_size);
    boolean               validate_object_id(stid_t sid, opid_t opid);

    virtual void disconnected(stid_t sid);
    virtual void login_refused(stid_t sid);
    virtual void invalidate(stid_t sid, opid_t opid);
    virtual void receive_message( int message) {};

  public:
    boolean open(const char* dbs_name, char const* login = NULL, char const* password = NULL);
    void close();
    void dialogue();
    void find(char* iCmd);
    void lock(char* iCmd, bool iLock);
    void set(char* iCmd);
    database_browser();
    virtual~database_browser() {}
};

database_browser::database_browser() :
 field_to_set(NULL),
 field_to_set_index(-1),
 value_to_set(NULL),
 set_error(NULL),
 opid_to_find(0),
 sid_to_find(0),
 search_maps(NULL),
 find_count(0)
{
}

void database_browser::disconnected(stid_t sid)
{
    console::output("Server %d is disconnected\n", sid);
    storage[sid]->close();
    delete storage[sid];
    storage[sid] = NULL;
}

void database_browser::login_refused(stid_t sid)
{
    console::output("Authorization procedure fails at server %d\n", sid);
    storage[sid]->close();
    delete storage[sid];
    storage[sid] = NULL;
}

void database_browser::invalidate(stid_t sid, opid_t opid)
{
    console::output("Object %x:%x was modified\n", sid, opid);
    storage[sid]->forget_object(opid);
}

boolean database_browser::open(const char* dbs_name, char const* login, char const* password)
{
    char cfg_file_name[MAX_CFG_FILE_LINE_SIZE];
    char cfg_buf[MAX_CFG_FILE_LINE_SIZE];

    int len = strlen(dbs_name);
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

    class_dict = new dnm_array<dbs_class_descriptor*>[n_storages];
    while (fgets(cfg_buf, sizeof cfg_buf, cfg)) {
        int i;
        char hostname[MAX_CFG_FILE_LINE_SIZE];

        if (sscanf(cfg_buf, "%d:%s", &i, hostname) == 2) {
            if (storage[i] != NULL) {
                console::output("Duplicated entry in configuration file: %s",
                                 cfg_buf);
            }
            //storage[i] = new dbs_client_storage(i, this);
			storage[i] = new magaya_client_storage(i, this);
            if (!storage[i]->open(hostname, login, password)) {
                console::output("Failed to establish connection with server"
                                 " '%s'\n", hostname);
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
        console::output("\"%.*s\"", n, bins);
    } else {
        char sep = '{';
        for (i = 0, n = len; i < n; i++) {
            console::output("%c0x%02x", sep, bins[i] & 0xFF);
            sep = ',';
        }
        if (sep == '{') {
            console::output("{}");
        } else {
            console::output("}");
        }
    }
    return bins + n;
}

static char* dump_imu_string(char* bins)
{
    nat2 len;
    bins = unpack2((char*)&len, bins);
    if(len == 0xFFFF) {
        console::output("null");
    } else {
        console::output("\"");
        while(len-- > 0) {
            nat2 ch;
            bins = unpack2((char*)&ch, bins);
            if((ch & 0xFF00) || !isprint(ch)) {
                console::output("\\u%04X", ch);
            } else if (ch == '\\') {
                console::output("\\\\");
            } else {
                console::output("%c", (char)ch);
            }
        }
        console::output("\"");
    }
    return bins;
}


void database_browser::dump_fields(dbs_class_descriptor* cld, size_t obj_size,
                 int field_no, int n_fields,
                 char* &refs, char* &bins)
{
    nat2 sid;
    nat4 opid;
    boolean first = True;

    console::output("{");
    if (n_fields != 0)  {
        int next_field = field_no + n_fields;

        do {
            dbs_field_descriptor* field = &cld->fields[field_no];
            if (!first) {
                console::output(", ");
            }
            first = False;
            console::output("%s=", &cld->names[field->name]);
            int n = field->is_varying()
                    ? cld->get_varying_length(obj_size) : field->n_items;
            if (field->size == 1 && is_ascii(bins, n)) {
                if (n == 1) {
                    set_field_value(cld, field, bins, obj_size);
                    char ch = *bins++;
                    if (ch == 0) {
                        console::output("0");
                    } else {
                        console::output("'%c'(%X)", ch, nat1(ch));
                    }
                } else {
                    set_field_value(cld, field, bins, obj_size);
                    console::output("\"%.*s\"", n, bins);
                    bins += n;
                }
            } else {
                if (n > 1) {
                    console::output("{");
                }
                for (int i = 0; i < n; i++) {
                    switch (field->type) {
                      case fld_structure:
                        set_field_value(cld, field, bins, obj_size);
                        dump_fields(cld, obj_size, field_no+1,
                            field->next
                                ? field->next - field_no - 1
                                : next_field - field_no - 1,
                            refs, bins);
                        break;
                      case fld_reference:
                        set_field_value(cld, field, refs, obj_size);
                        refs = unpackref(sid, opid, refs);
                        console::output("%x:%x", sid, opid);
                        break;
                      case fld_signed_integer:
                        if (i == 0)
                            set_field_value(cld, field, bins, obj_size);
                        if (field->size == 1) {
                            console::output(format_i, *(int1*)bins);
                            bins += 1;
                        } else if (field->size == 2) {
                            int2 val;
                            bins = unpack2((char*)&val, bins);
                            console::output(format_i, val);
                        } else if (field->size == 4) {
                            int4 val;
                            bins = unpack4((char*)&val, bins);
                            console::output(format_i, val);
                        } else {
                            int8 val;
                            bins = unpack8((char*)&val, bins);
                            console::output("%X%08x", int8_high_part(val),
                                                      int8_low_part(val));
                        }
                        break;
                      case fld_unsigned_integer:
                        set_field_value(cld, field, bins, obj_size);
                        if (field->size == 1) {
                            console::output(format_u, *(nat1*)bins);
                            bins += 1;
                        } else if (field->size == 2) {
                            nat2 val;
                            bins = unpack2((char*)&val, bins);
                            console::output(format_u, val);
                        } else if (field->size == 4) {
                            nat4 val;
                            bins = unpack4((char*)&val, bins);
                            console::output(format_u, val);
                        } else {
                            nat8 val;
                            bins = unpack8((char*)&val, bins);
                            console::output("%X%08x", nat8_high_part(val),
                                            nat8_low_part(val));
                        }
                        break;
                      case fld_real:
                        set_field_value(cld, field, bins, obj_size);
                        if (field->size == 4) {
                            real4 val;
                            bins = unpack4((char*)&val, bins);
                            console::output("%f", val);
                        } else {
                            real8 val;
                            bins = unpack8((char*)&val, bins);
                            console::output("%lf", val);
                        }
                        break;
                      case fld_string:
                        set_field_value(cld, field, bins, obj_size);
                        bins = dump_imu_string(bins);
                        break;
                      case fld_raw_binary:
                        bins = dump_raw_binary(bins);
                        break;
                    }
                    if (i != n-1) {
                        console::output(", ");
                    }
                }
                if (n > 1) {
                    console::output("}");
                }
           }
           field_no = field->next;
       } while (field_no != 0);
   }
   console::output("}");
}

void dump_class(dbs_class_descriptor* cld, int level,
                int field_no, int n_fields)
{
    const char indent[] = "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";
    if (n_fields == 0) {
        console::output("\n");
    } else {
        int next_field = field_no + n_fields;
        level += 1;
        do {
            dbs_field_descriptor* field = &cld->fields[field_no];
            console::output("%s", indent + sizeof(indent) - level);
            switch (field->type) {
              case fld_structure:
                console::output("struct {\n");
                dump_class(cld, level, field_no+1,
                           field->next
                             ? field->next - field_no - 1
                             : next_field - field_no - 1);
                console::output("%s}", indent + sizeof(indent)-level);
                break;
              case fld_reference:
                console::output("ref");
                break;
              case fld_signed_integer:
                console::output("int%d", field->size);
                break;
              case fld_unsigned_integer:
                console::output("nat%d", field->size);
                break;
              case fld_real:
                console::output("real%d", field->size);
                break;
            }
            console::output(" %s", &cld->names[field->name]);
            if (field->n_items > 1) {
                console::output("[%d]", field->n_items);
            } else if (field->is_varying()) {
                console::output("[1]");
            }
            console::output(";\n");
            field_no = field->next;
        } while (field_no != 0);
    }
}

boolean database_browser::validate_object_id(stid_t sid, opid_t opid)
{
    if (sid >= n_storages || storage[sid] == NULL) {
        console::output("Storage %d is not in configuration file or "
                        "not available\n", sid);
        return false;
    }
    if (opid == 0) {
        console::output("NULL\n");
        return false;
    }
    if (opid == RAW_CPID) {
        console::output("ABSTRACT ROOT CLASS\n");
        return false;
    }
    return true;
}

void database_browser::dump_object(stid_t sid, opid_t opid)
{
    dbs_class_descriptor* cld;
    validate_object_id(sid, opid);

    if (opid <= MAX_CPID) {
        if ((cld = class_dict[sid][opid]) == NULL) {
            storage[sid]->get_class(opid, cls_buf);
            if (cls_buf.size() == 0) {
                console::output("Class %x:%x is not in the database\n",
                                 sid, opid);
                return;
            }
            cld = (dbs_class_descriptor*)&cls_buf;
            cld->unpack();
            class_dict[sid][opid] = cld = cld->clone();
        }
        console::output("class %s {\n", cld->name());
        dump_class(cld, 1, 0, cld->n_fields);
        console::output("}\n");
        return;
    }
    storage[sid]->load(opid, lof_none, obj_buf);
    cld = get_class_descriptor(sid, opid);
    if (cld == NULL) {
        return;
    }

    console::output("Object \"%s\" %x:%x\n", cld->name(), sid, opid);

    dbs_object_header* hdr = (dbs_object_header*)&obj_buf;
    size_t obj_size = hdr->get_size();
    char* refs = hdr->body();
    char* bins = refs
        + cld->get_number_of_references(obj_size)*sizeof(dbs_reference_t);

    dump_fields(cld, obj_size, 0, cld->n_fields, refs, bins);
    console::output("\n");
}

class find_frame {
 public :
   stid_t m_sid;
   opid_t m_opid;
   nat4   m_ref;

   find_frame(stid_t sid, opid_t opid) : m_sid(sid), m_opid(opid), m_ref(0) {};
   find_frame(const find_frame &that) :
     m_sid(that.m_sid),
     m_opid(that.m_opid),
     m_ref(that.m_ref)
   {}

   find_frame &operator =(const find_frame &that)  { 
     m_sid = that.m_sid; 
     m_opid = that.m_opid; 
     m_ref = that.m_ref; 
     return *this;
   }
};


void database_browser::find(char* iCmd)
{
    opid_to_find = 0;
    sid_to_find  = 0;
    find_count   = 0;

    // Initialize our search maps, so we know which objects we've searched.
    search_maps = new dnm_buffer[n_storages];
    for (int i = 0; i < n_storages; i++) {
        size_t size = k_max_objects_per_storage / 8;
        if(k_max_objects_per_storage % 8 > 0) size++;
        memset(search_maps[i].put(size), 0, size);
    }

    // Skip past the "find " command.
    iCmd += 4;
    while(isspace(*iCmd)) {
        iCmd++;
    }

    // Read the object's ID from the command line.
    if((sscanf(iCmd, "%x:%x", (int*)&sid_to_find, (int*)&opid_to_find) < 2)
    && (sscanf(iCmd, "%x", (int*)&opid_to_find) != 1)) {
        console::output("Invalid object ID in `find'; type `help' for more "
            "information.\n");
        return;
    }
    if(!validate_object_id(sid_to_find, opid_to_find)) {
        return;
    }
    if(opid_to_find <= MAX_CPID) {
        console::output("Finding class descriptions not supported yet.\n");
        return;
    }

    // Start our mongo recursive find, starting with root.
    if(!find_path_to_object()) {
      console::output("Could not find a path from root to %x:%x\n", sid_to_find,
          opid_to_find);
    }
}

boolean database_browser::find_path_to_object(void)
{
    dbs_class_descriptor* cld;
    boolean               found         = false;
    nat4                  load_count    = 0;
    nat4                  scanned_count = 0;
    dnm_stack<find_frame> stack;

    stack.push(find_frame(0, ROOT_OPID));
    while (!stack.is_empty()) {
        find_frame inner    = stack.pop();
        int        num_refs = 0;
        opid_t     opid     = inner.m_opid;
        stid_t     sid      = inner.m_sid;

        // If we've found the object we're looking for, we're done!
        if ((sid_to_find == sid) && (opid_to_find == opid)) {
            stack.push(find_frame(sid, opid));
            break;
        }

        // Load the object from the database.
        storage[sid]->load(opid, lof_auto, obj_buf);
        if (++load_count % 100000 == 0) {
            console::output("Loaded %lu objects (%lu scanned)...\n",
                            load_count, scanned_count);
        }

        // Search this object's references.
        cld = get_class_descriptor(sid, opid);
        if (cld == NULL) {
            console::output("Warning: %x:%x has no class.\n", sid, opid);
            continue;
        }
        dbs_object_header* hdr = (dbs_object_header*)&obj_buf;
        size_t obj_size = hdr->get_size();
        char* refs = hdr->body();
        num_refs = cld->get_number_of_references(obj_size);

        storage[sid]->forget_object(opid);

        for (int i = 0; i < (int)inner.m_ref; i++) {
            opid_t skip_opid = 0;
            stid_t skip_sid  = 0;
            refs = unpackref(skip_sid, skip_opid, refs);
        }

        opid_t child_opid = 0;
        stid_t child_sid  = 0;

        while ((child_opid == 0) && ((int)inner.m_ref++ < num_refs)) {
            refs = unpackref(child_sid, child_opid, refs);
            if ((child_sid == 0) && (child_opid == 0)) {
                ;
            } else if (!validate_object_id(child_sid, child_opid)) {
                console::output("Bad reference %x:%x found in %x:%x\n",
                                child_sid, child_opid, sid, opid);
                child_opid = 0;
            } else {
                // Don't re-search an object if we've already run across it.
                int   bit = 1 << (child_opid % 8);
                char *byt = search_maps[child_sid].getPointerAt(child_opid / 8);
                if (*byt & bit) {
                    child_opid = 0;
                } else {
                    scanned_count++;
                    *byt |= bit;
                }
            }
        }
        if (child_opid != 0) {
            stack.push(inner);
            stack.push(find_frame(child_sid, child_opid));
        }
    }

    while (!stack.is_empty()) {
        find_frame f = stack.pop();
        storage[f.m_sid]->load(f.m_opid, lof_auto, obj_buf);
        cld = get_class_descriptor(f.m_sid, f.m_opid);
        storage[f.m_sid]->forget_object(f.m_opid);
        console::output(" %x:%x (%s)\n", f.m_sid, f.m_opid, cld->name());
        found = true;
    }

    return found;
}

dbs_class_descriptor* database_browser::get_class_descriptor(stid_t sid,
 opid_t opid)
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
    if ((cld = class_dict[sid][cpid]) == NULL) {
        storage[sid]->get_class(cpid, cls_buf);
        assert(cls_buf.size() != 0);
        cld = (dbs_class_descriptor*)&cls_buf;
        cld->unpack();
        class_dict[sid][cpid] = cld = cld->clone();
    }
    return cld;
}

dbs_field_descriptor* database_browser::get_field_descriptor(stid_t sid,
 opid_t opid, const char* iFieldName)
{
    dbs_class_descriptor* cld;
    dbs_field_descriptor* field;
    nat4                  fieldIndex = 0;
    const char*           fieldName;
    boolean               found      = false;

    cld = get_class_descriptor(sid, opid);
    if (cld == NULL) {
        return NULL;
    }
    while (!found && (fieldIndex <= cld->n_fields)) {
        field = &cld->fields[fieldIndex++];
        fieldName = &cld->names[field->name];
        found = (strncmp(fieldName, iFieldName, strlen(fieldName)) == 0);
    }
    if(!found)
        field = NULL;

    return field;
}

boolean database_browser::set_field_value(dbs_class_descriptor* iClassDesc,
 dbs_field_descriptor* iFieldDesc, char* oField, int obj_size)
{
    opid_t opid = 0;
    stid_t sid  = 0;

    if(!field_to_set || !value_to_set) return false;
    const char* fieldName = &iClassDesc->names[iFieldDesc->name];
    if(strcmp(fieldName, field_to_set) != 0)
        return false;

    if(field_to_set_index < 0) return false;
    if(field_to_set_index > 0) {
        field_to_set_index--;
        return false;
    }
    field_to_set_index--;

    switch (iFieldDesc->type) {
      case fld_structure:
        set_error = "modifying a structure is not supported";
        break;
      case fld_reference:
        if((sscanf(value_to_set, "%x:%x", (int*)&sid, (int*)&opid) < 2)
        && (sscanf(value_to_set, "%x", (int*)&opid) != 1)) {
            set_error = "invalid reference value";
        }
        packref(oField, sid, opid);
        break;
      case fld_signed_integer:
        if ((iFieldDesc->size == 1) && (*value_to_set != '{')) {
            int n = iFieldDesc->is_varying()
                ? iClassDesc->get_varying_length(obj_size) :
                iFieldDesc->n_items;
            strncpy(oField, value_to_set, n);
            oField[n-1] = 0;
        } else if (iFieldDesc->size == 1) {
            *((int1*)oField) = (int1) atol(value_to_set);
        } else if (iFieldDesc->size == 2) {
            int2 value = (int2)atol(value_to_set);
            pack2(oField, (char*)&value);
        } else if (iFieldDesc->size == 4) {
            int4 value = atol(value_to_set);
            pack4(oField, (char*)&value);
        } else {
            char fmtStr[80];
            int8 value;

            sprintf(fmtStr, "%s%s%s", "%", INT8_FORMAT, "d");
            sscanf(value_to_set, fmtStr, &value);
            pack8(oField, (char*)&value);
        }
        break;
      case fld_unsigned_integer:
        if (iFieldDesc->size == 1) {
            *((nat1*)oField) = (nat1) atol(value_to_set);
        } else if (iFieldDesc->size == 2) {
            nat2 value = (int2)atol(value_to_set);
            pack2(oField, (char*)&value);
        } else if (iFieldDesc->size == 4) {
            nat4 value;
            sscanf(value_to_set, "%lu", (long unsigned*)&value);
            pack4(oField, (char*)&value);
        } else {
            char fmtStr[80];
            int8 value;

            sprintf(fmtStr, "%s%s%s", "%", INT8_FORMAT, "u");
            sscanf(value_to_set, fmtStr, &value);
            pack8(oField, (char*)&value);
        }
        break;
      case fld_real:
        if (iFieldDesc->size == 4) {
            float value;
            sscanf(value_to_set, "%f", &value);
            pack4(oField, (char*)&value);
        } else {
            double value;
            sscanf(value_to_set, "%lf", &value);
            pack8(oField, (char*)&value);
        }
        break;
      case fld_string:
        set_error = "modifying a string is not supported";
        break;
    }
    return true;
}

void database_browser::set(char* iCmd)
{
    opid_t                opid      = 0;
    stid_t                sid       = 0;
    dnm_buffer            transBuf;
    trid_t                tid       = 0;

    // Validate format.
    if(!strstr(iCmd, ".") || !strstr(iCmd, ":") || !strstr(iCmd, "=")) {
        console::output("Invalid `set' format; type `help' for details.\n");
        return;
    }

    // Skip past the "set " command.
    iCmd += 3;
    while(isspace(*iCmd)) {
        iCmd++;
    }

    // Read the object's ID from the command line.
    if((sscanf(iCmd, "%x:%x", (int*)&sid, (int*)&opid) < 2)
    && (sscanf(iCmd, "%x", (int*)&opid) != 1)) {
        console::output("Invalid object ID in `set'; type `help' for more "
            "information.\n");
        return;
    }
    if(!validate_object_id(sid, opid)) {
        return;
    }
    if(opid <= MAX_CPID) {
        console::output("Cannot modify a class description.\n");
        return;
    }

    // Load the object from the database.
    storage[sid]->lock(opid, lck_exclusive, 0);
    storage[sid]->load(opid, lof_auto, obj_buf);

    // Find the name & value of the field to be set in the command line.
    set_error    = NULL;
    iCmd         = strstr(iCmd, ".");
    field_to_set = ++iCmd;

    field_to_set_index = 0;
    if(strstr(iCmd, "[")) {
        iCmd = strstr(iCmd, "[");
        field_to_set_index = atol(iCmd+1);
        *iCmd = 0;
        iCmd++;
    }
    iCmd         = strstr(iCmd, "=");
    *iCmd        = 0;
    value_to_set = ++iCmd;
    iCmd         = strstr(iCmd, "\n");
    *iCmd        = 0;

    // Set the object's field to the specified value.
    dump_object(sid, opid);
    field_to_set = value_to_set = NULL;

    // Display the message describing an error, if one occurred.
    if(set_error)
        console::output("--> Set failed: %s.\n", set_error);
    set_error = NULL;

    // Store the updated object back in the database.
    storage[sid]->begin_transaction(transBuf);
    dbs_object_header* hdr;
    hdr = (dbs_object_header*)transBuf.append(&obj_buf, obj_buf.size());
    hdr->set_flags(tof_update);
    storage[sid]->commit_coordinator_transaction(1, &sid, transBuf, tid);
    storage[sid]->unlock(opid, lck_none);
}

void database_browser::lock(char* iCmd, bool iLock)
{
    opid_t                opid      = 0;
    stid_t                sid       = 0;

    // Validate format.
    if(!strstr(iCmd, ":")) {
        console::output("Invalid `lock' format; type `help' for details.\n");
        return;
    }

    // Skip past the "lock " command.
    while(!isspace(*iCmd)) {
        iCmd++;
    }
    while(isspace(*iCmd)) {
        iCmd++;
    }

    // Read the object's ID from the command line.
    if((sscanf(iCmd, "%x:%x", (int*)&sid, (int*)&opid) < 2)
    && (sscanf(iCmd, "%x", (int*)&opid) != 1)) {
        console::output("Invalid object ID in `lock'; type `help' for more "
            "information.\n");
        return;
    }
    if(!validate_object_id(sid, opid)) {
        return;
    }
    if(opid <= MAX_CPID) {
        console::output("Cannot lock a class description.\n");
        return;
    }

    // Lock the database object.
    if(iLock)
        storage[sid]->lock(opid, lck_exclusive, 0);
    else
        storage[sid]->unlock(opid, lck_none);
}

void database_browser::dialogue()
{
    char buf[MAX_CFG_FILE_LINE_SIZE];

    while (True) {
        int sid = 0;
        int oid = ROOT_OPID;
        console::output(">> ");
        if (console::input(buf, sizeof buf)) {
            char* cmd = buf;
            while (isspace(*(nat1*)cmd)) cmd += 1;
            if (*cmd == '\0') {
                continue;
            }
            if (strincmp(cmd, ".hex", 4) == 0) {
                format_i = format_u = "%#x";
            } else if (strincmp(cmd, ".oct", 4) == 0) {
                format_i = format_u = "%#o";
            } else if (strincmp(cmd, ".dec", 4) == 0) {
                format_i = "%d";
                format_u = "%u";
            } else if (strincmp(cmd, "find ", 5) == 0) {
                find(cmd);
            } else if (strincmp(cmd, "set ", 4) == 0) {
                set(cmd);
            } else if (strincmp(cmd, "lock ", 5) == 0) {
                lock(cmd, true);
            } else if (strincmp(cmd, "unlock ", 7) == 0) {
                lock(cmd, false);
            } else if (strincmp(cmd, "exit", 4) == 0) {
                break;
            } else if ((strchr(cmd, ':') == NULL
                       && sscanf(cmd, "%x", &oid) == 1)
                       || sscanf(cmd, "%x:%x", &sid, &oid) >= 1)
            {
                dump_object(sid, oid);
            } else {
                console::output(
"Commands:\n"
"  .hex                     - output integer in hexademical radix\n"
"  .dec                     - output integer in decimal radix\n"
"  .oct                     - output integer in octal radix\n"
"  exit                     - exit browser\n"
"  <storage-id>:<object-id> - dump object from specified storage\n"
"  <object-id>              - dump object from storage 0\n"
"  <storage-id>:            - dump root object of specified storage\n"
"  find <storage-id>:<object-id>\n"
"                           - find the 1st path from root to an object\n"
"                             (helpful when the garbage collector doesn't\n"
"                             free an object and you don't know why)\n"
"  set <storage-id>:<object-id>.<field>=<value>\n"
"                           - modify object from specified storage\n"
"  set <storage-id>:<object-id>.<field>[<index>]=<value>\n"
"                           - modify an indexed member of an object from\n"
"                             specified storage\n"
"  lock <storage-id:object-id>\n"
"                           - exclusively lock object\n"
"  unlock <storage-id:object-id>\n"
"                           - unlock an exclusively locked object\n"
                );
           }
        } else {
           break;
        }
    }
}

int main(int argc, char* argv[])
{
    setlocale(LC_ALL, "");
    if (argc < 2) {
        console::output("GOODS database browser\n"
                        "Usage: browser <database name> [<login> <password>]\n");
        return EXIT_FAILURE;
    }
    task::initialize(task::huge_stack);
    database_browser db;
    char const* database_name = argv[1];
    char const* login = argc > 2 ? argv[2] : NULL;
    char const* password = argc > 3 ? argv[3] : NULL;
    if (db.open(database_name, login, password)) {
        db.dialogue();
        db.close();
        console::output("Browser terminated\n");
        return EXIT_SUCCESS;
    } else {
        console::output("Database not found\n");
        return EXIT_FAILURE;
    }
}
