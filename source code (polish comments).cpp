#include <LiquidCrystal.h>
#include <IRremote.h>

// Stale do obslugi czujnika podczerwieni
#define ZERO 0
#define ONE 1
#define TWO 2
#define THREE 3
#define FOUR 4
#define FIVE 5
#define SIX 6
#define SEVEN 7
#define EIGHT 8
#define NINE 9
#define ON_OFF 10
#define VOL_PLUS 11
#define FUNC_STOP 22
#define BACK 13
#define RESUME_PAUSE 14
#define FORWARD 15
#define DOWN 16
#define VOL_MINUS 17
#define UP 18
#define EQ 19
#define ST_REPT 20

// Piny
#define IRsensor  A5
#define buzzer    A4

#define redWire   8
#define greenWire 9
#define blueWire  10
#define pinkWire  11

// LCD
#define RS 7
#define E  6
#define D4 5
#define D5 4
#define D6 3
#define D7 2
LiquidCrystal led(RS, E, D4, D5, D6, D7);

// Customowy znak
byte skull[8] = {
  B00000,
  B01110,
  B10101,
  B11011,
  B01110,
  B01110,
  B00000,
};

// Czujnik podczerwieni
IRrecv IRrecv(IRsensor);
decode_results results;

// Zmienne
bool tmpState[4];                  // Tymczasowy stan (uzywane przy przyciskach)
bool state[4];                     // Aktualny stan (uzywane przy przyciskach)
bool wiresDisabled = false;        // Czy przyciski (kable) sa wylaczone
bool disarmed_or_exploded = false; // Czy bomba zostala rozbrojona lub wysadzona

bool initialized = false; // Czy bomba zostala zainicjalizowana
bool resettable = false;  // Czy bombe mozna jeszcze zresetowac naciskajac ponownie ON_OFF
bool timeEntered = false; // Czy czas zostal podany
bool wireEntered = false; // Czy kabel rozbrajajacy zostal wybrany
short timeIndex = 0;      // Indeks potrzebny do odczytania czasu odliczania
String time = "";         // Czas jako ciag znakow, ktory potem jest zamieniany w zmienne liczbowe
short disarmWire = 0;     // Kabel rozbrajajacy
long countdown = 0;       // Czas odliczania w sekundach

short startTimer, endTimer; // Potrzebne do zczytywania millis()

void setup()
{
  // Inicjalizacja komponentow
  pinMode(IRsensor, INPUT);
  pinMode(buzzer,   OUTPUT);
  
  pinMode(redWire,   INPUT);
  pinMode(greenWire, INPUT);
  pinMode(blueWire,  INPUT);
  pinMode(pinkWire,  INPUT);
  
  digitalWrite(redWire,   HIGH);
  digitalWrite(greenWire, HIGH);
  digitalWrite(blueWire,  HIGH);
  digitalWrite(pinkWire,  HIGH);
  
  led.begin(16,2);
  led.createChar(0, skull);
  IRrecv.enableIRIn();
  IRrecv.blink13(true);
  Serial.begin(9600);
}

void loop()
{
  short key = IRcheck();
  
  // ===== Etap inicjalizacji bomby =====
  if(!initialized)
  {
    if(key == ON_OFF
       && !initialized)
    {
      // Reset bomby
      if(resettable)
        reset();
      // Start inicjalizacji bomby
      else
      {
        lcdPrint(F("Starting bomb initialization"));
        delay(3000);
    
        lcdPrint(F("Enter countdown time: HH:MM:SS"));
        led.setCursor(6, 1);
        resettable = true;
      }
    }
  
    // Podanie czasu odliczania
    if(key >= ZERO && key <= NINE
       && !timeEntered
       && resettable)
    {
      key = inputTime(key);
    }
  
    // Podanie kabla rozbrajającego
    if(key >= ONE && key <= FOUR
       && !wireEntered
       && timeEntered
       && resettable)
    {
      wireEntered = true;
      disarmWire = key;
      lcdPrint(F("Bomb is ready, press >|| to arm"));
    }
  
    // Zatwierdzenie inicjalizacji bomby
    if(key == RESUME_PAUSE
       && wireEntered
       && timeEntered
       && resettable)
    {
      armed();
      startTimer = millis();
    }
  }
  
  // ===== Etap uzbrojonej bomby =====
  else
  {
    // Gdy bomba nie zostala rozbrojona lub wysadzona
    if(!disarmed_or_exploded)
    {
      endTimer = millis();
      
      // Pikanie bomby poprzez buzzer
      if(endTimer - startTimer <= 100)
      {
        digitalWrite(buzzer, HIGH);
        delayMicroseconds(200);
        digitalWrite(buzzer, LOW);
        delayMicroseconds(1);
      }
      
      // Sprawdza, czy minela sekunda
      if(endTimer - startTimer >= 1000)
      {
        startTimer = endTimer;
        countdown--;
        if(countdown == -1)
          explode();
        else if(countdown >= 0)
          printTime();
      }
    
      // Sprawdza, czy przecieto kabel
      short wire = btnCheck();
      if(wire != -1)
      {
        // Przecieto zly kabel
        if(wire != disarmWire)
          explode();
        // Przecieto kabel rozbrajajacy
        else
          disarm();
      }
    
      short key = IRcheck();
      // Zdalne wysadzenie
      if(key == ON_OFF)
        explode();
      // Zdalne rozbrojenie
      else if(key == RESUME_PAUSE)
        disarm();
    }
  }
}

// Resetowanie inicjalizacji bomby
void reset()
{
  timeEntered = false;
  resettable = false;
  wireEntered = false;
  timeIndex = 0;
  time = "";
  disarmWire = 0;
    
  lcdPrint(F("Resetting bomb, press ON/OFF"));
}

// Wpisywanie czasu przez uzytkownika
short inputTime(short key)
{
  if(timeIndex == 2 || timeIndex == 5)
  {
    led.print(":");
    timeIndex++;
  }
  if(timeIndex == 7)
  {
    time += key;
    timeEntered = true;
    led.print(key);
    led.print(F("ok"));
    delay(2000);
    lcdPrint(F("Enter disarm wire: 1,2,3 or 4"));
    key = -1;
  }
  else
  {
    time += key;
    timeIndex++;
    led.print(key);
  }
  return key;
}

// Uzbrajanie bomby
void armed()
{
  resettable = false;
  initialized = true;
      
  short hours   = atoi(time.substring(0,2).c_str());
  short minutes = atoi(time.substring(2,4).c_str());
  short seconds = atoi(time.substring(4,6).c_str());
  countdown = (long)hours*3600 + (long)minutes*60 + (long)seconds;
      
  led.clear();
  led.write(byte(0));
  led.print("  Bomb armed  ");
  led.write(byte(0));
  printTime();
}

// Wysadzanie bomby
void explode()
{
  disarmed_or_exploded = true;
  lcdPrint(F("    KA-BOOM!"));
  
  startTimer = millis();
  endTimer = startTimer;
  while(endTimer - startTimer <= 3000)
  {
    endTimer = millis();
  	digitalWrite(buzzer, HIGH);
    delay(10);
    digitalWrite(buzzer, LOW);
    delay(10);
  }
}

// Rozbrojenie bomby
void disarm()
{
  disarmed_or_exploded = true;
  led.clear();
  led.setCursor(6,0);
  led.print(F("Bomb")); 
  led.setCursor(4,1);
  led.print(F("disarmed"));
}

// Wypisuje na wyswietlaczu aktualny czas odliczania
void printTime()
{
  short hours = countdown/3600;
  short minutes = (countdown%3600)/60;
  short seconds = (countdown%3600)%60;
  
  led.setCursor(4,1);
  if(hours < 10)
    led.print(0);
  led.print(hours);
  led.print(F(":"));
        
  if(minutes < 10)
    led.print(0);
  led.print(minutes);
  led.print(F(":"));
        
  if(seconds < 10)
    led.print(0);
  led.print(seconds);
}


// Funkcja od wyswietlania na LCD
void lcdPrint(String msg)
{
  short length = msg.length();
  short index = 0;
  
  led.clear();
  led.setCursor(0,0);
  for(int i = 0; i < length; i++)
  {
    if(msg.charAt(i) == ' ')
    {
      led.print(msg.substring(index, i+1));
      index = i+1;
    }
    else if(i == 16)
      led.setCursor(0,1);
  }
  led.print(msg.substring(index, length));
}

// Sprawdza stany przyciskow i je obsluguje
short btnCheck()
{
  byte wire = redWire;
  for(int i = 0; i < 4; i++)
  {
    if(!wiresDisabled)
    {
      tmpState[i] = digitalRead(wire);
      if(state[i] != tmpState[i])
      {
        delay(10);
        tmpState[i] = digitalRead(wire);
        state[i] = tmpState[i];

        if(state[i] == LOW)
        {
      	  switch(wire)
          {
            case redWire:
          	  wiresDisabled = true;
          	  return 1;
            case greenWire:
              wiresDisabled = true;
          	  return 2;
            case blueWire:
              wiresDisabled = true;
          	  return 3;
            case pinkWire:
              wiresDisabled = true;
          	  return 4;
          }
        }
      }
    }
    wire++;
  }
  return -1;
}

// Sprawdza stan czujnika podczerwieni i go obsluguje
short IRcheck()
{
  short key = -1;
  
  if(IRrecv.decode(&results))
  {
    switch(results.value)
    {
      case 0xFD00FF:
        key = ON_OFF;
        break;
      case 0xFD807F:
        key = VOL_PLUS;
        break;
      case 0xFD40BF:
        key = FUNC_STOP;
        break;
      case 0xFD20DF:
        key = BACK;
        break;
      case 0xFDA05F:
        key = RESUME_PAUSE;
        break;
      case 0xFD609F:
        key = FORWARD;
        break;
      case 0xFD10EF:
        key = DOWN;
        break;
      case 0xFD906F:
        key = VOL_MINUS;
        break;
      case 0xFD50AF:
        key = UP;
        break;
      case 0xFD30CF:
        key = ZERO;
        break;
      case 0xFDB04F:
        key = EQ;
        break;
      case 0xFD708F:
        key = ST_REPT;
        break;
      case 0xFD08F7:
        key = ONE;
        break;
      case 0xFD8877:
        key = TWO;
        break;
      case 0xFD48B7:
        key = THREE;
        break;
      case 0xFD28D7:
        key = FOUR;
        break;
      case 0xFDA857:
        key = FIVE;
        break;
      case 0xFD6897:
        key = SIX;
        break;
      case 0xFD18E7:
        key = SEVEN;
        break;
      case 0xFD9867:
        key = EIGHT;
        break;
      case 0xFD58A7:
        key = NINE;
        break;
    }
    IRrecv.resume(); 
  }
  return key;
}