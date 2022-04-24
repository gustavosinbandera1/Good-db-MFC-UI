// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< CLASSMGR.CXX >--------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:     18-Jan-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 24-May-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Manager of class disctionary
//-------------------------------------------------------------------*--------*

#include "server.h"

BEGIN_GOODS_NAMESPACE

#define EXTERNAL_BLOB_CLASS_NAME "ExternalBlob"

class_manager::~class_manager() {}


inline unsigned dbs_class_manager::hash_function(char const* name)
{
    return string_hash_function(name) % CLASS_DESCRIPTOR_HASH_TABLE_SIZE;
}


void dbs_class_manager::link_node(dbs_descriptor_node* node)
{
    dbs_descriptor_node** chain =
        &hash_table[hash_function(node->desc->name())];
    node->collision_chain = *chain;
    *chain = node;
}

void dbs_class_manager::unlink_node(dbs_descriptor_node* node)
{
    dbs_descriptor_node** chain =
        &hash_table[hash_function(node->desc->name())];

    while (*chain != node) {
        chain = &(*chain)->collision_chain;
    }
    *chain = node->collision_chain;
}


dbs_class_descriptor* dbs_class_manager::get_and_lock_class
(cpid_t cpid, client_process* client)
{
    cs.enter(); // will be unlocked by "unlock_class"
    dbs_descriptor_node* np = &class_dictionary[cpid];
    if (np->id < client->id) {
        np->id = client->id;
    }
    if (np->desc == NULL) {
        cs.leave();
        return NULL;
    }
    return np->desc;
}

void dbs_class_manager::unlock_class(cpid_t)
{
    cs.leave();
}

int dbs_class_manager::get_number_of_references(cpid_t cpid, size_t size)
{
    if (cpid == RAW_CPID) {
        return 0;
    } else {
        cs.enter();
        dbs_descriptor_node* np = &class_dictionary[cpid];
        int n_refs = np->desc
            ? (int)np->desc->get_number_of_references(size) : -1;
        cs.leave();
        return n_refs;
    }
    return -1;
}

char* dbs_class_manager::get_class_name(cpid_t cpid, char* buf, size_t buf_size)
{
    if (cpid == RAW_CPID) {
        return (char*)"<RAW>";
    } else {
        cs.enter();
        dbs_descriptor_node* np = &class_dictionary[cpid];
        strncpy(buf, np->desc ? np->desc->name() : "???", buf_size-1);
        buf[buf_size-1] = '\0';
        cs.leave();
        return buf;
    }
    return NULL;
}

void dbs_class_manager::store_class(cpid_t cpid,
                                    dbs_class_descriptor* desc,
                                    size_t desc_size,
                                    client_process* proc)
{
    dnm_buffer buf;
    dbs_transaction_header* trans = (dbs_transaction_header*)
        buf.put(sizeof(dbs_transaction_header)
                + sizeof(dbs_transaction_object_header) + desc_size);

    trans->tid = 0;
    trans->coordinator = server->id;
    trans->size = nat4(desc_size + sizeof(dbs_transaction_object_header));
    dbs_object_header* hdr = trans->body();

    hdr->set_cpid(cpid);
    hdr->set_opid(cpid);
    hdr->set_size(desc_size);
    hdr->set_flags(tof_update|tof_change_metadata);
    dbs_class_descriptor* trans_desc = (dbs_class_descriptor*)hdr->body();
    memcpy(trans_desc, desc, desc_size);
    trans_desc->pack();

    server->trans_mgr->do_transaction(1, trans, proc);
}

cpid_t dbs_class_manager::put_class(dbs_class_descriptor* desc,
                                    client_process* client)
{
    cs.enter();

    dbs_descriptor_node *np;
    dbs_descriptor_node **chain = &hash_table[hash_function(desc->name())];
    size_t desc_size = desc->get_size();
    int cpid;

    for (np = *chain; np != NULL; np = np->collision_chain) {
        if (desc_size == np->desc->get_size()
            && memcmp(desc, np->desc, desc_size) == 0)
        {
            if (np->id < client->id) {
                np->id = client->id;
            }
            cs.leave();
            return np - class_dictionary;
        }
    }
    for (cpid = MIN_CPID;
         cpid <= MAX_CPID && class_dictionary[cpid].desc != NULL;
         cpid += 1);

    assert(cpid <= MAX_CPID); // space of class identifiers is not exhausted

    np = &class_dictionary[cpid];
    np->desc = desc->clone();
    np->id = client->id;
    np->collision_chain = *chain;
    *chain = np;
    n_classes += 1;
    if (cpid > max_cpid) { 
        max_cpid = cpid;
    }
    if (strcmp(desc->name(), EXTERNAL_BLOB_CLASS_NAME) == 0) { 
        ext_blob_cpid = cpid;
    }

    cs.leave();

    store_class(cpid, desc, desc_size, client);
    return cpid;
}
    

void dbs_class_manager::modify_class(cpid_t cpid,
                                     dbs_class_descriptor* desc,
                                     client_process* client)
{
    cs.enter();

    dbs_descriptor_node *np = &class_dictionary[cpid];
    size_t desc_size = desc->get_size();

    unlink_node(np);
    delete np->desc;
    np->desc = desc->clone();
    link_node(np);

    if (np->id < client->id) {
        np->id = client->id;
    }
    cs.leave();

    store_class(cpid, desc, desc_size, client);
}

boolean dbs_class_manager::rename_class(char const* original_name,
                                        char const* new_name)
{
    cs.enter();
    dbs_descriptor_node *np;
    dbs_descriptor_node** chain = &hash_table[hash_function(original_name)];

    while ((np = *chain) != NULL) {
        if (strcmp(np->desc->name(), original_name) == 0) {
            *chain = np->collision_chain;
            int delta = int(strlen(new_name) - strlen(original_name));
            size_t new_size = np->desc->get_size() + delta;
            dbs_class_descriptor* new_desc =
                new (new_size) dbs_class_descriptor;
            new_desc->total_names_size += delta;
            memcpy(new_desc, np->desc, new_size - np->desc->total_names_size);
            int n_fields = np->desc->n_fields;
            int offs = n_fields*sizeof(dbs_field_descriptor);
            strcpy(&new_desc->names[offs], new_name);
            offs += int(strlen(new_name) + 1);
            for (int i = 0; i < n_fields; i++) {
                strcpy(&new_desc->names[offs],
                       &np->desc->names[np->desc->fields[i].name]);
                new_desc->fields[i].name = offs;
                offs += int(strlen(&new_desc->names[offs]) + 1);
            }
            delete np->desc;
            np->desc = new_desc;
            link_node(np);
            cs.leave();
            store_class(np - class_dictionary, new_desc, new_size, NULL);
            return True;
        }
        chain = &np->collision_chain;
    }
    cs.leave();
    return False;
}


boolean dbs_class_manager::rename_class_component(char const* class_name,
                                                  char* component_path,
                                                  char const* new_name)
{
    cs.enter();
    dbs_descriptor_node* np = hash_table[hash_function(class_name)];

    while (np != NULL) {
        if (strcmp(np->desc->name(), class_name) == 0) {
            char* p = strchr(component_path, '.');
            if (p != NULL) {
                *p++ = '\0';
            }
            dbs_class_descriptor* desc = np->desc;
            int i = 0;
            int n_fields = desc->n_fields;

          next_level:
            if (n_fields != 0) {
                int last = i + n_fields;
                do {
                    if (strcmp(&desc->names[desc->fields[i].name],
                               component_path) == 0)
                    {
                        if (p == NULL) {
                            // change this field
                            size_t new_size = desc->get_size()
                                - strlen(component_path) + strlen(new_name);
                            dbs_class_descriptor* new_desc =
                                new (new_size) dbs_class_descriptor;

                            memcpy(new_desc, desc, desc->get_size() -
                                   desc->total_names_size+strlen(class_name)+1);
                            n_fields = desc->n_fields;
                            int offs = int(n_fields*sizeof(dbs_field_descriptor) + strlen(class_name) + 1);
                            for (int j = 0; j < n_fields; j++) {
                                if (i != j) {
                                    strcpy(&new_desc->names[offs],
                                           &desc->names[desc->fields[j].name]);
                                } else {
                                    strcpy(&new_desc->names[offs], new_name);
                                }
                                new_desc->fields[j].name = offs;
                                offs += int(strlen(&new_desc->names[offs]) + 1);
                            }
                            new_desc->total_names_size =
                                offs - n_fields*sizeof(dbs_field_descriptor);
                            delete np->desc;
                            np->desc = new_desc;
                            cs.leave();
                            store_class(np - class_dictionary,
                                        new_desc, new_size, NULL);
                            return True;
                        } else {
                            component_path = p;
                            p = strchr(component_path, '.');
                            if (p != NULL) {
                                *p++ = '\0';
                            }
                            i += 1;
                            n_fields = desc->fields[i].next
                                ? desc->fields[i].next - i - 1: last - i - 1;
                            goto next_level;
                        }
                    }
                } while ((i = desc->fields[i].next) != 0);
            }
            break;
        }
        np = np->collision_chain;
    }
    cs.leave();
    return False;
} 

void dbs_class_manager::remove(cpid_t cpid)
{
    unsigned oldest_client_id = server->get_oldest_client_id();
    cs.enter();
    dbs_descriptor_node* node = &class_dictionary[cpid];
    if (node->desc != NULL && (oldest_client_id == 0 || node->id < oldest_client_id)) {
        SERVER_TRACE_MSG((msg_important, "Remove class '%s'\n", node->desc->name()));
        unlink_node(node);
        delete node->desc;
        node->desc = NULL;
        n_classes -= 1;
        cs.leave();
        server->mem_mgr->dealloc(cpid);
    } else {
        cs.leave();
    }
}

boolean dbs_class_manager::open(dbs_server* server)
{
    this->server = server;
    memset(class_dictionary, 0, sizeof(dbs_descriptor_node)*(MAX_CPID+1));
    memset(hash_table, 0, sizeof hash_table);
    n_classes = 0;
    max_cpid = 0;
    ext_blob_cpid = 0;

    for (int i = MIN_CPID; i <= MAX_CPID; i++) {
        size_t size = server->mem_mgr->get_size(i);
        if (size != 0) {
            max_cpid = i;
            dbs_class_descriptor* desc = new (size) dbs_class_descriptor;
            server->pool_mgr->read(server->mem_mgr->get_pos(i), desc, size);

            class_dictionary[i].desc = desc->unpack();
            if (strcmp(desc->name(), EXTERNAL_BLOB_CLASS_NAME) == 0) { 
                ext_blob_cpid = i;
            }

            n_classes += 1;
            link_node(&class_dictionary[i]);
        }
    }
    opened = True;
    return True;
}

boolean dbs_class_manager::is_external_blob(cpid_t cpid) 
{
    assert(ext_blob_cpid != MAX_CPID);
    return cpid == ext_blob_cpid;
}

void dbs_class_manager::close()
{
    cs.enter();
    if (opened) {
        for (int i = MIN_CPID; i <= MAX_CPID; i++) {
            delete class_dictionary[i].desc;
        }
        opened = False;
    }
    cs.leave();
}

int dbs_class_manager::get_number_of_classes()
{
    return n_classes;
}

cpid_t  dbs_class_manager::get_max_cpid() 
{ 
    return max_cpid;
}

void dbs_class_manager::dump(char*)
{
    console::output("Number of classes in storage: %d\n", n_classes);
}

void dbs_class_manager::initialize() {}

void dbs_class_manager::shutdown() {}

dbs_class_manager::dbs_class_manager()
{
    opened = False;
    server = NULL;
    ext_blob_cpid = MAX_CPID;
    class_dictionary = new dbs_descriptor_node[MAX_CPID+1];
}

dbs_class_manager::~dbs_class_manager()
{
    delete[] class_dictionary;
}


END_GOODS_NAMESPACE
