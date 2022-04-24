#include "pch.h"
#include "setMember.h"
#include "address.h"


Address::Address(char const* city, char const* state,
	char const* country, char const* street, char const* s_type, AddressType e_type) :
	SetMember(self_class) {
	sCity = String::create(city);
	sState = String::create(state);
	sCountry = String::create(country);
	sStreetNumber = String::create(street);
	sType = String::create(s_type);
	eType = e_type;
}

field_descriptor &Address::describe_components() {
	return FIELD(sCity), FIELD(sState),
		FIELD(sCountry), FIELD(sStreetNumber), FIELD(sType);
}

void Address::print(void const *arg) const {
	console::output("addresses \nCity: ");
	sCity->print();
	console::output("\nstate: ");
	sState->print();
	console::output("\nCountry: ");
	sCountry->print();
	console::output("\nStreet: ");
	sStreetNumber->print();
	console::output("\nType: ");
	sType->print();
}

REGISTER(Address, SetMember, pessimistic_scheme);