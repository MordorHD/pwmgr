#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <locale.h>
#include <ncurses.h>
#include "pwmgr.h"

const char *realPath;
char path[1024];
int fdBackup;

#define ATTR_DEFAULT (COLOR_PAIR(0))
#define ATTR_HIGHLIGHT (COLOR_PAIR(0) | A_BOLD)
#define ATTR_SPECIAL (COLOR_PAIR(1))
#define ATTR_ERROR (COLOR_PAIR(2))
#define ATTR_FATAL (COLOR_PAIR(3))
#define ATTR_LOG (COLOR_PAIR(4))

static const struct {
	const char *name;
	const char *description;
	U32 token;	
} dependencies[] = {
	{ "account", "name", TWORD },
	{ "property", "name", TWORD },
	{ "value", "string", TSTRING },
};

struct node {
	const char *name;
	const char *description;
	U32 nSubnodes;
	union {
		const struct node *subnodes;
		void (*proc)(const struct node *node, struct value *values);
	};
};

void help(const struct node *node, struct value *values);
void add_account(const struct node *node, struct value *values);
void add_property(const struct node *node, struct value *values);
void remove_account(const struct node *node, struct value *values);
void remove_backup(const struct node *node, struct value *values);
void info_account(const struct node *node, struct value *values);
void tree(const struct node *node, struct value *values);
void list_account(const struct node *node, struct value *values);
void list_backup(const struct node *node, struct value *values);
void cmd_quit(const struct node *node, struct value *values);
void cmd_clear(const struct node *node, struct value *values);

static const struct node helpNodes[] = {
	{ "help", "shows help for a specific command", 0, .proc = help },
	{ "accounts", "accounts are combinations of data like password username, dob that make up an online presence", 0, .proc = help },
	{ "backup", "backups are local files that store accounts that were once created and even those that were deleted", 0, .proc = help },
	{ "tree", "shows a tree view of a utility command", 0, .proc = help },
};
static const struct node addPropertyAccountNodes[] = {
	{ "value", "set a specific value (\"name: value\")", 0, .proc = add_property },
};
static const struct node addPropertyNodes[] = {
	{ "account", "choose account to set property to", ARRLEN(addPropertyAccountNodes), .subnodes = addPropertyAccountNodes },
};
static const struct node addNodes[] = {
	{ "account", "add an account", 0, .proc = add_account },
	{ "property", "adds a property", ARRLEN(addPropertyNodes), .subnodes = addPropertyNodes },
};
static const struct node removeNodes[] = {
	{ "account", "remove an account from the list of accounts", 0, .proc = remove_account },
	{ "backup", "remove the active backup", 0, .proc = remove_backup },
};
static const struct node infoNodes[] = {
	{ "account", "shows information about an account", 0, .proc = info_account },
};
static const struct node listNodes[] = {
	{ "accounts", "lists all accounts", 0, .proc = list_account },
	{ "backup", "lists all accounts within the backup", 0, .proc = list_backup },
};
static const struct node nodes[] = {
	{ "help", "shows help for a specific command", ARRLEN(helpNodes), .subnodes = helpNodes },
	{ "add", "add an account or property", ARRLEN(addNodes), .subnodes = addNodes },
	{ "remove", "remove an account", ARRLEN(removeNodes), .subnodes = removeNodes },
	{ "info", "show information about an account", ARRLEN(infoNodes), .subnodes = infoNodes },
	{ "tree", "shows a tree view of all commands", 0, .proc = tree },
	{ "list", "shows a specific list", ARRLEN(listNodes), .subnodes = listNodes },
	{ "clear", "clears the screen", 0, .proc = cmd_clear },
	{ "quit", "quit the program", 0, .proc = cmd_quit },
	{ "exit", "exit the program (same as quit)", 0, .proc = cmd_quit },
};
static const struct node root[] = {
	{ NULL, NULL, ARRLEN(nodes), .subnodes = nodes },
};

static void
appendrealpath(const char *app, U32 nApp)
{
	U32 at;

	strcpy(path, realPath);
	at = strlen(path);
	path[at++] = '/';
	memcpy(path + at, app, nApp);
	at += nApp;
	path[at] = 0;
}

void
add_account(const struct node *node, struct value *values)
{
	char *name;
	U32 nName;
	int fd;

	name = values[0].word;
	nName = values[0].nWord;
	appendrealpath(name, nName);
	if(!access(path, F_OK))
	{
		attrset(ATTR_ERROR);
		printw("\nAccount '%.*s' already exists", nName, name);
		return;
	}
	fd = open(path, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if(fd == ERR)
	{
		attrset(ATTR_ERROR);
		printw("\nUnable to create the file inside '%s' (%s)", realPath, strerror(errno));
		return;
	}
	close(fd);
	attrset(ATTR_LOG);
	printw("\nCreated new account inside '%s'", path);
}

void
add_property(const struct node *node, struct value *values)
{
	char *propName;
	U32 nPropName;
	char *accName;
	U32 nAccName;
	int fd;
	ssize_t nRead;

	propName = values[0].word;
	nPropName = values[0].nWord;
	accName = values[1].word;
	nAccName = values[1].nWord;
	appendrealpath(accName, nAccName);
	fd = open(path, O_RDWR | O_APPEND);
	if(fd == ERR)
	{
		attrset(ATTR_ERROR);
		printw("\nUnable to access '%s' (%s)", path, strerror(errno));
		return;
	}
	path[sizeof(path) - 1] = 0;
	nRead = read(fd, path, sizeof(path) - 1);
	while(nRead > 0)
	{
		char *name;
		U32 nName;
		U32 nValue;

		name = path;
		nName = strlen(path);
		if(nName > nRead)
		{
			attrset(ATTR_ERROR);
			printw("\nFile '%s/%.*s' is corrupt (state: 0)", realPath, nAccName, accName);
			return;
		}
		if(nName == nPropName && !memcmp(name, propName, nPropName))
		{
			attrset(ATTR_ERROR);
			printw("\nProperty '%.*s' already exists", nPropName, propName);
			return;
		}
		nRead -= nName + 1;
		memcpy(path, path + nName + 1, nRead);
		nRead += read(fd, path + nRead, sizeof(path) - 1 - nRead);
		if(!nRead)
		{
			attrset(ATTR_ERROR);
			printw("\nFile '%s/%.*s' is corrupt (state: 1)", realPath, nAccName, accName);
			return;
		}
		nValue = strlen(path);
		while(nValue == sizeof(path) - 1)
		{
			nRead = read(fd, path, sizeof(path) - 1);
			nValue = strlen(path);
			if(nValue > nRead)
			{
				attrset(ATTR_ERROR);
				printw("\nFile '%s/%.*s' is corrupt (state: 2)", realPath, nAccName, accName);
				return;
			}
		}
		nRead -= nValue + 1;
		memcpy(path, path + nValue + 1, nRead);
		nRead += read(fd, path + nRead, sizeof(path) - 1 - nRead);
	}
	write(fd, propName, nPropName);
	write(fd, &(char) { 0 }, 1);
	write(fd, values[2].string, values[2].nString);
	write(fd, &(char) { 0 }, 1);
	close(fd);
	attrset(ATTR_LOG);
	printw("\nWritten '%.*s' to account '%.*s'", values[2].nString, values[2].string, nAccName, accName);
}

void
remove_property(const struct node *node, struct value *value)
{
	int fd;
	int fdTmp;


}

void
remove_account(const struct node *node, struct value *values)
{
	char *name;
	U32 nName;

	name = values[0].word;
	nName = values[0].nWord;
	appendrealpath(name, nName);
	if(remove(path))
	{
		attrset(ATTR_ERROR);
		printw("\nCouldn't remove account '%.*s'", nName, name);
	}
	else
	{
		attrset(ATTR_LOG);
		printw("\nSuccessfully removed account '%.*s'", nName, name);
	}
}

void
remove_backup(const struct node *node, struct value *values)
{
}

void
info_account(const struct node *node, struct value *values)
{
	char *accName;
	U32 nAccName;
	int fd;
	ssize_t nRead;

	accName = values[0].word;
	nAccName = values[0].nWord;
	appendrealpath(accName, nAccName);
	fd = open(path, O_RDONLY);
	if(fd == ERR)
	{
		attrset(ATTR_ERROR);
		printw("\nCouldn't open account '%.*s' ('%s')", nAccName, accName, strerror(errno));
		return;
	}
	attrset(ATTR_LOG);
	path[sizeof(path) - 1] = 0;
	nRead = read(fd, path, sizeof(path) - 1);
	while(nRead > 0)
	{
		char *name;
		U32 nName;
		U32 nValue;

		name = path;
		nName = strlen(path);
		if(nName > nRead)
		{
			attrset(ATTR_ERROR);
			printw("\nFile '%s/%.*s' is corrupt (state: 0)", realPath, nAccName, accName);
			return;
		}
		printw("\n%.*s = ", nName, name);
		nRead -= nName + 1;
		memcpy(path, path + nName + 1, nRead);
		nRead += read(fd, path + nRead, sizeof(path) - 1 - nRead);
		if(!nRead)
		{
			attrset(ATTR_ERROR);
			printw("\nFile '%s/%.*s' is corrupt (state: 1)", realPath, nAccName, accName);
			return;
		}
		nValue = strlen(path);
		printw("%.*s", nValue, path);
		while(nValue == sizeof(path) - 1)
		{
			nRead = read(fd, path, sizeof(path) - 1);
			nValue = strlen(path);
			printw("%.*s", nValue, path);
			if(nValue > nRead)
			{
				attrset(ATTR_ERROR);
				printw("\nFile '%s/%.*s' is corrupt (state: 2)", realPath, nAccName, accName);
				return;
			}
		}
		nRead -= nValue + 1;
		memcpy(path, path + nValue + 1, nRead);
		nRead += read(fd, path + nRead, sizeof(path) - 1 - nRead);
	}
	close(fd);
}

void
help(const struct node *node, struct value *values)
{
	attrset(ATTR_HIGHLIGHT);
	printw("\n%s", node->name);
	attrset(ATTR_DEFAULT);
	printw("\n\t%s", node->description);
}

void
tree_print(const struct node *branch, U32 depth)
{
	for(const struct node *s = branch->subnodes, *e = s + branch->nSubnodes; s != e; s++)
	{
		if(branch->nSubnodes == 1)
			printw(" ");
		else
		{
			printw("\n");
			for(U32 i = 0; i < depth; i++)
				printw(" |");
		}
		attrset(A_BOLD);
		printw("%s", s->name);
		for(U32 i = 0; i < ARRLEN(dependencies); i++)
			if(!strcmp(dependencies[i].name, s->name))
			{
				attron(A_ITALIC);
				printw(" %s", dependencies[i].description);
				break;
			}
		attrset(0);
		if(s->nSubnodes)
			tree_print(s, depth + 1);
		else
			printw(" %s", s->description);
	}
}

void
tree(const struct node *node, struct value *values)
{
	tree_print(root, 0);
}

void
list_account(const struct node *node, struct value *values)
{
	DIR *dir;
	struct dirent *dirent;

	dir = opendir(realPath);
	if(!dir)
		return;
	attrset(ATTR_LOG);
	while((dirent = readdir(dir)))
		if(dirent->d_type == DT_REG && strcmp(dirent->d_name, ".backup"))
			printw("\n\t%s", dirent->d_name);
	closedir(dir);
}

void list_backup(const struct node *node, struct value *values)
{
}

void
cmd_quit(const struct node *node, struct value *values)
{
	endwin();
	exit(0);
}

void
cmd_clear(const struct node *node, struct value *values)
{
	clear();
}

int
main(void)
{
	bool isUtf8;
	char *locale;
	const char * const homePath = getenv("HOME");
	struct input input;

	memset(&input, 0, sizeof(input));
	input.tokenizer.line = input.buf;
	locale = setlocale(LC_ALL, "");

	initscr();

	raw();
	noecho();
	
	keypad(stdscr, true);
	scrollok(stdscr, true);

	start_color();
	init_pair(1, COLOR_GREEN, COLOR_BLACK);
	init_pair(2, COLOR_MAGENTA, COLOR_BLACK);
	init_pair(3, COLOR_RED, COLOR_BLACK);
	init_pair(4, COLOR_CYAN, COLOR_BLACK);

	attrset(ATTR_LOG);
	printw("Starting setup...");

	if(!homePath)
	{
		attrset(ATTR_ERROR);
		printw("\nSetup failed: Enviroment variable HOME is not set!");
		goto err;
	}
	printw("\nHome path is '%s'", homePath);
	strcpy(path, homePath);
	strcat(path, "/Passwords");
	realPath = realpath(path, NULL);
	printw("\nThe real path is '%s'", realPath);
	if(mkdir(realPath, 0700))
	{
		if(errno == EACCES)
		{
			attrset(ATTR_ERROR);
			printw("\nSetup failed: Write permission denied!");
			goto err;
		}
		else if(errno != EEXIST)
		{
			attrset(ATTR_ERROR);
			printw("\nSetup failed: Could not open real path directory!");
			goto err;
		}
	}
	appendrealpath(".backup", sizeof(".backup") - 1);
	printw("\nOpening backup file '%s'...", path);
	fdBackup = open(path, O_CREAT | O_APPEND | O_WRONLY, S_IWUSR | S_IRUSR); 
	if(fdBackup == ERR)
	{
		attrset(ATTR_FATAL);
		printw("\nCouldn't open backup file");
		attrset(ATTR_LOG);
	}
	else
	{
		printw("\nBackup file successfully opened");
	}
	printw("\nChecking for UTF-8 support...");
	isUtf8 = locale && strstr(locale, "UTF-8");
	printw("\n%s", isUtf8 ? "UTF-8 is supported" : "UTF-8 is not supported");
	printw("\nSetup complete!\n");
	attrset(ATTR_SPECIAL);
	printw("\nPassword manager unstable version 1");
	list_account(NULL, NULL);
	// osu\0[4 bytes]firefox\0[4 bytes]gmx\0[4 bytes]
	// [4 bytes]value\0...
	//
	// [byte]
	// - add account [name]\0
	// - add property [name]\0[name]\0[value]\0
	while(1)
	{
		TOKEN *tok;
		const struct node *branch;
		const struct node *newBranch;
		struct value values[10];
		U32 nValues = 0;

		void printpossibilities(void)
		{
			attrset(ATTR_DEFAULT);
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

		attrset(ATTR_DEFAULT);
		printw("\n");
		getinput(&input, isUtf8);
		printw("\n%s", input.buf);
		if(tokenize(&input.tokenizer))
		{
			attrset(ATTR_ERROR);
			printw("\n");
			for(U32 i = 0; i < input.tokenizer.errPos; i++)
				printw(" ");
			printw("^\nInvalid token");
			continue;
		}
		printw("\n%u", input.tokenizer.nTokens);
		for(U32 i = 0; i < input.tokenizer.nTokens; i++)
			printw("\n%u, %u", input.tokenizer.tokens[i].pos, input.tokenizer.tokens[i].type);
		if(!input.tokenizer.nTokens)
			continue;
		branch = root;
		while((tok = nexttoken(&input.tokenizer)))
		{
			const char *word;
			U32 l = 0;
			U32 i;

			switch(tok->type)
			{
			case TWORD: word = input.buf + tok->pos; break;
			case TPLUS: word = "add"; break;
			case TMINUS: word = "remove"; break;	
			case TCOLON: word = "account"; break;
			case THASH: word = "info"; break;
			case TEQU: word = "value"; break;
			case TAT: word = "property"; break;
			case TQUESTION: word = "info"; break;
			default:
				attrset(ATTR_ERROR);
				printw("\n");
				for(U32 i = 0; i < tok->pos; i++)
					printw(" ");
				printw("^\nInvalid token");
				word = NULL;
			}
			if(!word)
			{
				branch = NULL;
				break;
			}
			while(isalpha(word[l]))
				l++;
			for(i = 0; i < ARRLEN(dependencies); i++)
				if(strlen(dependencies[i].name) == l && !memcmp(dependencies[i].name, word, l))
				{
					if(!(tok = nexttoken(&input.tokenizer)) || tok->type != dependencies[i].token)
					{
						attrset(ATTR_ERROR);
						printw("\nExpected %s after '%.*s'", dependencies[i].description, l, word);
						i = -1;
						branch = NULL;
						break;
					}
					toktovalue(&input.tokenizer, tok - input.tokenizer.tokens, values + nValues++);
					break;
				}
			if((I32) i == -1)
				break;
			newBranch = NULL;
			for(i = 0; i < branch->nSubnodes; i++)
			{
				if(strlen(branch->subnodes[i].name) == l && !memcmp(branch->subnodes[i].name, word, l))
				{
					newBranch = branch->subnodes + i;
					break;
				}
			}
			if(!newBranch)
			{
				attrset(ATTR_ERROR);
				if(branch->name)
					printw("\nBranch '%s' doesn't have the option '%.*s'", branch->name, l, word);
				else
					printw("\nBranch '%.*s' doesn't exist", l, word);
				printpossibilities();
				branch = NULL;
				break;
			}
			branch = newBranch;
			if(!branch->nSubnodes)
			{
				branch->proc(branch, values);
				branch = NULL;
				break;
			}
		}
		if(branch)
		{
			attrset(ATTR_ERROR);
			printw("\nNeed more options for branch '%s' (use 'tree' to get an overview of the commands)", branch->name);
			printpossibilities();
		}
	}
err:
	attrset(ATTR_FATAL);
	printw("\nAn unexpected error occured, press any key to exit...");
	getch();
	endwin();
	return ERR;
}
