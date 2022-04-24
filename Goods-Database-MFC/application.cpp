#include "pch.h"
#include "database-guard.h"
#include "application.h"

#define QUERY_CONFIG  \
  "C:\\Users\\gusta\\Documents\\server-config-dbs\\orders-config.cfg"

char buf[6][256];
//----------------------------------------------------
static void input(const char *prompt, char *buf, size_t buf_size) {
	char *p;
	do {
		console::output("\n		");
		console::output(prompt);
		*buf = '\0';
		console::input(buf, buf_size);
		p = buf + strlen(buf);
	} while (p <= buf + 1);

	if (*(p - 1) == '\n') {
		*--p = '\0';
	}
}
//----------------------------------------------------
Application::Application() {
	actions.emplace("A", [this](void) { this->addPerson(); });
	actions.emplace("B", [this](void) { this->printAllPerson(); });
	actions.emplace("C", [this](void) { this->deletePerson(); });
	actions.emplace("D", [this](void) { this->addProduct(); });
	actions.emplace("E", [this](void) { this->printAllProduct(); });
	actions.emplace("F", [this](void) { this->deleteProduct(); });
	actions.emplace("G", [this](void) { this->addOrder(); });
	actions.emplace("H", [this](void) { this->deleteOrder(); });
	actions.emplace("I", [this](void) { this->printOrder(); });
	actions.emplace("J", [this](void) { this->printAllOrders(); });
	actions.emplace("N", [this](void) { this->addDetail(); });
	actions.emplace("O", [this](void) { this->deleteDetail(); });
	actions.emplace("Q", [this](void) { this->quit(); });
	actions.emplace("Z", [this](void) { this->populateData(); });
}
//----------------------------------------------------
void Application::addDemoPeople() {
	//-------- Person-1 
	strcpy_s(buf[0], "gustavo");
	strcpy_s(buf[1], "gustavo@gmail.com");
	strcpy_s(buf[2], "1234");
	ref<Person> p1 = NEW Person(buf[0], buf[1], buf[2]);
	modify(p1)->setAddress("Armenia-1", "Quindio-1", "Colombia-1",
		"universal calle 24-1", "MAIN_ADDRESS");
	modify(p1)->setAddress("Armenia-2", "Quindio-2", "Colombia-2",
		"universal calle 24-2", "SHIPPING_ADDRESS");
	modify(p1)->setAddress("Armenia-3", "Quindio-3", "Colombia-3",
		"universal calle 24-3", "BILLING_ADDRESS");
	modify(root)->addPerson(buf[1], p1);

	//-------- Person-2
	strcpy_s(buf[0], "nicolas");
	strcpy_s(buf[1], "nico@gmail.com");
	strcpy_s(buf[2], "abcd");
	ref<Person> p2 = NEW Person(buf[0], buf[1], buf[2]);
	modify(p2)->setAddress("Pereira", "Risaralda", "Colombia",
		"Laureles calle 25", "BILLING_ADDRESS");
	modify(p2)->setAddress("Armenia-2", "Quindio-2", "Colombia-2",
		"universal calle 24-2", "SHIPPING_ADDRESS");
	modify(p2)->setAddress("Armenia-3", "Quindio-3", "Colombia-3",
		"universal calle 24-3", "BILLING_ADDRESS");
	modify(root)->addPerson(buf[1], p2);

	//-------- Person-3
	strcpy_s(buf[0], "pedro");
	strcpy_s(buf[1], "pedro@gmail.com");
	strcpy_s(buf[2], "4321");
	ref<Person> p3 = NEW Person(buf[0], buf[1], buf[2]);
	modify(p3)->setAddress("Florencia", "Caqueta", "Colombia", "Centro calle 26",
		"SHIPPING_ADDRESS");
	modify(p3)->setAddress("Armenia-2", "Quindio-2", "Colombia-2",
		"universal calle 24-2", "SHIPPING_ADDRESS");
	modify(p3)->setAddress("Armenia-3", "Quindio-3", "Colombia-3",
		"universal calle 24-3", "BILLING_ADDRESS");
	modify(root)->addPerson(buf[1], p3);

	//-------- Person-4
	strcpy_s(buf[0], "juan");
	strcpy_s(buf[1], "juan@gmail.com");
	strcpy_s(buf[2], "4321");
	ref<Person> p4 = NEW Person(buf[0], buf[1], buf[2]);
	modify(p4)->setAddress("Florencia", "Caqueta", "Colombia", "Centro calle 26",
		"SHIPPING_ADDRESS");
	modify(p4)->setAddress("Armenia-2", "Quindio-2", "Colombia-2",
		"universal calle 24-2", "SHIPPING_ADDRESS");
	modify(p4)->setAddress("Armenia-3", "Quindio-3", "Colombia-3",
		"universal calle 24-3", "BILLING_ADDRESS");
	modify(root)->addPerson(buf[1], p4);
}
//----------------------------------------------------
void Application::addDemoProducts(void) {
	strcpy_s(buf[0], "Modem");  // description
	strcpy_s(buf[1], "1235.4"); // price
	strcpy_s(buf[2], "32.1");   // weight
	modify(root)->addProduct(buf[0], std::stod(buf[1]), std::stod(buf[2]));

	strcpy_s(buf[0], "Pc");       // description
	strcpy_s(buf[1], "12435.23"); // price
	strcpy_s(buf[2], "12.18");    // weight
	modify(root)->addProduct(buf[0], std::stod(buf[1]),
		std::stod(buf[2]));

	strcpy_s(buf[0], "TV");       // description
	strcpy_s(buf[1], "43267.22"); // price
	strcpy_s(buf[2], "4.32");     // weight
	modify(root)->addProduct(buf[0], std::stod(buf[1]),
		std::stod(buf[2]));

	strcpy_s(buf[0], "Desktop"); // description
	strcpy_s(buf[1], "2345.6");  // price
	strcpy_s(buf[2], "2.32");    // weight
	modify(root)->addProduct(buf[0], std::stod(buf[1]),
		std::stod(buf[2]));

	strcpy_s(buf[0], "Car");    // description
	strcpy_s(buf[1], "12000"); // price
	strcpy_s(buf[2], "10.5"); // weight
	modify(root)->addProduct(buf[0], std::stod(buf[1]),
		std::stod(buf[2]));

	strcpy_s(buf[0], "Motor"); // description
	strcpy_s(buf[1], "1600");  // price
	strcpy_s(buf[2], "12.34");    // weight
	modify(root)->addProduct(buf[0], std::stod(buf[1]),
		std::stod(buf[2]));

}
//----------------------------------------------------
void Application::addDemoOrders() {
	ref<Person> person_1 = ordersDb->findPerson("gustavo@gmail.com");
	ref<Order> order_1 = ordersDb->findOrder("1");
	if (order_1 == NULL) {
		modify(root)->addOrder();
		order_1 = ordersDb->findOrder("1");
		modify(order_1)->setOwner(person_1);
		addDetail(order_1, "1", std::stod("3"));
		addDetail(order_1, "2", std::stod("4"));
		addDetail(order_1, "3", std::stod("4"));
	}

	ref<Person> person_2 = ordersDb->findPerson("nico@gmail.com");
	ref<Order> order_2 = ordersDb->findOrder("2");
	if (order_2 == NULL) {
		modify(root)->addOrder();
		order_2 = ordersDb->findOrder("2");
		modify(order_2)->setOwner(person_2);
		addDetail(order_2, "4", std::stod("2"));
		addDetail(order_2, "5", std::stod("2"));
		addDetail(order_2, "6", std::stod("2"));
	}

	ref<Person> person_3 = ordersDb->findPerson("pedro@gmail.com");
	ref<Order> order_3 = ordersDb->findOrder("3");
	if (order_3 == NULL) {
		modify(root)->addOrder();
		order_3 = ordersDb->findOrder("3");
		modify(order_3)->setOwner(person_3);
		addDetail(order_3, "1", std::stod("3"));
		addDetail(order_3, "2", std::stod("1"));
		addDetail(order_3, "3", std::stod("4"));
	}

}
//----------------------------------------------------
void Application::addPerson() {
	input("Name: ", buf[0], sizeof buf[0]);
	input("Email: ", buf[1], sizeof buf[1]);
	input("Password: ", buf[2], sizeof buf[2]);
	modify(root)->addPerson(buf[0], buf[1], buf[2]);
}
//----------------------------------------------------
void Application::printAllPerson() const {
	ordersDb->printAllPersons();
}
//----------------------------------------------------
void Application::deletePerson() {
	input("Name: ", buf[0], sizeof buf[0]);
	modify(root)->removePerson(buf[0]);
}
//----------------------------------------------------
void Application::addProduct() {
	input("Description: ", buf[0], sizeof buf[0]);
	input("Price: ", buf[1], sizeof buf[1]);
	input("Weight: ", buf[2], sizeof buf[2]);
	modify(root)->addProduct(buf[0], std::stod(buf[1]),
		std::stod(buf[2]));
}
//----------------------------------------------------
void Application::printAllProduct() {
	ordersDb->printAllProducts();
}
//----------------------------------------------------
void Application::deleteProduct() {
	input("Sku: ", buf[0], sizeof buf[0]);
	modify(root)->removeProduct(buf[0]);
}
//----------------------------------------------------
void Application::addOrder() {
	modify(root)->addOrder();
}
//----------------------------------------------------
void Application::addDetail() {
	input("Product sku: ", buf[0], sizeof buf[0]);
	input("Quantity: ", buf[1], sizeof buf[1]);
	input("Order ID: ", buf[2], sizeof buf[2]);

	ref<Product> p = ordersDb->findProduct(buf[0]);
	if (p == NULL) return;

	real4 tmpPrice = p->getPrice();
	console::output("\nthe price on table %0.2f", tmpPrice);

	ref<Order> order = ordersDb->findOrder(buf[2]);
	if (order == NULL) return;

	int4 index = ordersDb->getLastDetailIndex(order) + 1;
	ref<Detail> detail = NEW Detail(numberToString(index).c_str(), buf[0], std::stod(buf[1]));
	modify(detail)->setPrice(p->getPrice()*std::stod(buf[1]));
	modify(detail)->setOwner(order);
	insertDetail(buf[2], detail);
}
//----------------------------------------------------
void Application::addDetail(char const* orderId, char const* productSku, int quantity) {
	nat4 index;
	ref<Order> order = ordersDb->findOrder(orderId);
	ref<Product> product = ordersDb->findProduct(productSku);
	if (order != NULL) {
		index = ordersDb->getLastDetailIndex(order) + 1;
		console::output("\nwe get the order object .........");
		ref<Detail> detail = NEW Detail(numberToString(index).c_str(), productSku, quantity);
		modify(detail)->setOwner(order);
		if (product != NULL) {
			modify(detail)->setPrice(product->getPrice());
			modify(order)->addDetail(productSku, detail);
			console::output("\nDetail added ...");
			return;
		}
		console::output("\nNo product found ..");
		return;
	}
	console::output("\nThere was an issue trying to add a new detail to order");
}
//----------------------------------------------------
void Application::addDetail(ref<Order> order, char const* productSku, int quantity) {
	ref<Product> product = ordersDb->findProduct(productSku);
	if (order != NULL) {
		nat4 index = ordersDb->getLastDetailIndex(order) + 1;
		ref<Detail> detail = NEW Detail(numberToString(index).c_str(), productSku, quantity);
		modify(detail)->setOwner(order);
		if (product != NULL) {
			modify(detail)->setPrice(product->getPrice());
			modify(order)->addDetail(productSku, detail);
			console::output("\nDetail added ...");
			return;
		}
		console::output("\nNo product found ..");
		return;
	}
	console::output("\nThere was an issue trying to add a new detail to order");
}
//----------------------------------------------------
void Application::printAllOrders() {
	ordersDb->printAllOrders();
}
//----------------------------------------------------
void Application::deleteDetail(char const * orderId, char const * detailId) {
	ref<Order> order = ordersDb->findOrder(orderId);
	if (order != NULL) {
		modify(order)->removeDetail(detailId);
	}
}
//----------------------------------------------------
void Application::deleteDetail() {
	input("Order ID: ", buf[0], sizeof buf[0]);
	input("Detail ID: ", buf[1], sizeof buf[1]);
	deleteDetail(buf[0], buf[1]);
}
//----------------------------------------------------
void Application::quit() {
	session_opened = false;
	cs.leave();
}
//----------------------------------------------------
void Application::printOrder() {
	input("Order ID: ", buf[0], sizeof buf[0]);
	root->printOrder(buf[0]);
}
//----------------------------------------------------
void Application::deleteOrder() {
	input("Order ID: ", buf[0], sizeof buf[0]);
	modify(root)->removeOrder(buf[0]);
}
//----------------------------------------------------
void Application::populateData() {
	/* sample data */
	addDemoPeople();
	addDemoProducts();
	addDemoOrders();
}
//----------------------------------------------------
boolean Application::insertDetail(char const *orderID, ref<Detail> detail) {
	ref<Order> order = ordersDb->findOrder(orderID);
	if (order != NULL) {
		modify(order)->addDetail(detail->getSku()->get_text(), detail);
		return True;
	}
	console::output("Order no found");
	return False;
}
//----------------------------------------------------
void Application::printMenu() {

	console::output("Number of persons are %d\n", ordersDb->personListSize());
	console::output("Menu:\n\
	|(a)| Add new Person \n\
	|(b)| Print all persons \n\
	|(c)| Delete Person \n\
	|(d)| Add new Product \n\
	|(e)| Print all products \n\
	|(f)| Delete Product \n\
	|(g)| add order \n\
	|(h)| delete order \n\
	|(i)| print order \n\
	|(j)| print all Orders \n\
	|(k)| Set Order shipping address \n\
	|(l)| Set order billing address \n\
	|(m)| Set Order Owner \n\
	|(n)| Set Order Detail \n\
	|(o)| Delete Detail \n\
	|(q)| Quit \n");
}
//----------------------------------------------------
void task_proc Application::start_update_process(void *arg) {
	((Application *)arg)->update();
}
//----------------------------------------------------
void Application::dialogue() {
	task::create(start_update_process, this);

	while (session_opened) {
		printMenu();
		input(">> ", buf[0], sizeof buf[0]);

		char ch = toupper(*buf[0]);
		std::string option(1, ch);
		cs.enter();
		executeAction(option);
		cs.leave();
		console::output("\n<<< Press ENTER to  continue >>>");
		/*wait for key input to continue */
		console::input(buf[0], sizeof buf[0]);
	}
}
//----------------------------------------------------
void Application::update() {
	// thread for sending signals about  updates to others clients 
	while (session_opened) {
	}
}
//----------------------------------------------------
int Application::main() {
	task::initialize(task::huge_stack);

	if (database_guard _{ this->db, QUERY_CONFIG }) {
		session_opened = True;
		console::output("Session is opened\n");
		this->db.get_root(root);
		this->root->initialize();

		this->ordersDb = root->db;
		dialogue();

		console::output("Session is closed\n");
		return EXIT_SUCCESS;
	}
	else {
		console::output("Failed to open database\n");
		return EXIT_FAILURE;
	}
}
//----------------------------------------------------
boolean Application::executeAction(std::string action) {
	auto it = actions.find(action);
	if (it != actions.end()) {
		it->second();
		return true;
	}
	return false;
}
//----------------------------------------------------