#include "commandline.h"

#ifdef USE_CLI_DEBUGGER
#include "debugger/cli-debugger.h"
#endif

#ifdef USE_GDB_STUB
#include "debugger/gdb-stub.h"
#endif

#include <fcntl.h>
#include <getopt.h>

static const char* _defaultFilename = "test.rom";

static const struct option _options[] = {
	{ "bios", 1, 0, 'b' },
	{ "debug", 1, 0, 'd' },
	{ "gdb", 1, 0, 'g' },
	{ 0, 0, 0, 0 }
};

int parseCommandArgs(struct StartupOptions* opts, int argc, char* const* argv) {
	memset(opts, 0, sizeof(*opts));
	opts->fd = -1;
	opts->biosFd = -1;
	opts->width = 240;
	opts->height = 160;
	opts->fullscreen = 0;

	int multiplier = 1;
	int ch;
	while ((ch = getopt_long(argc, argv, "234b:dfg", _options, 0)) != -1) {
		switch (ch) {
		case 'b':
			opts->biosFd = open(optarg, O_RDONLY);
			break;
#ifdef USE_CLI_DEBUGGER
		case 'd':
			if (opts->debuggerType != DEBUGGER_NONE) {
				return 0;
			}
			opts->debuggerType = DEBUGGER_CLI;
			break;
#endif
		case 'f':
			opts->fullscreen = 1;
			break;
#ifdef USE_GDB_STUB
		case 'g':
			if (opts->debuggerType != DEBUGGER_NONE) {
				return 0;
			}
			opts->debuggerType = DEBUGGER_GDB;
			break;
#endif
		case '2':
			multiplier = 2;
			break;
		case '3':
			multiplier = 3;
			break;
		case '4':
			multiplier = 4;
			break;
		default:
			return 0;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc == 1) {
		opts->fname = argv[0];
	} else if (argc == 0) {
		opts->fname = _defaultFilename;
	} else {
		return 0;
	}
	opts->fd = open(opts->fname, O_RDONLY);
	opts->width *= multiplier;
	opts->height *= multiplier;
	return 1;
}

struct ARMDebugger* createDebugger(struct StartupOptions* opts) {
	union DebugUnion {
		struct ARMDebugger d;
#ifdef USE_CLI_DEBUGGER
		struct CLIDebugger cli;
#endif
#ifdef USE_GDB_STUB
		struct GDBStub gdb;
#endif
	};

	union DebugUnion* debugger = malloc(sizeof(union DebugUnion));

	switch (opts->debuggerType) {
#ifdef USE_CLI_DEBUGGER
	case DEBUGGER_CLI:
		CLIDebuggerCreate(&debugger->cli);
		break;
#endif
#ifdef USE_GDB_STUB
	case DEBUGGER_GDB:
		GDBStubCreate(&debugger->gdb);
		GDBStubListen(&debugger->gdb, 2345, 0);
		break;
#endif
	case DEBUGGER_NONE:
	case DEBUGGER_MAX:
		free(debugger);
		return 0;
		break;
	}

	return &debugger->d;
}

void usage(const char* arg0) {
	printf("usage: %s [option ...] file\n", arg0);
	printf("\nOptions:\n");
	printf("  -2               2x viewport\n");
	printf("  -3               3x viewport\n");
	printf("  -4               4x viewport\n");
	printf("  -b, --bios FILE  GBA BIOS file to use\n");
#ifdef USE_CLI_DEBUGGER
	printf("  -d, --debug      Use command-line debugger\n");
#endif
	printf("  -f               Sart full-screen\n");
#ifdef USE_GDB_STUB
	printf("  -g, --gdb        Start GDB session (default port 2345)\n");
#endif
}