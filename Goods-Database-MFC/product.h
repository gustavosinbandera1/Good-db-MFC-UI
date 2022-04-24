#pragma once
#include "setMember.h"

class Product : public SetMember {
public:
	inline int4 getId() const { return _iSku; }
	inline real4 getPrice(void) const { return _nPrice; };
	inline real4 getWeight(void) const { return _nWeight; };
	inline ref<String> getDescription(void) const { return _sDescription; };
	inline int4 getSku() const { return _iSku; }
	inline void setPrice(double  price) { _nPrice = price; }
	inline void setWeight(double weight) { _nWeight = weight; }

	struct selectPattern {
		char const *sSku;
	};

	void print(void const* arg = NULL) const override;

	METACLASS_DECLARATIONS(Product, SetMember);

	Product(char const *sku, char const *description, double price,
		double weight);

private:

	//ref<String> 
	int4 _iSku;
	ref<String> _sDescription;
	real4 _nPrice;
	real4 _nWeight;
};
