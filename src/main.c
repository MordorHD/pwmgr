#define _GNU_SOURCE
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <locale.h>
#include "pwmgr.h"

#define VERSION "Unstable Version 1"

// location of the main directory
const char *realPath;
// the path size must be at least
// 1 + sizeof(time_t) + 2 * MAX_NAME + 2
// bytes large
char path[1024];
// open backup file
int fdBackup;

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
add_account(const struct branch *branch, struct value *values)
{
	char *name;
	U32 nName;
	int fd;

	name = values[0].word;
	nName = values[0].nWord;
	if(nName > MAX_NAME)
	{
		attrset(ATTR_ERROR);
		printw("\nAccount name is not allowed to exceed %u bytes", MAX_NAME);
		return;
	}
	appendrealpath(name, nName);
	if(!access(path, F_OK))
	{
		attrset(ATTR_ERROR);
		printw("\nAccount '%.*s' already exists", nName, name);
		return;
	}
	fd = open(path, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
	if(fd == ERR)
	{
		attrset(ATTR_ERROR);
		printw("\nUnable to create file inside '%s' (%s)", realPath, strerror(errno));
		return;
	}
	close(fd);
	attrset(ATTR_LOG);
	printw("\nCreated new account inside '%s'", path);
	write(fdBackup, &(char) { BACKUP_ENTRY_ADDACCOUNT }, 1);
	write(fdBackup, &(time_t) { time(NULL) }, sizeof(time_t));
	write(fdBackup, name, nName);
	write(fdBackup, &(char) { 0 }, 1);
}

void
add_property(const struct branch *branch, struct value *values)
{
	char *propName;
	U32 nPropName;
	char *accName;
	U32 nAccName;
	int fd;
	ssize_t nRead;

	propName = values[0].word;
	nPropName = values[0].nWord;
	if(nPropName > MAX_NAME)
	{
		attrset(ATTR_ERROR);
		printw("\nProperty name is not allowed to exceed %u bytes", MAX_NAME);
		return;
	}
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
			close(fd);
			return;
		}
		if(nName == nPropName && !memcmp(name, propName, nPropName))
		{
			attrset(ATTR_ERROR);
			printw("\nProperty '%.*s' already exists", nPropName, propName);
			close(fd);
			return;
		}
		nRead -= nName + 1;
		memcpy(path, path + nName + 1, nRead);
		nRead += read(fd, path + nRead, sizeof(path) - 1 - nRead);
		if(!nRead)
		{
			attrset(ATTR_ERROR);
			printw("\nFile '%s/%.*s' is corrupt (state: 1)", realPath, nAccName, accName);
			close(fd);
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
				close(fd);
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
	write(fdBackup, &(char) { BACKUP_ENTRY_ADDPROPERTY }, 1);
	write(fdBackup, &(time_t) { time(NULL) }, sizeof(time_t));
	write(fdBackup, propName, nPropName);
	write(fdBackup, &(char) { 0 }, 1);
	write(fdBackup, accName, nAccName);
	write(fdBackup, &(char) { 0 }, 1);
	write(fdBackup, values[2].string, values[2].nString);
	write(fdBackup, &(char) { 0 }, 1);
}

void
remove_property(const struct branch *branch, struct value *values)
{
	char *propName;
	U32 nPropName;
	char *accName;
	U32 nAccName;
	int fd;
	ssize_t nRead;
	int fdTmp;
	bool removed = false;
	char tmpPath[sizeof(path)];

	propName = values[0].word;
	nPropName = values[0].nWord;
	accName = values[1].word;
	nAccName = values[1].nWord;
	appendrealpath(accName, nAccName);
	fd = open(path, O_RDONLY);
	if(fd == ERR)
	{
		attrset(ATTR_ERROR);
		printw("\nUnable to open account '%.*s' (%s)", nAccName, accName, strerror(errno));
		return;
	}
	appendrealpath(".tmp", 4);
	fdTmp = open(path, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
	if(fdTmp == ERR)
	{
		attrset(ATTR_FATAL);
		printw("\nUnable to create temporary file (%s)", strerror(errno));
		close(fd);
		return;
	}
	strcpy(tmpPath, path);

	// write all properties besides the property which should be removed
	path[sizeof(path) - 1] = 0;
	nRead = read(fd, path, sizeof(path) - 1);
	while(nRead > 0)
	{
		char *name;
		U32 nName;
		U32 nValue;
		bool ignoreWrite = false;

		name = path;
		nName = strlen(path);
		if(nName > nRead)
			goto corrupt;
		if(nName == nPropName && !memcmp(name, propName, nPropName))
		{
			attrset(ATTR_ERROR);
			removed = true;
			ignoreWrite = true;	
		}
		if(!ignoreWrite)
			write(fdTmp, name, nName + 1);
		nRead -= nName + 1;
		memcpy(path, path + nName + 1, nRead);
		nRead += read(fd, path + nRead, sizeof(path) - 1 - nRead);
		if(!nRead)
			goto corrupt;
		nValue = strlen(path);
		if(!ignoreWrite)
			write(fdTmp, path, nValue);
		while(nValue == sizeof(path) - 1)
		{
			nRead = read(fd, path, sizeof(path) - 1);
			nValue = strlen(path);
			if(nValue > nRead)
				goto corrupt;
			if(!ignoreWrite)
				write(fdTmp, path, nValue);
		}
		if(!ignoreWrite)
			write(fdTmp, &(char) { 0 }, 1);
		nRead -= nValue + 1;
		memcpy(path, path + nValue + 1, nRead);
		nRead += read(fd, path + nRead, sizeof(path) - 1 - nRead);
	}
	close(fd);
	if(!removed)
	{
		attrset(ATTR_ERROR);
		printw("\nProperty '%.*s' doesn't exist", nPropName, propName);
		return;
	}
	appendrealpath(accName, nAccName);
	if(renameat2(AT_FDCWD, tmpPath, AT_FDCWD, path, RENAME_EXCHANGE))
	{
		attrset(ATTR_FATAL);
		printw("\nFailed atomically swapping temporary file and new file (%s)", strerror(errno));
	}
	close(fdTmp);
	remove(tmpPath);
	attrset(ATTR_LOG);
	printw("\nRemoved property '%.*s' from account '%.*s'", nPropName, propName, nAccName, accName);
	write(fdBackup, &(char) { BACKUP_ENTRY_REMOVEPROPERTY }, 1);
	write(fdBackup, &(time_t) { time(NULL) }, sizeof(time_t));
	write(fdBackup, propName, nPropName);
	write(fdBackup, &(char) { 0 }, 1);
	write(fdBackup, accName, nAccName);
	write(fdBackup, &(char) { 0 }, 1);
	return;
corrupt:
	close(fdTmp);
	remove(tmpPath);
	close(fd);
	attrset(ATTR_FATAL);
	printw("\nFile '%s/%.*s' is corrupt", realPath, nAccName, accName);
}

void
remove_account(const struct branch *branch, struct value *values)
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
		write(fdBackup, &(char) { BACKUP_ENTRY_REMOVEACCOUNT }, 1);
		write(fdBackup, &(time_t) { time(NULL) }, sizeof(time_t));
		write(fdBackup, name, nName);
		write(fdBackup, &(char) { 0 }, 1);
	}
}

void
remove_backup(const struct branch *branch, struct value *values)
{
	int ans;

	attrset(ATTR_LOG);
	printw("\nAre you sure you want to remove the backup? [Yn]");
	ans = getch();
	if(ans != 'Y')
	{
		printw("\nCancelled removal of backup");
		return;
	}
	appendrealpath(".backup", sizeof(".backup") - 1);
	if(remove(path))
	{
		attrset(ATTR_ERROR);
		printw("\nFailed to remove backup (%s)", strerror(errno));
	}
	else
	{
		attrset(ATTR_LOG);
		printw("\nBackup was removed");
	}
}

void
info_account(const struct branch *branch, struct value *values)
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
			goto corrupt;
		printw("\n%.*s = ", nName, name);
		nRead -= nName + 1;
		memcpy(path, path + nName + 1, nRead);
		nRead += read(fd, path + nRead, sizeof(path) - 1 - nRead);
		if(!nRead)
			goto corrupt;
		nValue = strlen(path);
		printw("%.*s", nValue, path);
		while(nValue == sizeof(path) - 1)
		{
			nRead = read(fd, path, sizeof(path) - 1);
			nValue = strlen(path);
			if(nValue > nRead)
				goto corrupt;
			printw("%.*s", nValue, path);
		}
		nRead -= nValue + 1;
		memcpy(path, path + nValue + 1, nRead);
		nRead += read(fd, path + nRead, sizeof(path) - 1 - nRead);
	}
	close(fd);
	return;
corrupt:
	close(fd);
	attrset(ATTR_ERROR);
	printw("\nFile '%s/%.*s' is corrupt", realPath, nAccName, accName);
}

void info_backup(const struct branch *branch, struct value *values)
{
	ssize_t nRead;
	char *ptr;

	if(lseek(fdBackup, 0, SEEK_SET))
	{
		attrset(ATTR_ERROR);
		printw("\nUnable to read backup file (%s)", strerror(errno));
		return;
	}
	path[sizeof(path) - 1] = 0;
	nRead = read(fdBackup, path, sizeof(path) - 1);
	while(nRead > 0)
	{
		U32 l;
		U8 id;
		time_t time;
		struct tm *tm;

		ptr = path;
		id = *(ptr++);
		time = *(time_t*) ptr;
		tm = localtime(&time);
		ptr += sizeof(time_t);
		nRead -= sizeof(time_t) + 1;
		l = strlen(path);
		if(l >= nRead)
		{
			printw("\nCorrupt backup file, you must manually fix it ('help backup fix' for more info)");
			break;
		}
		l++;
		nRead -= l;
		switch(id)
		{
		case BACKUP_ENTRY_ADDACCOUNT:
			attrset(ATTR_ADD);
			printw("\nAdded account '%s'", ptr);
			ptr += l;
			break;
		case BACKUP_ENTRY_REMOVEACCOUNT:
			attrset(ATTR_SUB);
			printw("\nRemoved account '%s'", ptr);
			ptr += l;
			break;
		case BACKUP_ENTRY_ADDPROPERTY:
			attrset(ATTR_ADD);
			printw("\nAdded property '%s'", ptr);
			ptr += l;
			printw(" from account '%s'", ptr);
			nRead -= strlen(path) + 1;
			ptr += strlen(path) + 1;
			printw(" with value '%s'", ptr);
			break;
		case BACKUP_ENTRY_REMOVEPROPERTY:
			attrset(ATTR_SUB);
			printw("\nRemoved property '%s'", ptr);
			ptr += l;
			printw(" from account '%s'", ptr);
			nRead -= strlen(path) + 1;
			ptr += strlen(path) + 1;
			break;
		}
		attrset(ATTR_LOG);
		printw("\t\t%s", asctime(tm));
		memmove(path, ptr, nRead);
		nRead += read(fdBackup, path + nRead, sizeof(path) - 1 - nRead);
	}
}

void
help(const struct branch *helpBranch, struct input *input)
{
	static const struct {
		const char *name;
		const char *info;
	} general_infos[] = {
		{ "accounts", "accounts are combinations of data like password username, dob that make up an online presence" },
		{ "backups", "backups are local files that store all actions you performs" },
		{ "tree", "shows a tree view of all commands" },
	};
	const struct branch *branch;
	struct value value;
	TOKEN *tok;

	/*static const struct branch helpBackupNodes[] = {
		{ "fix", "if the backup file is corrupt; it may have occured because a write was interrupted"
			"\nThis might've been caused by a power outage"
			"\nTo manually manage the backup and fix it, use the 'backup ...' commands", 0, .proc = void_proc },
	};*/
	if((tok = peektoken(input, &value)) && tok->type == TWORD)
	{
		for(U32 i = 0; i < ARRLEN(general_infos); i++)
			if(!strncmp(general_infos[i].name, value.word, value.nWord) &&
					!general_infos[i].name[value.nWord])
			{
				attrset(ATTR_LOG);
				printw("\nINFO: %s", general_infos[i].info);
				return;
			}
	}
	branch = root;
	while(hasnexttoken(input) && (branch = nextbranch(branch, input)));
	if(branch)
	{
		if(!branch->description)
		{ // this means we stayed in root
			attrset(ATTR_LOG);
			addstr("\nPassword manager " VERSION);
			attrset(ATTR_DEFAULT);
			addstr("\nUse this as a manager for your accounts; you can do that by entering commands like help."
					" Commands are based on a 'branch' system, meaning a series of commands follows a specific branch; type 'tree' to visualize the available command tree."
					" Some commands also expect tokens right after it, for instance 'account' needs a name(word) argument."
					"\nFor more information put any of these words afer 'help'"
				);
			for(U32 i = 0; i < ARRLEN(general_infos); i++)
			{
				attrset(ATTR_DEFAULT);
				addstr("\n\thelp ");
				attrset(ATTR_HIGHLIGHT);
				addstr(general_infos[i].name);
			}

		}
		else
		{
			attrset(ATTR_LOG);
			printw("\n%s", branch->description);
		}
	}
}

void
tree_print(const struct branch *branch, U32 depth)
{
	for(const struct branch *s = branch->subnodes, *e = s + branch->nSubnodes; s != e; s++)
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
		attrset(ATTR_DEFAULT);
		if(!IS_EXEC_BRANCH(s))
			tree_print(s, depth + 1);
		else
			printw(" %s", s->description);
	}
}

void
tree(const struct branch *branch, struct value *values)
{
	tree_print(root, 0);
}

void
list_account(const struct branch *branch, struct value *values)
{
	DIR *dir;
	struct dirent *dirent;

	dir = opendir(realPath);
	if(!dir)
		return;
	attrset(ATTR_LOG);
	while((dirent = readdir(dir)))
		if(dirent->d_type == DT_REG && dirent->d_name[0] != '.')
			printw("\n\t%s", dirent->d_name);
	closedir(dir);
}

void
cmd_quit(const struct branch *branch, struct value *values)
{
	close(fdBackup);
	endwin();
	exit(0);
}

void
cmd_clear(const struct branch *branch, struct value *values)
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
	locale = setlocale(LC_ALL, "");

	initscr();

	raw();
	noecho();
	
	keypad(stdscr, true);
	scrollok(stdscr, true);

	addstr("Doing setup...");
	addstr("\nChecking for color support...");
	if(has_colors())
	{
		start_color();
		init_pair(1, COLOR_GREEN, COLOR_BLACK);
		init_pair(2, COLOR_MAGENTA, COLOR_BLACK);
		init_pair(3, COLOR_RED, COLOR_BLACK);
		init_pair(4, COLOR_CYAN, COLOR_BLACK);
		init_pair(5, COLOR_YELLOW, COLOR_BLACK);
		init_pair(6, COLOR_GREEN, COLOR_BLACK);
		init_pair(7, COLOR_RED, COLOR_BLACK);
		attrset(ATTR_ADD);
		addstr(" SUCCESS");
	}
	else
	{
		addstr(" FAILED");
	}
	if(!homePath)
	{
		attrset(ATTR_FATAL);
		addstr("\nSetup failed: Enviroment variable HOME is not set!");
		goto err;
	}
	attrset(ATTR_LOG);
	printw("\nHome path is '%s'", homePath);
	strcpy(path, homePath);
	strcat(path, "/Passwords");
	realPath = realpath(path, NULL);
	printw("\nThe real path is '%s'", realPath);
	if(mkdir(realPath, 0700))
	{
		if(errno != EEXIST)
		{
			attrset(ATTR_FATAL);
			printw("\nSetup failed: Could not create real path directory (%s)", strerror(errno));
			goto err;
		}
	}
	appendrealpath(".backup", sizeof(".backup") - 1);
	printw("\nOpening backup file '%s'...", path);
	fdBackup = open(path, O_CREAT | O_APPEND | O_RDWR, S_IWUSR | S_IRUSR); 
	if(fdBackup == ERR)
	{
		attrset(ATTR_FATAL);
		addstr(" FAILED!");
		attrset(ATTR_LOG);
	}
	else
	{
		attrset(ATTR_ADD);
		addstr(" SUCCESS");
		attrset(ATTR_LOG);
	}
	addstr("\nChecking for UTF-8 support...");
	isUtf8 = locale && strstr(locale, "UTF-8");
	attrset(isUtf8 ? ATTR_ADD : ATTR_SUB);
	printw(" UTF-8 is %ssupported", isUtf8 ? "" : "not ");
	attrset(ATTR_ADD);
	addstr("\nSetup complete!"
			"\n\nPassword manager" VERSION);
	list_account(NULL, NULL);
get_input: // while(1) IN GOTO WE TRUST
	{
		TOKEN *tok;
		const struct branch *branch;
		struct value value;
		struct value values[10];
		U32 nValues = 0;

		addch('\n');
		if(getinput(&input, isUtf8))
			goto get_input;
		branch = root;
		while(1)
		{
			if(!hasnexttoken(&input))
			{
				attrset(ATTR_ERROR);
				printw("\nBranch '%s' needs more options", branch->name);
				printoptions(branch);
				break;
			}
			if(!(branch = nextbranch(branch, &input)))
				break;
			// check if the branch has any dependencies and get them
			for(U32 i = 0; i < ARRLEN(dependencies); i++)
				if(!strcmp(dependencies[i].name, branch->name))
				{
					if(!(tok = nexttoken(&input, &value)) || tok->type != dependencies[i].token)
					{
						attrset(ATTR_ERROR);
						printw("\nExpected %s after '%s'", dependencies[i].description, branch->name);
						goto get_input;
					}
					values[nValues++] = value;
					break;
				}
			if(IS_EXEC_BRANCH(branch))
			{
				if(branch->nSubnodes)
					branch->special(branch, &input);
				else
					branch->proc(branch, values);
				break;
			}
		}
		goto get_input;
	}
err:
	attrset(ATTR_FATAL);
	printw("\nAn unexpected error occured, press any key to exit...");
	getch();
	endwin();
	return ERR;
}
