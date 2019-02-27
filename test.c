#include <lpc214x.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

__irq void UART0_Interrupt(void);

void GSM_Begin(void);							
void GSM_Calling(char *);
void GSM_HangCall(void);
void GSM_Response(void);
void GSM_Response_Display(void);
void GSM_Msg_Read(int);
bool GSM_Wait_for_Msg(void);
void GSM_Msg_Display(void);
void GSM_Msg_Delete(unsigned int);
void GSM_Send_Msg(char* , char*);
void GSM_Delete_All_Msg(void);
void GSM_GPRS_Connect(void);
void GSM_LOG(unsigned char *);
	
void SendMessage(unsigned char*);
void clear_recieverbuffer(void);
void clear_message(void);


char buff[160];		/* buffer to store responses and messages */
char dispBuff[160]="Hello";
bool status_flag = false;	/* for checking any new message */
bool status_flag2 = false;	/* for checking any new message */
volatile int buffer_pointer;
volatile int disp_pointer;
char Mobile_no[14];		/* store mobile no. of received message */
char message_received[60];		/* save received message */
int position = 0;	/* save location of current message */


unsigned int status,A0_1;
unsigned char adc_val[5];

void delay_ms(uint16_t j)
{
    uint16_t x,i;
	for(i=0;i<j;i++)
	{
    for(x=0; x<6000; x++);    /* loop to generate 1 milisecond delay with Cclk = 60MHz */
	}
}

void __irq ADC0_ISR0(void)
{
  int ad;
  status=AD0STAT;
  ad=status & 0x000000FF;
  switch(ad)
    {case 0x02:
                A0_1 = AD0DR1 ;            //Store converted data
                break;
      default :
                break;
     }
   if(ad==0x02)
     A0_1 = (A0_1 >>6);    //result-------every time you need to read it it is updated every  time so use it to display on lcd

    delay_ms(10);
    VICVectAddr = 0;    	// Acknowledge Interrupt

 }

void ADC_Init()                 //This function initialises ADC peripheral
{
				VICIntEnable=0x0;
        PINSEL1|= (1<<24);			//Select p0.28 as AD0.1
        AD0CR=(1<<1)|(2<<8)|(1<<21 | 2<<17);
        AD0INTEN=(0x00)|(1<<1);
        status=AD0STAT;

}


//UART0 is used for Display

void UART0_init(void)
{
	PINSEL0 = PINSEL0 | 0x00000005;	/* Enable UART0 Rx0 and Tx0 pins of UART0 */
	U0LCR = 0x83;	/* DLAB = 1, 1 stop bit, 8-bit character length */
	U0DLM = 0x00;	/* For baud rate of 9600 with Pclk = 12MHz */
	U0DLL = 0x61;	/* We get these values of U0DLL and U0DLM from formula */
	U0LCR = 0x03; /* DLAB = 0 */
	U0IER = 0x00000001; /* Enable RDA interrupts */
}

void UART0_TxChar(char ch) /* A function to send a byte on UART0 */
{
	U0IER = 0x00000000; /* Disable RDA interrupts */
	U0THR = ch;
	while( (U0LSR & 0x40) == 0 );	/* Wait till THRE bit becomes 1 which tells that transmission is completed */
	U0IER = 0x00000001; /* Enable RDA interrupts */
}


void UART0_SendString(char* str) /* A function to send string on UART0 */
{
	U0IER = 0x00000000; /* Disable RDA interrupts */
	uint8_t i = 0;
	while( str[i] != '\0' )
	{
		UART0_TxChar(str[i]);
		i++;
	}
	U0IER = 0x00000001; /* Enable RDA interrupts */
}


__irq void UART0_Interrupt(void)
{
	//UART0_SendString("USER input received\r\n");
	dispBuff[disp_pointer] = U0RBR;							/* copy UDR(received value) to buffer */
	disp_pointer++;
	status_flag2 = true;						            /* flag for new message arrival */
	VICVectAddr = 0x00;
}

void clearDispBuff()
{
	memset(dispBuff,'-',160);
}

//Uart1 for GSM Module

void UART1_init(void)
{
	PINSEL0 = 5<<16;	/* Enable UART0 Rx0 and Tx0 pins of UART0 */
	U1LCR = 0x83;	/* DLAB = 1, 1 stop bit, 8-bit character length */
	U1DLM = 0x00;	/* For baud rate of 9600 with Pclk = 12MHz */
	U1DLL = 0x61;	/* We get these values of U0DLL and U0DLM from formula */
	U1LCR = 0x03; /* DLAB = 0 */
	U1IER = 0x00000001; /* Enable RDA interrupts */
}

void UART1_TxChar(char ch) /* A function to send a byte on UART0 */
{
	U1IER = 0x00000000; /* Disable RDA interrupts */
	U1THR = ch;
	while( (U1LSR & 0x40) == 0 );	/* Wait till THRE bit becomes 1 which tells that transmission is completed */
	U1IER = 0x00000001; /* Enable RDA interrupts */
}

void UART1_SendString(char* str) /* A function to send string on UART0 */
{
	U1IER = 0x00000000; /* Disable RDA interrupts */
	uint8_t i = 0;
	while( str[i] != '\0' )
	{
		UART1_TxChar(str[i]);
		i++;
	}
	U1IER = 0x00000001; /* Enable RDA interrupts */
}

__irq void UART1_Interrupt(void)
{
	//UART0_SendString("GSM Input Recieved\r\n");
	buff[buffer_pointer] = U1RBR;							/* copy UDR(received value) to buffer */
	buffer_pointer++;
	status_flag = true;						            /* flag for new message arrival */
	VICVectAddr = 0x00;
}

void interrupt_init(void)
{
	VICVectAddr0 = (unsigned long) UART1_Interrupt;	/* UART0 ISR Address */
	VICVectCntl0 = 0x00000027;	/* Enable UART0 IRQ slot */
	VICVectAddr1 = (unsigned long) UART0_Interrupt;	/* UART0 ISR Address */
	VICVectCntl1 = 0x00000026;	/* Enable UART1 IRQ slot */
	VICIntEnable |= 0x00000040;	/* Enable UART0 & UART1 interrupts */
	VICIntEnable |= 0x00000080;
	VICIntSelect = 0x00000000;	/* UART0 configured as IRQ */
	
	VICVectAddr3=(unsigned long) ADC0_ISR0;	/*ADC0 ISR*/
  VICVectCntl3=0x20|18;		/*Enable ADC IRQ slot*/
  VICIntEnable |= (1<<18);	/*Enable ADC0 interrupts*/
}
double cal_val()
{
	double val;
	val = (3.3*(double)A0_1/1023);
	return val;
}
void disp_adc()
{
	double val;
	char ones, tenth;
	val = cal_val();
	ones = (char)val+0x30;
	tenth = ((char)val*10)%10+0x30;
	sprintf(adc_val, "%c.%c\r\n", ones,tenth);
	UART0_SendString(adc_val);
	UART0_SendString("\r\n");
}

int menu()
{
	unsigned char op;
	clearDispBuff();
	UART0_SendString("01.For checking log\r\n02.For sending temperature sms\r\n03.For calling admin\r\n04.For AT command\r\nEnter your option : ");
	while(1)
	{
	if(strstr(dispBuff, "01"))
	{
		UART0_SendString("Enter \"stop\" to exit\r\n");
		while(1)
		{
			if(strstr(dispBuff,"stop"))
				return 0;
			disp_adc();
			delay_ms(1000);
		}
		clearDispBuff();
		break;
	}
	else if(strstr(dispBuff, "02"))
	{
		UART0_SendString("Sending Message!!!\r\n");
		disp_adc();
		GSM_Send_Msg("+919834572630",adc_val);
		UART0_SendString("Message Sent to admin\r\n");
		clearDispBuff();
		break;
	}
	else if(strstr(dispBuff, "03"))
	{
		UART0_SendString("Calling admin\r\n");
		GSM_Calling("+919834572630");
		UART0_SendString("Enter \"hang\" to end call\r\n");
		while(1)
		{
			if(strstr(dispBuff,"hang"))
			{
				UART1_SendString("ATH\r\n");
				return 0;
			}
		}
		clearDispBuff();
	}
	else if(strstr(dispBuff,"04"))
	{
		memset(dispBuff,0,160);
		disp_pointer = 0;
		UART0_SendString("01.For ATE0\r\n02.For ATD+919834572630;\r\n03.For manufacturer name AT+CGMI\r\n04.For IMSI number(AT+CIMI)\r\n05.Message list AT+CMGL\r\n06.Read sms\r\n");
		while(1)
		{
		if(strstr(dispBuff,"stop"))
		return 0;
		if(strstr(dispBuff,"ATE0"))
		{
			memset(buff,0,160);
			UART1_SendString("ATE0\r\n");
			delay_ms(10);
			UART0_SendString(buff);
			memset(dispBuff,0,160);
			disp_pointer = 0;
		}
		else if(strstr(dispBuff,"ATD"))
		{
			memset(buff,0,160);
			UART1_SendString("ATD+919834572630;\r\n");
			delay_ms(10);
			UART0_SendString(buff);
			
		}
		else if(strstr(dispBuff,"AT+CGMI"))
		{
			memset(buff,0,160);
			UART1_SendString("AT+CGMI\r\n");
			delay_ms(10);
			UART0_SendString(buff);
			
		}
		else if(strstr(dispBuff,"AT+CIMI"))
		{
			memset(buff,0,160);
			UART1_SendString("AT+CIMI\r\n");
			delay_ms(10);
			UART0_SendString(buff);
			
		}
		else if(strstr(dispBuff,"AT+CMGL"))
		{
			memset(buff,0,160);
			UART1_SendString("AT+CMGL\r\n");
			delay_ms(10);
			UART0_SendString(buff);
			
		}
		else if(strstr(dispBuff,"sms"))
		{
			GSM_Wait_for_Msg();
			GSM_Msg_Read(position);
			UART0_SendString(message_received);
			
		}
		
	}
	}
	}
}

int main(void)
{
	buffer_pointer = 0;
	bool is_msg_arrived;
	memset(message_received, 0, 60);
	interrupt_init();
	UART1_init();
	delay_ms(500);
	UART0_init();
  delay_ms(500);
	ADC_Init();
	delay_ms(500);
	UART0_SendString("Welcome UART Initialised!!!\r\n");
	GSM_Begin();	/* check GSM responses and initialize GSM */	
	GSM_Delete_All_Msg();
	UART0_SendString("GSM ready\r\n");
	while (1)
		{	clear_message();
			if(status_flag == true)
				{
						menu();
				}		
		}

}

void GSM_GPRS_Connect(void)
{
	UART1_SendString("AT+CGATT=1\r\n"); // Attach to GPRS
	delay_ms(2000);
	UART1_SendString("AT+SAPBR=1,1\r\n"); // Open a GPRS context
	delay_ms(2000);
	UART1_SendString("AT+SAPBER=2,1\r\n");  // To query the GPRS context
	delay_ms(2000);
}

void GSM_LOG(unsigned char * abc)
{
	  UART1_SendString("AT+HTTPINIT\r\n");                  // Initialize HTTP
    delay_ms(1000);
    UART1_SendString("AT+HTTPPARA=\"URL\",\"http://INSERT_YOUR_SERVER_HERE/add_temp.php?t="); // Send PARA command
    delay_ms(50);
    UART1_SendString(abc);   // Add temp to the url
    delay_ms(50);
    UART1_SendString("\"\r\n");   // close url
    delay_ms(2000);
    UART1_SendString("AT+HTTPPARA=\"CID\",1\r\n");    // End the PARA
    delay_ms(2000); 
    UART1_SendString("AT+HTTPACTION=0\r\n");
    delay_ms(3000);    
    UART1_SendString("AT+HTTPTERM\r\n");
    delay_ms(3000);       
}

void clear_message(void)
{
	memset(message_received,0,strlen(message_received));
	UART0_SendString("Cleared message buffer\r\n");
}
void clear_recieverbuffer(void)
{
	memset(buff,0,strlen(buff));
	buffer_pointer = 0;
	UART0_SendString("Cleared GSM buffer\r\n");
}
void SendMessage(unsigned char* sms)
{
  UART1_SendString("AT+CMGF=1\r\n");    //Sets the GSM Module in Text Mode
  delay_ms(1000);  // Delay of 1000 milli seconds or 1 second
  UART1_SendString("AT+CMGS=\"+918806932096\"\r\n"); // Replace x with mobile number
  delay_ms(200);
	while(1)
	{
		if(buff[buffer_pointer]==0x3e)		/* wait for '>' character*/
		{
			buffer_pointer = 0;
			memset(buff,0,strlen(buff));
			UART1_SendString(sms);		/* send msg to given no. */
			UART1_TxChar(0x1a);		/* send Ctrl+Z then only message will transmit*/
			break;
		}
		buffer_pointer++;
	}
  delay_ms(1000);
}

void GSM_Begin(void)
{
	UART0_SendString("Preparing GSM Module!!!\r\n");
	while(1)
	{
		UART1_SendString("ATE0\r\n");		/* send ATE0 to check module is ready or not */
		delay_ms(500);
		
		if(strstr(buff,"OK"))
		{
			UART0_SendString("OK\r\n");
			GSM_Response();		/* get Response */
			memset(buff,0,160);
			break;
		}
		else
		{
			//UART1_SendString("Error");
		}
	}
	delay_ms(1000);

	//UART1_SendString("Text Mode");
	UART1_SendString("AT+CMGF=1\r\n");	/* select message format as text */
	GSM_Response();
	delay_ms(1000);
}

void GSM_Msg_Delete(unsigned int position)
{
	buffer_pointer=0;
	char delete_cmd[20];
	sprintf(delete_cmd,"AT+CMGD=%d\r\n",position);	/* delete message at specified position */
	UART1_SendString(delete_cmd);
}

void GSM_Delete_All_Msg(void)
{
	UART1_SendString("AT+CMGDA=\"DEL ALL\"\r\n");	/* delete all messages of SIM */	
	UART0_SendString("INBOX cleared!!!\r\n");
}

bool GSM_Wait_for_Msg(void)
{
	char msg_location[4];
	int i;
	delay_ms(500);
	buffer_pointer=0;

	while(1)
	{
		if(buff[buffer_pointer]=='\r' || buff[buffer_pointer]== '\n')	/*eliminate "\r \n" which is start of string */
		{
			buffer_pointer++;
		}
		else
			break;
	}
		
	if(strstr(buff,"CMTI:"))		/* "CMTI:" to check if any new message received */
	{
		while(buff[buffer_pointer]!= ',')
		{
			buffer_pointer++;
		}
		buffer_pointer++;
		
		i=0;
		while(buff[buffer_pointer]!= '\r')
		{
			msg_location[i]=buff[buffer_pointer];		/* copy location of received message where it is stored */
			buffer_pointer++;
			i++;
		}

		/* convert string of position to integer value */
		position = atoi(msg_location);
		
		memset(buff,0,strlen(buff));
		buffer_pointer=0;

		return true;
	}
	else
	{
		return false;
	}
}

void GSM_Send_Msg(char *num,char *sms)
{
	char sms_buffer[35];
	buffer_pointer=0;
	sprintf(sms_buffer,"AT+CMGS=\"%s\"\r\n",num);
	UART1_SendString(sms_buffer);		/*send command AT+CMGS="Mobile No."\r */
	delay_ms(200);
	while(1)
	{
		if(buff[buffer_pointer]==0x3e)		/* wait for '>' character*/
		{
			buffer_pointer = 0;
			memset(buff,0,strlen(buff));
			UART1_SendString(sms);		/* send msg to given no. */
			UART1_TxChar(0x1a);		/* send Ctrl+Z then only message will transmit*/
			break;
		}
		buffer_pointer++;
	}
	delay_ms(300);
	buffer_pointer = 0;
	memset(buff,0,strlen(buff));
	memset(sms_buffer,0,strlen(sms_buffer));
}

void GSM_Calling(char *Mob_no)
{
	char call[20];
	sprintf(call,"ATD%s;\r\n",Mob_no);		
	UART1_SendString(call);		/* send command ATD<Mobile_No>; for calling*/
}

void GSM_HangCall(void)
{
	UART1_SendString("ATH\r\n");		/*send command ATH\r to hang call*/
	
}

void GSM_Response(void)
{
	unsigned int timeout=0;
	int CRLF_Found=0;
	char CRLF_buff[2];
	int Response_Length=0;
	while(1)
	{
		if(timeout>=60000)		/*if timeout occur then return */
		return;
		Response_Length = strlen(buff);
		if(Response_Length)
		{
			delay_ms(1);
			timeout++;
			if(Response_Length==strlen(buff))
			{
				for(int i=0;i<Response_Length;i++)
				{
					memmove(CRLF_buff,CRLF_buff+1,1);
					CRLF_buff[1]=buff[i];
					if(strncmp(CRLF_buff,"\r\n",2))
					{
						if(CRLF_Found++==2)		/* search for \r\n in string */
						{
							GSM_Response_Display();		/* display response */
							return;
						}
					}

				}
				CRLF_Found = 0;

			}
			
		}
		delay_ms(1);
		timeout++;
	}
	//status_flag = false;
}

void GSM_Response_Display(void)
{
	buffer_pointer = 0;
	while(1)
	{
		if(buff[buffer_pointer]== '\r' || buff[buffer_pointer]== '\n')		/* search for \r\n in string */
		{
			buffer_pointer++;
		}
		else
			break;
	}
	

	while(buff[buffer_pointer]!='\r')		/* display response till "\r" */
	{
		UART1_TxChar(buff[buffer_pointer]);								
		buffer_pointer++;
	}
	buffer_pointer=0;
	memset(buff,0,strlen(buff));
}

void GSM_Msg_Read(int position)
{
	char read_cmd[10];
	memset(read_cmd, 0, strlen(read_cmd));
	sprintf(read_cmd,"AT+CMGR=%d\r\n",position);
	UART1_SendString(read_cmd);		/* read message at specified location/position */
	GSM_Msg_Display();		/* display message */
}

void GSM_Msg_Display(void)
{
	delay_ms(500);
	if(!(strstr(buff,"+CMGR")))		/*check for +CMGR response */
	{
		UART1_SendString("No message");
	}
	else
	{
		buffer_pointer = 0;
		
		while(1)
		{
			if(buff[buffer_pointer]=='\r' || buff[buffer_pointer]== '\n')		/*wait till \r\n not over*/
			{
				buffer_pointer++;
			}
			else
			break;
		}
		
		/* search for 1st ',' to get mobile no.*/
		while(buff[buffer_pointer]!=',')
		{
			buffer_pointer++;
		}
		buffer_pointer = buffer_pointer+2;

		/* extract mobile no. of message sender */
		for(int i=0;i<=12;i++)
		{
			Mobile_no[i] = buff[buffer_pointer];
			buffer_pointer++;
		}
		
		do
		{
			buffer_pointer++;
		}while(buff[buffer_pointer-1]!= '\n');
		
		int i=0;

		/* display and save message */
		while(buff[buffer_pointer]!= '\r' && i<31)
		{
				UART1_TxChar(buff[buffer_pointer]);
				message_received[i]=buff[buffer_pointer];
				
				buffer_pointer++;
				i++;
		}
		
		buffer_pointer = 0;
		memset(buff,0,strlen(buff));
	}
	status_flag = false;
}