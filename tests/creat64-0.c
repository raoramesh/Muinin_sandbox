#define FUNC creat64
#define SFUNC "creat64"
#define FUNC_STR "\"%s\", %o"
#define FUNC_IMP file, mode
#define ARG_CNT 2
#define ARG_USE "<file> <mode>"

#define process_args() \
	s = argv[i++]; \
	char *file = s; \
	\
	s = argv[i++]; \
	mode_t mode; \
	sscanf(s, "%i", &mode);

#include "test-skel-0.c"