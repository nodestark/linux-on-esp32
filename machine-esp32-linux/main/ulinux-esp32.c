#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/param.h>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

SemaphoreHandle_t vm_lock; /*cpu_loop and virtio_loop run as separate tasks and both touch window_[]/memory_file/csr/plic; serialize them*/

static int64_t host_get_time_us(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t) ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}

#define MAX_VQUEUE_SIZE 32 /*Number max of items in the queue (a power of 2)*/

struct virtq_used_elem_t {
	uint32_t id;
	uint32_t len;
};

struct virtq_used_t {
	uint16_t flags;
	uint16_t idx;
	struct virtq_used_elem_t ring[MAX_VQUEUE_SIZE];
	uint16_t used_event; /* Only if VIRTIO_F_EVENT_IDX */
};

struct virtq_avail_t {
	uint16_t flags;
	uint16_t idx;
	uint16_t ring[MAX_VQUEUE_SIZE];
	uint16_t avail_event; /* Only if VIRTIO_F_EVENT_IDX */
};

struct virtq_desc_t {
	uint64_t addr;
	uint32_t len;
	uint16_t flags;
	uint16_t next;
};

struct virtio_queue_t {
	uint32_t num;
	uint32_t ready;
	uint32_t desc_addr;
	uint32_t driver_addr;
	uint32_t dev_addr;
	uint8_t notify;
};

struct virtio_t {
	uint32_t dev_id;
	uint32_t dev_status;

	uint32_t dev_feat[2];
	uint32_t dev_feat_sel;

	uint32_t guest_feat[2];
	uint32_t guest_feat_sel;

	uint32_t int_status;
	uint32_t queue_sel;
	uint8_t *config_space;
	struct virtio_queue_t queue[3 /*Block Device... Console Device... GPIO Device...*/];
} virtio1_console = { .dev_id = 3 }, virtio2_blk = { .dev_id = 2 }, virtio3_gpio = { .dev_id = 41 }, virtio4_i2c = { .dev_id = 34 }, virtio8_net = { .dev_id = 1 };

struct uart_t {
	uint8_t ier /*Interrupt Enable Register*/;
	uint8_t fcr /*FIFO Control Register*/;
	uint8_t lcr /*Line Control Register*/;
	uint8_t lsr /*Line Status Register*/;
	uint8_t isr /*Interrupt Status Register*/;
	uint8_t mcr /*Modem Control Register*/;
	uint8_t msr /*Modem Status Register*/;
	uint8_t dll /*Divisor LSB*/;
	uint8_t dlm /*Divisor MSB*/;
	uint8_t thr /*Transmit Holding Register*/;
	uint8_t rhr /*Receive Holding Register*/;
	uint8_t spr /*Scratch Pad Register*/;
} uart_com_1 = { .lsr = 0 | (1 << 5 /*THR Empty*/) };

struct clint_t {
	uint64_t time;
	uint64_t timecmp;
} clint;

struct csr_t {
	struct satp_t {
		uint32_t ppn :22;
		uint16_t asid :9;
		uint8_t mode :1;
	} satp;
	uint32_t fflags;
	uint32_t frm;
	uint32_t fcsr;
	uint32_t misa;
	uint32_t mstatus, sstatus;
	uint32_t mhartid;
	uint32_t mscratch, sscratch;
	uint32_t mtvec, stvec;
	uint32_t mcountern, scounteren;
	uint32_t mie, sie;
	uint32_t mip, sip;
	uint32_t pmpaddr0;
	uint32_t pmpcfg0;
	uint32_t mepc, sepc;
	uint32_t mcause, scause;
	uint32_t mtval, stval;
	uint32_t mideleg;
	uint32_t medeleg;
} csr = { .mhartid = 0x00 };

enum RISCV_SHIFT_EXCEPTIONS {
	INSTRUCTION_ADDRESS_MISALIGNED = 0x0, 	/* Instruction address misaligned: An instruction fetch occurred from an address that is not aligned to the instruction width */
	INSTRUCTION_ACCESS_FAULT = 0x1, 		/* Instruction access fault: An attempt to fetch an instruction failed due to a memory access violation */
	ILLEGAL_INSTRUCTION = 0x2, 				/* Illegal instruction: An invalid or unsupported instruction was encountered */
	BREAKPOINT = 0x3, 						/* Breakpoint: A breakpoint (debug trap) instruction was encountered */
	LOAD_ADDRESS_MISALIGNED = 0x4, 			/* Load address misaligned: A memory read operation was attempted at an address that is not aligned to the data size */
	LOAD_ACCESS_FAULT = 0x5, 				/* Load access fault: A memory read operation failed due to a memory access violation */
	STORE_AMO_ADDRESS_MISALIGNED = 0x6, 	/* Store/AMO address misaligned: A memory write operation or an atomic operation was attempted at an address that is not aligned to the data size */
	STORE_AMO_ACCESS_FAULT = 0x7, 			/* Store/AMO access fault: A memory write operation or an atomic operation failed due to a memory access violation */
	ENV_CALL_FROM_UMODE = 0x8, 				/* Environment call from U-mode: A system call (ecall) was made from user mode (U-mode) */
	ENV_CALL_FROM_SMODE = 0x9, 				/* Environment call from S-mode: A system call (ecall) was made from supervisor mode (S-mode) */
	ENV_CALL_FROM_MMODE = 0xb, 				/* Environment call from M-mode: A system call (ecall) was made from machine mode (M-mode) */
	INSTRUCTION_PAGE_FAULT = 0xc, 			/* Instruction page fault: An instruction fetch failed due to a page fault */
	LOAD_PAGE_FAULT = 0xd, 					/* Load page fault: A memory read operation failed due to a page fault */
	STORE_AMO_PAGE_FAULT = 0xf 				/* Store/AMO page fault: A memory write operation or an atomic operation failed due to a page fault */
};

enum RISCV_SHIFT_INTERRUPTIONS {
	SUPERVISOR_SOFTWARE_INTERRUPT = 0x1,
	MACHINE_SOFTWARE_INTERRUPT = 0x3,
	SUPERVISOR_TIMER_INTERRUPT = 0x5,
	MACHINE_TIMER_INTERRUPT = 0x7,
	SUPERVISOR_EXTERNAL_INTERRUPT = 0x9,
	MACHINE_EXTERNAL_INTERRUPT = 0xb
};

struct plic_t {
	uint32_t sclaim;
} plic;

enum RISCV_BASEADDRS {
	RAM_BASE = 		0x80000000,
	PLIC_BASE = 	0x40100000,
	VIRTIO8_BASE = 	0x40080000,
	SYSCON_BASE = 	0x40060000,
	UART1_BASE = 	0x40050000,
	VIRTIO4_BASE = 	0x40040000,
	VIRTIO3_BASE = 	0x40030000,
	VIRTIO2_BASE = 	0x40020000,
	VIRTIO1_BASE = 	0x40010000,
	CLINT_BASE = 	0x02000000,
	DTB_BASE = 		0x00000000
};

typedef enum RISCV_PRIVILEGES {
	USER = 0x00, SUPERVISOR = 0x01, MACHINE = 0x03
} privelege_t;

struct cpu_t {
	uint64_t freg[32];
	uint32_t pc, xreg[32], reserved;
	privelege_t privilege;
} cpu = { .pc = 0x80000000, .xreg = { [10 /*a0*/]= 0x00, [11 /*a1*/]= DTB_BASE }, .privilege = MACHINE };

struct memory_t {
	void *data;
} low, rootfs;

#define GUEST_RAM_SIZE 0xf00000 /*15MB, matches memory@80000000 in the dtb; real memory kept in data/memory.bin*/

#define LRU_SEC_SIZE 4096 /*cache block size; also the sector size in memory.bin*/

struct window_t {
	uint8_t w; /*dirty*/
	uint8_t *data; /*NULL when swapped out to memory_file*/
	uint32_t time; /*last access timestamp, for eviction*/
} window_[GUEST_RAM_SIZE / LRU_SEC_SIZE]; /*one entry per sector of the whole guest RAM, direct-mapped by addr/LRU_SEC_SIZE*/

uint32_t lru_time;
uint32_t window_budget; /*max sectors kept resident in host RAM at once*/
uint32_t window_resident;

FILE *memory_file;

struct window_t *cpu_lru_translate(uint32_t addr) {

	uint32_t sector = addr / LRU_SEC_SIZE;
	struct window_t *win = &window_[sector];

	win->time = ++lru_time;

	if (win->data)
		return win;

	win->data = (uint8_t*) malloc(LRU_SEC_SIZE);
	assert(win->data);

	fseek(memory_file, (long) sector * LRU_SEC_SIZE, SEEK_SET);
	fread(win->data, 1, LRU_SEC_SIZE, memory_file);

	if (++window_resident > window_budget) {
		/*every slot's sector is its own fixed index, so eviction never reassigns sectors between slots
		 * (that's what corrupted data in the esp32-s3 reference: it recycled a slot's sector into a
		 * different slot, so two guest addresses could end up sharing one flash sector)*/

		struct window_t *tmp = (struct window_t*) 0;
		for (uint32_t x = 0; x < (GUEST_RAM_SIZE / LRU_SEC_SIZE); x++)
			if (window_[x].data && &window_[x] != win && (!tmp || window_[x].time < tmp->time))
				tmp = &window_[x];

		if (tmp->w) {
			fseek(memory_file, (long) (tmp - window_) * LRU_SEC_SIZE, SEEK_SET);
			fwrite(tmp->data, 1, LRU_SEC_SIZE, memory_file);
		}

		free(tmp->data);
		tmp->data = (uint8_t*) 0;
		tmp->w = 0;

		--window_resident;
	}

	return win;
}

void stream_lru_read(uint32_t addr, uint32_t size, uint8_t *buff) {

	struct window_t *win = cpu_lru_translate(addr);

	uint16_t reading = MIN(LRU_SEC_SIZE - (addr % LRU_SEC_SIZE), size);
	memcpy(buff, win->data + (addr % LRU_SEC_SIZE), reading);

	if (size - reading)
		stream_lru_read(addr + reading, size - reading, buff + reading);
}

void stream_lru_write(uint32_t addr, uint32_t size, uint8_t *buff) {

	struct window_t *win = cpu_lru_translate(addr);
	win->w = 1;

	uint16_t reading = MIN(LRU_SEC_SIZE - (addr % LRU_SEC_SIZE), size);
	memcpy(win->data + (addr % LRU_SEC_SIZE), buff, reading);

	if (size - reading)
		stream_lru_write(addr + reading, size - reading, buff + reading);
}

void cpu_take_trap(uint32_t cause, uint32_t tval) {

	if (cpu.privilege <= SUPERVISOR
			&& (((cause & 0x80000000) && ((csr.mideleg >> (cause & 0x7FFFFFFF)) & 1)) || ((~cause & 0x80000000) && ((csr.medeleg >> cause) & 1)))) {

		csr.sepc = cpu.pc;

		if (cause & (1 << 31)) {
			/*This bit is set when the exception was triggered by an interrupt.*/

			uint32_t vectored = (csr.stvec & 3) ? ((cause & 0x7FFFFFFF) << 2) : 0;
			cpu.pc = (csr.stvec & ~3) + vectored;

		} else {

			cpu.pc = csr.stvec & ~3;
		}

		csr.scause = cause;
		csr.stval = tval;

		csr.sstatus &= ~(1 << 5 /*SPIE*/);                         	// clear SPIE
		csr.sstatus |= ((csr.sstatus >> cpu.privilege) & 1) << 5; 	// copies SIE into SPIE
		csr.sstatus &= ~(1 << cpu.privilege /*SIE*/);               // clear SIE

		csr.sstatus &= ~(1 << 8 /*SPP*/);          		// clear SPP
		csr.sstatus |= (cpu.privilege << 8 /*SPP*/); 	// copies current privilege into SPP

		cpu.privilege = SUPERVISOR;

	} else {

		csr.mepc = cpu.pc;

		if (cause & (1 << 31)) {
			/*This bit is set when the exception was triggered by an interrupt.*/

			uint32_t vectored = (csr.mtvec & 3) ? ((cause & 0x7FFFFFFF) << 2) : 0;
			cpu.pc = (csr.mtvec & ~3) + vectored;

		} else {

			cpu.pc = csr.mtvec & ~3;
		}

		csr.mcause = cause;
		csr.mtval = tval;

		csr.mstatus &= ~(1 << 7 /*MPIE*/);                        	// clear MPIE
		csr.mstatus |= ((csr.mstatus >> cpu.privilege) & 1) << 7; 	// copies MIE into MPIE
		csr.mstatus &= ~(1 << cpu.privilege /*MIE*/);             	// clear MIE

		csr.mstatus &= ~(3 << 11 /*MPP*/);              // clear MPP
		csr.mstatus |= (cpu.privilege << 11 /*MPP*/); 	// copies current privilege into MPP

		cpu.privilege = MACHINE;
	}
}

#define TLB

/* The TLB is a high-level cache of the page table, which stores the most recently used translations and makes them quickly and efficiently accessible.
 * Instead of accessing the page table in main memory, the processor first checks the TLB to see if the translation of the virtual address is already stored there.
 * If it is, the translation is used directly, without the need to access the page table in main memory.
 * This reduces memory access latency and increases system performance. */

#define TLB_SIZE 89

struct tlb_t {
	uint32_t vaddr;
	uint32_t paddr;
	uint8_t except;
} tlb_[TLB_SIZE], *tlb;

int8_t cpu_addr_translate(uint32_t *addr, uint8_t except) {

	tlb = &tlb_[(*addr >> 12) % TLB_SIZE];

#ifdef TLB
	if (tlb->vaddr == (*addr & ~0xfff) && tlb->except == except) {
		*addr = (tlb->paddr | (*addr & 0xfff));
		return 1;
	}
#endif

	if (csr.satp.mode && cpu.privilege <= SUPERVISOR) {

		/* ppn - physical page number
		 * vpn - virtual page number
		 * pte - page table entry */

		struct sv32_va_t {
			uint32_t offset :12;
			uint32_t vpn_0 :10;
			uint32_t vpn_1 :10;
		} *sv32_va = (struct sv32_va_t*) addr;

		uint32_t vpn[2] = { sv32_va->vpn_0, sv32_va->vpn_1 };

		struct sv32_pte_t {
			uint8_t v :1;
			uint8_t r :1;
			uint8_t w :1;
			uint8_t x :1;
			uint8_t u :1;
			uint8_t g :1;
			uint8_t a :1;
			uint8_t d :1;
			uint8_t rsw :2;
			uint32_t ppn_0 :10;
			uint32_t ppn_1 :12;
		} sv32_pte;

		uint32_t a = csr.satp.ppn << 12;
		for (int i = 1; i >= 0 /*sv32 2 levels*/; i--) {

			stream_lru_read((a | vpn[i] << 2) - RAM_BASE, sizeof(struct sv32_pte_t), (uint8_t*) &sv32_pte);

			if (!sv32_pte.v) {
				cpu_take_trap(except, *addr);
				return 0;
			}

			if (sv32_pte.r || sv32_pte.x) {

				if (except == STORE_AMO_PAGE_FAULT && !sv32_pte.w) {
					cpu_take_trap(except, *addr);
					return 0;
				}

				if (!sv32_pte.a) {

					sv32_pte.a = 1;
					stream_lru_write((a | vpn[i] << 2) - RAM_BASE, sizeof(struct sv32_pte_t), (uint8_t*) &sv32_pte);
				}

				if (except == STORE_AMO_PAGE_FAULT && !sv32_pte.d) {

					sv32_pte.d = 1;
					stream_lru_write((a | vpn[i] << 2) - RAM_BASE, sizeof(struct sv32_pte_t), (uint8_t*) &sv32_pte);
				}

//				assert( !(except == STORE_AMO_PAGE_FAULT && !sv32_pte.w) );
//
//				assert( !(except == INSTRUCTION_PAGE_FAULT && !sv32_pte.x) );
//				assert(!(csr.sstatus & (1 << 19 /*mxr*/)));
//
//				assert( !(except == LOAD_PAGE_FAULT && !sv32_pte.r) );
//				assert( !(sv32_pte.a == 0) );
//				assert( !(except == STORE_AMO_PAGE_FAULT && !sv32_pte.d) );
//
//				uint8_t privilege = cpu.privilege;
//
//				if (((csr.sstatus >> 17) & 1 /*MPRV*/) && (except != INSTRUCTION_PAGE_FAULT)) {
//					privilege = ((csr.sstatus >> 11) & 3 /*MPP*/);
//				}
//				assert( !(privilege == USER && !sv32_pte.u) );
//
//				assert( !((privilege == SUPERVISOR) && sv32_pte.u && !(csr.sstatus & (1 << 18 /*sum*/))) );

				tlb->vaddr = (*addr & ~0xfff);

				switch (i) {
				case 1:
					*addr = ((sv32_pte.ppn_1 << 10) | sv32_va->vpn_0) << 12 | sv32_va->offset;
					break;
				case 0:
					*addr = ((sv32_pte.ppn_1 << 10) | sv32_pte.ppn_0) << 12 | sv32_va->offset;
				}

				tlb->paddr = (*addr & ~0xfff);
				tlb->except = except;

				return 1;
			}
			a = ((sv32_pte.ppn_1 << 10) | sv32_pte.ppn_0) << 12;
		}

		cpu_take_trap(except, *addr);

		return 0;
	}

	return 1;
}

void csr_read_(uint32_t offset, uint32_t **value) {
//	assert( cpu.privilege >= ((offset >> 8) & 3 /*priv*/) );

	switch (offset) {
	case 0x001:
		*value = &csr.fflags;
		break;
	case 0x002:
		*value = &csr.frm;
		break;
	case 0x003:
		*value = &csr.fcsr;
		break;
	case 0x100:
		*value = &csr.sstatus;
		break;
	case 0x104:
		*value = &csr.sie;
		break;
	case 0x105:
		*value = &csr.stvec;
		break;
	case 0x106:
		*value = &csr.scounteren;
		break;
	case 0x140:
		*value = &csr.sscratch;
		break;
	case 0x141:
		*value = &csr.sepc;
		break;
	case 0x142:
		*value = &csr.scause;
		break;
	case 0x143:
		*value = &csr.stval;
		break;
	case 0x144:
		*value = &csr.sip;
		break;
	case 0x180:
		*value = (uint32_t*) &csr.satp;
		break;
	case 0x300:
		*value = &csr.mstatus;
		break;
	case 0x301:
		*value = &csr.misa;
		break;
	case 0x302:
		*value = &csr.medeleg;
		break;
	case 0x303:
		*value = &csr.mideleg;
		break;
	case 0x304:
		*value = &csr.mie;
		break;
	case 0x305:
		*value = &csr.mtvec;
		break;
	case 0x306:
		*value = &csr.mcountern;
		break;
	case 0x340:
		*value = &csr.mscratch;
		break;
	case 0x341:
		*value = &csr.mepc;
		break;
	case 0x342:
		*value = &csr.mcause;
		break;
	case 0x343:
		*value = &csr.mtval;
		break;
	case 0x344:
		*value = &csr.mip;
		break;
	case 0x3a0:
		*value = &csr.pmpcfg0;
		break;
	case 0x3b0:
		*value = &csr.pmpaddr0;
		break;
	case 0xc01:
		*value = (uint32_t*) &clint.time;
		break;
	case 0xc81:
		*value = ((uint32_t*) &clint.time) + 1;
		break;
	case 0xf14:
		*value = &csr.mhartid;
		break;
	default:
		assert(0);
	}
}

void plic_read(uint32_t offset, uint32_t *value) {

	switch (offset) {
	case 0x00200000:
		*value = 0;
		break;
	case 0x00200004:

		*value = 0;

		for(int irq= 1; irq < 32; irq++)
			if (plic.sclaim & (1 << irq))
				*value = irq;

		break;
	default:
		*value = 0;
	}
}

void plic_write(uint32_t offset, uint32_t value) {

	switch (offset) {
	case 0x200004: /*Claim acknowledge*/
		plic.sclaim &= ~(1 << value);
		break;
	}
}

void clint_read(uint32_t offset, uint32_t *value) {

	switch (offset) {
	case 0x00004000:
		*value = clint.timecmp;
		break;
	case 0x00004004:
		*value = clint.timecmp >> 32;
		break;
	default:
		*value = 0;
		break;
	}
}

void clint_write(uint32_t offset, uint32_t value) {

	switch (offset) {
	case 0x00004000: /*lower 32 bits*/
		clint.timecmp = (clint.timecmp & 0xFFFFFFFF00000000ULL) | value;
		break;
	case 0x00004004: /*upper 32 bits*/
		clint.timecmp = (clint.timecmp & 0x00000000FFFFFFFFULL) | ((uint64_t) value << 32);
		break;
	}
}

void uart1_write(uint32_t offset, uint8_t value) {

	switch (offset) {
	case 0b000:

		if (~uart_com_1.lcr & (1 << 7 /*Divisor Enable*/)) {
			uart_com_1.thr = value;
		} else {
			uart_com_1.dll = value;
		}
		break;
	case 0b001:
		(uart_com_1.lcr & (1 << 7 /*Divisor Enable*/) ? (uart_com_1.dlm = value) : (uart_com_1.ier = value));

		if (uart_com_1.ier & (1 << 1 /*TX (THR)*/)) {

			plic.sclaim |= (1 << 0x05 /*uart_com_1's irq*/);
			csr.sip |= (1 << SUPERVISOR_EXTERNAL_INTERRUPT);
		}

		break;
	case 0b010:
		uart_com_1.fcr = value;
		break;
	case 0b011:
		uart_com_1.lcr = value;
		break;
	case 0b100:
		uart_com_1.mcr = value;
		break;
	case 0b111:
		uart_com_1.spr = value;
		break;
	default:
		assert(0);
	}
}

void uart1_read(uint32_t offset, uint8_t *value) {

	switch(offset){
	case 0b000:
		(uart_com_1.lcr & (1 << 7 /*Divisor Enable*/) ? (*value= uart_com_1.dll) : (*value= uart_com_1.rhr));

		uart_com_1.lsr &= ~1;

		break;
	case 0b001:
		(uart_com_1.lcr & (1 << 7 /*Divisor Enable*/) ? (*value= uart_com_1.dlm) : (*value= uart_com_1.ier));
		break;
	case 0b010:
		*value= uart_com_1.isr;
		break;
	case 0b011:
		*value= uart_com_1.lcr;
		break;
	case 0b100:
		*value= uart_com_1.mcr;
		break;
	case 0b101:
		*value= uart_com_1.lsr;
		break;
	case 0b110:
		*value= uart_com_1.msr;
		break;
	case 0b111:
		*value= uart_com_1.spr;
		break;
	default:
		assert(0);
	}
}

void virtio_read(uint32_t offset, uint8_t size, uint32_t *value, struct virtio_t *virtio) {

	switch (offset) {
	case 0x000: /*Magic value - R*/
		*value = 0x74726976;
		break;
	case 0x004: /*Device version number - R*/
		*value = 2;
		break;
	case 0x008: /*Virtio Subsystem Device ID - R*/
		*value = virtio->dev_id;
		break;
	case 0x010: /*Flags representing features the device supports - R*/
		*value = virtio->dev_feat[virtio->dev_feat_sel];
		break;
	case 0x00c: /*Virtio Subsystem Vendor ID - R*/
		*value = 0x0000ffff;
		break;
	case 0x034: /*Maximum virtual queue size - R*/
		*value = MAX_VQUEUE_SIZE;
		break;
	case 0x044:
		*value = virtio->queue[virtio->queue_sel].ready;
		break;
	case 0x060: /*Interrupt status - R*/
		*value = virtio->int_status;
		break;
	case 0x070: /*Device status - RW*/
		*value = virtio->dev_status;
		break;
	case 0x0fc: /*Configuration atomicity value - R*/
		*value = 0;
		break;
	default:

		if (offset >= 0x100) {
			memcpy(value, virtio->config_space + (offset - 0x100), size);
			return;
		}

		assert(0); // ...virtio address not found
	}
}

void virtio_write(uint32_t offset, uint8_t size, uint32_t value, struct virtio_t *virtio) {

	switch (offset) {
	case 0x014: /*Device (host) features word selection - W*/
		virtio->dev_feat_sel = value;
		break;
	case 0x020: /*Flags representing device features understood and activated by the driver - W*/
		virtio->guest_feat[virtio->guest_feat_sel]= value;
		break;
	case 0x024: /*Activated (guest) features word selection - W*/
		virtio->guest_feat_sel = value;
		break;
	case 0x030: /*Virtual queue index - W*/
//		assert(value < (sizeof(virtio->queue)/sizeof(struct virtio_queue_t)));

		virtio->queue_sel = value;
		break;
	case 0x038: /*Virtual queue size - W*/
		virtio->queue[virtio->queue_sel].num = value;
		break;
	case 0x044:
		virtio->queue[virtio->queue_sel].ready = value;
		break;
	case 0x050: /*Queue notifier - W*/
		virtio->queue[value].notify = 1;
		break;
	case 0x064: /*Interrupt acknowledge - W*/
		virtio->int_status &= ~value;
		break;
	case 0x070: /*Device status - RW*/

//		assert(!(value == 0 && virtio->dev_status != 0));

		// #ACKNOWLEDGE (1 << 0)
		// #DRIVER (1 << 1)
		// #DRIVER_OK (1 << 3)
		// #FEATURES_OK (1 << 4)
		// #DEVICE_NEEDS_RESET (1 << 7)
		// #FAILED (1 << 8)

		virtio->dev_status = value;

		break;
	case 0x080:
		*(uint32_t*) &virtio->queue[virtio->queue_sel].desc_addr = value;
		break;
	case 0x084:
		*((uint32_t*) &virtio->queue[virtio->queue_sel].desc_addr + 1)= value;
		break;
	case 0x090:
		*(uint32_t*) &virtio->queue[virtio->queue_sel].driver_addr = value;
		break;
	case 0x094:
		*((uint32_t*) &virtio->queue[virtio->queue_sel].driver_addr + 1)= value;
		break;
	case 0x0a0:
		*(uint32_t*) &virtio->queue[virtio->queue_sel].dev_addr = value;
		break;
	case 0x0a4:
		*((uint32_t*) &virtio->queue[virtio->queue_sel].dev_addr + 1)= value;
		break;
	default:
		assert(0); // ...virtio address not found
	}
}

void bus_read(uint32_t addr, uint8_t size, uint8_t *value) {

	if (addr >= RAM_BASE) {
		stream_lru_read(addr - RAM_BASE, size, value);
		return;
	}

	if (addr >= CLINT_BASE && addr <= (CLINT_BASE + 0xc0000)) {
		clint_read(addr - CLINT_BASE, (uint32_t*) value);
		return;
	}

	if (addr >= PLIC_BASE && addr <= (PLIC_BASE + 0x400000)) {
		plic_read(addr - PLIC_BASE, (uint32_t*) value);
		return;
	}

	if (addr >= VIRTIO8_BASE && addr <= (VIRTIO8_BASE + 0x1000)) {
		virtio_read(addr - VIRTIO8_BASE, size, (uint32_t*) value, &virtio8_net);
		return;
	}

	if (addr >= UART1_BASE && addr <= (UART1_BASE + 0x100)) {
		uart1_read(addr - UART1_BASE, value);
		return;
	}

	if (addr >= VIRTIO4_BASE && addr <= (VIRTIO4_BASE + 0x1000)) {
		virtio_read(addr - VIRTIO4_BASE, size, (uint32_t*) value, &virtio4_i2c);
		return;
	}

	if (addr >= SYSCON_BASE && addr <= (SYSCON_BASE + 0x1000)) {
		*((uint32_t*) value) = 0x00000000;
		return;
	}

	if (addr >= VIRTIO3_BASE && addr <= (VIRTIO3_BASE + 0x1000)) {
		virtio_read(addr - VIRTIO3_BASE, size, (uint32_t*) value, &virtio3_gpio);
		return;
	}

	if (addr >= VIRTIO2_BASE && addr <= (VIRTIO2_BASE + 0x1000)) {
		virtio_read(addr - VIRTIO2_BASE, size, (uint32_t*) value, &virtio2_blk);
		return;
	}

	if (addr >= VIRTIO1_BASE && addr <= (VIRTIO1_BASE + 0x1000)) {
		virtio_read(addr - VIRTIO1_BASE, size, (uint32_t*) value, &virtio1_console);
		return;
	}

	if (addr <= (DTB_BASE + 4096 /*4KB*/)) {
		memcpy((uint8_t*) value, (uint8_t*) (low.data + (addr - DTB_BASE)), size);
		return;
	}

	cpu_take_trap(LOAD_ACCESS_FAULT, addr);
}

void bus_write(uint32_t addr, uint8_t size, uint64_t value) {

	if (addr >= RAM_BASE) {
		stream_lru_write(addr - RAM_BASE, size, (uint8_t*) &value);
		return;
	}

	if (addr >= CLINT_BASE && addr <= (CLINT_BASE + 0xc0000)) {
		clint_write(addr - CLINT_BASE, value);
		return;
	}

	if (addr >= PLIC_BASE && addr <= (PLIC_BASE + 0x400000)) {
		plic_write(addr - PLIC_BASE, value);
		return;
	}

	if (addr >= VIRTIO8_BASE && addr <= (VIRTIO8_BASE + 0x1000)) {
		virtio_write(addr - VIRTIO8_BASE, size, value, &virtio8_net);
		return;
	}

	if (addr >= UART1_BASE && addr <= (UART1_BASE + 0x100)) {
		uart1_write(addr - UART1_BASE, value);
		return;
	}

	if (addr >= VIRTIO4_BASE && addr <= (VIRTIO4_BASE + 0x1000)) {
		virtio_write(addr - VIRTIO4_BASE, size, value, &virtio4_i2c);
		return;
	}

	if (addr >= SYSCON_BASE && addr <= (SYSCON_BASE + 0x1000)) {

		switch(addr - SYSCON_BASE){
		case 0x00 /*offset*/:
			if (value == 0x00007777 /*syscon-reboot*/
					|| value == 0x00005555 /*syscon-poweroff*/) {
				exit(0);
			}
		}

		return;
	}

	if (addr >= VIRTIO3_BASE && addr <= (VIRTIO3_BASE + 0x1000)) {
		virtio_write(addr - VIRTIO3_BASE, size, value, &virtio3_gpio);
		return;
	}

	if (addr >= VIRTIO2_BASE && addr <= (VIRTIO2_BASE + 0x1000)) {
		virtio_write(addr - VIRTIO2_BASE, size, value, &virtio2_blk);
		return;
	}

	if (addr >= VIRTIO1_BASE && addr <= (VIRTIO1_BASE + 0x1000)) {
		virtio_write(addr - VIRTIO1_BASE, size, value, &virtio1_console);
		return;
	}

	if (addr <= (DTB_BASE + 4096 /*4KB*/)) {
		memcpy((uint8_t*) (low.data + (addr - DTB_BASE)), (uint8_t*) &value, size);
		return;
	}

	cpu_take_trap(STORE_AMO_ACCESS_FAULT, addr);
}

#include "cpu.h"
#include "cpu.c"

void cpu_loop(void *pvParameters){

	uint32_t addr, inst[2];

	for(;;){

		xSemaphoreTake(vm_lock, portMAX_DELAY);

		clint.time = host_get_time_us() /*microseconds*/ * 240 /*Mhz*/;

		if ( (uint32_t) clint.time > (uint32_t) clint.timecmp) {
			csr.sip |= (1 << SUPERVISOR_TIMER_INTERRUPT);
		}

		for(uint16_t x= 46368; x--; /*only to performance, prioritizing the cpu_execute*/){

			addr= cpu.pc & ~3;
			if (cpu_addr_translate(&addr, INSTRUCTION_PAGE_FAULT)) {

				stream_lru_read(addr - RAM_BASE, 8, (uint8_t*) &inst);

				if ((cpu.pc & 2) == 2) {

					inst[0] >>= 16;

					if ((inst[0] & 3) == 3) {

						inst[0] |= (inst[1] << 16);
					}
				}
				cpu_execute(inst[0]);
			}

			/*Check interrupt enabled*/
			if (cpu.privilege <= SUPERVISOR && !((csr.sstatus >> cpu.privilege) & 1))
				continue;

			if (cpu.privilege == MACHINE && !((csr.mstatus >> MACHINE) & 1))
				continue;

			/*Check pending interrupt*/
			if (csr.sie & csr.sip & (1 << SUPERVISOR_TIMER_INTERRUPT)) {
				csr.sip &= ~(1 << SUPERVISOR_TIMER_INTERRUPT);
				cpu_take_trap(SUPERVISOR_TIMER_INTERRUPT | (1 << 31), 0);

			} else if (csr.sie & csr.sip & (1 << SUPERVISOR_EXTERNAL_INTERRUPT)) {
				csr.sip &= ~(1 << SUPERVISOR_EXTERNAL_INTERRUPT);
				cpu_take_trap(SUPERVISOR_EXTERNAL_INTERRUPT | (1 << 31), 0);

			} /*else if (csr.sie & csr.sip & (1 << SUPERVISOR_SOFTWARE_INTERRUPT)) {
				csr.sip &= ~(1 << SUPERVISOR_SOFTWARE_INTERRUPT);
				cpu_take_trap(SUPERVISOR_SOFTWARE_INTERRUPT | (1 << 31), 0);

			} else if (csr.mie & csr.mip & (1 << MACHINE_EXTERNAL_INTERRUPT)) {
				csr.mip &= ~(1 << MACHINE_EXTERNAL_INTERRUPT);
				cpu_take_trap(MACHINE_EXTERNAL_INTERRUPT | (1 << 31), 0);

			} else if (csr.mie & csr.mip & (1 << MACHINE_TIMER_INTERRUPT)) {
				csr.mip &= ~(1 << MACHINE_TIMER_INTERRUPT);
				cpu_take_trap(MACHINE_TIMER_INTERRUPT | (1 << 31), 0);

			} else if (csr.mie & csr.mip & (1 << MACHINE_SOFTWARE_INTERRUPT)) {
				csr.mip &= ~(1 << MACHINE_SOFTWARE_INTERRUPT);
				cpu_take_trap(MACHINE_SOFTWARE_INTERRUPT | (1 << 31), 0);
			}*/
		}

		xSemaphoreGive(vm_lock);

		taskYIELD(); /*this task and virtio_loop run at the same priority with no other blocking calls; without a yield one of them starves the other of CPU*/
	}
}

void virtio_loop(void *pvParameters) {

	uint8_t uart_buff[16], uart_reading;

	for(;;){

		xSemaphoreTake(vm_lock, portMAX_DELAY);

		if (virtio1_console.queue[1 /*transmitq*/].notify) {
			struct virtio_queue_t *virtio_queue = &virtio1_console.queue[1];

			virtio_queue->notify = 0;

			struct virtq_avail_t virtq_avail;
			stream_lru_read(virtio_queue->driver_addr - RAM_BASE, sizeof(struct virtq_avail_t), (uint8_t*) &virtq_avail);

			struct virtq_used_t virtq_used;
			stream_lru_read(virtio_queue->dev_addr - RAM_BASE, sizeof(struct virtq_used_t), (uint8_t*) &virtq_used);

			while (virtq_used.idx != virtq_avail.idx) {

				struct virtq_desc_t virtq_desc;
				stream_lru_read( virtio_queue->desc_addr + virtq_avail.ring[virtq_used.idx % virtio_queue->num] * sizeof(struct virtq_desc_t) - RAM_BASE, sizeof(struct virtq_desc_t), (uint8_t*) &virtq_desc);

				uint8_t buff[virtq_desc.len];
				stream_lru_read(virtq_desc.addr - RAM_BASE, sizeof(buff), (uint8_t*) &buff);

				fwrite(&buff, sizeof(uint8_t), sizeof(buff), stdout);

				virtq_used.ring[virtq_used.idx % virtio_queue->num].id = virtq_avail.ring[virtq_used.idx % virtio_queue->num];
				virtq_used.ring[virtq_used.idx % virtio_queue->num].len = virtq_desc.len;

				++virtq_used.idx;
			}
			stream_lru_write(virtio_queue->dev_addr - RAM_BASE, sizeof(struct virtq_used_t), (uint8_t*) &virtq_used);
		}

		if (virtio2_blk.queue[0 /*requestq*/].notify) {
			struct virtio_queue_t *virtio_queue = &virtio2_blk.queue[0];

			virtio_queue->notify = 0;

			struct virtq_avail_t virtq_avail;
			stream_lru_read(virtio_queue->driver_addr - RAM_BASE, sizeof(struct virtq_avail_t), (uint8_t*) &virtq_avail);

			struct virtq_used_t virtq_used;
			stream_lru_read(virtio_queue->dev_addr - RAM_BASE, sizeof(struct virtq_used_t), (uint8_t*) &virtq_used);

			while (virtq_used.idx != virtq_avail.idx) {

				struct virtq_desc_t virtq_desc;
				stream_lru_read( virtio_queue->desc_addr + virtq_avail.ring[virtq_used.idx % virtio_queue->num] * sizeof(struct virtq_desc_t) - RAM_BASE, sizeof(struct virtq_desc_t), (uint8_t*) &virtq_desc);

				struct virtq_desc_t virtq_desc_1;
				stream_lru_read( virtio_queue->desc_addr + virtq_desc.next * sizeof(struct virtq_desc_t) - RAM_BASE, sizeof(struct virtq_desc_t), (uint8_t*) &virtq_desc_1);

				struct virtq_desc_t virtq_desc_2;
				stream_lru_read( virtio_queue->desc_addr + virtq_desc_1.next * sizeof(struct virtq_desc_t) - RAM_BASE, sizeof(struct virtq_desc_t), (uint8_t*) &virtq_desc_2);

				struct virtio_blk_req_t {
					uint32_t type;
					uint32_t unused0;
					uint64_t sector;
				} virtio_blk_req;

				stream_lru_read(virtq_desc.addr - RAM_BASE, sizeof(struct virtio_blk_req_t), (uint8_t*) &virtio_blk_req);

				fseek((FILE*) rootfs.data, virtio_blk_req.sector * 512, SEEK_SET);

				uint8_t buff[1024];
				switch(virtq_desc_1.flags & (2 /*VRING_DESC_F_WRITE*/)){
				case 0:
					for(uint32_t i= 0; i < virtq_desc_1.len; i += MIN(virtq_desc_1.len - i, sizeof(buff)) ){

						stream_lru_read(virtq_desc_1.addr + i - RAM_BASE, MIN(virtq_desc_1.len - i, sizeof(buff)), (uint8_t*) &buff);
						fwrite(&buff, sizeof(uint8_t), MIN(virtq_desc_1.len - i, sizeof(buff)), (FILE*) rootfs.data);
					}
					break;
				case 2:
					for(uint32_t i= 0; i < virtq_desc_1.len; i += MIN(virtq_desc_1.len - i, sizeof(buff)) ){

						fread(&buff, sizeof(uint8_t), MIN(virtq_desc_1.len - i, sizeof(buff)), (FILE*) rootfs.data);
						stream_lru_write(virtq_desc_1.addr + i - RAM_BASE, MIN(virtq_desc_1.len - i, sizeof(buff)), (uint8_t*) &buff);
					}
				}

				uint8_t status[1] = { 0x00 /*VIRTIO_BLK_S_OK*/};
				stream_lru_write(virtq_desc_2.addr - RAM_BASE, 1, (uint8_t*) &status);

				virtq_used.ring[virtq_used.idx % virtio_queue->num].id = virtq_avail.ring[virtq_used.idx % virtio_queue->num];
				virtq_used.ring[virtq_used.idx % virtio_queue->num].len = virtq_desc_1.len;

				++virtq_used.idx;
			}
			stream_lru_write(virtio_queue->dev_addr - RAM_BASE, sizeof(struct virtq_used_t), (uint8_t*) &virtq_used);

			plic.sclaim |= (1 << 0x02 /*block's irq*/);
			csr.sip |= (1 << SUPERVISOR_EXTERNAL_INTERRUPT);

			virtio2_blk.int_status |= 1;
		}

		if (virtio3_gpio.queue[0 /*requestq*/].notify) {
			struct virtio_queue_t *virtio_queue = &virtio3_gpio.queue[0];

			virtio_queue->notify = 0;

			struct virtq_avail_t virtq_avail;
			stream_lru_read(virtio_queue->driver_addr - RAM_BASE, sizeof(struct virtq_avail_t), (uint8_t*) &virtq_avail);

			struct virtq_used_t virtq_used;
			stream_lru_read(virtio_queue->dev_addr - RAM_BASE, sizeof(struct virtq_used_t), (uint8_t*) &virtq_used);

			struct virtq_desc_t virtq_desc;
			stream_lru_read( virtio_queue->desc_addr + virtq_avail.ring[virtq_used.idx % virtio_queue->num] * sizeof(struct virtq_desc_t) - RAM_BASE, sizeof(struct virtq_desc_t), (uint8_t*) &virtq_desc);

			struct virtq_desc_t virtq_desc_1;
			stream_lru_read( virtio_queue->desc_addr + virtq_desc.next * sizeof(struct virtq_desc_t) - RAM_BASE, sizeof(struct virtq_desc_t), (uint8_t*) &virtq_desc_1);

			struct virtio_gpio_resp_t {
				uint8_t status;
				uint8_t value;
			};

			struct virtio_gpio_resp_t virtio_gpio_resp= {
					.status= (0x01 /* VIRTIO_GPIO_STATUS_ERR*/),
					.value= 0x00
			};

			struct virtio_gpio_req_t {
				uint16_t type;
				uint16_t gpio;
				uint32_t value;
			} virtio_gpio_req;

			stream_lru_read(virtq_desc.addr - RAM_BASE, sizeof(struct virtio_gpio_req_t), (uint8_t*) &virtio_gpio_req);

			do {
				/*No real GPIO on the linux target: line state is emulated in software.*/
				static uint8_t gpio_level[2];

				if (virtio_gpio_req.gpio > 1) break;

				if (virtio_gpio_req.type == 0x004 /*VIRTIO_GPIO_MSG_GET_VALUE*/) {
					virtio_gpio_resp.value = gpio_level[virtio_gpio_req.gpio];

				} else if (virtio_gpio_req.type == 0x005 /*VIRTIO_GPIO_MSG_SET_VALUE*/) {
					gpio_level[virtio_gpio_req.gpio] = virtio_gpio_req.value;

				} else if (virtio_gpio_req.type != 0x002 /*VIRTIO_GPIO_MSG_GET_DIRECTION*/ && virtio_gpio_req.type != 0x003 /*VIRTIO_GPIO_MSG_SET_DIRECTION*/) {
					break;
				}

				virtio_gpio_resp.status = (0x00 /*VIRTIO_GPIO_STATUS_OK*/);

			} while (0);
			stream_lru_write(virtq_desc_1.addr - RAM_BASE, sizeof(struct virtio_gpio_resp_t), (uint8_t*) &virtio_gpio_resp);

			virtq_used.ring[virtq_used.idx % virtio_queue->num].id = virtq_avail.ring[virtq_used.idx % virtio_queue->num];
			virtq_used.ring[virtq_used.idx % virtio_queue->num].len = sizeof(struct virtio_gpio_resp_t);
			++virtq_used.idx;

			stream_lru_write(virtio_queue->dev_addr - RAM_BASE, sizeof(struct virtq_used_t), (uint8_t*) &virtq_used);

			plic.sclaim |= (1 << 0x03 /*gpio's irq*/);
			csr.sip |= (1 << SUPERVISOR_EXTERNAL_INTERRUPT);

			virtio3_gpio.int_status |= 1;
		}

		if (virtio8_net.queue[1 /*transmitq1*/].notify) {
			struct virtio_queue_t *virtio_queue = &virtio8_net.queue[1];

			virtio_queue->notify = 0;

			struct virtq_avail_t virtq_avail;
			stream_lru_read(virtio_queue->driver_addr - RAM_BASE, sizeof(struct virtq_avail_t), (uint8_t*) &virtq_avail);

			struct virtq_used_t virtq_used;
			stream_lru_read(virtio_queue->dev_addr - RAM_BASE, sizeof(struct virtq_used_t), (uint8_t*) &virtq_used);

			struct virtio_net_hdr_t {
				uint8_t flags;
				uint8_t gso_type;
				uint16_t hdr_len;
				uint16_t gso_size;
				uint16_t csum_start;
				uint16_t csum_offset;
				uint16_t num_buffers;
			};

			while (virtq_used.idx != virtq_avail.idx) {

				struct virtq_desc_t virtq_desc;
				stream_lru_read( virtio_queue->desc_addr + virtq_avail.ring[virtq_used.idx % virtio_queue->num] * sizeof(struct virtq_desc_t) - RAM_BASE, sizeof(struct virtq_desc_t), (uint8_t*) &virtq_desc);

				struct virtio_net_packet_t {
					struct virtio_net_hdr_t hdr;
					uint8_t data[virtq_desc.len - sizeof(struct virtio_net_hdr_t)];
				} virtio_net_packet;

				stream_lru_read(virtq_desc.addr - RAM_BASE, sizeof(struct virtio_net_packet_t), (uint8_t*) &virtio_net_packet);

				/*No network backend on the linux target: packet is dropped.*/

				virtq_used.ring[virtq_used.idx % virtio_queue->num].id = virtq_avail.ring[virtq_used.idx % virtio_queue->num];
				virtq_used.ring[virtq_used.idx % virtio_queue->num].len = virtq_desc.len;

				++virtq_used.idx;
			}
			stream_lru_write(virtio_queue->dev_addr - RAM_BASE, sizeof(struct virtq_used_t), (uint8_t*) &virtq_used);
		}

		/*No network backend on the linux target: virtio-net rx queue never gets fed.*/
		/*Secondary ns16550 uart has no real backend either: guest console is virtio (hvc0).*/

		if (virtio1_console.queue[0 /*receiveq*/].notify && (uart_reading= fread(&uart_buff, sizeof(uint8_t), sizeof(uart_buff), stdin) ) ) {

			struct virtio_queue_t *virtio_queue = &virtio1_console.queue[0];

			virtio_queue->notify = 0;

			struct virtq_avail_t virtq_avail;
			stream_lru_read(virtio_queue->driver_addr - RAM_BASE, sizeof(struct virtq_avail_t), (uint8_t*) &virtq_avail);

			struct virtq_used_t virtq_used;
			stream_lru_read(virtio_queue->dev_addr - RAM_BASE, sizeof(struct virtq_used_t), (uint8_t*) &virtq_used);

			struct virtq_desc_t virtq_desc;
			stream_lru_read( virtio_queue->desc_addr + virtq_avail.ring[virtq_used.idx % virtio_queue->num] * sizeof(struct virtq_desc_t) - RAM_BASE, sizeof(struct virtq_desc_t), (uint8_t*) &virtq_desc);

			stream_lru_write(virtq_desc.addr - RAM_BASE, uart_reading, (uint8_t*) &uart_buff);

			virtq_used.ring[virtq_used.idx % virtio_queue->num].id = virtq_avail.ring[virtq_used.idx % virtio_queue->num];
			virtq_used.ring[virtq_used.idx % virtio_queue->num].len = uart_reading;
			++virtq_used.idx;

			stream_lru_write(virtio_queue->dev_addr - RAM_BASE, sizeof(struct virtq_used_t), (uint8_t*) &virtq_used);

			plic.sclaim |= (1 << 0x01 /*console's irq*/);
			csr.sip |= (1 << SUPERVISOR_EXTERNAL_INTERRUPT);

			virtio1_console.int_status |= 1;
		}

		xSemaphoreGive(vm_lock);
		taskYIELD(); /*idle passes (no notify, no stdin data) would otherwise spin the mutex take/give as fast as possible*/
	}
}

void load_file_to_lru(const char *path, uint32_t addr) {

	uint8_t buff[1024 /*1KB*/];

	FILE *file = fopen(path, "rb");
	assert(file);

	for (;; addr += sizeof(buff)) {

		uint16_t reading = fread(buff, sizeof(uint8_t), sizeof(buff), file);

		if (reading == 0) break;

		stream_lru_write(addr, reading, (uint8_t*) &buff);
	}

	fclose(file);
}

void app_main() {

	{
		memory_file = fopen("../data/memory.bin", "r+b");

		if (!memory_file) {
			/*first run: create memory.bin with the full guest RAM size, zeroed*/

			memory_file = fopen("../data/memory.bin", "w+b");
			assert(memory_file);

			uint8_t zero[LRU_SEC_SIZE] = { 0 };
			for (uint32_t off = 0; off < GUEST_RAM_SIZE; off += sizeof(zero))
				fwrite(zero, 1, sizeof(zero), memory_file);
		}
		window_budget = (8*1024*1024) / LRU_SEC_SIZE;
	}

	csr.misa =
			(1 << 0 /*Atomic extension*/) |
			(1 << 2 /*Compressed extension*/) |
			(1 << 3 /*Double-precision floating-point extension*/) |
//			(1 << 4 /*RV32E base ISA*/) |
//			(1 << 5 /*Single-precision floating-point extension*/) |
//			(1 << 7 /*Hypervisor extension*/) |
			(1 << 8 /*RV32I/64I/128I base ISA*/) |
			(1 << 12 /*Integer Multiply/Divide extension*/) |
//			(1 << 13 /*User-level interrupts supported*/) |
//			(1 << 16 /*Quad-precision floating-point extension*/) |
			(1 << 18 /*Supervisor mode implemented*/) |
			(1 << 20 /*User mode implemented*/) |
			(1 << 30 /*32bit 1[32] 2[64] 3[128]*/);

	{
		*(uint64_t*) &virtio1_console.dev_feat = 0 |
//				(1 << 0 /*VIRTIO_CONSOLE_F_SIZE*/) |
//				(1 << 1 /*VIRTIO_CONSOLE_F_MULTIPORT*/) |
//				(1 << 2 /*VIRTIO_CONSOLE_F_EMERG_WRITE*/) |
				((uint64_t) 1 << 32 /*VIRTIO_F_VERSION_1*/);

//		struct virtio_console_config {
//				uint16_t cols;
//				uint16_t rows;
//				uint32_t max_nr_ports;
//				uint32_t emerg_wr;
//		};
	}

	{
		*(uint64_t*) &virtio2_blk.dev_feat = 0 |
//				(1 << 1 /*VIRTIO_BLK_F_SIZE_MAX*/) |
//				(1 << 2 /*VIRTIO_BLK_F_SEG_MAX*/) |
//				(1 << 4 /*VIRTIO_BLK_F_GEOMETRY*/) |
//				(1 << 5 /*VIRTIO_BLK_F_RO*/) |
//				(1 << 6 /*VIRTIO_BLK_F_BLK_SIZE*/) |
//				(1 << 9 /*VIRTIO_BLK_F_FLUSH*/) |
//				(1 << 10 /*VIRTIO_BLK_F_TOPOLOGY*/) |
//				(1 << 11 /*VIRTIO_BLK_F_CONFIG_WCE*/) |
				((uint64_t) 1 << 32 /*VIRTIO_F_VERSION_1*/);

		struct virtio_blk_config_t {
			uint64_t capacity;
			uint32_t size_max;
			uint32_t seg_max;
			struct virtio_blk_geometry {
				uint16_t cylinders;
				uint8_t heads;
				uint8_t sectors;
			} geometry;
			uint32_t blk_size;
			struct virtio_blk_topology {
				uint8_t physical_block_exp;
				uint8_t alignment_offset;
				uint16_t min_io_size;
				uint32_t opt_io_size;
			} topology;
			uint8_t writeback;
			uint8_t unused0;
			uint16_t num_queues;
			uint32_t max_discard_sectors;
			uint32_t max_discard_seg;
			uint32_t discard_sector_alignment;
			uint32_t max_write_zeroes_sectors;
			uint32_t max_write_zeroes_seg;
			uint8_t write_zeroes_may_unmap;
			uint8_t unused1[3];
			uint32_t max_secure_erase_sectors;
			uint32_t max_secure_erase_seg;
			uint32_t secure_erase_sector_alignment;
		};

		virtio2_blk.config_space = calloc(1, sizeof(struct virtio_blk_config_t));
		assert(virtio2_blk.config_space);

		struct virtio_blk_config_t *virtio_blk_config = (struct virtio_blk_config_t*) virtio2_blk.config_space;

		rootfs.data = fopen("../data/rootfs.ext2", "r+b");
		assert(rootfs.data);

		fseek((FILE*) rootfs.data, 0L, SEEK_END);
		virtio_blk_config->capacity = ftell((FILE*) rootfs.data) / 512;
	}

	{
		*(uint64_t*) &virtio3_gpio.dev_feat = 0 |
//				(1 << 0 /*VIRTIO_GPIO_F_IRQ*/) |
				((uint64_t) 1 << 32 /*VIRTIO_F_VERSION_1*/);

		struct virtio_gpio_config_t {
			uint16_t ngpio;
			uint8_t padding[2];
			uint32_t gpio_names_size;
		};

		virtio3_gpio.config_space = calloc(1, sizeof(struct virtio_gpio_config_t));
		assert(virtio3_gpio.config_space);

		struct virtio_gpio_config_t *virtio_gpio_config = (struct virtio_gpio_config_t*) virtio3_gpio.config_space;

		virtio_gpio_config->ngpio = 2; /*total number of GPIO lines supported*/
	}

	*(uint64_t*) &virtio4_i2c.dev_feat = 0 |
			(1 << 0 /*VIRTIO_I2C_F_ZERO_LENGTH_REQUEST*/) |
			((uint64_t) 1 << 32 /*VIRTIO_F_VERSION_1*/);

	{
		*(uint64_t*) &virtio8_net.dev_feat = 0 |
				((uint32_t) 1 << 3 /*VIRTIO_NET_F_MTU*/) |
				((uint32_t) 1 << 5 /*VIRTIO_NET_F_MAC*/) |
				((uint64_t) 1 << 32 /*VIRTIO_F_VERSION_1*/);

		struct virtio_net_config_t {
			uint8_t mac[6];
			uint16_t status;
			uint16_t max_virtqueue_pairs;
			uint16_t mtu;
			uint32_t speed;
			uint8_t duplex;
			uint8_t rss_max_key_size;
			uint16_t rss_max_indirection_table_length;
			uint32_t supported_hash_types;
		};

		virtio8_net.config_space = calloc(1, sizeof(struct virtio_net_config_t));
		assert(virtio8_net.config_space);

		struct virtio_net_config_t *virtio_net_config = (struct virtio_net_config_t*) virtio8_net.config_space;

		virtio_net_config->mtu = 576;

		virtio_net_config->mac[0] = 0x02;
		virtio_net_config->mac[1] = 0x00;
		virtio_net_config->mac[2] = 0x00;
		virtio_net_config->mac[3] = 0x00;
		virtio_net_config->mac[4] = 0x00;
		virtio_net_config->mac[5] = 0x01;
	}

	{
		low.data = (uint8_t*) malloc(4096 /*4Kb*/);

		FILE *dtb = fopen("../data/riscv_emulator.dtb", "rb");
		assert(dtb);

		fread(low.data, sizeof(uint8_t), 4096, dtb);
		fclose(dtb);
	}

	load_file_to_lru("../data/bbl32.bin", 0x00000000);
	load_file_to_lru("../data/Image", 0x00400000);

	vm_lock = xSemaphoreCreateMutex();
	assert(vm_lock);

	xTaskCreate( cpu_loop, "cpu_loop", 4096, (void*) 0, 1, (void*) 0);
	xTaskCreate( virtio_loop, "virtio_loop", 4096, (void*) 0, 1, (void*) 0);
}