#pragma once

#include "setMember.h"
#include "small_set.h"
#include "person.h"


typedef enum _PaymentType {
	CASH,
	CREDIT_CARD,
	CHECK
}Payment_type;

class Detail;

class Order : public SetMember {
protected:
	int4 id;

public:
	Order(char const *id, class_descriptor &desc = self_class);
	inline int4 getId() const { return id; }
	inline void setOwner(ref<Person> owner) { this->_owner = owner; };
	inline void setShippingAddress(ref<Address> addr) { this->_shipping_address = addr; }
	inline void setBillingAddress(ref<Address> addr) { this->_billing_address = addr; }
	inline void setMainAddress(ref<Address> addr) { this->_main_address = addr; }
	inline void setPaymentType(Payment_type type) { _payment_type = type; }
	inline void setFirstLine(char const* first_line) {
		_first_line_description = String::create(first_line);
	}

	inline void setSecondLine(char const* second_line) {
		_second_line_description = String::create(second_line);
	}


	inline Payment_type getPaymentType() { return _payment_type; }
	real4 getOrderPrice(long int id);
	real4 getLineOrderPrice(char const* line_id);
	inline ref<Address> getAddress() { return this->_shipping_address; }
	inline ref<Person> getOwner() { return this->_owner; }
	inline boolean commited() { return _commited; }


	boolean addDetail(char const* key, ref<Detail> detail);
	boolean removeDetail(char const* key);
	inline void commitOrder(void) { _commited = True; }

	void print(void const *arg = NULL) const;
	void printBrief() const;
	void printDetails() const;
	boolean select(void const *pattern) const;


	small_set<Detail> getSetDetails() const { return _set_order_details; }

	METACLASS_DECLARATIONS(Order, SetMember);

private:
	boolean _commited = False;
	small_set<Detail> _set_order_details;
	ref<Person> _owner;
	ref<Address> _shipping_address;
	ref<Address> _billing_address;
	ref<Address> _main_address;
	ref<String> _first_line_description;
	ref<String> _second_line_description;

	Payment_type _payment_type;
};

