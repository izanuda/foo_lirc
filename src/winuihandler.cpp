#include "StdAfx.h"
#include "winuihandler.h"

Handle TreeView::getHandle() const
{
	return m_handle;
}

void TreeView::setHandle(Handle handle)
{
	m_handle = handle;
}

void TreeView::clear()
{
	if (m_handle != NULL)
		TreeView_DeleteAllItems(m_handle);
}

TreeItem TreeView::addItem(TreeItem parent, const string8 &item)
{
	if (m_handle == NULL)
		return NULL;

	static HTREEITEM prevItem = (HTREEITEM)TVI_FIRST; 
	LPSTR itemName = (LPSTR)item.get_ptr();

	TVITEMA tvItem{};
	tvItem.mask = TVIF_TEXT | TVIF_PARAM | TVIF_STATE; 
	// set the text and state of the item
	tvItem.pszText = itemName;
	tvItem.state = TVIS_EXPANDED;
	tvItem.stateMask = TVIS_EXPANDED;
	//tvItem.cchTextMax = sizeof(tvItem.pszText)/sizeof(tvItem.pszText[0]);

	TVINSERTSTRUCTA tvInsert{};
	tvInsert.item = tvItem;
	tvInsert.hInsertAfter = prevItem;

	// set the parent item based on the specified level
	tvInsert.hParent = parent ? parent : TVI_ROOT;

	// add the item to the tree-view control.
	prevItem = (HTREEITEM)SendMessage(m_handle, TVM_INSERTITEMA, 0, (LPARAM)(LPTVINSERTSTRUCTA)&tvInsert); 
	return prevItem; 
}

TreeItem TreeView::getSelectedItem() const
{
	return TreeView_GetSelection(m_handle);
}
