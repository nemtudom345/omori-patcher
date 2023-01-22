#include "utils.h"
#include "pch.h"
#include "psapi.h"
#include "io.h"
#include <fcntl.h>
#include <cstdio>
#include <iostream>

typedef unsigned int natural;

typedef struct HookResult
{
    void* trampolinePtr;
    void* backupPtr;
};

namespace Consts {
    const int RESET = 7;
    const int SUCCESS = 10;
    const int INFO = 11;
    const int ERR = 12;
    const int WARN = 14;


    const DWORD_PTR codecave = 0x0000000142BEC100;
    const DWORD_PTR codecaveEnd = 0x0000000142BED0B5;
    const DWORD_PTR JS_Eval = 0x00000001427776ED; // x64dbg actually thinks that this function is at: 0x1427776EC
}

namespace Utils
{
    // https://stackoverflow.com/questions/311955/redirecting-cout-to-a-console-in-windows
    void BindCrtHandlesToStdHandles(bool bindStdIn, bool bindStdOut, bool bindStdErr)
    {
        // Re-initialize the C runtime "FILE" handles with clean handles bound to "nul". We do this because it has been
        // observed that the file number of our standard handle file objects can be assigned internally to a value of -2
        // when not bound to a valid target, which represents some kind of unknown internal invalid state. In this state our
        // call to "_dup2" fails, as it specifically tests to ensure that the target file number isn't equal to this value
        // before allowing the operation to continue. We can resolve this issue by first "re-opening" the target files to
        // use the "nul" device, which will place them into a valid state, after which we can redirect them to our target
        // using the "_dup2" function.
        if (bindStdIn)
        {
            FILE* dummyFile;
            freopen_s(&dummyFile, "nul", "r", stdin);
        }
        if (bindStdOut)
        {
            FILE* dummyFile;
            freopen_s(&dummyFile, "nul", "w", stdout);
        }
        if (bindStdErr)
        {
            FILE* dummyFile;
            freopen_s(&dummyFile, "nul", "w", stderr);
        }

        // Redirect unbuffered stdin from the current standard input handle
        if (bindStdIn)
        {
            HANDLE stdHandle = GetStdHandle(STD_INPUT_HANDLE);
            if (stdHandle != INVALID_HANDLE_VALUE)
            {
                int fileDescriptor = _open_osfhandle((intptr_t)stdHandle, _O_TEXT);
                if (fileDescriptor != -1)
                {
                    FILE* file = _fdopen(fileDescriptor, "r");
                    if (file != NULL)
                    {
                        int dup2Result = _dup2(_fileno(file), _fileno(stdin));
                        if (dup2Result == 0)
                        {
                            setvbuf(stdin, NULL, _IONBF, 0);
                        }
                    }
                }
            }
        }

        // Redirect unbuffered stdout to the current standard output handle
        if (bindStdOut)
        {
            HANDLE stdHandle = GetStdHandle(STD_OUTPUT_HANDLE);
            if (stdHandle != INVALID_HANDLE_VALUE)
            {
                int fileDescriptor = _open_osfhandle((intptr_t)stdHandle, _O_TEXT);
                if (fileDescriptor != -1)
                {
                    FILE* file = _fdopen(fileDescriptor, "w");
                    if (file != NULL)
                    {
                        int dup2Result = _dup2(_fileno(file), _fileno(stdout));
                        if (dup2Result == 0)
                        {
                            setvbuf(stdout, NULL, _IONBF, 0);
                        }
                    }
                }
            }
        }

        // Redirect unbuffered stderr to the current standard error handle
        if (bindStdErr)
        {
            HANDLE stdHandle = GetStdHandle(STD_ERROR_HANDLE);
            if (stdHandle != INVALID_HANDLE_VALUE)
            {
                int fileDescriptor = _open_osfhandle((intptr_t)stdHandle, _O_TEXT);
                if (fileDescriptor != -1)
                {
                    FILE* file = _fdopen(fileDescriptor, "w");
                    if (file != NULL)
                    {
                        int dup2Result = _dup2(_fileno(file), _fileno(stderr));
                        if (dup2Result == 0)
                        {
                            setvbuf(stderr, NULL, _IONBF, 0);
                        }
                    }
                }
            }
        }

        // Clear the error state for each of the C++ standard stream objects. We need to do this, as attempts to access the
        // standard streams before they refer to a valid target will cause the iostream objects to enter an error state. In
        // versions of Visual Studio after 2005, this seems to always occur during startup regardless of whether anything
        // has been read from or written to the targets or not.
        if (bindStdIn)
        {
            std::wcin.clear();
            std::cin.clear();
        }
        if (bindStdOut)
        {
            std::wcout.clear();
            std::cout.clear();
        }
        if (bindStdErr)
        {
            std::wcerr.clear();
            std::cerr.clear();
        }
    }

    void Info(const char* msg)
    {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(hConsole, Consts::INFO);
        std::cout << "[INFO] ";
        SetConsoleTextAttribute(hConsole, Consts::RESET);
        std::cout << msg << std::endl;
    }

    void Infof(const char* format, ...)
    {
        va_list argptr;
        va_start(argptr, format);

        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(hConsole, Consts::INFO);
        std::cout << "[INFO] ";
        SetConsoleTextAttribute(hConsole, Consts::RESET);

        vprintf(format, argptr);
        va_end(argptr);
        std::cout << std::endl;
    }

    void Success(const char* msg)
    {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(hConsole, Consts::SUCCESS);
        std::cout << "[SUCCESS] ";
        SetConsoleTextAttribute(hConsole, Consts::RESET);
        std::cout << msg << std::endl;
    }

    void Successf(const char* format, ...)
    {
        va_list argptr;
        va_start(argptr, format);

        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(hConsole, Consts::SUCCESS);
        std::cout << "[SUCCESS] ";
        SetConsoleTextAttribute(hConsole, Consts::RESET);

        vprintf(format, argptr);
        va_end(argptr);
        std::cout << std::endl;
    }

    void Warn(const char* msg)
    {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(hConsole, Consts::WARN);
        std::cout << "[WARN] ";
        SetConsoleTextAttribute(hConsole, Consts::RESET);
        std::cout << msg << std::endl;
    }

    void Warnf(const char* format, ...)
    {
        va_list argptr;
        va_start(argptr, format);

        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(hConsole, Consts::WARN);
        std::cout << "[WARN] ";
        SetConsoleTextAttribute(hConsole, Consts::RESET);

        vprintf(format, argptr);
        va_end(argptr);
        std::cout << std::endl;
    }

    void Error(const char* msg)
    {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(hConsole, Consts::ERR);
        std::cout << "[ERROR] ";
        SetConsoleTextAttribute(hConsole, Consts::RESET);
        std::cout << msg << std::endl;
    }

    void Errorf(const char* format, ...)
    {
        va_list argptr;
        va_start(argptr, format);

        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(hConsole, Consts::ERR);
        std::cout << "[ERROR] ";
        SetConsoleTextAttribute(hConsole, Consts::RESET);

        vprintf(format, argptr);
        va_end(argptr);
        std::cout << std::endl;
    }

    void Debug(DWORD_PTR addr, size_t len)
    {
        Utils::Infof("====[%p]====", addr);
        for (size_t i = 0; i < len; i++)
        {
            Utils::Infof("[%p] %X", addr, (*(int*)addr) & 0xFF);
            addr++;
        }
        Utils::Info("========");
    }
}

static DWORD_PTR mallocI = Consts::codecave;

namespace Mem
{
    // I am not writing my own function to calculate instruction length - nemtudom345
    // https://stackoverflow.com/questions/23788236/get-size-of-assembly-instructions
    /* (C) Copyright 2012-2014 Semantic Designs, Inc.
        You may freely use this code provided you retain this copyright message
    */
    natural InstructionLength(BYTE* pc)
    { // returns length of instruction at PC
        bool trace_clone_checking = false;
        natural length = 0;
        natural opcode, opcode2;
        natural modrm;
        natural sib;
        BYTE* p = pc;

        while (true)
        {  // scan across prefix bytes
            opcode = *p++;
            switch (opcode)
            {
            case 0x64: case 0x65: // FS: GS: prefixes
            case 0x36: // SS: prefix
            case 0x66: case 0x67: // operand size overrides
            case 0xF0: case 0xF2: // LOCK, REPNE prefixes
                length++;
                break;
            case 0x2E: // CS: prefix, used as HNT prefix on jumps
            case 0x3E: // DS: prefix, used as HT prefix on jumps
                length++;
                // goto process relative jmp // tighter check possible here
                break;
            default:
                goto process_instruction_body;
            }
        }

    process_instruction_body:
        switch (opcode) // switch on main opcode
        {
            // ONE BYTE OPCODE, move to next opcode without remark
        case 0x27: case 0x2F:
        case 0x37: case 0x3F:
        case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
        case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4E: case 0x4F:
        case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57:
        case 0x58: case 0x59: case 0x5A: case 0x5B: case 0x5C: case 0x5D: case 0x5E: case 0x5F:
        case 0x90: // nop
        case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97: // xchg
        case 0x98: case 0x99:
        case 0x9C: case 0x9D: case 0x9E: case 0x9F:
        case 0xA4: case 0xA5: case 0xA6: case 0xA7: case 0xAA: case 0xAB: // string operators
        case 0xAC: case 0xAD: case 0xAE: case 0xAF:
            /* case 0xC3: // RET handled elsewhere */
        case 0xC9:
        case 0xCC: // int3
        case 0xF5: case 0xF8: case 0xF9: case 0xFC: case 0xFD:
            return length + 1; // include opcode

        case 0xC3: // RET
            if (*p++ != 0xCC)
                return length + 1;
            if (*p++ != 0xCC)
                return length + 2;
            if (*p++ == 0xCC
                && *p++ == 0xCC)
                return length + 5;
            goto error;

            // TWO BYTE INSTRUCTION
        case 0x04: case 0x0C: case 0x14: case 0x1C: case 0x24: case 0x2C: case 0x34: case 0x3C:
        case 0x6A:
        case 0xB0: case 0xB1: case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: case 0xB7:
        case 0xC2:
            return length + 2;

            // TWO BYTE RELATIVE BRANCH
        case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
        case 0x78: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7E: case 0x7F:
        case 0xE0: case 0xE1: case 0xE2: case 0xE3: case 0xEB:
            return length + 2;

            // THREE BYTE INSTRUCTION (NONE!)

        // FIVE BYTE INSTRUCTION:
        case 0x05: case 0x0D: case 0x15: case 0x1D:
        case 0x25: case 0x2D: case 0x35: case 0x3D:
        case 0x68:
        case 0xA9:
        case 0xB8: case 0xB9: case 0xBA: case 0xBB: case 0xBC: case 0xBD: case 0xBE: case 0xBF:
            return length + 5;

            // FIVE BYTE RELATIVE CALL
        case 0xE8:
            return length + 5;

            // FIVE BYTE RELATIVE BRANCH
        case 0xE9:
            if (p[4] == 0xCC)
                return length + 6; // <jmp near ptr ...  int 3>
            return length + 5; // plain <jmp near ptr>

            // FIVE BYTE DIRECT ADDRESS
        case 0xA1: case 0xA2: case 0xA3: // MOV AL,AX,EAX moffset...
            return length + 5;
            break;

            // ModR/M with no immediate operand
        case 0x00: case 0x01: case 0x02: case 0x03: case 0x08: case 0x09: case 0x0A: case 0x0B:
        case 0x10: case 0x11: case 0x12: case 0x13: case 0x18: case 0x19: case 0x1A: case 0x1B:
        case 0x20: case 0x21: case 0x22: case 0x23: case 0x28: case 0x29: case 0x2A: case 0x2B:
        case 0x30: case 0x31: case 0x32: case 0x33: case 0x38: case 0x39: case 0x3A: case 0x3B:
        case 0x84: case 0x85: case 0x86: case 0x87: case 0x88: case 0x89: case 0x8A: case 0x8B: case 0x8D: case 0x8F:
        case 0xD1: case 0xD2: case 0xD3:
        case 0xFE: case 0xFF: // misinterprets JMP far and CALL far, not worth fixing
            length++; // count opcode
            goto modrm;

            // ModR/M with immediate 8 bit value
        case 0x80: case 0x82: case 0x83:
        case 0xC0: case 0xC1:
        case 0xC6:  // with r=0?
            length += 2; // count opcode and immediate byte
            goto modrm;

            // ModR/M with immediate 32 bit value
        case 0x81:
        case 0xC7:  // with r=0?
            length += 5; // count opcode and immediate byte
            goto modrm;

        case 0x9B: // FSTSW AX = 9B DF E0
            if (*p++ == 0xDF)
            {
                if (*p++ == 0xE0)
                    return length + 3;
                printf("InstructionLength: Unimplemented 0x9B tertiary opcode %2x at %x\n", *p, p);
                goto error;
            }
            else {
                printf("InstructionLength: Unimplemented 0x9B secondary opcode %2x at %x\n", *p, p);
                goto error;
            }

        case 0xD9: // various FP instructions
            modrm = *p++;
            length++; //  account for FP prefix
            switch (modrm)
            {
            case 0xC9: case 0xD0:
            case 0xE0: case 0xE1: case 0xE4: case 0xE5:
            case 0xE8: case 0xE9: case 0xEA: case 0xEB: case 0xEC: case 0xED: case 0xEE:
            case 0xF8: case 0xF9: case 0xFA: case 0xFB: case 0xFC: case 0xFD: case 0xFE: case 0xFF:
                return length + 1;
            default:  // r bits matter if not one of the above specific opcodes
                switch ((modrm & 0x38) >> 3)
                {
                case 0: goto modrm_fetched;  // fld
                case 1: return length + 1; // fxch
                case 2: goto modrm_fetched; // fst
                case 3: goto modrm_fetched; // fstp
                case 4: goto modrm_fetched; // fldenv
                case 5: goto modrm_fetched; // fldcw
                case 6: goto modrm_fetched; // fnstenv
                case 7: goto modrm_fetched; // fnstcw
                }
                goto error; // unrecognized 2nd byte
            }

        case 0xDB: // various FP instructions
            modrm = *p++;
            length++; //  account for FP prefix
            switch (modrm)
            {
            case 0xE3:
                return length + 1;
            default:  // r bits matter if not one of the above specific opcodes
#if 0
                switch ((modrm & 0x38) >> 3)
                {
                case 0: goto modrm_fetched;  // fld
                case 1: return length + 1; // fxch
                case 2: goto modrm_fetched; // fst
                case 3: goto modrm_fetched; // fstp
                case 4: goto modrm_fetched; // fldenv
                case 5: goto modrm_fetched; // fldcw
                case 6: goto modrm_fetched; // fnstenv
                case 7: goto modrm_fetched; // fnstcw
                }
#endif
                goto error; // unrecognized 2nd byte
            }

        case 0xDD: // various FP instructions
            modrm = *p++;
            length++; //  account for FP prefix
            switch (modrm)
            {
            case 0xE1: case 0xE9:
                return length + 1;
            default:  // r bits matter if not one of the above specific opcodes
                switch ((modrm & 0x38) >> 3)
                {
                case 0: goto modrm_fetched;  // fld
                    // case 1: return length+1; // fisttp
                case 2: goto modrm_fetched; // fst
                case 3: goto modrm_fetched; // fstp
                case 4: return length + 1; // frstor
                case 5: return length + 1; // fucomp
                case 6: goto modrm_fetched; // fnsav
                case 7: goto modrm_fetched; // fnstsw
                }
                goto error; // unrecognized 2nd byte
            }

        case 0xF3: // funny prefix REPE
            opcode2 = *p++;  // get second opcode byte
            switch (opcode2)
            {
            case 0x90: // == PAUSE
            case 0xA4: case 0xA5: case 0xA6: case 0xA7: case 0xAA: case 0xAB: // string operators
                return length + 2;
            case 0xC3: // (REP) RET
                if (*p++ != 0xCC)
                    return length + 2; // only (REP) RET
                if (*p++ != 0xCC)
                    goto error;
                if (*p++ == 0xCC)
                    return length + 5; // (REP) RET CLONE IS LONG JUMP RELATIVE
                goto error;
            case 0x66: // operand size override (32->16 bits)
                if (*p++ == 0xA5) // "rep movsw"
                    return length + 3;
                goto error;
            default: goto error;
            }

        case 0xF6: // funny subblock of opcodes
            modrm = *p++;
            if ((modrm & 0x20) == 0)
                length++; // 8 bit immediate operand
            goto modrm_fetched;

        case 0xF7: // funny subblock of opcodes
            modrm = *p++;
            if ((modrm & 0x30) == 0)
                length += 4; // 32 bit immediate operand
            goto modrm_fetched;

            // Intel's special prefix opcode
        case 0x0F:
            length += 2; // add one for special prefix, and one for following opcode
            opcode2 = *p++;
            switch (opcode2)
            {
            case 0x31: // RDTSC
                return length;

                // CMOVxx
            case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
            case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4E: case 0x4F:
                goto modrm;

                // JC relative 32 bits
            case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x86: case 0x87:
            case 0x88: case 0x89: case 0x8A: case 0x8B: case 0x8C: case 0x8D: case 0x8E: case 0x8F:
                return length + 4; // account for subopcode and displacement

                // SETxx rm32
            case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97:
            case 0x98: case 0x99: case 0x9A: case 0x9B: case 0x9C: case 0x9D: case 0x9E: case 0x9F:
                goto modrm;

            case 0xA2: // CPUID
                return length + 2;

            case 0xAE: // LFENCE, SFENCE, MFENCE
                opcode2 = *p++;
                switch (opcode2)
                {
                case 0xE8: // LFENCE
                case 0xF0: // MFENCE
                case 0xF8: // SFENCE
                    return length + 1;
                default:
                    printf("InstructionLength: Unimplemented 0x0F, 0xAE tertiary opcode in clone  %2x at %x\n", opcode2, p - 1);
                    goto error;
                }

            case 0xAF: // imul
            case 0xB0: // cmpxchg 8 bits
                goto error;

            case 0xB1: // cmpxchg 32 bits
            case 0xB6: case 0xB7: // movzx
            case 0xBC: /* bsf */ case 0xBD: // bsr
                // case 0xBE: case 0xBF: // movsx 
            case 0xC1: // xadd
            case 0xC7: // cmpxchg8b
                goto modrm;

            default:
                printf("InstructionLength: Unimplemented 0x0F secondary opcode in clone %2x at %x\n", opcode, p - 1);
                goto error;
            } // switch

         // ALL THE THE REST OF THE INSTRUCTIONS; these are instructions that runtime system shouldn't ever use
        default:
            /* case 0x26: case 0x36: // ES: SS: prefixes
               case 0x9A:
               case 0xC8: case 0xCA: case 0xCB: case 0xCD: case 0xCE: case 0xCF:
               case 0xD6: case 0xD7:
               case 0xE4: case 0xE5: case 0xE6: case 0xE7: case 0xEA: case 0xEB: case 0xEC: case 0xED: case 0xEF:
               case 0xF4: case 0xFA: case 0xFB:
                */
            printf("InstructionLength: Unexpected opcode %2x\n", opcode);
            goto error;
        }

    modrm:
        modrm = *p++;
    modrm_fetched:
        if (trace_clone_checking)
            printf("InstructionLength: ModR/M byte %x %2x\n", pc, modrm);
        if (modrm >= 0xC0)
            return length + 1;  // account for modrm opcode
        else
        {  /* memory access */
            if ((modrm & 0x7) == 0x04)
            { /* instruction with SIB byte */
                length++; // account for SIB byte
                sib = *p++; // fetch the sib byte
                if ((sib & 0x7) == 0x05)
                {
                    if ((modrm & 0xC0) == 0x40)
                        return length + 1 + 1; // account for MOD + byte displacment
                    else return length + 1 + 4; // account for MOD + dword displacement
                }
            }
            switch (modrm & 0xC0)
            {
            case 0x0:
                if ((modrm & 0x07) == 0x05)
                    return length + 5; // 4 byte displacement
                else return length + 1; // zero length offset
            case 0x80:
                return length + 5;  // 4 byte offset
            default:
                return length + 2;  // one byte offset
            }
        }

    error:
        {  printf("InstructionLength: unhandled opcode at %8x with opcode %2x\n", pc, opcode);
        }
        return 0; // can't actually execute this
    }

    void Write(DWORD_PTR dst, void* src, size_t len)
    {
        DWORD old;
        VirtualProtect((LPVOID)dst, len, PAGE_EXECUTE_READWRITE, &old);
        memcpy((void*)dst, src, len);
        VirtualProtect((LPVOID)dst, len, old, &old);
    }

    void* codecaveAlloc(size_t size)
    {
        DWORD _;
        void* old = (void*) mallocI;
        VirtualProtect(old, size, PAGE_EXECUTE_READWRITE, &_);
        mallocI += size;
        if (mallocI > Consts::codecaveEnd)
        {
            Utils::Error("codecaveMalloc: No more free space in codecave");
            return NULL;
        }
        return old;
    }

    void* CreateCall(DWORD_PTR addr)
    {
        void* code = new BYTE[12]
        {
            0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov rax, ...
            0xFF, 0xD0  // call rax
        };

        memcpy((void*)((DWORD_PTR)code + 2), &addr, 8);

        return code;
    }

    HookResult _hook(DWORD_PTR insn, DWORD_PTR targetFn, size_t freeSpace = 100)
    {
        DWORD _;
        void* targetCall = Mem::CreateCall(targetFn);
        BYTE* reader = (BYTE*)insn;
        size_t i = 0;

        for (; i < 12; i += Mem::InstructionLength(reader))
        {
            reader = (BYTE*)(insn + i);
            Utils::Infof("%p %X %d %d", reader, (*reader) & 0xFF, i, Mem::InstructionLength(reader));
        }

        reader = (BYTE*)insn;
        void* trampoline = codecaveAlloc(i + 13 + freeSpace);
        void* backup = malloc(i);

        Utils::Infof("Address of trampoline: %p", trampoline);
        Utils::Infof("Backing up: %d bytes", i);
        memcpy(backup, (void*)reader, i);

        Utils::Infof("Replacing instructions");
        memset(trampoline, 0x90, i + 13 + freeSpace);
        memcpy(trampoline, targetCall, 12);
        ((BYTE*)trampoline)[12 + freeSpace + i] = 0xC3;
        VirtualProtect(trampoline, i + 12 + 1, PAGE_EXECUTE_READ, &_);

        Utils::Infof("trampoline: %p", trampoline);
        void* trampolineCall = Mem::CreateCall((DWORD_PTR)trampoline);

        Mem::Write(insn, trampolineCall, 12);
        Utils::Infof("Adding: %d bytes of padding", i - 12);


        for (size_t j = 0; j < (i-12); j++)
        {
            Mem::Write(insn+12+j, new BYTE[1]{ 0xCC }, 1);
        }

        return {
            trampoline,
            backup
        };
    }

    void Hook(DWORD_PTR insn, DWORD_PTR targetFn)
    {
        DWORD _;
        void* hookBackup = malloc(12);
        HookResult result = _hook(insn, targetFn, 50);
        memcpy((void*)hookBackup, result.trampolinePtr, 12);

        // Make insn to insn+50 rwx
        VirtualProtect((LPVOID)insn, 50, PAGE_EXECUTE_READWRITE, &_);

        BYTE* pre = new BYTE[9]
        {
            // Make a copy of the return address to r12
            0x41, 0x5C,     // pop r12

            // Backup registers
            0x50,    // push rax
            0x51,    // push rcx
            0x52,    // push rdx
            0x53,    // push rbx00000001427776F9
            0x55,    // push rbp
            0x56,    // push rsi
            0x57,    // push rdi
        };

        BYTE* post = new BYTE[47]
        {
            // subtract 0x0D (13) from the ret address
            0x49, 0x83, 0xEC, 0x0D,             // sub r12, 0x0D

            // memcpy(RSI, backup, 12);
            // RCX: _Dst
            // RDX: _Src
            // R8 : _Size

            0x49, 0x83, 0xEC, 0x00,             // sub r12, 0x00
            0x49, 0x8B, 0xCC,                   // mov rcx, r12
            0xBA, 0x00, 0x00, 0x00, 0x00,       // mov edx, backup
            0x41, 0xB8, 0x0B, 0x00, 0x00, 0x00, // mov r8d, 0x0B
            0xFF, 0x15, 0x1F, 0x73, 0x6D, 0x00, // call qword ptr ds : [0x00000001432C3450]

            0x49, 0x83, 0xC4, 0x00,             // add r12, 0x00

            // restore registers
            0x5F,       // pop rdi
            0x5E,       // pop rsi
            0x5D,       // pop rbp
            0x5B,       // pop rbx
            0x5A,       // pop rdx
            0x59,       // pop rcx
            0x58,       // pop rax

            // Push the ret address back on the stack
            0x41, 0x54,                         // push r12
            0x41, 0xBC, 0x00, 0x00, 0x00, 0x00  // mov r12d, 0
        };

        memcpy((void*)((DWORD_PTR)post + 12), &result.backupPtr, 4);

        Mem::Write((DWORD_PTR)result.trampolinePtr, pre, 9);
        Mem::Write((DWORD_PTR)result.trampolinePtr + 9, hookBackup, 12);
        Mem::Write((DWORD_PTR)result.trampolinePtr + 21, post, 47);
        Utils::Infof("backupPtr: 0x%p trampolinePtr: 0x%p", result.backupPtr, result.trampolinePtr);

    }
}