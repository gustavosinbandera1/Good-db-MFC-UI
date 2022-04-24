#pragma once
#include "setMember.h"
//#include "order.h" ******************** ojo *****************
#include "product.h"
#include <string>
#include <sstream>


class Order;
template <typename T>
std::string numberToString(T Number) {
	std::ostringstream ss;
	ss << Number;
	return ss.str();
}

class Detail : public SetMember {
protected:
	ref<Order> detail_owner_;
public:
	void print(void const *arg = NULL) const;
	void print_sku() const;
	nat4 getId() const { return this->_id; };
	ref<String> getSku() const { return this->_product_sku; };
	Detail(char const* detailID, char const* productSku, int quantity);

	void setOwner(ref<Order> order);
	ref<Order> getOwner() const { return detail_owner_; };

	inline void setPrice(real4 price) { _product_price = price; }
	inline real4 getPrice() const { return _product_price; }


	/*static ref<Detail> create(int quantity, double price,
		ref<Order> bill, char const* productSku);*/
	METACLASS_DECLARATIONS(Detail, SetMember);

private:
	nat4 _id;
	ref<String> _product_sku;
	nat1 _quantity;
	real4 _product_price;
	//real4 _total;
};

