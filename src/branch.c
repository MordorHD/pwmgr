#include "pwmgr.h"

// these are all defined inside of src/main.c
void help(const struct branch *branch, struct input *input);
void set(const struct branch *branch, struct value *values);
void add_account(const struct branch *branch, struct value *values);
void add_property(const struct branch *branch, struct value *values);
void backup_edit(const struct branch *branch, struct value *values){}
void backup_undo(const struct branch *branch, struct value *values){}
void backup_redo(const struct branch *branch, struct value *values){}
void remove_account(const struct branch *branch, struct value *values);
void remove_property(const struct branch *branch, struct value *values);
void remove_backup(const struct branch *branch, struct value *values);
void info_account(const struct branch *branch, struct value *values);
void info_backup(const struct branch *branch, struct value *values);
void tree(const struct branch *branch, struct value *values);
void list_account(const struct branch *branch, struct value *values);
void cmd_quit(const struct branch *branch, struct value *values);
void cmd_clear(const struct branch *branch, struct value *values);

static const struct branch setNodes[] = {
	{ "value", "possible values are", 0, .proc = set },
};
static const struct branch addPropertyAccountNodes[] = {
	{ "value", "set a specific value (\"value\")", 0, .proc = add_property },
};
static const struct branch addNodes[] = {
	{ "account", "add an account", 0, .proc = add_account },
};
static const struct branch removePropertyNodes[] = {
	{ "account", "choose an account to remove the property from", 0, .proc = remove_property },
};
static const struct branch removeNodes[] = {
	{ "account", "remove an account from the list of accounts", 0, .proc = remove_account },
	{ "backup", "remove the active backup", 0, .proc = remove_backup },
	{ "property", "remove the active backup", ARRLEN(removePropertyNodes), .subnodes = removePropertyNodes},
};
static const struct branch listNodes[] = {
	{ "accounts", "lists all accounts", 0, .proc = list_account },
};
static const struct branch accountNodes[] = {
	{ "show", "shows data of given account", 0, .proc = list_account },
	//{ "property", "adds a property", ARRLEN(accountPropertyNodes), .subnodes = accountPropertyNodes },
};
static const struct branch backupNodes[] = {
	{ "edit", "directly edit the backup file", 0, .proc = backup_edit },
	{ "undo", "undoes the last operation in the backup file", 0, .proc = backup_undo },
	{ "redo", "redoes the last undone action", 0, .proc = backup_redo },
};
static const struct branch nodes[] = {
	{ "help", "shows help for a specific command", -1, .special = help },
	{ "set", "set a system variable (options are: area)", ARRLEN(setNodes), .subnodes = setNodes },
	{ "add", "add an account", ARRLEN(addNodes), .subnodes = addNodes },
	{ "remove", "remove an account", ARRLEN(removeNodes), .subnodes = removeNodes },
	{ "list", "shows a specific list", ARRLEN(listNodes), .subnodes = listNodes },
	{ "tree", "shows a tree view of all commands", 0, .proc = tree },
	{ "account", "access account file", ARRLEN(accountNodes), .subnodes = accountNodes },
	{ "backup", "access the backup file", ARRLEN(backupNodes), .subnodes = backupNodes },
	{ "clear", "clears the screen", 0, .proc = cmd_clear },
	{ "quit", "quit the program", 0, .proc = cmd_quit },
	{ "exit", "exit the program (same as quit)", 0, .proc = cmd_quit },
};
static const struct branch _root[] = {
	{ NULL, NULL, ARRLEN(nodes), .subnodes = nodes },
};
const struct branch *const root = _root;

void
printoptions(const struct branch *branch)
{
	wattrset(out, ATTR_LOG);
	wprintw(out, "\nPossible options are:");
	for(U32 i = 0; i < branch->nSubnodes; i++)
	{
		wattrset(out, ATTR_HIGHLIGHT);
		wprintw(out, "\n\t%s", branch->subnodes[i].name);
		for(U32 i = 0; i < ARRLEN(dependencies); i++)
			if(!strcmp(dependencies[i].name, branch->subnodes[i].name))
			{
				wattron(out, A_ITALIC);
				wprintw(out, " %s", dependencies[i].description);
				break;
			}
		wattrset(out, ATTR_DEFAULT);
		wprintw(out, "\t%s", branch->subnodes[i].description);
	}
}

const struct branch *
nextbranch(const struct branch *branch, struct input *input)
{
	TOKEN *tok;
	struct value value;
	const struct branch *newBranch = NULL; const char *word; U32 nWord;

	if(IS_EXEC_BRANCH(branch) || !(tok = nexttoken(input, &value)))
		return NULL;
	switch(tok->type)
	{
	case TWORD: word = value.word; nWord = value.nWord; break;
	case TPLUS: word = "add"; nWord = 3; break;
	case TMINUS: word = "remove"; nWord = 6; break;	
	case TCOLON: word = "account"; nWord = 7; break;
	case TQUESTION: word = "info"; nWord = 4; break;
	case TEQU: word = "value"; nWord = 5; break;
	case TAT: word = "property"; nWord = 8; break;
	case TDOT: word = "set"; nWord = 3; break;
	default: return NULL;
	}
	for(U32 i = 0; i < branch->nSubnodes; i++)
	{
		if(strlen(branch->subnodes[i].name) == nWord && !memcmp(branch->subnodes[i].name, word, nWord))
		{
			newBranch = branch->subnodes + i;
			break;
		}
	}
	if(!newBranch)
	{
		wattrset(out, ATTR_ERROR);
		if(branch->name)
			wprintw(out, "\nBranch '%s' doesn't have the option '%.*s'", branch->name, nWord, word);
		else
			wprintw(out, "\nBranch '%.*s' doesn't exist", nWord, word);
		printoptions(branch);
		return NULL;
	}
	return newBranch;
}
