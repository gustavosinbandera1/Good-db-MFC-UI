#include "pch.h"
#include "person.h"


boolean Person::isUser() const { return True; }

void Person::changePassword(char const *password) {
	_password = String::create(password);
}

void Person::changeEmail(char const *e_mail) {
	_email_address = String::create(e_mail);
}

void Person::print(void const *arg) const {
	console::output("Person ");
	console::output("\nName: ");
	_name->print();
	console::output("\nEmail: ");
	_email_address->print();
	console::output("\npassword: ");
	_password->print();
	_set_all_user_addresses.print();
}

boolean Person::select(void const *pattern) const {
	SelectPattern *pat = (SelectPattern *)pattern;
	return (pat->name == NULL || _name->index(pat->name) != -1) &&
		(pat->eMail == NULL || _email_address->index(pat->eMail) != -1);
}

void Person::setAddress(char const* city, char const* state,
	char const* country, char const* streetNumber, char const* type) {
	ref<Address> tmpAddress = NEW Address(city, state, country, streetNumber, type);
	_set_all_user_addresses.insertUnique(type, tmpAddress);
}

void Person::setAddress(char const* owner_address_email, ref<Address> address) {
	_set_all_user_addresses.insertUnique(owner_address_email, address);
}

boolean Person::deleteAddress(char const * type) {
	return _set_all_user_addresses.erase(type) != NULL ? True : False;
}

ref<Address> Person::getAddress(char const * type) const {
	return  _set_all_user_addresses.find(type);
}

field_descriptor &Person::describe_components() {
	return FIELD(_name), FIELD(_email_address),
		FIELD(_password), FIELD(_set_all_user_addresses);
}

Person::Person(char const *name, char const *eMail,
	char const *password, class_descriptor &desc)
	: SetMember(desc) {
	_name = String::create(name);
	_email_address = String::create(eMail);
	_password = String::create(password);
	_set_all_user_addresses.initialize(this);
}


REGISTER(Person, SetMember, pessimistic_scheme);
