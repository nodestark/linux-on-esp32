#include <stdio.h>

void cpu_execute(uint32_t inst) {
	/*RV32I RV32M RV32A RV32F RV32C*/

	cpu.xreg[0] = 0;

	if ((inst & 3) == 3) {

		if ((inst & 0b00000000000000000111000001111111) == 0b00000000000000000010000000000011) {
			/*lw*/
			uint32_t imm = /*imm[11:0]*/(int32_t) inst >> 20;

			uint32_t addr= cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] + imm;
			if (cpu_addr_translate(&addr, LOAD_PAGE_FAULT)) {
				cpu.pc += 4;

				bus_read(addr, 4, (uint8_t*) &cpu.xreg[(inst >> 7) & 0b11111 /*rd*/]);
			}
			return;
		}
		if ((inst & 0b00000000000000000111000001111111) == 0b00000000000000000010000000100011) {
			/*sw*/
			uint32_t imm = /*imm[11:5] imm[4:0]*/
					( inst & 0b11111110000000000000000000000000) |
					((inst & 0b00000000000000000000111110000000) << 13);

			uint32_t addr= cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] + ((int32_t) imm >> 20);
			if (cpu_addr_translate(&addr, STORE_AMO_PAGE_FAULT)) {
				cpu.pc += 4;

				bus_write(addr, 4, cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/]);
			}
			return;
		}
		if ((inst & 0b00000000000000000111000001111111) == 0b00000000000000000000000000010011) {
			/*addi*/
			cpu.pc += 4;

			uint32_t imm = /*imm[11:0]*/(int32_t) inst >> 20;
			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] + imm;
			return;
		}
		if ((inst & 0b00000000000000000111000001111111) == 0b00000000000000000001000001100011) {
			/*bne*/

			if (cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] != cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/]) {

				uint32_t imm = /*imm[12|10:5] imm[4:1|11]*/
						((inst & 0b10000000000000000000000000000000) >> 19) |
						((inst & 0b01111110000000000000000000000000) >> 20) |
						((inst & 0b00000000000000000000111100000000) >> 7) |
						((inst & 0b00000000000000000000000010000000) << 4);

				cpu.pc += ((int32_t) (imm << 19) >> 19);

			} else cpu.pc += 4;

			return;
		}
		if ((inst & 0b00000000000000000111000001111111) == 0b00000000000000000100000000000011) {
			/*lbu*/
			uint32_t imm = /*imm[11:0]*/(int32_t) inst >> 20;

			uint32_t addr= cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] + imm;
			if (cpu_addr_translate(&addr, LOAD_PAGE_FAULT)) {
				cpu.pc += 4;

				uint8_t value;
				bus_read(addr, 1, &value);

				cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = value;
			}
			return;
		}
		if ((inst & 0b00000000000000000000000001111111) == 0b00000000000000000000000000110111) {
			/*lui*/
			cpu.pc += 4;

			uint32_t imm = /*imm[31:12]*/inst & 0xfffff000;
			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = imm;
			return;
		}
		if ((inst & 0b00000000000000000000000001111111) == 0b00000000000000000000000001101111) {
			/*jal*/

			uint32_t imm = /*imm[20|10:1|11|19:12]*/
					((inst & 0b10000000000000000000000000000000) >> 11) |
					((inst & 0b01111111111000000000000000000000) >> 20) |
					((inst & 0b00000000000100000000000000000000) >> 9) |
					(inst & 0b00000000000011111111000000000000);

			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = cpu.pc + 4;
			cpu.pc += ((int32_t) (imm << 11) >> 11);
			return;
		}
		if ((inst & 0b00000000000000000111000001111111) == 0b00000000000000000000000001100011) {
			/*beq*/
			if (cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] == cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/]) {

				uint32_t imm = /*imm[12|10:5] imm[4:1|11]*/
						((inst & 0b10000000000000000000000000000000) >> 19) |
						((inst & 0b01111110000000000000000000000000) >> 20) |
						((inst & 0b00000000000000000000111100000000) >> 7) |
						((inst & 0b00000000000000000000000010000000) << 4);

				cpu.pc += ((int32_t) (imm << 19) >> 19);
			} else cpu.pc += 4;

			return;
		}
		if ((inst & 0b00000000000000000111000001111111) == 0b00000000000000000111000000010011) {
			/*andi*/
			cpu.pc += 4;

			uint32_t imm = /*imm[11:0]*/(int32_t) inst >> 20;
			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] & imm;
			return;
		}
		if ((inst & 0b00000000000000000111000001111111) == 0b00000000000000000110000001100011) {
			/*bltu*/
			if (cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] < cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/]) {

				uint32_t imm = /*imm[12|10:5] imm[4:1|11]*/
						((inst & 0b10000000000000000000000000000000) >> 19) |
						((inst & 0b01111110000000000000000000000000) >> 20) |
						((inst & 0b00000000000000000000111100000000) >> 7) |
						((inst & 0b00000000000000000000000010000000) << 4);

				cpu.pc += ((int32_t) (imm << 19) >> 19);

			} else cpu.pc += 4;

			return;
		}
		if ((inst & 0b11111110000000000111000001111111) == 0b01000000000000000000000000110011) {
			/*sub*/
			cpu.pc += 4;

			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] - cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/];
			return;
		}
		if ((inst & 0b11111110000000000111000001111111) == 0b00000000000000000000000000110011) {
			/*add*/
			cpu.pc += 4;

			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] + cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/];
			return;
		}
		if ((inst & 0b11111000000000000111000001111111) == 0b00000000000000000001000000010011) {
			/*slli*/
			cpu.pc += 4;

			uint32_t imm = /*imm[4:0]*/(inst & 0x1f00000) >> 20;
			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] << imm;
			return;
		}
		if ((inst & 0b00000000000000000111000001111111) == 0b00000000000000000111000001100011) {
			/*bgeu*/
			if (cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] >= cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/]) {

				uint32_t imm = /*imm[12|10:5] imm[4:1|11]*/
						((inst & 0b10000000000000000000000000000000) >> 19) |
						((inst & 0b01111110000000000000000000000000) >> 20) |
						((inst & 0b00000000000000000000111100000000) >> 7) |
						((inst & 0b00000000000000000000000010000000) << 4);

				cpu.pc += ((int32_t) (imm << 19) >> 19);

			} else cpu.pc += 4;

			return;
		}

		if ((inst & 0b11111110000000000111000001111111) == 0b00000000000000000011000000110011) {
			/*sltu*/
			cpu.pc += 4;

			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] < cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/];
			return;
		}
		if ((inst & 0b11111000000000000111000001111111) == 0b00000000000000000101000000010011) {
			/*srli*/
			cpu.pc += 4;

			uint32_t imm = /*imm[4:0]*/(inst & 0x1f00000) >> 20;
			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] >> imm;
			return;
		}
		if ((inst & 0b00000000000000000111000001111111) == 0b00000000000000000000000000100011) {
			/*sb*/

			uint32_t imm = /*imm[11:5] imm[4:0]*/
					( inst & 0b11111110000000000000000000000000) |
					((inst & 0b00000000000000000000111110000000) << 13);

			uint32_t addr= cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] + ((int32_t) imm >> 20);
			if (cpu_addr_translate(&addr, STORE_AMO_PAGE_FAULT)) {
				cpu.pc += 4;

				bus_write(addr, 1, cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/]);
			}
			return;
		}
		if ((inst & 0b00000000000000000000000001111111) == 0b00000000000000000000000000010111) {
			/*auipc*/

			uint32_t imm = /*imm[31:12]*/inst & 0xfffff000;
			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = cpu.pc + imm;

			cpu.pc += 4;

			return;
		}
		if ((inst & 0b00000000000000000111000001111111) == 0b00000000000000000100000001100011) {
			/*blt*/
			if ((int32_t) cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] < (int32_t) cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/]) {

				uint32_t imm = /*imm[12|10:5] imm[4:1|11]*/
						((inst & 0b10000000000000000000000000000000) >> 19) |
						((inst & 0b01111110000000000000000000000000) >> 20) |
						((inst & 0b00000000000000000000111100000000) >> 7) |
						((inst & 0b00000000000000000000000010000000) << 4);

				cpu.pc += ((int32_t) (imm << 19) >> 19);

			} else cpu.pc += 4;

			return;
		}
		if ((inst & 0b00000000000000000111000001111111) == 0b00000000000000000000000001100111) {
			/*jalr*/
			uint32_t imm = /*imm[11:0]*/(int32_t) inst >> 20;

			uint32_t tmp = cpu.pc + 4;
			cpu.pc = (cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] + imm) & ~1;
			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = tmp;

			return;
		}
		if ((inst & 0b00000000000000000111000001111111) == 0b00000000000000000100000000010011) {
			/*xori*/
			cpu.pc += 4;

			uint32_t imm = /*imm[11:0]*/(int32_t) inst >> 20;
			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] ^ imm;
			return;
		}
		if ((inst & 0b00000000000000000111000001111111) == 0b00000000000000000101000001100011) {
			/*bge*/

			if ((int32_t) cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] >= (int32_t) cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/]) {

				uint32_t imm = /*imm[12|10:5] imm[4:1|11]*/
						((inst & 0b10000000000000000000000000000000) >> 19) |
						((inst & 0b01111110000000000000000000000000) >> 20) |
						((inst & 0b00000000000000000000111100000000) >> 7) |
						((inst & 0b00000000000000000000000010000000) << 4);

				cpu.pc += ((int32_t) (imm << 19) >> 19);

			} else cpu.pc += 4;

			return;
		}
		if ((inst & 0b11111110000000000111000001111111) == 0b00000000000000000110000000110011) {
			/*or*/
			cpu.pc += 4;

			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] | cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/];
			return;
		}
		if ((inst & 0b11111110000000000111000001111111) == 0b00000010000000000000000000110011) {
			/*mul*/
			cpu.pc += 4;

			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = (int32_t) cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] * (int32_t) cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/];
			return;
		}
		if ((inst & 0b11111110000000000111000001111111) == 0b00000000000000000100000000110011) {
			/*xor*/
			cpu.pc += 4;

			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] ^ cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/];
			return;
		}
		if ((inst & 0b00000000000000000111000001111111) == 0b00000000000000000111000001110011) {
			/*csrrci*/
			cpu.pc += 4;

			uint32_t imm = /*imm[4:0]*/((inst & 0b00000000000011111000000000000000) >> 15);

			uint32_t addr = /*csr[11:0]*/inst >> 20;

			uint32_t *csr;
			csr_read_(addr, &csr);

			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = *csr;
			*csr &= ~imm;
			return;
		}
		if ((inst & 0b00000000000000000111000001111111) == 0b00000000000000000101000000000011) {
			/*lhu*/
			uint32_t imm = /*imm[11:0]*/(int32_t) inst >> 20;

			uint32_t addr= cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] + imm;
			if (cpu_addr_translate(&addr, LOAD_PAGE_FAULT)) {
				cpu.pc += 4;

				uint16_t value;
				bus_read(addr, 2, (uint8_t*) &value);

				cpu.xreg[(inst >> 7) & 0b11111 /*rd*/]= value;
			}
			return;
		}
		if ((inst & 0b00000000000000000111000001111111) == 0b00000000000000000010000001110011) {
			/*csrrs*/
			cpu.pc += 4;

			uint32_t addr = /*csr[11:0]*/inst >> 20;

			uint32_t *csr;
			csr_read_(addr, &csr);

			uint32_t tmp= *csr;

			*csr |= cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/];
			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = tmp;
			return;
		}
		if ((inst & 0b11111110000000000111000001111111) == 0b00000000000000000101000000110011) {
			/*srl*/
			cpu.pc += 4;

			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] >> (cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/] & 0x1f);
			return;
		}
		if ((inst & 0b11111110000000000111000001111111) == 0b00000000000000000111000000110011) {
			/*and*/
			cpu.pc += 4;

			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] & cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/];
			return;
		}
		if ((inst & 0b11111110000000000111000001111111) == 0b00000000000000000001000000110011) {
			/*sll*/
			cpu.pc += 4;

			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] << (cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/] & 0x1f);
			return;
		}
		if ((inst & 0b11110000000011111111111111111111) == 0b00000000000000000000000000001111) {
			/*fence*/
			cpu.pc += 4;

			// uint8_t pred= (inst & 0b00001111000000000000000000000000) >> 24;
			// uint8_t succ= (inst & 0b00000000111100000000000000000000) >> 20;
			return;
		}
		if ((inst & 0b00000000000000000111000001111111) == 0b00000000000000000110000001110011) {
			/*csrrsi*/
			cpu.pc += 4;

			uint32_t imm = /*imm[4:0]*/((inst & 0b00000000000011111000000000000000) >> 15);

			uint32_t addr = /*csr[11:0]*/inst >> 20;

			uint32_t *csr;
			csr_read_(addr, &csr);

			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = *csr;
			*csr |= imm;
			return;
		}
		if ((inst & 0b11111000000000000111000001111111) == 0b00000000000000000010000000101111) {
			/*amoadd.w*/
			uint32_t addr= cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/];
			if (cpu_addr_translate(&addr, STORE_AMO_PAGE_FAULT)) {
				cpu.pc += 4;

				uint32_t value;
				bus_read(addr, 4, (uint8_t*) &value);

				bus_write(addr, 4, value + cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/]);
				cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = value;
			}
			return;
		}
		if ((inst & 0b00000000000000000111000001111111) == 0b00000000000000000000000000000011) {
			/*lb*/
			uint32_t imm = /*imm[11:0]*/(int32_t) inst >> 20;

			uint32_t addr= cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] + imm;
			if (cpu_addr_translate(&addr, LOAD_PAGE_FAULT)) {
				cpu.pc += 4;

				int8_t value;
				bus_read(addr, 1, (uint8_t*) &value);

				cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = value;
			}
			return;
		}
		if ((inst & 0b00000000000000000111000001111111) == 0b00000000000000000011000000010011) {
			/*sltiu*/
			cpu.pc += 4;

			uint32_t imm = /*imm[11:0]*/(int32_t) inst >> 20;
			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] < imm;
			return;
		}
		if ((inst & 0b00000000000000000111000001111111) == 0b00000000000000000001000000100011) {
			/*sh*/
			uint32_t imm = /*imm[11:5] imm[4:0]*/
					( inst & 0b11111110000000000000000000000000) |
					((inst & 0b00000000000000000000111110000000) << 13);

			uint32_t addr= cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] + ((int32_t) imm >> 20);
			if (cpu_addr_translate(&addr, STORE_AMO_PAGE_FAULT)) {
				cpu.pc += 4;

				bus_write(addr, 2, cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/]);
			}
			return;
		}
		if ((inst & 0b00000000000000000111000001111111) == 0b00000000000000000110000000010011) {
			/*ori*/
			cpu.pc += 4;

			uint32_t imm = /*imm[11:0]*/(int32_t) inst >> 20;
			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] | imm;
			return;
		}
		if ((inst & 0b11111110000000000111000001111111) == 0b00000010000000000011000000110011) {
			/*mulhu*/
			cpu.pc += 4;

			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = ((uint64_t) cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] * (uint64_t) cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/]) >> 32;
			return;
		}
		if ((inst & 0b11111000000000000111000001111111) == 0b01000000000000000101000000010011) {
			/*srai*/
			cpu.pc += 4;

			uint32_t imm = /*imm[4:0]*/(inst & 0x01f00000) >> 20;
			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = (int32_t) cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] >> imm;
			return;
		}
		if ((inst & 0b11111000000000000111000001111111) == 0b00011000000000000010000000101111) {
			/*sc.w*/
			if (cpu.reserved == cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/]) {

				uint32_t addr= cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/];
				if (cpu_addr_translate(&addr, STORE_AMO_PAGE_FAULT)) {
					cpu.pc += 4;

					bus_write(addr, 4, cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/]);
					cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = 0;
				}

			} else {

				cpu.pc += 4;

				cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = 1;
			}
			return;
		}
		if ((inst & 0b11111001111100000111000001111111) == 0b00010000000000000010000000101111) {
			/*lr.w*/
			uint32_t addr= cpu.reserved = cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/];
			if (cpu_addr_translate(&addr, LOAD_PAGE_FAULT)) {
				cpu.pc += 4;

				bus_read(addr, 4, (uint8_t*) &cpu.xreg[(inst >> 7) & 0b11111 /*rd*/]);
			}
			return;
		}
		if ((inst & 0b11111000000000000111000001111111) == 0b01000000000000000010000000101111) {
			/*amoor.w*/
			uint32_t addr= cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/];
			if (cpu_addr_translate(&addr, STORE_AMO_PAGE_FAULT)) {
				cpu.pc += 4;

				uint32_t value;
				bus_read(addr, 4, (uint8_t*) &value);

				bus_write(addr, 4, value | cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/]);
				cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = value;
			}
			return;
		}
		if ((inst & 0b11111110000000000111000001111111) == 0b01000000000000000101000000110011) {
			/*sra*/
			cpu.pc += 4;

			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = (int32_t) cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] >> (cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/] & 0x1f);
			return;
		}
		if ((inst & 0b11111000000000000111000001111111) == 0b01100000000000000010000000101111) {
			/*amoand.w*/
			uint32_t addr= cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/];
			if (cpu_addr_translate(&addr, STORE_AMO_PAGE_FAULT)) {
				cpu.pc += 4;

				uint32_t value;
				bus_read(addr, 4, (uint8_t*) &value);

				bus_write(addr, 4, value & cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/]);
				cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = value;
			}
			return;
		}
		if ((inst & 0b00000000000000000111000001111111) == 0b00000000000000000001000000000011) {
			/*lh*/
			uint32_t imm = /*imm[11:0]*/(int32_t) inst >> 20;

			uint32_t addr= cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] + imm;
			if (cpu_addr_translate(&addr, LOAD_PAGE_FAULT)) {
				cpu.pc += 4;

				int16_t value;
				bus_read(addr, 2, (uint8_t*) &value);

				cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = value;
			}
			return;
		}
		if ((inst & 0b00000000000000000111000001111111) == 0b00000000000000000001000001110011) {
			/*csrrw*/
			cpu.pc += 4;

			uint32_t addr = /*csr[11:0]*/inst >> 20;

			uint32_t *csr;
			csr_read_(addr, &csr);

			uint32_t tmp= *csr;

			*csr = cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/];
			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = tmp;
			return;
		}
		if ((inst & 0b11111110000000000111000001111111) == 0b00000010000000000111000000110011) {
			/*remu*/
			cpu.pc += 4;

			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = (cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/] == 0) ? -1 : (cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] % cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/]);
			return;
		}
		if ((inst & 0b00000000000000000111000001111111) == 0b00000000000000000011000001110011) {
			/*csrrc*/
			cpu.pc += 4;

			uint32_t addr = /*csr[11:0]*/inst >> 20;

			uint32_t *csr;
			csr_read_(addr, &csr);

			uint32_t tmp= *csr;

			*csr &= ~cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/];
			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = tmp;
			return;
		}
		if ((inst & 0b11111110000000000111000001111111) == 0b00000010000000000101000000110011) {
			/*divu*/
			cpu.pc += 4;

			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = (cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/] == 0) ? -1 : (cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] / cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/]);
			return;
		}
		if ((inst & 0b11111000000000000111000001111111) == 0b00001000000000000010000000101111) {
			/*amoswap.w*/
			uint32_t addr= cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/];
			if (cpu_addr_translate(&addr, STORE_AMO_PAGE_FAULT)) {
				cpu.pc += 4;

				uint32_t value;
				bus_read(addr, 4, (uint8_t*) &value);

				bus_write(addr, 4, cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/]);
				cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = value;
			}
			return;
		}
		if ((inst & 0b11111110000000000111000001111111) == 0b00010010000000000000000001110011) {
			/*sfence.vma*/
			cpu.pc += 4;

			memset(&tlb_, -1, TLB_SIZE * sizeof(struct tlb_t));

			return;
		}
		if ((inst & 0b11111110000000000111000001111111) == 0b00000010000000000100000000110011) {
			/*div*/
			cpu.pc += 4;

			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = (cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/] == 0) ? -1 : ((int32_t) cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] / (int32_t) cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/]);
			return;
		}
		if (inst == 0b00000000000000000001000000001111) {
			/*fence.i*/
			cpu.pc += 4;
			return;
		}
		if (inst == 0b00010000001000000000000001110011) {
			/*sret*/

			memset(&tlb_, -1, TLB_SIZE * sizeof(struct tlb_t));

			// Return from traps in S-mode, and SRET copies SPIE into SIE, then sets SPIE.
			cpu.privilege = (csr.sstatus >> 8 /*SPP*/) & 1;
			csr.sstatus &= ~(1 << 8 /*SPP*/);

			csr.sstatus &= ~(1 << cpu.privilege /*SIE*/);	// clear SIE
			csr.sstatus |= ((csr.sstatus >> 5 /*SPIE*/) & 1) << cpu.privilege; // copies SPIE into SIE
			csr.sstatus |= (1 << 5 /*SPIE*/);               // set SPIE

			cpu.pc = csr.sepc;
			return;
		}
		if (inst == 0b00000000000000000000000001110011) {
			/*ecall*/

			// Environment call from {U,S,M}-mode
			cpu_take_trap(cpu.privilege + 8, 0);

			return;
		}
		if (inst == 0b00110000001000000000000001110011) {
			/*mret*/

			// Return from traps in M-mode, and MRET copies MPIE into MIE, then sets MPIE.
			cpu.privilege = (csr.mstatus >> 11 /*MPP*/) & 3;
			csr.mstatus &= ~(3 << 11 /*MPP*/);

			csr.mstatus &= ~(1 << cpu.privilege /*MIE*/); 	// clear MIE
			csr.mstatus |= ((csr.mstatus >> 7 /*MPIE*/) & 1) << cpu.privilege; // copies MPIE into MIE
			csr.mstatus |= (1 << 7 /*MPIE*/);               // sets MPIE

			cpu.pc = csr.mepc;
			return;
		}
		if ((inst & 0b11111110000000000111000001111111) == 0b00000000000000000010000000110011) {
			/*slt*/
			cpu.pc += 4;

			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = (int32_t) cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] < (int32_t) cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/];
			return;
		}
		if ((inst & 0b11111110000000000111000001111111) == 0b00000010000000000110000000110011) {
			/*rem*/
			cpu.pc += 4;

			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = (cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/] == 0) ? -1 : ((int32_t) cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] % (int32_t) cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/]);
			return;
		}
		if ((inst & 0b11111110000000000111000001111111) == 0b00000010000000000001000000110011) {
			/*mulh*/
			cpu.pc += 4;

			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = ((int64_t) cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] * (int64_t) cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/]) >> 32;
			return;
		}
		if ((inst & 0b00000000000000000111000001111111) == 0b00000000000000000010000000010011) {
			/*slti*/
			cpu.pc += 4;

			uint32_t imm = /*imm[11:0]*/(int32_t) inst >> 20;
			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = (int32_t) cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] < (int32_t) imm;
			return;
		}
		if ((inst & 0b00000000000000000111000001111111) == 0b00000000000000000101000001110011) {
			/*csrrwi*/
			cpu.pc += 4;

			uint32_t imm = /*imm[4:0]*/((inst & 0b00000000000011111000000000000000) >> 15);

			uint32_t addr = /*csr[11:0]*/inst >> 20;

			uint32_t *csr;
			csr_read_(addr, &csr);

			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = *csr;
			*csr = imm;
			return;
		}
		if ((inst & 0b11111000000000000111000001111111) == 0b11100000000000000010000000101111) {
			/*amomaxu.w*/
			uint32_t addr= cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/];
			if (cpu_addr_translate(&addr, STORE_AMO_PAGE_FAULT)) {
				cpu.pc += 4;

				uint32_t value;
				bus_read(addr, 4, (uint8_t*) &value);

				bus_write(addr, 4, MAX(value, cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/]));
				cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = value;
			}
			return;
		}
		if ((inst & 0b11111110000000000111000001111111) == 0b00000010000000000010000000110011) {
			/*mulhsu*/
			cpu.pc += 4;

			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/] = ((int64_t) cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] * (uint64_t) cpu.xreg[(inst >> 20) & 0b11111 /*rs2*/]) >> 32;
			return;
		}
		if ((inst & 0b00000000000000000111000001111111) == 0b00000000000000000011000000100111) {
			/*fsd*/
			uint32_t imm = /*imm[11:5] imm[4:0]*/
					( inst & 0b11111110000000000000000000000000) |
					((inst & 0b00000000000000000000111110000000) << 13);

			uint32_t addr= cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] + ((int32_t) imm >> 20);
			if (cpu_addr_translate(&addr, STORE_AMO_PAGE_FAULT)) {
				cpu.pc += 4;

				bus_write(addr, 8, cpu.freg[(inst >> 20) & 0b11111 /*rs2*/] );
			}
			return;
		}
		if ((inst & 0b00000000000000000111000001111111) == 0b00000000000000000011000000000111) {
			/*fld*/
			uint32_t imm = /*imm[11:0]*/(int32_t) inst >> 20;

			uint32_t addr= cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] + imm;
			if (cpu_addr_translate(&addr, LOAD_PAGE_FAULT)) {
				cpu.pc += 4;

				bus_read(addr, 8, (uint8_t*) &cpu.freg[(inst >> 7) & 0b11111 /*rd*/]);
			}
			return;
		}
		if ((inst & 0b11111000000000000111000001111111) == 0b10100000000000000010000001010011) {
			/*feq.s*/
			/*feq.d*/
			cpu.pc += 4;

			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/]= (cpu.freg[(inst >> 15) & 0b11111 /*rs1*/] == cpu.freg[(inst >> 20) & 0b11111 /*rs2*/]);
			return;
		}
		if ((inst & 0b11111110000000000111000001111111) == 0b10100010000000000001000001010011) {
			/*flt.d*/
			cpu.pc += 4;

			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/]= unpack754_64(cpu.freg[(inst >> 15) & 0b11111 /*rs1*/]) < unpack754_64(cpu.freg[(inst >> 20) & 0b11111 /*rs2*/]);
			return;
		}
		if ((inst & 0b11111110000000000111000001111111) == 0b10100010000000000000000001010011) {
			/*fle.d*/
			cpu.pc += 4;

			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/]= unpack754_64(cpu.freg[(inst >> 15) & 0b11111 /*rs1*/]) <= unpack754_64(cpu.freg[(inst >> 20) & 0b11111 /*rs2*/]);
			return;
		}
		if ((inst & 0b11111110000000000000000001111111) == 0b00000010000000000000000001010011) {
			/*fadd.d*/
			cpu.pc += 4;

			cpu.freg[(inst >> 7) & 0b11111 /*rd*/]= pack754_64(unpack754_64(cpu.freg[(inst >> 15) & 0b11111 /*rs1*/]) + unpack754_64(cpu.freg[(inst >> 20) & 0b11111 /*rs2*/]));
			return;
		}
		if ((inst & 0b11111110000000000000000001111111) == 0b00001010000000000000000001010011) {
			/*fsub.d*/
			cpu.pc += 4;

			cpu.freg[(inst >> 7) & 0b11111 /*rd*/]= pack754_64(unpack754_64(cpu.freg[(inst >> 15) & 0b11111 /*rs1*/]) - unpack754_64(cpu.freg[(inst >> 20) & 0b11111 /*rs2*/]));
			return;
		}
		if ((inst & 0b11111110000000000000000001111111) == 0b00010010000000000000000001010011) {
			/*fmul.d*/
			cpu.pc += 4;

			cpu.freg[(inst >> 7) & 0b11111 /*rd*/]= pack754_64(unpack754_64(cpu.freg[(inst >> 15) & 0b11111 /*rs1*/]) * unpack754_64(cpu.freg[(inst >> 20) & 0b11111 /*rs2*/]));
			return;
		}
		if ((inst & 0b11111110000000000000000001111111) == 0b00011010000000000000000001010011) {
			/*fdiv.d*/
			cpu.pc += 4;

			cpu.freg[(inst >> 7) & 0b11111 /*rd*/]= pack754_64( unpack754_64(cpu.freg[(inst >> 15) & 0b11111 /*rs1*/]) / unpack754_64(cpu.freg[(inst >> 20) & 0b11111 /*rs2*/]) );
			return;
		}
		if ((inst & 0b11111111111100000000000001111111) == 0b11010010000000000000000001010011) {
			/*fcvt.d.w*/
			cpu.pc += 4;

			cpu.freg[(inst >> 7) & 0b11111 /*rd*/]= pack754_64( (int32_t) cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] );
			return;
		}
		if ((inst & 0b11111111111100000000000001111111) == 0b11000010000000000000000001010011) {
			/*fcvt.w.d*/
			cpu.pc += 4;

			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/]= (int32_t) unpack754_64(cpu.freg[(inst >> 15) & 0b11111 /*rs1*/]);
			return;
		}
		if ((inst & 0b11111111111100000000000001111111) == 0b11010010000100000000000001010011) {
			/*fcvt.d.wu*/
			cpu.pc += 4;

			cpu.freg[(inst >> 7) & 0b11111 /*rd*/]= pack754_64( (uint32_t) cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/] );
			return;
		}
		if ((inst & 0b11111111111100000000000001111111) == 0b11000010000100000000000001010011) {
			/*fcvt.wu.d*/
			cpu.pc += 4;

			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/]= (uint32_t) unpack754_64(cpu.freg[(inst >> 15) & 0b11111 /*rs1*/]);
			return;
		}
		if ((inst & 0b11111111111100000111000001111111) == 0b11110000000000000000000001010011) {
			/*fmv.w.x*/
			cpu.pc += 4;

			cpu.freg[(inst >> 7) & 0b11111 /*rd*/]= cpu.xreg[(inst >> 15) & 0b11111 /*rs1*/];
			return;
		}
		if ((inst & 0b11111111111100000111000001111111) == 0b11100000000000000000000001010011) {
			/*fmv.x.w*/
			cpu.pc += 4;

			cpu.xreg[(inst >> 7) & 0b11111 /*rd*/]= cpu.freg[(inst >> 15) & 0b11111 /*rs1*/];
			return;
		}
		if ((inst & 0b00000110000000000000000001111111) == 0b00000010000000000000000001001011) {
			/*fnmsub.d*/
			cpu.pc += 4;

			uint32_t rs3 = (inst >> 27) & 0b11111;

			cpu.freg[(inst >> 7) & 0b11111 /*rd*/]= pack754_64( -unpack754_64(cpu.freg[(inst >> 15) & 0b11111 /*rs1*/]) * unpack754_64(cpu.freg[(inst >> 20) & 0b11111 /*rs2*/]) + unpack754_64(cpu.freg[rs3]) );
			return;
		}
		if ((inst & 0b00000110000000000000000001111111) == 0b00000010000000000000000001000011) {
			/*fmadd.d*/
			cpu.pc += 4;

			uint32_t rs3 = (inst >> 27) & 0b11111;

			cpu.freg[(inst >> 7) & 0b11111 /*rd*/]= pack754_64( unpack754_64(cpu.freg[(inst >> 15) & 0b11111 /*rs1*/]) * unpack754_64(cpu.freg[(inst >> 20) & 0b11111 /*rs2*/]) + unpack754_64(cpu.freg[rs3]) );
			return;
		}
		if ((inst & 0b11111110000000000111000001111111) == 0b00100010000000000000000001010011) {
			/*fsgnj.d*/
			cpu.pc += 4;

			cpu.freg[(inst >> 7) & 0b11111 /*rd*/]= (cpu.freg[(inst >> 15) & 0b11111 /*rs1*/] & ~(1LL << 63)) | (cpu.freg[(inst >> 20) & 0b11111 /*rs2*/] & (1LL << 63));
			return;
		}
		if ((inst & 0b11111110000000000111000001111111) == 0b00100010000000000001000001010011) {
			/*fsgnjn.d*/
			cpu.pc += 4;

			cpu.freg[(inst >> 7) & 0b11111 /*rd*/]= (cpu.freg[(inst >> 15) & 0b11111 /*rs1*/] & ~(1LL << 63)) | (~cpu.freg[(inst >> 20) & 0b11111 /*rs2*/] & (1LL << 63));
			return;
		}
		if ((inst & 0b11111110000000000111000001111111) == 0b00100010000000000010000001010011) {
			/*fsgnjx.d*/
			cpu.pc += 4;

			cpu.freg[(inst >> 7) & 0b11111 /*rd*/]= (cpu.freg[(inst >> 15) & 0b11111 /*rs1*/] & ~(1LL << 63)) | ((cpu.freg[(inst >> 20) & 0b11111 /*rs2*/] ^ cpu.freg[(inst >> 15) & 0b11111 /*rs1*/]) & (1LL << 63));
			return;
		}
		if ((inst & 0b11111111111100000000000001111111) == 0b01000000000100000000000001010011) {
			/*fcvt.s.d*/
			cpu.pc += 4;

			cpu.freg[(inst >> 7) & 0b11111 /*rd*/]= pack754_32( (float) unpack754_64(cpu.freg[(inst >> 15) & 0b11111 /*rs1*/]) );
			return;
		}
		if ((inst & 0b11111111111100000000000001111111) == 0b01000010000000000000000001010011) {
			/*fcvt.d.s*/
			cpu.pc += 4;

			cpu.freg[(inst >> 7) & 0b11111 /*rd*/]= pack754_64( (double) unpack754_32(cpu.freg[(inst >> 15) & 0b11111 /*rs1*/]) );
			return;
		}
		/*if (inst == 0b00000000000100000000000001110011) {
			// ebreak

			cpu_take_trap(BREAKPOINT, cpu.pc + 4);
			return;
		}*/
		if (inst == 0b00010000010100000000000001110011) {
			/*wfi*/
			cpu.pc += 4;

			return;
		}

	} else {

		if ((inst & 0b1110000000000011) == 0b1100000000000010) {
			/*c.swsp*/
			uint16_t imm = // imm[5:2|7:6]
					((inst & 0b0001111000000000) >> 7) |
					((inst & 0b0000000110000000) >> 1);

			uint32_t addr= cpu.xreg[2] + imm;
			if (cpu_addr_translate(&addr, STORE_AMO_PAGE_FAULT)) {
                cpu.pc += 2;

				bus_write(addr, 4, cpu.xreg[(inst >> 2) & 0b11111]);
			}
			return;
		}
		if ((inst & 0b1110000000000011) == 0b0100000000000010) {
			/*c.lwsp*/
			uint16_t imm = // imm[5] imm[4:2|7:6]
					((inst & 0b0001000000000000) >> 7) |
					((inst & 0b0000000001110000) >> 2) |
					((inst & 0b0000000000001100) << 4);

			uint32_t addr= cpu.xreg[2] + imm;
			if (cpu_addr_translate(&addr, LOAD_PAGE_FAULT)) {
				cpu.pc += 2;

				bus_read(addr, 4, (uint8_t*) &cpu.xreg[(inst >> 7) & 0b11111]);
			}
			return;
		}
		if ((inst & 0b1111000001111111) == 0b1000000000000010) {
			/*c.jr*/
			cpu.pc = cpu.xreg[(inst >> 7) & 0b11111];
			return;

		} else if ((inst & 0b1111000000000011) == 0b1000000000000010) {
			/*c.mv*/
			cpu.pc += 2;

			cpu.xreg[(inst >> 7) & 0b11111] = cpu.xreg[(inst >> 2) & 0b11111];
			return;
		}
		if ((inst & 0b1110000000000011) == 0b0000000000000001) {
			/*c.addi/c.nop*/
			cpu.pc += 2;

			uint16_t imm = // imm[5] imm[4:0]
					((inst & 0b0001000000000000) >> 7) |
					((inst & 0b0000000001111100) >> 2);

			cpu.xreg[(inst >> 7) & 0b11111] += ((int16_t) (imm << 10) >> 10);
			return;
		}
		if ((inst & 0b1110000000000011) == 0b0100000000000000) {
			/*c.lw*/
			uint16_t imm = // imm[5:3] imm[2|6]
					((inst & 0b0001110000000000) >> 7) |
					((inst & 0b0000000001000000) >> 4) |
					((inst & 0b0000000000100000) << 1);

			uint32_t addr= cpu.xreg[((inst >> 7) & 0b111) + 8] + imm;
			if (cpu_addr_translate(&addr, LOAD_PAGE_FAULT)) {
				cpu.pc += 2;

				bus_read(addr, 4, (uint8_t*) &cpu.xreg[((inst >> 2) & 0b111) + 8]);
			}
			return;
		}
		if ((inst & 0b1110000000000011) == 0b0100000000000001) {
			/*c.li*/
			cpu.pc += 2;

			uint16_t imm = // imm[5] imm[4:0]
					((inst & 0b0001000000000000) >> 7) |
					((inst & 0b0000000001111100) >> 2);

			cpu.xreg[(inst >> 7) & 0b11111] = ((int16_t) (imm << 10) >> 10);
			return;
		}
		if ((inst & 0b1110000000000011) == 0b1100000000000001) {
			/*c.beqz*/
			if (cpu.xreg[((inst >> 7) & 0b111) + 8] == 0) {

				uint16_t imm = // imm[8|4:3] imm[7:6|2:1|5]
						 ((inst & 0b0001000000000000) >> 4) |
						 ((inst & 0b0000110000000000) >> 7) |
						 ((inst & 0b0000000001100000) << 1) |
						 ((inst & 0b0000000000011000) >> 2) |
						 ((inst & 0b0000000000000100) << 3);

				cpu.pc += ((int16_t) (imm << 7) >> 7);

			} else cpu.pc += 2;

			return;
		}
		if ((inst & 0b1110000000000011) == 0b0000000000000000) {
			/*c.addi4spn*/
			cpu.pc += 2;

			uint16_t imm = // imm[5:4|9:6|2|3]
					((inst & 0b0001100000000000) >> 7) |
					((inst & 0b0000011110000000) >> 1) |
					((inst & 0b0000000001000000) >> 4) |
					((inst & 0b0000000000100000) >> 2);

			cpu.xreg[((inst >> 2) & 0b111) + 8] = cpu.xreg[2] + imm;
			return;
		}
		if ((inst & 0b1110000000000011) == 0b1110000000000001) {
			/*c.bnez*/
			if (cpu.xreg[((inst >> 7) & 0b111) + 8] != 0) {

				uint16_t imm = // imm[8|4:3] imm[7:6|2:1|5]
						((inst & 0b0001000000000000) >> 4) |
						((inst & 0b0000110000000000) >> 7) |
						((inst & 0b0000000001100000) << 1) |
						((inst & 0b0000000000011000) >> 2) |
						((inst & 0b0000000000000100) << 3);

				cpu.pc += ((int16_t) (imm << 7) >> 7);

			} else cpu.pc += 2;

			return;
		}
		/*if (inst == 0b1001000000000010) {
			c.ebreak

			cpu_take_trap(BREAKPOINT, cpu.pc + 2);
			return;

		} else*/ if ((inst & 0b1111000001111111) == 0b1001000000000010) {
			/*c.jalr*/

			uint32_t tmp = cpu.pc + 2;
			cpu.pc = cpu.xreg[(inst >> 7) & 0b11111];

			cpu.xreg[1] = tmp;
			return;

		} else if ((inst & 0b1111000000000011) == 0b1001000000000010) {
			/*c.add*/
			cpu.pc += 2;

			cpu.xreg[(inst >> 7) & 0b11111] += cpu.xreg[(inst >> 2) & 0b11111];
			return;
		}
		if ((inst & 0b1110000000000011) == 0b1010000000000001) {
			/*c.j*/
			uint16_t imm = // imm[11|4|9:8|10|6|7|3:1|5]
					((inst & 0b0001000000000000) >> 1) |
					((inst & 0b0000100000000000) >> 7) |
					((inst & 0b0000011000000000) >> 1) |
					((inst & 0b0000000100000000) << 2) |
					((inst & 0b0000000010000000) >> 1) |
					((inst & 0b0000000001000000) << 1) |
					((inst & 0b0000000000111000) >> 2) |
					((inst & 0b0000000000000100) << 3);

			cpu.pc += ((int16_t) (imm << 4) >> 4);
			return;
		}
		if ((inst & 0b1110111110000011) == 0b0110000100000001) {
			/*c.addi16sp*/
			cpu.pc += 2;

			uint16_t imm = // imm[9] | imm[4|6|8:7|5]
					((inst & 0b0001000000000000) >> 3) |
					((inst & 0b0000000001000000) >> 2) |
					((inst & 0b0000000000100000) << 1) |
					((inst & 0b0000000000011000) << 4) |
					((inst & 0b0000000000000100) << 3);

			cpu.xreg[2] += ((int16_t) (imm << 6) >> 6);
			return;

		} else if ((inst & 0b1110000000000011) == 0b0110000000000001) {
			/*c.lui*/
			cpu.pc += 2;

			uint32_t imm = // imm[17] | imm[16:12]
					((inst & 0b0001000000000000) << 5) |
					((inst & 0b0000000001111100) << 10);

			cpu.xreg[(inst >> 7) & 0b11111] = ((int32_t) (imm << 14) >> 14);
			return;
		}
		if ((inst & 0b1110110000000011) == 0b1000100000000001) {
			/*c.andi*/
			cpu.pc += 2;

			uint16_t imm = // imm[5] imm[4:0]
					((inst & 0b0001000000000000) >> 7) |
					((inst & 0b0000000001111100) >> 2);

			cpu.xreg[((inst >> 7) & 0b111) + 8] &= ((int16_t) (imm << 10) >> 10);
			return;
		}
		if ((inst & 0b1110000000000011) == 0b0010000000000001) {
			/*c.jal*/
			uint16_t imm = // imm[11|4|9:8|10|6|7|3:1|5]
					((inst & 0b0001000000000000) >> 1) |
					((inst & 0b0000100000000000) >> 7) |
					((inst & 0b0000011000000000) >> 1) |
					((inst & 0b0000000100000000) << 2) |
					((inst & 0b0000000010000000) >> 1) |
					((inst & 0b0000000001000000) << 1) |
					((inst & 0b0000000000111000) >> 2) |
					((inst & 0b0000000000000100) << 3);

			cpu.xreg[1] = cpu.pc + 2;
			cpu.pc += ((int16_t) (imm << 4) >> 4);
			return;
		}
		if ((inst & 0b1110000000000011) == 0b1100000000000000) {
			/*c.sw*/
			uint16_t imm = // imm[5:3] imm[2|6]
					((inst & 0b0001110000000000) >> 7) |
					((inst & 0b0000000001000000) >> 4) |
					((inst & 0b0000000000100000) << 1);

			uint32_t addr= cpu.xreg[((inst >> 7) & 0b111) + 8] + imm;
			if (cpu_addr_translate(&addr, STORE_AMO_PAGE_FAULT)) {
				cpu.pc += 2;

				bus_write(addr, 4, cpu.xreg[((inst >> 2) & 0b111) + 8]);
			}
			return;
		}
		if ((inst & 0b1110000000000011) == 0b0000000000000010) {
			/*c.slli*/
			cpu.pc += 2;

			uint16_t imm = // imm[5] imm[4:0]
//					((inst & 0b0001000000000000) >> 8) | zero for RV32C
					((inst & 0b0000000001111100) >> 2);

			cpu.xreg[(inst >> 7) & 0b11111] <<= imm;
			return;
		}
		if ((inst & 0b1110110000000011) == 0b1000000000000001) {
			/*c.srli*/
			cpu.pc += 2;

			uint32_t imm = /*imm[4:0]*/ ((inst >> 2) & 0x1f);

			cpu.xreg[((inst >> 7) & 0b111) + 8] >>= imm;
			return;
		}
		if ((inst & 0b1111110001100011) == 0b1000110001000001) {
			/*c.or*/
			cpu.pc += 2;

			cpu.xreg[((inst >> 7) & 0b111) + 8] |= cpu.xreg[((inst >> 2) & 0b111) + 8];
			return;
		}
		if ((inst & 0b1111110001100011) == 0b1000110001100001) {
			/*c.and*/
			cpu.pc += 2;

			cpu.xreg[((inst >> 7) & 0b111) + 8] &= cpu.xreg[((inst >> 2) & 0b111) + 8];
			return;
		}
		if ((inst & 0b1111110001100011) == 0b1000110000000001) {
			/*c.sub*/
			cpu.pc += 2;

			cpu.xreg[((inst >> 7) & 0b111) + 8] -= cpu.xreg[((inst >> 2) & 0b111) + 8];
			return;
		}
		if ((inst & 0b1111110001100011) == 0b1000110000100001) {
			/*c.xor*/
			cpu.pc += 2;

			cpu.xreg[((inst >> 7) & 0b111) + 8] ^= cpu.xreg[((inst >> 2) & 0b111) + 8];
			return;
		}
		if ((inst & 0b1110110000000011) == 0b1000010000000001) {
			/*c.srai*/
			cpu.pc += 2;

			uint32_t imm = /*imm[4:0]*/ ((inst >> 2) & 0x1f);

			cpu.xreg[((inst >> 7) & 0b111) + 8] = (int32_t) cpu.xreg[((inst >> 7) & 0b111) + 8] >> imm;
			return;
		}
		if ((inst & 0b1110000000000011) == 0b0010000000000000) {
			/*c.fld*/
			uint16_t imm = // imm[5:3] imm[7:6]
					((inst & 0b0001110000000000) >> 7) |
					((inst & 0b0000000001100000) << 1);

			uint32_t addr= cpu.xreg[((inst >> 7) & 0b111) + 8] + imm;
			if (cpu_addr_translate(&addr, LOAD_PAGE_FAULT)) {
				cpu.pc += 2;

				bus_read(addr, 8, (uint8_t*) &cpu.freg[((inst >> 2) & 0b111) + 8]);
			}
			return;
		}
	}

	cpu_take_trap(ILLEGAL_INSTRUCTION, inst);
}