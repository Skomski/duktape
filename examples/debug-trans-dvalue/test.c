/*
 *  Example program using debug transport.
 */

#include <stdio.h>
#include <stdlib.h>

#include "duktape.h"
#include "duk_debug_trans_dvalue.h"

void my_cooperate(duk_trans_dvalue_ctx *ctx, int block) {
	static int first_blocked = 1;

	if (!block) {
		/* Duktape is not blocked; you can cooperate with e.g. a user
		 * interface here and send dvalues to Duktape, but don't block.
		 */
		return;
	}

	/* Duktape is blocked on a read and won't continue until debug
	 * command(s) are sent.
	 *
	 * The code below sends some example messages.
	 *
	 * If you create dvalues manually and send them using
	 * duk_trans_dvalue_send(), you must free the dvalues
	 * after the send call returns using duk_dvalue_free().
	 */

	if (first_blocked) {
		/* First time Duktape becomes blocked, send DumpHeap which
		 * exercises a lot of parsing code.
		 *
		 * NOTE: Valgrind may complain about reading uninitialized
		 * bytes.  This is caused by the DumpHeap command writing out
		 * verbatim duk_tval values which are intentionally not
		 * always fully initialized for performance reasons.
		 */
		first_blocked = 0;

		fprintf(stderr, "Duktape is blocked, send DumpHeap\n");
		fflush(stderr);

		duk_trans_dvalue_send_req(ctx);
		duk_trans_dvalue_send_integer(ctx, 0x20);  /* DumpHeap */
		duk_trans_dvalue_send_eom(ctx);
	}

	fprintf(stderr, "Duktape is blocked, send Eval and StepInto to resume execution\n");
	fflush(stderr);

	/* duk_trans_dvalue_send_req_cmd() sends a REQ dvalue followed by
	 * an integer dvalue (command) for convenience.
	 */

	duk_trans_dvalue_send_req_cmd(ctx, 0x1e);  /* 0x1e = Eval */
	duk_trans_dvalue_send_string(ctx, "evalMe");
	duk_trans_dvalue_send_eom(ctx);

	duk_trans_dvalue_send_req_cmd(ctx, 0x14);  /* 0x14 = StepOver */
	duk_trans_dvalue_send_eom(ctx);
}

void my_received(duk_trans_dvalue_ctx *ctx, duk_dvalue *dv) {
	char buf[DUK_DVALUE_TOSTRING_BUFLEN];
	(void) ctx;

	duk_dvalue_to_string(dv, buf);
	fprintf(stderr, "Received dvalue: %s\n", buf);
	fflush(stderr);

	/* Here a normal debug client would wait for dvalues until an EOM
	 * dvalue was received (which completes a debug message).  The
	 * debug message would then be handled, possibly causing UI changes
	 * and/or causing debug commands to be sent to Duktape.
	 *
	 * The callback is responsible for eventually freeing the dvalue.
	 * Here we free it immediately, but an actual client would probably
	 * gather dvalues into an array or linked list to handle when the
	 * debug message was complete.
	 */

	duk_dvalue_free(dv);
}

void my_handshake(duk_trans_dvalue_ctx *ctx, const char *line) {
	(void) ctx;

	/* The Duktape handshake line is given in 'line' (without LF).
	 * The 'line' argument can be accessed for the duration of the
	 * callback (read only).  Don't free 'line' here, the transport
	 * handles that.
	 */

	fprintf(stderr, "Received handshake line: '%s'\n", line);
	fflush(stderr);
}

void my_detached(duk_trans_dvalue_ctx *ctx) {
	(void) ctx;

	/* Detached call forwarded as is. */

	fprintf(stderr, "Debug transport detached\n");
	fflush(stderr);
}

int main(int argc, char *argv[]) {
	duk_context *ctx;
	duk_trans_dvalue_ctx *trans_ctx;

	(void) argc; (void) argv;  /* suppress warning */

	ctx = duk_create_heap_default();
	if (!ctx) {
		fprintf(stderr, "Failed to create Duktape heap\n");
		fflush(stderr);
		exit(1);
	}

	trans_ctx = duk_trans_dvalue_init();
	if (!trans_ctx) {
		/* No cleanup here, doesn't matter (except for Amiga :-). */
		fprintf(stderr, "Failed to create debug transport context\n");
		fflush(stderr);
		exit(1);
	}
	trans_ctx->cooperate = my_cooperate;
	trans_ctx->received = my_received;
	trans_ctx->handshake = my_handshake;
	trans_ctx->detached = my_detached;

	duk_debugger_attach(ctx,
	                    duk_debug_trans_dvalue_read,
	                    duk_debug_trans_dvalue_write,
	                    duk_debug_trans_dvalue_peek,
	                    duk_debug_trans_dvalue_read_flush,
	                    duk_debug_trans_dvalue_write_flush,
	                    duk_debug_trans_dvalue_detached,
	                    (void *) trans_ctx);

	fprintf(stderr, "Debugger attached, running eval\n");
	fflush(stderr);

	/* Evaluate simple test code, callbacks will "step over" until end.
	 *
	 * The test code tries to exercise the debug protocol.  The 'evalMe'
	 * variable is evaluated (using debugger command Eval) before every
	 * step to force different dvalues to be carried over the transport.
	 */

	duk_eval_string(ctx,
		"var evalMe;\n"
		"\n"
		"print('Hello world!');\n"
		"[ undefined, null, true, false, 123, 123.1, 0, -0, 1/0, 0/0, -1/0, \n"
		"  'foo', Duktape.Buffer('bar'), Duktape.Pointer('dummy'), Math.cos, \n"
		"].forEach(function (val) {\n"
		"    print(val);\n"
		"    evalMe = val;\n"
		"});\n"
		"\n"
		"var str = 'xxx'\n"
		"for (i = 0; i < 10; i++) {\n"
		"    print(i, str);\n"
		"    evalMe = str;\n"
		"    evalMe = Duktape.Buffer(str);\n"
		"    str = str + str;\n"
		"}\n"
	);
	duk_pop(ctx);

	duk_debugger_detach(ctx);

	duk_trans_dvalue_free(trans_ctx);
	trans_ctx = NULL;

	duk_destroy_heap(ctx);

	return 0;
}
