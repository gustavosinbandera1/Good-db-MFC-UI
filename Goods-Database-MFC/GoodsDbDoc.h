
// GoodsDbDoc.h : interface of the CGoodsDbDoc class
//

#pragma once

//UINT dbTaskThread(LPVOID param);
//class Order;
class CGoodsDbDoc : public CDocument {
protected: // create from serialization only
  CGoodsDbDoc() noexcept;
  DECLARE_DYNCREATE(CGoodsDbDoc)

private:
	DatabaseManager *dbManager;

public:
  virtual BOOL OnNewDocument();
  virtual void Serialize(CArchive &ar);
#ifdef SHARED_HANDLERS
  virtual void InitializeSearchContent();
  virtual void OnDrawThumbnail(CDC &dc, LPRECT lprcBounds);
#endif // SHARED_HANDLERS

  // Implementation
public:
  virtual ~CGoodsDbDoc();
#ifdef _DEBUG
  virtual void AssertValid() const;
  virtual void Dump(CDumpContext &dc) const;
#endif
  void addDemoPeople(void);
  void addDemoProducts(void);
  void addDemoOrders(void);

  void addUser(CString name, CString email, CString password);
  void printAllUser() const;
  void deleteUser(CString email);
  large_set<Person> getAllUser();

  void addProduct(CString description, double price, double weight);
  void printAllProduct();
  void deleteProduct();
  large_set<Product> getAllProduct();

  void addOrder();
  void printAllOrders();
  void printOrder();
  void deleteOrder();

  void addDetail();
  void addDetail(char const* orderId, char const* productSku, int quantity);
  void addDetail(ref<Order> order, char const* productSku, int quantity);
  void deleteDetail(char const* orderId, char const* detailId);
  void deleteDetail();


  void addUserAddress(CString name, CString email, CString password);
  void printAllUserAddress() const;
  void deleteUserAddress(CString email);
  
protected:
  // Generated message map functions
protected:
  DECLARE_MESSAGE_MAP()

#ifdef SHARED_HANDLERS
  // Helper function that sets search content for a Search Handler
  void SetSearchContent(const CString &value);
#endif // SHARED_HANDLERS
};
