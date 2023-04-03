#include "pwmgr.h"

U32 area = 200 * 200;
U32 inputHeight = 3;

static struct variable *variables;
static U32 nVariables;

static void __attribute__((constructor))
init(void)
{
	static const struct variable builtin_variables[] = {
		{ "area", NULL },
		{ "inputHeight", NULL },
	};
	
	variables = malloc(sizeof(builtin_variables));
	nVariables = ARRLEN(builtin_variables);
	memcpy(variables, builtin_variables, sizeof(builtin_variables));
}

void
addvariable(const struct variable *var)
{
	variables = realloc(variables, sizeof(*variables) * (nVariables + 1));
	variables[nVariables++] = *var;
}

struct variable *
getvariable(const char *name, U32 nName)
{
	for(U32 v = 0; v < nVariables; v++)
	{
		if(!strncmp(variables[v].name, name, nName)
				&& !variables[v].name[nName])
			return variables + v;
	}
	return NULL;
}

