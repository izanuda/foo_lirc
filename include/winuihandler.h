#ifndef __WINUIHANDLER_H__
#define __WINUIHANDLER_H__

using TreeItem = HTREEITEM;
using Handle = HWND;
using pfc::string8;

class TreeView
{
private:
	Handle m_handle;

public:
	TreeView() : m_handle(NULL) {}

	void setHandle(Handle handle);
	Handle getHandle() const;

	void clear();
	TreeItem addItem(TreeItem parent, const string8& item);
	TreeItem getSelectedItem() const;
};


#endif /* __WINUIHANDLER_H__ */


