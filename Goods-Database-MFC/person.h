#pragma once
#include "setMember.h"
#include "address.h"
#include "small_set.h"

class Person : public SetMember {
public:
	virtual boolean isUser() const;
	void changePassword(char const *password);
	void changeEmail(char const *e_mail);
	void setAddress(char const *city, char const *state, char const *country,
		char const *streetNumber, char const *type);
	void setAddress(char const *owner_address_email, ref<Address> address);
	boolean deleteAddress(char const *type);
	ref<Address> getAddress(char const *type = "SHIPPING_ADDRESS") const;
	ref<String> getName() const { return this->_name; }
	ref<String> getEmail() const { return this->_email_address; }
	ref<String> getPassword() const { return this->_password; }

	virtual void print(void const *arg = NULL) const override;
	//virtual void returnItem(std::vector<Person> buffer) override;

	struct SelectPattern {
		char const *name;
		char const *eMail;
	};

	boolean select(void const *pattern) const;

	inline boolean checkPassword(char const *password) const {
		return this->_password->compare(password) == 0;
	}

	METACLASS_DECLARATIONS(Person, SetMember);

	Person(char const *name, char const *eMail, char const *password,
		class_descriptor &desc = self_class);

private:
	ref<String> _name;
	ref<String> _email_address;
	ref<String> _password;
	small_set<Address> _set_all_user_addresses;
};
