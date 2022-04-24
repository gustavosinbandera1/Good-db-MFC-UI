#pragma once
#include "pch.h"
#include "rootObject.h"
#include "ordersDB.h"
#include <functional>
#include <map>

#define DB_MANAGER DatabaseManager::getInstance()

using CallbackAction = std::function<void(void)>;
using Actions = std::map<std::string, CallbackAction>;


class DatabaseManager {

private:
	static DatabaseManager *inst_;  // The one, single instance
	DatabaseManager();             // private constructor
	DatabaseManager(const DatabaseManager &);
	DatabaseManager &operator=(const DatabaseManager &);
	
public:

	// This is how clients can access the single instance
	static DatabaseManager *getInstance();
	// just for testing  test data, 
	void addDemoPeople(void);
	void addDemoProducts(void);
	void addDemoOrders(void);

	void addPerson(CString name, CString email, CString passowrd);
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
	void update();
	void populateData(void);
	boolean insertDetail(char const* orderID, ref<Detail> detail);
	static void task_proc start_update_process(void* arg);
public:
	int connect();
	Actions actions;
	boolean executeAction(std::string action);
	inline boolean isSessionOpened() { return session_opened; }
};
