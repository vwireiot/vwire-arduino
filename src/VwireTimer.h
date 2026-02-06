/*
 * Vwire IOT Arduino Library - Timer
 * 
 * Non-blocking software timer for scheduling tasks.
 * Uses millis() for universal compatibility with all Arduino boards.
 * 
 * Features:
 * - Multiple concurrent timers
 * - Repeating intervals (setInterval)
 * - One-shot timeouts (setTimeout)
 * - Run N times (setTimer)
 * - Enable/disable/toggle control
 * - Change interval on the fly
 * - Callback with optional argument
 * 
 * Compatible with:
 * - ESP32, ESP8266
 * - Arduino Uno, Nano, Mega (AVR)
 * - Arduino Due (SAM)
 * - SAMD21/SAMD51 (MKR, Zero)
 * - RP2040 (Raspberry Pi Pico)
 * - STM32, Teensy, and more
 * 
 * Copyright (c) 2026 Vwire IOT
 * Website: https://vwire.io
 * MIT License
 */

#ifndef VWIRE_TIMER_H
#define VWIRE_TIMER_H

#include <Arduino.h>

// =============================================================================
// CONFIGURATION
// =============================================================================

// Maximum number of timers - can be overridden before including this header
#ifndef VWIRE_MAX_TIMERS
  #if defined(__AVR__)
    #define VWIRE_MAX_TIMERS 8      // Limited RAM on AVR boards
  #elif defined(ESP8266)
    #define VWIRE_MAX_TIMERS 16     // ESP8266 has more RAM
  #elif defined(ESP32)
    #define VWIRE_MAX_TIMERS 16     // ESP32 has plenty of RAM
  #else
    #define VWIRE_MAX_TIMERS 10     // Default for other boards
  #endif
#endif

// Invalid timer ID
#define VWIRE_TIMER_INVALID -1

// Infinite runs (for setInterval)
#define VWIRE_RUN_FOREVER -1

// =============================================================================
// CALLBACK TYPES
// =============================================================================

// Simple callback without arguments
typedef void (*vwire_timer_callback)(void);

// Callback with user-defined argument
typedef void (*vwire_timer_callback_arg)(void*);

// =============================================================================
// VWIRE TIMER CLASS
// =============================================================================

class VwireTimer {
public:
  /**
   * Constructor - initializes all timer slots
   */
  VwireTimer();
  
  // =========================================================================
  // TIMER CREATION METHODS
  // =========================================================================
  
  /**
   * Set a repeating interval timer
   * @param interval Time in milliseconds between calls
   * @param callback Function to call
   * @return Timer ID (0 to VWIRE_MAX_TIMERS-1) or -1 on failure
   */
  int setInterval(unsigned long interval, vwire_timer_callback callback);
  
  /**
   * Set a repeating interval timer with argument
   * @param interval Time in milliseconds between calls
   * @param callback Function to call with argument
   * @param arg User-defined argument passed to callback
   * @return Timer ID or -1 on failure
   */
  int setInterval(unsigned long interval, vwire_timer_callback_arg callback, void* arg);
  
  /**
   * Set a one-shot timeout timer
   * @param timeout Time in milliseconds until callback
   * @param callback Function to call once
   * @return Timer ID or -1 on failure
   */
  int setTimeout(unsigned long timeout, vwire_timer_callback callback);
  
  /**
   * Set a one-shot timeout timer with argument
   * @param timeout Time in milliseconds until callback
   * @param callback Function to call once with argument
   * @param arg User-defined argument passed to callback
   * @return Timer ID or -1 on failure
   */
  int setTimeout(unsigned long timeout, vwire_timer_callback_arg callback, void* arg);
  
  /**
   * Set a timer that runs a specific number of times
   * @param interval Time in milliseconds between calls
   * @param callback Function to call
   * @param numRuns Number of times to run (then auto-deletes)
   * @return Timer ID or -1 on failure
   */
  int setTimer(unsigned long interval, vwire_timer_callback callback, unsigned int numRuns);
  
  /**
   * Set a timer that runs a specific number of times with argument
   * @param interval Time in milliseconds between calls
   * @param callback Function to call with argument
   * @param arg User-defined argument passed to callback
   * @param numRuns Number of times to run (then auto-deletes)
   * @return Timer ID or -1 on failure
   */
  int setTimer(unsigned long interval, vwire_timer_callback_arg callback, void* arg, unsigned int numRuns);
  
  // =========================================================================
  // TIMER CONTROL METHODS
  // =========================================================================
  
  /**
   * Delete a timer and free its slot
   * @param timerId Timer ID returned from set* methods
   */
  void deleteTimer(int timerId);
  
  /**
   * Enable a timer (resume execution)
   * @param timerId Timer ID
   */
  void enable(int timerId);
  
  /**
   * Disable a timer (pause execution, keeps settings)
   * @param timerId Timer ID
   */
  void disable(int timerId);
  
  /**
   * Toggle a timer's enabled state
   * @param timerId Timer ID
   * @return New enabled state (true = enabled)
   */
  bool toggle(int timerId);
  
  /**
   * Restart a timer (reset the interval countdown)
   * @param timerId Timer ID
   */
  void restartTimer(int timerId);
  
  /**
   * Change the interval of an existing timer
   * @param timerId Timer ID
   * @param newInterval New interval in milliseconds
   */
  void changeInterval(int timerId, unsigned long newInterval);
  
  /**
   * Get the remaining time until next execution
   * @param timerId Timer ID
   * @return Milliseconds until next run, or 0 if invalid/disabled
   */
  unsigned long getRemaining(int timerId);
  
  // =========================================================================
  // STATUS METHODS
  // =========================================================================
  
  /**
   * Check if a timer is enabled
   * @param timerId Timer ID
   * @return true if enabled, false if disabled or invalid
   */
  bool isEnabled(int timerId);
  
  /**
   * Check if a timer slot is in use
   * @param timerId Timer ID
   * @return true if timer exists, false if slot is free
   */
  bool isValid(int timerId);
  
  /**
   * Get the number of active timers
   * @return Count of timers currently in use
   */
  int getNumTimers();
  
  /**
   * Get the number of available timer slots
   * @return Count of free slots
   */
  int getNumAvailableTimers();
  
  /**
   * Get the maximum number of timers supported
   * @return VWIRE_MAX_TIMERS
   */
  int getMaxTimers();
  
  // =========================================================================
  // EXECUTION METHOD
  // =========================================================================
  
  /**
   * Process all timers - MUST be called in loop()
   * Checks each timer and executes callbacks as needed
   */
  void run();
  
  /**
   * Delete all timers
   */
  void deleteAllTimers();

private:
  // Timer slot structure
  struct TimerSlot {
    vwire_timer_callback callback;          // Simple callback
    vwire_timer_callback_arg callbackArg;   // Callback with argument
    void* arg;                              // User argument
    unsigned long interval;                 // Interval in ms
    unsigned long lastTriggered;            // Last execution time
    int maxRuns;                            // Max runs (-1 = infinite, 0 = done)
    int currentRun;                         // Current run count
    bool enabled;                           // Timer active flag
    bool hasArg;                            // Uses callback with argument
    bool inUse;                             // Slot is occupied
  };
  
  // Timer storage
  TimerSlot _timers[VWIRE_MAX_TIMERS];
  int _numTimers;
  
  // Internal helpers
  int _findFreeSlot();
  bool _isValidId(int timerId);
  int _createTimer(unsigned long interval, int maxRuns);
};

#endif // VWIRE_TIMER_H
