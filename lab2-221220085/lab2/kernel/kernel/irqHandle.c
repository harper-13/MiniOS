#include "x86.h"
#include "device.h"

extern int displayRow;
extern int displayCol;

extern uint32_t keyBuffer[MAX_KEYBUFFER_SIZE];
extern int bufferHead;
extern int bufferTail;

int tail=0;

void GProtectFaultHandle(struct TrapFrame *tf);

void KeyboardHandle(struct TrapFrame *tf);

void syscallHandle(struct TrapFrame *tf);
void syscallWrite(struct TrapFrame *tf);
void syscallPrint(struct TrapFrame *tf);
void syscallRead(struct TrapFrame *tf);
void syscallGetChar(struct TrapFrame *tf);
void syscallGetStr(struct TrapFrame *tf);


void irqHandle(struct TrapFrame *tf) { // pointer tf = esp
	/*
	 * 中断处理程序
	 */
	/* Reassign segment register */
	asm volatile("movw %%ax, %%ds"::"a"(KSEL(SEG_KDATA)));

	switch(tf->irq) {
		// TODO: 填好中断处理程序的调用
		case -1:
			break;
		case 0xd:
			GProtectFaultHandle(tf);
			break;
		case 0x21:
			KeyboardHandle(tf);
			break;
		case 0x80:
			syscallHandle(tf);
			break;

		default:assert(0);
	}
}

void GProtectFaultHandle(struct TrapFrame *tf){
	assert(0);
	return;
}

void KeyboardHandle(struct TrapFrame *tf){
	uint32_t code = getKeyCode();

	if(code == 0xe){ // 退格符
		//要求只能退格用户键盘输入的字符串，且最多退到当行行首
		if(displayCol>0&&displayCol>tail){
			displayCol--;
			uint16_t data = 0 | (0x0c << 8);
			int pos = (80*displayRow+displayCol)*2;
			asm volatile("movw %0, (%1)"::"r"(data),"r"(pos+0xb8000));
		}
	}else if(code == 0x1c){ // 回车符
		//处理回车情况
		keyBuffer[bufferTail++]='\n';
		displayRow++;
		displayCol=0;
		tail=0;
		if(displayRow==25){
			scrollScreen();
			displayRow=24;
			displayCol=0;
		}
	}else if(code < 0x81){ 
		// TODO: 处理正常的字符
		char character=getChar(code);
		if(character!=0){
			putChar(character);
			keyBuffer[bufferTail++]=character;
			bufferTail%=MAX_KEYBUFFER_SIZE;
			//将字符 character 显示在屏幕的 displayRow 行 displayCol 列
			uint16_t data = character | (0x0c << 8);
			int pos = (80*displayRow+displayCol)*2;
			asm volatile("movw %0, (%1)"::"r"(data),"r"(pos+0xb8000));

			displayCol+=1;
			if(displayCol==80){
				displayCol=0;
				displayRow++;
				if(displayRow==25){
					scrollScreen();
					displayRow=24;
					displayCol=0;
				}
			}
		}
		

	}
	updateCursor(displayRow, displayCol);
	
}

void syscallHandle(struct TrapFrame *tf) {
	switch(tf->eax) { // syscall number
		case 0:
			syscallWrite(tf);
			break; // for SYS_WRITE
		case 1:
			syscallRead(tf);
			break; // for SYS_READ
		default:break;
	}
}

void syscallWrite(struct TrapFrame *tf) {
	switch(tf->ecx) { // file descriptor
		case 0:
			syscallPrint(tf);
			break; // for STD_OUT
		default:break;
	}
}

void syscallPrint(struct TrapFrame *tf) {
	int sel =  USEL(SEG_UDATA);
	char *str = (char*)tf->edx;
	int size = tf->ebx;
	int i = 0;
	int pos = 0;
	char character = 0;
	uint16_t data = 0;
	asm volatile("movw %0, %%es"::"m"(sel));
	for (i = 0; i < size; i++) {
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str+i));
		// TODO: 完成光标的维护和打印到显存
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

	}
	tail=displayCol;
	updateCursor(displayRow, displayCol);
}

void syscallRead(struct TrapFrame *tf){
	switch(tf->ecx){ //file descriptor
		case 0:
			syscallGetChar(tf);
			break; // for STD_IN
		case 1:
			syscallGetStr(tf);
			break; // for STD_STR
		default:break;
	}
}

void syscallGetChar(struct TrapFrame *tf){
	// TODO: 自由实现
	keyBuffer[0]=0;
	keyBuffer[1]=0;
	char c=0;
	
	while(c == 0){
		enableInterrupt();/* 打开外部中断 */
		c = keyBuffer[0];
		putChar(c);
		disableInterrupt();/* 关闭外部中断 */
	}
	tf->eax=c;

	char wait=0;
	while(wait==0){
		enableInterrupt();
		wait = keyBuffer[1];//等待用户按下回车键来确认输入
		disableInterrupt();
	}
	return;
}

void syscallGetStr(struct TrapFrame *tf){
	// TODO: 自由实现
	char* str=(char*)(tf->edx);//str pointer
	int size=(int)(tf->ebx);//str size
	bufferHead=0;
	bufferTail=0;
	//for(int j=0;j<MAX_KEYBUFFER_SIZE;j++)keyBuffer[j]=0;//init
	int j=0;
	while(j<MAX_KEYBUFFER_SIZE){
		keyBuffer[j]=0;
		j++;
	}
	int i=0;
	//该循环会从键盘缓冲区中读取字符，直到遇到换行符 \n 或者达到指定的字符数 size
	char c=0;
	while(c!='\n' && i<size){
		//在内部的 while 循环中，我们等待键盘缓冲区中的字符不再为零（即有输入）。一旦有输入，我们将其存储在 c 变量中，并递增计数器 i。
		while(keyBuffer[i]==0){
			enableInterrupt();
		}
		c=keyBuffer[i];
		i++;
		disableInterrupt();
	}

	int selector=USEL(SEG_UDATA);//初始化一个整数变量 selector，其值与用户数据段相关
	asm volatile("movw %0, %%es"::"m"(selector));//将 selector 的值移动到额外段寄存器（ES）中。
	j=0;
	for(int p=0;p<i-1;p++){
		asm volatile("movl %0, %%es:(%1)"::"r"(keyBuffer[p]),"r"(str+j));//这行将 keyBuffer[p] 的一个字节复制到 str+k
		j++;
	}
	asm volatile("movl $0x00, %%es:(%0)"::"r"(str+i));//在 str 缓冲区的末尾写入一个空终止符（0x00）
	return;
}
