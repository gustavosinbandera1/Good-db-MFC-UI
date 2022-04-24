#pragma once
#include "ordersDB.h"

class RootObject : public object {
public:
	ref<OrdersDB> db;
	void initialize() const;

	RootObject(int version) : object(self_class) {
		db = NEW OrdersDB(version);
	}

	boolean addPerson(const char* name, const char* email, const char* password);
	boolean addPerson(const char *name, ref<Person> person);
	boolean removePerson(char const* name) const;

	boolean addOrder();
	boolean removeOrder(char const* order_id);
	void printOrder(char const* id) const;


	boolean addProduct(const char* description, double price, double weight);
	boolean addProduct(ref<Product> p);
	boolean removeProduct(char const* sku) const;

	boolean addDetail(ref<Detail> detail, ref<Order> order);
	boolean removeDetail(ref<Detail> detail, ref<Order> order);
	void printDetail(ref<Detail> d);

	METACLASS_DECLARATIONS(RootObject, object);
};

