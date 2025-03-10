/*
  Код AlexGyver и EfimKR изменен для работы с RGB лентой
 */

/*
  Скетч к проекту "Будильник - рассвет"
  Страница проекта (схемы, описания): https://alexgyver.ru/dawn-clock/
  Исходники на GitHub: https://github.com/AlexGyver/dawn-clock
  Нравится, как написан и закомментирован код? Поддержи автора! https://alexgyver.ru/support_alex/
  Автор: AlexGyver Technologies, 2018
  http://AlexGyver.ru/
*/

/*
  Доработал EfimKR - добавлена возможность выбора срабатывания по будним дням
  Исходники на GitHub: https://github.com/EfimKR/Dawn-Clock

  При установке даты есть разумные ограничение на ввод значений, но они охватывают не все.
  Если Вы ввели что-то криво, то Вы сам себе злобный буратино.

  установка даты: крутим ручку енкодера - меняем день, крутим зажатую ручку - меняем месяц.
*/

/*
   Клик в режиме часов -> установка будильника
   Клик в режиме установки будильника -> установка времени
   Клик в режиме установки времени -> установка даты
   Клик в режиме установки даты -> установка года
   Клик в режиме установки года -> режим часов

   Удержание в режиме часов -> вкл/выкл будильник
   Удержание в режиме любой установки -> режим часов

   Режим часов: двоеточие моргает раз в секунду
   Установка будильника: цифры вместе с двоеточием моргают
   Установка часов: двоеточие горит, цифры моргают
   Установка даты: цифры горят, двоеточия нет
   Установка года: цифры горят, первые цифры 20, двоеточия нет
*/



// константы

#define DAWN_TYPE_AC 0
#define DAWN_TYPE_DC 1
#define DAY_CONDITION_EVERY_DAY 0
#define DAY_CONDITION_WEEK_DAYS 1

// *************************** НАСТРОЙКИ ***************************
#define DAWN_TIME 30      // продолжительность рассвета (в минутах)
#define ALARM_TIMEOUT 70  // таймаут на автоотключение будильника, секунды
#define ALARM_BLINK 0     // 1 - мигать лампой при будильнике, 0 - не мигать
#define CLOCK_EFFECT 2    // эффект перелистывания часов: 0 - обычный, 1 - прокрутка, 2 - скрутка
#define BUZZ 0            // пищать пищалкой (1 вкл, 0 выкл)
#define BUZZ_FREQ 800     // частота писка (Гц)

#define DAWN_TYPE DAWN_TYPE_DC       // 1 - мосфет (DC диммер), 0 - симистор (AC диммер) СМОТРИ СХЕМЫ
#define DAWN_MIN 20                  // начальная яркость лампы (0 - 255) (для сетевых матриц начало света примерно с 50)
#define DAWN_MAX 255                 // максимальная яркость лампы (0 - 255)

#define MAX_BRIGHT 7      // яркость дисплея дневная (0 - 7)
#define MIN_BRIGHT 1      // яркость дисплея ночная (0 - 7)
#define NIGHT_START 23    // час перехода на ночную подсветку (MIN_BRIGHT)
#define NIGHT_END 7       // час перехода на дневную подсветку (MAX_BRIGHT)
#define LED_BRIGHT 20     // яркость светодиода индикатора (0 - 255)

#define ENCODER_TYPE 1    // тип энкодера (0 или 1). Типы энкодеров расписаны на странице проекта

#define DAY_CONDITION DAY_CONDITION_WEEK_DAYS   // 0 - срабатывать каждый день, 1 - по будням

// ************ ПИНЫ ************
#define CLKe 8        // энкодер
#define DTe 9         // энкодер
#define SWe 10        // энкодер

#define CLK 12        // дисплей
#define DIO 11        // дисплей

#define ZERO_PIN 2    // пин детектора нуля (Z-C) для диммера (если он используется)
//#define DIM_PIN 3     // мосфет / DIM(PWM) пин диммера
#define DIM_PIN_RED 5
#define DIM_PIN_BLUE 6
#define DIM_PIN_GREEN 3
//#define BUZZ_PIN 7    // пищалка (по желанию)
//#define LED_PIN 6     // светодиод индикатор

// ***************** РЕЖИМЫ  *****************
#define MODE_DEFAULT 0    // обычный режим
#define MODE_SET_ALARM 1  // установка будильника
#define MODE_SET_TIME 2   // установка времени
#define MODE_SET_DATE 3   // установка даты
#define MODE_SET_YEAR 4   // установка года

// ***************** ОБЪЕКТЫ И ПЕРЕМЕННЫЕ *****************
# include "GyverTimer.h"
GTimer_ms halfsTimer(500);
GTimer_ms blinkTimer(800);
GTimer_ms timeoutTimer(15000);
GTimer_ms dutyTimer((long) DAWN_TIME * 60 * 1000 / (DAWN_MAX - DAWN_MIN));
GTimer_ms alarmTimeout((long) ALARM_TIMEOUT * 1000);

# include "GyverEncoder.h"
Encoder enc(CLKe, DTe, SWe);

# include "GyverTM1637.h"
GyverTM1637 disp(CLK, DIO);

# include "EEPROM.h"
# include <CyberLib.h> // шустрая библиотека для таймера

# include <Wire.h>
# include "RTClib.h"
RTC_DS3231 rtc;

boolean dotFlag, alarmFlag, minuteFlag, blinkFlag, needSaveDateTime;
int8_t hrs = 21, mins = 55, secs, day, month, year, dayOfWeek;
int8_t alm_hrs, alm_mins;
int8_t dwn_hrs, dwn_mins;
byte mode;  // смотри define MODE_

boolean dawn_start = false;
boolean alarm = false;
int tic, duty;
int val;
uint8_t daysInMonth[]  = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

void setup()
{
  Serial.begin(9600);
  pinMode(DIM_PIN_GREEN, OUTPUT);
  Serial.println("setup. pinmode_green");
  pinMode(DIM_PIN_RED, OUTPUT);
  Serial.println("setup. pinmode_red");
  pinMode(DIM_PIN_BLUE, OUTPUT);
  Serial.println("setup. pinmode_blue");

#if (DAWN_TYPE == DAWN_TYPE_AC)
  pinMode(ZERO_PIN, INPUT);
  Serial.println("setup. pinmode_zero");
  attachInterrupt(0, detect_up, FALLING);
  StartTimer1(timer_interrupt, 40);        // время для одного разряда ШИМ
  StopTimer1();                            // остановить таймер
#endif

  enc.setType(ENCODER_TYPE);     // тип энкодера TYPE1 одношаговый, TYPE2 двухшаговый. Если ваш энкодер работает странно, смените тип
  Serial.println("setup. setType_encoder");
  disp.clear();
  Serial.println("setup. disp_clear");

  if (rtc.begin()) {
    Serial.println("DS3231 found");
  }
  if (!rtc.begin()) {
    Serial.println("DS3231 not found");
  }
  
 if (rtc.lostPower()) {            // выполнится при сбросе батарейки
    Serial.println("lost power!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }


  
  DateTime now = rtc.now();
  secs = now.second();
  mins = now.minute();
  hrs = now.hour();
  day = now.day();
  month = now.month();
  year = now.year() - 2000;
  year = constrain(year, 1, 99);
  dayOfWeek = now.dayOfTheWeek();

  disp.displayClock(hrs, mins);
  Serial.println("setup. displayClock");

  alm_hrs = EEPROM.read(0);
  Serial.println("setup. eeprom_reed_hrs");
  alm_mins = EEPROM.read(1);
  Serial.print("setup. eeprom_reed_mins: ");
  alarmFlag = EEPROM.read(2);
  alm_hrs = constrain(alm_hrs, 0, 23);
  alm_mins = constrain(alm_mins, 0, 59);
  calculateDawn();      // расчёт времени рассвета
  alarmFlag = constrain(alarmFlag, 0, 1);

  // установка яркости от времени суток
  checkDisplayBrightness();
}

void loop()
{
  encoderTick();  // отработка энкодера
  clockTick();    // считаем время
  alarmTick();    // обработка будильника
  settings();     // настройки
  dutyTick();     // управление лампой

  if (enc.isRight() && !dawn_start && !alarm) {
    val++ ;
    if (val > 15) val = 15;
    duty = val*17;
    analogWrite(DIM_PIN_RED, duty); 
   // analogWrite(DIM_PIN_GREEN, duty/4);
   // analogWrite(DIM_PIN_BLUE, duty/4); 
    Serial.println(duty);

    }
  if (enc.isLeft() && !dawn_start && !alarm) {
    val--;
    if (val <0) val = 0;
    duty = val*17;
    analogWrite(DIM_PIN_RED, duty); 
   // analogWrite(DIM_PIN_GREEN, duty);
    //analogWrite(DIM_PIN_BLUE, duty); 
    Serial.println(duty);
}

  if (minuteFlag && mode == MODE_DEFAULT && !alarm)    // если новая минута и стоит режим часов и не орёт будильник
  {
    minuteFlag = false;
    // выводим время
    if (CLOCK_EFFECT == 0)
    {
      disp.displayClock(hrs, mins);
    }
    else if (CLOCK_EFFECT == 1)
    {
      disp.displayClockScroll(hrs, mins, 70);
    }
    else
    {
      disp.displayClockTwist(hrs, mins, 35);
    }
  }
}

void checkDisplayBrightness()
{
  if ((hrs >= NIGHT_START && hrs <= 23) || (hrs >= 0 && hrs <= NIGHT_END))
  {
    disp.brightness(MIN_BRIGHT);
  }
  else
  {
    disp.brightness(MAX_BRIGHT);
  }
}

void calculateDawn()
{
  // расчёт времени рассвета
  if (alm_mins >= DAWN_TIME)
  { // если минут во времени будильника больше продолжительности рассвета
    dwn_hrs = alm_hrs;                // час рассвета равен часу будильника
    dwn_mins = alm_mins - DAWN_TIME;  // минуты рассвета = минуты будильника - продолж. рассвета
  }
  else
  { // если минут во времени будильника меньше продолжительности рассвета
    dwn_hrs = alm_hrs - 1;            // значит рассвет будет часом раньше
    if (dwn_hrs < 0)
    {
      dwn_hrs = 23;    // защита от совсем поехавших
    }

    dwn_mins = 60 - (DAWN_TIME - alm_mins);   // находим минуту рассвета в новом часе
  }
}

void dutyTick()
{
#if (DAWN_TYPE == DAWN_TYPE_DC)      // если мосфет
  if (dawn_start || alarm)      // если рассвет или уже будильник
  {
    //analogWrite(DIM_PIN, duty);   // жарим ШИМ
    analogWrite(DIM_PIN_RED, duty); 
    analogWrite(DIM_PIN_GREEN, duty);
    analogWrite(DIM_PIN_BLUE, duty); 
  }
#endif // если DAWN_TYPE == DAWN_TYPE_AC то код не скомпилируется и функция dutyTick будет пуста
}

#if (DAWN_TYPE == DAWN_TYPE_AC)  // если диммер
//----------------------ОБРАБОТЧИКИ ПРЕРЫВАНИЙ--------------------------
void timer_interrupt() {          // прерывания таймера срабатывают каждые 40 мкс
  if (duty > 0) {
    tic++;                        // счетчик
    if (tic > (255 - duty))       // если настало время включать ток
      \\digitalWrite(DIM_PIN, 1);   // врубить ток
      digitalWrite(DIM_PIN_BLUE, 1);   // врубить ток
      digitalWrite(DIM_PIN_RED, 1);
      digitalWrite(DIM_PIN_GREEN, 1);

  }
}

void detect_up() {    // обработка внешнего прерывания на пересекание нуля снизу
  if (duty > 0) {
    tic = 0;                                  // обнулить счетчик
    ResumeTimer1();                           // перезапустить таймер
    attachInterrupt(0, detect_down, RISING);  // перенастроить прерывание
  }
}

void detect_down() {      // обработка внешнего прерывания на пересекание нуля сверху
  if (duty > 0) {
    tic = 0;                                  // обнулить счетчик
    StopTimer1();                             // остановить таймер
    \\digitalWrite(DIM_PIN, 0);                 // вырубить ток
    digitalWrite(DIM_PIN_RED, 0);                 // вырубить ток
    digitalWrite(DIM_PIN_BLUE, 0);                 // вырубить ток
    digitalWrite(DIM_PIN_GREEN, 0);                 // вырубить ток

    attachInterrupt(0, detect_up, FALLING);   // перенастроить прерывание
  }
}
#endif

void settings()
{
  switch (mode)
  {
    case MODE_SET_ALARM:
      // *********** РЕЖИМ УСТАНОВКИ БУДИЛЬНИКА **********

      if (timeoutTimer.isReady())
      {
        mode = MODE_DEFAULT;   // если сработал таймаут, вернёмся в режим 0
      }

      if (enc.isRight())
      {
        alm_mins++;
        if (alm_mins > 59)
        {
          alm_mins = 0;
          alm_hrs++;
          if (alm_hrs > 23)
          {
            alm_hrs = 0;
          }
        }
      }

      if (enc.isLeft())
      {
        alm_mins--;
        if (alm_mins < 0)
        {
          alm_mins = 59;
          alm_hrs--;
          if (alm_hrs < 0)
          {
            alm_hrs = 23;
          }
        }
      }

      if (enc.isRightH())
      {
        alm_hrs++;
        if (alm_hrs > 23)
        {
          alm_hrs = 0;
        }
      }

      if (enc.isLeftH())
      {
        alm_hrs--;
        if (alm_hrs < 0)
        {
          alm_hrs = 23;
        }
      }

      if (enc.isTurn() && !blinkFlag)
      {
        // вывести свежие изменения при повороте
        disp.displayClock(alm_hrs, alm_mins);
        timeoutTimer.reset();               // сбросить таймаут
      }

      if (blinkTimer.isReady())
      {
        if (blinkFlag)
        {
          blinkFlag = false;
          blinkTimer.setInterval(700);
          disp.point(1);
          disp.displayClock(alm_hrs, alm_mins);
        }
        else
        {
          blinkFlag = true;
          blinkTimer.setInterval(300);
          disp.point(0);
          disp.clear();
        }
      }
      break;

    case MODE_SET_TIME:
      // *********** РЕЖИМ УСТАНОВКИ ВРЕМЕНИ **********

      if (timeoutTimer.isReady())
      {
        mode = MODE_DEFAULT;   // если сработал таймаут, вернёмся в режим 0
      }

      if (!needSaveDateTime)
      {
        needSaveDateTime = true;   // флаг на изменение времени
      }

      if (enc.isRight())
      {
        mins++;
        if (mins > 59)
        {
          mins = 0;
          hrs++;
          if (hrs > 23)
          {
            hrs = 0;
          }
        }
      }

      if (enc.isLeft())
      {
        mins--;
        if (mins < 0)
        {
          mins = 59;
          hrs--;
          if (hrs < 0)
          {
            hrs = 23;
          }
        }
      }

      if (enc.isRightH())
      {
        hrs++;
        if (hrs > 23)
        {
          hrs = 0;
        }
      }

      if (enc.isLeftH())
      {
        hrs--;
        if (hrs < 0)
        {
          hrs = 23;
        }
      }

      if (enc.isTurn() && !blinkFlag)
      { // вывести свежие изменения при повороте
        disp.displayClock(hrs, mins);
        timeoutTimer.reset();           // сбросить таймаут
      }

      if (blinkTimer.isReady())
      {
        // прикол с перенастройкой таймера, чтобы цифры дольше горели
        disp.point(1);
        if (blinkFlag)
        {
          blinkFlag = false;
          blinkTimer.setInterval(700);
          disp.displayClock(hrs, mins);
        }
        else
        {
          blinkFlag = true;
          blinkTimer.setInterval(300);
          disp.clear();
        }
      }

      break;

    case MODE_SET_DATE:
      // *********** РЕЖИМ УСТАНОВКИ ДАТЫ **********

      if (timeoutTimer.isReady())
      {
        mode = MODE_DEFAULT;   // если сработал таймаут, вернёмся в режим 0
      }

      if (!needSaveDateTime)
      {
        needSaveDateTime = true;   // флаг на изменение времени
      }

      if (enc.isRight())
      {
        day++;
        if (day > daysInMonth[month - 1])
        {
          day = 1;
        }
      }

      if (enc.isLeft())
      {
        day--;
        if (day < 0)
        {
          day = daysInMonth[month - 1];
        }
      }

      if (enc.isRightH())
      {
        month++;
        if (month > 12)
        {
          month = 1;
        }
      }

      if (enc.isLeftH())
      {
        month--;
        if (month < 0)
        {
          month = 12;
        }
      }

      if (enc.isTurn() && !blinkFlag)
      {
        // вывести свежие изменения при повороте
        disp.displayClock(month, day);
        timeoutTimer.reset();           // сбросить таймаут
      }

      if (blinkTimer.isReady())
      {
        disp.point(0);

        if (blinkFlag)
        {
          blinkFlag = false;
          blinkTimer.setInterval(700);
          disp.displayClock(month, day);
        }
        else
        {
          blinkFlag = true;
          blinkTimer.setInterval(300);
        }
      }

      break;

    case MODE_SET_YEAR:
      // *********** РЕЖИМ УСТАНОВКИ ГОДА **********

      if (timeoutTimer.isReady())
      {
        mode = MODE_DEFAULT;   // если сработал таймаут, вернёмся в режим 0
      }

      if (!needSaveDateTime)
      {
        needSaveDateTime = true;   // флаг на изменение времени
      }

      if (enc.isRight())
      {
        year++;
        if (year > 99)
        {
          year = 1;
        }
      }

      if (enc.isLeft())
      {
        year--;
        if (year < 0)
        {
          year = 99;
        }
      }

      if (enc.isTurn() && !blinkFlag)
      {
        // вывести свежие изменения при повороте
        disp.displayClock(20, year);
        timeoutTimer.reset();           // сбросить таймаут
      }

      if (blinkTimer.isReady())
      {
        disp.point(0);

        if (blinkFlag)
        {
          blinkFlag = false;
          blinkTimer.setInterval(700);
          disp.displayClock(20, year);
        }
        else
        {
          blinkFlag = true;
          blinkTimer.setInterval(300);
        }
      }

      break;
  }
}

void encoderTick()
{
  enc.tick();           // работаем с энкодером
  // *********** КЛИК ПО ЭНКОДЕРУ **********
  if (enc.isClick())
  {
    minuteFlag = true;      // вывести минуты при следующем входе в режим 0
    mode++;           // сменить режим

    if (mode > MODE_SET_YEAR)
    {
      // выход с режима установки будильника и часов
      mode = MODE_DEFAULT;
      calculateDawn();        // расчёт времени рассвета
      EEPROM.update(0, alm_hrs);
      EEPROM.update(1, alm_mins);

      // установка яркости от времени суток
      checkDisplayBrightness();

      disp.displayClock(hrs, mins);
    }

    timeoutTimer.reset();     // сбросить таймаут
  }

  // *********** УДЕРЖАНИЕ ЭНКОДЕРА **********
  if (enc.isHolded())
  {
    minuteFlag = true;        // вывести минуты при следующем входе в режим 0
    if (dawn_start)
    {
      // если удержана во время рассвета или будильника
      dawn_start = false;     // прекратить рассвет
      alarm = false;          // и будильник
      duty = 0;
      //digitalWrite(DIM_PIN, 0);
      digitalWrite(DIM_PIN_RED, 0);
      digitalWrite(DIM_PIN_GREEN, 0);
      digitalWrite(DIM_PIN_BLUE, 0);


//      if (BUZZ)
//      {
//        noTone(BUZZ_PIN);
//      }

      return;
    }

    if (mode == MODE_DEFAULT)
    {
      // кнопка удержана в режиме часов и сейчас не рассвет
      disp.point(0);              // гасим кнопку
      alarmFlag = !alarmFlag;     // переключаем будильник
      if (alarmFlag)
      {
        disp.scrollByte(_empty, _o, _n, _empty, 70);
//        analogWrite(LED_PIN, LED_BRIGHT);
      }
      else
      {
        disp.scrollByte(_empty, _o, _F, _F, 70);
//        digitalWrite(LED_PIN, 0);
      }

      EEPROM.update(2, alarmFlag);
      delay(1000);
      disp.displayClockScroll(hrs, mins, 70);
    }
    else
    {
      // кнопка зажата в любом режиме настройки
      // переходим в режим часов

      mode = MODE_DEFAULT;

      calculateDawn();        // расчёт времени рассвета
      EEPROM.update(0, alm_hrs);
      EEPROM.update(1, alm_mins);

      // установка яркости от времени суток
      checkDisplayBrightness();

      disp.displayClock(hrs, mins);
    }

    timeoutTimer.reset();     // сбросить таймаут
  }
}

void alarmTick()
{
  if (dawn_start && dutyTimer.isReady())
  {
    // поднимаем яркость по таймеру
    duty++;
    if (duty > DAWN_MAX)
    {
      duty = DAWN_MAX;
    }
  }

  if (alarm)
  {
    // настало время будильника
    if (alarmTimeout.isReady())
    {
      dawn_start = false;         // прекратить рассвет
      alarm = false;              // и будильник
      duty = 0;
      //digitalWrite(DIM_PIN, 0);
      digitalWrite(DIM_PIN_RED, 0);
      digitalWrite(DIM_PIN_GREEN, 0);
      digitalWrite(DIM_PIN_BLUE, 0);

//      if (BUZZ)
//      {
//       noTone(BUZZ_PIN);
//      }
    }

    if (blinkTimer.isReady())
    {
      if (blinkFlag)
      {
        blinkFlag = false;
        blinkTimer.setInterval(700);
        disp.point(1);
        disp.displayClock(hrs, mins);

        if (ALARM_BLINK)
        {
          duty = DAWN_MAX;     // мигаем светом
        }

//        if (BUZZ)
  //      {
    //    tone(BUZZ_PIN, BUZZ_FREQ);  // пищим
      //  }
      }
      else
      {
        blinkFlag = true;
        blinkTimer.setInterval(300);
        disp.point(0);
        disp.clear();

        if (ALARM_BLINK)
        {
          duty = DAWN_MIN;
        }

//        if (BUZZ)
  //      {
    //      noTone(BUZZ_PIN);
      //  }
      }
    }
  }
}

void clockTick()
{
  if (halfsTimer.isReady())
  {
    if (needSaveDateTime)
    {
      needSaveDateTime = false;
      secs = 0;
      rtc.adjust(DateTime(2000 + year, month, day, hrs, mins, 0)); // установка нового времени в RTC
    }

    dotFlag = !dotFlag;
    if (mode == MODE_DEFAULT)
    {
      disp.point(dotFlag);                 // выкл/выкл точки
      disp.displayClock(hrs, mins);        // костыль, без него подлагивает дисплей
    }

/*    if (alarmFlag)
    {
      if (dotFlag)
      {
        analogWrite(LED_PIN, LED_BRIGHT);    // мигаем светодиодом что стоит аларм
      }
      else
      {
        digitalWrite(LED_PIN, 0);
      }
    }
*/
    if (dotFlag)
    {
      // каждую секунду пересчёт времени
      secs++;
      if (secs > 59)
      {
        secs = 0;
        mins++;
        minuteFlag = true;
      }

      if (mins > 59)
      {
        // некоторые переменные имеют имя совпадающее с ключевыми словами функции
        // ардуино IDE подсвечивает их, как методы
        // не обращайте на это внимание
        DateTime now = rtc.now();
        secs = now.second();
        mins = now.minute();
        hrs = now.hour();
        day = now.day();
        month = now.month();
        year = now.year() - 2000;
        dayOfWeek = now.dayOfTheWeek();

        // меняем яркость
        if (hrs == NIGHT_START)
        {
          disp.brightness(MIN_BRIGHT);
        }

        if (hrs == NIGHT_END)
        {
          disp.brightness(MAX_BRIGHT);
        }
      }

      // после пересчёта часов проверяем будильник!
      // если сегодня день срабатывания будильника
      // и будильник установлен

      if (dotFlag && isAlarmDay() && alarmFlag)
      {
        if (dwn_hrs == hrs && dwn_mins == mins && !dawn_start)
        {
          duty = DAWN_MIN;
          dawn_start = true;
        }

        if (alm_hrs == hrs && alm_mins == mins && dawn_start && !alarm)
        {
          alarm = true;
          alarmTimeout.reset();
        }
      }
    }
  }
}

boolean isAlarmDay()
{
#if (DAY_CONDITION == DAY_CONDITION_EVERY_DAY)
  return true;
#else

  if (dayOfWeek == 0 || dayOfWeek == 6)  // воскресенье или субота
  {
    return false;
  }
  else
  {
    return true;
  }

#endif
}
