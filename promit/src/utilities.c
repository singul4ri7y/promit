#include <math.h>

#include "utilities.h"
#include "object.h"

ssize_t getdelim(char **buf, size_t *bufsiz, int delimiter, FILE *fp) {
	char *ptr, *eptr;

	if (*buf == NULL || *bufsiz == 0) {
		*bufsiz = BUFSIZ;
		if ((*buf = ALLOCATE(char, *bufsiz)) == NULL)
			return -1;
	}

	for (ptr = *buf, eptr = *buf + *bufsiz;;) {
		int c = fgetc(fp);
		if (c == -1) {
			if (feof(fp))
				return ptr == *buf ? -1 : ptr - *buf;
			else return -1;
		}
		*ptr++ = c;
		if (c == delimiter) {
			*ptr = '\0';
			return ptr - *buf;
		}
		if (ptr + 2 >= eptr) {
			char *nbuf;
			size_t nbufsiz = *bufsiz * 2;
			ssize_t d = ptr - *buf;
			if ((nbuf = (char*) reallocate(*buf, *bufsiz, nbufsiz * sizeof(char))) == NULL)
				return -1;
			*buf = nbuf;
			*bufsiz = nbufsiz;
			eptr = nbuf + nbufsiz;
			ptr = nbuf + d;
		}
	}
}

ssize_t getline(char **buf, size_t *bufsiz, FILE *fp) {
	return getdelim(buf, bufsiz, '\n', fp);
}

extern ObjFile* vm_stdin;

char* get_string() {
	// See if the Promit System stdin is available. If not, return NULL.

	if(vm_stdin -> file == NULL) 
		return NULL;

	// Growable buffer for characters
	char* buffer = NULL;

	// Capacity of buffer
	size_t capacity = 0;

	// Number of characters actually in buffer
	size_t size = 0;

	// Character read or EOF
	int c;

	// To store old capacity.
	size_t oldCapacity = 0u;

	// Iteratively get characters from standard input, checking for CR (Mac OS), LF (Linux), and CRLF (Windows)
	while ((c = fgetc(vm_stdin -> file)) != '\r' && c != '\n' && c != EOF)
	{
		// Grow buffer if necessary
		if (size + 1 > capacity)
		{
			// Increment buffer's capacity if possible
			if (capacity < SIZE_MAX)
			{
				oldCapacity = capacity;
				capacity++;
			}
			else
			{
				FREE_ARRAY(char, buffer, capacity);
				return NULL;
			}

			// Extend buffer's capacity
			char* temp = (char*) reallocate(buffer, oldCapacity, capacity * sizeof(char));

			if (temp == NULL)
			{
				FREE_ARRAY(char, buffer, oldCapacity);
				return NULL;
			}

			buffer = temp;
			temp = NULL;
		}

		// Append current character to buffer
		buffer[size++] = c;
	}

	// Check whether user provided no input
	if (size == 0 && c == EOF)
	{
		return NULL;
	}

	// Check whether user provided too much input (leaving no room for trailing NULL)
	if (size == SIZE_MAX)
	{
		FREE_ARRAY(char, buffer, capacity);
		return NULL;
	}

	// If last character read was CR, try to read LF as well
	if (c == '\r' && (c = fgetc(vm_stdin -> file)) != '\n')
	{
		// Return NULL if character can't be pushed back onto standard input
		if (c != EOF && ungetc(c, vm_stdin -> file) == EOF)
		{
			FREE_ARRAY(char, buffer, capacity);
			return NULL;
		}
	}

	// Minimize buffer
	char* s = (char*) reallocate(buffer, capacity, (size + 1) * sizeof(char));

	if (s == NULL)
	{
		FREE_ARRAY(char, buffer, capacity);
		return NULL;
	}

	// Terminate string
	s[size] = '\0';

	// Return string
	return s;
}

double pstrtod(const char* str) {
	char* end;

	double number = strtod(str, &end);

	return *end == '\0' ? number : NAN;
}

#ifdef _WIN32

int gettimeofday(struct timeval* tp, struct timeval* tzp) {
    // EPOCH from 00:00:00 January 1, 1970. Windows epoch is 
    // different from Unix.
    static const uint64_t EPOCH = ((uint64_t) 116444736000000000ULL);

    SYSTEMTIME  system_time;
    FILETIME    file_time;
    uint64_t    time;

    GetSystemTime(&system_time);
    SystemTimeToFileTime( &system_time, &file_time );
    
    time  = ((uint64_t) file_time.dwLowDateTime);
    time += ((uint64_t) file_time.dwHighDateTime) << 32;

    tp -> tv_sec  = (long) ((time - EPOCH) / 10000000L);
    tp -> tv_usec = (long) (system_time.wMilliseconds * 1000);
    
    return 0;
}

#elif defined(__unix__)

void getch() {
	struct termios old, current;
	
	tcgetattr(STDIN_FILENO, &old);
	
	current = old;
	
	current.c_lflag &= ~(ICANON | ECHO);
	
	tcsetattr(STDIN_FILENO, TCSANOW, &current);
	
	getchar();
	
	tcsetattr(STDIN_FILENO, TCSANOW, &old);
}

#endif
