#include "StdAfx.h"
#include "cfg_action.h"
#include "winuihandler.h"

DECLARE_COMPONENT_VERSION(
	// component name
	"LIRC Client",
	// component version
	VERSION,
	// about text, use \n to separate multiple lines
	// If you don't want any about text, set this parameter to NULL.
	"LIRC Client "VERSION"\n"
	"for foobar2000 v0.9.x\n"
	"update to 0.9.x SDK branch by wazoo\n\n"
	"enhanced by phi\n\n"
	"Original version copyright (C) 2004, Lev Shamardin\n"
	"http://sourceforge.net/projects/foolirc\n"
	)

#define WM_REMOTE_KEY WM_USER+1
#define WM_SOCKET WM_USER+10

#ifdef _DEBUG
#define DEBUG(x) console::warning(x)
#else
#define DEBUG(x)
#endif

static const GUID guid_cfg_enabled = { 0xdaa7975e, 0x5147, 0x4252, { 0xa8, 0xb8, 0x65, 0x94, 0x0, 0x2e, 0x60, 0x01 } };
static const GUID guid_cfg_lircport = { 0xdaa7975e, 0x5147, 0x4252, { 0xa8, 0xb8, 0x65, 0x94, 0x0, 0x2e, 0x60, 0x02 } };
static const GUID guid_cfg_lircaddr = { 0xdaa7975e, 0x5147, 0x4252, { 0xa8, 0xb8, 0x65, 0x94, 0x0, 0x2e, 0x60, 0x03 } };
static const GUID guid_cfg_actions = { 0xdaa7975e, 0x5147, 0x4252, { 0xa8, 0xb8, 0x65, 0x94, 0x0, 0x2e, 0x60, 0x04 } };
static const GUID guid_prefs_cfg_winlirc = { 0xdaa7975e, 0x5147, 0x4252, { 0xa8, 0xb8, 0x65, 0x94, 0x0, 0x2e, 0x60, 0x06 } };


cfg_int cfg_enabled(guid_cfg_enabled, 0);
cfg_int cfg_lircport(guid_cfg_lircport, 8765);
cfg_string cfg_lircaddr(guid_cfg_lircaddr, "localhost");

HWND config_window;
HWND msg_window;

static SOCKET lirc_socket = INVALID_SOCKET;

static cfg_action g_actions(guid_cfg_actions);

static LRESULT CALLBACK LircWindowProc(HWND wnd,UINT msg,WPARAM wp,LPARAM lp);

HWND create_message_window()
{
	/* create our message window */
	static HWND handle;
	static bool g_inited;
	static const char g_class_name[] = "foo_lirc.dll window class";
	pfc::stringcvt::string_os_from_utf8 os_class_name(g_class_name);
	if (!g_inited)
	{
		g_inited = true;
		WNDCLASS wc;
		memset(&wc,0,sizeof(wc));
		wc.lpfnWndProc = LircWindowProc;
		wc.hInstance = core_api::get_my_instance();
		wc.lpszClassName = os_class_name;
		RegisterClass(&wc);
	}
	if (!handle)
	{
		handle = uCreateWindowEx(0,g_class_name,"foo_lirc",WS_POPUP,0,0,0,0,core_api::get_main_window(),0,core_api::get_my_instance(),0);
	}

	_ASSERTE (handle != NULL);

	ShowWindow(handle,SW_SHOWNOACTIVATE);
	return handle;
}

bool open_socket(HWND message_window)
{
	/* create our socket */
	_ASSERTE (lirc_socket == INVALID_SOCKET);
	lirc_socket = socket (AF_INET,SOCK_STREAM, 0);
	if (lirc_socket == INVALID_SOCKET)
	{
		console::error ("Couldn't create socket.");
		return false;
	}

	/* make the socket asynchronous */
	if (WSAAsyncSelect(lirc_socket, message_window, WM_SOCKET, 
		FD_CONNECT | FD_READ | FD_CLOSE) != 0)
	{
		console::error ("WSAAsyncSelect() failed.");
		return false;
	}

	/* resolve dns if necessary */
	u_long addr = inet_addr (cfg_lircaddr.get_ptr());
	if (addr == INADDR_NONE)
	{
		/* should be using the non-blocking WSAAsyncGetHostByName() here */
		hostent* h = gethostbyname (cfg_lircaddr.get_ptr());
		if (!h) 
		{
			console::error ("Couldn't resolve lirc server address.");
			return false;
		}
		addr = *((u_long*)h->h_addr_list[0]);
	}

	/* connect to the server */
	struct sockaddr_in dest_addr;
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons (cfg_lircport);
	dest_addr.sin_addr.s_addr = addr;
	memset(&(dest_addr.sin_zero), '\0', 8);
	
	if (connect(lirc_socket, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr))==SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSAEWOULDBLOCK)
		{
			console::error ("Socket error.");
			return false;
		}
	}
#if 0
	else
	{
		/* just in case we got lucky and connected instantly */
		PostMessage(message_window, WM_SOCKET, lirc_socket, MAKELPARAM(FD_CONNECT, 0));
	}
#endif
	return true;
}

void close_socket ()
{
	if (lirc_socket != INVALID_SOCKET)
	{
		closesocket (lirc_socket);
		lirc_socket = INVALID_SOCKET;
	}
}

void disconnect_socket()
{
	if (lirc_socket != INVALID_SOCKET)
	{
		shutdown(lirc_socket, 0x01/*SD_SEND*/);
	}
}

bool init_windows_sockets ()
{
	WSADATA w;
	if (WSAStartup (0x0202, &w) != 0)
	{
		console::error ("WSAStartup() failed");
		return false;
	}
	if (w.wVersion != 0x0202)
	{
		console::error ("Wrong WinSock version");
		WSACleanup();
		return false;
	}
	return true;
}

void deinit_windows_sockets ()
{
	WSACleanup();
}

void start_lirc()
{
	_ASSERTE (core_api::assert_main_thread());
	if (!core_api::assert_main_thread()) return;

	msg_window = create_message_window();

	if (!open_socket (msg_window))
	{
		close_socket();
	}
}

static LRESULT CALLBACK LircWindowProc(HWND wnd,UINT msg,WPARAM wp,LPARAM lp)
{
	switch (msg)
	{
	case WM_SOCKET:
		{
			switch(WSAGETSELECTEVENT(lp))
			{
			case FD_CONNECT:
				{
					DEBUG("FD_CONNECT");
					if (WSAGETSELECTERROR(lp))
					{
						close_socket();

						console::warning ("Could not connect to lirc server.");
						if (config_window)
							PostMessage (config_window,msg,0,FD_CLOSE);
					}
					else
					{
						DEBUG("Connected to server");
						if (config_window)
							PostMessage (config_window,msg,wp,lp);
					}
				}
				break;
			case FD_READ:
				{
					DEBUG("FD_READ");
#define BUF_SIZE 8192
					string8 buf;
					int bytes_recv = recv(wp, (char *)string_buffer(buf,BUF_SIZE), BUF_SIZE, 0);
					if (bytes_recv != 0)
					{
						DEBUG(string_printf ("%d bytes received",bytes_recv));
						DEBUG(string_printf("data: %s",buf.get_ptr()));

						unsigned int repeat_count;
						char keyname[BUF_SIZE];
						char remotename[BUF_SIZE];

						if(sscanf(buf.get_ptr(),"%*I64x %x %s %[^\n]",&repeat_count,keyname,remotename) == 3)
						{
							string8 key_id = string_printf("%s (%s)",keyname,remotename);
							if (config_window)
							{
								if (repeat_count==0)
									uSendMessage (config_window,WM_REMOTE_KEY,(long)key_id.get_ptr(),0);
							}
							else
							{
								g_actions.process_keypress (key_id, repeat_count);
							}
						}
					}
#undef BUF_SIZE
				}
				break;
			case FD_CLOSE:
				{
					DEBUG("FD_CLOSE");
					if (config_window)
						PostMessage (config_window,msg,wp,lp);

					char buf[8192];
					int rv;
					do
					{
						rv = recv (lirc_socket, buf, 1024, 0);
					} while (rv != 0 && rv != SOCKET_ERROR);

					close_socket();
				}
				break;
			}
		}
		return 1;
	} /* case: WM_SOCKET */
	return uDefWindowProc (wnd,msg,wp,lp);
}


class initquit_winlirc : public initquit 
{
	virtual void on_init() 
	{
		if (cfg_enabled) 
		{
			if (init_windows_sockets())
				start_lirc();
		}
	}

	virtual void on_quit() 
	{
		disconnect_socket();
		deinit_windows_sockets();
		DestroyWindow (msg_window);
	}
};

static initquit_factory_t< initquit_winlirc > foo_initquit;

static TreeView action_listbox_wnd;
static HWND listview_wnd;

class config_winlirc : public preferences_page {
private:
	struct command_item {
		ptr_list_t<command_item> subitems;

		string8 name;
		string8 path;
		string8 description;

		void sortItems() {
			// sort subitems by name
			subitems.sort_t(command_item::compare_by_name);

			// now make those subitems that have children sort themselves
			size_t count = subitems.get_count();
			for (size_t i = 0; i < count; i++) {
				command_item* current = subitems.get_item(i);
				if (current->subitems.get_count() > 0) {
					current->sortItems();
				}
			}
		}

		command_item* find(string8& byName) {
			size_t count = subitems.get_count();
			for (size_t i = 0; i < count; i++) {
				command_item* current = subitems.get_item(i);
				if (!stricmp_utf8(current->name, byName)) {
					return current;
				}
			}
			return NULL;
		}

		static int compare_by_name(const command_item * p_item1, const command_item * p_item2) {
			return stricmp_utf8(p_item1->name, p_item2->name);
		}
	};

	static void findMainMenuName(GUID & parent, pfc::string8 & out) {
		service_enum_t<mainmenu_group> e;
		service_ptr_t<mainmenu_group> ptr;

		while (e.next(ptr)) {
			if (parent != ptr->get_guid()) {
				continue;
			}

			// TODO: extract the name (find the other way - for now only popup items work !!!)

			service_ptr_t<mainmenu_group_popup> popup;
			if (ptr->service_query_t(popup)) {
				popup->get_display_string(out);
				return;
			} else {
				// console::warning("!!!! not popup");
			}
		}
	}

	static void addMainMenuActions(command_item& actions) {
		command_item* contextActions = new command_item();
		contextActions->name = "[main]";
		actions.subitems.add_item(contextActions);

		service_enum_t<mainmenu_commands> e;
		service_ptr_t<mainmenu_commands> ptr;

		while (e.next(ptr)) {
			command_item* parent = NULL;

			const t_size count = ptr->get_command_count();
			for(t_size i = 0; i < count; i++) {
				if (i == 0) {
					string8 currentParentName;
					findMainMenuName(ptr->get_parent(), currentParentName);

					// see whether such item already exists inside the parent
					parent = contextActions->find(currentParentName);
					if (! parent) {
						// check for items without a parent, such children are added 
						// directly to the context actions root
						if (currentParentName.length() == 0) {
							parent = contextActions;
						} else {
							parent = new command_item();
							contextActions->subitems.add_item(parent);
							parent->name = currentParentName;
						}
					}
				}

				string8 actionName;
				ptr->get_name(i, actionName);

				string8 actionDescr;
				ptr->get_description(i, actionDescr);

				// create the command_item for the current action
				command_item* action = new command_item();
				action->name = actionName;
				action->description = actionDescr;

				// add the action to current parent
				parent->subitems.add_item(action);
			}
		}
	}

	static void addContextMenuActions(command_item& actions) {
		command_item* contextActions = new command_item();
		contextActions->name = "[context]";
		actions.subitems.add_item(contextActions);

		service_enum_t<contextmenu_item> enumer;
		service_ptr_t<contextmenu_item> ptr;

		while (enumer.next(ptr)) {
			command_item* parent = NULL;

			const t_size count = ptr->get_num_items();
			for (t_size i = 0; i < count; i++) {
				if (i == 0) {
					string8 currentParentName;
					ptr->get_item_default_path(i, currentParentName);

					// see whether such item already exists inside the parent
					parent = contextActions->find(currentParentName);
					if (! parent) {
						// check for items without a parent, such children are added 
						// directly to the context actions root
						if (currentParentName.length() == 0) {
							parent = contextActions;
						} else {
							parent = new command_item();
							contextActions->subitems.add_item(parent);
							parent->name = currentParentName;
						}
					}
				}

				string8 actionName;
				ptr->get_item_name(i, actionName);

				string8 actionDescr;
				ptr->get_item_description(i, actionDescr);

				// create the command_item for the current action
				command_item* action = new command_item();
				action->name = actionName;
				action->description = actionDescr;

				// add the action to current parent
				parent->subitems.add_item(action);
			}
		}
	}

	static void addItems(command_item& item, TreeItem parent) {
		TreeItem currentParent = parent;
		if (item.name.length() > 0) {
			currentParent = action_listbox_wnd.addItem(parent, item.name);
		}

		size_t count = item.subitems.get_count();
		for (size_t i = 0; i < count; i++) {
			command_item& current = *item.subitems.get_item(i);
			addItems(current, currentParent);
		}
	}

public:

	static void update_action_list() {
		// start afresh
		action_listbox_wnd.clear();

		command_item allActions;

		addMainMenuActions(allActions);
		addContextMenuActions(allActions);

		// sort to look better
		allActions.sortItems();

		// add to the tree view
		addItems(allActions, NULL);
	}

	static void update_listview(HWND listview_wnd)
	{
		LVITEM item;
		ListView_DeleteAllItems(listview_wnd);
		int num_actions = g_actions.get_count();
		for(int i=0;i<num_actions;i++) 
		{
			action * a = g_actions[i];
			ZeroMemory(&item, sizeof(item));
			item.mask = LVIF_TEXT | LVIF_PARAM;
			item.lParam = i;
			pfc::stringcvt::string_os_from_utf8 os_pszText1(a->get_key_name());
			item.pszText = _tcsdup(os_pszText1.get_ptr());
			int index = ListView_InsertItem(listview_wnd, &item);
			free(item.pszText);

			item.mask = LVIF_TEXT;
			item.iItem = index;
			item.iSubItem = 1;
			pfc::stringcvt::string_os_from_utf8 os_pszText2(a->get_command());
			item.pszText = _tcsdup(os_pszText2.get_ptr());
			ListView_SetItem(listview_wnd, &item);
			free(item.pszText);

			item.iSubItem = 2;
			pfc::stringcvt::string_os_from_utf8 os_pszText3(a->get_repeatable() ? "No" : "Yes");
			item.pszText = _tcsdup(os_pszText3.get_ptr());
			ListView_SetItem(listview_wnd, &item);
			free(item.pszText);
		}
	}
	static void toggle_selected_repeatable()
	{
		int idx = ListView_GetSelectionMark(listview_wnd);
		if (idx != -1)
		{
			LVITEM item;
			ZeroMemory(&item, sizeof(item));
			item.mask = LVIF_PARAM;
			item.iItem = idx;
			ListView_GetItem(listview_wnd, &item);
			
			g_actions.toggle_repeatable_by_idx(item.lParam);
			update_listview(listview_wnd);
			ListView_SetItemState(listview_wnd,idx,LVIS_FOCUSED|LVIS_SELECTED,LVIS_FOCUSED|LVIS_SELECTED);
			ListView_EnsureVisible(listview_wnd,idx,0);
		}
	}
	static void enable_server_controls (bool en)
	{
		EnableWindow (GetDlgItem (config_window,IDC_SERVER_LABEL), en);
		EnableWindow (GetDlgItem (config_window,IDC_SERVER), en);
		EnableWindow (GetDlgItem (config_window,IDC_PORT_LABEL), en);
		EnableWindow (GetDlgItem (config_window,IDC_PORT), en);
		EnableWindow (GetDlgItem (config_window,IDC_CONNECT), en);
	}

	static int ListView_FindItemByKeyName (const char *key)
	{
		LVFINDINFO fi;
		ZeroMemory(&fi,sizeof(fi));
		fi.flags = LVFI_STRING;
		fi.psz = (LPCTSTR) key;
		return ListView_FindItem(listview_wnd,-1,&fi);
	}

	static BOOL CALLBACK ConfigProc(HWND wnd,UINT msg,WPARAM wp,LPARAM lp)
	{
		static HWND status_wnd;

		switch(msg) 
		{
		case WM_INITDIALOG:
			{
				action_listbox_wnd.setHandle(GetDlgItem(wnd,IDC_LB_ACTIONS));
				listview_wnd = GetDlgItem(wnd,IDC_LISTVIEW);
				status_wnd = GetDlgItem(wnd,IDC_STAT);
				config_window = wnd;
				
				{
					LVCOLUMN col;
					ListView_SetExtendedListViewStyle(listview_wnd,
						LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
					
					ZeroMemory(&col, sizeof(col));
					col.mask = LVCF_TEXT;
					col.pszText = TEXT("Button");
					ListView_InsertColumn(listview_wnd, 0, &col);
					col.pszText = TEXT("Action");
					ListView_InsertColumn(listview_wnd, 1, &col);
					col.pszText = TEXT("Ignore repeats");
					ListView_InsertColumn(listview_wnd, 2, &col);

					ListView_SetColumnWidth(listview_wnd, 0, 150);
					ListView_SetColumnWidth(listview_wnd, 1, 230);
					ListView_SetColumnWidth(listview_wnd, 2, 90);
				}
				uSetDlgItemText(wnd, IDC_SERVER, cfg_lircaddr.get_ptr());
				SetDlgItemInt(wnd, IDC_PORT, cfg_lircport,false);
				uSendDlgItemMessage(wnd, IDC_ENABLED, BM_SETCHECK, cfg_enabled, 0);

				if (lirc_socket != INVALID_SOCKET)
				{
					uSetWindowText(status_wnd,
						string_printf("Connected to %s:%d",
						cfg_lircaddr.get_ptr(),(int) cfg_lircport).get_ptr()
						);
					uSetWindowText (GetDlgItem (wnd,IDC_CONNECT),"Disconnect");
				}
				else
				{
					uSetWindowText (GetDlgItem (wnd,IDC_CONNECT),"Connect");
					uSetWindowText(status_wnd,"Not connected.");
				}

				update_action_list();
				update_listview(listview_wnd);

				enable_server_controls (!!cfg_enabled);
			} return 1;
		case WM_NOTIFY:
			switch (((LPNMHDR)lp)->idFrom)
			{
			case IDC_LISTVIEW:
				switch (((LPNMHDR)lp)->code) 
				{
				case NM_DBLCLK:
					toggle_selected_repeatable();
					break;
				case NM_CLICK:
					{
						LVHITTESTINFO info;
						info.pt = ((LPNMLISTVIEW)lp)->ptAction;
						ListView_HitTest (listview_wnd, &info);

						if (info.flags & LVHT_ONITEMLABEL)
						{
							LVITEM item;
							ZeroMemory(&item, sizeof(item));
							item.mask = LVIF_PARAM;
							item.iItem = info.iItem;
							ListView_GetItem(listview_wnd, &item);

							action * a = g_actions[item.lParam];
							if (a)
							{
								uSetDlgItemText (wnd,IDC_E_KEY,a->get_key_name());
								return 0;
							}
						}
						uSetDlgItemText (wnd,IDC_E_KEY,"");

					}
					break;
				}
				break;
			} 
			break;
		case WM_COMMAND:
			switch(wp) 
			{
			case IDC_ENABLED | (BN_CLICKED<<16):
				{
					cfg_enabled = SendDlgItemMessage(wnd, IDC_ENABLED, BM_GETCHECK, 0, 0)==BST_CHECKED;
					if(cfg_enabled)
					{
						init_windows_sockets();
						enable_server_controls (true);
					}
					else
					{
						disconnect_socket();
						enable_server_controls (false);
						deinit_windows_sockets();
					}
				} break;
			case IDC_CONNECT:
				if (lirc_socket != INVALID_SOCKET)
				{
					uSetWindowText (GetDlgItem (wnd,IDC_CONNECT),"Connect");
					disconnect_socket();
				}
				else
				{
					uSetWindowText (status_wnd,
						string_printf("Attempting connection to %s:%d",
							cfg_lircaddr.get_ptr(),(int) cfg_lircport).get_ptr());
					uSetWindowText (GetDlgItem (wnd,IDC_CONNECT),"Disconnect");
					start_lirc();
				}
				break;
			case IDC_B_REPEAT:
				toggle_selected_repeatable();
				break;
			case IDC_B_ADD:
				{
					string8 selected_key,command;
					uGetDlgItemText(wnd,IDC_E_KEY,selected_key);
					if (selected_key.is_empty()) break;

					uTreeView_GetText(action_listbox_wnd.getHandle(),action_listbox_wnd.getSelectedItem(),command);
					g_actions.assign_command_to_key(selected_key,command);
					update_listview(listview_wnd);

					int key_idx = ListView_FindItemByKeyName (selected_key);
					ListView_SetItemState(listview_wnd,key_idx,LVIS_FOCUSED|LVIS_SELECTED,LVIS_FOCUSED|LVIS_SELECTED);
					ListView_EnsureVisible(listview_wnd,key_idx,0);
					SetFocus(listview_wnd);
				} break;
			case IDC_B_REMOVE:
				{
					int idx = ListView_GetSelectionMark(listview_wnd);
					if (idx != -1)
					{
						LVITEM item;
						ZeroMemory(&item, sizeof(item));
						item.mask = LVIF_PARAM;
						item.iItem = idx;
						ListView_GetItem(listview_wnd, &item);

						g_actions.delete_key_by_idx(item.lParam);
						int top_idx = ListView_GetTopIndex (listview_wnd);
						update_listview(listview_wnd);
						ListView_EnsureVisible(listview_wnd,top_idx,0);
						SetFocus(listview_wnd);
					}
				} break;
			case IDC_B_RESET:
				g_actions.reset();
				update_listview(listview_wnd);
				break;
			case IDC_PORT | (EN_KILLFOCUS<<16):
			case IDC_SERVER | (EN_KILLFOCUS<<16):
				{
					string8 tmp;
					uGetWindowText((HWND) lp, tmp);
					if ((wp & IDC_SERVER) == IDC_SERVER)
						cfg_lircaddr = tmp.get_ptr();
					else
						cfg_lircport = atoi(tmp.get_ptr());
				} break;
			case IDC_LB_ACTIONS | (LBN_SELCHANGE<<16):
				{
					string8* desc;
					int idx = uSendMessage(action_listbox_wnd.getHandle(),LB_GETCURSEL,0,0);
					desc = reinterpret_cast<string8*>(uSendMessage(action_listbox_wnd.getHandle(),LB_GETITEMDATA,idx,0));
					if (desc)
						uSetWindowText(GetDlgItem(wnd,IDC_DESC),*desc);
					else
						uSetWindowText(GetDlgItem(wnd,IDC_DESC),"");
				} break;
			} break;
		case WM_REMOTE_KEY:
			{
				const char * key_name = (const char *)wp;
				ListView_SetItemState(listview_wnd,-1,0,LVIS_SELECTED);
					
				/* highlight whatever key was pressed (if it exists in the LV) */
				int idx = ListView_FindItemByKeyName (key_name);
				if (idx != -1)
				{
					ListView_SetItemState(listview_wnd,idx,LVIS_FOCUSED|LVIS_SELECTED,LVIS_FOCUSED|LVIS_SELECTED);
					ListView_EnsureVisible(listview_wnd,idx,true);
					SetFocus(listview_wnd);
				}
				uSetDlgItemText(wnd,IDC_E_KEY,key_name);
			} break;
		case WM_SOCKET:
			switch(WSAGETSELECTEVENT(lp))
			{
			case FD_CONNECT:
				uSetWindowText(status_wnd,
					string_printf("Connected to %s:%d",
						cfg_lircaddr.get_ptr(),(int) cfg_lircport).get_ptr());
				uSetWindowText (GetDlgItem (wnd,IDC_CONNECT),"Disconnect");
				break;
			case FD_CLOSE:
				uSetWindowText(status_wnd,"Not connected.");
				uSetWindowText (GetDlgItem (wnd,IDC_CONNECT),"Connect");
				break;
			}
			break;
		case WM_DESTROY:
			{
				config_window = 0;
				string8 buf;
				uGetDlgItemText(wnd, IDC_SERVER, buf);
				cfg_lircaddr = buf.get_ptr();
				uGetDlgItemText(wnd, IDC_PORT, buf);
				cfg_lircport = atoi(buf.get_ptr());

				string8* desc;
				int num_items = uSendMessage(action_listbox_wnd.getHandle(),LB_GETCOUNT,0,0);
				for (int i=0; i<num_items; i++)
				{
					desc = reinterpret_cast<string8*>(uSendMessage(action_listbox_wnd.getHandle(),LB_GETITEMDATA,i,0));
					delete desc;
				}
			} break;
		}
		return 0;
	}
	virtual HWND create(HWND parent)
	{
		return uCreateDialog(IDD_CONFIG2, parent, ConfigProc, 0);
	}

	virtual const char * get_name()
	{
		return "WinLIRC Client";
	}

	virtual GUID get_guid()
	{
		return guid_prefs_cfg_winlirc;
	}

	virtual GUID get_parent_guid()
	{
		return preferences_page::guid_tools;
	}

	virtual bool reset_query()
	{
		return false;
	}

	virtual void reset()
	{
	}
};

static preferences_page_factory_t<config_winlirc> foo_preferences_page_winlirc;
