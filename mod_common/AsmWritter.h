#pragma once

#include "MemUtils.h"
#include "AsmOpcodes.h"
#include <cassert>
#include <optional>

class AsmReg
{
public:
    int regNum;
    int size;

    constexpr explicit AsmReg(int regNum, int size) : regNum(regNum), size(size) {}
    constexpr bool operator==(const AsmReg &other) const { return regNum == other.regNum && size == other.size; }
};

struct AsmReg32 : public AsmReg
{
    constexpr AsmReg32(int num) : AsmReg(num, 4) {}
};

struct AsmReg16 : public AsmReg
{
    constexpr AsmReg16(int num) : AsmReg(num, 2) {}
};

struct AsmReg8 : public AsmReg
{
    constexpr AsmReg8(int num) : AsmReg(num, 1) {}
};

struct AsmRegMem
{
    bool memory;
    std::optional<AsmReg> regOpt;
    int32_t displacement;

    AsmRegMem(bool memory, std::optional<AsmReg> regOpt, int32_t displacement = 0) :
        memory(memory), regOpt(regOpt), displacement(displacement)
    {}
};

namespace AsmRegs
{
    enum GenPurpRegNum
    {
        GPRN_AX, GPRN_CX, GPRN_DX, GPRN_BX,
        GPRN_SP, GPRN_BP, GPRN_SI, GPRN_DI,
    };

    constexpr AsmReg32 EAX(GPRN_AX), ECX(GPRN_CX), EDX(GPRN_DX), EBX(GPRN_BX);
    constexpr AsmReg32 ESP(GPRN_SP), EBP(GPRN_BP), ESI(GPRN_SI), EDI(GPRN_DI);
    constexpr AsmReg16 AX(GPRN_AX), CX(GPRN_CX), DX(GPRN_DX), BX(GPRN_BX);
    constexpr AsmReg16 SP(GPRN_SP), BP(GPRN_BP), SI(GPRN_SI), DI(GPRN_DI);
    constexpr AsmReg8 AL(GPRN_AX), CL(GPRN_CX), DL(GPRN_DX), BL(GPRN_BX);
    constexpr AsmReg8 AH(GPRN_SP), CH(GPRN_BP), DH(GPRN_SI), BH(GPRN_DI);
}

class AsmWritter
{
private:
    enum AsmPrefix
    {
        PR_OPERAND_SIZE_OVERRIDE = 0x66,
    };

public:
    static const unsigned UNK_END_ADDR = 0xFFFFFFFF;

    AsmWritter(unsigned beginAddr, unsigned endAddr = UNK_END_ADDR) :
        m_addr(beginAddr), m_endAddr(endAddr) {}

    ~AsmWritter()
    {
        if (m_endAddr != UNK_END_ADDR)
        {
            assert(m_addr <= m_endAddr);
            while (m_addr < m_endAddr)
                nop();
        }
    }

    AsmWritter &pusha()
    {
        WriteMemUInt8(m_addr++, 0x60); // Opcode
        return *this;
    }

    AsmWritter &popa()
    {
        WriteMemUInt8(m_addr++, 0x61); // Opcode
        return *this;
    }

    AsmWritter &push(const AsmReg32 &reg)
    {
        WriteMemUInt8(m_addr++, 0x50 | reg.regNum); // Opcode
        return *this;
    }

    AsmWritter &push(const AsmReg16 &reg)
    {
        WriteMemUInt8(m_addr++, PR_OPERAND_SIZE_OVERRIDE); // Prefix
        WriteMemUInt8(m_addr++, 0x50 | reg.regNum);
        return *this;
    }

    AsmWritter &pop(const AsmReg32 &reg)
    {
        WriteMemUInt8(m_addr++, 0x58 | reg.regNum); // Opcode
        return *this;
    }

    AsmWritter &pop(const AsmReg16 &reg)
    {
        WriteMemUInt8(m_addr++, PR_OPERAND_SIZE_OVERRIDE); // Prefix
        WriteMemUInt8(m_addr++, 0x58 | reg.regNum); // Opcode
        return *this;
    }

    AsmWritter &push(int32_t Val)
    {
        WriteMemUInt8(m_addr++, 0x68); // Opcode
        WriteMemInt32(m_addr, Val);
        m_addr += 4;
        return *this;
    }

    AsmWritter &pushf()
    {
        WriteMemUInt8(m_addr++, 0x9C); // Opcode
        return *this;
    }

    AsmWritter &popf()
    {
        WriteMemUInt8(m_addr++, 0x9D); // Opcode
        return *this;
    }

    AsmWritter &xor_(const AsmReg32 &dstReg, const AsmReg32 &srcReg)
    {
        WriteMemUInt8(m_addr++, 0x33); // Opcode
        WriteMemUInt8(m_addr++, 0xC0 | (srcReg.regNum << 3) | dstReg.regNum); // ModR/M
        return *this;
    }

    AsmWritter &add(const AsmRegMem &dstRM, int32_t imm32)
    {
        if (abs(imm32) < 128)
            return add(dstRM, static_cast<int8_t>(imm32));
        WriteMemUInt8(m_addr++, 0x81); // Opcode
        writeModRm(dstRM, 0);
        WriteMemInt32(m_addr, imm32);
        m_addr += 4;
        return *this;
    }

    AsmWritter &add(const AsmRegMem &dstRM, int8_t imm8)
    {
        WriteMemUInt8(m_addr++, 0x83); // Opcode
        writeModRm(dstRM, 0);
        WriteMemInt8(m_addr++, imm8);
        return *this;
    }

    AsmWritter &nop()
    {
        WriteMemUInt8(m_addr++, 0x90);
        return *this;
    }

    AsmWritter &jmpLong(uint32_t addr)
    {
        WriteMemUInt8(m_addr, 0xE9);
        WriteMemInt32(m_addr + 1, addr - (m_addr + 0x5));
        m_addr += 5;
        return *this;
    }

    template<typename T>
    AsmWritter &jmpLong(T *addr)
    {
        return jmpLong(reinterpret_cast<uintptr_t>(addr));
    }

    AsmWritter &jmpNear(uint32_t addr)
    {
        WriteMemUInt8(m_addr, 0xEB);
        WriteMemInt8(m_addr + 1, addr - (m_addr + 0x2));
        m_addr += 2;
        return *this;
    }

    AsmWritter &callLong(uint32_t addr)
    {
        WriteMemUInt8(m_addr, 0xE8);
        WriteMemInt32(m_addr + 1, addr - (m_addr + 0x5));
        m_addr += 5;
        return *this;
    }

    template<typename T> 
    AsmWritter &callLong(T *addr)
    {
        WriteMemUInt8(m_addr, 0xE8);
        WriteMemInt32(m_addr + 1, reinterpret_cast<intptr_t>(addr) - (m_addr + 0x5));
        m_addr += 5;
        return *this;
    }

    AsmWritter &ret()
    {
        WriteMemUInt8(m_addr++, 0xC3);
        return *this;
    }

    AsmWritter &mov(const AsmRegMem &dstRM, const AsmReg32 &srcReg)
    {
        WriteMemUInt8(m_addr++, 0x89); // Opcode
        writeModRm(dstRM, srcReg.regNum);
        return *this;
    }

    AsmWritter &mov(const AsmReg32 &dstReg, const AsmRegMem &srcRM)
    {
        WriteMemUInt8(m_addr++, 0x8B); // Opcode
        writeModRm(srcRM, dstReg.regNum);
        return *this;
    }

    AsmWritter &mov(const AsmReg32 &dstReg, const AsmReg32 &srcReg)
    {
        WriteMemUInt8(m_addr++, 0x89); // Opcode
        WriteMemUInt8(m_addr++, 0xC0 | (srcReg.regNum << 3) | dstReg.regNum); // ModR/M
        return *this;
    }

    AsmWritter &mov(const AsmReg16 &dstReg, const AsmReg16 &srcReg)
    {
        WriteMemUInt8(m_addr++, PR_OPERAND_SIZE_OVERRIDE); // Prefix
        WriteMemUInt8(m_addr++, 0x89); // Opcode
        WriteMemUInt8(m_addr++, 0xC0 | (srcReg.regNum << 3) | dstReg.regNum); // ModR/M
        return *this;
    }

    AsmWritter &mov(const AsmReg8 &dstReg, const AsmReg8 &srcReg)
    {
        WriteMemUInt8(m_addr++, 0x88); // Opcode
        WriteMemUInt8(m_addr++, 0xC0 | (srcReg.regNum << 3) | dstReg.regNum); // ModR/M
        return *this;
    }

    AsmWritter &mov(const AsmReg32 &dstReg, int32_t imm)
    {
        WriteMemUInt8(m_addr++, 0xB8 | dstReg.regNum); // Opcode
        WriteMemInt32(m_addr, imm);
        m_addr += 4;
        return *this;
    }

    AsmWritter &mov(const AsmReg16 &dstReg, int16_t imm)
    {
        WriteMemUInt8(m_addr++, PR_OPERAND_SIZE_OVERRIDE); // Prefix
        WriteMemUInt8(m_addr++, 0xB8 | dstReg.regNum); // Opcode
        WriteMemInt16(m_addr, imm);
        m_addr += 2;
        return *this;
    }

    AsmWritter &mov(const AsmReg8 &dstReg, int8_t imm)
    {
        WriteMemUInt8(m_addr++, 0xB0 | dstReg.regNum); // Opcode
        WriteMemInt8(m_addr++, imm);
        return *this;
    }

    AsmWritter &movEaxMemEsp(int8_t off)
    {
        WriteMemUInt8(m_addr++, 0x8B);
        WriteMemUInt8(m_addr++, 0x44);
        WriteMemUInt8(m_addr++, 0x24);
        WriteMemInt8(m_addr++, off);
        return *this;
    }

    AsmWritter &movEaxMemEsp32(int32_t off)
    {
        WriteMemUInt8(m_addr++, 0x8B);
        WriteMemUInt8(m_addr++, 0x84);
        WriteMemUInt8(m_addr++, 0x24);
        WriteMemInt32(m_addr++, off);
        return *this;
    }

    AsmWritter &addEsp(int8_t val)
    {
        WriteMemUInt8(m_addr++, 0x83);
        WriteMemUInt8(m_addr++, 0xC4);
        WriteMemInt8(m_addr++, val);
        return *this;
    }

    AsmWritter &leaEaxEsp(int8_t addVal)
    {
        WriteMemUInt8(m_addr++, 0x8D);
        WriteMemUInt8(m_addr++, 0x44);
        WriteMemUInt8(m_addr++, 0x24);
        WriteMemInt8(m_addr++, addVal);
        return *this;
    }

    AsmWritter &leaEdxEsp(int8_t addVal)
    {
        WriteMemUInt8(m_addr++, 0x8D);
        WriteMemUInt8(m_addr++, 0x54);
        WriteMemUInt8(m_addr++, 0x24);
        WriteMemInt8(m_addr++, addVal);
        return *this;
    }

    AsmWritter &cmp(const AsmReg8 &reg, int8_t imm)
    {
        if (reg == AsmRegs::AL) // al
            WriteMemUInt8(m_addr++, 0x3C);
        else
            assert(false);
        WriteMemInt8(m_addr++, imm);
        return *this;
    }

private:
    unsigned m_addr, m_endAddr;

    void writeModRm(const AsmRegMem &regMem, uint8_t regField)
    {
        uint8_t modField = 0;
        if (!regMem.memory)
            modField = 3;
        else if (regMem.displacement == 0 || !regMem.regOpt)
            modField = 0;
        else if (abs(regMem.displacement) < 128)
            modField = 1;
        else
            modField = 2;

        uint8_t rmField = regMem.regOpt ? regMem.regOpt.value().regNum : 5;
        uint8_t modRegRmByte = (modField << 6) | (regField << 3) | rmField;
        WriteMemUInt8(m_addr++, modRegRmByte);

        // TODO: full SIB support?
        if (rmField == 4 && modField != 3)
        {
            uint8_t scaleField = 0;
            uint8_t indexField = 4; // no index
            uint8_t baseField = 4; // esp
            uint8_t sibByte = (scaleField << 6) | (indexField << 3) | baseField;
            WriteMemUInt8(m_addr++, sibByte);
        }

        if (modField == 1)
            WriteMemInt8(m_addr++, regMem.displacement);
        else if (modField == 2 || !regMem.regOpt)
            WriteMemInt32(m_addr++, regMem.displacement);
    }

    uint8_t modRegRmByte()
    {
        

    }
};
