#include <malloc.h>
#include "types.h"
#include "result.h"
#include "arm/atomics.h"
#include "services/nv.h"
#include "nvidia/ioctl.h"
#include "nvidia/buffer.h"

static u32 g_nvmap_fd = -1;
static u64 g_refCnt;

Result nvbufInit(void)
{
    Result rc;

    if (atomicIncrement64(&g_refCnt) > 0)
        return 0;

     rc = nvOpen(&g_nvmap_fd, "/dev/nvmap");

     if (R_FAILED(rc))
         atomicDecrement64(&g_refCnt);

     return rc;
}

void nvbufExit(void)
{
    if (atomicDecrement64(&g_refCnt) == 0)
    {
        if (g_nvmap_fd != -1)
            nvClose(g_nvmap_fd);

        g_nvmap_fd = -1;
    }
}

static Result _nvbufCreate(NvBuffer* m, size_t size, u32 flags, u32 align, NvBufferKind kind)
{
    Result rc;

    m->has_init = true;
    m->size = size;
    m->fd = -1;
    m->ptr = memalign(size, align);
    m->kind = kind;

    if (m->ptr == NULL)
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);

    rc = nvioctlNvmap_Create(g_nvmap_fd, size, &m->fd);

    if (R_FAILED(rc))
        m->fd = -1;

    if (R_SUCCEEDED(rc))
        rc = nvioctlNvmap_Alloc(g_nvmap_fd, m->fd, 0, flags | NvBufferFlags_Nintendo, align, kind, m->ptr);

    if (R_FAILED(rc))
        nvbufFree(m);

    return rc;
}

Result nvbufCreate(NvBuffer* m, size_t size, u32 align, NvBufferKind kind) {
    return _nvbufCreate(m, size, 0, align, kind);
}

Result nvbufCreateRw(NvBuffer* m, size_t size, u32 align, NvBufferKind kind) {
    return _nvbufCreate(m, size, NvBufferFlags_Writable, align, kind);
}

void nvbufFree(NvBuffer* m)
{
    if (!m->has_init)
        return;

    free(m->ptr);
    m->ptr = NULL;

    if (m->fd != -1)
        nvClose(m->fd);

    m->fd = -1;
}

void* nvbufGetAddr(NvBuffer* m) {
    return m->ptr;
}
