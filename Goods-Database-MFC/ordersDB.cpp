#include "pch.h"
#include "ordersDB.h"

ref<Person> OrdersDB::findPerson(char const *email) const {
	return  _set_all_people.find(email);
}

boolean OrdersDB::removePerson(char const *email) const {
	return _set_all_people.erase(email) != NULL ? True : False;
}

boolean OrdersDB::addPerson(char const *name, char const* email,
	char const* password) {
	ref<Person> p = NEW Person(name, email, password);
	return _set_all_people.insertUnique(email, p);
}

boolean OrdersDB::addPerson(char const * email, ref<Person> p) {
	return _set_all_people.insertUnique(email, p);
}

size_t OrdersDB::personListSize(void) const {
	return _set_all_people.size();
}

void OrdersDB::printPerson(char const *email) const {
	findPerson(email)->print();
}

void OrdersDB::printAllPersons(void) const {
	_set_all_people.print();
	getLastPersonIndex();
}

int4 OrdersDB::getLastPersonIndex() const {
	ref<Person>  p = _set_all_people.members->last->obj;
	//console::output("last People index %d ", p->getId());
	//return p->getId();
	return 0;
}

large_set<Person> OrdersDB::getPersonList() const {
	return _set_all_people;
}

int4 OrdersDB::getLastProductIndex() const {
	if (_set_all_products.members->last != NULL) {
		ref<Product>  p = _set_all_products.members->last->obj;
		if (p != NULL) return p->getId();
	}
	return 0;
}

large_set<Product> OrdersDB::getProductList() const {
	return _set_all_products;
}

ref<Product> OrdersDB::findProduct(char const * sku) const {
	return _set_all_products.find(sku);
}

boolean OrdersDB::removeProduct(char const * sku) const {
	return _set_all_products.erase(sku) != NULL ? True : False;
}

boolean OrdersDB::addProduct(const char * description, double price, double weight) {
	int4 productIndex = getLastProductIndex() + 1;
	std::string sku = numberToString(productIndex);
	console::output("getting product index %s ", sku.c_str());
	ref<Product> p = NEW Product(sku.c_str(), description, price, weight);
	return _set_all_products.insertUnique(sku.c_str(), p);
}

boolean OrdersDB::addProduct(ref<Product> p) {
	int4 productIndex = getLastProductIndex() + 1;
	std::string sku = numberToString(productIndex);
	console::output("getting product index %s ", sku.c_str());
	return _set_all_products.insertUnique(sku.c_str(), p);
}

size_t OrdersDB::productListSize(void) const {
	return _set_all_products.size();
}

void OrdersDB::printProduct(char const * sku) const {
	findProduct(sku)->print();
}

void OrdersDB::printAllProducts(void) const {
	_set_all_products.print();
	console::output("last index %d ", getLastProductIndex());
}

ref<Order> OrdersDB::findOrder(char const * orderID) const {
	return _set_all_orders.find(orderID);
}

boolean OrdersDB::removeOrder(char const * orderID) const {
	return _set_all_orders.erase(orderID) != NULL ? True : False;
}

boolean OrdersDB::addOrder() {
	int4 orderIndex = getLastOrderIndex() + 1;
	console::output("Order index .......... %d ", orderIndex);
	std::string sOrderID = numberToString(orderIndex);
	console::output("getting order index %s", sOrderID.c_str());
	ref<Order> order = NEW Order(sOrderID.c_str());
	return _set_all_orders.insertUnique(sOrderID.c_str(), order);
}

boolean OrdersDB::addOrder(ref<Order> o) {
	int4 orderIndex = getLastOrderIndex() + 1;
	console::output("Order index .......... %d ", orderIndex);
	std::string sOrderID = numberToString(orderIndex);
	console::output("getting order index %s", sOrderID.c_str());
	return _set_all_orders.insertUnique(sOrderID.c_str(), o);
}

size_t OrdersDB::orderListSize(void) const {
	return _set_all_orders.size();
}

void OrdersDB::printOrder(char const * orderID) const {
	ref<Order> order = _set_all_orders.find(orderID);
	order != NULL ? order->print() : console::output("order not found");
}

void OrdersDB::printAllOrders(void) const {
	_set_all_orders.print();
	console::output("last Order index %d ", getLastOrderIndex());
}

int4 OrdersDB::getLastOrderIndex() const {
	console::output("Testing here.........");
	if (_set_all_orders.members->last != NULL) {
		ref<Order>  o = _set_all_orders.members->last->obj;
		if (o != NULL) return o->getId();
	}
	return 0;
}

ref<Address> OrdersDB::findAddress(char const* owner_address_email, char const* s_type, AddressType e_type) const {
	ref<Person> p = _set_all_people.find(owner_address_email);
	if (p != NULL) {
		return  p->getAddress(s_type);
	}
	return NULL;
}

boolean OrdersDB::removeAddress(char const * owner_address_email, char const * s_type, AddressType e_type) {
	ref<Person> p = _set_all_people.find(owner_address_email);
	if (p != NULL) {
		return modify(p)->deleteAddress(s_type);
	}
	return False;
}

boolean OrdersDB::addAddress(char const * email, ref<Address> address) {
	ref<Person> p = _set_all_people.find(email);
	if (p != NULL) {
		modify(p)->setAddress(email, address);
		return True;
	}
	return False;
}

int4 OrdersDB::getLastDetailIndex(ref<Order> order) const {
	if (order->getSetDetails().members->last != NULL) {
		ref<Detail> d = order->getSetDetails().members->last->obj;
		if (d != NULL) return d->getId();
	}
	return 0;;
}

boolean OrdersDB::addDetail(ref<Detail> newDetail, ref<Order> order) {
	int4 detailIndex = getLastDetailIndex(order) + 1;
	console::output("Detail index %d", detailIndex);
	std::string sDetailId = numberToString(detailIndex);
	console::output("getting detail index %s ", sDetailId.c_str());
	return modify(order)->getSetDetails().insertUnique(sDetailId.c_str(), newDetail);
}

boolean OrdersDB::removeDetail(ref<Detail> detail, ref<Order> order) {
	ref<Detail> d = modify(order)->getSetDetails().erase(numberToString(detail->getId()).c_str());
	if (d != NULL) return True;
	return False;
}

OrdersDB::OrdersDB(int vers) : object(self_class) {
	version = vers;
	_set_all_orders.initialize(this);
	_set_all_people.initialize(this);
	_set_all_products.initialize(this);
}

field_descriptor &OrdersDB::describe_components() {
	return  FIELD(_set_all_people), FIELD(_set_all_products),
		FIELD(_set_all_orders);
}

REGISTER(OrdersDB, object, pessimistic_scheme);
