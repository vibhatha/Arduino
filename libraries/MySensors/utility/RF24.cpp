/*
 Copyright (C) 2011 J. Coliz <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */

#include "nRF24L01.h"
#include "RF24_config.h"
#include "RF24.h"

/****************************************************************************/
static void print_hex( uint8_t v, const bool prefix0x = false )
{
  if (prefix0x)
    Serial.print(F("0x"));
  
  Serial.print(v >> 4, HEX);
  Serial.print(v & 0x0F, HEX);
}

void RF24::csn(bool mode)
{
  // Minimum ideal SPI bus speed is 2x data rate
  // If we assume 2Mbs data rate and 16Mhz clock, a
  // divider of 4 is the minimum we want.
  // CLK:BUS 8Mhz:2Mhz, 16Mhz:4Mhz, or 20Mhz:5Mhz
#ifdef ARDUINO
	#if  ( !defined(RF24_TINY) && !defined (__arm__)  && !defined (SOFTSPI)) || defined (CORE_TEENSY)
 			_SPI.setBitOrder(MSBFIRST);
  			_SPI.setDataMode(SPI_MODE0);
			_SPI.setClockDivider(SPI_CLOCK_DIV2);
	#endif
#endif


#if defined (RF24_TINY)
	if (ce_pin != csn_pin) {
		digitalWrite(csn_pin,mode);
	} 
	else {
		if (mode == HIGH) {
			PORTB |= (1<<PINB2);  	// SCK->CSN HIGH
			delayMicroseconds(100); // allow csn to settle.
		} 
		else {
			PORTB &= ~(1<<PINB2);	// SCK->CSN LOW
			delayMicroseconds(11);  // allow csn to settle
		}
	}		
#elif !defined  (__arm__) || defined (CORE_TEENSY)
	digitalWrite(csn_pin,mode);		
#endif

}

/****************************************************************************/

void RF24::ce(bool level)
{
  //Allow for 3-pin use on ATTiny
  if (ce_pin != csn_pin) digitalWrite(ce_pin,level);
}

/****************************************************************************/

uint8_t RF24::read_register(uint8_t reg, uint8_t* buf, uint8_t len)
{
  uint8_t status;

#if defined (__arm__) && !defined ( CORE_TEENSY )
  status = _SPI.transfer(csn_pin, R_REGISTER | ( REGISTER_MASK & reg ), SPI_CONTINUE );
  while ( len-- > 1 ){
    *buf++ = _SPI.transfer(csn_pin,0xff, SPI_CONTINUE);
  }
  *buf++ = _SPI.transfer(csn_pin,0xff);

#else
  csn(LOW);
  status = _SPI.transfer( R_REGISTER | ( REGISTER_MASK & reg ) );
  while ( len-- ){
    *buf++ = _SPI.transfer(0xff);
  }
  csn(HIGH);

#endif

  return status;
}

/****************************************************************************/

uint8_t RF24::read_register(uint8_t reg)
{

  #if defined (__arm__) && !defined ( CORE_TEENSY )
  _SPI.transfer(csn_pin, R_REGISTER | ( REGISTER_MASK & reg ) , SPI_CONTINUE);
  uint8_t result = _SPI.transfer(csn_pin,0xff);
  #else
  csn(LOW);
  _SPI.transfer( R_REGISTER | ( REGISTER_MASK & reg ) );
  uint8_t result = _SPI.transfer(0xff);

  csn(HIGH);
  #endif

  return result;
}

/****************************************************************************/

uint8_t RF24::write_register(uint8_t reg, const uint8_t* buf, uint8_t len)
{
  uint8_t status;

  #if defined (__arm__) && !defined ( CORE_TEENSY )
  	status = _SPI.transfer(csn_pin, W_REGISTER | ( REGISTER_MASK & reg ), SPI_CONTINUE );
    while ( --len){
    	_SPI.transfer(csn_pin,*buf++, SPI_CONTINUE);
	}
	_SPI.transfer(csn_pin,*buf++);
  #else

  csn(LOW);
  status = _SPI.transfer( W_REGISTER | ( REGISTER_MASK & reg ) );
  while ( len-- )
    _SPI.transfer(*buf++);

  csn(HIGH);

  #endif

  return status;
}

/****************************************************************************/

uint8_t RF24::write_register(uint8_t reg, uint8_t value)
{
  uint8_t status;

#ifdef DEBUG
  Serial.print(F("write_register(")); print_hex(reg, true); Serial.print(F(",")); print_hex(value, true); Serial.println(F(")")); 
#endif

#if defined (__arm__) && !defined ( CORE_TEENSY )
  status = _SPI.transfer(csn_pin, W_REGISTER | ( REGISTER_MASK & reg ), SPI_CONTINUE);
  _SPI.transfer(csn_pin,value);
#else
  csn(LOW);
  status = _SPI.transfer( W_REGISTER | ( REGISTER_MASK & reg ) );
  _SPI.transfer(value);
  csn(HIGH);
#endif

  return status;
}

/****************************************************************************/

uint8_t RF24::write_payload(const void* buf, uint8_t data_len, const uint8_t writeType)
{
  uint8_t status;
  const uint8_t* current = reinterpret_cast<const uint8_t*>(buf);

   data_len = min(data_len, payload_size);
   uint8_t blank_len = dynamic_payloads_enabled ? 0 : payload_size - data_len;
  
  //printf("[Writing %u bytes %u blanks]",data_len,blank_len);

 #if defined (__arm__) && !defined ( CORE_TEENSY )

  status = _SPI.transfer(csn_pin, writeType , SPI_CONTINUE);

  if(blank_len){
    while ( data_len--){
      _SPI.transfer(csn_pin,*current++, SPI_CONTINUE);
    }
    while ( --blank_len ){
      _SPI.transfer(csn_pin,0, SPI_CONTINUE);
    }
    _SPI.transfer(csn_pin,0);
  }else{
    while( --data_len ){
      _SPI.transfer(csn_pin,*current++, SPI_CONTINUE);
    }
    _SPI.transfer(csn_pin,*current);
  }

  #else

  csn(LOW);
  status = _SPI.transfer( writeType );
  while ( data_len-- ) {
    _SPI.transfer(*current++);
  }
  while ( blank_len-- ) {
    _SPI.transfer(0);
  }
  csn(HIGH);

  #endif

  return status;
}

/****************************************************************************/

uint8_t RF24::read_payload(void* buf, uint8_t data_len)
{
  uint8_t status;
  uint8_t* current = reinterpret_cast<uint8_t*>(buf);

  if(data_len > payload_size) data_len = payload_size;
  uint8_t blank_len = dynamic_payloads_enabled ? 0 : payload_size - data_len;
  
  //printf("[Reading %u bytes %u blanks]",data_len,blank_len);


  #if defined (__arm__) && !defined ( CORE_TEENSY )

  status = _SPI.transfer(csn_pin, R_RX_PAYLOAD, SPI_CONTINUE );

  if( blank_len ){
	while ( data_len-- ){
      *current++ = _SPI.transfer(csn_pin,0xFF, SPI_CONTINUE);
	}

	while ( --blank_len ){
	  _SPI.transfer(csn_pin,0xFF, SPI_CONTINUE);
	}
	_SPI.transfer(csn_pin,0xFF);
  }else{
	while ( --data_len ){
	  *current++ = _SPI.transfer(csn_pin,0xFF, SPI_CONTINUE);
	}
	*current = _SPI.transfer(csn_pin,0xFF);
  }

  #else

  csn(LOW);
  status = _SPI.transfer( R_RX_PAYLOAD );
  while ( data_len-- ) {
    *current++ = _SPI.transfer(0xFF);
  }
  while ( blank_len-- ) {
    _SPI.transfer(0xff);
  }
  csn(HIGH);

  #endif

  return status;
}

/****************************************************************************/

uint8_t RF24::flush_rx(void)
{
  return spiTrans( FLUSH_RX );
}

/****************************************************************************/

uint8_t RF24::flush_tx(void)
{
  return spiTrans( FLUSH_TX );
}

/****************************************************************************/

uint8_t RF24::spiTrans(uint8_t cmd){

  uint8_t status;
  #if defined (__arm__) && !defined ( CORE_TEENSY )
	status = _SPI.transfer(csn_pin, cmd );
  #else

  csn(LOW);
  status = _SPI.transfer( cmd );
  csn(HIGH);
  #endif
  return status;

}

/****************************************************************************/

uint8_t RF24::get_status(void)
{
  return spiTrans(NOP);
}

/****************************************************************************/
void RF24::print_feature(void)
{
#ifdef DEBUG
  Serial.print(F("FEATURE="));
  print_hex(read_register(FEATURE), true);
  Serial.println(F("")); 
#endif
}

/****************************************************************************/
void RF24::print_status(uint8_t status) const
{
#ifdef MY_DEBUG_VERBOSE
  const uint8_t one  = uint8_t(1);
  const uint8_t zero = uint8_t(0);
  print_hex(status, true);
  Serial.print(F(" RX_DR="));   Serial.print((status & _BV(RX_DR))     ? one : zero );
  Serial.print(F(" TX_DS="));   Serial.print((status & _BV(TX_DS))     ? one : zero );
  Serial.print(F(" MAX_RT="));  Serial.print((status & _BV(MAX_RT))    ? one : zero );
  Serial.print(F(" RX_P_NO=")); Serial.print((status >> RX_P_NO) & uint8_t(B111));
  Serial.print(F(" TX_FULL=")); Serial.println((status & _BV(TX_FULL)) ? one : zero );
#else
  (void)status;
#endif
}

/****************************************************************************/
void RF24::print_observe_tx(uint8_t value) const
{
#ifdef MY_DEBUG_VERBOSE
  print_hex(value, true);
  Serial.print(F(": POLS_CNT=")); Serial.print((value >> PLOS_CNT)   & uint8_t(B1111));
  Serial.print(F(" ARC_CNT="));   Serial.println((value >> ARC_CNT)  & uint8_t(B1111));
#else
  (void)value;
#endif
}

/****************************************************************************/
void RF24::print_byte_register(uint8_t reg, uint8_t qty)
{
#ifdef MY_DEBUG_VERBOSE
  bool prefix0x = true;
  while (qty--)
  {
    print_hex(read_register(reg++), prefix0x);
    Serial.print(F(" ")); 
    prefix0x = false;
  }
  Serial.println(F(""));
#else
  (void)reg;
  (void)qty;
#endif
}

/****************************************************************************/
void RF24::print_address_register(uint8_t reg, uint8_t qty)
{
#ifdef MY_DEBUG_VERBOSE
  bool prefix0x = true;
  while (qty--)
  {
    uint8_t buffer[addr_width];
    read_register(reg++,buffer,sizeof buffer);

    uint8_t* bufptr = buffer + sizeof buffer;
    while( --bufptr >= buffer )
    {
      print_hex(read_register(*bufptr), prefix0x);
      prefix0x = false;
    }
    Serial.print(F(" "));
  }
  Serial.println(F(""));
#else
  (void)reg;
  (void)qty;
#endif
}
/****************************************************************************/

RF24::RF24(uint8_t _cepin, uint8_t _cspin):
  ce_pin(_cepin), csn_pin(_cspin), p_variant(false),
  payload_size(32), dynamic_payloads_enabled(false), addr_width(5)//,pipe0_reading_address(0)
{
}

/****************************************************************************/

void RF24::setChannel(uint8_t channel)
{
  const uint8_t max_channel = 127;
  write_register(RF_CH,min(channel,max_channel));
}

/****************************************************************************/

void RF24::setPayloadSize(uint8_t size)
{
  payload_size = min(size,32);
}

/****************************************************************************/

uint8_t RF24::getPayloadSize(void)
{
  return payload_size;
}

/****************************************************************************/
#ifdef MY_DEBUG_VERBOSE
static const char rf24_datarate_e_str_0[] PROGMEM = "1MBPS";
static const char rf24_datarate_e_str_1[] PROGMEM = "2MBPS";
static const char rf24_datarate_e_str_2[] PROGMEM = "250KBPS";
static const char * const rf24_datarate_e_str_P[] PROGMEM = {
  rf24_datarate_e_str_0,
  rf24_datarate_e_str_1,
  rf24_datarate_e_str_2,
};
static const char rf24_model_e_str_0[] PROGMEM = "nRF24L01";
static const char rf24_model_e_str_1[] PROGMEM = "nRF24L01+";
static const char * const rf24_model_e_str_P[] PROGMEM = {
  rf24_model_e_str_0,
  rf24_model_e_str_1,
};
static const char rf24_crclength_e_str_0[] PROGMEM = "Disabled";
static const char rf24_crclength_e_str_1[] PROGMEM = "8 bits";
static const char rf24_crclength_e_str_2[] PROGMEM = "16 bits" ;
static const char * const rf24_crclength_e_str_P[] PROGMEM = {
  rf24_crclength_e_str_0,
  rf24_crclength_e_str_1,
  rf24_crclength_e_str_2,
};
static const char rf24_pa_dbm_e_str_0[] PROGMEM = "PA_MIN";
static const char rf24_pa_dbm_e_str_1[] PROGMEM = "PA_LOW";
static const char rf24_pa_dbm_e_str_2[] PROGMEM = "PA_HIGH";
static const char rf24_pa_dbm_e_str_3[] PROGMEM = "PA_MAX";
static const char * const rf24_pa_dbm_e_str_P[] PROGMEM = {
  rf24_pa_dbm_e_str_0,
  rf24_pa_dbm_e_str_1,
  rf24_pa_dbm_e_str_2,
  rf24_pa_dbm_e_str_3,
};
#endif

#if (INTPTR_MAX == INT16_MAX)
// 16-bit architecture -> read word from flash and cast to ptr
#define PGM_READ_PTR(adr)  (reinterpret_cast<__FlashStringHelper*>(pgm_read_word(adr)))
#elif (INTPTR_MAX == INT32_MAX)
// 32-bit architecture -> read double-word from flash and cast to ptr
#define PGM_READ_PTR(adr)  (reinterpret_cast<__FlashStringHelper*>(pgm_read_dword(adr)))
#else
#error Unsupported processor architecture!
#endif

void RF24::printDetails(void)
{
#ifdef MY_DEBUG_VERBOSE
  Serial.print(F("STATUS\t\t"));      print_status(get_status());
  Serial.print(F("RX_ADDR_P0-1\t"));  print_address_register(RX_ADDR_P0,2);
  Serial.print(F("RX_ADDR_P2-5\t"));  print_byte_register(RX_ADDR_P2,4);
  Serial.print(F("TX_ADDR\t\t"));     print_address_register(TX_ADDR);

  Serial.print(F("RX_PW_P0-6\t"));    print_byte_register(RX_PW_P0,6);
  Serial.print(F("EN_AA\t\t"));       print_byte_register(EN_AA);
  Serial.print(F("EN_RXADDR\t"));     print_byte_register(EN_RXADDR);
  Serial.print(F("RF_CH\t\t"));       print_byte_register(RF_CH);
  Serial.print(F("RF_SETUP\t"));      print_byte_register(RF_SETUP);
  Serial.print(F("CONFIG\t\t"));      print_byte_register(CONFIG);
  Serial.print(F("DYNPD/FEATURE\t")); print_byte_register(DYNPD,2);

  Serial.print(F("Data Rate\t"));     Serial.println(PGM_READ_PTR(&rf24_datarate_e_str_P[getDataRate()]));
  Serial.print(F("Model\t\t"));       Serial.println(PGM_READ_PTR(&rf24_model_e_str_P[isPVariant()]));
  Serial.print(F("CRC Length\t"));    Serial.println(PGM_READ_PTR(&rf24_crclength_e_str_P[getCRCLength()]));
  Serial.print(F("PA Power\t"));      Serial.println(PGM_READ_PTR(&rf24_pa_dbm_e_str_P[getPALevel()]));
#endif
}
/****************************************************************************/

void RF24::begin(void)
{
  // Initialize pins
  if (ce_pin != csn_pin) pinMode(ce_pin,OUTPUT);

  #if defined(__arm__) && ! defined( CORE_TEENSY )
  	_SPI.begin(csn_pin);					// Using the extended SPI features of the DUE
	_SPI.setClockDivider(csn_pin, 9);   // Set the bus speed to 8.4mhz on Due
	_SPI.setBitOrder(csn_pin,MSBFIRST);	// Set the bit order and mode specific to this device
  	_SPI.setDataMode(csn_pin,SPI_MODE0);
	ce(LOW);
  	//csn(HIGH);
  #else
    if (ce_pin != csn_pin) pinMode(csn_pin,OUTPUT);
    _SPI.begin();
    ce(LOW);
  	csn(HIGH);
  #endif

  // Must allow the radio time to settle else configuration bits will not necessarily stick.
  // This is actually only required following power up but some settling time also appears to
  // be required after resets too. For full coverage, we'll always assume the worst.
  // Enabling 16b CRC is by far the most obvious case if the wrong timing is used - or skipped.
  // Technically we require 4.5ms + 14us as a worst case. We'll just call it 5ms for good measure.
  // WARNING: Delay is based on P-variant whereby non-P *may* require different timing.
  delay( 5 ) ;

  // Set 1500uS (minimum for 32B payload in ESB@250KBPS) timeouts, to make testing a little easier
  // WARNING: If this is ever lowered, either 250KBS mode with AA is broken or maximum packet
  // sizes must never be used. See documentation for a more complete explanation.
  setRetries(5,15);

  // Reset value is MAX
  //setPALevel( RF24_PA_MAX ) ;

  // Determine if this is a p or non-p RF24 module and then
  // reset our data rate back to default value. This works
  // because a non-P variant won't allow the data rate to
  // be set to 250Kbps.
  if( setDataRate( RF24_250KBPS ) )
  {
    p_variant = true ;
  }

  // Then set the data rate to the slowest (and most reliable) speed supported by all
  // hardware.
  setDataRate( RF24_1MBPS ) ;

  // Initialize CRC and request 2-byte (16bit) CRC
  setCRCLength( RF24_CRC_16 ) ;

  // Disable dynamic payloads, to match dynamic_payloads_enabled setting - Reset value is 0
  //write_register(DYNPD,0);

  // Reset current status
  // Notice reset and flush is the last thing we do
  write_register(RF24_STATUS,_BV(RX_DR) | _BV(TX_DS) | _BV(MAX_RT) );

  // Set up default configuration.  Callers can always change it later.
  // This channel should be universally safe and not bleed over into adjacent
  // spectrum.
  setChannel(76);

  // Flush buffers
  flush_rx();
  flush_tx();

  powerUp(); //Power up by default when begin() is called

  // Enable PTX, do not write CE high so radio will remain in standby I mode ( 130us max to transition to RX or TX instead of 1500us from powerUp )
  // PTX should use only 22uA of power
  write_register(CONFIG, ( read_register(CONFIG) ) & ~_BV(PRIM_RX) );

  // Dump register values
  printDetails();
}

/****************************************************************************/

void RF24::startListening(void)
{
 #if !defined (RF24_TINY)
  powerUp();
 #endif
  write_register(CONFIG, read_register(CONFIG) | _BV(PRIM_RX));
  write_register(RF24_STATUS, _BV(RX_DR) | _BV(TX_DS) | _BV(MAX_RT) );

  // Restore the pipe0 adddress, if exists
  if (pipe0_reading_address[0] > 0) {
    write_register(RX_ADDR_P0, pipe0_reading_address, addr_width);
  } else {
    closeReadingPipe(0);
  }

  // Flush buffers
  //flush_rx();
  if(read_register(FEATURE) & _BV(EN_ACK_PAY)) {
    flush_tx();
  }

  // Go!
  ce(HIGH);
}

/****************************************************************************/
static uint8_t get_child_pipe_mask(const uint8_t idx)
{
#ifdef ESP8266
  static const uint8_t child_pipe_enable[] =
  {
    _BV(ERX_P0), _BV(ERX_P1), _BV(ERX_P2), _BV(ERX_P3), _BV(ERX_P4), _BV(ERX_P5)
  };
  return child_pipe_enable[idx];
#else
  static const uint8_t child_pipe_enable[] PROGMEM =
  {
    _BV(ERX_P0), _BV(ERX_P1), _BV(ERX_P2), _BV(ERX_P3), _BV(ERX_P4), _BV(ERX_P5)
  };
  return pgm_read_byte(&child_pipe_enable[idx]);
#endif
}

void RF24::stopListening(void)
{
  ce(LOW);
#if defined(__arm__)
  delayMicroseconds(300);
#endif
  delayMicroseconds(130);

  if(read_register(FEATURE) & _BV(EN_ACK_PAY)) {
    flush_tx();
  }

  //flush_rx();
  write_register(CONFIG, ( read_register(CONFIG) ) & ~_BV(PRIM_RX) );
 
  #if defined (RF24_TINY)
  // for 3 pins solution TX mode is only left with additonal powerDown/powerUp cycle
  if (ce_pin == csn_pin) {
    powerDown();
    powerUp();
  }
  #endif

  write_register(EN_RXADDR,read_register(EN_RXADDR) | get_child_pipe_mask(0)); // Enable RX on pipe0
  
  delayMicroseconds(100);
}

/****************************************************************************/

void RF24::powerDown(void)
{
  ce(LOW); // Guarantee CE is low on powerDown
  write_register(CONFIG,read_register(CONFIG) & ~_BV(PWR_UP));
}

/****************************************************************************/

//Power up now. Radio will not power down unless instructed by MCU for config changes etc.
void RF24::powerUp(void)
{
   uint8_t cfg = read_register(CONFIG);

   // if not powered up then power up and wait for the radio to initialize
   if (!(cfg & _BV(PWR_UP))){
      write_register(CONFIG,read_register(CONFIG) | _BV(PWR_UP));

      // For nRF24L01+ to go from power down mode to TX or RX mode it must first pass through stand-by mode.
	  // There must be a delay of Tpd2stby (see Table 16.) after the nRF24L01+ leaves power down mode before
	  // the CEis set high. - Tpd2stby can be up to 5ms per the 1.0 datasheet
      delay(5);

   }
}

/******************************************************************/
#if defined (FAILURE_HANDLING)
void RF24::errNotify(){
#ifdef DEBUG
  Serial.println(F("HARDWARE FAIL"));
#endif
  failureDetected = 1;
}
#endif
/******************************************************************/

//Similar to the previous write, clears the interrupt flags
bool RF24::write( const void* buf, uint8_t len, const bool multicast )
{
	//Start Writing
	startFastWrite(buf,len,multicast);

	//Wait until complete or failed
	#if defined (FAILURE_HANDLING)
		uint32_t timer = millis();
	#endif 
	
	while( ! ( get_status()  & ( _BV(TX_DS) | _BV(MAX_RT) ))) { 
		#if defined (FAILURE_HANDLING)
			if(millis() - timer > 75){			
				errNotify();
				return 0;							
			}
		#endif
	}
    
	ce(LOW);

	uint8_t status = write_register(RF24_STATUS,_BV(RX_DR) | _BV(TX_DS) | _BV(MAX_RT) );

  //Max retries exceeded
  if( status & _BV(MAX_RT)){
  	flush_tx(); //Only going to be 1 packet int the FIFO at a time using this method, so just flush
  	return 0;
  }
	//TX OK 1 or 0
  return 1;
}

bool RF24::write( const void* buf, uint8_t len ){
	return write(buf,len,0);
}
/****************************************************************************/

//For general use, the interrupt flags are not important to clear
bool RF24::writeBlocking( const void* buf, uint8_t len, uint32_t timeout )
{
	//Block until the FIFO is NOT full.
	//Keep track of the MAX retries and set auto-retry if seeing failures
	//This way the FIFO will fill up and allow blocking until packets go through
	//The radio will auto-clear everything in the FIFO as long as CE remains high

	uint32_t timer = millis();							  //Get the time that the payload transmission started

	while( ( get_status()  & ( _BV(TX_FULL) ))) {		  //Blocking only if FIFO is full. This will loop and block until TX is successful or timeout

		if( get_status() & _BV(MAX_RT)){					  //If MAX Retries have been reached
			reUseTX();										  //Set re-transmit and clear the MAX_RT interrupt flag
			if(millis() - timer > timeout){ return 0; }		  //If this payload has exceeded the user-defined timeout, exit and return 0
		}
		#if defined (FAILURE_HANDLING)
			if(millis() - timer > (timeout+75) ){			
				errNotify();
				return 0;							
			}
		#endif

  	}

  	//Start Writing
	startFastWrite(buf,len,0);								  //Write the payload if a buffer is clear

	return 1;												  //Return 1 to indicate successful transmission
}

/****************************************************************************/

void RF24::reUseTX(){
		write_register(RF24_STATUS,_BV(MAX_RT) );			  //Clear max retry flag
		spiTrans( REUSE_TX_PL );
		ce(LOW);										  //Re-Transfer packet
		ce(HIGH);
}

/****************************************************************************/

bool RF24::writeFast( const void* buf, uint8_t len, const bool multicast )
{
	//Block until the FIFO is NOT full.
	//Keep track of the MAX retries and set auto-retry if seeing failures
	//Return 0 so the user can control the retrys and set a timer or failure counter if required
	//The radio will auto-clear everything in the FIFO as long as CE remains high

	#if defined (FAILURE_HANDLING)
		uint32_t timer = millis();
	#endif
	
	while( ( get_status()  & ( _BV(TX_FULL) ))) {			  //Blocking only if FIFO is full. This will loop and block until TX is successful or fail

		if( get_status() & _BV(MAX_RT)){
			//reUseTX();										  //Set re-transmit
			write_register(RF24_STATUS,_BV(MAX_RT) );			  //Clear max retry flag
			return 0;										  //Return 0. The previous payload has been retransmitted
															  //From the user perspective, if you get a 0, just keep trying to send the same payload
		}
		#if defined (FAILURE_HANDLING)
			if(millis() - timer > 75 ){			
				errNotify();
				return 0;							
			}
		#endif
  	}
		     //Start Writing
	startFastWrite(buf,len,multicast);

	return 1;
}

bool RF24::writeFast( const void* buf, uint8_t len ){
	return writeFast(buf,len,0);
}

/****************************************************************************/

//Per the documentation, we want to set PTX Mode when not listening. Then all we do is write data and set CE high
//In this mode, if we can keep the FIFO buffers loaded, packets will transmit immediately (no 130us delay)
//Otherwise we enter Standby-II mode, which is still faster than standby mode
//Also, we remove the need to keep writing the config register over and over and delaying for 150 us each time if sending a stream of data

void RF24::startFastWrite( const void* buf, uint8_t len, const bool multicast){ //TMRh20

	//write_payload( buf,len);
	write_payload( buf, len,multicast ? W_TX_PAYLOAD_NO_ACK : W_TX_PAYLOAD ) ;
	ce(HIGH);

}

/****************************************************************************/

//Added the original startWrite back in so users can still use interrupts, ack payloads, etc
//Allows the library to pass all tests
void RF24::startWrite( const void* buf, uint8_t len, const bool multicast ){

  // Send the payload

  //write_payload( buf, len );
  write_payload( buf, len,multicast? W_TX_PAYLOAD_NO_ACK : W_TX_PAYLOAD ) ;
  ce(HIGH);
  #if defined(CORE_TEENSY) || !defined(ARDUINO)
	delayMicroseconds(10);
  #endif
  ce(LOW);


}

/****************************************************************************/

bool RF24::rxFifoFull(){
	return read_register(FIFO_STATUS) & _BV(RX_FULL);
}
/****************************************************************************/

bool RF24::txStandBy(){
    #if defined (FAILURE_HANDLING)
		uint32_t timeout = millis();
	#endif
	while( ! (read_register(FIFO_STATUS) & _BV(TX_EMPTY)) ){
		if( get_status() & _BV(MAX_RT)){
			write_register(RF24_STATUS,_BV(MAX_RT) );
			ce(LOW);
			flush_tx();    //Non blocking, flush the data
			return 0;
		}
		#if defined (FAILURE_HANDLING)
			if( millis() - timeout > 75){
				errNotify();
				return 0;	
			}
		#endif
	}

	ce(LOW);			   //Set STANDBY-I mode
	return 1;
}

/****************************************************************************/

bool RF24::txStandBy(uint32_t timeout){

	uint32_t start = millis();

	while( ! (read_register(FIFO_STATUS) & _BV(TX_EMPTY)) ){
		if( get_status() & _BV(MAX_RT)){
			write_register(RF24_STATUS,_BV(MAX_RT) );
				ce(LOW);										  //Set re-transmit
				ce(HIGH);
				if(millis() - start >= timeout){
					ce(LOW); flush_tx(); return 0;
				}
		}
		#if defined (FAILURE_HANDLING)
			if( millis() - start > (timeout+75)){
				errNotify();
				return 0;	
			}
		#endif
	}

	
	ce(LOW);				   //Set STANDBY-I mode
	return 1;

}
/****************************************************************************/

void RF24::maskIRQ(bool tx, bool fail, bool rx){

	write_register(CONFIG, ( read_register(CONFIG) ) | fail << MASK_MAX_RT | tx << MASK_TX_DS | rx << MASK_RX_DR  );
}

/****************************************************************************/

uint8_t RF24::getDynamicPayloadSize(void)
{
  uint8_t result = 0;

  #if defined (__arm__) && ! defined( CORE_TEENSY )
  _SPI.transfer(csn_pin, R_RX_PL_WID, SPI_CONTINUE );
  result = _SPI.transfer(csn_pin,0xff);
  #else
  csn(LOW);
  _SPI.transfer( R_RX_PL_WID );
  result = _SPI.transfer(0xff);
  csn(HIGH);

  #endif

  if(result > 32) { flush_rx(); delay(2); return 0; }
  return result;
}

/****************************************************************************/

bool RF24::available(void)
{
  return available(NULL);
}

/****************************************************************************/

bool RF24::available(uint8_t* pipe_num)
{
    //Check the FIFO buffer to see if data is waiting to be read
	if(listeningStarted){
		while(micros() - lastAvailableCheck < 800 && listeningStarted){};
		lastAvailableCheck = micros();
		listeningStarted = 0;
	}
  if (!( read_register(FIFO_STATUS) & _BV(RX_EMPTY) )){

    // If the caller wants the pipe number, include that
    if ( pipe_num ){
	  uint8_t status = get_status();
      *pipe_num = ( status >> RX_P_NO ) & B111;
  	}
  	return 1;
  }


  return 0;


}

/****************************************************************************/

void RF24::read( void* buf, uint8_t len ){

  // Fetch the payload
  read_payload( buf, len );

  //Clear the two possible interrupt flags with one command
  write_register(RF24_STATUS,_BV(RX_DR) | _BV(MAX_RT) | _BV(TX_DS) );

}

/****************************************************************************/

void RF24::whatHappened(bool& tx_ok,bool& tx_fail,bool& rx_ready)
{
  // Read the status & reset the status in one easy call
  // Or is that such a good idea?
  uint8_t status = write_register(RF24_STATUS,_BV(RX_DR) | _BV(TX_DS) | _BV(MAX_RT) );

  // Report to the user what happened
  tx_ok = status & _BV(TX_DS);
  tx_fail = status & _BV(MAX_RT);
  rx_ready = status & _BV(RX_DR);
}

/****************************************************************************/

void RF24::openWritingPipe(uint64_t value)
{
  // Note that AVR 8-bit uC's store this LSB first, and the NRF24L01(+)
  // expects it LSB first too, so we're good.

  write_register(RX_ADDR_P0, reinterpret_cast<uint8_t*>(&value), addr_width);
  write_register(TX_ADDR, reinterpret_cast<uint8_t*>(&value), addr_width);
  
  
  //const uint8_t max_payload_size = 32;
  //write_register(RX_PW_P0,min(payload_size,max_payload_size));
  write_register(RX_PW_P0,payload_size);
}

/****************************************************************************/
void RF24::openWritingPipe(const uint8_t *address)
{
  // Note that AVR 8-bit uC's store this LSB first, and the NRF24L01(+)
  // expects it LSB first too, so we're good.

  write_register(RX_ADDR_P0,address, addr_width);
  write_register(TX_ADDR, address, addr_width);

  //const uint8_t max_payload_size = 32;
  //write_register(RX_PW_P0,min(payload_size,max_payload_size));
  write_register(RX_PW_P0,payload_size);
}

/****************************************************************************/
static const uint8_t child_pipe[] PROGMEM =
{
  RX_ADDR_P0, RX_ADDR_P1, RX_ADDR_P2, RX_ADDR_P3, RX_ADDR_P4, RX_ADDR_P5
};
static const uint8_t child_payload_size[] PROGMEM =
{
  RX_PW_P0, RX_PW_P1, RX_PW_P2, RX_PW_P3, RX_PW_P4, RX_PW_P5
};


void RF24::openReadingPipe(uint8_t child, uint64_t address)
{
  // If this is pipe 0, cache the address.  This is needed because
  // openWritingPipe() will overwrite the pipe 0 address, so
  // startListening() will have to restore it.
  if (child == 0){
    memcpy(pipe0_reading_address,&address,addr_width);
  }

  if (child <= 6)
  {
    // For pipes 2-5, only write the LSB
    if ( child < 2 )
      write_register(pgm_read_byte(&child_pipe[child]), reinterpret_cast<const uint8_t*>(&address), addr_width);
    else
      write_register(pgm_read_byte(&child_pipe[child]), reinterpret_cast<const uint8_t*>(&address), 1);

    write_register(pgm_read_byte(&child_payload_size[child]),payload_size);

    // Note it would be more efficient to set all of the bits for all open
    // pipes at once.  However, I thought it would make the calling code
    // more simple to do it this way.
    write_register(EN_RXADDR,read_register(EN_RXADDR) | get_child_pipe_mask(child));
  }
}

/****************************************************************************/
void RF24::setAddressWidth(uint8_t a_width){

	if(a_width -= 2){
		write_register(SETUP_AW,a_width%4);
		addr_width = (a_width%4) + 2;
	}

}

/****************************************************************************/

void RF24::openReadingPipe(uint8_t child, const uint8_t *address)
{
  // If this is pipe 0, cache the address.  This is needed because
  // openWritingPipe() will overwrite the pipe 0 address, so
  // startListening() will have to restore it.
  if (child == 0){
    memcpy(pipe0_reading_address,address,addr_width);
  }
  if (child <= 6)
  {
    // For pipes 2-5, only write the LSB
    if ( child < 2 ){
      write_register(pgm_read_byte(&child_pipe[child]), address, addr_width);
    }else{
      write_register(pgm_read_byte(&child_pipe[child]), address, 1);
	}
    write_register(pgm_read_byte(&child_payload_size[child]),payload_size);

    // Note it would be more efficient to set all of the bits for all open
    // pipes at once.  However, I thought it would make the calling code
    // more simple to do it this way.
    write_register(EN_RXADDR,read_register(EN_RXADDR) | get_child_pipe_mask(child));

  }
}

/****************************************************************************/

void RF24::closeReadingPipe( uint8_t pipe )
{
  write_register(EN_RXADDR,read_register(EN_RXADDR) & ~get_child_pipe_mask(pipe));
}

/****************************************************************************/

void RF24::toggle_features(void)
{

  #if defined (__arm__) && ! defined( CORE_TEENSY )
  _SPI.transfer(csn_pin, ACTIVATE, SPI_CONTINUE );
  _SPI.transfer(csn_pin, 0x73 );
  #else
  csn(LOW);
  _SPI.transfer( ACTIVATE );
  _SPI.transfer( 0x73 );
  csn(HIGH);
  #endif
}

/****************************************************************************/
void RF24::enableDynamicPayloads(void)
{
  // Enable dynamic payload throughout the system
  toggle_features();
  write_register(FEATURE,read_register(FEATURE) | _BV(EN_DPL) );
  print_feature();

  // Enable dynamic payload on all pipes
  //
  // Not sure the use case of only having dynamic payload on certain
  // pipes, so the library does not support it.
  write_register(DYNPD,read_register(DYNPD) | _BV(DPL_P5) | _BV(DPL_P4) | _BV(DPL_P3) | _BV(DPL_P2) | _BV(DPL_P1) | _BV(DPL_P0));

  dynamic_payloads_enabled = true;
}

/****************************************************************************/

void RF24::enableAckPayload(void)
{
  //
  // enable ack payload and dynamic payload features
  //
  toggle_features();
  write_register(FEATURE,read_register(FEATURE) | _BV(EN_ACK_PAY) | _BV(EN_DPL) );
  print_feature();

  //
  // Enable dynamic payload on pipes 0 & 1
  //

  write_register(DYNPD,read_register(DYNPD) | _BV(DPL_P1) | _BV(DPL_P0));
  dynamic_payloads_enabled = true;
}

/****************************************************************************/

void RF24::enableDynamicAck(void){
  //
  // enable dynamic ack features
  //
  toggle_features();
  write_register(FEATURE,read_register(FEATURE) | _BV(EN_DYN_ACK) );
  print_feature();
}

/****************************************************************************/

void RF24::writeAckPayload(uint8_t pipe, const void* buf, uint8_t len)
{
  const uint8_t* current = reinterpret_cast<const uint8_t*>(buf);

  uint8_t data_len = min(len,32);

  #if defined (__arm__) && ! defined( CORE_TEENSY )
	_SPI.transfer(csn_pin, W_ACK_PAYLOAD | ( pipe & B111 ), SPI_CONTINUE);
	while ( data_len-- > 1 ){
		_SPI.transfer(csn_pin,*current++, SPI_CONTINUE);
	}
	_SPI.transfer(csn_pin,*current++);

  #else
  csn(LOW);
  _SPI.transfer(W_ACK_PAYLOAD | ( pipe & B111 ) );

  while ( data_len-- )
    _SPI.transfer(*current++);

  csn(HIGH);

  #endif
}

/****************************************************************************/

bool RF24::isAckPayloadAvailable(void)
{
  return ! read_register(FIFO_STATUS) & _BV(RX_EMPTY);
}

/****************************************************************************/

bool RF24::isPVariant(void)
{
  return p_variant ;
}

/****************************************************************************/

void RF24::setAutoAck(bool enable)
{
  if ( enable )
    write_register(EN_AA, B111111);
  else
    write_register(EN_AA, 0);
}

/****************************************************************************/

void RF24::setAutoAck( uint8_t pipe, bool enable )
{
  if ( pipe <= 6 )
  {
    uint8_t en_aa = read_register( EN_AA ) ;
    if( enable )
    {
      en_aa |= _BV(pipe) ;
    }
    else
    {
      en_aa &= ~_BV(pipe) ;
    }
    write_register( EN_AA, en_aa ) ;
  }
}

/****************************************************************************/

bool RF24::testCarrier(void)
{
  return ( read_register(CD) & 1 );
}

/****************************************************************************/

bool RF24::testRPD(void)
{
  return ( read_register(RPD) & 1 ) ;
}

/****************************************************************************/

void RF24::setPALevel(uint8_t level)
{

  uint8_t setup = read_register(RF_SETUP) & 0b11111000;

  if(level > 3){  						// If invalid level, go to max PA
	  level = (RF24_PA_MAX << 1) + 1;		// +1 to support the SI24R1 chip extra bit
  }else{
	  level = (level << 1) + 1;	 		// Else set level as requested
  }


  write_register( RF_SETUP, setup |= level ) ;	// Write it to the chip
}

/****************************************************************************/

uint8_t RF24::getPALevel(void)
{

  return (read_register(RF_SETUP) & (_BV(RF_PWR_LOW) | _BV(RF_PWR_HIGH))) >> 1 ;
}

/****************************************************************************/

bool RF24::setDataRate(rf24_datarate_e speed)
{
  bool result = false;
  uint8_t setup = read_register(RF_SETUP) ;

  // HIGH and LOW '00' is 1Mbs - our default
  setup &= ~(_BV(RF_DR_LOW) | _BV(RF_DR_HIGH)) ;
  if( speed == RF24_250KBPS )
  {
    // Must set the RF_DR_LOW to 1; RF_DR_HIGH (used to be RF_DR) is already 0
    // Making it '10'.
    setup |= _BV( RF_DR_LOW ) ;
  }
  else
  {
    // Set 2Mbs, RF_DR (RF_DR_HIGH) is set 1
    // Making it '01'
    if ( speed == RF24_2MBPS )
    {
      setup |= _BV(RF_DR_HIGH);
    }
  }
  write_register(RF_SETUP,setup);

  // Verify our result
  if ( read_register(RF_SETUP) == setup )
  {
    result = true;
  }

  return result;
}

/****************************************************************************/

rf24_datarate_e RF24::getDataRate( void )
{
  rf24_datarate_e result ;
  uint8_t dr = read_register(RF_SETUP) & (_BV(RF_DR_LOW) | _BV(RF_DR_HIGH));

  // switch uses RAM (evil!)
  // Order matters in our case below
  if ( dr == _BV(RF_DR_LOW) )
  {
    // '10' = 250KBPS
    result = RF24_250KBPS ;
  }
  else if ( dr == _BV(RF_DR_HIGH) )
  {
    // '01' = 2MBPS
    result = RF24_2MBPS ;
  }
  else
  {
    // '00' = 1MBPS
    result = RF24_1MBPS ;
  }
  return result ;
}

/****************************************************************************/

void RF24::setCRCLength(rf24_crclength_e length)
{
  uint8_t config = read_register(CONFIG) & ~( _BV(CRCO) | _BV(EN_CRC)) ;

  // switch uses RAM (evil!)
  if ( length == RF24_CRC_DISABLED )
  {
    // Do nothing, we turned it off above.
  }
  else if ( length == RF24_CRC_8 )
  {
    config |= _BV(EN_CRC);
  }
  else
  {
    config |= _BV(EN_CRC);
    config |= _BV( CRCO );
  }
  write_register( CONFIG, config ) ;
}

/****************************************************************************/

rf24_crclength_e RF24::getCRCLength(void)
{
  rf24_crclength_e result = RF24_CRC_DISABLED;
  
  uint8_t config = read_register(CONFIG) & ( _BV(CRCO) | _BV(EN_CRC)) ;
  uint8_t AA = read_register(EN_AA);
  
  if ( config & _BV(EN_CRC ) || AA)
  {
    if ( config & _BV(CRCO) )
      result = RF24_CRC_16;
    else
      result = RF24_CRC_8;
  }

  return result;
}

/****************************************************************************/

void RF24::disableCRC( void )
{
  uint8_t disable = read_register(CONFIG) & ~_BV(EN_CRC) ;
  write_register( CONFIG, disable ) ;
}

/****************************************************************************/
void RF24::setRetries(uint8_t delay, uint8_t count)
{
 write_register(SETUP_RETR,(delay&0xf)<<ARD | (count&0xf)<<ARC);
}


//ATTiny support code pulled in from https://github.com/jscrane/RF24

#if defined(__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
// see http://gammon.com.au/spi
#	define DI   0  // D0, pin 5  Data In
#	define DO   1  // D1, pin 6  Data Out (this is *not* MOSI)
#	define USCK 2  // D2, pin 7  Universal Serial Interface clock
#	define SS   3  // D3, pin 2  Slave Select
#elif defined(__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
// these depend on the core used (check pins_arduino.h)
// this is for jeelabs' one (based on google-code core)
#	define DI   4   // PA6
#	define DO   5   // PA5
#	define USCK 6   // PA4
#	define SS   3   // PA7
#endif

#if defined(RF24_TINY)

void SPIClass::begin() {

  digitalWrite(SS, HIGH);
  pinMode(USCK, OUTPUT);
  pinMode(DO, OUTPUT);
  pinMode(SS, OUTPUT);
  pinMode(DI, INPUT);
  USICR = _BV(USIWM0);

}

byte SPIClass::transfer(byte b) {

  USIDR = b;
  USISR = _BV(USIOIF);
  do
    USICR = _BV(USIWM0) | _BV(USICS1) | _BV(USICLK) | _BV(USITC);
  while ((USISR & _BV(USIOIF)) == 0);
  return USIDR;

}

void SPIClass::end() {}
void SPIClass::setDataMode(uint8_t mode){}
void SPIClass::setBitOrder(uint8_t bitOrder){}
void SPIClass::setClockDivider(uint8_t rate){}


#endif
