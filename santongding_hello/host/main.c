
#include <err.h>
#include <stdio.h>
#include <string.h>

/* OP-TEE TEE client API (built by optee_client) */
#include <tee_client_api.h>

/* For the UUID (found in the TA's h-file(s)) */
#include <santongding_hello_ta.h>

TEEC_SharedMemory *shared_mem;
void init_shared_buffer(TEEC_Context *ctx, size_t size)
{
	if (shared_mem != NULL)
	{
		errx(1, "realloc shared mem");
	}
	shared_mem = (TEEC_SharedMemory *)malloc(sizeof(TEEC_SharedMemory));
	memset(shared_mem, 0, sizeof(TEEC_SharedMemory));
	shared_mem->buffer = malloc(size);
	shared_mem->size = size;
	shared_mem->flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
	TEEC_Result res = TEEC_AllocateSharedMemory(ctx, shared_mem);
	if (res != TEEC_SUCCESS)
		errx(1, "TEEC_Allocate failed with code 0x%x", res);
}

void release_shared_buffer()
{
	if (shared_mem == NULL)
	{
		errx(1, "repeat release shared mem");
	}
	TEEC_ReleaseSharedMemory(shared_mem);
}

int main(int argc, char *argv[])
{
	char *input_v;
	int len;
	if (argc != 2)
	{
		errx(1, "wrong params");
	}
	input_v = argv[1];
	len = strlen(input_v);
	TEEC_Result res;
	TEEC_Context ctx;
	TEEC_Session sess;
	TEEC_Operation op;
	TEEC_UUID uuid = TA_HELLO_WORLD_UUID;
	uint32_t err_origin;

	/* Initialize a context connecting us to the TEE */
	res = TEEC_InitializeContext(NULL, &ctx);
	if (res != TEEC_SUCCESS)
		errx(1, "TEEC_InitializeContext failed with code 0x%x", res);

	init_shared_buffer(&ctx, 1024);

	/*
	 * Open a session to the TA, the TA will print "hello
	 * world!" in the log when the session is created.
	 */
	res = TEEC_OpenSession(&ctx, &sess, &uuid,
						   TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
	if (res != TEEC_SUCCESS)
		errx(1, "TEEC_Opensession failed with code 0x%x origin 0x%x",
			 res, err_origin);

	/*
	 * Execute a function in the TA by invoking it, in this case
	 * we're incrementing a number.
	 *
	 * The value of command ID part and how the parameters are
	 * interpreted is part of the interface provided by the TA.
	 */

	/* Clear the TEEC_Operation struct */
	memset(&op, 0, sizeof(op));

	/*
	 * Prepare the argument. Pass a value in the first parameter,
	 * the remaining three parameters are unused.
	 */
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INOUT,
									 TEEC_NONE, TEEC_NONE, TEEC_NONE);

	memcpy(shared_mem->buffer, input_v, len + 1);
	TEEC_RegisteredMemoryReference mem = {shared_mem, shared_mem->size, 0};
	op.params[0].memref = mem;

	/*
	 * TA_HELLO_WORLD_CMD_INC_VALUE is the actual function in the TA to be
	 * called.
	 */
	printf("Invoking TA with input: %s\n", (char *)(op.params[0].memref.parent->buffer + op.params[1].memref.offset));
	res = TEEC_InvokeCommand(&sess, 0, &op,
							 &err_origin);
	if (res != TEEC_SUCCESS)
		errx(1, "TEEC_InvokeCommand failed with code 0x%x origin 0x%x",
			 res, err_origin);

	printf("TA return string: %s\n", (char *)(op.params[0].memref.parent->buffer + op.params[1].memref.offset));

	/*
	 * We're done with the TA, close the session and
	 * destroy the context.
	 *
	 * The TA will print "Goodbye!" in the log when the
	 * session is closed.
	 */

	TEEC_CloseSession(&sess);

	release_shared_buffer();

	TEEC_FinalizeContext(&ctx);

	return 0;
}
