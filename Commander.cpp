// 
// Commander class for processing commands
// https://github.com/gcsalzburg
// © George Cave 2020
//

#include "Commander.h"

Commander::Commander(char *n_id, char *b_id){
	strcpy(network_id, n_id);
	strcpy(board_id, b_id);
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
		Serial.println("LoRa radio init failed");
		Serial.println("Uncomment '#define SERIAL_DEBUG' in RH_RF95.cpp for detailed debug info");
		while (1);
	}
	Serial.println("LoRa radio init!");

	// Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
	if (!rf95.setFrequency(freq)) {
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
	send_boot_msg();
}

bool Commander::available(){
	// Read in incoming data
	if (rf95.available()){
		
		uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
		uint8_t len = sizeof(buf);

		if (rf95.recv(buf, &len)){
			// Read in string to commander!
			if(read(buf, len)){
				return true;
			}
		}else{
			// Receive must have failed, for some reason
		}
	}
	return false;
}

//
// Reads in a whole buffer length at a time, e.g. from LoRa radio
// Will return true if we have a new message to read
bool Commander::read(uint8_t *buff, uint8_t len){

	strcpy(buffer, (char *)buff);
	return process_input();
}

//
// Strip out message into parts for processing
// See README.md for expected message format
bool Commander::process_input(){

	msg_length = strlen(buffer);

	if(msg_length < 5){
		// Message was too short
		// TODO change this arbitrary length?
		cleanup();
		return false;
	}
	if((buffer[0] != network_id[0]) || (buffer[1] != network_id[1])){
		// Network ID is incorrect
		cleanup();
		return false;
	}
	if(buffer[2] != board_id[0]){
		// Board ID not for us!
		cleanup();
		return false;
	}
	if(buffer[3] != '.'){
		// Separator is not a . (maybe its just an ack message)
		cleanup();
		return false;
	}
	status_change(RECEIVING);

	// Send an acknowledgement
	_send(&buffer[msg_length-3], 4, false, true);

	// Save message ready for retrieval
	memset(msg, 0, sizeof msg);	
	msg_length = msg_length-8;	// Strip header and footer from message
	strncpy(msg, &buffer[4], msg_length);
	cleanup();

	status_change(OK);

	return true;
}

void Commander::send(char *msg, uint8_t len, bool request_reply){
	status_change(SENDING);
	_send(msg, board_id, len, request_reply, false);
}

void Commander::send(char *msg, char *b_id, uint8_t len, bool request_reply){
	status_change(SENDING);
	_send(msg, b_id, len, request_reply, false);
}

void Commander::ping(){
	if(millis() >= last_send+ping_interval){
		char packet_msg[] = "9";
		status_change(PING_START);
		_send(packet_msg, 2, false, false);
	}
}

// //////////////////////////////////////
// //////////////////////////////////////
// Private functions

void Commander::_send(char *msg, uint8_t len, bool do_retry, bool is_ack){
	_send(msg, board_id, len, is_ack, do_retry);
}

void Commander::_send(char *msg, char *b_id, uint8_t len, bool do_retry, bool is_ack){

	char send_buffer[buffer_size+1];
	memset(send_buffer, 0, sizeof send_buffer);

	// Assemble packet in expected format
	// ##x.###########.###

	// Add network ID and device ID
	strncpy(send_buffer, network_id, 2);
	strncpy(&send_buffer[2], b_id, 1);

	if(!is_ack){
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
	}else{
		// It's an ACK message, so we just fire back the provided random chars
		send_buffer[3] = '>';					
		strncpy(&send_buffer[4], msg, len);

		// Send packet!
		rf95.send((uint8_t *)send_buffer, 4+len);
	}	

	rf95.waitPacketSent();

	// Retry to send if no response
	// We never retry to send ACK messages
	if(do_retry && !is_ack){
		uint8_t buf[buffer_size];
		uint8_t len = sizeof(buf);
		uint8_t retries = 0;
		while(retries < max_retries){
			if(rf95.waitAvailableTimeout(resend_delay)){
				if(rf95.recv(buf, &len)){
					Serial.print("Got reply: ");
					Serial.println((char*)buf);
					Serial.print("RSSI: ");
					Serial.println(rf95.lastRssi(), DEC);   
					break; 
				}else{
					Serial.println("Receive failed");
				}
			}else{
				Serial.println("No reply found");
			}
			retries++;
		}
		Serial.println("Ending retry loop");
	}

	// Set state for finished
	status_change(OK);


	// Save timer
	if(!is_ack){
		last_send = millis();
	}
}

// //////////////////////////////////////

void Commander::status_change(Commander::status new_status){
	if( _status_callback != NULL ){
		_status_callback(new_status);
	}
}

void Commander::send_boot_msg(){
	char packet_msg[] = "1";
	_send(packet_msg, 2, false, false);
}

void Commander::cleanup(){
	memset(buffer, 0, sizeof buffer);
	buffer_i = 0;
}