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
	static DatabaseManager *_inst;  // The one, single instance
	DatabaseManager();             // private constructor
	DatabaseManager(const DatabaseManager &);
	~DatabaseManager();
	DatabaseManager &operator=(const DatabaseManager &);
	
	/*
	Can more than one thread within the same application access this resource at one time 
	(for example, your application allows up to five windows with views on the same document)?
	If yes, use CSemaphore.
	*/
	CSemaphore c_sema;
	CCriticalSection c_cs;
public:

	// This is how clients can access the single instance
	static DatabaseManager *getInstance();
	// just for testing  test data, 
	void addDemoPeople(void);
	void addDemoProducts(void);
	void addDemoOrders(void);

	void addPerson(CString name, CString email, CString passowrd);
	void printAllPerson() const;
	large_set<Person> getAllPerson() const;
	void deletePerson();

	void addProduct(CString desc, double price, double weight);
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
	boolean connect();
	Actions actions;
	boolean executeAction(std::string action);
	inline boolean isSessionOpened() { return session_opened; }
};
