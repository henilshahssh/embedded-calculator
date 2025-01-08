
#include <LiquidCrystal.h>
#include <Keypad.h>
#include <EEPROM.h>

#include "TimerOne.h"
#include "TimerThree.h"

#define PIN_TRIG 6
#define PIN_ECHO 5

LiquidCrystal lcd(12, 11, 10, 9, 8, 7);

const byte ROWS = 4;
const byte COLS = 4;
const byte CLEARBTN = 37;
const float BETA = 3950;

String currentKey = "";
bool ledIsOn = true;
char lastKey = 0;
int calculateInputLength = 0;
int lastValidIndex = 0;
bool showMenu = true;
bool showCalc = false;
bool memFunctionUsed = false;

String calculateInput[100] = { "Null" }; // Set default value as Null
byte rowPins[ROWS] = {A7, A6, A5, A4};
byte colPins[COLS] = {A3, A2, A1, A0};

char keys[ROWS][COLS] = {
  {'1', '2', '3', 'x'},
  {'4', '5', '6', '+'},
  {'7', '8', '9', '-'},
  {'T', '0', '.', '='}
};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

const uint8_t no_key = 0U;

const uint8_t keys_val[2][4] = {
  {'/','N','%','R'},
  {'F','A','S','C'},
};

volatile uint8_t key_main_access;
volatile uint8_t key_state;
volatile uint8_t old_col;
volatile uint8_t new_col, new_row;
volatile uint8_t main_read;
volatile uint8_t main_interrupted;
volatile uint8_t key1, key2;
volatile uint32_t old_time, new_time, diff = 0U;

const int row_1 = 19;
const int row_2 = 18;

const int col_1 = 41;
const int col_2 = 43;
const int col_3 = 45;
const int col_4 = 47;

void setup() {
  // Setup pins and lcd
  lcd.begin(16, 2);
  lcd.setCursor(1, 0);
  pinMode(CLEARBTN, INPUT);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  attachInterrupt(digitalPinToInterrupt(2), clearBtnISR, FALLING);

  // Setup serial, set default password and check for validity of entered password
  Serial.begin(9600);
  pushButtonMatrixSetup();

  String defPass = "EEE20003";
  // We decided to use address from 0 to store default password
  for(int i=0; i<defPass.length(); i++){
    EEPROM.write(i, defPass[i]);
  }
  
  Serial.println("Please enter password to continue: ");

  while(!Serial.available()){}
  String input = Serial.readString();

  for(int i=0; i<input.length()-1; i++){
    if(EEPROM.read(i) != input[i]){
      Serial.println("Incorrect password!");
      showMenu = false;
    }
  }

  Serial.print("You entered: ");
  Serial.println(input);

  // If password is correct show the menu with options
  if(showMenu){
    Serial.setTimeout(10000);
    Serial.println("Please select one of the options below by entering their corresponding number: ");
    Serial.println("1. Change intial text to be display in LCD.");
    Serial.println("2. Display initial data from the sensor to be stored in the EEPROM.");
    Serial.println("3. Interrupt-driven programming(calculator and sensor readings).");

    while(!Serial.available()){}
    int option = Serial.parseInt();
    
    Serial.print("You entered: ");
    Serial.println(option);
    Serial.println();
    
    if(option == 1) changeInitialText();

    if(option == 2){
      // Reason why we have been able to use timer3 at two places
      // is because since options can only be selected once in setup
      // option 2 does not interfere with option 3.
      Timer3.initialize(500000);
      Timer3.attachInterrupt(showAndStoreSensorReading);
    }

    if(option == 3) {
      showCalc = true;
      Timer3.initialize(1000000);
      Timer3.attachInterrupt(sensorReadings);
    }
  }
}

// Display sensor readings to lcd and store them in EEPROM
void showAndStoreSensorReading(){
  lcd.clear();
  lcd.setCursor(1, 0);

  String tempReading = String(temperatureSensorReading()) + " C";
  for(int i=0; i<tempReading.length(); i++){
    lcd.print(tempReading[i]);
    // We decided to use address from 10 to store tempReading
    EEPROM.write(10+i, tempReading[i]);
  }

  lcd.setCursor(1, 1);
  String distanceReading = String(distanceSensorReading()) + " cm";
  for(int i=0; i<distanceReading.length(); i++){
    lcd.print(distanceReading[i]);
    // We decided to use address from 20 to store distanceReading
    EEPROM.write(20+i, distanceReading[i]);
  }
}

// Clear everything when clear button is pressed using hardware interrupt
void clearBtnISR(){
  // Reset lcd and all data
  lcd.clear();
  lcd.setCursor(1, 0);
  for(int i=0; i<calculateInputLength; i++){
    calculateInput[i] = "Null";
  }
  calculateInputLength = 0;
  lastKey = 0;
  currentKey = "";
}

// Change initial text of lcd based on given input 
void changeInitialText(){
  Serial.println("Please enter text to display on LCD: ");
  Serial.setTimeout(2000);
  
  while(!Serial.available()){}
  String input = Serial.readString();

  Serial.print("You entered: ");
  Serial.println(input);

  lcd.clear();
  for(int i=0; i<input.length()-1; i++){
    lcd.write(input[i]);
    // We decided to use address from 30 to store text
    EEPROM.write(30+i, input[i]);
  }
}

void loop() {
  // Only turn lcd and keypad on if calculator is selected from options
  if(showCalc){
    cursorFlashing();

    char key = keypad.getKey();
    if (key) {
      process(key);
    }
  }
}

// Perform negation(+/-)
void negation(){
  if(currentKey != ""){
    calculateInput[calculateInputLength] = currentKey;
    calculateInputLength++;
  }

  if(calculateInputLength == 0 || calculateInputLength == 1){
    calculateInput[0] = String(0 - calculateInput[0].toDouble());
    currentKey = "";
    lcd.clear();
    lcd.setCursor(1, 0);
    lcd.print(calculateInput[0]);
  }
}

// Perform square root
void squareRoot(){
  if(currentKey != ""){
    calculateInput[calculateInputLength] = currentKey;
    calculateInputLength++;
  }

  if(calculateInputLength == 0 || calculateInputLength == 1){
    calculateInput[0] = String(sqrt(calculateInput[0].toDouble()));
    currentKey = "";
    lcd.clear();
    lcd.setCursor(1, 0);
    lcd.print(calculateInput[0]);
  }
}

// Display blinking cursor
void cursorFlashing(){
  if(ledIsOn){
    lcd.cursor();
    delay(5);
    lcd.noCursor();
    delay(5);
  }
}

// Calculate and returns result of two numbers based on the operator passed
double performOperation(char operatorKey, double leftSide, double rightSide) {
  switch (operatorKey) {
    case 'x': return leftSide * rightSide;
    case '+': return leftSide + rightSide;
    case '-': return leftSide - rightSide;
    case '%': return (int)leftSide % (int)rightSide;
    case '/': return leftSide / rightSide;
    default : return;
  }
}

// Process the key entered
void process(char key){

  // After memory functions(M+, M-) are used, display the output 
  // until a new key is entered
  if(memFunctionUsed){
    lcd.clear();
    lcd.setCursor(1, 0);
    memFunctionUsed = false;
  }

  //Prevent adding operator before a value unless its a minus sign at the very start
  if(lastKey == 0 && (key == 'x' || key == '+') && calculateInputLength == 0){
    return;
  }

  //Prevent consecutive minus
  if(lastKey == '-' && key == '-'){
    return;
  }

  //Prevent consecutive operators
  if((lastKey == 'x' || lastKey == '+' || lastKey == '-') && (key == 'x' || key == '+')){
    return;
  }

  // Allow minus after certain operator(e.g., 5 x -5)
  if((lastKey == 'x' || lastKey == '+' || lastKey == 0) && key == '-'){
    lastKey = key;
    currentKey += String(key);
    lcd.print(key);
    return;
  }

  lastKey = key;

  // Turn it on/off
  if(key == 'T'){
    if(ledIsOn){
      lcd.noDisplay();
      lcd.noCursor();
    }else{
      lcd.display();
      lcd.cursor();
    }
    ledIsOn = !ledIsOn;
    return;
  }

  // Preventing adding fullstop if it already exists
  if (key == '.' && currentKey.indexOf('.') >= 0) {
    return;
  }

  switch(key){
    case 'x':
    case '+':
    case '-':
    case '/':
    case '%':

      // To store number input
      if(currentKey != ""){
        calculateInput[calculateInputLength] = currentKey;
        calculateInputLength++;
      }

      // To store operator input
      calculateInput[calculateInputLength] = key;
      calculateInputLength++;

      currentKey = "";
      
      lcd.print(key);
      return;
    
    case '=':
      
      calculateInput[calculateInputLength] = currentKey;
      calculateInputLength++;

      // "For loop" for edge cases: 45-6+7 resulting in 45+1 instead of 47 
      // This happens because function multiply, add, minus calculate all times/plus/minus once
      // so if there is a sign change, -6+7=+1 like minus becoming a plus then function add
      // needs to run again.
      for(int i=0; i<2; i++){
        // According to BODMAS
        mod();
        divide();
        multiply();
        add();
        minus();
      }

      // Reset everything and display result
      currentKey = "";
      lcd.clear();
      lcd.setCursor(1, 0);
      lcd.print(calculateInput[0]);
      calculateInputLength = 1;
      return;
  }

  currentKey += String(key);
  lcd.print(key);
}

// Perform all mod/remainder at once
void mod(){
  lastValidIndex = 0;

  // Only do mod if it is a valid number
  for(int i=0; i<calculateInputLength; i++){
    if(calculateInput[i] != "Null"){

      // lastValidIndex is where the result from mod will be stored
      if(calculateInput[i+1] == "%"){
        lastValidIndex = i;
      }

      if(calculateInput[i] == "%"){
        // Doing mod using function performOperation
        calculateInput[lastValidIndex] = performOperation('%', calculateInput[lastValidIndex].toDouble(), calculateInput[i+1].toDouble());

        // Reset these values to Null since we have done the mod of these numbers
        calculateInput[i] = "Null";
        calculateInput[i+1] = "Null";
      }
    }
  }
}

// Perform all division at once
void divide(){
  lastValidIndex = 0;
  
  // Only do division if it is a valid number
  for(int i=0; i<calculateInputLength; i++){
    if(calculateInput[i] != "Null"){

      // lastValidIndex is where the result from division will be stored
      if(calculateInput[i+1] == "/"){
        lastValidIndex = i;
      }

      if(calculateInput[i] == "/"){
        // Doing division using function performOperation
        calculateInput[lastValidIndex] = performOperation('/', calculateInput[lastValidIndex].toDouble(), calculateInput[i+1].toDouble());

        // Reset these values to Null since we have done the division of these numbers
        calculateInput[i] = "Null";
        calculateInput[i+1] = "Null";
      }
    }
  }
}

// Perform all multiplication at once
void multiply(){
  lastValidIndex = 0;

  // Only do multiplication if it is a valid number
  for(int i=0; i<calculateInputLength; i++){
    if(calculateInput[i] != "Null"){

      // lastValidIndex is where the result from multiplication will be stored
      if(calculateInput[i+1] == "x"){
        lastValidIndex = i;
      }

      if(calculateInput[i] == "x"){
        // Doing multiplication using function performOperation
        calculateInput[lastValidIndex] = performOperation('x', calculateInput[lastValidIndex].toDouble(), calculateInput[i+1].toDouble());

        // Reset these values to Null since we have done the multiplication of these numbers
        calculateInput[i] = "Null";
        calculateInput[i+1] = "Null";
      }
    }
  }
}

// Perform all additions at once
void add(){
  lastValidIndex = 0;

  for(int i=0; i<calculateInputLength; i++){
    // Only do addition if it is a valid number
    if(calculateInput[i] != "Null"){

      // lastValidIndex is where the result from addition will be stored
      if(calculateInput[i+1] == "+"){
        lastValidIndex = i;
      }

      if(calculateInput[i] == "+"){
        if(calculateInput[lastValidIndex-1] == "-"){
          // If lastValidIndex value starts with minus and if that value is less than 
          // value on right side of plus then change - sign to +. E.g., -6 + 7 = +1
          if(calculateInput[lastValidIndex].toDouble() < calculateInput[i+1].toDouble()){
            calculateInput[lastValidIndex-1] = "+";
          }
          calculateInput[lastValidIndex] = performOperation('+', 0 - calculateInput[lastValidIndex].toDouble(), calculateInput[i+1].toDouble());
        }

        // Doing addition using function performOperation
        else{
          calculateInput[lastValidIndex] = performOperation('+', calculateInput[lastValidIndex].toDouble(), calculateInput[i+1].toDouble());
        }

        // Reset these values to Null since we have done the addition of these numbers
        calculateInput[i] = "Null";
        calculateInput[i+1] = "Null";
      }
    }
  }
}

// Perform all substraction at once
void minus(){
  lastValidIndex = 0;

  // Only do substraction if it is a valid number
  for(int i=0; i<calculateInputLength; i++){
    if(calculateInput[i] != "Null"){

      // lastValidIndex is where the result from substraction will be stored
      if(calculateInput[i+1] == "-"){
        lastValidIndex = i;
      }

      if(calculateInput[i] == "-"){
        // Doing substraction using function performOperation
        calculateInput[lastValidIndex] = performOperation('-', calculateInput[lastValidIndex].toDouble(), calculateInput[i+1].toDouble());

        // Reset these values to Null since we have done the substraction of these numbers
        calculateInput[i] = "Null";
        calculateInput[i+1] = "Null";
      }
    }
    
  }
}

// Read data from given memory address
double memReadFrom(int readFrom){
  String readOutput = "";

  // 20 length is enough to read any stored value by memWriteAt
  for(int i=readFrom; i<readFrom+20; i++){
    if(EEPROM.read(i) == 255) break;

    readOutput = readOutput + (char)EEPROM.read(i);
  }
  
  if(readOutput == ""){
    return 0;
  }

  return readOutput.toDouble();
}

// Write data at given memory address
void memWriteAt(double data, int writeAt){
  memClear(writeAt);

  for(int i=0; i<String(data).length(); i++){
    EEPROM.write(writeAt+i, String(data)[i]);
  }
}

// Clear memory based on given address
void memClear(int clearFrom){
  // 20 length is enough to clear any stored value by memWriteAt
  for(int i=clearFrom; i<clearFrom+20; i++){
    EEPROM.write(i, 255);
  }
}

// Add calculation to memory
void memoryAdd(){
  if(currentKey != ""){
    calculateInput[calculateInputLength] = currentKey;
    calculateInputLength++;
  }

  // Calculate output
  for(int i=0; i<2; i++){
    // According to BODMAS
    mod();
    divide();
    multiply();
    add();
    minus();
  }

  // Reset everything and display result
  currentKey = "";
  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print(calculateInput[0]);
  calculateInputLength = 1;
  

  // Save output to memory
  memFunctionUsed = true;

  // We decided to use address from 40 to store memory function data in memory
  // hence 40 is used in memWriteAt function
  memWriteAt(memReadFrom(40) + calculateInput[0].toDouble(), 40);

  calculateInput[0] = "Null";
  calculateInputLength = 0;
  lastKey = 0;
  currentKey = "";
}

// Substract calculation to memory
void memorySubstract(){
  if(currentKey != ""){
    calculateInput[calculateInputLength] = currentKey;
    calculateInputLength++;
  }

  // Calculate output
  for(int i=0; i<2; i++){
    // According to BODMAS
    mod();
    divide();
    multiply();
    add();
    minus();
  }

  // Reset everything and display result
  currentKey = "";
  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print(calculateInput[0]);
  calculateInputLength = 1;


  // Save output to memory
  memFunctionUsed = true;

  // We decided to use address from 40 to store memory function data in memory
  // hence 40 is used in memWriteAt function
  memWriteAt(memReadFrom(40) - calculateInput[0].toDouble(), 40);

  calculateInput[0] = "Null";
  calculateInputLength = 0;
  lastKey = 0;
  currentKey = "";
}

// Recall memory
void memoryRecall(){
  memFunctionUsed = false;
  lcd.clear();
  lcd.setCursor(1, 0);

  // We decided to use address from 40 to store memory function data in memory
  // hence memReadFrom is reading starting from 40 to get back
  // data saved in eeprom
  String lcdOutput = String(memReadFrom(40));
  for(int i=0; i<lcdOutput.length(); i++){
    lcd.print(lcdOutput[i]);
  }
  
  currentKey = "";
  calculateInputLength = 1;
  calculateInput[0] = lcdOutput;
}

// Clear memory
void memoryClear(){
  memFunctionUsed = false;
  // We decided to use address from 40 to store memory function data in memory
  // hence memClear is clearing memory starting from 40.
  memClear(40);

  calculateInput[0] = "Null";
  calculateInputLength = 0;
  lastKey = 0;
  currentKey = "";
  lcd.clear();
  lcd.setCursor(1, 0);
}

// Read both sensors
void sensorReadings(){
  temperatureSensorReading();
  distanceSensorReading();
}

// Display and return distance sensor readings
double distanceSensorReading(){
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  int duration = pulseIn(PIN_ECHO, HIGH);
  Serial.print("Distance in CM: ");
  Serial.println(duration / 58);
  Serial.print("Distance in inches: ");
  Serial.println(duration / 148);
  Serial.println();

  return duration / 58;
}

// Display and return temperature sensor readings
double temperatureSensorReading(){
  int analogValue = analogRead(A8);
  double celsius = 1 / (log(1 / (1023. / analogValue - 1)) / BETA + 1.0 / 298.15) - 273.15;
  double fahrenheit = (celsius * 9 /5) + 32;

  Serial.print("Celsius: ");
  Serial.println(celsius);

  Serial.print("Fahrenheit: ");
  Serial.println(fahrenheit);
  Serial.println();

  return celsius;
}

// Run relevant function based on button pressed
void executeButton(){
  if(showCalc){
    uint8_t temp = key_read();
    if(no_key != temp){

      if((char)temp == '/') process('/');

      if((char)temp == 'N') negation();

      if((char)temp == '%') process('%');

      if((char)temp == 'F') squareRoot();

      if((char)temp == 'A'){
        memoryAdd();
        Serial.println("Added to memory");
      } 

      if((char)temp == 'S'){
        memorySubstract();
        Serial.println("Substracted from memory");
      } 

      if((char)temp == 'R'){
        memoryRecall();
        Serial.println("Memory recalled");
      } 

      if((char)temp == 'C'){
        memoryClear();
        Serial.println("Memory cleared");
      }
    }
  }
}

// Setup push buttons to used timer and hardware interrupt
void pushButtonMatrixSetup(){
  initaliseKeys();

  // 20ms timer interrupt
  Timer1.initialize(20000);
  Timer1.attachInterrupt(isr_key_callback);
  attachInterrupt(digitalPinToInterrupt(19), executeButton, FALLING);
  attachInterrupt(digitalPinToInterrupt(18), executeButton, FALLING);

  pinMode(col_1, INPUT_PULLUP);
  pinMode(col_2, INPUT_PULLUP);
  pinMode(col_3, INPUT_PULLUP);
  pinMode(col_4, INPUT_PULLUP);

  pinMode(row_1, OUTPUT);
  pinMode(row_2, OUTPUT);

  digitalWrite(row_1, HIGH);
  digitalWrite(row_2, HIGH);
}

// Read key
uint8_t key_read(void){
  main_read = 1U;
  uint8_t temp = key1;
  key1 = no_key;
  main_read = 0U;

  if(1U == main_interrupted){
    temp = key2;
    key2 = no_key;
    main_interrupted = 0U;
  }

  return temp;
}

// Initialise keys
void initaliseKeys(void){
  key_main_access = 0U;
  key_state = 0U;
  old_col = 1U;
  new_col = 1U;
  new_row = 1U;
  main_read = 0U;
  main_interrupted = 0U;
  key1 = no_key;
  key2 = no_key;
}

// ISR for timer interrupt
void isr_key_callback(){
  uint8_t check;

  if(0U == key_main_access){
    if(0U == key_state){
      digitalWrite(row_1, LOW); digitalWrite(row_2, LOW);  

      if(!digitalRead(col_1)){
        old_col = 1U;
        key_state = 1U;
      } else if(!digitalRead(col_2)){
        old_col = 2U;
        key_state = 1U;
      } else if(!digitalRead(col_3)){
        old_col = 3U;
        key_state = 1U;
      } else if(!digitalRead(col_4)){
        old_col = 4U;
        key_state = 1U;
      }                       
    } else if(1U == key_state){
      check = 0U;
      digitalWrite(row_1, LOW); digitalWrite(row_2, HIGH); 

      if(!digitalRead(col_1)){
        new_col = 1U;
        new_row = 1U;
      } else if(!digitalRead(col_2)){
        new_col = 2U;
        new_row = 1U;
      } else if(!digitalRead(col_3)){
        new_col = 3U;
        new_row = 1U;
      } else if(!digitalRead(col_4)){
        new_col = 4U;
        new_row = 1U;
      } else{
        check = 1U;  
      }

      if(1U == check){
        check = 0U;
        digitalWrite(row_1, HIGH); digitalWrite(row_2, LOW); 
        if(!digitalRead(col_1)){
          new_col = 1U;
          new_row = 2U;
        } else if(!digitalRead(col_2)){
          new_col = 2U;
          new_row = 2U;
        } else if(!digitalRead(col_3)){
          new_col = 3U;
          new_row = 2U;
        } else if(!digitalRead(col_4)){
          new_col = 4U;
          new_row = 2U;
        } else{
          check = 1U;
        }
      }

      if(1U == check){
        check = 0U;
        digitalWrite(row_1, HIGH); digitalWrite(row_2, HIGH); 
        if(!digitalRead(col_1)){
          new_col = 1U;
          new_row = 3U;
        } else if(!digitalRead(col_2)){
          new_col = 2U;
          new_row = 3U;
        } else if(!digitalRead(col_3)){
          new_col = 3U;
          new_row = 3U;
        } else if(!digitalRead(col_4)){
          new_col = 4U;
          new_row = 3U;
        } else{
          check = 1U;
        }
      }       

      if(1U == check){
        check = 0U;
        digitalWrite(row_1, HIGH); digitalWrite(row_2, HIGH); 
        if(!digitalRead(col_1)){
          new_col = 1U;
          new_row = 4U;
        } else if(!digitalRead(col_2)){
          new_col = 2U;
          new_row = 4U;
        } else if(!digitalRead(col_3)){
          new_col = 3U;
          new_row = 4U;
        } else if(!digitalRead(col_4)){
          new_col = 4U;
          new_row = 4U;
        } else{
          check = 1U;
        }
      }     

      if(0U == check){
        if(new_col == old_col){
          if((new_col >= 1U) && (new_col <= 4) && (new_row >= 1) && (new_row <= 4)){
            if(0U == main_read){
              key1 = keys_val[new_row-1U][new_col-1U];  
            } else{
              key2 = keys_val[new_row-1U][new_col-1U]; 
              main_interrupted = 1U;
            }
            key_state = 2U;
          } else{
            key_state = 0U;      
          }
        } else{
          key_state = 0U;
        }
      } else{
        key_state = 0U;
      }     

  } else if(2U == key_state){
      digitalWrite(row_1, LOW); digitalWrite(row_2, LOW); 
      if(digitalRead(col_1) && digitalRead(col_2) && digitalRead(col_3) && digitalRead(col_4)){
        key_state = 0U;
      }
    }
  }
}