#pragma once

/*
 Debug.h — глобальная опция подробной отладки.

 По умолчанию подробные Serial-логи выключены. Чтобы включить их:
  - либо раскомментируйте строку ниже:
      #define DEBUG_VERBOSE
  - либо при компиляции добавьте флаг компилятора:
      -DDEBUG_VERBOSE

 Рекомендуется включать DEBUG_VERBOSE только для отладки (локально),
 т.к. частый Serial-вывод в "горячих" участках (контроль SSR, автотюн и т.п.)
 блокирует цикл и ухудшает отзывчивость в реальном времени.
*/

/// Раскомментируйте, чтобы включить подробные логи:
#define DEBUG_VERBOSE

// Утилитарные макросы (можно использовать где удобно)
#ifdef DEBUG_VERBOSE
  #define DBG_PRINT(x)    Serial.print(x)
  #define DBG_PRINTLN(x)  Serial.println(x)
#else
  #define DBG_PRINT(x)    ((void)0)
  #define DBG_PRINTLN(x)  ((void)0)
#endif