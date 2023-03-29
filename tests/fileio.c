#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

typedef unsigned int U32;

void
write_file(void)
{
	int fd;
	char buf[4096];
	U32 i = 0;

	fd = open("test", O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);

	memset(buf, 'a', 20);
	i += 20;
	buf[i++] = 0;
	memset(buf + i, 'b', 3000);
	i += 3000;
	buf[i++] = 0;
	memset(buf + i, 'c', 12);
	i += 12;
	buf[i++] = 0;
	memset(buf + i, 'd', 600);
	i += 600;
	buf[i++] = 0;
	write(fd, buf, i);

	close(fd);
}

void
read_file(void)
{
	int fd;
	char buf[512];
	ssize_t nRead;
	
	fd = open("test", O_RDONLY);

	buf[sizeof(buf) - 1] = 0;
	nRead = read(fd, buf, sizeof(buf) - 1);
	while(nRead > 0)
	{
		char *name;
		U32 nName;

		printf("Read: %ld\n", nRead);

		name = buf;
		nName = strlen(buf);

		printf("NAME: %.*s\n", nName, name);

		nRead -= nName + 1;
		memcpy(buf, buf + nName + 1, nRead);
		nRead += read(fd, buf + nRead, sizeof(buf) - 1 - nRead);

		if(!nRead)
		{
			printf("error: corrupt file\n");
			return;
		}
		
		U32 nValue;
		U32 parts = 1;

		nValue = strlen(buf);
		while(nValue == sizeof(buf) - 1)
		{
			nRead = read(fd, buf, sizeof(buf) - 1);
			nValue = strlen(buf);
			if(nValue > nRead)
			{
				printf("error: corrupt file (2)\n");
				return;
			}
			printf("READ INNER: %ld, %u\n", nRead, nValue);
			parts++;
		}
		nRead -= nValue + 1;
		memcpy(buf, buf + nValue + 1, nRead);
		nRead += read(fd, buf + nRead, sizeof(buf) - 1 - nRead);
		printf("VALUE PARTS: %u\n", parts);
	}

	close(fd);
}

int
main(void)
{
	write_file();
	read_file();
	return 0;	
}
