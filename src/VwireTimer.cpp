/*
 * Vwire IOT Arduino Library - Timer Implementation
 * 
 * Non-blocking software timer using millis() for universal compatibility.
 * 
 * Copyright (c) 2026 Vwire IOT
 * Website: https://vwire.io
 * MIT License
 */

#include "VwireTimer.h"

// =============================================================================
// CONSTRUCTOR
// =============================================================================

VwireTimer::VwireTimer() {
  _numTimers = 0;
  
  // Initialize all timer slots
  for (int i = 0; i < VWIRE_MAX_TIMERS; i++) {
    _timers[i].callback = NULL;
    _timers[i].callbackArg = NULL;
    _timers[i].arg = NULL;
    _timers[i].interval = 0;
    _timers[i].lastTriggered = 0;
    _timers[i].maxRuns = 0;
    _timers[i].currentRun = 0;
    _timers[i].enabled = false;
    _timers[i].hasArg = false;
    _timers[i].inUse = false;
  }
}

// =============================================================================
// INTERNAL HELPERS
// =============================================================================

int VwireTimer::_findFreeSlot() {
  for (int i = 0; i < VWIRE_MAX_TIMERS; i++) {
    if (!_timers[i].inUse) {
      return i;
    }
  }
  return VWIRE_TIMER_INVALID;
}

bool VwireTimer::_isValidId(int timerId) {
  return (timerId >= 0 && timerId < VWIRE_MAX_TIMERS && _timers[timerId].inUse);
}

int VwireTimer::_createTimer(unsigned long interval, int maxRuns) {
  int slot = _findFreeSlot();
  
  if (slot == VWIRE_TIMER_INVALID) {
    return VWIRE_TIMER_INVALID;
  }
  
  _timers[slot].interval = interval;
  _timers[slot].lastTriggered = millis();
  _timers[slot].maxRuns = maxRuns;
  _timers[slot].currentRun = 0;
  _timers[slot].enabled = true;
  _timers[slot].inUse = true;
  _timers[slot].hasArg = false;
  _timers[slot].callback = NULL;
  _timers[slot].callbackArg = NULL;
  _timers[slot].arg = NULL;
  
  _numTimers++;
  
  return slot;
}

// =============================================================================
// TIMER CREATION - setInterval
// =============================================================================

int VwireTimer::setInterval(unsigned long interval, vwire_timer_callback callback) {
  int slot = _createTimer(interval, VWIRE_RUN_FOREVER);
  
  if (slot != VWIRE_TIMER_INVALID) {
    _timers[slot].callback = callback;
    _timers[slot].hasArg = false;
  }
  
  return slot;
}

int VwireTimer::setInterval(unsigned long interval, vwire_timer_callback_arg callback, void* arg) {
  int slot = _createTimer(interval, VWIRE_RUN_FOREVER);
  
  if (slot != VWIRE_TIMER_INVALID) {
    _timers[slot].callbackArg = callback;
    _timers[slot].arg = arg;
    _timers[slot].hasArg = true;
  }
  
  return slot;
}

// =============================================================================
// TIMER CREATION - setTimeout
// =============================================================================

int VwireTimer::setTimeout(unsigned long timeout, vwire_timer_callback callback) {
  int slot = _createTimer(timeout, 1);  // Run once
  
  if (slot != VWIRE_TIMER_INVALID) {
    _timers[slot].callback = callback;
    _timers[slot].hasArg = false;
  }
  
  return slot;
}

int VwireTimer::setTimeout(unsigned long timeout, vwire_timer_callback_arg callback, void* arg) {
  int slot = _createTimer(timeout, 1);  // Run once
  
  if (slot != VWIRE_TIMER_INVALID) {
    _timers[slot].callbackArg = callback;
    _timers[slot].arg = arg;
    _timers[slot].hasArg = true;
  }
  
  return slot;
}

// =============================================================================
// TIMER CREATION - setTimer (run N times)
// =============================================================================

int VwireTimer::setTimer(unsigned long interval, vwire_timer_callback callback, unsigned int numRuns) {
  if (numRuns == 0) {
    return VWIRE_TIMER_INVALID;
  }
  
  int slot = _createTimer(interval, (int)numRuns);
  
  if (slot != VWIRE_TIMER_INVALID) {
    _timers[slot].callback = callback;
    _timers[slot].hasArg = false;
  }
  
  return slot;
}

int VwireTimer::setTimer(unsigned long interval, vwire_timer_callback_arg callback, void* arg, unsigned int numRuns) {
  if (numRuns == 0) {
    return VWIRE_TIMER_INVALID;
  }
  
  int slot = _createTimer(interval, (int)numRuns);
  
  if (slot != VWIRE_TIMER_INVALID) {
    _timers[slot].callbackArg = callback;
    _timers[slot].arg = arg;
    _timers[slot].hasArg = true;
  }
  
  return slot;
}

// =============================================================================
// TIMER CONTROL
// =============================================================================

void VwireTimer::deleteTimer(int timerId) {
  if (!_isValidId(timerId)) {
    return;
  }
  
  _timers[timerId].callback = NULL;
  _timers[timerId].callbackArg = NULL;
  _timers[timerId].arg = NULL;
  _timers[timerId].interval = 0;
  _timers[timerId].lastTriggered = 0;
  _timers[timerId].maxRuns = 0;
  _timers[timerId].currentRun = 0;
  _timers[timerId].enabled = false;
  _timers[timerId].hasArg = false;
  _timers[timerId].inUse = false;
  
  _numTimers--;
}

void VwireTimer::enable(int timerId) {
  if (_isValidId(timerId)) {
    _timers[timerId].enabled = true;
    _timers[timerId].lastTriggered = millis();  // Reset timing
  }
}

void VwireTimer::disable(int timerId) {
  if (_isValidId(timerId)) {
    _timers[timerId].enabled = false;
  }
}

bool VwireTimer::toggle(int timerId) {
  if (!_isValidId(timerId)) {
    return false;
  }
  
  _timers[timerId].enabled = !_timers[timerId].enabled;
  
  if (_timers[timerId].enabled) {
    _timers[timerId].lastTriggered = millis();  // Reset timing on enable
  }
  
  return _timers[timerId].enabled;
}

void VwireTimer::restartTimer(int timerId) {
  if (_isValidId(timerId)) {
    _timers[timerId].lastTriggered = millis();
    _timers[timerId].currentRun = 0;
    _timers[timerId].enabled = true;
  }
}

void VwireTimer::changeInterval(int timerId, unsigned long newInterval) {
  if (_isValidId(timerId)) {
    _timers[timerId].interval = newInterval;
    _timers[timerId].lastTriggered = millis();  // Reset timing
  }
}

unsigned long VwireTimer::getRemaining(int timerId) {
  if (!_isValidId(timerId) || !_timers[timerId].enabled) {
    return 0;
  }
  
  unsigned long elapsed = millis() - _timers[timerId].lastTriggered;
  
  if (elapsed >= _timers[timerId].interval) {
    return 0;
  }
  
  return _timers[timerId].interval - elapsed;
}

// =============================================================================
// STATUS METHODS
// =============================================================================

bool VwireTimer::isEnabled(int timerId) {
  if (!_isValidId(timerId)) {
    return false;
  }
  return _timers[timerId].enabled;
}

bool VwireTimer::isValid(int timerId) {
  return _isValidId(timerId);
}

int VwireTimer::getNumTimers() {
  return _numTimers;
}

int VwireTimer::getNumAvailableTimers() {
  return VWIRE_MAX_TIMERS - _numTimers;
}

int VwireTimer::getMaxTimers() {
  return VWIRE_MAX_TIMERS;
}

// =============================================================================
// EXECUTION - Must be called in loop()
// =============================================================================

void VwireTimer::run() {
  unsigned long currentMillis = millis();
  
  for (int i = 0; i < VWIRE_MAX_TIMERS; i++) {
    // Skip unused or disabled slots
    if (!_timers[i].inUse || !_timers[i].enabled) {
      continue;
    }
    
    // Check if interval has elapsed (handles millis() overflow correctly)
    if ((unsigned long)(currentMillis - _timers[i].lastTriggered) >= _timers[i].interval) {
      
      // Update last triggered time
      _timers[i].lastTriggered = currentMillis;
      
      // Increment run count
      _timers[i].currentRun++;
      
      // Execute callback
      if (_timers[i].hasArg && _timers[i].callbackArg != NULL) {
        _timers[i].callbackArg(_timers[i].arg);
      } else if (_timers[i].callback != NULL) {
        _timers[i].callback();
      }
      
      // Check if timer should stop (not infinite and reached max runs)
      if (_timers[i].maxRuns != VWIRE_RUN_FOREVER && 
          _timers[i].currentRun >= _timers[i].maxRuns) {
        // Auto-delete timer that has finished
        deleteTimer(i);
      }
    }
  }
}

void VwireTimer::deleteAllTimers() {
  for (int i = 0; i < VWIRE_MAX_TIMERS; i++) {
    if (_timers[i].inUse) {
      deleteTimer(i);
    }
  }
}
