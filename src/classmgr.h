// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< CLASSMGR.H >----------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:     18-Jan-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 22-May-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Server:
// Manager of class disctionary
//-------------------------------------------------------------------*--------*

#ifndef __CLASSMGR_H__
#define __CLASSMGR_H__

#include "goodsdlx.h"

BEGIN_GOODS_NAMESPACE

class dbs_server;

//
// Abstract class manager
//

class GOODS_DLL_EXPORT class_manager { 
  public:
    //
    // Get class descriptor using class persistent indentifier
    //
    virtual dbs_class_descriptor* get_and_lock_class(cpid_t cpid, 
                                                     client_process* client)=0;
    virtual void unlock_class(cpid_t cpid) = 0;

    //
    // Calculate number of references in object with given size
    // of specified class. This method returns -1 if class was not found.
    //
    virtual int  get_number_of_references(cpid_t cpid, size_t size) = 0;
    
    //
    // Get name of the specified class.
    //
    virtual char* get_class_name(cpid_t cpid, char* buf, size_t buf_size) = 0;
    

    //
    // Register new class in store dicrtionary
    //
    virtual cpid_t put_class(dbs_class_descriptor* desc, 
                             client_process* client) = 0;

    //
    // Modify definition of existed class 
    //
    virtual void modify_class(cpid_t cpid, 
                              dbs_class_descriptor* desc, 
                              client_process* client) = 0;

    //
    // Rename class
    //
    virtual boolean rename_class(char const* original_name, 
                                 char const* new_name) = 0; 

    //
    // Rename class component
    //
    virtual boolean rename_class_component(char const* class_name,
                                           char*       original_component_path,
                                           char const* new_component_name) = 0; 
    //
    // Remove class descriptors with no instances
    //
    virtual void remove(cpid_t cpid) = 0; 


    //
    // Get number of classes in repository
    //
    virtual int get_number_of_classes() = 0;

    //
    // Get maximal class ID
    //
    virtual cpid_t get_max_cpid() = 0;


    virtual boolean is_external_blob(cpid_t cpid) = 0;

    virtual void dump(char* what) = 0;
    
    virtual boolean open(dbs_server* server) = 0;
    virtual void initialize() = 0;
    virtual void shutdown() = 0;    
    virtual void close() = 0;

    virtual ~class_manager();
}; 


//
// Implementation of class disctionary manager
//
struct dbs_descriptor_node {
    unsigned                id;  // id of youngest client accessed this class
    dbs_class_descriptor*   desc; 
    dbs_descriptor_node*    collision_chain; 
}; 

#define CLASS_DESCRIPTOR_HASH_TABLE_SIZE 1023

class GOODS_DLL_EXPORT dbs_class_manager : public class_manager { 
  protected: 
    mutex                cs;
    int                  n_classes; // number of used classes
    cpid_t               max_cpid;
    cpid_t               ext_blob_cpid;
    dbs_server*          server; 
    boolean              opened; 
    
    dbs_descriptor_node* class_dictionary;
    dbs_descriptor_node* hash_table[CLASS_DESCRIPTOR_HASH_TABLE_SIZE];

    static unsigned hash_function(char const* name);

    void link_node(dbs_descriptor_node* node);
    void unlink_node(dbs_descriptor_node* node); 

    void store_class(cpid_t cpid, dbs_class_descriptor* desc,
                     size_t desc_size, client_process* client);

  public:
    virtual dbs_class_descriptor* get_and_lock_class(cpid_t cpid, 
                                                     client_process* client);

    virtual void unlock_class(cpid_t cpid);

    virtual int  get_number_of_references(cpid_t cpid, size_t size);

    virtual char* get_class_name(cpid_t cpid, char* buf, size_t buf_size);

    virtual cpid_t put_class(dbs_class_descriptor* desc, 
                             client_process* client);

    virtual void   modify_class(cpid_t cpid, 
                                dbs_class_descriptor* desc, 
                                client_process* client);

    virtual boolean rename_class(char const* original_name, 
                                 char const* new_name); 

    virtual boolean rename_class_component(char const* class_name,
                                           char*       original_component_path,
                                           char const* new_component_name); 
    virtual void   remove(cpid_t cpid); 

    virtual int    get_number_of_classes();

    virtual cpid_t get_max_cpid();

    virtual boolean is_external_blob(cpid_t cpid);
        
    virtual void   dump(char* what);

    virtual boolean open(dbs_server* server);
    virtual void   initialize();
    virtual void   shutdown();
    virtual void   close();

    dbs_class_manager();
    virtual~dbs_class_manager();
}; 
END_GOODS_NAMESPACE

#endif
