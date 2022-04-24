#pragma once
#include "setMember.h"

typedef enum _AddressType {
	BILLING_ADDRESS,
	MAIN_ADDRESS,
	SHIPPING_ADDRESS
} AddressType;

class Address : public SetMember {
public:
	inline ref<String> getCity(void) const { return this->sCity; };
	inline ref<String> getState(void) const { return this->sState; };
	inline ref<String> getCountry(void) const { return this->sCountry; };
	inline ref<String> getStreet(void) const { return this->sStreetNumber; };
	inline ref<String> getType(void) const { return this->sType; };
	AddressType get_Type(void) const { return this->eType; };

	METACLASS_DECLARATIONS(Address, SetMember);
	Address(char const *city, char const *state, char const *country,
		char const *street, char const *type,
		AddressType address = MAIN_ADDRESS);

	void print(void const *arg = NULL) const;
	// String typeToString();
private:
	ref<String> sCity;
	ref<String> sState;
	ref<String> sCountry;
	ref<String> sStreetNumber;
	ref<String> sType;
	AddressType eType;
};
