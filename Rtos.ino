// Board : STM32F401CCU6 Black Pill
// Chip  : STM32F401CCU6
// LCD   : ST7920 128x64 GLCD (Serial Mode)


#include <Arduino.h>
#define LED_PIN PA5
#define GLCD_RS PB12
#define GLCD_RW PB15
#define GLCD_E PB13

bool ledState = false;

typedef void (*TaskFunction_t)(void);

struct TaskControlBlock {
  TaskFunction_t func;
  uint8_t priority;
  uint32_t periodMs;
  uint32_t lastRun;
};

#define MAX_TASKS 5
TaskControlBlock taskList[MAX_TASKS];
int taskCount = 0;

void RTOS_CreateTask(TaskFunction_t func, uint8_t priority, uint32_t periodMs) {
  taskList[taskCount].func = func;
  taskList[taskCount].priority = priority;
  taskList[taskCount].periodMs = periodMs;
  taskList[taskCount].lastRun = 0;
  taskCount++;
}

void RTOS_Scheduler() {
  uint32_t now = millis();
  int bestTask = -1;
  uint8_t bestPrio = 0;
  for (int i = 0; i < taskCount; i++) {
    bool isReady = (taskList[i].periodMs == 0) || ((now - taskList[i].lastRun) >= taskList[i].periodMs);
    if (isReady && taskList[i].priority > bestPrio) {
      bestPrio = taskList[i].priority;
      bestTask = i;
    }
  }
  if (bestTask != -1) {
    taskList[bestTask].lastRun = now;
    taskList[bestTask].func();
  }
}

#define QUEUE_SIZE 32
volatile uint32_t queueBuf[QUEUE_SIZE];
volatile int qHead = 0, qTail = 0, qCount = 0;

bool Queue_Send(uint32_t id) {
  noInterrupts();
  if (qCount < QUEUE_SIZE) {
    queueBuf[qTail] = id;
    qTail = (qTail + 1) % QUEUE_SIZE;
    qCount++;
    interrupts();
    return true;
  }
  interrupts();
  return false;
}

bool Queue_Receive(uint32_t *id) {
  noInterrupts();
  if (qCount > 0) {
    *id = queueBuf[qHead];
    qHead = (qHead + 1) % QUEUE_SIZE;
    qCount--;
    interrupts();
    return true;
  }
  interrupts();
  return false;
}

volatile uint32_t sampleID = 0;
volatile uint32_t missedCount = 0;
volatile uint32_t stressLoad = 3000;
volatile bool serialLocked = false;

bool Mutex_Take() {
  noInterrupts();
  if (!serialLocked) {
    serialLocked = true;
    interrupts();
    return true;
  }
  interrupts();
  return false;
}

void Mutex_Give() {
  noInterrupts();
  serialLocked = false;
  interrupts();
}

void GLCD_SendBit(bool b) {
  digitalWrite(GLCD_RW, b);
  digitalWrite(GLCD_E, HIGH);
  delayMicroseconds(20);
  digitalWrite(GLCD_E, LOW);
  delayMicroseconds(20);
}

void GLCD_SendByte(bool isData, uint8_t data) {
  digitalWrite(GLCD_RS, HIGH);
  for (int i = 0; i < 5; i++) GLCD_SendBit(1);
  GLCD_SendBit(0);
  GLCD_SendBit(isData);
  GLCD_SendBit(0);
  for (int i = 7; i >= 4; i--) GLCD_SendBit((data >> i) & 1);
  for (int i = 0; i < 4; i++) GLCD_SendBit(0);
  for (int i = 3; i >= 0; i--) GLCD_SendBit((data >> i) & 1);
  for (int i = 0; i < 4; i++) GLCD_SendBit(0);
  digitalWrite(GLCD_RS, LOW);
  delay(3);
}

void GLCD_Command(uint8_t cmd) {
  GLCD_SendByte(false, cmd);
}

void GLCD_Data(uint8_t data) {
  GLCD_SendByte(true, data);
}

void GLCD_SetCursor(uint8_t row, uint8_t col) {
  uint8_t addr = 0x80;
  if (row == 0) addr = 0x80 + col;
  if (row == 1) addr = 0x90 + col;
  if (row == 2) addr = 0x88 + col;
  if (row == 3) addr = 0x98 + col;
  GLCD_Command(addr);
}

void GLCD_Print(const char *str) {
  while (*str) {
    GLCD_Data(*str);
    str++;
  }
}

void GLCD_Clear() {
  GLCD_Command(0x01);
  delay(20);
}

void GLCD_Init() {
  pinMode(GLCD_RS, OUTPUT);
  pinMode(GLCD_RW, OUTPUT);
  pinMode(GLCD_E, OUTPUT);
  digitalWrite(GLCD_RS, LOW);
  digitalWrite(GLCD_RW, LOW);
  digitalWrite(GLCD_E, LOW);
  delay(100);
  GLCD_Command(0x30);
  delay(20);
  GLCD_Command(0x30);
  delay(20);
  GLCD_Command(0x0C);
  delay(20);
  GLCD_Command(0x01);
  delay(20);
  GLCD_Command(0x06);
  delay(20);
}

void SamplingTask();
void CommsTask();
void LoggingTask();
void DisplayTask();
void StressTask();

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  Serial1.begin(9600);
  GLCD_Init();
  GLCD_Clear();
  GLCD_SetCursor(0, 0);
  GLCD_Print("RTOS Started");
  GLCD_SetCursor(1, 0);
  GLCD_Print("Waiting...");
  delay(500);
  Serial1.println("=== RTOS Started ===");
  RTOS_CreateTask(SamplingTask, 4, 500);
  RTOS_CreateTask(CommsTask, 3, 10);
  RTOS_CreateTask(LoggingTask, 2, 1000);
  RTOS_CreateTask(DisplayTask, 2, 1000);
  RTOS_CreateTask(StressTask, 1, 0);
}

void loop() {
  RTOS_Scheduler();
}

void SamplingTask() {
  ledState = !ledState;
  digitalWrite(LED_PIN, ledState);
  sampleID++;
  if (!Queue_Send(sampleID)) missedCount++;
}

void CommsTask() {
  uint32_t id;
  while (Queue_Receive(&id)) {
    if (Mutex_Take()) {
      Serial1.print("COMMS: ID=");
      Serial1.print(id);
      Serial1.println(" OK");
      Mutex_Give();
    }
  }
  if (Serial1.available()) {
    char c = Serial1.read();
    if (c == 'H' || c == 'h') {
      stressLoad = 30000;
      if (Mutex_Take()) {
        Serial1.println("COMMS: High Stress ON");
        Mutex_Give();
      }
    }
    if (c == 'L' || c == 'l') {
      stressLoad = 3000;
      if (Mutex_Take()) {
        Serial1.println("COMMS: Low Stress ON");
        Mutex_Give();
      }
    }
  }
}

void LoggingTask() {
  if (Mutex_Take()) {
    Serial1.print("LOG: ID=");
    Serial1.print(sampleID);
    Serial1.print(" Missed=");
    Serial1.print(missedCount);
    Serial1.print(" QueueLeft=");
    Serial1.print(qCount);
    if (missedCount == 0) Serial1.println(" RESULT=PASS");
    else Serial1.println(" RESULT=FAIL");
    Mutex_Give();
  }
}

void DisplayTask() {
  char buf[16];
  GLCD_SetCursor(0, 0);
  GLCD_Print("ID:             ");
  GLCD_SetCursor(0, 4);
  sprintf(buf, "%lu", sampleID);
  GLCD_Print(buf);
  GLCD_SetCursor(1, 0);
  GLCD_Print("Missed:         ");
  GLCD_SetCursor(1, 8);
  sprintf(buf, "%lu", missedCount);
  GLCD_Print(buf);
  GLCD_SetCursor(2, 0);
  if (missedCount == 0) GLCD_Print("RESULT: PASS    ");
  else GLCD_Print("RESULT: FAIL    ");
  GLCD_SetCursor(3, 0);
  GLCD_Print("Queue:          ");
  GLCD_SetCursor(3, 7);
  sprintf(buf, "%d", qCount);
  GLCD_Print(buf);
}

void StressTask() {
  volatile long x = 0;
  for (uint32_t i = 0; i < stressLoad; i++) x += i * i;
  (void)x;
}
