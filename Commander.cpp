// 
// Commander class for processing commands
// https://github.com/gcsalzburg
// © George Cave 2020
//

#include "Commander.h"

// Constructor
// Requires a network ID and board ID
Commander::Commander(char *network_id, char *board_id){
	strcpy(_network_id, network_id);
	strcpy(_board_id, board_id);
}

void Commander::setStatusCallback(void (*status_callback)(Commander::status)){
    _status_callback = status_callback;
}

void Commander::init(float freq, int8_t power){
	pinMode(RFM95_RST, OUTPUT);
	digitalWrite(RFM95_RST, HIGH);
	delay(100);

	// manual reset
	digitalWrite(RFM95_RST, LOW);
	delay(10);
	digitalWrite(RFM95_RST, HIGH);
	delay(10);

	// Check if init was possible
	while (!rf95.init()) {
		_status_change(ERROR);
		Serial.println("LoRa radio init failed");
		Serial.println("Uncomment '#define SERIAL_DEBUG' in RH_RF95.cpp for detailed debug info");
		while (1);
	}
	Serial.println("LoRa radio init!");

	// Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
	if (!rf95.setFrequency(freq)) {
		_status_change(ERROR);
		Serial.println("setFrequency failed");
		while (1);
	}
	Serial.print("Set Freq to: "); Serial.println(RF95_FREQ);

	// Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on

	// The default transmitter power is 13dBm, using PA_BOOST.
	// If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then 
	// you can set transmitter powers from 5 to 23 dBm:
	rf95.setTxPower(power, false);

	// Send a bootup message
	_send_boot_msg();
}

bool Commander::available(){
	// Read in incoming data
	if (rf95.available()){
		
		uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
		uint8_t len = sizeof(buf);

		if (rf95.recv(buf, &len)){
			_status_change(READING);
			// Read in string to commander!
			if(_read(buf, len)){
				return true;
			}
		}else{
			// Receive must have failed, for some reason
		}
	}
	return false;
}

void Commander::send(char *msg, uint8_t len, bool request_reply){
	_status_change(SENDING);
	_send(msg, _board_id, len, request_reply, false);
}

void Commander::send(char *msg, char *board_id, uint8_t len, bool request_reply){
	_status_change(SENDING);
	_send(msg, board_id, len, request_reply, false);
}

void Commander::ping(){
	if(millis() >= _last_send+_ping_interval){
		char packet_msg[] = "9";
		_status_change(PING_START);
		_send(packet_msg, 2, false, false);
	}
}

// //////////////////////////////////////
// //////////////////////////////////////
// Private functions

void Commander::_send(char *msg, uint8_t len, bool do_retry, bool is_ack){
	_send(msg, _board_id, len, is_ack, do_retry);
}

void Commander::_send(char *msg, char *board_id, uint8_t len, bool do_retry, bool is_ack){

	char send_buffer[BUFFER_SIZE+1];
	memset(send_buffer, 0, sizeof send_buffer);

	// Assemble packet in expected format
	// ##x.###########.###

	// Add network ID and device ID
	strncpy(send_buffer, _network_id, 2);
	strncpy(&send_buffer[2], board_id, 1);

	if(is_ack){
		// It's an ACK message, so we just fire back the provided random chars
		send_buffer[3] = '>';					
		strncpy(&send_buffer[4], msg, len);

		// Send packet!
		rf95.send((uint8_t *)send_buffer, 4+len);
		rf95.waitPacketSent();
		
		// Set state for finished as not expecting a reply
		_status_change(OK);

	}else{

		// It's just a normal message, so append message and random chars 
		send_buffer[3] = '.';				  
		strncpy(&send_buffer[4], msg, len);

		// Overwrite \0 char from msg with a .
		send_buffer[4+(len-1)] = '.';		

		// Add random characters at end
		for(uint8_t i=0; i<3; i++){
			send_buffer[4+len+i] = alphanumeric[random(0, 62)];
		}										     

		// Send packet!
		rf95.send((uint8_t *)send_buffer, 4+len+4);
		rf95.waitPacketSent();

		// Retry to send if no response
		// We never retry to send ACK messages
		
		bool had_reply = false;
		if(do_retry){
			
			// Buffer to store response in
			uint8_t ack_buff[RH_RF95_MAX_MESSAGE_LEN];
			uint8_t ack_len = sizeof(ack_buff);

			// Data about retries
			uint8_t retries = 0;

			while(retries < _max_retries){
				if(retries == 1){
					// After the first attempt, let them know we are waiting...
					_status_change(AWAITING_RESPONSE);
				}
				if(rf95.waitAvailableTimeout(_resend_delay)){
					if(rf95.recv(ack_buff, &ack_len)){
		
						// Copy it to the message buffer
						strcpy(_buffer, (char *)ack_buff);

						// Check if its correct
						if(_process_input(true)){
							if(
								(send_buffer[4+len]   == msg_rand[0]) &&
								(send_buffer[4+len+1] == msg_rand[1]) &&
								(send_buffer[4+len+2] == msg_rand[2])
							){
								had_reply = true;
								break; 
							}
						}		
					}
				}
				retries++;
			}
		}

		// Set final status
		if(do_retry && !had_reply){
			_status_change(NO_RESPONSE);
		}else{
			_status_change(OK);
		}
	}

	// Save timer
	if(!is_ack){
		_last_send = millis();
	}
}

//
// Reads in a whole buffer length at a time, e.g. from LoRa radio
// Will return true if we have a new message to read
bool Commander::_read(uint8_t *buff, uint8_t len){

	strcpy(_buffer, (char *)buff);
	return _process_input();
}

//
// Strip out message into parts for processing
// See README.md for expected message format
// We pass in the board_id so we can check for an ACK coming back
bool Commander::_process_input(bool is_ack_check){

	msg_length = strlen(_buffer);

	if(msg_length < 5){
		// Message was too short
		// TODO change this arbitrary length?
		_cleanup();
		return false;
	}
	if((_buffer[0] != _network_id[0]) || (_buffer[1] != _network_id[1])){
		// Network ID is incorrect
		_cleanup();
		return false;
	}

	// If we are doing this loop to check the returning ACK, we ignore everything else
	if(is_ack_check){

		if(!(_buffer[3] == '>')){
			// Was not an ACK or a normal message
			_cleanup();
			return false;
		}

		// Reset message rand buffer
		memset(msg_rand, 0, sizeof msg_rand);

		// Save the rand bit
		strncpy(msg_rand, &_buffer[msg_length-3], 3);	

	}else{

		if(_buffer[2] != _board_id[0]){
			// Board ID not for us!
			_cleanup();
			return false;
		}
		if( !(_buffer[3] == '.') ){
			// Was not an ACK or a normal message
			_cleanup();
			return false;
		}

		_status_change(RECEIVING);

		// Send an acknowledgement
		_send(&_buffer[msg_length-3], 4, false, true);

		// Reset msg buffers
		memset(msg, 0, sizeof msg);

		// Save message and random string ready for retrieval
		strncpy(msg, &_buffer[4], msg_length-8); // -8 = Strip header and footer from message

	}

	// Finish
	_status_change(OK);
	_cleanup();
	return true;
}

// //////////////////////////////////////
// Private helper functions

void Commander::_status_change(Commander::status new_status){
	if( _status_callback != NULL ){
		_status_callback(new_status);
	}
}

void Commander::_send_boot_msg(){
	char packet_msg[] = "1";
	_send(packet_msg, 2, false, false);
}

void Commander::_cleanup(){
	memset(_buffer, 0, sizeof _buffer);
	_buffer_i = 0; // used if saving to buffer one char at a time (not currently in use in LoRa)
}