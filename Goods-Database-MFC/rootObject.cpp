#include "pch.h"
#include "rootObject.h"


//----------------------------------------------------------
void RootObject::initialize() const {
	if (is_abstract_root()) {
		ref<RootObject> root = this;
		modify(root)->become(NEW RootObject(DB_VERSION));
	}
}
//----------------------------------------------------------
boolean RootObject::removePerson(char const *email) const {
	return modify(db)->removePerson(email);
}
//----------------------------------------------------------
boolean RootObject::addPerson(const char *name, const char *email,
	const char *password) {
	return modify(db)->addPerson(name, email, password);
}
//----------------------------------------------------------
boolean RootObject::addPerson(const char *email, ref<Person> person) {
	return modify(db)->addPerson(email, person);
}
//----------------------------------------------------------
boolean RootObject::addProduct(const char* description,
	double price, double weight) {
	return modify(db)->addProduct(description, price, weight);
}
boolean RootObject::addProduct(ref<Product> p) {
	return modify(db)->addProduct(p);
}
//----------------------------------------------------------
boolean RootObject::removeProduct(char const* sku) const {
	return modify(db)->removeProduct(sku);
}

boolean RootObject::addDetail(ref<Detail> detail, ref<Order> order) {
	return modify(db)->addDetail(detail, order);
}

boolean RootObject::removeDetail(ref<Detail> detail, ref<Order> order) {
	return modify(db)->removeDetail(detail, order);
}
//-------------------------------------------------------
void RootObject::printDetail(ref<Detail> d) {
	if (d != NULL) {
		d->print();
	}
}
//----------------------------------------------------------
boolean RootObject::addOrder() {
	return modify(db)->addOrder();
}
//----------------------------------------------------------
boolean RootObject::removeOrder(char const* order_id) {
	return modify(db)->removeOrder(order_id);
}

void RootObject::printOrder(char const * id) const {
	db->findOrder(id)->print();
}

//----------------------------------------------------------
field_descriptor &RootObject::describe_components() {
	return FIELD(db);
}
//----------------------------------------------------------
REGISTER(RootObject, object, pessimistic_exclusive_scheme);
