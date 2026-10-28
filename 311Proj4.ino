#define MY_SREG   (*(volatile unsigned char *)0x5F)

#define MY_TCCR2A (*(volatile unsigned char *)0xB0)
#define MY_TCCR2B (*(volatile unsigned char *)0xB1)
#define MY_TIMSK2 (*(volatile unsigned char *)0x70)

#define MY_TCCR1A (*(volatile unsigned char *)0x80)
#define MY_TCCR1B (*(volatile unsigned char *)0x81)
#define MY_TCNT1L (*(volatile unsigned char *)0x84)
#define MY_TCNT1H (*(volatile unsigned char *)0x85)
#define MY_ICR1L  (*(volatile unsigned char *)0x86)
#define MY_ICR1H  (*(volatile unsigned char *)0x87)
#define MY_OCR1AL (*(volatile unsigned char *)0x88)
#define MY_OCR1AH (*(volatile unsigned char *)0x89)

#define MY_DDRB   (*(volatile unsigned char *)0x24)
#define MY_PORTB  (*(volatile unsigned char *)0x25)
#define MY_PIND   (*(volatile unsigned char *)0x29)
#define MY_DDRD   (*(volatile unsigned char *)0x2A)
#define MY_PORTD  (*(volatile unsigned char *)0x2B)

#define MY_SREG_I     7
#define MY_TOIE2      0
#define MY_WGM21      1
#define MY_WGM20      0
#define MY_CS22       2
#define MY_COM1A1     7
#define MY_COM1A0     6
#define MY_WGM11      1
#define MY_WGM13      4
#define MY_WGM12      3
#define MY_CS10       0

#define LED1_BIT      2
#define LED2_BIT      3
#define MOTOR_BIT     1
#define BUTTON_BIT    4

volatile unsigned long my_millis_count = 0;
volatile unsigned char my_fract_count = 0;

extern "C" void __vector_9(void) __attribute__ ((signal, used, externally_visible));
void __vector_9(void) {
  unsigned long m = my_millis_count;
  unsigned char f = my_fract_count;

  m += 1;
  f += 3;
  if (f >= 125) {
    f -= 125;
    m += 1;
  }

  my_millis_count = m;
  my_fract_count = f;
}

void init_mymilli() {
  MY_TCCR2A = (1 << MY_WGM21) | (1 << MY_WGM20);
  MY_TCCR2B = (1 << MY_CS22);
  MY_TIMSK2 = (1 << MY_TOIE2);
  MY_SREG |= (1 << MY_SREG_I);
}

unsigned long mymilli() {
  unsigned long m;
  unsigned char oldSREG = MY_SREG;

  MY_SREG &= ~(1 << MY_SREG_I);
  m = my_millis_count;
  MY_SREG = oldSREG;

  return m;
}

const unsigned int PWM_TOP = 799;

#define FAN_OFF   0
#define FAN_LOW   1
#define FAN_MED   2
#define FAN_HIGH  3
const unsigned char speedDuty[4] = {0, 40, 65, 100};

const unsigned char fanSequence[] = {FAN_OFF, FAN_LOW, FAN_MED, FAN_HIGH, FAN_MED, FAN_LOW};
const unsigned char SEQ_LEN = sizeof(fanSequence) / sizeof(fanSequence[0]);
unsigned char seqIndex = 0;
unsigned char currentSpeed = FAN_OFF;

const unsigned long KICK_MS = 150;
unsigned char kickActive = 0;
unsigned long kickStart = 0;

void writeTimer1Reg16(volatile unsigned char *high, volatile unsigned char *low, unsigned int value) {
  unsigned char oldSREG = MY_SREG;
  MY_SREG &= ~(1 << MY_SREG_I);
  *high = (unsigned char)(value >> 8);
  *low  = (unsigned char)(value & 0xFF);
  MY_SREG = oldSREG;
}

void init_motor_pwm() {
  MY_PORTB &= ~(1 << MOTOR_BIT);
  MY_DDRB  |=  (1 << MOTOR_BIT);

  MY_TCCR1A = 0;
  MY_TCCR1B = 0;
  writeTimer1Reg16(&MY_TCNT1H, &MY_TCNT1L, 0);
  writeTimer1Reg16(&MY_ICR1H,  &MY_ICR1L,  PWM_TOP);
  writeTimer1Reg16(&MY_OCR1AH, &MY_OCR1AL, 0);

  MY_TCCR1A = (1 << MY_WGM11);
  MY_TCCR1B = (1 << MY_WGM13) | (1 << MY_WGM12) | (1 << MY_CS10);
}

void setMotorDuty(unsigned char percent) {
  if (percent == 0) {
    MY_TCCR1A &= ~((1 << MY_COM1A1) | (1 << MY_COM1A0));
    MY_PORTB  &= ~(1 << MOTOR_BIT);
    return;
  }
  if (percent > 100) {
    percent = 100;
  }

  unsigned int ocr = (unsigned int)(((unsigned long)(PWM_TOP + 1) * percent) / 100);
  if (ocr > 0) {
    ocr -= 1;
  }
  writeTimer1Reg16(&MY_OCR1AH, &MY_OCR1AL, ocr);
  MY_TCCR1A |= (1 << MY_COM1A1);
}

void setFanSpeed(unsigned char speed, unsigned long now) {
  if (currentSpeed == FAN_OFF && speed != FAN_OFF) {
    setMotorDuty(100);
    kickActive = 1;
    kickStart = now;
  } else {
    kickActive = 0;
    setMotorDuty(speedDuty[speed]);
  }
  currentSpeed = speed;
}

const unsigned long DEBOUNCE_MS = 30;
unsigned char btnLastRaw = 1;
unsigned char btnStable = 1;
unsigned long btnChangeTime = 0;

void init_button() {
  MY_DDRD  &= ~(1 << BUTTON_BIT);
  MY_PORTD |=  (1 << BUTTON_BIT);
}

const unsigned char ledBits[2] = {LED1_BIT, LED2_BIT};
unsigned long blinkDuration[2] = {0, 0};
unsigned long lastUpdate[2] = {0, 0};
unsigned char currentOut[2] = {0, 0};

void init_leds() {
  for (int i = 0; i < 2; i++) {
    MY_PORTD &= ~(1 << ledBits[i]);
    MY_DDRD  |=  (1 << ledBits[i]);
  }
}

void writeLed(int i, unsigned char on) {
  unsigned char oldSREG = MY_SREG;
  MY_SREG &= ~(1 << MY_SREG_I);
  if (on) {
    MY_PORTD |= (1 << ledBits[i]);
  } else {
    MY_PORTD &= ~(1 << ledBits[i]);
  }
  MY_SREG = oldSREG;
}

char rxBuffer[24];
int rxIndex = 0;
int targetLed = 0;
int promptStage = 0;

void updateSingleLed(int i, unsigned long now) {
  if (blinkDuration[i] > 0 && (now - lastUpdate[i] >= blinkDuration[i])) {
    lastUpdate[i] = now;
    currentOut[i] = !currentOut[i];
    writeLed(i, currentOut[i]);
  }
}

void processCommand(long parsedVal, unsigned long now) {
  if (promptStage == 0) {
    if (parsedVal == 1 || parsedVal == 2) {
      targetLed = parsedVal - 1;
      promptStage = 1;
      Serial.println("What interval (in msec)?");
    } else {
      Serial.println("Invalid selection. What LED? (1 or 2)");
    }
  } else {
    if (parsedVal > 0) {
      unsigned long halfTime = parsedVal / 2;
      if (halfTime == 0) {
        halfTime = 1;
      }

      blinkDuration[targetLed] = halfTime;
      lastUpdate[targetLed] = now;
      currentOut[targetLed] = 0;
      writeLed(targetLed, 0);

      promptStage = 0;
      Serial.println("What LED? (1 or 2)");
    } else {
      Serial.println("Invalid time. What interval (in msec)?");
    }
  }
}

void taskLed1(unsigned long now) {
  updateSingleLed(0, now);
}

void taskLed2(unsigned long now) {
  updateSingleLed(1, now);
}

void taskSerial(unsigned long now) {
  if (Serial.available() > 0) {
    char incoming = Serial.read();

    if (incoming == '\n' || incoming == '\r') {
      if (rxIndex > 0) {
        rxBuffer[rxIndex] = '\0';
        long parsedVal = atol(rxBuffer);
        rxIndex = 0;
        processCommand(parsedVal, now);
      }
    } else if (rxIndex < (int)sizeof(rxBuffer) - 1) {
      rxBuffer[rxIndex++] = incoming;
    }
  }
}

void taskButton(unsigned long now) {
  unsigned char raw = (MY_PIND >> BUTTON_BIT) & 0x01;

  if (raw != btnLastRaw) {
    btnLastRaw = raw;
    btnChangeTime = now;
  }

  if ((now - btnChangeTime) >= DEBOUNCE_MS && raw != btnStable) {
    btnStable = raw;
    if (btnStable == 0) {
      seqIndex = (seqIndex + 1) % SEQ_LEN;
      setFanSpeed(fanSequence[seqIndex], now);
    }
  }
}

void taskMotor(unsigned long now) {
  if (kickActive && (now - kickStart >= KICK_MS)) {
    kickActive = 0;
    setMotorDuty(speedDuty[currentSpeed]);
  }
}

typedef void (*TaskFunc)(unsigned long);
const TaskFunc taskTable[] = {
  taskLed1,
  taskLed2,
  taskSerial,
  taskButton,
  taskMotor
};
const int NUM_TASKS = sizeof(taskTable) / sizeof(taskTable[0]);

void setup() {
  init_mymilli();
  init_motor_pwm();
  init_button();
  btnLastRaw = (MY_PIND >> BUTTON_BIT) & 0x01;
  btnStable = btnLastRaw;

  init_leds();

  Serial.begin(9600);
  Serial.println("What LED? (1 or 2)");
}

void loop() {
  unsigned long now = mymilli();
  for (int i = 0; i < NUM_TASKS; i++) {
    taskTable[i](now);
  }
}
