#ifndef __WINUIHANDLER_H__
#define __WINUIHANDLER_H__

using namespace pfc;

typedef HTREEITEM TreeItem;
typedef HWND Handle;

class TreeView {
private:
	Handle m_handle;

public:
	TreeView() : m_handle(NULL) {
	}

	void setHandle(Handle handle);
	Handle getHandle();

	void clear();
	TreeItem addItem(TreeItem parent, string8& item);
	TreeItem getSelectedItem();
};


#endif /* __WINUIHANDLER_H__ */


