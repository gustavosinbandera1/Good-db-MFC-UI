#pragma once
#include "pch.h"
#include "rootObject.h"
#include "ordersDB.h"
#include <functional>
#include <map>

using CallbackAction = std::function<void(void)>;
using Actions = std::map<std::string, CallbackAction>;

class Application {
private:
	// just for testing  test data, 
	void addDemoPeople(void);
	void addDemoProducts(void);
	void addDemoOrders(void);

	void addPerson();
	void printAllPerson() const;
	void deletePerson();

	void addProduct();
	void printAllProduct();
	void deleteProduct();

	void addOrder();
	void printAllOrders();
	void printOrder();
	void deleteOrder();

	void addDetail();
	void addDetail(char const* orderId, char const* productSku, int quantity);
	void addDetail(ref<Order> order, char const* productSku, int quantity);
	void deleteDetail(char const* orderId, char const* detailId);
	void deleteDetail();
	void quit();

protected:
	mutex           cs;
	database        db;
	ref<OrdersDB>   ordersDb;
	ref<RootObject>  root;
	boolean session_opened;
	void printMenu();
	void dialogue();
	void update();

	void populateData(void);
	boolean insertDetail(char const* orderID, ref<Detail> detail);
	static void task_proc start_update_process(void* arg);
public:
	int main();
	Actions actions;
	Application();
	boolean executeAction(std::string action);
};
