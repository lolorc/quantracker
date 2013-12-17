
/*
 Copyright (c) 2013 Andy Little 

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
#include <cstdint>

#include "mavlink.hpp"
#include "gps.hpp"
#include "settings.hpp"
#include "events.hpp"
#include "resources.hpp"

extern "C" void setup();

namespace {

   void read_gps()
   { 
      the_gps.parse();
   }

   void read_data()
   {
      switch( settings::data_source){
         case settings::data_source_t::mavlink:
            read_mavlink();
         break;
         default:
            read_gps();
         break;
      }
   }
}

namespace {
   bool want_commandline()
   {
     
      quan::stm32::module_enable<posdata_txo_pin::port_type>();
      quan::stm32::apply<
         posdata_txo_pin,
         quan::stm32::gpio::mode::input,
         quan::stm32::gpio::pupd::pullup
      >();
      // delay so pullup can act.
      for ( uint32_t i =0; i < 1000; ++i){
         asm("nop");
      }
      bool result = quan::stm32::get<posdata_txo_pin>();
      // turn off pullup
       quan::stm32::apply<
         posdata_txo_pin,
         quan::stm32::gpio::pupd::none
      >();
      return result;

   }
   void do_command_line(){}
}

int main()
{

   /*
    Set TXO pin as input with pullup. Then read it.
    if it reads low then go into command line mode
   */
   if (! want_commandline() ){
      setup();
      for(;;){
         read_data();
         service_events();
      }
   }else{
      do_command_line();
   }
}
