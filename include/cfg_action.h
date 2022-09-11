#ifndef _CFG_ACTION_H
#define _CFG_ACTION_H

using namespace pfc;

class action {
	friend class cfg_action;

protected:
	string8 key;
	string8 command;
	bool repeatable;
	int code;

	action () : repeatable (false), code(0) {}

	action(string8 key, string8 command, bool repeatable, int code) 
	{
		this->key = key;
		this->command = command;
		this->repeatable = repeatable; 
		this->code = code;
	}
	action(const action& v) 
	{
		this->key = v.key;
		this->command = v.command; 
		this->repeatable = v.repeatable; 
		this->code = v.code;
	}
	void assign_command(const char * command)
	{
		this->command = command;
	}
	inline void operator=(const action& v) 
	{
		key = v.key;
		command = v.command;
		repeatable = v.repeatable;
		code = v.code;
	}
	void get_data_raw(stream_writer * p_stream,abort_callback & p_abort)
	{
		p_stream->write_string(key.get_ptr(), p_abort);			
		p_stream->write_string(command.get_ptr(), p_abort);
		p_stream->write_lendian_t<bool>(repeatable, p_abort);
		p_stream->write_lendian_t<int>(code, p_abort);
	}
	void set_data_raw(stream_reader * p_stream,t_size p_sizehint,abort_callback & p_abort)
	{
		p_stream->read_string(key, p_abort);
		p_stream->read_string(command, p_abort);
		bool repeatable;
		p_stream->read_lendian_t<bool>(repeatable, p_abort);
		this->repeatable = repeatable;
		int code;
		p_stream->read_lendian_t<int>(code, p_abort);
		this->code = code;
	}
public:
	void get_command (string_base& out) { out.set_string(command); }
	const char * get_command () { return command.get_ptr(); }
	const char * get_key_name () { return key.get_ptr(); }
	int get_custom_code() {	return code; }
	bool get_repeatable () { return repeatable; }
};

class cfg_action : public cfg_var 
{
	ptr_list_t<action> actions;

	virtual void get_data_raw(stream_writer * p_stream,abort_callback & p_abort)
	{
		unsigned n;
		unsigned num = actions.get_count();
		p_stream->write_lendian_t<unsigned>(num, p_abort);
		for(n=0;n<num;n++) 
			actions[n]->get_data_raw(p_stream, p_abort);
	}
	virtual void set_data_raw(stream_reader * p_stream,t_size size,abort_callback & p_abort)
	{
		actions.delete_all();

		unsigned num;
		p_stream->read_lendian_t<unsigned>(num, p_abort);
		for (unsigned i = 0; i<num; i++)
		{
			action * a = new action;
			a->set_data_raw(p_stream, size, p_abort);
			actions.add_item (a);
		}
	}

public:
	cfg_action(const GUID& guid) : cfg_var(guid) {
	}

	~cfg_action() {
		actions.delete_all();
	}

	action* get_action_by_idx(unsigned idx) {
		if (idx >= 0 && idx < actions.get_count()) {
			return actions[idx];
		}
		return 0;
	}

	inline action* operator[](int idx) {
		return get_action_by_idx(idx);
	}

	void toggle_repeatable_by_idx(unsigned idx) {
		action * a = get_action_by_idx(idx);
		if (a) {
			a->repeatable = !a->repeatable;
		}
	}

	void assign_command_to_key(const char * key, const char * command) {
		int n = actions.get_count();
		for (int i = 0; i < n; i++) {
			if (!uStringCompare(actions[i]->key, key))
			{
				actions[i]->assign_command(command);
				return;
			}
		}
		actions.add_item(new action(key, command, false, 0));
	}

	bool process_keypress(const char *key, int repeat_count) {
		int n = actions.get_count();
		for (int i = 0; i < n; i++) {
			console::print(key);
			if (uStringCompare(key,actions[i]->key)) {
				continue;
			}
			if (repeat_count == 0 || actions[i]->repeatable) {
				performCommand(actions[i]->command);
			}
			return true;
		}
		return false;
	}

	void performCommand(const char* command) {
		GUID foundGUID;
		if (mainmenu_commands::g_find_by_name(command, foundGUID)) {
			mainmenu_commands::g_execute(foundGUID);
		}
	}

	void delete_key_by_idx(unsigned idx) {
		actions.delete_by_idx(idx);
	}

	void reset() {
		actions.delete_all();
	}

	int get_count () {
		return actions.get_count();
	}
};

#endif /* _CFG_ACTION_H */
