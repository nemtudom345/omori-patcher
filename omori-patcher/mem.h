#pragma once
#include "pch.h"
#include "consts.h"

typedef unsigned int natural;

struct HookResult
{
    void* trampolinePtr;
    void* backupPtr;
    size_t size;
    size_t padding;
};

static DWORD_PTR mallocI = Consts::codecave;

namespace Mem
{
    void Write(DWORD_PTR addr, void* input, size_t len);
    void* CreateCall(DWORD_PTR addr);
    HookResult HookOnce(DWORD_PTR targetInsn, DWORD_PTR hookFn);
    void Hook(DWORD_PTR targetInsn, DWORD_PTR hookFn);
}