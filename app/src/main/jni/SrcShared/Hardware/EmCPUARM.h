/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
	Copyright (c) 2001 Palm, Inc. or its subsidiaries.
	All rights reserved.

	This file is part of the Palm OS Emulator.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
\* ===================================================================== */

#ifndef EmCPUARM_h
#define EmCPUARM_h

#include "EmCPU.h"				// EmCPU

/*---------------------------------------------------------------------
 *	ARM Register IDs. All register IDs must be unique even if registers
 *	are in a different group such as FPU registers.
 *--------------------------------------------------------------------*/
enum EmCPUARMRegID
{
	eARMRegID_Invalid		= 0,	/* Zero is an invalid register number */

	/*-----------------------------------------------------------
	// User mode registers
	//----------------------------------------------------------*/
	eARMRegID_R0 = 1,
	eARMRegID_R1,
	eARMRegID_R2,
	eARMRegID_R3,
	eARMRegID_R4,
	eARMRegID_R5,
	eARMRegID_R6,
	eARMRegID_R7,
	eARMRegID_R8,
	eARMRegID_R9,
	eARMRegID_R10,
	eARMRegID_R11,
	eARMRegID_R12,
	eARMRegID_SP,		eARMRegID_R13 = eARMRegID_SP,
	eARMRegID_LR,		eARMRegID_R14 = eARMRegID_LR,
	eARMRegID_PC,		eARMRegID_R15 = eARMRegID_PC,
	eARMRegID_CPSR,

	/*-----------------------------------------------------------
	// FIQ mode registers
	//----------------------------------------------------------*/
	eARMRegID_R8_fiq,
	eARMRegID_R9_fiq,
	eARMRegID_R10_fiq,
	eARMRegID_R11_fiq,
	eARMRegID_R12_fiq,
	eARMRegID_R13_fiq,
	eARMRegID_R14_fiq,
	eARMRegID_SPSR_fiq,

	/*-----------------------------------------------------------
	// IRQ mode registers
	//----------------------------------------------------------*/
	eARMRegID_R13_irq,
	eARMRegID_R14_irq,
	eARMRegID_SPSR_irq,

	/*-----------------------------------------------------------
	// Supervisor mode registers
	//----------------------------------------------------------*/
	eARMRegID_R13_svc,
	eARMRegID_R14_svc,
	eARMRegID_SPSR_svc,

	/*-----------------------------------------------------------
	// Abort mode registers
	//----------------------------------------------------------*/
	eARMRegID_R13_abt,
	eARMRegID_R14_abt,
	eARMRegID_SPSR_abt,


	/*-----------------------------------------------------------
	// Undefined mode registers
	//----------------------------------------------------------*/
	eARMRegID_R13_und,
	eARMRegID_R14_und,
	eARMRegID_SPSR_und,

	/*-----------------------------------------------------------
	// SB register (static base). 
	//
	// Any references to R9 can be remapped (in debugger plugin
	// preferences) to access this SB register which can be 
	// described by an expression in the preferences. The 
	// expression should not contain any references to R9 (since
	// this will create an infinite loop (since R9 has been 
	// remapped to SB)). Any need for the actual value of R9 in 
	// the SB expression should use the "genreg_gp1" (which means
	// "General Register Global Pointer 1") opcode which
	// will actually read R9.
	//----------------------------------------------------------*/
	eARMRegID_SB,
	
	/*-----------------------------------------------------------
	// Coprocessor 0 registers
	//----------------------------------------------------------*/
	eARMRegID_CP0_R0,
	eARMRegID_CP0_R1,
	eARMRegID_CP0_R2,
	eARMRegID_CP0_R3,
	eARMRegID_CP0_R4,
	eARMRegID_CP0_R5,
	eARMRegID_CP0_R6,
	eARMRegID_CP0_R7,
	eARMRegID_CP0_R8,
	eARMRegID_CP0_R9,
	eARMRegID_CP0_R10,
	eARMRegID_CP0_R11,
	eARMRegID_CP0_R12,
	eARMRegID_CP0_R13,
	eARMRegID_CP0_R14,
	eARMRegID_CP0_R15,
	eARMRegID_CP0_R16,
	eARMRegID_CP0_R17,
	eARMRegID_CP0_R18,
	eARMRegID_CP0_R19,
	eARMRegID_CP0_R20,
	eARMRegID_CP0_R21,
	eARMRegID_CP0_R22,
	eARMRegID_CP0_R23,
	eARMRegID_CP0_R24,
	eARMRegID_CP0_R25,
	eARMRegID_CP0_R26,
	eARMRegID_CP0_R27,
	eARMRegID_CP0_R28,
	eARMRegID_CP0_R29,
	eARMRegID_CP0_R30,
	eARMRegID_CP0_R31,

	/*-----------------------------------------------------------
	// Coprocessor 1 registers
	//----------------------------------------------------------*/
	eARMRegID_CP1_R0,
	eARMRegID_CP1_R1,
	eARMRegID_CP1_R2,
	eARMRegID_CP1_R3,
	eARMRegID_CP1_R4,
	eARMRegID_CP1_R5,
	eARMRegID_CP1_R6,
	eARMRegID_CP1_R7,
	eARMRegID_CP1_R8,
	eARMRegID_CP1_R9,
	eARMRegID_CP1_R10,
	eARMRegID_CP1_R11,
	eARMRegID_CP1_R12,
	eARMRegID_CP1_R13,
	eARMRegID_CP1_R14,
	eARMRegID_CP1_R15,
	eARMRegID_CP1_R16,
	eARMRegID_CP1_R17,
	eARMRegID_CP1_R18,
	eARMRegID_CP1_R19,
	eARMRegID_CP1_R20,
	eARMRegID_CP1_R21,
	eARMRegID_CP1_R22,
	eARMRegID_CP1_R23,
	eARMRegID_CP1_R24,
	eARMRegID_CP1_R25,
	eARMRegID_CP1_R26,
	eARMRegID_CP1_R27,
	eARMRegID_CP1_R28,
	eARMRegID_CP1_R29,
	eARMRegID_CP1_R30,
	eARMRegID_CP1_R31,

	/*-----------------------------------------------------------
	// Coprocessor 2 registers
	//----------------------------------------------------------*/
	eARMRegID_CP2_R0,
	eARMRegID_CP2_R1,
	eARMRegID_CP2_R2,
	eARMRegID_CP2_R3,
	eARMRegID_CP2_R4,
	eARMRegID_CP2_R5,
	eARMRegID_CP2_R6,
	eARMRegID_CP2_R7,
	eARMRegID_CP2_R8,
	eARMRegID_CP2_R9,
	eARMRegID_CP2_R10,
	eARMRegID_CP2_R11,
	eARMRegID_CP2_R12,
	eARMRegID_CP2_R13,
	eARMRegID_CP2_R14,
	eARMRegID_CP2_R15,
	eARMRegID_CP2_R16,
	eARMRegID_CP2_R17,
	eARMRegID_CP2_R18,
	eARMRegID_CP2_R19,
	eARMRegID_CP2_R20,
	eARMRegID_CP2_R21,
	eARMRegID_CP2_R22,
	eARMRegID_CP2_R23,
	eARMRegID_CP2_R24,
	eARMRegID_CP2_R25,
	eARMRegID_CP2_R26,
	eARMRegID_CP2_R27,
	eARMRegID_CP2_R28,
	eARMRegID_CP2_R29,
	eARMRegID_CP2_R30,
	eARMRegID_CP2_R31,

	/*-----------------------------------------------------------
	// Coprocessor 3 registers
	//----------------------------------------------------------*/
	eARMRegID_CP3_R0,
	eARMRegID_CP3_R1,
	eARMRegID_CP3_R2,
	eARMRegID_CP3_R3,
	eARMRegID_CP3_R4,
	eARMRegID_CP3_R5,
	eARMRegID_CP3_R6,
	eARMRegID_CP3_R7,
	eARMRegID_CP3_R8,
	eARMRegID_CP3_R9,
	eARMRegID_CP3_R10,
	eARMRegID_CP3_R11,
	eARMRegID_CP3_R12,
	eARMRegID_CP3_R13,
	eARMRegID_CP3_R14,
	eARMRegID_CP3_R15,
	eARMRegID_CP3_R16,
	eARMRegID_CP3_R17,
	eARMRegID_CP3_R18,
	eARMRegID_CP3_R19,
	eARMRegID_CP3_R20,
	eARMRegID_CP3_R21,
	eARMRegID_CP3_R22,
	eARMRegID_CP3_R23,
	eARMRegID_CP3_R24,
	eARMRegID_CP3_R25,
	eARMRegID_CP3_R26,
	eARMRegID_CP3_R27,
	eARMRegID_CP3_R28,
	eARMRegID_CP3_R29,
	eARMRegID_CP3_R30,
	eARMRegID_CP3_R31,

	/*-----------------------------------------------------------
	// Coprocessor 4 registers
	//----------------------------------------------------------*/
	eARMRegID_CP4_R0,
	eARMRegID_CP4_R1,
	eARMRegID_CP4_R2,
	eARMRegID_CP4_R3,
	eARMRegID_CP4_R4,
	eARMRegID_CP4_R5,
	eARMRegID_CP4_R6,
	eARMRegID_CP4_R7,
	eARMRegID_CP4_R8,
	eARMRegID_CP4_R9,
	eARMRegID_CP4_R10,
	eARMRegID_CP4_R11,
	eARMRegID_CP4_R12,
	eARMRegID_CP4_R13,
	eARMRegID_CP4_R14,
	eARMRegID_CP4_R15,
	eARMRegID_CP4_R16,
	eARMRegID_CP4_R17,
	eARMRegID_CP4_R18,
	eARMRegID_CP4_R19,
	eARMRegID_CP4_R20,
	eARMRegID_CP4_R21,
	eARMRegID_CP4_R22,
	eARMRegID_CP4_R23,
	eARMRegID_CP4_R24,
	eARMRegID_CP4_R25,
	eARMRegID_CP4_R26,
	eARMRegID_CP4_R27,
	eARMRegID_CP4_R28,
	eARMRegID_CP4_R29,
	eARMRegID_CP4_R30,
	eARMRegID_CP4_R31,

	/*-----------------------------------------------------------
	// Coprocessor 5 registers
	//----------------------------------------------------------*/
	eARMRegID_CP5_R0,
	eARMRegID_CP5_R1,
	eARMRegID_CP5_R2,
	eARMRegID_CP5_R3,
	eARMRegID_CP5_R4,
	eARMRegID_CP5_R5,
	eARMRegID_CP5_R6,
	eARMRegID_CP5_R7,
	eARMRegID_CP5_R8,
	eARMRegID_CP5_R9,
	eARMRegID_CP5_R10,
	eARMRegID_CP5_R11,
	eARMRegID_CP5_R12,
	eARMRegID_CP5_R13,
	eARMRegID_CP5_R14,
	eARMRegID_CP5_R15,
	eARMRegID_CP5_R16,
	eARMRegID_CP5_R17,
	eARMRegID_CP5_R18,
	eARMRegID_CP5_R19,
	eARMRegID_CP5_R20,
	eARMRegID_CP5_R21,
	eARMRegID_CP5_R22,
	eARMRegID_CP5_R23,
	eARMRegID_CP5_R24,
	eARMRegID_CP5_R25,
	eARMRegID_CP5_R26,
	eARMRegID_CP5_R27,
	eARMRegID_CP5_R28,
	eARMRegID_CP5_R29,
	eARMRegID_CP5_R30,
	eARMRegID_CP5_R31,

	/*-----------------------------------------------------------
	// Coprocessor 6 registers
	//----------------------------------------------------------*/
	eARMRegID_CP6_R0,
	eARMRegID_CP6_R1,
	eARMRegID_CP6_R2,
	eARMRegID_CP6_R3,
	eARMRegID_CP6_R4,
	eARMRegID_CP6_R5,
	eARMRegID_CP6_R6,
	eARMRegID_CP6_R7,
	eARMRegID_CP6_R8,
	eARMRegID_CP6_R9,
	eARMRegID_CP6_R10,
	eARMRegID_CP6_R11,
	eARMRegID_CP6_R12,
	eARMRegID_CP6_R13,
	eARMRegID_CP6_R14,
	eARMRegID_CP6_R15,
	eARMRegID_CP6_R16,
	eARMRegID_CP6_R17,
	eARMRegID_CP6_R18,
	eARMRegID_CP6_R19,
	eARMRegID_CP6_R20,
	eARMRegID_CP6_R21,
	eARMRegID_CP6_R22,
	eARMRegID_CP6_R23,
	eARMRegID_CP6_R24,
	eARMRegID_CP6_R25,
	eARMRegID_CP6_R26,
	eARMRegID_CP6_R27,
	eARMRegID_CP6_R28,
	eARMRegID_CP6_R29,
	eARMRegID_CP6_R30,
	eARMRegID_CP6_R31,

	/*-----------------------------------------------------------
	// Coprocessor 7 registers
	//----------------------------------------------------------*/
	eARMRegID_CP7_R0,
	eARMRegID_CP7_R1,
	eARMRegID_CP7_R2,
	eARMRegID_CP7_R3,
	eARMRegID_CP7_R4,
	eARMRegID_CP7_R5,
	eARMRegID_CP7_R6,
	eARMRegID_CP7_R7,
	eARMRegID_CP7_R8,
	eARMRegID_CP7_R9,
	eARMRegID_CP7_R10,
	eARMRegID_CP7_R11,
	eARMRegID_CP7_R12,
	eARMRegID_CP7_R13,
	eARMRegID_CP7_R14,
	eARMRegID_CP7_R15,
	eARMRegID_CP7_R16,
	eARMRegID_CP7_R17,
	eARMRegID_CP7_R18,
	eARMRegID_CP7_R19,
	eARMRegID_CP7_R20,
	eARMRegID_CP7_R21,
	eARMRegID_CP7_R22,
	eARMRegID_CP7_R23,
	eARMRegID_CP7_R24,
	eARMRegID_CP7_R25,
	eARMRegID_CP7_R26,
	eARMRegID_CP7_R27,
	eARMRegID_CP7_R28,
	eARMRegID_CP7_R29,
	eARMRegID_CP7_R30,
	eARMRegID_CP7_R31,

	/*-----------------------------------------------------------
	// Coprocessor 8 registers
	//----------------------------------------------------------*/
	eARMRegID_CP8_R0,
	eARMRegID_CP8_R1,
	eARMRegID_CP8_R2,
	eARMRegID_CP8_R3,
	eARMRegID_CP8_R4,
	eARMRegID_CP8_R5,
	eARMRegID_CP8_R6,
	eARMRegID_CP8_R7,
	eARMRegID_CP8_R8,
	eARMRegID_CP8_R9,
	eARMRegID_CP8_R10,
	eARMRegID_CP8_R11,
	eARMRegID_CP8_R12,
	eARMRegID_CP8_R13,
	eARMRegID_CP8_R14,
	eARMRegID_CP8_R15,
	eARMRegID_CP8_R16,
	eARMRegID_CP8_R17,
	eARMRegID_CP8_R18,
	eARMRegID_CP8_R19,
	eARMRegID_CP8_R20,
	eARMRegID_CP8_R21,
	eARMRegID_CP8_R22,
	eARMRegID_CP8_R23,
	eARMRegID_CP8_R24,
	eARMRegID_CP8_R25,
	eARMRegID_CP8_R26,
	eARMRegID_CP8_R27,
	eARMRegID_CP8_R28,
	eARMRegID_CP8_R29,
	eARMRegID_CP8_R30,
	eARMRegID_CP8_R31,

	/*-----------------------------------------------------------
	// Coprocessor 9 registers
	//----------------------------------------------------------*/
	eARMRegID_CP9_R0,
	eARMRegID_CP9_R1,
	eARMRegID_CP9_R2,
	eARMRegID_CP9_R3,
	eARMRegID_CP9_R4,
	eARMRegID_CP9_R5,
	eARMRegID_CP9_R6,
	eARMRegID_CP9_R7,
	eARMRegID_CP9_R8,
	eARMRegID_CP9_R9,
	eARMRegID_CP9_R10,
	eARMRegID_CP9_R11,
	eARMRegID_CP9_R12,
	eARMRegID_CP9_R13,
	eARMRegID_CP9_R14,
	eARMRegID_CP9_R15,
	eARMRegID_CP9_R16,
	eARMRegID_CP9_R17,
	eARMRegID_CP9_R18,
	eARMRegID_CP9_R19,
	eARMRegID_CP9_R20,
	eARMRegID_CP9_R21,
	eARMRegID_CP9_R22,
	eARMRegID_CP9_R23,
	eARMRegID_CP9_R24,
	eARMRegID_CP9_R25,
	eARMRegID_CP9_R26,
	eARMRegID_CP9_R27,
	eARMRegID_CP9_R28,
	eARMRegID_CP9_R29,
	eARMRegID_CP9_R30,
	eARMRegID_CP9_R31,

	/*-----------------------------------------------------------
	// Coprocessor 10 registers
	//----------------------------------------------------------*/
	eARMRegID_CP10_R0,
	eARMRegID_CP10_R1,
	eARMRegID_CP10_R2,
	eARMRegID_CP10_R3,
	eARMRegID_CP10_R4,
	eARMRegID_CP10_R5,
	eARMRegID_CP10_R6,
	eARMRegID_CP10_R7,
	eARMRegID_CP10_R8,
	eARMRegID_CP10_R9,
	eARMRegID_CP10_R10,
	eARMRegID_CP10_R11,
	eARMRegID_CP10_R12,
	eARMRegID_CP10_R13,
	eARMRegID_CP10_R14,
	eARMRegID_CP10_R15,
	eARMRegID_CP10_R16,
	eARMRegID_CP10_R17,
	eARMRegID_CP10_R18,
	eARMRegID_CP10_R19,
	eARMRegID_CP10_R20,
	eARMRegID_CP10_R21,
	eARMRegID_CP10_R22,
	eARMRegID_CP10_R23,
	eARMRegID_CP10_R24,
	eARMRegID_CP10_R25,
	eARMRegID_CP10_R26,
	eARMRegID_CP10_R27,
	eARMRegID_CP10_R28,
	eARMRegID_CP10_R29,
	eARMRegID_CP10_R30,
	eARMRegID_CP10_R31,

	/*-----------------------------------------------------------
	// Coprocessor 11 registers
	//----------------------------------------------------------*/
	eARMRegID_CP11_R0,
	eARMRegID_CP11_R1,
	eARMRegID_CP11_R2,
	eARMRegID_CP11_R3,
	eARMRegID_CP11_R4,
	eARMRegID_CP11_R5,
	eARMRegID_CP11_R6,
	eARMRegID_CP11_R7,
	eARMRegID_CP11_R8,
	eARMRegID_CP11_R9,
	eARMRegID_CP11_R10,
	eARMRegID_CP11_R11,
	eARMRegID_CP11_R12,
	eARMRegID_CP11_R13,
	eARMRegID_CP11_R14,
	eARMRegID_CP11_R15,
	eARMRegID_CP11_R16,
	eARMRegID_CP11_R17,
	eARMRegID_CP11_R18,
	eARMRegID_CP11_R19,
	eARMRegID_CP11_R20,
	eARMRegID_CP11_R21,
	eARMRegID_CP11_R22,
	eARMRegID_CP11_R23,
	eARMRegID_CP11_R24,
	eARMRegID_CP11_R25,
	eARMRegID_CP11_R26,
	eARMRegID_CP11_R27,
	eARMRegID_CP11_R28,
	eARMRegID_CP11_R29,
	eARMRegID_CP11_R30,
	eARMRegID_CP11_R31,

	/*-----------------------------------------------------------
	// Coprocessor 12 registers
	//----------------------------------------------------------*/
	eARMRegID_CP12_R0,
	eARMRegID_CP12_R1,
	eARMRegID_CP12_R2,
	eARMRegID_CP12_R3,
	eARMRegID_CP12_R4,
	eARMRegID_CP12_R5,
	eARMRegID_CP12_R6,
	eARMRegID_CP12_R7,
	eARMRegID_CP12_R8,
	eARMRegID_CP12_R9,
	eARMRegID_CP12_R10,
	eARMRegID_CP12_R11,
	eARMRegID_CP12_R12,
	eARMRegID_CP12_R13,
	eARMRegID_CP12_R14,
	eARMRegID_CP12_R15,
	eARMRegID_CP12_R16,
	eARMRegID_CP12_R17,
	eARMRegID_CP12_R18,
	eARMRegID_CP12_R19,
	eARMRegID_CP12_R20,
	eARMRegID_CP12_R21,
	eARMRegID_CP12_R22,
	eARMRegID_CP12_R23,
	eARMRegID_CP12_R24,
	eARMRegID_CP12_R25,
	eARMRegID_CP12_R26,
	eARMRegID_CP12_R27,
	eARMRegID_CP12_R28,
	eARMRegID_CP12_R29,
	eARMRegID_CP12_R30,
	eARMRegID_CP12_R31,

	/*-----------------------------------------------------------
	// Coprocessor 13 registers
	//----------------------------------------------------------*/
	eARMRegID_CP13_R0,
	eARMRegID_CP13_R1,
	eARMRegID_CP13_R2,
	eARMRegID_CP13_R3,
	eARMRegID_CP13_R4,
	eARMRegID_CP13_R5,
	eARMRegID_CP13_R6,
	eARMRegID_CP13_R7,
	eARMRegID_CP13_R8,
	eARMRegID_CP13_R9,
	eARMRegID_CP13_R10,
	eARMRegID_CP13_R11,
	eARMRegID_CP13_R12,
	eARMRegID_CP13_R13,
	eARMRegID_CP13_R14,
	eARMRegID_CP13_R15,
	eARMRegID_CP13_R16,
	eARMRegID_CP13_R17,
	eARMRegID_CP13_R18,
	eARMRegID_CP13_R19,
	eARMRegID_CP13_R20,
	eARMRegID_CP13_R21,
	eARMRegID_CP13_R22,
	eARMRegID_CP13_R23,
	eARMRegID_CP13_R24,
	eARMRegID_CP13_R25,
	eARMRegID_CP13_R26,
	eARMRegID_CP13_R27,
	eARMRegID_CP13_R28,
	eARMRegID_CP13_R29,
	eARMRegID_CP13_R30,
	eARMRegID_CP13_R31,

	/*-----------------------------------------------------------
	// Coprocessor 14 registers
	//----------------------------------------------------------*/
	eARMRegID_CP14_R0,
	eARMRegID_CP14_R1,
	eARMRegID_CP14_R2,
	eARMRegID_CP14_R3,
	eARMRegID_CP14_R4,
	eARMRegID_CP14_R5,
	eARMRegID_CP14_R6,
	eARMRegID_CP14_R7,
	eARMRegID_CP14_R8,
	eARMRegID_CP14_R9,
	eARMRegID_CP14_R10,
	eARMRegID_CP14_R11,
	eARMRegID_CP14_R12,
	eARMRegID_CP14_R13,
	eARMRegID_CP14_R14,
	eARMRegID_CP14_R15,
	eARMRegID_CP14_R16,
	eARMRegID_CP14_R17,
	eARMRegID_CP14_R18,
	eARMRegID_CP14_R19,
	eARMRegID_CP14_R20,
	eARMRegID_CP14_R21,
	eARMRegID_CP14_R22,
	eARMRegID_CP14_R23,
	eARMRegID_CP14_R24,
	eARMRegID_CP14_R25,
	eARMRegID_CP14_R26,
	eARMRegID_CP14_R27,
	eARMRegID_CP14_R28,
	eARMRegID_CP14_R29,
	eARMRegID_CP14_R30,
	eARMRegID_CP14_R31,

	/*-----------------------------------------------------------
	// Coprocessor 15 registers
	//----------------------------------------------------------*/
	eARMRegID_CP15_R0,
	eARMRegID_CP15_R1,
	eARMRegID_CP15_R2,
	eARMRegID_CP15_R3,
	eARMRegID_CP15_R4,
	eARMRegID_CP15_R5,
	eARMRegID_CP15_R6,
	eARMRegID_CP15_R7,
	eARMRegID_CP15_R8,
	eARMRegID_CP15_R9,
	eARMRegID_CP15_R10,
	eARMRegID_CP15_R11,
	eARMRegID_CP15_R12,
	eARMRegID_CP15_R13,
	eARMRegID_CP15_R14,
	eARMRegID_CP15_R15,
	eARMRegID_CP15_R16,
	eARMRegID_CP15_R17,
	eARMRegID_CP15_R18,
	eARMRegID_CP15_R19,
	eARMRegID_CP15_R20,
	eARMRegID_CP15_R21,
	eARMRegID_CP15_R22,
	eARMRegID_CP15_R23,
	eARMRegID_CP15_R24,
	eARMRegID_CP15_R25,
	eARMRegID_CP15_R26,
	eARMRegID_CP15_R27,
	eARMRegID_CP15_R28,
	eARMRegID_CP15_R29,
	eARMRegID_CP15_R30,
	eARMRegID_CP15_R31
};


class EmCPUARM;
extern EmCPUARM*	gCPUARM;

class EmCPUARM : public EmCPU
{
	public:
		// -----------------------------------------------------------------------------
		// constructor / destructor
		// -----------------------------------------------------------------------------

								EmCPUARM 			(EmSession*);
		virtual 				~EmCPUARM			(void);

		// -----------------------------------------------------------------------------
		// public methods
		// -----------------------------------------------------------------------------

		// Standard sub-system methods:
		//		Reset:	Resets the state.  Called on hardware resets or on
		//				calls to SysReset.  Also called from constructor.
		//		Save:	Saves the state to the given file.
		//		Load:	Loads the state from the given file.  Can assume that
		//				Reset has been called first.

		virtual void 			Reset				(Bool hardwareReset);
		virtual void 			Save				(SessionFile&);
		virtual void 			Load				(SessionFile&);

		// Execute the main CPU loop until asked to stop.

		virtual void 			Execute 			(void);
		virtual void 			CheckAfterCycle		(void);

		// Low-level access to CPU state.

		virtual emuptr			GetPC				(void);
		virtual emuptr			GetSP				(void);
		virtual uint32			GetRegister			(int);

		virtual void			SetPC				(emuptr);
		virtual void			SetSP				(emuptr);
		virtual void			SetRegister			(int, uint32);

		virtual Bool			Stopped				(void);

	private:
		void					DoReset				(Bool cold);
};

#endif	// EmCPUARM_h
