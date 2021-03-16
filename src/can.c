#define TESTE
#define true 1
#define false 0
#define freio_pin LATB4_bit // Pino da luz de freio
#define display LATC        // Reg do display
#define dig_uni LATA4_bit   // Controle display
#define dig_dez LATA5_bit   // Controle display
#define liga 1
#define desliga 0
typedef unsigned char boo;

// Variáveis globais auxiliares
static unsigned long tmr = 0, tx_tmr = 0, rx_tmr = 0;
static boo check = true;
static boo comparador = true;
static boo timer0_ativo = false;

// Variáveis de configuração da CAN
static unsigned char Can_Init_Flags, Can_Send_Flags, Can_Rcv_Flags;
static char tx_can_data[8];
static long id_can_data;
static char SJW, Phase_Seg1, Phase_Seg2, Prop_Seg, BRP;

//RX
static unsigned char Rx_Data_Len;
static char Msg_Rcvd;
static long Rx_ID;
static char rx_can_data[8];

// Funções de interrupção
void interrupt(void);
void interrupt_low(void);

void main()
{
    // Entradas
    int int_ac1 = 0, int_ac2 = 0, int_freio = 0;

    // Ponteiros para as posições do ac1 e freio no campo de dados da CAN
    int *pos_freio = (int *)(&tx_can_data[1]);
    int *pos_ac1 = (int *)(&tx_can_data[3]);

    // Var auxiliares
    char i = 0;
    double div = 0.0;
    int max = 0, min = 0;
    boo aux = 1;

    // Display
    unsigned int digs[] = {0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92,
                           0x82, 0xF8, 0x80, 0x90};
    unsigned char dez = 0, uni = 0;

    // ========================= Config. regs. I/O =================================

    ADCON0 = 0x09; // Liga o conversor A/D e seleciona os pinos.
    ADCON1 = 0x0C; // Configura os pinos AN0, AN1 e AN2 como analógicos.

    // Portas de entrada
    TRISA0_bit = 0x01;
    TRISA1_bit = 0x01;
    TRISA2_bit = 0x01;

    // Portas de saida
    TRISA4_bit = 0x00; // Controle display, dig_uni
    TRISA5_bit = 0x00; // Controle display, dig_dez
    TRISB4_bit = 0x00; // Luz de freio
    TRISC = 0x00;      // Saida display

    display = 0x01;
    dig_uni = desliga;
    dig_dez = desliga;

    freio_pin = 0x00;

    //========================== Config CAN TX/RX ==================================

    // Parâmetros CAN (1Mbps, 8MHz -> 32MHz com PLL)
    Can_Init_Flags = 0;
    Can_Send_Flags = 0;
    Can_Rcv_Flags = 0;

    // Valores calculados utilizando calculadora CAN
    SJW = 1;
    BRP = 2;
    Phase_Seg1 = 1;
    Phase_Seg2 = 2;
    Prop_Seg = 4;
    id_can_data = 5;

    Can_Init_Flags = _CAN_CONFIG_SAMPLE_ONCE &
                     _CAN_CONFIG_PHSEG2_PRG_ON &
                     _CAN_CONFIG_XTD_MSG &
                     _CAN_CONFIG_DBL_BUFFER_ON &
                     _CAN_CONFIG_VALID_XTD_MSG &
                     _CAN_CONFIG_LINE_FILTER_OFF;

    Can_Send_Flags = _CAN_TX_PRIORITY_0 &
                     _CAN_TX_XTD_FRAME &
                     _CAN_TX_NO_RTR_FRAME;

    CANInitialize(SJW, BRP, Phase_Seg1, Phase_Seg2, Prop_Seg, Can_Init_Flags);
    CANSetOperationMode(_CAN_MODE_CONFIG, 0xFF);
    CANSetMask(_CAN_MASK_B1, -1, _CAN_CONFIG_XTD_MSG);
    CANSetMask(_CAN_MASK_B2, -1, _CAN_CONFIG_XTD_MSG);
    CANSetFilter(_CAN_FILTER_B1_F1, 10, _CAN_CONFIG_XTD_MSG);
    CANSetOperationMode(_CAN_MODE_NORMAL, 0xFF);

    //-========================= CONFIGURAÇÃO TIMERS ===============================

    /*
- Cálculo dos parâmetros do TIMER para tempo de overflow:
- FREQUÊNCIA DO CICLO DE MAQUINA = clock/4 -> PERÍODO = 4/clock
- TEMPO_OVERFLOW = (2^(8 ou 16) - (TMRH:TMRL)) * período do CM * prescaler
- TMRH:TMRL = 2^(8 ou 16) - (TEMPO_OVERFLOW / (período do CM * prescaler))
*/
    // Reg RCON
    IPEN_bit = 0x01; // Habilita prioridade nas interrupções. pag.129

    /*
  ====  TIMER0 Config 

- Com clock 32MHz e overflow em 100 ms, temos:(utilizando os dois regs)
- PERIODO = 0.125us, (TMR0H:TMR0L) = 40536 e prescaler = 32.
*/

    INTCON = 0xE0; // Habilita interrupção global e do TIMER0.

    // Reg INTCON2
    TMR0IP_bit = 0x01; // Prioridade alta na interrupção do TIMER0.

    TMR0ON_bit = 0x00; // TIMER0 inicialmente desativado
    T08BIT_bit = 0x00; // 16-bit timer
    T0CS_bit = 0x00;   // Utilizar o clock do sistema
    PSA_bit = 0x00;    // Ativa o prescaler

    // Prescaler de 32
    T0PS2_bit = 0x01;
    T0PS1_bit = 0x00;
    T0PS0_bit = 0x00;

    // TIMER0 inicia com o valor de 40536
    TMR0L = 0x58;
    TMR0H = 0x9E;

    /*
  =====  TIMER1 Config pag.153

- Com clock 32MHz e overflow em 5ms, temos:(utilizando os dois regs)
- PERIODO = 0.125us, (TMR0H:TMR0L) = 25536 e prescaler = 1.
*/
    // Reg PIE1
    TMR1IE_bit = 0x01; // Habilita interrupção do TIMER1.

    // REG IPR1                                                                 // Prioridade baixa na interrupção do TIMER1. pag.126
    TMR1IP_bit = 0x00;

    // Config timer1 16 bits e sem prescaler
    RD16_bit = 0x01;
    T1RUN_bit = 0x00;
    T1CKPS0_bit = 0x00;
    T1CKPS1_bit = 0x00;
    T1OSCEN_bit = 0x00;
    T1SYNC_bit = 0x00;
    TMR1CS_bit = 0x00;

    // Inicializa com 25536
    TMR1L = 0xC0;
    TMR1H = 0x63;

    TMR1ON_bit = 0x01; // Liga TIMER1

    //==============================================================================

    while (true)
    {
        // Entrada dos dados
        for (i = 0; i < 10; i++)
        {                           // Lê os sinais do TPS1, TPS2 e freio 10 vezes
                                    // e fica com a média para uma maior precisão
            int_ac1 += ADC_Read(0); // na leitura.
            int_ac2 += ADC_Read(1);
            int_freio += ADC_Read(2);
        }

        int_ac1 = int_ac1 / 10;
        int_ac2 = int_ac2 / 10;
        int_freio = int_freio / 10;

        if (int_freio > 15)
        {
            freio_pin = 0x01;
        }
        else
        {
            freio_pin = 0x00;
        }

        int_ac2 = 1024 - int_ac2;

        max = int_ac1;
        min = int_ac2;
        if (min > max)
        {
            max = min;
            min = int_ac1;
        }

        // COMPARADOR
        div = (float)min / max;

        if (div < 0.9)
        { // Se diferença de 10% entre os dois sinais
            if (comparador == true && timer0_ativo == false)
            {
                timer0_ativo = true;
                TMR0ON_bit = 0x01; // Ativa o TIMER0
            }
        }
        else if (timer0_ativo == true)
        {
            TMR0ON_bit = 0x00; // Desliga TIMER0
            TMR0L = 0x58;      // Reinicia reg low
            TMR0H = 0x9E;      // Reinicia reg high
            timer0_ativo = false;
        }
        else if (comparador == false)
        {
            comparador = true;
        }

        // CHECK
        if (check == true && int_freio > 15 && int_ac1 > 640)
        {
            check = false;
        }
        else if (check == false && int_ac1 <= 537)
        {
            check = true;
        }

        tx_can_data[0] = (check && comparador);
        *(int *)pos_freio = int_freio;
        *(int *)pos_ac1 = int_ac1;

        // Envia e recebe sinais da CAN a cada 5ms
        if (tmr > tx_tmr)
        {

            tx_tmr = tmr;
            CANWrite(id_can_data, tx_can_data, 5, Can_Send_Flags);

            if (aux == 1)
            {
                dig_uni = liga;
                dig_dez = desliga;
                display = digs[uni];
                aux = 0;
            }
            else
            {
                dig_dez = liga;
                dig_uni = desliga;
                display = digs[dez];
                aux = 1;
            }
        }

        if ((tmr - rx_tmr) > 20)
        { // Lê e envia a velocidade a cada 100ms

            rx_tmr = tmr;
            Msg_Rcvd = CANRead(&Rx_ID, rx_can_data, &Rx_Data_Len, &Can_Rcv_Flags);

            if (Msg_Rcvd && (Rx_ID == 10))
            {
                dez = rx_can_data[0] / 10;
                uni = rx_can_data[0] % 10;
            }
        }
    } // end while
} // end main

void interrupt()
{

    // Verifica a flag de interrupção do TIMER0.
    if (TMR0IF_bit)
    {

        TMR0IF_bit = 0x00; // Zera a flag de interrupção
        TMR0ON_bit = 0x00; // Desliga o TIMER0

        // Reinicia os registradores do TIMER0
        TMR0L = 0x58; // Reinicia reg low
        TMR0H = 0x9E; // Reinicia reg high

        comparador = false;
        timer0_ativo = false;
    }
}

void interrupt_low()
{
    if (TMR1IF_bit)
    {

        TMR1IF_bit = 0x00;
        TMR1L = 0xC0;
        TMR1H = 0x63;
        tmr = tmr + 1;
    }
}