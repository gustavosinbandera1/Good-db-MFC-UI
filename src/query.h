// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< QUERY.H >------------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET   *     ?  *
// (Generic Object Oriented Database System)                        *   /\|  *
//                                                                  *  /  \  *
//                          Created:     28-May-2012  K.A. Knizhnik * / [] \ *
//                          Last update: 28-May-2012  K.A. Knizhnik * GARRET *
//------------------------------------------------------------------*--------*
// Query engine
//------------------------------------------------------------------*--------*

#ifndef __QUERY_H__
#define __QUERY_H__

#include "stdinc.h"
#include "protocol.h"
#include "wstring.h"

BEGIN_GOODS_NAMESPACE

const size_t RUNTIME_ALLOC_BLOCK_SIZE = 64*1024;
const size_t RUNTIME_HASH_SIZE = 101;
const size_t MAX_IDENT_LENGTH = 256;
const size_t MAX_ERROR_MESSAGE_LENGTH = 256;

class dbs_server;
class client_process;

class QueryServerException
{
  public:
    virtual size_t getMessage(char* buf) const = 0;
};

class InvalidOperationException : public QueryServerException
{
  public:
    InvalidOperationException(char const* nm) : name(nm) {}

    virtual size_t getMessage(char* buf) const { 
        return sprintf(buf, "Operation %s is not supported", name);
    }
    
    char const* getName() const { 
        return name;
    }
    
  private:
    char const* name;
};
    
class ConversionException : public QueryServerException
{
  public:
    ConversionException(char const* val, char const* typ) : value(val), type(typ) {}

    virtual size_t getMessage(char* buf) const { 
        return sprintf(buf, "Failed to convert '%s' to %s", value, type);
    }
    
    char const* getValue() const { 
        return value;
    }

    char const* getType() const { 
        return type;
    }
    
  private:
    char const* value;
    char const* type;
};

class CompileException : public QueryServerException
{
  public:
    CompileException(size_t p) : pos(p) {}

    virtual size_t getMessage(char* buf) const { 
        return sprintf(buf, "Compile error at position %d", (int)pos);
    }
    
    size_t getPosition() const { 
        return pos;
    }
    
  private:
    size_t pos;
};
    
   
        
class Value;

class Runtime 
{
  public:
    struct RefObject { 
        RefObject* next;
        opid_t opid;
        cpid_t cpid;
        size_t size;
        char   body[1];
    };

    struct Binding { 
        Binding* collision;
        Binding* next;
        size_t   hash;
        Value*   value;
        bool     indirect;
        bool     direct;
        char     name[1];
    };

    dbs_server* getServer() const { 
        return server;
    } 
    client_process* getClient() const { 
        return client;
    }
  private:
    struct AllocBlock { 
        AllocBlock* next;
        char data[1];

        void* operator new(size_t fix_size, size_t var_size) { 
            return new char[fix_size + var_size];
        }
     
        void operator delete(void* ptr) { 
            delete[] (char*)ptr;
        }
   
        AllocBlock(AllocBlock* chain) { 
            next = chain;
        }
    };
    size_t used;
    AllocBlock* curr;
    AllocBlock* markCurr;
    size_t markUsed;
    Binding* bindings;
    RefObject* refs;
    dbs_server* server;
    client_process* client;
    int queryFlags;
    Binding* hashTable[RUNTIME_HASH_SIZE];

    char* unpackStruct(char* &refs, char* bins, size_t size, dbs_class_descriptor* desc, char* name, int fno, bool skip);
    void unpackObject(opid_t opid, char* name);
    void init() 
    {
        curr = NULL;        
        bindings = NULL;
        refs = NULL;
        used = RUNTIME_ALLOC_BLOCK_SIZE;
        memset(hashTable, 0, sizeof(hashTable));
    }

  public:
    Runtime(dbs_server* srv, client_process* cln, int flags) : server(srv), client(cln), queryFlags(flags)
    {
        init();
    }

    Runtime(Runtime const& other) : server(other.server), client(other.client), queryFlags(other.queryFlags)
    {
        init();
    }

    Binding* getBinding(char const* name);
    Binding* addBinding(char const* name, bool indirect = false); 
    RefObject* getReferencedObjects() { 
        return refs;
    }

    void mark() { 
        markCurr = curr;
        markUsed = used;
    }

    void reset();

    void* allocate(size_t size) {
        size = DOALIGN(size, 8);
        if (size + used > RUNTIME_ALLOC_BLOCK_SIZE) { 
            size_t blockSize = size;
            if (blockSize < RUNTIME_ALLOC_BLOCK_SIZE) { 
                blockSize = RUNTIME_ALLOC_BLOCK_SIZE;
            }
            curr = new (blockSize) AllocBlock(curr);
            used = 0;
        }
        char* data = &curr->data[used];
        used += size;
        return data;        
    }

    int getQueryFlags() const { 
        return queryFlags;
    }

    void unpackObject(char* body, size_t size, cpid_t cpid);
    
    ~Runtime() { 
        AllocBlock *block, *next;
        for (block = curr; block != NULL; block = next) { 
            next = block->next;
            delete block;
        }
    }
};

//
// Temporary wstring
//
class tstring_t : public wstring_t
{
  public:
    void toLower() { 
        wchar_t* p = chars;
        for (size_t i = 0, n = len; i < n; i++) { 
            p[i] = towlower(p[i]);
        }
    }

    tstring_t() {}

    tstring_t(Runtime* rt, char const* str, bool lowercase) {
        size_t n = mbstowcs(NULL, str, 0) + 1;
        chars = (wchar_t*)rt->allocate(n*sizeof(wchar_t));
        len = (int)mbstowcs(chars, str, n) + 1;
        assert((size_t)len == n);
        if (lowercase) {  
            toLower();
        }
    }

    tstring_t& operator=(tstring_t const& str) {
        chars = str.chars;
        len = str.len;
        return *this;
    }

    bool like(tstring_t const& pattern) const;

    tstring_t(tstring_t const& str) {
        chars = str.chars;
        len = str.len;
    }
    
    void resize(Runtime* rt, size_t size) { 
        chars = (wchar_t*)rt->allocate(size*sizeof(wchar_t));
        len = (nat4)size; 
    }

    void setLength(int val) {
        len = val;
    }

    tstring_t concat(Runtime* rt, wstring_t const& str) const { 
        tstring_t newStr;
        newStr.len = len + str.length();
        newStr.chars = (wchar_t*)rt->allocate(newStr.len*sizeof(wchar_t));
        memcpy(newStr.chars, chars, (len-1)*sizeof(wchar_t));
        memcpy(newStr.chars + len - 1, str.getWideChars(), (newStr.len-len+1)*sizeof(wchar_t));
        return newStr;
    }
        
    ~tstring_t() { 
        chars = NULL;
    }
};

class Value
{    
  public:
    void* operator new(size_t size, Runtime* rt) { 
        return rt->allocate(size);
    }
    void operator delete(void*) {}

    virtual bool isNull() { return false; }
    virtual Value* add(Runtime* rt, Value* opd) = 0;
    virtual Value* sub(Runtime* rt, Value* opd) = 0;
    virtual Value* mul(Runtime* rt, Value* opd) = 0;
    virtual Value* div(Runtime* rt, Value* opd) = 0;
    virtual Value* neg(Runtime* rt) = 0;
    virtual Value* bit_and(Runtime* rt, Value* opd) {
        throw InvalidOperationException("Value::bit_and");
    }
    virtual Value* bit_or(Runtime* rt, Value* opd) {
        throw InvalidOperationException("Value::bit_or");
    }
    virtual Value* bit_xor(Runtime* rt, Value* opd) {
        throw InvalidOperationException("Value::bit_xor");
    }
    virtual Value* bit_not(Runtime* rt) {
        throw InvalidOperationException("Value::bit_not");
    }
    virtual bool eq(Runtime* rt, Value* opd) = 0;
    virtual bool ne(Runtime* rt, Value* opd) = 0;
    virtual bool gt(Runtime* rt, Value* opd) = 0;
    virtual bool ge(Runtime* rt, Value* opd) = 0;
    virtual bool lt(Runtime* rt, Value* opd) = 0;
    virtual bool le(Runtime* rt, Value* opd) = 0;
    virtual double toReal(Runtime* rt) = 0;
    virtual int8   toInt(Runtime* rt) = 0;
    virtual tstring_t toString(Runtime* rt) = 0;

    virtual bool like(Runtime* rt, Value* opd)
    {
        return !opd->isNull() && toString(rt).like(opd->toString(rt));
    }
    virtual ~Value() {}
};

class NullValue : public Value 
{
  public:
    static NullValue null;

	//[MC]
	virtual bool isNull() override
	{
		return true;
	}

    virtual Value* add(Runtime* rt, Value* opd)
    {
        return &null;
    }

    virtual Value* sub(Runtime* rt, Value* opd) 
    {
        return &null;
    }

    virtual Value* mul(Runtime* rt, Value* opd) 
    {
        return &null;
    }

    virtual Value* div(Runtime* rt, Value* opd)
    {
        return &null;
    }

    virtual Value* bit_and(Runtime* rt, Value* opd)
    {
        return &null;
    }

    virtual Value* bit_or(Runtime* rt, Value* opd)
    {
        return &null;
    }

    virtual Value* bit_xor(Runtime* rt, Value* opd)
    {
        return &null;
    }

    virtual Value* bit_not(Runtime* rt) 
    {
        return &null;
    }

    virtual Value* neg(Runtime* rt) 
    {
        return &null;
    }

    virtual bool eq(Runtime* rt, Value* opd)
    {
		//[MC]
		return opd->isNull() || (opd->toInt(rt) == 0);
    }

    virtual bool ne(Runtime* rt, Value* opd)
    {
		//[MC]
		return !eq(rt, opd);
    }

    virtual bool gt(Runtime* rt, Value* opd)
    {
        return false;
    }

    virtual bool ge(Runtime* rt, Value* opd) 
    {
        return false;
    }

    virtual bool lt(Runtime* rt, Value* opd) 
    {
        return false;
    }

    virtual bool le(Runtime* rt, Value* opd) 
    {
        return false;
    }

    virtual bool like(Runtime* rt, Value* opd)
    {
        return false;
    }

    virtual double toReal(Runtime* rt) 
    {
        throw InvalidOperationException("NullValue::toReal");
    }
    
    virtual int8   toInt(Runtime* rt) 
    { 
        throw InvalidOperationException("NullValue::toInt");
    }

    virtual tstring_t toString(Runtime* rt)
    {
        throw InvalidOperationException("NullValue::toString");
    }
};

class IntValue : public Value
{
  public:
    virtual Value* add(Runtime* rt, Value* opd)
    {
        return opd->isNull() ? (Value*)&NullValue::null : (Value*)new (rt) IntValue(val + opd->toInt(rt));
    }

    virtual Value* sub(Runtime* rt, Value* opd) 
    {
        return opd->isNull() ? (Value*)&NullValue::null : (Value*)new (rt) IntValue(val - opd->toInt(rt));
    }

    virtual Value* mul(Runtime* rt, Value* opd) 
    {
        return opd->isNull() ? (Value*)&NullValue::null : (Value*)new (rt) IntValue(val * opd->toInt(rt));
    }

    virtual Value* div(Runtime* rt, Value* opd)
    {
        return opd->isNull() ? (Value*)&NullValue::null : (Value*)new (rt) IntValue(val / opd->toInt(rt));
    }

    virtual Value* bit_and(Runtime* rt, Value* opd)
    {
        return opd->isNull() ? (Value*)&NullValue::null : (Value*)new (rt) IntValue(val & opd->toInt(rt));
    }

    virtual Value* bit_or(Runtime* rt, Value* opd)
    {
        return opd->isNull() ? (Value*)&NullValue::null : (Value*)new (rt) IntValue(val | opd->toInt(rt));
    }

    virtual Value* bit_xor(Runtime* rt, Value* opd)
    {
        return opd->isNull() ? (Value*)&NullValue::null : (Value*)new (rt) IntValue(val ^ opd->toInt(rt));
    }

    virtual Value* bit_not(Runtime* rt) 
    {
        return new (rt) IntValue(~val);
    }

    virtual Value* neg(Runtime* rt) 
    {
        return new (rt) IntValue(-val);
    }

    virtual bool eq(Runtime* rt, Value* opd)
    {
        return opd->isNull() ? false : val == opd->toInt(rt);
    }

    virtual bool ne(Runtime* rt, Value* opd)
    {
        return !opd->isNull() && val != opd->toInt(rt);
    }

    virtual bool gt(Runtime* rt, Value* opd)
    {
        return !opd->isNull() && val > opd->toInt(rt);
    }

    virtual bool ge(Runtime* rt, Value* opd) 
    {
        return !opd->isNull() && val >= opd->toInt(rt);
    }

    virtual bool lt(Runtime* rt, Value* opd) 
    {
        return !opd->isNull() && val < opd->toInt(rt);
    }

    virtual bool le(Runtime* rt, Value* opd) 
    {
        return !opd->isNull() && val <= opd->toInt(rt);
    }

    virtual double toReal(Runtime* rt) 
    {
        return (double)val;
    }
    
    virtual int8   toInt(Runtime* rt) { 
        return val;
    }

    virtual tstring_t toString(Runtime* rt)
    {
        char buf[64];
        sprintf(buf, "%" INT8_FORMAT "d", val);
        return tstring_t(rt, buf, false);
    }
    
    IntValue(int8 v) : val(v) {}

  private:
    const int8 val;
};
    

class RealValue : public Value
{
 public:
    virtual Value* add(Runtime* rt, Value* opd)
    {
        return opd->isNull() ? (Value*)&NullValue::null : (Value*)new (rt) RealValue(val + opd->toReal(rt));
    }

    virtual Value* sub(Runtime* rt, Value* opd) 
    {
        return opd->isNull() ? (Value*)&NullValue::null : (Value*)new (rt) RealValue(val - opd->toReal(rt));
    }

    virtual Value* mul(Runtime* rt, Value* opd) 
    {
        return opd->isNull() ? (Value*)&NullValue::null : (Value*)new (rt) RealValue(val * opd->toReal(rt));
    }

    virtual Value* div(Runtime* rt, Value* opd)
    {
        return opd->isNull() ? (Value*)&NullValue::null : (Value*)new (rt) RealValue(val / opd->toReal(rt));
    }

    virtual Value* neg(Runtime* rt) 
    {
        return new (rt) RealValue(-val);
    }

    virtual bool eq(Runtime* rt, Value* opd)
    {
        return !opd->isNull() && val == opd->toReal(rt);
    }

    virtual bool ne(Runtime* rt, Value* opd)
    {
        return !opd->isNull() && val != opd->toReal(rt);
    }

    virtual bool gt(Runtime* rt, Value* opd)
    {
        return !opd->isNull() && val > opd->toReal(rt);
    }

    virtual bool ge(Runtime* rt, Value* opd) 
    {
        return !opd->isNull() && val >= opd->toReal(rt);
    }

    virtual bool lt(Runtime* rt, Value* opd) 
    {
        return !opd->isNull() && val < opd->toReal(rt);
    }

    virtual bool le(Runtime* rt, Value* opd) 
    {
        return !opd->isNull() && val <= opd->toReal(rt);
    }

    virtual double toReal(Runtime* rt) 
    {
        return val;
    }
    
    virtual int8   toInt(Runtime* rt) { 
        return (int8)val;
    }

    virtual tstring_t toString(Runtime* rt)
    {
        char buf[64];
        sprintf(buf, "%lf", val);
        return tstring_t(rt, buf, false);
    }
    
    RealValue(double v) : val(v) {}

  private:
    const double val;
};
    

class StringValue : public Value
{    
  public:
    virtual Value* add(Runtime* rt, Value* opd)
    {
        return opd->isNull() ? (Value*)&NullValue::null : (Value*)new (rt) StringValue(val.concat(rt, opd->toString(rt)));
    }

    virtual Value* sub(Runtime* rt, Value* opd) 
    {
        throw InvalidOperationException("StringValue::sub");
    }

    virtual Value* mul(Runtime* rt, Value* opd) 
    {
        throw InvalidOperationException("StringValue::mul");
    }

    virtual Value* div(Runtime* rt, Value* opd)
    {
        throw InvalidOperationException("StringValue::div");
    }

    virtual Value* neg(Runtime* rt) 
    {
        throw InvalidOperationException("StringValue::neg");
    }

    virtual bool eq(Runtime* rt, Value* opd)
    {
        return !opd->isNull() && val == opd->toString(rt);
    }

    virtual bool ne(Runtime* rt, Value* opd)
    {
        return !opd->isNull() && val != opd->toString(rt);
    }

    virtual bool gt(Runtime* rt, Value* opd)
    {
        return !opd->isNull() && val > opd->toString(rt);
    }

    virtual bool ge(Runtime* rt, Value* opd) 
    {
        return !opd->isNull() && val >= opd->toString(rt);
    }

    virtual bool lt(Runtime* rt, Value* opd) 
    {
        return !opd->isNull() && val < opd->toString(rt);
    }

    virtual bool le(Runtime* rt, Value* opd) 
    {
        return !opd->isNull() && val <= opd->toString(rt);
    }

    virtual bool like(Runtime* rt, Value* opd)
    {
        return !opd->isNull() && val.like(opd->toString(rt));
    }

    virtual double toReal(Runtime* rt) 
    {
        char buf[64];
        double d;
        int n;
        size_t len = val.getChars(buf, sizeof buf);
        if (len < sizeof(buf) && sscanf(buf, "%lf%n", &d, &n) == 1 && (size_t)n == len) { 
            return d;
        }
        throw ConversionException(buf, "real");
    }
    
    virtual int8 toInt(Runtime* rt) { 
        char buf[64];
        int8 i;
        int n;
        size_t len = val.getChars(buf, sizeof buf);
        if (len < sizeof(buf) && sscanf(buf, "%" INT8_FORMAT "d%n", &i, &n) == 1 && (size_t)n == len) { 
            return i;
        }
        throw ConversionException(buf, "integer");
    }

    virtual tstring_t toString(Runtime* rt)
    {
        return val;
    }
    
    StringValue(tstring_t v) : val(v) {}

  private:
    const tstring_t val;
};

class Query 
{
  public:
    struct Expression { 
        void* operator new(size_t size, Runtime* rt) { 
            return rt->allocate(size);
        }
        void operator delete(void*) {}

        virtual Value* evaluate(Runtime* rt) { return &NullValue::null; }
        virtual bool evaluateCond(Runtime* rt) { return false; }
        virtual bool isCondition() { return false; }
    };

    struct AddExpression : Expression { 
        Value* evaluate(Runtime* rt) {
            return left->evaluate(rt)->add(rt, right->evaluate(rt));        
        }
        AddExpression(Expression* l, Expression* r) : left(l), right(r) {}
        Expression* left;
        Expression* right;
    };

    struct BitAndExpression : Expression { 
        Value* evaluate(Runtime* rt) {
            return left->evaluate(rt)->bit_and(rt, right->evaluate(rt));        
        }
        BitAndExpression(Expression* l, Expression* r) : left(l), right(r) {}
        Expression* left;
        Expression* right;
    };

    struct BitOrExpression : Expression { 
        Value* evaluate(Runtime* rt) {
            return left->evaluate(rt)->bit_or(rt, right->evaluate(rt));        
        }
        BitOrExpression(Expression* l, Expression* r) : left(l), right(r) {}
        Expression* left;
        Expression* right;
    };

    struct BitXorExpression : Expression { 
        Value* evaluate(Runtime* rt) {
            return left->evaluate(rt)->bit_xor(rt, right->evaluate(rt));        
        }
        BitXorExpression(Expression* l, Expression* r) : left(l), right(r) {}
        Expression* left;
        Expression* right;
    };

    struct SubExpression : Expression { 
        Value* evaluate(Runtime* rt) {
            return left->evaluate(rt)->sub(rt, right->evaluate(rt));        
        }
        SubExpression(Expression* l, Expression* r) : left(l), right(r) {}
        Expression* left;
        Expression* right;
    };

    struct MulExpression : Expression { 
        Value* evaluate(Runtime* rt) {
            return left->evaluate(rt)->mul(rt, right->evaluate(rt));        
        }
        MulExpression(Expression* l, Expression* r) : left(l), right(r) {}
        Expression* left;
        Expression* right;
    };

    struct DivExpression : Expression { 
        Value* evaluate(Runtime* rt) {
            return left->evaluate(rt)->div(rt, right->evaluate(rt));        
        }
        DivExpression(Expression* l, Expression* r) : left(l), right(r) {}
        Expression* left;
        Expression* right;
    };


    struct NegExpression : Expression { 
        Value* evaluate(Runtime* rt) {
            return opd->evaluate(rt)->neg(rt);
        }
        NegExpression(Expression* o) : opd(o) {}
        Expression* opd;
    };

    struct BitNotExpression : Expression { 
        Value* evaluate(Runtime* rt) {
            return opd->evaluate(rt)->bit_not(rt);
        }
        BitNotExpression(Expression* o) : opd(o) {}
        Expression* opd;
    };

    struct LiteralExpression : Expression { 
        Value* evaluate(Runtime* rt) {
            return val;
        }
        LiteralExpression(Value* v) : val(v) {}
        Value* val;
    };

    
    struct VarExpression : Expression { 
        Value* evaluate(Runtime* rt) {
            assert(binding->value != NULL);
            return binding->value;
        }

        VarExpression(Runtime::Binding* b) : binding(b) {}
        Runtime::Binding* binding;
    };

    struct Condition : Expression { 
        virtual bool isCondition() { return true; }
    };

    struct NotCondition : Condition { 
        bool evaluateCond(Runtime* rt) {
            return !opd->evaluateCond(rt);
        }
        NotCondition(Expression* o) : opd(o) {}
        Expression* opd;
    };


    struct EqCondition : Condition { 
        bool evaluateCond(Runtime* rt) {
            return left->evaluate(rt)->eq(rt, right->evaluate(rt));        
        }
        EqCondition(Expression* l, Expression* r) : left(l), right(r) {}
        Expression* left;
        Expression* right;
    };

    struct NeCondition : Condition { 
        bool evaluateCond(Runtime* rt) {
            return left->evaluate(rt)->ne(rt, right->evaluate(rt));        
        }
        NeCondition(Expression* l, Expression* r) : left(l), right(r) {}
        Expression* left;
        Expression* right;
    };

    struct GtCondition : Condition { 
        bool evaluateCond(Runtime* rt) {
            return left->evaluate(rt)->gt(rt, right->evaluate(rt));        
        }
        GtCondition(Expression* l, Expression* r) : left(l), right(r) {}
        Expression* left;
        Expression* right;
    };

    struct GeCondition : Condition { 
        bool evaluateCond(Runtime* rt) {
            return left->evaluate(rt)->ge(rt, right->evaluate(rt));        
        }
        GeCondition(Expression* l, Expression* r) : left(l), right(r) {}
        Expression* left;
        Expression* right;
    };

    struct LtCondition : Condition { 
        bool evaluateCond(Runtime* rt) {
            return left->evaluate(rt)->lt(rt, right->evaluate(rt));        
        }
        LtCondition(Expression* l, Expression* r) : left(l), right(r) {}
        Expression* left;
        Expression* right;
    };
        
    struct LeCondition : Condition { 
        bool evaluateCond(Runtime* rt) {
            return left->evaluate(rt)->le(rt, right->evaluate(rt));        
        }
        LeCondition(Expression* l, Expression* r) : left(l), right(r) {}
        Expression* left;
        Expression* right;
    };

    struct LikeCondition : Condition { 
        bool evaluateCond(Runtime* rt) {
            return left->evaluate(rt)->like(rt, right->evaluate(rt));        
        }
        LikeCondition(Expression* l, Expression* r) : left(l), right(r) {}
        Expression* left;
        Expression* right;
    };

    struct AndCondition : Condition { 
        bool evaluateCond(Runtime* rt) {
            return left->evaluateCond(rt) && right->evaluateCond(rt);        
        }
        AndCondition(Expression* l, Expression* r) : left(l), right(r) {}
        Expression* left;
        Expression* right;
    };

    struct OrCondition : Condition { 
        bool evaluateCond(Runtime* rt) {
            return left->evaluateCond(rt) || right->evaluateCond(rt);        
        }
        OrCondition(Expression* l, Expression* r) : left(l), right(r) {}
        Expression* left;
        Expression* right;
    };

    struct ContainsCondition : Condition { 
        bool evaluateCond(Runtime* rt);
        ContainsCondition(Expression* set, Runtime* rt) : setExpr(set), runtime(*rt) {}
        
        Expression* setExpr;
        Expression* subquery;
        Runtime runtime;
    };

    class Compiler { 
      public:
        Expression* compile();
        Expression* addVariable();

        Compiler(Runtime* r, char* q) : rt(r), query(q), curr(q) {}

      private:
        enum Token { 
            TKN_EOF,
            TKN_LPAR,
            TKN_RPAR,
            TKN_MUL,
            TKN_DIV,
            TKN_MINUS,
            TKN_PLUS,
            TKN_SCONST,
            TKN_ICONST,
            TKN_RCONST,
            TKN_IDENT,
            TKN_EQ,
            TKN_NE,
            TKN_GT,
            TKN_GE,
            TKN_LT,
            TKN_LE,
            TKN_OR,
            TKN_AND,
            TKN_NOT,
            TKN_LIKE,
            TKN_BIT_AND,
            TKN_BIT_OR,
            TKN_BIT_XOR,
            TKN_BIT_NOT,
            TKN_CONTAINS
        };
        
        Token scan();

        void error(char* p);

        Expression* term();
        Expression* addition();
        Expression* multiplication();
        Expression* conjunction();
        Expression* disjunction();
        Expression* comparison();

        Token     tkn;
        char*     query;
        char*     curr;
        tstring_t sconst;
        double    rconst;
        int8      iconst;
        char      ident[MAX_IDENT_LENGTH];

        Runtime* rt;
    };        
};
        
END_GOODS_NAMESPACE

#endif
