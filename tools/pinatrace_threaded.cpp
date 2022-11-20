/*
 * Copyright (C) 2004-2021 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*
 *  This file contains an ISA-portable PIN tool for tracing memory accesses.
 */

#include <stdio.h>
#include "pin.H"

FILE *trace;
PIN_LOCK pinlock;

// Print a memory read record
VOID RecordMemRead(THREADID threadId, VOID *ip, VOID *addr)
{
	PIN_GetLock(&pinlock, threadId + 1);
	fprintf(trace, "threadId: %d, %p: R %p\n", threadId, ip, addr);
	PIN_ReleaseLock(&pinlock);
}

// Print a memory write record
VOID RecordMemWrite(THREADID threadId, VOID *ip, VOID *addr)
{
	PIN_GetLock(&pinlock, threadId + 1);
	fprintf(trace, "threadId: %d, %p: W %p\n", threadId, ip, addr);
	PIN_ReleaseLock(&pinlock);
}

// Is called for every instruction and instruments reads and writes
VOID Instruction(INS ins, VOID *v)
{
	// Instruments memory accesses using a predicated call, i.e.
	// the instrumentation is called iff the instruction will actually be executed.
	//
	// On the IA-32 and Intel(R) 64 architectures conditional moves and REP
	// prefixed instructions appear as predicated instructions in Pin.
	UINT32 memOperands = INS_MemoryOperandCount(ins);

	// Iterate over each memory operand of the instruction.
	for (UINT32 memOp = 0; memOp < memOperands; memOp++)
	{
		if (INS_MemoryOperandIsRead(ins, memOp))
		{
			INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordMemRead, IARG_THREAD_ID, IARG_INST_PTR, IARG_MEMORYOP_EA, memOp,
									 IARG_END);
		}
		// Note that in some architectures a single memory operand can be
		// both read and written (for instance incl (%eax) on IA-32)
		// In that case we instrument it once for read and once for write.
		if (INS_MemoryOperandIsWritten(ins, memOp))
		{
			INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordMemWrite, IARG_THREAD_ID, IARG_INST_PTR, IARG_MEMORYOP_EA, memOp,
									 IARG_END);
		}
	}
}

VOID Fini(INT32 code, VOID *v)
{
	fprintf(trace, "#eof\n");
	fclose(trace);
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
	PIN_ERROR("This Pintool prints a trace of memory addresses\n" + KNOB_BASE::StringKnobSummary() + "\n");
	return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[])
{
	if (PIN_Init(argc, argv))
		return Usage();
	PIN_InitLock(&pinlock);

	trace = fopen("pinatrace.out", "w");

	INS_AddInstrumentFunction(Instruction, 0);
	PIN_AddFiniFunction(Fini, 0);

	// Never returns
	PIN_StartProgram();

	return 0;
}
