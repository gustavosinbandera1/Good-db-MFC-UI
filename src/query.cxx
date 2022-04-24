// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< QUERY.CXX >----------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET   *     ?  *
// (Generic Object Oriented Database System)                        *   /\|  *
//                                                                  *  /  \  *
//                          Created:     28-May-2012  K.A. Knizhnik * / [] \ *
//                          Last update: 28-May-2012  K.A. Knizhnik * GARRET *
//------------------------------------------------------------------*--------*
// Query engine
//------------------------------------------------------------------*--------*

#include "server.h"
#include "query.h"

BEGIN_GOODS_NAMESPACE

NullValue NullValue::null;

bool tstring_t::like(tstring_t const& patternString) const
{
    const wchar_t matchAnySubstring = '%';
    const wchar_t matchAnyOneChar = '_';
    const wchar_t escapeChar = '\\';

    if (chars == NULL) { 
        return false;
    }

    wchar_t const* str = chars;
    wchar_t const* pattern = patternString.getWideChars();
    wchar_t const* wildcard = NULL;
    wchar_t const* strpos = NULL;

    while (true) {
        if (*pattern == matchAnySubstring) {
            wildcard = ++pattern;
            strpos = str;
        } else if (*str == '\0') {
            return (*pattern == '\0');
        } else if (*pattern == escapeChar && pattern[1] == *str) {
            str += 1;
            pattern += 2;
        } else if (*pattern != escapeChar
                   && (*str == *pattern || *pattern == matchAnyOneChar))
        {
            str += 1;
            pattern += 1;
        } else if (wildcard) {
            str = ++strpos;
            pattern = wildcard;
        } else {
            return false;
        }
    }
}


bool Query::ContainsCondition::evaluateCond(Runtime* rt)
{
    Value* set = setExpr->evaluate(rt);
    if (set->isNull()) { 
        return false;
    }
    opid_t set_opid = set->toInt(rt);
    if (set_opid == 0) { 
        return false;
    }
    dbs_server* server = runtime.getServer();
    client_process* client = runtime.getClient();
    dnm_buffer mbr_buf;
    dbs_handle hnd;
    server->obj_mgr->load_object(set_opid, lof_bulk, client);
    server->mem_mgr->get_handle(set_opid, hnd);
    size_t size = hnd.get_size();
    cpid_t cpid = hnd.get_cpid();
    if (size == 0 || cpid == 0) { 
        server->obj_mgr->release_object(set_opid);
        return false;
    }              
    stid_t self_sid = server->id;
    int n_refs = server->class_mgr->get_number_of_references(cpid, size);
    if (n_refs < 3) { 
        server->obj_mgr->release_object(set_opid);
        return false;
    }
    mbr_buf.put(size);
    server->pool_mgr->read(hnd.get_pos(), &mbr_buf, size);
    server->obj_mgr->release_object(set_opid);

	objref_t next_opid;
    stid_t next_sid;
    unpackref(next_sid, next_opid, &mbr_buf);
    
    while (next_sid == self_sid && next_opid != 0) {
        server->obj_mgr->load_object(next_opid, lof_none, client);
        server->mem_mgr->get_handle(next_opid, hnd);
        size = hnd.get_size();
        cpid = hnd.get_cpid();
        assert(size != 0 && cpid != 0);
        n_refs = server->class_mgr->get_number_of_references(cpid, size);
        if (n_refs < 4) { 
            server->obj_mgr->release_object(next_opid);
            return false;
        }
        mbr_buf.put(size);
        server->pool_mgr->read(hnd.get_pos(), &mbr_buf, size);
        server->obj_mgr->release_object(next_opid);
 
        char* p = &mbr_buf;
        p = unpackref(next_sid, next_opid, p);
        p += OBJECT_REF_SIZE; // prev
        p += OBJECT_REF_SIZE; // owner

		objref_t obj_opid;
        stid_t obj_sid;
        unpackref(obj_sid, obj_opid, p); // obj
       
        if (obj_sid == self_sid) { 
            server->mem_mgr->get_handle(obj_opid, hnd);                
            server->obj_mgr->load_object(obj_opid, lof_none, client);
            size = hnd.get_size();
            cpid = hnd.get_cpid();
            mbr_buf.put(size);
            server->pool_mgr->read(hnd.get_pos(), &mbr_buf, size);
            server->obj_mgr->release_object(obj_opid);

            runtime.mark();
            runtime.unpackObject(&mbr_buf, size, cpid);
 
            bool matched = false;
            try { 
                matched = subquery->evaluateCond(&runtime);
            } catch (QueryServerException const&) {}
            
            runtime.reset();
            if (matched) { 
                return true;
            }
        }
    }
    return false;
}

Query::Compiler::Token Query::Compiler::scan()
{
    char* p = curr;
    int ch, i, n;
    while ((ch = (*p++ & 0xFF)) != '\0' && isspace(ch));
    curr = p;
    switch (ch) { 
      case '\0':
        return TKN_EOF;
      case '(':
        return TKN_LPAR;
      case ')':
        return TKN_RPAR;
      case '+':
        return TKN_PLUS;
      case '-':
        return TKN_MINUS;
      case '*':
        return TKN_MUL;
      case '/':
        return TKN_DIV;
      case '<':
        if (*p == '=') { 
            curr += 1;
            return TKN_LE;
        } else if (*p == '>') { 
            curr += 1;
            return TKN_NE;
        } else {
            return TKN_LT;
        }
      case '>':
        if (*p == '=') { 
            curr += 1;
            return TKN_GE;
        } else {
            return TKN_GT;
        }
      case '=':
        if (*p == '=') { 
            curr += 1;
        }
        return TKN_EQ;        
      case '!':
        if (*p == '=') { 
            curr += 1;
            return TKN_NE;
        }
        return TKN_NOT;
      case '&':
        if (*p == '&') { 
            curr += 1;
            return TKN_AND;
        }
        return TKN_BIT_AND;
      case '|':
        if (*p == '|') { 
            curr += 1;
            return TKN_OR;
        }
        return TKN_BIT_OR;
      case '^':
        return TKN_BIT_XOR;
      case '~':
        return TKN_BIT_NOT;
      case '\'':
      case '"':
        while (*p != '\0' && *p != ch) { 
            if (*p++ == '\\') { 
                p += 1;
            }
        }
        if (*p != ch) { 
            error(p);
        }
        *p = '\0';
        sconst = tstring_t(rt, curr, (rt->getQueryFlags() & qf_insensitive) != 0);
        *p++ = (char)ch;
        curr = p;
        return TKN_SCONST;
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        p -= 1;
        if (sscanf(p, "%" INT8_FORMAT "d%n", &iconst, &n) != 1) { 
            error(p);
        }
        ch = p[n];
        if (ch == '.' || ch == 'e' || ch == 'E') { 
            if (sscanf(p, "%lf%n", &rconst, &n) != 1) { 
                error(p);
            }
            curr = p + n;
            return TKN_RCONST;
        }
        curr = p + n;
        return TKN_ICONST;
      default:
        for (i = 0; ch == '_' || ch == '.' || isalnum(ch); i++) { 
            if (i == MAX_IDENT_LENGTH-1) { 
                error(curr-1);
            }
            ident[i] = ch;
            ch = *p++ & 0xFF;
        }
        if (i == 0) {
            error(curr-1);
        }
        ident[i] = '\0';
        curr = p-1;
        if (stricmp(ident, "and") == 0) { 
            return TKN_AND;
        } else if (stricmp(ident, "or") == 0) { 
            return TKN_OR;
        } else if (stricmp(ident, "not") == 0) { 
            return TKN_NOT;
        } else if (stricmp(ident, "like") == 0) { 
            return TKN_LIKE;
        } else if (stricmp(ident, "contains") == 0) { 
            return TKN_CONTAINS;
        } else { 
            return TKN_IDENT;
        }
    }
}
 
Query::Expression* Query::Compiler::addVariable()
{
    Runtime::Binding* binding = rt->addBinding(ident);
    char* p;
    // add bindings for all prefixes
    while ((p = strrchr(ident, '.')) != NULL) { 
        *p = '\0';
        rt->addBinding(ident, true);
    }    
    return new (rt) VarExpression(binding);
}

Query::Expression* Query::Compiler::term()
{
    char* p = curr;   
    Expression* expr = nullptr;
    switch (scan()) { 
      case TKN_LPAR:
        expr = disjunction();
        if (tkn != TKN_RPAR) { 
            error(p);
        }
        break;
      case TKN_NOT:
        return new (rt) NotCondition(term());   
      case TKN_BIT_NOT:
        return new (rt) BitNotExpression(term());   
      case TKN_MINUS:
        return new (rt) NegExpression(term());
      case TKN_PLUS:
        return term();
      case TKN_IDENT:
        expr = addVariable();
        break;
      case TKN_ICONST:
        expr = new (rt) LiteralExpression(new (rt) IntValue(iconst));
        break;
      case TKN_RCONST:
        expr = new (rt) LiteralExpression(new (rt) RealValue(rconst));
        break;
      case TKN_SCONST:
        expr = new (rt) LiteralExpression(new (rt) StringValue(sconst));
        break;
      default:
        error(p);
    }
    tkn = scan();
    return expr;
}
    
Query::Expression* Query::Compiler::addition()
{
    Expression* expr = multiplication();
    switch (tkn) {
    case TKN_PLUS:
        expr = new (rt) AddExpression(expr, addition());
        break;
    case TKN_MINUS:
        expr = new (rt) SubExpression(expr, addition());
        break;
    case TKN_BIT_OR:
        expr = new (rt) BitOrExpression(expr, addition());
        break;
    default:
        break;
    }
    return expr;
}        

Query::Expression* Query::Compiler::multiplication()
{
    Expression* expr = term();
    switch (tkn) {
    case TKN_MUL:
        expr = new (rt) MulExpression(expr, multiplication());
        break;
    case TKN_DIV:
        expr = new (rt) DivExpression(expr, multiplication());
        break;
    case TKN_BIT_AND:
        expr = new (rt) BitAndExpression(expr, multiplication());
        break;
    case TKN_BIT_XOR:
        expr = new (rt) BitXorExpression(expr, multiplication());
        break;
    default:
        break;
    }
    return expr;
}        

Query::Expression*  Query::Compiler::disjunction()
{
    Expression* cond = conjunction();
    if (tkn == TKN_OR) {
        cond = new (rt) OrCondition(cond, disjunction());
    }
    return cond;
}

Query::Expression*  Query::Compiler::conjunction()
{
    Expression* cond = comparison();
    if (tkn == TKN_AND) {
        cond = new (rt) AndCondition(cond, conjunction());
    }
    return cond;
}

Query::Expression*  Query::Compiler::comparison()
{
    Expression* expr = addition();
    if (tkn == TKN_EQ) { 
        return new (rt) EqCondition(expr, addition());
    } else if (tkn == TKN_NE) { 
        return new (rt) NeCondition(expr, addition());
    } else if (tkn == TKN_GT) { 
        return new (rt) GtCondition(expr, addition());
    } else if (tkn == TKN_GE) { 
        return new (rt) GeCondition(expr, addition());
    } else if (tkn == TKN_LT) { 
        return new (rt) LtCondition(expr, addition());
    } else if (tkn == TKN_LE) { 
        return new (rt) LeCondition(expr, addition());
    } else if (tkn == TKN_LIKE) { 
        return new (rt) LikeCondition(expr, addition());
    } else if (tkn == TKN_CONTAINS) { 
        ContainsCondition* contains = new (rt) ContainsCondition(expr, rt);
        Runtime* save = rt;
        rt = &contains->runtime;
        contains->subquery = addition();
        rt = save;
        expr = contains;
     } 
    return expr;
}    

Query::Expression* Query::Compiler::compile() 
{
    Expression* expr = disjunction();
    if (tkn != TKN_EOF) {
        error(curr);
    } else if (!expr->isCondition()) { 
        error(query);
    }
    return expr;
}

void Query::Compiler::error(char* p) 
{ 
    throw CompileException(p - query);
}


Runtime::Binding* Runtime::getBinding(char const* name)
{
    size_t hash = asci_string_hash_function(name);
    size_t chain = hash % RUNTIME_HASH_SIZE;
    for (Binding* b = hashTable[chain]; b != NULL; b = b->collision) { 
        if (b->hash == hash && strcmp(b->name, name) == 0) { 
            return b;
        }
    }
    return NULL;
}

Runtime::Binding* Runtime::addBinding(char const* name, bool indirect) 
{ 
    size_t hash = asci_string_hash_function(name);
    size_t chain = hash % RUNTIME_HASH_SIZE;
    Binding* b;

    for (b = hashTable[chain]; b != NULL; b = b->collision) { 
        if (b->hash == hash && strcmp(b->name, name) == 0) { 
            b->indirect |= indirect;
            if (!indirect && !b->direct) { 
                b->direct = true;
                b->next = bindings;
                bindings = b;
            }
            return b;
        }
    }
    b = (Binding*)allocate(sizeof(Binding) + strlen(name));
    b->collision = hashTable[chain];
    b->hash = hash;
    b->value = &NullValue::null;
    b->indirect = indirect;
    b->direct = !indirect;
    if (!indirect) { 
        b->next = bindings;
        bindings = b;
    }
    strcpy(b->name, name);
    hashTable[chain] = b;
    return b;
}

void Runtime::reset() 
{ 
    AllocBlock *block, *next;
    for (block = curr; block != markCurr; block = next) { 
        next = block->next;
        delete block;
    }
    curr = markCurr;
    used = markUsed;
    refs = NULL;
    for (Binding* b = bindings; b != NULL; b = b->next) { 
        b->value = &NullValue::null;
    }
}

void Runtime::unpackObject(char* body, size_t size, cpid_t cpid)
{
    char name[MAX_IDENT_LENGTH];
    *name = '\0'; 
    dbs_class_descriptor* desc = server->class_mgr->get_and_lock_class(cpid, client);
    char* refs = body;
    char* bins = refs + desc->get_number_of_references(size)*sizeof(dbs_reference_t);
    unpackStruct(refs, bins, size, desc, name, 0, false);
    server->class_mgr->unlock_class(cpid);
}

void Runtime::unpackObject(opid_t opid, char* name)
{
    dbs_handle hnd; 
    server->obj_mgr->load_object(opid, lof_none, client);
    server->mem_mgr->get_handle(opid, hnd);
    size_t size = hnd.get_size();
    cpid_t cpid = hnd.get_cpid();
    dbs_class_descriptor* desc = server->class_mgr->get_and_lock_class(cpid, client);
    RefObject* ref = (RefObject*)allocate(sizeof(RefObject) + size);
    ref->next = refs;
    refs = ref;
    ref->cpid = cpid;
    ref->size = size;
    ref->opid = opid;
    server->pool_mgr->read(hnd.get_pos(), ref->body, size); 
    server->obj_mgr->release_object(opid);
    char* refs = ref->body;
    char* bins = refs + desc->get_number_of_references(size)*sizeof(dbs_reference_t);
    unpackStruct(refs, bins, size, desc, name, 0, false);
    server->class_mgr->unlock_class(cpid);
}

// [MC] -- fix supporting multiple levels of inheritance
// TODO: Change implementation to check in existing class list
// We are assuming that base classes always begin with 'C' and member structure fields do not
static inline bool IsInheritedClassField(dbs_class_descriptor* desc, int no)
{
	if (no == 0)
	{
		return true;
	}

	dbs_field_descriptor* field = &desc->fields[no];
	const auto* field_name = &desc->names[field->name];

	return field_name[0] == 'C';
}

char* Runtime::unpackStruct(char* &refs, char* bins, size_t size, dbs_class_descriptor* desc, char* name, int fno, bool skip)
{
	const bool is_inherited_class = IsInheritedClassField(desc, fno);
    size_t prefix_len = strlen(name);
    do { 
        dbs_field_descriptor* field = &desc->fields[fno];
        int no = fno;
        fno = field->next;
        Binding* binding = NULL;
		// [MC] -- fix supporting multiple levels of inheritance
		if (!skip && (!is_inherited_class || field->type != fld_structure))
		{
            strcpy(name + prefix_len, &desc->names[field->name]);    
            binding = getBinding(name);
        }
        if (field->n_items == 0) { // varying part
            if (binding != NULL && field->type == fld_signed_integer && field->size == 1) { 
                binding->value = new (this) StringValue(tstring_t(this, bins, (queryFlags & qf_insensitive) != 0));
            }
        } else if (field->n_items == 1) { 
            switch (field->type) { 
            case fld_reference:
                if (binding != NULL) {
					nat2   sid;
					objref_t opid;
                    refs = unpackref(sid, opid, refs);
                    binding->value = new (this) IntValue(cons_int8(sid, opid));
                    if (sid == server->id && opid != 0 && binding->indirect) { 
                        strcat(name, ".");
                        unpackObject(opid, name);
                    }
                } else {
                    refs += OBJECT_REF_SIZE;
                }
                continue;
            case fld_structure:
                if (fno > no+1 || (fno == 0 && no+1 < (int)desc->n_fields)) {
					// [MC] -- fix supporting multiple levels of inheritance
					if (is_inherited_class)
					{ // first struct used to represent base class: let access it without class name prefix
                        bins = unpackStruct(refs, bins, size, desc, name, no+1, false);
                    } else { 
                        strcat(name, ".");
                        bins = unpackStruct(refs, bins, size, desc, name, no+1, binding == NULL);
                    }
                }
                continue;
            case fld_signed_integer:
                if (binding != NULL) {
                    int8 ival;
                    switch (field->size) { 
                    case 1:
                        ival = *(int1*)bins;
                        break;
                    case 2:
                        ival = (int2)unpack2(bins);
                        break;
                    case 4:
                        ival = (int4)unpack4(bins);
                        break;
                    default:
                        unpack8((char*)&ival, bins);
                    }
                    binding->value = new (this) IntValue(ival);
                } 
                bins += field->size;
                continue;
            case fld_unsigned_integer:
                if (binding != NULL) { 
                    int8 ival;
                    switch (field->size) { 
                    case 1:
                        ival = *(nat1*)bins;
                        break;
                    case 2:
                        ival = unpack2(bins);
                        break;
                    case 4:
                        ival = unpack4(bins);
                        break;
                    default:
                        unpack8((char*)&ival, bins);
                    }
                    binding->value = new (this) IntValue(ival);
                }
                bins += field->size;
                continue;
            case fld_real:
                if (binding != NULL) {
                    double d;
                    if (field->size == 4) { 
                        float f;
                        bins = unpack4((char*)&f, bins);
                        d = f;
                    } else { 
                        bins = unpack8((char*)&d, bins);
                    }
                    binding->value = new (this) RealValue(d);
                } else { 
                    bins += field->size;
                }
                continue;
            case fld_string:
            {
                tstring_t str;
                int size = unpack2(bins);
                bins += 2;
                if (binding != NULL) { 
                    if (size != 0xFFFF) { 
                        str.resize(this, size+1);
                        wchar_t* chars = const_cast<wchar_t*>(str.getWideChars());
                        for (int i = 0; i < size; i++) { 
                            chars[i] = unpack2(bins);
                            bins += 2;
                        }
                        chars[size] = 0;
                    }
                    if (queryFlags & qf_insensitive) { 
                        str.toLower();
                    }
                    binding->value = new (this) StringValue(str);
                } else if (size !=  0xFFFF) { 
                    bins += size*2;
                }
            }
			continue;
			// [MC] : used for uuid
			case fld_raw_binary: //support raw_binary_t as strings
				{
					tstring_t str;
					int size = unpack4(bins);
					bins += 4;
					if (binding != NULL) { 
						if (size) { 
							str.resize(this, size+1);
							wchar_t* chars = const_cast<wchar_t*>(str.getWideChars());
							for (int i = 0; i < size; i++) { 
								chars[i] = (char)*(bins + i);								
								if(chars[i] == 0) 
								{
									str.setLength(i);
									break;
								}
							}
							bins += size;
							chars[size] = 0;
						}
						if (queryFlags & qf_insensitive) { 
							str.toLower();
						}
						binding->value = new (this) StringValue(str);
					} else {
						bins += size;
					}
				}
			continue;
            }
        } else if (field->n_items > 1) { // ignore array
            int n = field->n_items;
            switch (field->type) { 
              case fld_string:
                do { 
                    int size = unpack2(bins);
                    bins += 2;
                    if (size != 0xFFFF) { 
                        bins += 2*size;
                    }
                } while (--n != 0);
                continue;
              case fld_reference:
                refs += sizeof(dbs_reference_t) * n;
                continue;
            case fld_signed_integer:
            case fld_unsigned_integer:
                //process strings that are expressed as character array
                if (binding != NULL && field->size <= 2) { 
                    tstring_t str;
                    wchar_t ch = 0;
                    if(field->size == 1) { 
                        ch = (char)*bins;
                    } else { 
                        ch = unpack2(bins);
                    }
                    if (ch) { //non empty
                        str.resize(this, n+1);
                        wchar_t* chars = const_cast<wchar_t*>(str.getWideChars());
                        for (int i = 0; i < n; i++) { 
                            if(field->size == 1) { 
                                chars[i] = (char)*(bins + i*field->size);
                            } else { 
                                chars[i] = unpack2(bins + i*field->size);
                            }
                            if(chars[i] == 0) {
                                str.setLength(i);
                                break;
                            }
                        }
                        if (queryFlags & qf_insensitive) { 
                            str.toLower();
                        }
                    }
                    binding->value = new (this) StringValue(str);
                }                
                bins += field->size * n;
                continue;
              case fld_structure:
                do {
                    bins = unpackStruct(refs, bins, size, desc, NULL, no+1, true);
                } while (--n != 0);
                continue;
              default:
                bins += field->size * n;
            }
        }
    } while (fno != 0);
    
    return bins;
}


END_GOODS_NAMESPACE
