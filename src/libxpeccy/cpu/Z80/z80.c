#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "../cpu.h"


extern opCode npTab[256];
extern opCode ddTab[256];
extern opCode fdTab[256];
extern opCode cbTab[256];
extern opCode edTab[256];
extern opCode ddcbTab[256];
extern opCode fdcbTab[256];

void z80_reset(CPU* cpu) {
	cpu->pc = 0;
	cpu->iff1 = 0;
	cpu->iff2 = 0;
	cpu->imode = 0;
	cpu->af = cpu->bc = cpu->de = cpu->hl = 0xffff;
	cpu->af_ = cpu->bc_ = cpu->de_ = cpu->hl_ = 0xffff;
	cpu->ix = cpu->iy = 0xffff;
	cpu->sp = 0xffff;
	cpu->i = cpu->r = cpu->r7 = 0;
	cpu->halt = 0;
	cpu->intrq = 0;
	cpu->wait = 0;
}

int z80_exec(CPU* cpu) {
	if (cpu->wait) return 1;
	cpu->t = 0;
	cpu->noint = 0;
	cpu->opTab = npTab;
	do {
		cpu->com = cpu->mrd(cpu->pc++,1,cpu->data);
		cpu->op = &cpu->opTab[cpu->com];
		cpu->r++;
		cpu->t += cpu->op->t;
		cpu->op->exec(cpu);
	} while (cpu->op->prefix);
	return cpu->t;
}

int z80_int(CPU* cpu) {
	int res = 0;
	if (cpu->wait) return 0;
	if (cpu->intrq & 1) {		// int
		if (cpu->iff1 && !cpu->noint) {
			cpu->iff1 = 0;
			cpu->iff2 = 0;
			if (cpu->halt) {
				cpu->pc++;
				cpu->halt = 0;
			}
			if (cpu->resPV) {
				cpu->f &= ~FP;
				cpu->resPV = 0;
			}
			cpu->opTab = npTab;
			switch(cpu->imode) {
				case 0:
					cpu->t = 2;
					cpu->op = &cpu->opTab[cpu->irq(cpu->data)];
					cpu->r++;
					cpu->t += cpu->op->t;		// +5 (RST38 fetch)
					cpu->op->exec(cpu);		// +3 +3 execution. 13 total
					while (cpu->op->prefix) {
						cpu->op = &cpu->opTab[cpu->mrd(cpu->pc++,1,cpu->data)];
						cpu->r++;
						cpu->t += cpu->op->t;
						cpu->op->exec(cpu);
					}
					break;
				case 1:
					cpu->r++;
					cpu->t = 2 + 5;	// 2 extra + 5 on RST38 fetch
					RST(0x38);	// +3 +3 execution. 13 total
					break;
				case 2:
					cpu->r++;
					cpu->t = 7;
					PUSH(cpu->hpc,cpu->lpc);		// +3 (10) +3 (13)
					cpu->lptr = cpu->irq(cpu->data);	// int vector (FF)
					cpu->hptr = cpu->i;
					cpu->lpc = MEMRD(cpu->mptr++,3);	// +3 (16)
					cpu->hpc = MEMRD(cpu->mptr,3);		// +3 (19)
					cpu->mptr = cpu->pc;
					break;
			}
			res = cpu->t;
		}
	} else if (cpu->intrq & 2) {			// nmi
		if (!cpu->noint) {
			cpu->r++;
			cpu->iff2 = cpu->iff1;
			cpu->iff1 = 0;
			cpu->t = 5;
			PUSH(cpu->hpc,cpu->lpc);
			cpu->pc = 0x0066;
			cpu->mptr = cpu->pc;
			res = cpu->t;		// always 11
		}
	}
	cpu->intrq = 0;
	return res;
}

// disasm

unsigned char z80_cnd[4] = {FZ, FC, FP, FS};

xMnem z80_mnem(CPU* cpu, unsigned short adr, cbdmr mrd, void* data) {
	xMnem mn;
	opCode* opt = npTab;
	opCode* opc;
	unsigned char op;
	unsigned char e = 0;
	unsigned short madr;
	mn.len = 0;
	do {
		op = mrd(adr++,data);
		mn.len++;
		opc = &opt[op];
		if (opc->prefix) {
			opt = opc->tab;
			if ((opt == ddcbTab) || (opt == fdcbTab)) {
				e = mrd(adr, data);
				adr++;
				mn.len++;
			}
		}
	} while (opc->prefix);
	mn.mnem = opc->mnem;
	// mem reading
	mn.mem = 0;
	mn.mop = 0xff;
	if (strstr(opc->mnem, "(hl)") && strcmp(opc->mnem, "jp (hl)")) {
		mn.mem = 1;
		mn.mop = mrd(cpu->hl, data);
	} else if (strstr(opc->mnem, "(de)")) {
		mn.mem = 1;
		mn.mop = mrd(cpu->de, data);
	} else if (strstr(opc->mnem, "(bc)")) {
		mn.mem = 1;
		mn.mop = mrd(cpu->bc, data);
	} else if (strstr(opc->mnem, "(ix")) {
		mn.mem = 1;
		if (opt != ddcbTab) e = mrd(adr, data);
		mn.mop = mrd((cpu->ix + (signed char)e) & 0xffff, data);
	} else if (strstr(opc->mnem, "(iy")) {
		mn.mem = 1;
		if (opt != fdcbTab) e = mrd(adr, data);
		mn.mop = mrd((cpu->iy + (signed char)e) & 0xffff, data);
	} else if (strstr(opc->mnem, "(:2)")) {
		mn.mem = 1;
		madr = mrd(adr, data) & 0xff;
		madr |= (mrd(adr+1, data) << 8);
		mn.mop = mrd(madr, data);
	}
	// conditions
	mn.cond = 0;
	mn.met = 0;
	if (strstr(opc->mnem, "djnz")) {
		mn.cond = 1;
		mn.met = (cpu->b == 1) ? 0 : 1;
	} else if (opt == npTab) {
		if (((op & 0xc7) == 0xc2) || ((op & 0xc7) == 0xc4) || ((op & 0xc7) == 0xc0)) {		// call, jp, ret
			mn.cond = 1;
			mn.met = (cpu->f & z80_cnd[(op & 0x30) >> 4]) ? 0 : 1;
			if (op & 8)
				mn.met ^= 1;
		} else if ((op & 0xe7) == 0x20) {							// jr
			mn.cond = 1;
			mn.met = (cpu->f & z80_cnd[(op & 0x10) >> 4] ? 0 : 1);
			if (op & 8)
				mn.met ^= 1;
		}
	}
	return mn;
}

// asm

xAsmScan z80_asm(const char* cbuf, char* buf) {
	xAsmScan res = scanAsmTab(cbuf, npTab);
	res.ptr = buf;
	if (!res.match) {
		res = scanAsmTab(cbuf, ddTab);
		res.ptr = buf;
		*res.ptr++ = 0xdd;
	}
	if (!res.match) {
		res = scanAsmTab(cbuf, fdTab);
		res.ptr = buf;
		*res.ptr++ = 0xfd;
	}
	if (!res.match) {
		res = scanAsmTab(cbuf, cbTab);
		res.ptr = buf;
		*res.ptr++ = 0xcb;
	}
	if (!res.match) {
		res = scanAsmTab(cbuf, edTab);
		res.ptr = buf;
		*res.ptr++ = 0xed;
	}
	if (!res.match) {
		res = scanAsmTab(cbuf, ddcbTab);
		res.ptr = buf;
		*res.ptr++ = 0xdd;
		*res.ptr++ = 0xcb;
	}
	if (!res.match) {
		res = scanAsmTab(cbuf, fdcbTab);
		res.ptr = buf;
		*res.ptr++ = 0xfd;
		*res.ptr++ = 0xcb;
	}
	if (res.match) {
		*res.ptr++ = res.idx;
	}
	return res;
}