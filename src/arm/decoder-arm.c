/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "decoder.h"

#include "decoder-inlines.h"
#include "emitter-arm.h"
#include "isa-inlines.h"

#define ADDR_MODE_1_SHIFT(OP) \
	info->op3.a.reg = opcode & 0x0000000F; \
	info->op3.a.shifterOp = ARM_SHIFT_ ## OP; \
	info->operandFormat |= ARM_OPERAND_REGISTER_3; \
	if (opcode & 0x00000010) { \
		info->op3.a.b.shifterReg = (opcode >> 8) & 0xF; \
		++info->iCycles; \
		info->operandFormat |= ARM_OPERAND_SHIFT_REGISTER_3; \
	} else { \
		info->op3.a.b.shifterImm = (opcode >> 7) & 0x1F; \
		info->operandFormat |= ARM_OPERAND_SHIFT_IMMEDIATE_3; \
	}

#define ADDR_MODE_1_LSL \
	ADDR_MODE_1_SHIFT(LSL) \
	if (!info->op3.a.b.shifterImm) { \
		info->operandFormat &= ~ARM_OPERAND_SHIFT_IMMEDIATE_3; \
		info->op3.a.shifterOp = ARM_SHIFT_NONE; \
	}

#define ADDR_MODE_1_LSR ADDR_MODE_1_SHIFT(LSR)
#define ADDR_MODE_1_ASR ADDR_MODE_1_SHIFT(ASR)
#define ADDR_MODE_1_ROR \
	ADDR_MODE_1_SHIFT(ROR) \
	if (!info->op3.a.b.shifterImm) { \
		info->op3.a.shifterOp = ARM_SHIFT_RRX; \
	}

#define ADDR_MODE_1_IMM \
	int rotate = (opcode & 0x00000F00) >> 7; \
	int immediate = opcode & 0x000000FF; \
	info->op3.immediate = ROR(immediate, rotate); \
	info->operandFormat |= ARM_OPERAND_IMMEDIATE_3;

#define ADDR_MODE_2_SHIFT(OP) \
	info->memory.format |= ARM_MEMORY_REGISTER_OFFSET | ARM_MEMORY_SHIFTED_OFFSET; \
	info->memory.offset.a.shifterOp = ARM_SHIFT_ ## OP; \
	info->memory.offset.a.b.shifterImm = (opcode >> 7) & 0x1F; \
	info->memory.offset.a.reg = opcode & 0x0000000F;

#define ADDR_MODE_2_LSL \
	ADDR_MODE_2_SHIFT(LSL) \
	if (!info->memory.offset.a.b.shifterImm) { \
		info->memory.format &= ~ARM_MEMORY_SHIFTED_OFFSET; \
		info->memory.offset.a.shifterOp = ARM_SHIFT_NONE; \
	}

#define ADDR_MODE_2_LSR ADDR_MODE_2_SHIFT(LSR) \
	if (!info->memory.offset.a.b.shifterImm) { \
		info->memory.offset.a.b.shifterImm = 32; \
	}

#define ADDR_MODE_2_ASR ADDR_MODE_2_SHIFT(ASR) \
	if (!info->memory.offset.a.b.shifterImm) { \
		info->memory.offset.a.b.shifterImm = 32; \
	}

#define ADDR_MODE_2_ROR \
	ADDR_MODE_2_SHIFT(ROR) \
	if (!info->memory.offset.a.b.shifterImm) { \
		info->memory.offset.a.shifterOp = ARM_SHIFT_RRX; \
	}

#define ADDR_MODE_2_IMM \
	info->memory.format |= ARM_MEMORY_IMMEDIATE_OFFSET; \
	info->memory.offset.immediate = opcode & 0x00000FFF;

#define ADDR_MODE_3_REG \
	info->memory.format |= ARM_MEMORY_REGISTER_OFFSET; \
	info->memory.offset.a.reg = opcode & 0x0000000F;

#define ADDR_MODE_3_IMM \
	info->memory.format |= ARM_MEMORY_IMMEDIATE_OFFSET; \
	info->memory.offset.immediate = (opcode & 0x0000000F) | ((opcode & 0x00000F00) >> 4);

#define DEFINE_DECODER_ARM(NAME, MNEMONIC, BODY) \
	static void _ARMDecode ## NAME (uint32_t opcode, struct ARMInstructionInfo* info) { \
		UNUSED(opcode); \
		info->mnemonic = ARM_MN_ ## MNEMONIC; \
		BODY; \
	}

#define DEFINE_ALU_DECODER_EX_ARM(NAME, MNEMONIC, S, SHIFTER, OTHER_AFFECTED, SKIPPED) \
	DEFINE_DECODER_ARM(NAME, MNEMONIC, \
		info->op1.a.reg = (opcode >> 12) & 0xF; \
		info->op2.a.reg = (opcode >> 16) & 0xF; \
		info->operandFormat = ARM_OPERAND_REGISTER_1 | \
			OTHER_AFFECTED | \
			ARM_OPERAND_REGISTER_2; \
		info->affectsCPSR = S; \
		SHIFTER; \
		if (SKIPPED == 1) { \
			info->op1 = info->op2; \
			info->op2 = info->op3; \
			info->operandFormat >>= 8; \
		} else if (SKIPPED == 2) { \
			info->op2 = info->op3; \
			info->operandFormat |= info->operandFormat >> 8; \
			info->operandFormat &= ~ARM_OPERAND_3; \
		} \
		if (info->op1.a.reg == ARM_PC) { \
			info->branchType = ARM_BRANCH_INDIRECT; \
		})

#define DEFINE_ALU_DECODER_ARM(NAME, SKIPPED) \
	DEFINE_ALU_DECODER_EX_ARM(NAME ## _LSL, NAME, 0, ADDR_MODE_1_LSL, ARM_OPERAND_AFFECTED_1, SKIPPED) \
	DEFINE_ALU_DECODER_EX_ARM(NAME ## S_LSL, NAME, 1, ADDR_MODE_1_LSL, ARM_OPERAND_AFFECTED_1, SKIPPED) \
	DEFINE_ALU_DECODER_EX_ARM(NAME ## _LSR, NAME, 0, ADDR_MODE_1_LSR, ARM_OPERAND_AFFECTED_1, SKIPPED) \
	DEFINE_ALU_DECODER_EX_ARM(NAME ## S_LSR, NAME, 1, ADDR_MODE_1_LSR, ARM_OPERAND_AFFECTED_1, SKIPPED) \
	DEFINE_ALU_DECODER_EX_ARM(NAME ## _ASR, NAME, 0, ADDR_MODE_1_ASR, ARM_OPERAND_AFFECTED_1, SKIPPED) \
	DEFINE_ALU_DECODER_EX_ARM(NAME ## S_ASR, NAME, 1, ADDR_MODE_1_ASR, ARM_OPERAND_AFFECTED_1, SKIPPED) \
	DEFINE_ALU_DECODER_EX_ARM(NAME ## _ROR, NAME, 0, ADDR_MODE_1_ROR, ARM_OPERAND_AFFECTED_1, SKIPPED) \
	DEFINE_ALU_DECODER_EX_ARM(NAME ## S_ROR, NAME, 1, ADDR_MODE_1_ROR, ARM_OPERAND_AFFECTED_1, SKIPPED) \
	DEFINE_ALU_DECODER_EX_ARM(NAME ## I, NAME, 0, ADDR_MODE_1_IMM, ARM_OPERAND_AFFECTED_1, SKIPPED) \
	DEFINE_ALU_DECODER_EX_ARM(NAME ## SI, NAME, 1, ADDR_MODE_1_IMM, ARM_OPERAND_AFFECTED_1, SKIPPED)

#define DEFINE_ALU_DECODER_S_ONLY_ARM(NAME) \
	DEFINE_ALU_DECODER_EX_ARM(NAME ## _LSL, NAME, 1, ADDR_MODE_1_LSL, ARM_OPERAND_NONE, 1) \
	DEFINE_ALU_DECODER_EX_ARM(NAME ## _LSR, NAME, 1, ADDR_MODE_1_LSR, ARM_OPERAND_NONE, 1) \
	DEFINE_ALU_DECODER_EX_ARM(NAME ## _ASR, NAME, 1, ADDR_MODE_1_ASR, ARM_OPERAND_NONE, 1) \
	DEFINE_ALU_DECODER_EX_ARM(NAME ## _ROR, NAME, 1, ADDR_MODE_1_ROR, ARM_OPERAND_NONE, 1) \
	DEFINE_ALU_DECODER_EX_ARM(NAME ## I, NAME, 1, ADDR_MODE_1_IMM, ARM_OPERAND_NONE, 1)

#define DEFINE_MULTIPLY_DECODER_EX_ARM(NAME, MNEMONIC, S, OTHER_AFFECTED) \
	DEFINE_DECODER_ARM(NAME, MNEMONIC, \
		info->op1.a.reg = (opcode >> 16) & 0xF; \
		info->op2.a.reg = opcode & 0xF; \
		info->op3.a.reg = (opcode >> 8) & 0xF; \
		info->op4.a.reg = (opcode >> 12) & 0xF; \
		info->operandFormat = ARM_OPERAND_REGISTER_1 | \
			ARM_OPERAND_AFFECTED_1 | \
			ARM_OPERAND_REGISTER_2 | \
			ARM_OPERAND_REGISTER_3 | \
			OTHER_AFFECTED; \
		info->affectsCPSR = S; \
		if (info->op1.a.reg == ARM_PC) { \
			info->branchType = ARM_BRANCH_INDIRECT; \
		})

#define DEFINE_LONG_MULTIPLY_DECODER_EX_ARM(NAME, MNEMONIC, S) \
	DEFINE_DECODER_ARM(NAME, MNEMONIC, \
		info->op1.a.reg = (opcode >> 12) & 0xF; \
		info->op2.a.reg = (opcode >> 16) & 0xF; \
		info->op3.a.reg = opcode & 0xF; \
		info->op4.a.reg = (opcode >> 8) & 0xF; \
		info->operandFormat = ARM_OPERAND_REGISTER_1 | \
			ARM_OPERAND_AFFECTED_1 | \
			ARM_OPERAND_REGISTER_2 | \
			ARM_OPERAND_AFFECTED_2 | \
			ARM_OPERAND_REGISTER_3 | \
			ARM_OPERAND_REGISTER_4; \
		info->affectsCPSR = S; \
		if (info->op1.a.reg == ARM_PC) { \
			info->branchType = ARM_BRANCH_INDIRECT; \
		})

#define DEFINE_MULTIPLY_DECODER_ARM(NAME, OTHER_AFFECTED) \
	DEFINE_MULTIPLY_DECODER_EX_ARM(NAME, NAME, 0, OTHER_AFFECTED) \
	DEFINE_MULTIPLY_DECODER_EX_ARM(NAME ## S, NAME, 1, OTHER_AFFECTED)

#define DEFINE_LONG_MULTIPLY_DECODER_ARM(NAME) \
	DEFINE_LONG_MULTIPLY_DECODER_EX_ARM(NAME, NAME, 0) \
	DEFINE_LONG_MULTIPLY_DECODER_EX_ARM(NAME ## S, NAME, 1)

#define DEFINE_LOAD_STORE_DECODER_EX_ARM(NAME, MNEMONIC, ADDRESSING_MODE, ADDRESSING_DECODING, CYCLES, TYPE) \
	DEFINE_DECODER_ARM(NAME, MNEMONIC, \
		info->op1.a.reg = (opcode >> 12) & 0xF; \
		info->memory.baseReg = (opcode >> 16) & 0xF; \
		info->memory.width = TYPE; \
		info->operandFormat = ARM_OPERAND_REGISTER_1 | \
			ARM_OPERAND_AFFECTED_1 | /* TODO: Remove this for STR */ \
			ARM_OPERAND_MEMORY_2; \
		info->memory.format = ARM_MEMORY_REGISTER_BASE | ADDRESSING_MODE; \
		ADDRESSING_DECODING; \
		CYCLES;)

#define DEFINE_LOAD_STORE_DECODER_SET_ARM(NAME, MNEMONIC, ADDRESSING_MODE, CYCLES, TYPE) \
	DEFINE_LOAD_STORE_DECODER_EX_ARM(NAME, MNEMONIC, \
		ARM_MEMORY_POST_INCREMENT | \
		ARM_MEMORY_WRITEBACK | \
		ARM_MEMORY_OFFSET_SUBTRACT, \
		ADDRESSING_MODE, CYCLES, TYPE) \
	DEFINE_LOAD_STORE_DECODER_EX_ARM(NAME ## U, MNEMONIC, \
		ARM_MEMORY_POST_INCREMENT | \
		ARM_MEMORY_WRITEBACK, \
		ADDRESSING_MODE, CYCLES, TYPE) \
	DEFINE_LOAD_STORE_DECODER_EX_ARM(NAME ## P, MNEMONIC, \
		ARM_MEMORY_OFFSET_SUBTRACT, \
		ADDRESSING_MODE, CYCLES, TYPE) \
	DEFINE_LOAD_STORE_DECODER_EX_ARM(NAME ## PW, MNEMONIC, \
		ARM_MEMORY_PRE_INCREMENT | \
		ARM_MEMORY_WRITEBACK | \
		ARM_MEMORY_OFFSET_SUBTRACT, \
		ADDRESSING_MODE, CYCLES, TYPE) \
	DEFINE_LOAD_STORE_DECODER_EX_ARM(NAME ## PU, MNEMONIC, \
		0, \
		ADDRESSING_MODE, CYCLES, TYPE) \
	DEFINE_LOAD_STORE_DECODER_EX_ARM(NAME ## PUW, MNEMONIC, \
		ARM_MEMORY_WRITEBACK, \
		ADDRESSING_MODE, CYCLES, TYPE)

#define DEFINE_LOAD_STORE_MODE_2_DECODER_ARM(NAME, MNEMONIC, CYCLES, TYPE) \
	DEFINE_LOAD_STORE_DECODER_SET_ARM(NAME ## _LSL_, MNEMONIC, ADDR_MODE_2_LSL, CYCLES, TYPE) \
	DEFINE_LOAD_STORE_DECODER_SET_ARM(NAME ## _LSR_, MNEMONIC, ADDR_MODE_2_LSR, CYCLES, TYPE) \
	DEFINE_LOAD_STORE_DECODER_SET_ARM(NAME ## _ASR_, MNEMONIC, ADDR_MODE_2_ASR, CYCLES, TYPE) \
	DEFINE_LOAD_STORE_DECODER_SET_ARM(NAME ## _ROR_, MNEMONIC, ADDR_MODE_2_ROR, CYCLES, TYPE) \
	DEFINE_LOAD_STORE_DECODER_SET_ARM(NAME ## I, MNEMONIC, ADDR_MODE_2_IMM, CYCLES, TYPE)

#define DEFINE_LOAD_STORE_MODE_3_DECODER_ARM(NAME, MNEMONIC, CYCLES, TYPE) \
	DEFINE_LOAD_STORE_DECODER_SET_ARM(NAME, MNEMONIC, ADDR_MODE_3_REG, CYCLES, TYPE) \
	DEFINE_LOAD_STORE_DECODER_SET_ARM(NAME ## I, MNEMONIC, ADDR_MODE_3_IMM, CYCLES, TYPE)

#define DEFINE_LOAD_STORE_T_DECODER_SET_ARM(NAME, MNEMONIC, ADDRESSING_MODE, CYCLES, TYPE) \
	DEFINE_LOAD_STORE_DECODER_EX_ARM(NAME, MNEMONIC, \
		ARM_MEMORY_POST_INCREMENT | \
		ARM_MEMORY_WRITEBACK | \
		ARM_MEMORY_OFFSET_SUBTRACT, \
		ADDRESSING_MODE, CYCLES, TYPE) \
	DEFINE_LOAD_STORE_DECODER_EX_ARM(NAME ## U, MNEMONIC, \
		ARM_MEMORY_POST_INCREMENT | \
		ARM_MEMORY_WRITEBACK, \
		ADDRESSING_MODE, CYCLES, TYPE)

#define DEFINE_LOAD_STORE_T_DECODER_ARM(NAME, MNEMONIC, CYCLES, TYPE) \
	DEFINE_LOAD_STORE_T_DECODER_SET_ARM(NAME ## _LSL_, MNEMONIC, ADDR_MODE_2_LSL, CYCLES, TYPE) \
	DEFINE_LOAD_STORE_T_DECODER_SET_ARM(NAME ## _LSR_, MNEMONIC, ADDR_MODE_2_LSR, CYCLES, TYPE) \
	DEFINE_LOAD_STORE_T_DECODER_SET_ARM(NAME ## _ASR_, MNEMONIC, ADDR_MODE_2_ASR, CYCLES, TYPE) \
	DEFINE_LOAD_STORE_T_DECODER_SET_ARM(NAME ## _ROR_, MNEMONIC, ADDR_MODE_2_ROR, CYCLES, TYPE) \
	DEFINE_LOAD_STORE_T_DECODER_SET_ARM(NAME ## I, MNEMONIC, ADDR_MODE_2_IMM, CYCLES, TYPE)

#define DEFINE_LOAD_STORE_MULTIPLE_DECODER_EX_ARM(NAME, MNEMONIC, DIRECTION, WRITEBACK) \
	DEFINE_DECODER_ARM(NAME, MNEMONIC, \
		info->memory.baseReg = (opcode >> 16) & 0xF; \
		info->op1.immediate = opcode & 0x0000FFFF; \
		if (info->op1.immediate & (1 << ARM_PC)) { \
			info->branchType = ARM_BRANCH_INDIRECT; \
		} \
		info->operandFormat = ARM_OPERAND_MEMORY_1; \
		info->memory.format = ARM_MEMORY_REGISTER_BASE | \
			WRITEBACK | \
			ARM_MEMORY_ ## DIRECTION;)


#define DEFINE_LOAD_STORE_MULTIPLE_DECODER_ARM(NAME) \
	DEFINE_LOAD_STORE_MULTIPLE_DECODER_EX_ARM(NAME ## DA,   NAME, DECREMENT_AFTER, 0) \
	DEFINE_LOAD_STORE_MULTIPLE_DECODER_EX_ARM(NAME ## DAW,  NAME, DECREMENT_AFTER, ARM_MEMORY_WRITEBACK) \
	DEFINE_LOAD_STORE_MULTIPLE_DECODER_EX_ARM(NAME ## DB,   NAME, DECREMENT_BEFORE, 0) \
	DEFINE_LOAD_STORE_MULTIPLE_DECODER_EX_ARM(NAME ## DBW,  NAME, DECREMENT_BEFORE, ARM_MEMORY_WRITEBACK) \
	DEFINE_LOAD_STORE_MULTIPLE_DECODER_EX_ARM(NAME ## IA,   NAME, INCREMENT_AFTER, 0) \
	DEFINE_LOAD_STORE_MULTIPLE_DECODER_EX_ARM(NAME ## IAW,  NAME, INCREMENT_AFTER, ARM_MEMORY_WRITEBACK) \
	DEFINE_LOAD_STORE_MULTIPLE_DECODER_EX_ARM(NAME ## IB,   NAME, INCREMENT_BEFORE, 0) \
	DEFINE_LOAD_STORE_MULTIPLE_DECODER_EX_ARM(NAME ## IBW,  NAME, INCREMENT_BEFORE, ARM_MEMORY_WRITEBACK) \
	DEFINE_LOAD_STORE_MULTIPLE_DECODER_EX_ARM(NAME ## SDA,  NAME, DECREMENT_AFTER, ARM_MEMORY_SPSR_SWAP) \
	DEFINE_LOAD_STORE_MULTIPLE_DECODER_EX_ARM(NAME ## SDAW, NAME, DECREMENT_AFTER, ARM_MEMORY_WRITEBACK | ARM_MEMORY_SPSR_SWAP) \
	DEFINE_LOAD_STORE_MULTIPLE_DECODER_EX_ARM(NAME ## SDB,  NAME, DECREMENT_BEFORE, ARM_MEMORY_SPSR_SWAP) \
	DEFINE_LOAD_STORE_MULTIPLE_DECODER_EX_ARM(NAME ## SDBW, NAME, DECREMENT_BEFORE, ARM_MEMORY_WRITEBACK | ARM_MEMORY_SPSR_SWAP) \
	DEFINE_LOAD_STORE_MULTIPLE_DECODER_EX_ARM(NAME ## SIA,  NAME, INCREMENT_AFTER, ARM_MEMORY_SPSR_SWAP) \
	DEFINE_LOAD_STORE_MULTIPLE_DECODER_EX_ARM(NAME ## SIAW, NAME, INCREMENT_AFTER, ARM_MEMORY_WRITEBACK | ARM_MEMORY_SPSR_SWAP) \
	DEFINE_LOAD_STORE_MULTIPLE_DECODER_EX_ARM(NAME ## SIB,  NAME, INCREMENT_BEFORE, ARM_MEMORY_SPSR_SWAP) \
	DEFINE_LOAD_STORE_MULTIPLE_DECODER_EX_ARM(NAME ## SIBW, NAME, INCREMENT_BEFORE, ARM_MEMORY_WRITEBACK | ARM_MEMORY_SPSR_SWAP)

#define DEFINE_SWP_DECODER_ARM(NAME, TYPE) \
	DEFINE_DECODER_ARM(NAME, SWP, \
		info->memory.baseReg = (opcode >> 16) & 0xF; \
		info->op1.a.reg = (opcode >> 12) & 0xF; \
		info->op2.a.reg = opcode & 0xF; \
		info->operandFormat = ARM_OPERAND_REGISTER_1 | \
			ARM_OPERAND_AFFECTED_1 | \
			ARM_OPERAND_REGISTER_2 | \
			ARM_OPERAND_MEMORY_3; \
		info->memory.format = ARM_MEMORY_REGISTER_BASE; \
		info->memory.width = TYPE;)

DEFINE_ALU_DECODER_ARM(ADD, 0)
DEFINE_ALU_DECODER_ARM(ADC, 0)
DEFINE_ALU_DECODER_ARM(AND, 0)
DEFINE_ALU_DECODER_ARM(BIC, 0)
DEFINE_ALU_DECODER_S_ONLY_ARM(CMN)
DEFINE_ALU_DECODER_S_ONLY_ARM(CMP)
DEFINE_ALU_DECODER_ARM(EOR, 0)
DEFINE_ALU_DECODER_ARM(MOV, 2)
DEFINE_ALU_DECODER_ARM(MVN, 2)
DEFINE_ALU_DECODER_ARM(ORR, 0)
DEFINE_ALU_DECODER_ARM(RSB, 0)
DEFINE_ALU_DECODER_ARM(RSC, 0)
DEFINE_ALU_DECODER_ARM(SBC, 0)
DEFINE_ALU_DECODER_ARM(SUB, 0)
DEFINE_ALU_DECODER_S_ONLY_ARM(TEQ)
DEFINE_ALU_DECODER_S_ONLY_ARM(TST)

// TOOD: Estimate cycles
DEFINE_MULTIPLY_DECODER_ARM(MLA, ARM_OPERAND_REGISTER_4)
DEFINE_MULTIPLY_DECODER_ARM(MUL, ARM_OPERAND_NONE)

DEFINE_LONG_MULTIPLY_DECODER_ARM(SMLAL)
DEFINE_LONG_MULTIPLY_DECODER_ARM(SMULL)
DEFINE_LONG_MULTIPLY_DECODER_ARM(UMLAL)
DEFINE_LONG_MULTIPLY_DECODER_ARM(UMULL)

// Begin load/store definitions

DEFINE_LOAD_STORE_MODE_2_DECODER_ARM(LDR, LDR, LOAD_CYCLES, ARM_ACCESS_WORD)
DEFINE_LOAD_STORE_MODE_2_DECODER_ARM(LDRB, LDR, LOAD_CYCLES, ARM_ACCESS_BYTE)
DEFINE_LOAD_STORE_MODE_3_DECODER_ARM(LDRH, LDR, LOAD_CYCLES, ARM_ACCESS_HALFWORD)
DEFINE_LOAD_STORE_MODE_3_DECODER_ARM(LDRSB, LDR, LOAD_CYCLES, ARM_ACCESS_SIGNED_BYTE)
DEFINE_LOAD_STORE_MODE_3_DECODER_ARM(LDRSH, LDR, LOAD_CYCLES, ARM_ACCESS_SIGNED_HALFWORD)
DEFINE_LOAD_STORE_MODE_2_DECODER_ARM(STR, STR, STORE_CYCLES, ARM_ACCESS_WORD)
DEFINE_LOAD_STORE_MODE_2_DECODER_ARM(STRB, STR, STORE_CYCLES, ARM_ACCESS_BYTE)
DEFINE_LOAD_STORE_MODE_3_DECODER_ARM(STRH, STR, STORE_CYCLES, ARM_ACCESS_HALFWORD)

DEFINE_LOAD_STORE_T_DECODER_ARM(LDRBT, LDR, LOAD_CYCLES, ARM_ACCESS_TRANSLATED_BYTE)
DEFINE_LOAD_STORE_T_DECODER_ARM(LDRT, LDR, LOAD_CYCLES, ARM_ACCESS_TRANSLATED_WORD)
DEFINE_LOAD_STORE_T_DECODER_ARM(STRBT, STR, STORE_CYCLES, ARM_ACCESS_TRANSLATED_BYTE)
DEFINE_LOAD_STORE_T_DECODER_ARM(STRT, STR, STORE_CYCLES, ARM_ACCESS_TRANSLATED_WORD)

DEFINE_LOAD_STORE_MULTIPLE_DECODER_ARM(LDM)
DEFINE_LOAD_STORE_MULTIPLE_DECODER_ARM(STM)

DEFINE_SWP_DECODER_ARM(SWP, ARM_ACCESS_WORD)
DEFINE_SWP_DECODER_ARM(SWPB, ARM_ACCESS_BYTE)

// End load/store definitions

// Begin branch definitions

DEFINE_DECODER_ARM(B, B,
	int32_t offset = opcode << 8;
	info->op1.immediate = offset >> 6;
	info->operandFormat = ARM_OPERAND_IMMEDIATE_1;
	info->branchType = ARM_BRANCH;)

DEFINE_DECODER_ARM(BL, BL,
	int32_t offset = opcode << 8;
	info->op1.immediate = offset >> 6;
	info->operandFormat = ARM_OPERAND_IMMEDIATE_1;
	info->branchType = ARM_BRANCH_LINKED;)

DEFINE_DECODER_ARM(BX, BX,
	info->op1.a.reg = opcode & 0x0000000F;
	info->operandFormat = ARM_OPERAND_REGISTER_1;
	info->branchType = ARM_BRANCH_INDIRECT;)

// End branch definitions

// Begin coprocessor definitions

DEFINE_DECODER_ARM(CDP, ILL, info->operandFormat = ARM_OPERAND_NONE;)
DEFINE_DECODER_ARM(LDC, ILL, info->operandFormat = ARM_OPERAND_NONE;)
DEFINE_DECODER_ARM(STC, ILL, info->operandFormat = ARM_OPERAND_NONE;)
DEFINE_DECODER_ARM(MCR, ILL, info->operandFormat = ARM_OPERAND_NONE;)
DEFINE_DECODER_ARM(MRC, ILL, info->operandFormat = ARM_OPERAND_NONE;)

// Begin miscellaneous definitions

DEFINE_DECODER_ARM(BKPT, BKPT,
	info->operandFormat = ARM_OPERAND_NONE;
	info->traps = 1;) // Not strictly in ARMv4T, but here for convenience
DEFINE_DECODER_ARM(ILL, ILL,
	info->operandFormat = ARM_OPERAND_NONE;
	info->traps = 1;) // Illegal opcode

DEFINE_DECODER_ARM(MSR, MSR,
	info->affectsCPSR = 1;
	info->op1.a.reg = ARM_CPSR;
	info->op1.a.b.psrBits = (opcode >> 16) & ARM_PSR_MASK;
	info->op2.a.reg = opcode & 0x0000000F;
	info->operandFormat = ARM_OPERAND_REGISTER_1 |
		ARM_OPERAND_AFFECTED_1 |
		ARM_OPERAND_REGISTER_2;)

DEFINE_DECODER_ARM(MSRR, MSR,
	info->op1.a.reg = ARM_SPSR;
	info->op1.a.b.psrBits = (opcode >> 16) & ARM_PSR_MASK;
	info->op2.a.reg = opcode & 0x0000000F;
	info->operandFormat = ARM_OPERAND_REGISTER_1 |
		ARM_OPERAND_AFFECTED_1 |
		ARM_OPERAND_REGISTER_2;)

DEFINE_DECODER_ARM(MRS, MRS,
	info->affectsCPSR = 1;
	info->op1.a.reg = (opcode >> 12) & 0xF;
	info->op2.a.reg = ARM_CPSR;
	info->op2.a.b.psrBits = 0;
	info->operandFormat = ARM_OPERAND_REGISTER_1 |
		ARM_OPERAND_AFFECTED_1 |
		ARM_OPERAND_REGISTER_2;)

DEFINE_DECODER_ARM(MRSR, MRS,
	info->op1.a.reg = (opcode >> 12) & 0xF;
	info->op2.a.reg = ARM_SPSR;
	info->op2.a.b.psrBits = 0;
	info->operandFormat = ARM_OPERAND_REGISTER_1 |
		ARM_OPERAND_AFFECTED_1 |
		ARM_OPERAND_REGISTER_2;)

DEFINE_DECODER_ARM(MSRI, MSR,
	int rotate = (opcode & 0x00000F00) >> 7;
	int32_t operand = ROR(opcode & 0x000000FF, rotate);
	info->affectsCPSR = 1;
	info->op1.a.reg = ARM_CPSR;
	info->op1.a.b.psrBits = (opcode >> 16) & ARM_PSR_MASK;
	info->op2.immediate = operand;
	info->operandFormat = ARM_OPERAND_REGISTER_1 |
		ARM_OPERAND_AFFECTED_1 |
		ARM_OPERAND_IMMEDIATE_2;)

DEFINE_DECODER_ARM(MSRRI, MSR,
	int rotate = (opcode & 0x00000F00) >> 7;
	int32_t operand = ROR(opcode & 0x000000FF, rotate);
	info->op1.a.reg = ARM_SPSR;
	info->op1.a.b.psrBits = (opcode >> 16) & ARM_PSR_MASK;
	info->op2.immediate = operand;
	info->operandFormat = ARM_OPERAND_REGISTER_1 |
		ARM_OPERAND_AFFECTED_1 |
		ARM_OPERAND_IMMEDIATE_2;)

DEFINE_DECODER_ARM(SWI, SWI,
	info->op1.immediate = opcode & 0xFFFFFF;
	info->operandFormat = ARM_OPERAND_IMMEDIATE_1;
	info->traps = 1;)

typedef void (*ARMDecoder)(uint32_t opcode, struct ARMInstructionInfo* info);

static const ARMDecoder _armDecoderTable[0x1000] = {
	DECLARE_ARM_EMITTER_BLOCK(_ARMDecode)
};

void ARMDecodeARM(uint32_t opcode, struct ARMInstructionInfo* info) {
	memset(info, 0, sizeof(*info));
	info->execMode = MODE_ARM;
	info->opcode = opcode;
	info->branchType = ARM_BRANCH_NONE;
	info->condition = opcode >> 28;
	info->sInstructionCycles = 1;
	ARMDecoder decoder = _armDecoderTable[((opcode >> 16) & 0xFF0) | ((opcode >> 4) & 0x00F)];
	decoder(opcode, info);
}
