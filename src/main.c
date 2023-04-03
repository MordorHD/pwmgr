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
// output window
WINDOW *out;
int iPage;
// input window
struct input input;

void
setoutpage(int page)
{
	int x, y, sx, pageSize;
	const int outSize = LINES - inputHeight;

	if(page < 0)
		return;
	getyx(out, y, x);
	pageSize = MAX(outSize / 2, 1);
	sx = y - outSize + 1 - page * pageSize;
	while(page && sx + pageSize <= 0)
	{
		sx += pageSize;
		page--;
	}
	iPage = page;
	prefresh(out, MAX(sx, 0), 0, 0, 0, outSize, COLS);
}

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
set(const struct branch *branch, struct value *values)
{
	char *name;
	U32 nName;
	char *value;
	U32 nValue;
	struct variable *var;
	I64 iVal;

	name = values[0].word;
	nName = values[0].nWord;
	value = values[1].string;
	nValue = values[1].nString;
	var = getvariable(name, nName);
	if(!var)
	{
		wattrset(out, ATTR_LOG);
		wprintw(out, "\nVariable '%.*s' doesn't exist, do you want to create it? [yn]", nName, name);
		setoutpage(0);
		if(wgetch(out) != 'y')
		{
			wattrset(out, ATTR_LOG);
			waddstr(out, "\nCreation of variable cancelled");
			return;
		}
		addvariable(&(struct variable) { strndup(name, nName), strndup(value, nValue) });
		attrset(ATTR_LOG);
		wprintw(out, "\nVariable '%.*s' created!", nName, name);
		return;
	}
	if(var->value)
	{
		free(var->value);
		var->value = strndup(value, nValue);
	}
	else if(!strcmp(var->name, "area"))
	{
		WINDOW *newOut;

		iVal = strtoll(value, NULL, 0);
		area = iVal;
		if(area / COLS <= LINES)
			area = COLS * LINES;
		newOut = newpad(area / COLS, COLS);
		overwrite(out, newOut);
		delwin(out);
		out = newOut;
		scrollok(out, true);
	}
	else if(!strcmp(var->name, "inputHeight"))
	{
		iVal = strtoll(value, NULL, 0);
		if(!iVal)
		{
			wattrset(out, ATTR_ERROR);
			wprintw(out, "\nThe input height can't be 0");
			return;
		}
		inputHeight = MIN(iVal, MAX(LINES / 2, 1));
		input.win = newwin(inputHeight, COLS, LINES - inputHeight, 0);
		keypad(input.win, true);
	}
}

void
add_account(const struct branch *branch, struct value *values)
{
	char *name;
	U32 nName;
	int fd;

	name = values[0].word;
	nName = values[0].nWord;
	appendrealpath(name, nName);
	if(!access(path, F_OK))
	{
		wattrset(out, ATTR_ERROR);
		wprintw(out, "\nAccount '%.*s' already exists", nName, name);
		return;
	}
	fd = open(path, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
	if(fd == ERR)
	{
		wattrset(out, ATTR_ERROR);
		wprintw(out, "\nUnable to create file inside '%s' (%s)", realPath, strerror(errno));
		return;
	}
	close(fd);
	wattrset(out, ATTR_LOG);
	wprintw(out, "\nCreated new account inside '%s'", path);
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
	accName = values[1].word;
	nAccName = values[1].nWord;
	appendrealpath(accName, nAccName);
	fd = open(path, O_RDWR | O_APPEND);
	if(fd == ERR)
	{
		wattrset(out, ATTR_ERROR);
		wprintw(out, "\nUnable to access '%s' (%s)", path, strerror(errno));
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
			wattrset(out, ATTR_ERROR);
			wprintw(out, "\nFile '%s/%.*s' is corrupt (state: 0)", realPath, nAccName, accName);
			close(fd);
			return;
		}
		if(nName == nPropName && !memcmp(name, propName, nPropName))
		{
			wattrset(out, ATTR_ERROR);
			wprintw(out, "\nProperty '%.*s' already exists", nPropName, propName);
			close(fd);
			return;
		}
		nRead -= nName + 1;
		memcpy(path, path + nName + 1, nRead);
		nRead += read(fd, path + nRead, sizeof(path) - 1 - nRead);
		if(!nRead)
		{
			wattrset(out, ATTR_ERROR);
			wprintw(out, "\nFile '%s/%.*s' is corrupt (state: 1)", realPath, nAccName, accName);
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
				wattrset(out, ATTR_ERROR);
				wprintw(out, "\nFile '%s/%.*s' is corrupt (state: 2)", realPath, nAccName, accName);
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
	wattrset(out, ATTR_LOG);
	wprintw(out, "\nWritten '%.*s' to account '%.*s'", values[2].nString, values[2].string, nAccName, accName);
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
		wattrset(out, ATTR_ERROR);
		wprintw(out, "\nUnable to open account '%.*s' (%s)", nAccName, accName, strerror(errno));
		return;
	}
	appendrealpath(".tmp", 4);
	fdTmp = open(path, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
	if(fdTmp == ERR)
	{
		wattrset(out, ATTR_FATAL);
		wprintw(out, "\nUnable to create temporary file (%s)", strerror(errno));
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
			wattrset(out, ATTR_ERROR);
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
		wattrset(out, ATTR_ERROR);
		wprintw(out, "\nProperty '%.*s' doesn't exist", nPropName, propName);
		return;
	}
	appendrealpath(accName, nAccName);
	if(renameat2(AT_FDCWD, tmpPath, AT_FDCWD, path, RENAME_EXCHANGE))
	{
		wattrset(out, ATTR_FATAL);
		wprintw(out, "\nFailed atomically swapping temporary file and new file (%s)", strerror(errno));
	}
	close(fdTmp);
	remove(tmpPath);
	wattrset(out, ATTR_LOG);
	wprintw(out, "\nRemoved property '%.*s' from account '%.*s'", nPropName, propName, nAccName, accName);
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
	wattrset(out, ATTR_FATAL);
	wprintw(out, "\nFile '%s/%.*s' is corrupt", realPath, nAccName, accName);
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
		wattrset(out, ATTR_ERROR);
		wprintw(out, "\nCouldn't remove account '%.*s'", nName, name);
	}
	else
	{
		wattrset(out, ATTR_LOG);
		wprintw(out, "\nSuccessfully removed account '%.*s'", nName, name);
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

	wattrset(out, ATTR_LOG);
	wprintw(out, "\nAre you sure you want to remove the backup? [Yn]");
	wgetch(out);
	if(ans != 'Y')
	{
		wprintw(out, "\nCancelled removal of backup");
		return;
	}
	appendrealpath(".backup", sizeof(".backup") - 1);
	if(remove(path))
	{
		wattrset(out, ATTR_ERROR);
		wprintw(out, "\nFailed to remove backup (%s)", strerror(errno));
	}
	else
	{
		wattrset(out, ATTR_LOG);
		wprintw(out, "\nBackup was removed");
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
		wattrset(out, ATTR_ERROR);
		wprintw(out, "\nCouldn't open account '%.*s' ('%s')", nAccName, accName, strerror(errno));
		return;
	}
	wattrset(out, ATTR_LOG);
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
		wprintw(out, "\n%.*s = ", nName, name);
		nRead -= nName + 1;
		memcpy(path, path + nName + 1, nRead);
		nRead += read(fd, path + nRead, sizeof(path) - 1 - nRead);
		if(!nRead)
			goto corrupt;
		nValue = strlen(path);
		wprintw(out, "%.*s", nValue, path);
		while(nValue == sizeof(path) - 1)
		{
			nRead = read(fd, path, sizeof(path) - 1);
			nValue = strlen(path);
			if(nValue > nRead)
				goto corrupt;
			wprintw(out, "%.*s", nValue, path);
		}
		nRead -= nValue + 1;
		memcpy(path, path + nValue + 1, nRead);
		nRead += read(fd, path + nRead, sizeof(path) - 1 - nRead);
	}
	close(fd);
	return;
corrupt:
	close(fd);
	wattrset(out, ATTR_ERROR);
	wprintw(out, "\nFile '%s/%.*s' is corrupt", realPath, nAccName, accName);
}

void info_backup(const struct branch *branch, struct value *values)
{
	ssize_t nRead;
	char *ptr;
	U32 iEvent = 1;

	if(lseek(fdBackup, 0, SEEK_SET))
	{
		wattrset(out, ATTR_ERROR);
		wprintw(out, "\nUnable to read backup file (%s)", strerror(errno));
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
		char strTime[100];

		ptr = path;
		id = *(ptr++);
		time = *(time_t*) ptr;
		ptr += sizeof(time_t);
		nRead -= sizeof(time_t) + 1;
		l = strlen(ptr);
		if(l >= nRead)
			goto corrupt;
		wattrset(out, ATTR_LOG);
		wprintw(out, "\n%u - ", iEvent++);
		l++;
		nRead -= l;
		switch(id)
		{
		case BACKUP_ENTRY_ADDACCOUNT:
			wattrset(out, ATTR_ADD);
			wprintw(out, "Added account '%s'", ptr);
			ptr += l;
			break;
		case BACKUP_ENTRY_REMOVEACCOUNT:
			wattrset(out, ATTR_SUB);
			wprintw(out, "Removed account '%s'", ptr);
			ptr += l;
			break;
		case BACKUP_ENTRY_ADDPROPERTY:
			wattrset(out, ATTR_ADD);
			wprintw(out, "Added property '%s'", ptr);
			ptr += l;
			wprintw(out, " to account '%s'", ptr);
			l = strlen(ptr) + 1;
			nRead -= l;
			ptr += l;
			waddstr(out, " with value '");
			memmove(path, ptr, nRead);
			nRead += read(fdBackup, path + nRead, sizeof(path) - 1 - nRead);
			waddstr(out, path);
			while(strlen(path) == sizeof(path) - 1)
			{
				nRead = read(fdBackup, path, sizeof(path) - 1);
				if(strlen(path) > nRead)
					goto corrupt;
				waddstr(out, path);
			}
			l = strlen(path) + 1;
			nRead -= l;
			ptr = path + l;
			break;
		case BACKUP_ENTRY_REMOVEPROPERTY:
			wattrset(out, ATTR_SUB);
			wprintw(out, "Removed property '%s'", ptr);
			ptr += l;
			wprintw(out, " from account '%s'", ptr);
			l = strlen(ptr) + 1;
			nRead -= l;
			ptr += l;
			break;
		}
		wattrset(out, ATTR_LOG);
		tm = localtime(&time);
		strftime(strTime, sizeof(strTime), "%F %r", tm);
		wprintw(out, "\t%s", strTime);
		memmove(path, ptr, nRead);
		nRead += read(fdBackup, path + nRead, sizeof(path) - 1 - nRead);
	}
	return;
corrupt:
	wattrset(out, ATTR_ERROR);
	waddstr(out, "\nCorrupt backup file, you must manually fix it ('help backup fix' for more info)");
}

void
help(const struct branch *helpBranch, struct input *input)
{
	static const struct {
		const char *name;
		const char *info;
	} general_infos[] = {
		{ "variables", "variables can be set using the 'set' command. Availabe variables are:"
			"\n\tarea\t\tArea of the output window"
			"\n\tinputHeight\tHeight of the input window"
	   		"\nYou may also set your own variables using 'set'" },
		{ "accounts", "accounts are combinations of data like password username, dob that make up an online presence" },
		{ "backups", "backups are local files that store all actions you perform" },
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
				wattrset(out, ATTR_LOG);
				wprintw(out, "\n%s", general_infos[i].info);
				return;
			}
	}
	branch = root;
	while(hasnexttoken(input) && (branch = nextbranch(branch, input)));
	if(branch)
	{
		if(!branch->description)
		{ // this means we stayed in root
			wattrset(out, ATTR_LOG);
			waddstr(out, "\nPassword manager " VERSION);
			wattrset(out, ATTR_DEFAULT);
			waddstr(out, "\nUse this as a manager for your accounts; you can do that by entering commands like help."
					" Commands are based on a 'branch' system, meaning a series of commands follows a specific branch; type 'tree' to visualize the available command tree."
					" Some commands also expect tokens right after it, for instance 'account' needs a name(word) argument."
					"\nFor more information put any of these words afer 'help'"
				);
			for(U32 i = 0; i < ARRLEN(general_infos); i++)
			{
				wattrset(out, ATTR_DEFAULT);
				waddstr(out, "\n\thelp ");
				wattrset(out, ATTR_HIGHLIGHT);
				waddstr(out, general_infos[i].name);
			}

		}
		else
		{
			wattrset(out, ATTR_LOG);
			wprintw(out, "\n%s", branch->description);
		}
	}
}

void
tree_print(const struct branch *branch, U32 depth)
{
	for(const struct branch *s = branch->subnodes, *e = s + branch->nSubnodes; s != e; s++)
	{
		if(branch->nSubnodes == 1)
			wprintw(out, " ");
		else
		{
			wprintw(out, "\n");
			for(U32 i = 0; i < depth; i++)
				wprintw(out, " |");
		}
		wattrset(out, ATTR_HIGHLIGHT);
		wprintw(out, "%s", s->name);
		for(U32 i = 0; i < ARRLEN(dependencies); i++)
			if(!strcmp(dependencies[i].name, s->name))
			{
				wprintw(out, " [%s]", dependencies[i].description);
				break;
			}
		wattrset(out, ATTR_DEFAULT);
		if(!IS_EXEC_BRANCH(s))
			tree_print(s, depth + 1);
		else
			wprintw(out, " %s", s->description);
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
	wattrset(out, ATTR_LOG);
	while((dirent = readdir(dir)))
		if(dirent->d_type == DT_REG && !strchr(dirent->d_name, '.'))
			wprintw(out, "\n\t%s", dirent->d_name);
	closedir(dir);
}

void
cmd_quit(const struct branch *branch, struct value *values)
{
	int fd;

	close(fdBackup);
	appendrealpath(".history", 8);
	fd = open(path, O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
	write(fd, &input.nextHistory, sizeof(input.nextHistory));
	write(fd, input.history, sizeof(input.history));
	close(fd);
	endwin();
	exit(0);
}

void
cmd_clear(const struct branch *branch, struct value *values)
{
	wclear(out);
}

int
main(void)
{
	bool isUtf8;
	char *locale;
	const char * const homePath = getenv("HOME");
	int fd;

	locale = setlocale(LC_ALL, "");

	initscr();
	raw();
	noecho();
	
	input.win = newwin(inputHeight, COLS, LINES - inputHeight, 0);
	keypad(input.win, true);
	input.buf = malloc(MAX_INPUT);

	out = newpad(area / COLS, COLS);
	scrollok(out, true);

	waddstr(out, "Doing setup...");
	waddstr(out, "\nChecking for color support...");
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
		wattrset(out, ATTR_ADD);
		waddstr(out, " SUCCESS");
	}
	else
	{
		waddstr(out, " FAILED");
	}
	if(!homePath)
	{
		wattrset(out, ATTR_FATAL);
		waddstr(out, "\nSetup failed: Enviroment variable HOME is not set!");
		goto err;
	}
	wattrset(out, ATTR_LOG);
	wprintw(out, "\nHome path is '%s'", homePath);
	strcpy(path, homePath);
	strcat(path, "/Passwords");
	realPath = realpath(path, NULL);
	wprintw(out, "\nThe real path is '%s'", realPath);
	if(mkdir(realPath, 0700))
	{
		if(errno != EEXIST)
		{
			wattrset(out, ATTR_FATAL);
			wprintw(out, "\nSetup failed: Could not create real path directory (%s)", strerror(errno));
			goto err;
		}
	}
	appendrealpath(".history", sizeof(".history") - 1);
	fd = open(path, O_RDONLY);
	if(fd != ERR)
	{
		read(fd, &input.nextHistory, sizeof(input.nextHistory));
		read(fd, input.history, sizeof(input.history));
		close(fd);
	}
	appendrealpath(".backup", sizeof(".backup") - 1);
	wprintw(out, "\nOpening backup file '%s'...", path);
	fdBackup = open(path, O_CREAT | O_APPEND | O_RDWR, S_IWUSR | S_IRUSR); 
	if(fdBackup == ERR)
	{
		wattrset(out, ATTR_FATAL);
		waddstr(out, " FAILED!");
		wattrset(out, ATTR_LOG);
	}
	else
	{
		wattrset(out, ATTR_ADD);
		waddstr(out, " SUCCESS");
		wattrset(out, ATTR_LOG);
	}
	waddstr(out, "\nChecking for UTF-8 support...");
	isUtf8 = locale && strstr(locale, "UTF-8");
	wattrset(out, isUtf8 ? ATTR_ADD : ATTR_SUB);
	wprintw(out, " UTF-8 is %ssupported", isUtf8 ? "" : "not ");
	wattrset(out, ATTR_ADD);
	waddstr(out, "\nSetup complete!"
			"\n\nPassword manager" VERSION);
	list_account(NULL, NULL);
get_input: // while(1) IN GOTO WE TRUST
	{
		int y, x, sx;
		int pageSize, iPage;
		TOKEN *tok;
		const struct branch *branch;
		struct value value;
		struct value values[10];
		U32 nValues = 0;

		// render out
		setoutpage(0);
		if(getinput(&input, isUtf8))
			goto get_input;
		branch = root;
		while(1)
		{
			if(!hasnexttoken(&input))
			{
				wattrset(out, ATTR_ERROR);
				wprintw(out, "\nBranch '%s' needs more options", branch->name);
				printoptions(branch);
				break;
			}
			if(!(branch = nextbranch(branch, &input)))
				break;
			// check if the branch has any dependencies and get them
			for(U32 i = 0; i < ARRLEN(dependencies); i++)
				if(!strcmp(dependencies[i].name, branch->name))
				{
					if(!(tok = nexttoken(&input, &value)) || (tok->type != dependencies[i].token && dependencies[i].token))
					{
						wattrset(out, ATTR_ERROR);
						wprintw(out, "\nExpected %s after '%s'", dependencies[i].description, branch->name);
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
	wattrset(out, ATTR_FATAL);
	waddstr(out, "\nAn unexpected error occured, press any key to exit...");
	wgetch(out);
	endwin();
	return ERR;
}
