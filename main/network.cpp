#include "stdafx.h"
#include "network.h"
#include "rf.h"
#include "pf.h"
#include "utils.h"
#include "config.h"

//#define TEST_BUFFER_OVERFLOW_FIXES

static void ProcessUnreliableGamePacketsHook(const char *pData, int cbData, void *pAddr, void *pPlayer)
{
    RfProcessGamePackets(pData, cbData, pAddr, pPlayer);

#if MASK_AS_PF
    ProcessPfPacket(pData, cbData, pAddr, pPlayer);
#endif
}

static void HandleNewPlayerPacketHook(BYTE *pData, NET_ADDR *pAddr)
{
    if (*g_pbNetworkGame && !*g_pbLocalNetworkGame && GetForegroundWindow() != *g_phWnd)
        Beep(750, 300);
    RfHandleNewPlayerPacket(pData, pAddr);
}

extern "C" void SafeStrCpy(char *pDest, const char *pSrc, size_t DestSize)
{
#ifdef TEST_BUFFER_OVERFLOW_FIXES
    strcpy(pDest, "test");
#else
    strncpy(pDest, pSrc, DestSize);
    pDest[DestSize - 1] = 0;
#endif
}

NAKED void HandleGameInfoPacket_Security_0047B2D3()
{ // ecx - num, esi - source, ebx - dest
    _asm
    {
        pushad
        push 256
        push esi
        push ebx
        call SafeStrCpy
        add esp, 12
        popad
        xor eax, eax
        mov ecx, 0047B2E3h
        jmp ecx
    }
}

NAKED void HandleGameInfoPacket_Security_0047B334()
{ // ecx - num, esi -source, edi - dest
    _asm
    {
        push edx
        push 256
        push esi
        push edi
        call SafeStrCpy
        add esp, 12
        pop edx
        xor eax, eax
        mov ecx, 0047B342h
        jmp ecx
    }
}

NAKED void HandleGameInfoPacket_Security_0047B38E()
{ // ecx - num, esi -source, edi - dest
    _asm
    {
        pushad
        push 256
        push esi
        push edi
        call SafeStrCpy
        add esp, 12
        popad
        xor eax, eax
        mov ecx, 0047B39Ch
        jmp ecx
    }
}

NAKED void HandleJoinReqPacket_Security_0047AD4E()
{ // ecx - num, esi -source, edi - dest
    _asm
    {
        pushad
        push 256
        push esi
        push edi
        call SafeStrCpy
        add esp, 12
        popad
        mov ecx, 0047AD5Ah
        jmp ecx
    }
}

NAKED void HandleJoinAcceptPacket_Security_0047A8AE()
{ // ecx - num, esi -source, edi - dest
    _asm
    {
        pushad
        push 256
        push esi
        push edi
        call SafeStrCpy
        add esp, 12
        popad
        mov ecx, 0047A8BAh
        jmp ecx
    }
}

NAKED void HandleNewPlayerPacket_Security_0047A5F4()
{ // ecx - num, esi -source, edi - dest
    _asm
    {
        pushad
        push 256
        push esi
        push edi
        call SafeStrCpy
        add esp, 12
        popad
        mov byte ptr[esp + 12Ch - 118h], bl
        mov ecx, 0047A604h
        jmp ecx
    }
}

NAKED void HandlePlayersPacket_Security_00481EE6()
{ // ecx - num, esi -source, edi - dest
    _asm
    {
        pushad
        push 256
        push esi
        push edi
        call SafeStrCpy
        add esp, 12
        popad
        xor eax, eax
        mov ecx, 00481EF4h
        jmp ecx
    }
}

NAKED void HandleStateInfoReqPacket_Security_00481BEC()
{ // ecx - num, esi -source, edi - dest
    _asm
    {
        pushad
        push 256
        push esi
        push edi
        call SafeStrCpy
        add esp, 12
        popad
        mov al, byte ptr 0x0064EC40 // g_GameOptions
        mov ecx, 00481BFDh
        jmp ecx
    }
}

NAKED void HandleChatLinePacket_Security_004448B0()
{ // ecx - num, esi -source, edi - dest
    _asm
    {
        pushad
        push 256
        push esi
        push edi
        call SafeStrCpy
        add esp, 12
        popad
        cmp bl, 0FFh
        mov ecx, 004448BFh
        jmp ecx
    }
}

NAKED void HandleNameChangePacket_Security_0046EB24()
{ // ecx - num, esi -source, edi - dest
    _asm
    {
        pushad
        push 256
        push esi
        push edi
        call SafeStrCpy
        add esp, 12
        popad
        mov ecx, 0046EB30h
        jmp ecx
    }
}

NAKED void HandleLeaveLimboPacket_Security_0047C1C3()
{ // ecx - num, esi -source, edi - dest
    _asm
    {
        pushad
        push 256
        push esi
        push edi
        call SafeStrCpy
        add esp, 12
        popad
        mov ecx, 0047C1CFh
        jmp ecx
    }
}

NAKED void HandleObjKillPacket_Security_0047EE6E()
{ // ecx - num, esi -source, edi - dest
    _asm
    {
        pushad
        push 256
        push esi
        push edi
        call SafeStrCpy
        add esp, 12
        popad
        xor eax, eax
        push 0
        mov ecx, 0047EE7Eh
        jmp ecx
    }
}

NAKED void HandleEntityCreatePacket_Security_00475474()
{ // ecx - num, esi -source, edi - dest
    _asm
    {
        pushad
        push 256
        push esi
        push edi
        call SafeStrCpy
        add esp, 12
        popad
        xor eax, eax
        mov ecx, 00475482h
        jmp ecx
    }
}

NAKED void HandleItemCreatePacket_Security_00479FAA()
{ // ecx - num, esi -source, edi - dest
    _asm
    {
        pushad
        push 256
        push esi
        push edi
        call SafeStrCpy
        add esp, 12
        popad
        xor eax, eax
        mov ecx, 00479FB8h
        jmp ecx
    }
}

NAKED void HandleRconReqPacket_Security_0046C590()
{ // ecx - num, esi -source, edi - dest
    _asm
    {
        pushad
        push 256
        push esi
        push edi
        call SafeStrCpy
        add esp, 12
        popad
        lea eax, [esp + 110h - 100h]
        mov ecx, 0046C5A0h
        jmp ecx
    }
}

NAKED void HandleRconPacket_Security_0046C751()
{ // ecx - num, esi -source, edi - dest
    _asm
    {
        pushad
        push 512
        push esi
        push edi
        call SafeStrCpy
        add esp, 12
        popad
        xor eax, eax
        mov ecx, 0046C75Fh
        jmp ecx
    }
}

void NetworkInit()
{
    /* ProcessGamePackets hook (not reliable only) */
    WriteMemPtr((PVOID)0x00479245, (PVOID)((ULONG_PTR)ProcessUnreliableGamePacketsHook - (0x00479244 + 0x5)));

    /* Improve SimultaneousPing */
    *g_pSimultaneousPing = 32;

    /* Allow ports < 1023 (especially 0 - any port) */
    WriteMemUInt8Repeat((PVOID)0x00528F24, 0x90, 2);

    /* Default port: 0 */
    WriteMemUInt16((PVOID)0x0059CDE4, 0);

    /* If server forces player character, don't save it in settings */
    WriteMemUInt8Repeat((PVOID)0x004755C1, ASM_NOP, 6);
    WriteMemUInt8Repeat((PVOID)0x004755C7, ASM_NOP, 6);

    /* Show valid info for servers with incompatible version */
    WriteMemUInt8((PVOID)0x0047B3CB, ASM_SHORT_JMP_REL);

    /* Beep when new player joins */
    WriteMemPtr((PVOID)0x0059E158, HandleNewPlayerPacketHook);

    /* Buffer Overflow fixes */
    WriteMemUInt8((PVOID)0x0047B2D3, ASM_LONG_JMP_REL);
    WriteMemUInt32((PVOID)(0x0047B2D3 + 1), ((ULONG_PTR)HandleGameInfoPacket_Security_0047B2D3 - (0x0047B2D3 + 0x5)));
    WriteMemUInt8((PVOID)0x0047B334, ASM_LONG_JMP_REL);
    WriteMemUInt32((PVOID)(0x0047B334 + 1), ((ULONG_PTR)HandleGameInfoPacket_Security_0047B334 - (0x0047B334 + 0x5)));
#ifndef TEST_BUFFER_OVERFLOW_FIXES
    WriteMemUInt8((PVOID)0x0047B38E, ASM_LONG_JMP_REL);
    WriteMemUInt32((PVOID)(0x0047B38E + 1), ((ULONG_PTR)HandleGameInfoPacket_Security_0047B38E - (0x0047B38E + 0x5)));
#endif
    WriteMemUInt8((PVOID)0x0047AD4E, ASM_LONG_JMP_REL);
    WriteMemUInt32((PVOID)(0x0047AD4E + 1), ((ULONG_PTR)HandleJoinReqPacket_Security_0047AD4E - (0x0047AD4E + 0x5)));
    WriteMemUInt8((PVOID)0x0047A8AE, ASM_LONG_JMP_REL);
    WriteMemUInt32((PVOID)(0x0047A8AE + 1), ((ULONG_PTR)HandleJoinAcceptPacket_Security_0047A8AE - (0x0047A8AE + 0x5)));
    WriteMemUInt8((PVOID)0x0047A5F4, ASM_LONG_JMP_REL);
    WriteMemUInt32((PVOID)(0x0047A5F4 + 1), ((ULONG_PTR)HandleNewPlayerPacket_Security_0047A5F4 - (0x0047A5F4 + 0x5)));
    WriteMemUInt8((PVOID)0x00481EE6, ASM_LONG_JMP_REL);
    WriteMemUInt32((PVOID)(0x00481EE6 + 1), ((ULONG_PTR)HandlePlayersPacket_Security_00481EE6 - (0x00481EE6 + 0x5)));
    WriteMemUInt8((PVOID)0x00481BEC, ASM_LONG_JMP_REL);
    WriteMemUInt32((PVOID)(0x00481BEC + 1), ((ULONG_PTR)HandleStateInfoReqPacket_Security_00481BEC - (0x00481BEC + 0x5)));
    WriteMemUInt8((PVOID)0x004448B0, ASM_LONG_JMP_REL);
    WriteMemUInt32((PVOID)(0x004448B0 + 1), ((ULONG_PTR)HandleChatLinePacket_Security_004448B0 - (0x004448B0 + 0x5)));
    WriteMemUInt8((PVOID)0x0046EB24, ASM_LONG_JMP_REL);
    WriteMemUInt32((PVOID)(0x0046EB24 + 1), ((ULONG_PTR)HandleNameChangePacket_Security_0046EB24 - (0x0046EB24 + 0x5)));
    WriteMemUInt8((PVOID)0x0047C1C3, ASM_LONG_JMP_REL);
    WriteMemUInt32((PVOID)(0x0047C1C3 + 1), ((ULONG_PTR)HandleLeaveLimboPacket_Security_0047C1C3 - (0x0047C1C3 + 0x5)));
    WriteMemUInt8((PVOID)0x0047EE6E, ASM_LONG_JMP_REL);
    WriteMemUInt32((PVOID)(0x0047EE6E + 1), ((ULONG_PTR)HandleObjKillPacket_Security_0047EE6E - (0x0047EE6E + 0x5)));
    WriteMemUInt8((PVOID)0x00475474, ASM_LONG_JMP_REL);
    WriteMemUInt32((PVOID)(0x00475474 + 1), ((ULONG_PTR)HandleEntityCreatePacket_Security_00475474 - (0x00475474 + 0x5)));
    WriteMemUInt8((PVOID)0x00479FAA, ASM_LONG_JMP_REL);
    WriteMemUInt32((PVOID)(0x00479FAA + 1), ((ULONG_PTR)HandleItemCreatePacket_Security_00479FAA - (0x00479FAA + 0x5)));
    WriteMemUInt8((PVOID)0x0046C590, ASM_LONG_JMP_REL);
    WriteMemUInt32((PVOID)(0x0046C590 + 1), ((ULONG_PTR)HandleRconReqPacket_Security_0046C590 - (0x0046C590 + 0x5)));
    WriteMemUInt8((PVOID)0x0046C751, ASM_LONG_JMP_REL);
    WriteMemUInt32((PVOID)(0x0046C751 + 1), ((ULONG_PTR)HandleRconPacket_Security_0046C751 - (0x0046C751 + 0x5)));
}