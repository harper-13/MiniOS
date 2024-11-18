#include "x86.h"
#include "device.h"

extern TSS tss;
extern ProcessTable pcb[MAX_PCB_NUM];
extern int current;

extern int displayRow;
extern int displayCol;

void GProtectFaultHandle(struct StackFrame *sf);

void syscallHandle(struct StackFrame *sf);

void syscallWrite(struct StackFrame *sf);
void syscallPrint(struct StackFrame *sf);
void syscallSleep(struct StackFrame *sf);
void syscallFork(struct StackFrame *sf);
void syscallExit(struct StackFrame *sf);
void timerHandle(struct StackFrame *sf);

void irqHandle(struct StackFrame *sf)
{ // pointer sf = esp
	/* Reassign segment register */
	asm volatile("movw %%ax, %%ds" ::"a"(KSEL(SEG_KDATA)));
	/*XXX Save esp to stackTop */
	uint32_t tmpStackTop = pcb[current].stackTop;
	pcb[current].prevStackTop = pcb[current].stackTop;
	pcb[current].stackTop = (uint32_t)sf;

	switch (sf->irq)
	{
	case -1:
		break;
	case 0xd:
		GProtectFaultHandle(sf);
		break;
	case 0x20:
		timerHandle(sf);
		break;
	case 0x80:
		syscallHandle(sf);
		break;
	default:
		assert(0);
	}
	/*XXX Recover stackTop */
	pcb[current].stackTop = tmpStackTop;
}

void GProtectFaultHandle(struct StackFrame *sf)
{
	assert(0);
	return;
}

void timerHandle(struct StackFrame *sf)
{
	// TODO
	for (int i = 0; i < MAX_PCB_NUM; i++){	
		if (pcb[i].state == STATE_BLOCKED){
			pcb[i].sleepTime--;
			if (pcb[i].sleepTime == 0)
				pcb[i].state = STATE_RUNNABLE;
		}
	}
	pcb[current].timeCount++;
	if (pcb[current].timeCount >= MAX_TIME_COUNT){
		int i = (current + 1) % MAX_PCB_NUM;

		while (i != current){
			if (pcb[i].state == STATE_RUNNABLE)
				break;
			i = (i + 1) % MAX_PCB_NUM;
		}

		if (i == current){
			if (pcb[current].state == STATE_RUNNABLE || pcb[current].state == STATE_RUNNING){
				pcb[current].timeCount = 0;
			}
			else
				current = 0;
		}
		else{			
            current = i;
			pcb[current].state = STATE_RUNNING;
		}
	}


	uint32_t tmpStackTop = pcb[current].stackTop;
	pcb[current].stackTop = pcb[current].prevStackTop;
	tss.esp0 = (uint32_t)&(pcb[current].stackTop);
	asm volatile("movl %0, %%esp"::"m"(tmpStackTop)); // switch kernel stack
	asm volatile("popl %gs");
	asm volatile("popl %fs");
	asm volatile("popl %es");
	asm volatile("popl %ds");
	asm volatile("popal");
	asm volatile("addl $8, %esp");
	asm volatile("iret");

}

void syscallHandle(struct StackFrame *sf)
{
	switch (sf->eax)
	{ // syscall number
	case 0:
		syscallWrite(sf);
		break; // for SYS_WRITE
	/*TODO Add Fork,Sleep... */
	case 1:
		syscallFork(sf);
		break; 
	case 3:
		syscallSleep(sf);
		break; 
	case 4:
		syscallExit(sf);
		break; 
	default:
		break;
	}
}

void syscallWrite(struct StackFrame *sf)
{
	switch (sf->ecx)
	{ // file descriptor
	case 0:
		syscallPrint(sf);
		break; // for STD_OUT
	default:
		break;
	}
}

void syscallPrint(struct StackFrame *sf)
{
	int sel = sf->ds; // segment selector for user data, need further modification
	char *str = (char *)sf->edx;
	int size = sf->ebx;
	int i = 0;
	int pos = 0;
	char character = 0;
	uint16_t data = 0;
	asm volatile("movw %0, %%es" ::"m"(sel));
	for (i = 0; i < size; i++)
	{
		asm volatile("movb %%es:(%1), %0" : "=r"(character) : "r"(str + i));
		if (character == '\n')
		{
			displayRow++;
			displayCol = 0;
			if (displayRow == 25)
			{
				displayRow = 24;
				displayCol = 0;
				scrollScreen();
			}
		}
		else
		{
			data = character | (0x0c << 8);
			pos = (80 * displayRow + displayCol) * 2;
			asm volatile("movw %0, (%1)" ::"r"(data), "r"(pos + 0xb8000));
			displayCol++;
			if (displayCol == 80)
			{
				displayRow++;
				displayCol = 0;
				if (displayRow == 25)
				{
					displayRow = 24;
					displayCol = 0;
					scrollScreen();
				}
			}
		}
		// asm volatile("int $0x20"); //XXX Testing irqTimer during syscall
		// asm volatile("int $0x20":::"memory"); //XXX Testing irqTimer during syscall
	}

	updateCursor(displayRow, displayCol);
	// take care of return value
	return;
}



// TODO syscallFork ...
void syscallFork(struct StackFrame *sf) {
    int Pos = -1;
    for(int i = 1; i < MAX_PCB_NUM; ++i){
        if(pcb[i].state == STATE_DEAD){
            Pos = i;
        }
    }
    if(Pos == -1) {
        // FORK FAILED
        pcb[current].regs.eax = -1;
    } else {
        // FORK SUCCESSFUL
        enableInterrupt();
		for (int j = 0; j < 0x100000; j++) {
			*(uint8_t *)(j + (Pos+1)*0x100000) = *(uint8_t *)(j + (current+1)*0x100000);
		}
		disableInterrupt();
        for (int j = 0; j < sizeof(ProcessTable); ++j)
			*((uint8_t *)(&pcb[Pos]) + j) = *((uint8_t *)(&pcb[current]) + j);
		// user process  initProc_reference
        pcb[Pos].stackTop = (uint32_t)&(pcb[Pos].regs);
		pcb[Pos].prevStackTop = (uint32_t)&(pcb[Pos].stackTop);
		pcb[Pos].state = STATE_RUNNABLE;
		pcb[Pos].timeCount = 0;
		pcb[Pos].sleepTime = 0;
		pcb[Pos].pid = Pos;

		pcb[Pos].regs.ss = USEL(2+2*Pos);
		pcb[Pos].regs.cs = USEL(1+2*Pos);
		pcb[Pos].regs.ds = USEL(2+2*Pos);
		pcb[Pos].regs.es = USEL(2+2*Pos);
		pcb[Pos].regs.fs = USEL(2+2*Pos);
		pcb[Pos].regs.gs = USEL(2+2*Pos);
		
        pcb[Pos].regs.eax = 0;
		pcb[current].regs.eax = Pos;

    }
}

void syscallSleep(struct StackFrame *sf){
	int time = sf->ecx;
	if(time >= 0){
		pcb[current].sleepTime = time;
		pcb[current].state = STATE_BLOCKED;
		asm volatile("int $0x20");
	}
}

void syscallExit(struct StackFrame *sf){
	pcb[current].state = STATE_DEAD;
	asm volatile("int $0x20");
}