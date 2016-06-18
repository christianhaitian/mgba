/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GB_SERIALIZE_H
#define GB_SERIALIZE_H

#include "util/common.h"

#include "core/core.h"
#include "gb/gb.h"

extern const uint32_t GB_SAVESTATE_MAGIC;
extern const uint32_t GB_SAVESTATE_VERSION;

mLOG_DECLARE_CATEGORY(GB_STATE);

/* Savestate format:
 * 0x00000 - 0x00003: Version Magic (0x01000001)
 * 0x00004 - 0x00007: ROM CRC32
 * 0x00008: Game Boy model
 * 0x00009 - 0x0000F: Reserved (leave zero)
 * 0x00010 - 0x0001F: Game title/code (e.g. PM_CRYSTALBYTE)
 * 0x00020 - 0x00047: CPU state:
 * | 0x00020: A register
 * | 0x00021: F register
 * | 0x00022: B register
 * | 0x00023: C register
 * | 0x00024: D register
 * | 0x00025: E register
 * | 0x00026: H register
 * | 0x00027: L register
 * | 0x00028 - 0z00029: SP register
 * | 0x0002A - 0z0002B: PC register
 * | 0x0002C - 0x0002F: Cycles since last event
 * | 0x00030 - 0x00033: Cycles until next event
 * | 0x00034 - 0x00035: Reserved (current instruction)
 * | 0x00036 - 0x00037: Index address
 * | 0x00038: Bus value
 * | 0x00039: Execution state
 * | 0x0003A - 0x0003B: IRQ vector
 * | 0x0003C - 0x0003F: EI pending cycles
 * | 0x00040 - 0x00043: Reserved (DI pending cycles)
 * | 0x00044 - 0x00047: Flags
 *   | bit 0: Is condition met?
 *   | bit 1: Is condition IRQ pending?
 *   | bit 2: Double speed
 *   | bits 3 - 31: Reserved
 * 0x00048 - 0x0005B: Audio channel 1/framer state
 * | 0x00048 - 0x0004B: Envelepe timing
 *   | bits 0 - 6: Remaining length
 *   | bits 7 - 9: Next step
 *   | bits 10 - 20: Shadow frequency register
 *   | bits 21 - 31: Reserved
 * | 0x0004C - 0x0004F: Next frame
 * | 0x00050 - 0x00057: Reserved
 * | 0x00058 - 0x0005B: Next event
 * 0x0005C - 0x0006B: Audio channel 2 state
 * | 0x0005C - 0x0005F: Envelepe timing
 *   | bits 0 - 2: Remaining length
 *   | bits 3 - 5: Next step
 *   | bits 6 - 31: Reserved
 * | 0x00060 - 0x00067: Reserved
 * | 0x00068 - 0x0006B: Next event
 * 0x0006C - 0x00093: Audio channel 3 state
 * | 0x0006C - 0x0008B: Wave banks
 * | 0x0008C - 0x0008D: Remaining length
 * | 0x0008E - 0x0008F: Reserved
 * | 0x00090 - 0x00093: Next event
 * 0x00094 - 0x000A3: Audio channel 4 state
 * | 0x00094 - 0x00097: Linear feedback shift register state
 * | 0x00098 - 0x0009B: Envelepe timing
 *   | bits 0 - 2: Remaining length
 *   | bits 3 - 5: Next step
 *   | bits 6 - 31: Reserved
 * | 0x00098 - 0x0009F: Reserved
 * | 0x000A0 - 0x000A3: Next event
 * 0x000A4 - 0x000B7: Audio miscellaneous state
 * | TODO: Fix this, they're in big-endian order, but field is little-endian
 * | 0x000A4: Channel 1 envelope state
 *   | bits 0 - 3: Current volume
 *   | bits 4 - 5: Is dead?
 *   | bit 6: Is high?
 * | 0x000A5: Channel 2 envelope state
 *   | bits 0 - 3: Current volume
 *   | bits 4 - 5: Is dead?
 *   | bit 6: Is high?
*    | bits 7: Reserved
 * | 0x000A6: Channel 4 envelope state
 *   | bits 0 - 3: Current volume
 *   | bits 4 - 5: Is dead?
 *   | bit 6: Is high?
*    | bits 7: Reserved
 * | 0x000A7: Miscellaneous audio flags
 *   | bits 0 - 3: Current frame
 *   | bit 4: Is channel 1 sweep enabled?
 *   | bit 5: Has channel 1 sweep occurred?
 *   | bits 6 - 7: Reserved
 * | 0x000A8 - 0x000AB: Next event
 * | 0x000AC - 0x000AF: Event diff
 * | 0x000B0 - 0x000B3: Next sample
 * 0x000B4 - 0x000153: Video state
 * | 0x000B4 - 0x000B5: Current x
 * | 0x000B6 - 0x000B7: Current y (ly)
 * | 0x000B8 - 0x000BB: Next event
 * | 0x000BC - 0x000BF: Event diff
 * | 0x000C0 - 0x000C3: Next mode
 * | 0x000C4 - 0x000C7: Dot cycle counter
 * | 0x000C8 - 0x000CB: Frame counter
 * | 0x000CC: Current VRAM bank
 * | 0x000CD: Palette flags
 *   | bit 0: BCP increment
 *   | bit 1: OCP increment
 *   | bits 2 - 7: Reserved
 * | 0x000CE - 0x000CF: Reserved
 * | 0x000D0 - 0x000D1: BCP index
 * | 0x000D1 - 0x000D3: OCP index
 * | 0x000D4 - 0x00153: Palette entries
 * 0x00154 - 0x000167: Timer state
 * | 0x00154 - 0x00157: Next event
 * | 0x00158 - 0x0015B: Event diff
 * | 0x0015C - 0x0015F: Next DIV
 * | 0x00160 - 0x00163: Next TIMA
 * | 0x00164 - 0x00167: TIMA period
 * 0x000168 - 0x000197: Memory state
 * | 0x00168 - 0x00169: Current ROM bank
 * | 0x0016A: Current WRAM bank
 * | 0x0016B: Current SRAM bank
 * | 0x0016C - 0x0016F: Next DMA
 * | 0x00170 - 0x00171: Next DMA source
 * | 0x00172 - 0x00173: Next DMA destination
 * | 0x00174 - 0x00177: Next HDMA
 * | 0x00178 - 0x00179: Next HDMA source
 * | 0x0017A - 0x0017B: Next HDMA destination
 * | 0x0017C - 0x0017D: HDMA remaining
 * | 0x0017E: DMA remaining
 * | 0x0017F - 0x00183: RTC registers
 * | 0x00184 - 0x00193: MBC state (TODO)
 * | 0x00194 - 0x00195: Flags
 *   | bit 0: SRAM accessable
 *   | bit 1: RTC accessible
 *   | bit 2: RTC latched
 *   | bit 3: IME
 *   | bit 4: Is HDMA active?
 *   | bits 5 - 7:  Active RTC register
 * | 0x00196 - 0x00197: Reserved (leave zero)
 * 0x00198 - 0x0019F: Savestate creation time (usec since 1970)
 * 0x001A0 - 0x0025F: Reserved (leave zero)
 * 0x00260 - 0x002FF: OAM
 * 0x00300 - 0x0037F: I/O memory
 * 0x00380 - 0x003FE: HRAM
 * 0x003FF: Interrupts enabled
 * 0x00400 - 0x043FF: VRAM
 * 0x04400 - 0x0C3FF: WRAM
 * Total size: 0xC400 (50,176) bytes
*/

DECL_BITFIELD(GBSerializedAudioFlags, uint32_t);
DECL_BITS(GBSerializedAudioFlags, Ch1Volume, 0, 4);
DECL_BITS(GBSerializedAudioFlags, Ch1Dead, 4, 2);
DECL_BIT(GBSerializedAudioFlags, Ch1Hi, 6);
DECL_BITS(GBSerializedAudioFlags, Ch2Volume, 8, 4);
DECL_BITS(GBSerializedAudioFlags, Ch2Dead, 12, 2);
DECL_BIT(GBSerializedAudioFlags, Ch2Hi, 14);
DECL_BITS(GBSerializedAudioFlags, Ch4Volume, 16, 4);
DECL_BITS(GBSerializedAudioFlags, Ch4Dead, 20, 2);
DECL_BITS(GBSerializedAudioFlags, Frame, 22, 3);
DECL_BIT(GBSerializedAudioFlags, Ch1SweepEnabled, 25);
DECL_BIT(GBSerializedAudioFlags, Ch1SweepOccurred, 26);

DECL_BITFIELD(GBSerializedAudioEnvelope, uint32_t);
DECL_BITS(GBSerializedAudioEnvelope, Length, 0, 7);
DECL_BITS(GBSerializedAudioEnvelope, NextStep, 7, 3);
DECL_BITS(GBSerializedAudioEnvelope, Frequency, 10, 11);

struct GBSerializedPSGState {
	struct {
		GBSerializedAudioEnvelope envelope;
		int32_t nextFrame;
		int32_t reserved[2];
		int32_t nextEvent;
	} ch1;
	struct {
		GBSerializedAudioEnvelope envelope;
		int32_t reserved[2];
		int32_t nextEvent;
	} ch2;
	struct {
		uint32_t wavebanks[8];
		int16_t length;
		int16_t reserved;
		int32_t nextEvent;
	} ch3;
	struct {
		int32_t lfsr;
		GBSerializedAudioEnvelope envelope;
		int32_t reserved;
		int32_t nextEvent;
	} ch4;
};

DECL_BITFIELD(GBSerializedCpuFlags, uint32_t);
DECL_BIT(GBSerializedCpuFlags, Condition, 0);
DECL_BIT(GBSerializedCpuFlags, IrqPending, 1);
DECL_BIT(GBSerializedCpuFlags, DoubleSpeed, 2);


DECL_BITFIELD(GBSerializedVideoFlags, uint8_t);
DECL_BIT(GBSerializedVideoFlags, BcpIncrement, 0);
DECL_BIT(GBSerializedVideoFlags, OcpIncrement, 1);

DECL_BITFIELD(GBSerializedMBC7Flags, uint8_t);
DECL_BITS(GBSerializedMBC7Flags, Command, 0, 2);
DECL_BIT(GBSerializedMBC7Flags, Writable, 2);

DECL_BITFIELD(GBSerializedMemoryFlags, uint16_t);
DECL_BIT(GBSerializedMemoryFlags, SramAccess, 0);
DECL_BIT(GBSerializedMemoryFlags, RtcAccess, 1);
DECL_BIT(GBSerializedMemoryFlags, RtcLatched, 2);
DECL_BIT(GBSerializedMemoryFlags, Ime, 3);
DECL_BIT(GBSerializedMemoryFlags, IsHdma, 4);
DECL_BITS(GBSerializedMemoryFlags, ActiveRtcReg, 5, 3);

#pragma pack(push, 1)
struct GBSerializedState {
	uint32_t versionMagic;
	uint32_t romCrc32;
	uint8_t model;
	uint8_t reservedHeader[7];

	char title[16];

	struct {
		uint8_t a;
		uint8_t f;
		uint8_t b;
		uint8_t c;
		uint8_t d;
		uint8_t e;
		uint8_t h;
		uint8_t l;
		uint16_t sp;
		uint16_t pc;

		int32_t cycles;
		int32_t nextEvent;

		uint16_t reservedInstruction;
		uint16_t index;
		uint8_t bus;
		uint8_t executionState;

		uint16_t irqVector;

		int32_t eiPending;
		int32_t reservedDiPending;
		GBSerializedCpuFlags flags;
	} cpu;

	struct {
		struct GBSerializedPSGState psg;
		GBSerializedAudioFlags flags;
		int32_t nextEvent;
		int32_t eventDiff;
		int32_t nextSample;
	} audio;

	struct {
		int16_t x;
		int16_t ly;
		int32_t nextEvent;
		int32_t eventDiff;
		int32_t nextMode;
		int32_t dotCounter;
		int32_t frameCounter;

		uint8_t vramCurrentBank;
		GBSerializedVideoFlags flags;
		uint16_t reserved;

		uint16_t bcpIndex;
		uint16_t ocpIndex;

		uint16_t palette[64];
	} video;

	struct {
		int32_t nextEvent;
		int32_t eventDiff;

		int32_t nextDiv;
		int32_t nextTima;
		int32_t timaPeriod;
	} timer;

	struct {
		uint16_t currentBank;
		uint8_t wramCurrentBank;
		uint8_t sramCurrentBank;

		int32_t dmaNext;
		uint16_t dmaSource;
		uint16_t dmaDest;

		int32_t hdmaNext;
		uint16_t hdmaSource;
		uint16_t hdmaDest;

		uint16_t hdmaRemaining;
		uint8_t dmaRemaining;
		uint8_t rtcRegs[5];

		union {
			struct {
				uint32_t mode;
			} mbc1;
			struct {
				int8_t machineState;
				GBMBC7Field field;
				int8_t address;
				uint8_t srBits;
				uint32_t sr;
				GBSerializedMBC7Flags flags;
			} mbc7;
			struct {
				uint8_t reserved[16];
			} padding;
		};

		GBSerializedMemoryFlags flags;
		uint16_t reserved;
	} memory;

	uint64_t creationUsec;

	uint32_t reserved[48];

	uint8_t oam[GB_SIZE_OAM];

	uint8_t io[GB_SIZE_IO];
	uint8_t hram[GB_SIZE_HRAM];
	uint8_t ie;

	uint8_t vram[GB_SIZE_VRAM];
	uint8_t wram[GB_SIZE_WORKING_RAM];
};
#pragma pack(pop)

bool GBDeserialize(struct GB* gb, const struct GBSerializedState* state);
void GBSerialize(struct GB* gb, struct GBSerializedState* state);

#endif
