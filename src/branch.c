#include "pwmgr.h"

void void_proc(const struct branch *branch, struct value *values)
{
	printw("\nThis command has no specific function");
}
// these are all defined inside of src/main.c
void help(const struct branch *branch, struct input *input);
void add_account(const struct branch *branch, struct value *values);
void add_property(const struct branch *branch, struct value *values);
void remove_account(const struct branch *branch, struct value *values);
void remove_property(const struct branch *branch, struct value *values);
void remove_backup(const struct branch *branch, struct value *values);
void info_account(const struct branch *branch, struct value *values);
void info_backup(const struct branch *branch, struct value *values);
void tree(const struct branch *branch, struct value *values);
void list_account(const struct branch *branch, struct value *values);
void cmd_quit(const struct branch *branch, struct value *values);
void cmd_clear(const struct branch *branch, struct value *values);

static const struct branch addPropertyAccountNodes[] = {
	{ "value", "set a specific value (\"value\")", 0, .proc = add_property },
};
static const struct branch addPropertyNodes[] = {
	{ "account", "choose account to set property to", ARRLEN(addPropertyAccountNodes), .subnodes = addPropertyAccountNodes },
};
static const struct branch addNodes[] = {
	{ "account", "add an account", 0, .proc = add_account },
	{ "property", "adds a property", ARRLEN(addPropertyNodes), .subnodes = addPropertyNodes },
};
static const struct branch removePropertyNodes[] = {
	{ "account", "choose an account to remove the property from", 0, .proc = remove_property },
};
static const struct branch removeNodes[] = {
	{ "account", "remove an account from the list of accounts", 0, .proc = remove_account },
	{ "backup", "remove the active backup", 0, .proc = remove_backup },
	{ "property", "remove the active backup", ARRLEN(removePropertyNodes), .subnodes = removePropertyNodes},
};
static const struct branch infoNodes[] = {
	{ "account", "shows information about an account", 0, .proc = info_account },
	{ "backup", "shows all edits within the backup", 0, .proc = info_backup },
};
static const struct branch listNodes[] = {
	{ "accounts", "lists all accounts", 0, .proc = list_account },
};
static const struct branch nodes[] = {
	{ "help", "shows help for a specific command", -1, .special = help },
	{ "add", "add an account or property", ARRLEN(addNodes), .subnodes = addNodes },
	{ "remove", "remove an account", ARRLEN(removeNodes), .subnodes = removeNodes },
	{ "info", "show information about an account", ARRLEN(infoNodes), .subnodes = infoNodes },
	{ "list", "shows a specific list", ARRLEN(listNodes), .subnodes = listNodes },
	{ "tree", "shows a tree view of all commands", 0, .proc = tree },
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
	attrset(ATTR_LOG);
	printw("\nPossible options are:");
	for(U32 i = 0; i < branch->nSubnodes; i++)
	{
		attrset(ATTR_HIGHLIGHT);
		printw("\n\t%s", branch->subnodes[i].name);
		for(U32 i = 0; i < ARRLEN(dependencies); i++)
			if(!strcmp(dependencies[i].name, branch->subnodes[i].name))
			{
				attron(A_ITALIC);
				printw(" %s", dependencies[i].description);
				break;
			}
		attrset(ATTR_DEFAULT);
		printw("\t%s", branch->subnodes[i].description);
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
	default:
		attrset(ATTR_ERROR);
		addch('\n');
		for(U32 i = 0; i < tok->pos; i++)
			addch(' ');
		addstr("^\nInvalid token");
		return NULL;
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
		attrset(ATTR_ERROR);
		if(branch->name)
			printw("\nBranch '%s' doesn't have the option '%.*s'", branch->name, nWord, word);
		else
			printw("\nBranch '%.*s' doesn't exist", nWord, word);
		printoptions(branch);
		return NULL;
	}
	return newBranch;
}
