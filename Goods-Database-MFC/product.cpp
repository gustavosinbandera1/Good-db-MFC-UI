#include "pch.h"
#include "product.h"
#include <string>

Product::Product(char const *sku, char const *description, double price,
	double weight)
	: SetMember(self_class) {
	this->_iSku = std::stod(sku);
	//sSku = String::create(sku);
	_sDescription = String::create(description);
	_nPrice = (real4)price;
	_nWeight = (real4)weight;
}

void Product::print(void const *arg) const {
	//console::output("sku: %d", _iSku);
	//sSku->print();
	//console::output("\nDescription: ");
	//_sDescription->print();
	//console::output("\nPrice: %0.2lf", _nPrice);
	//console::output("\nWeight: %0.2lf", _nWeight);
}

field_descriptor &Product::describe_components() {
	return FIELD(_iSku), FIELD(_sDescription), FIELD(_nPrice), FIELD(_nWeight);
}

REGISTER(Product, SetMember, pessimistic_scheme);
