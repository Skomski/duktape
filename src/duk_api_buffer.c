/*
 *  Buffer
 */

#include "duk_internal.h"

DUK_EXTERNAL void *duk_resize_buffer(duk_context *ctx, duk_idx_t index, duk_size_t new_size) {
	duk_hthread *thr = (duk_hthread *) ctx;
	duk_hbuffer_dynamic *h;

	DUK_ASSERT_CTX_VALID(ctx);

	h = (duk_hbuffer_dynamic *) duk_require_hbuffer(ctx, index);
	DUK_ASSERT(h != NULL);

	if (!DUK_HBUFFER_HAS_DYNAMIC(h)) {
		DUK_ERROR(thr, DUK_ERR_TYPE_ERROR, DUK_STR_BUFFER_NOT_DYNAMIC);
	}

	/* maximum size check is handled by callee */
	duk_hbuffer_resize(thr, h, new_size);

	return DUK_HBUFFER_DYNAMIC_GET_DATA_PTR(thr->heap, h);
}

DUK_EXTERNAL void *duk_steal_buffer(duk_context *ctx, duk_idx_t index, duk_size_t *out_size) {
	duk_hthread *thr = (duk_hthread *) ctx;
	duk_hbuffer_dynamic *h;
	void *ptr;
	duk_size_t sz;

	DUK_ASSERT(ctx != NULL);

	h = (duk_hbuffer_dynamic *) duk_require_hbuffer(ctx, index);
	DUK_ASSERT(h != NULL);

	if (!DUK_HBUFFER_HAS_DYNAMIC(h)) {
		DUK_ERROR(thr, DUK_ERR_TYPE_ERROR, DUK_STR_BUFFER_NOT_DYNAMIC);
	}

	/* Forget the previous allocation, setting size to 0 and alloc to
	 * NULL.  Caller is responsible for freeing the previous allocation.
	 * Getting the allocation and clearing it is done in the same API
	 * call to avoid any chance of a realloc.
	 */
	ptr = DUK_HBUFFER_DYNAMIC_GET_DATA_PTR(thr->heap, h);
	sz = DUK_HBUFFER_DYNAMIC_GET_SIZE(h);
	if (out_size) {
		*out_size = sz;
	}
	DUK_HBUFFER_DYNAMIC_SET_DATA_PTR_NULL(thr->heap, h);
	DUK_HBUFFER_DYNAMIC_SET_SIZE(h, 0);

	return ptr;
}
