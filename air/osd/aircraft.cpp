/*
 Copyright (c) 2012 - 2013 Andy Little 

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program. If not, see <http://www.gnu.org/licenses/>
*/
//#if defined(ARDUINO)
//#include <Arduino.h>
//#endif

#include "aircraft.hpp"

aircraft the_aircraft;

void aircraft::mutex_init()
{
   m_mutex = xSemaphoreCreateMutex();
}
void aircraft::mutex_acquire()
{
   xSemaphoreTake(m_mutex,portMAX_DELAY);
}
void aircraft::mutex_release()
{
   xSemaphoreGive(m_mutex);
}